#ifndef CGG_GRAPH_HPP
#define CGG_GRAPH_HPP

// -----------------------------------------------------------------------------
// CGGGraph: an implementation of the CBList data structure proposed in
//
//   Li, Tao, Yu, Gong, Zhang, Yao, Yu, Yu, Zhou.
//   "GastCoCo: Graph Storage and Coroutine-Based Prefetch Co-Design for
//    Dynamic Graph Processing." PVLDB 17(13), 2024. (arXiv:2312.14396v5)
//
// This file implements the STORAGE LAYER only (Sec. 4 of the paper): the
// vertex table, the hierarchical "update-read balanced" edge storage
// (small chunks for low-degree vertices, B+ trees for high-degree
// vertices), and the prefetch-friendly Global Traversal Chain (GTChain).
//
// The coroutine-based software-prefetching execution engine, the task
// allocator/scheduler, and the adaptation layer (Sec. 5-6) are NOT part of
// this file: they operate on top of a storage structure like this one, but
// are orthogonal to the BaseGraph/GraphReq storage interface used in this
// codebase, so they are out of scope here.
//
// ---------------------------------------------------------------------------
// v2 note (hardware-locality fix): the first version of this file stored a
// B+ tree node's keys/values/children in std::vector members. That defeats
// the entire point of Sec 4.2: a std::vector's payload lives in a SEPARATE
// heap allocation from the node object itself, so when the hardware
// prefetcher pulls in the cache line(s) containing a BPlusNode, it does NOT
// pull in the node's actual keys/values/children -- those sit at an
// unrelated address. This version instead stores keys/values/children as
// fixed-size C arrays sized at compile time from BPlusTreeOrder, so they
// are physically embedded inside the node, and both SmallChunk and
// BPlusNode are `alignas(64)` so each node starts on a cache-line boundary,
// matching the paper's "capacities ... set as integer multiples ... of the
// cache line size" (Sec 4.2). The only remaining std::vector usage is a
// *local, temporary* working buffer inside promoteToBPlusTree() used once
// while bulk-reorganizing a vertex's neighborhood -- it is not part of the
// persistent structure touched by hot-path queries/traversals.
// ---------------------------------------------------------------------------
//
// Where the paper is not fully explicit about an implementation detail, a
// reasonable, clearly-commented choice was made below.
// -----------------------------------------------------------------------------

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <stdexcept>
#include "Concepts.hpp"
#include "BaseGraph.hpp"

template <typename VertexType, Numeric WeightType = double,
          std::size_t SmallChunkCapacity = 4,   // Sec 4.2: "multiples (1-4) of the cache line size"
          std::size_t BPlusTreeOrder = 8>       // max children per internal B+ tree node
class CGGGraph : public BaseGraph<CGGGraph<VertexType, WeightType, SmallChunkCapacity, BPlusTreeOrder>,
                                   VertexType, WeightType> {
    static_assert(SmallChunkCapacity >= 1, "SmallChunkCapacity must be >= 1");
    static_assert(BPlusTreeOrder >= 3, "BPlusTreeOrder must be >= 3");

public:
    using Base = BaseGraph<CGGGraph, VertexType, WeightType>;

    // The paper does not fix the exact number of chained small chunks a
    // level-0 vertex may accumulate before its neighborhood is reorganized
    // into a B+ tree (Sec 4.2, "When the data exceeds the capacities of
    // small chunks, we reorganize it in a B+ tree"). We expose it as a
    // tunable promotion threshold.
    static constexpr std::size_t SmallChunkPromotionThreshold = 4;

private:
    enum class ChainKind : uint8_t { Chunk, Leaf };

    // Common header giving every node that can participate in the Global
    // Traversal Chain (GTChain, Sec 4.3, "the chain formed by red dashed
    // lines in Figure 4") a uniform splice point, so a single logical chain
    // can be walked across the two heterogeneous physical node types.
    struct GTChainNode {
        ChainKind kind;
        uint32_t vertexIdx;             // logical id of the owning vertex
        GTChainNode* gtNext = nullptr;  // set ONLY on the last local node of a
                                         // vertex's neighborhood; points at the
                                         // first local node of the next
                                         // non-empty vertex (Fig. 4 red dashes).
        GTChainNode(ChainKind k, uint32_t v) : kind(k), vertexIdx(v) {}
    };

    // ---- Level 0: chained "small chunks" (Sec 4.2) --------------------------
    // Fixed-capacity, alignas(64): the whole node -- header, count, and the
    // (dst, weight) payload -- is one contiguous, cache-line-aligned block,
    // so a hardware prefetcher pulling adjacent lines for this node also
    // pulls in its actual edge data.
    struct alignas(64) SmallChunk : GTChainNode {
        uint32_t count = 0;
        uint32_t dst[SmallChunkCapacity];
        WeightType weight[SmallChunkCapacity];
        SmallChunk* next = nullptr;     // intra-vertex chaining == Traversal pointer target

        explicit SmallChunk(uint32_t v) : GTChainNode(ChainKind::Chunk, v) {}

        bool full() const { return count == SmallChunkCapacity; }

        // Keeps the chunk sorted by destination id, enabling the "binary
        // search on small chunks" mentioned in Sec 6.1. Returns false if
        // the chunk has no room (caller must allocate a new chunk).
        bool insertSorted(uint32_t d, const WeightType& w) {
            if (full()) return false;
            uint32_t pos = 0;
            while (pos < count && dst[pos] < d) ++pos;
            for (uint32_t i = count; i > pos; --i) {
                dst[i] = dst[i - 1];
                weight[i] = weight[i - 1];
            }
            dst[pos] = d;
            weight[pos] = w;
            ++count;
            return true;
        }

        bool find(uint32_t d, WeightType* outW = nullptr) const {
            uint32_t lo = 0, hi = count;
            while (lo < hi) {
                uint32_t mid = lo + (hi - lo) / 2;
                if (dst[mid] == d) {
                    if (outW) *outW = weight[mid];
                    return true;
                }
                if (dst[mid] < d) lo = mid + 1; else hi = mid;
            }
            return false;
        }
    };

    // ---- Level > 0: a B+ tree keyed by destination logical id ---------------
    // Same fixed-size / alignas(64) treatment as SmallChunk. `keys`, `vals`
    // and `children` are plain arrays sized from BPlusTreeOrder at compile
    // time (with one extra slot to hold the transient overflow state right
    // before a split), so they live inside the node object itself rather
    // than behind a separate heap pointer. Leaves are singly linked via
    // `next` (Sec 4.2: "scan_edges(v_src) accessed from Traversal pointer in
    // a B+ tree can be considered as sequential data access on a linked
    // list"); this leaf chain is also what gets spliced into the GTChain
    // (Sec 4.3).
    struct alignas(64) BPlusNode : GTChainNode {
        static constexpr std::size_t KeyCap = BPlusTreeOrder;      // transient max before split
        static constexpr std::size_t ChildCap = BPlusTreeOrder + 1;

        bool isLeaf;
        uint32_t numKeys = 0;
        uint32_t keys[KeyCap];
        WeightType vals[KeyCap];          // meaningful only when isLeaf
        BPlusNode* children[ChildCap];    // meaningful only when !isLeaf
        BPlusNode* next = nullptr;        // leaf chain, leaf-only
        BPlusNode* parent = nullptr;

        BPlusNode(bool leaf, uint32_t v) : GTChainNode(ChainKind::Leaf, v), isLeaf(leaf) {}
    };

    struct SplitResult {
        bool split = false;
        uint32_t promotedKey = 0;
        BPlusNode* rightSibling = nullptr;
    };

    // ---- Vertex table entry (Fig. 4 record format) -------------------------
    struct VertexEntry {
        std::size_t size = 0;              // number of adjacent edges
        uint32_t level = 0;                // 0 = small-chunk chain; >0 = #leaf nodes
        bool deleteFlag = false;           // tombstone flag (no delete op required by GraphReq)
        void* traversalPointer = nullptr;  // SmallChunk* head, or B+ tree leftmost leaf
        void* queryPointer = nullptr;      // SmallChunk* head, or B+ tree root
    };

    std::unordered_map<VertexType, uint32_t> idMap;
    std::vector<VertexType> reverseIdMap;
    std::vector<VertexEntry> vertexTable;

    mutable GTChainNode* gtHead = nullptr;
    mutable bool gtChainDirty = true;

    uint32_t getOrRegisterVertex(const VertexType& x) {
        auto it = idMap.find(x);
        if (it != idMap.end()) return it->second;
        uint32_t id = static_cast<uint32_t>(reverseIdMap.size());
        idMap[x] = id;
        reverseIdMap.push_back(x);
        vertexTable.push_back(VertexEntry{});
        gtChainDirty = true;
        return id;
    }

    // Searches the existing neighborhood (either representation) for `vId`
    // and, if found, overwrites its weight in place. This keeps edges
    // de-duplicated consistently between the small-chunk representation and
    // the B+ tree representation (see promoteToBPlusTree(), which also
    // de-duplicates when reorganizing).
    bool tryUpdateExisting(VertexEntry& ve, uint32_t vId, const WeightType& w) {
        if (ve.level == 0) {
            for (SmallChunk* c = static_cast<SmallChunk*>(ve.queryPointer); c; c = c->next) {
                for (uint32_t i = 0; i < c->count; ++i) {
                    if (c->dst[i] == vId) { c->weight[i] = w; return true; }
                }
            }
            return false;
        }
        BPlusNode* node = static_cast<BPlusNode*>(ve.queryPointer);
        while (!node->isLeaf) {
            uint32_t pos = static_cast<uint32_t>(
                std::upper_bound(node->keys, node->keys + node->numKeys, vId) - node->keys);
            node = node->children[pos];
        }
        for (uint32_t i = 0; i < node->numKeys; ++i) {
            if (node->keys[i] == vId) { node->vals[i] = w; return true; }
        }
        return false;
    }

    // ---- B+ tree insertion (standard split-on-overflow insertion) -----------
    SplitResult bplusInsertLeaf(BPlusNode* leaf, uint32_t key, const WeightType& val, uint32_t vertexIdx) {
        uint32_t pos = static_cast<uint32_t>(
            std::lower_bound(leaf->keys, leaf->keys + leaf->numKeys, key) - leaf->keys);
        for (uint32_t i = leaf->numKeys; i > pos; --i) {
            leaf->keys[i] = leaf->keys[i - 1];
            leaf->vals[i] = leaf->vals[i - 1];
        }
        leaf->keys[pos] = key;
        leaf->vals[pos] = val;
        ++leaf->numKeys;

        if (leaf->numKeys < BPlusTreeOrder) return {};

        uint32_t mid = leaf->numKeys / 2;
        BPlusNode* right = new BPlusNode(true, vertexIdx);
        uint32_t rCount = leaf->numKeys - mid;
        for (uint32_t i = 0; i < rCount; ++i) {
            right->keys[i] = leaf->keys[mid + i];
            right->vals[i] = leaf->vals[mid + i];
        }
        right->numKeys = rCount;
        leaf->numKeys = mid;
        right->next = leaf->next;
        leaf->next = right;
        right->parent = leaf->parent;

        SplitResult r;
        r.split = true;
        r.promotedKey = right->keys[0];
        r.rightSibling = right;
        return r;
    }

    SplitResult bplusInsertInternal(BPlusNode* node, uint32_t promotedKey, BPlusNode* rightChild, uint32_t vertexIdx) {
        uint32_t pos = static_cast<uint32_t>(
            std::upper_bound(node->keys, node->keys + node->numKeys, promotedKey) - node->keys);
        for (uint32_t i = node->numKeys; i > pos; --i) node->keys[i] = node->keys[i - 1];
        for (uint32_t i = node->numKeys + 1; i > pos + 1; --i) node->children[i] = node->children[i - 1];
        node->keys[pos] = promotedKey;
        node->children[pos + 1] = rightChild;
        rightChild->parent = node;
        ++node->numKeys;

        if (node->numKeys < BPlusTreeOrder) return {};

        uint32_t mid = node->numKeys / 2;
        uint32_t upKey = node->keys[mid];
        BPlusNode* right = new BPlusNode(false, vertexIdx);
        uint32_t rKeyCount = node->numKeys - mid - 1;
        for (uint32_t i = 0; i < rKeyCount; ++i) right->keys[i] = node->keys[mid + 1 + i];
        uint32_t rChildCount = rKeyCount + 1;
        for (uint32_t i = 0; i < rChildCount; ++i) {
            right->children[i] = node->children[mid + 1 + i];
            right->children[i]->parent = right;
        }
        right->numKeys = rKeyCount;
        node->numKeys = mid;
        right->parent = node->parent;

        SplitResult r;
        r.split = true;
        r.promotedKey = upKey;
        r.rightSibling = right;
        return r;
    }

    // Inserts (key,val) into vertex `vertexIdx`'s B+ tree, splitting nodes
    // (and, if needed, creating a new root) as required. `ve.level` is kept
    // equal to the current number of leaf nodes, per Sec 4.1: "other numbers
    // indicate using B+ trees and the value represents the number of leaf
    // nodes in the tree."
    void bplusInsert(VertexEntry& ve, uint32_t vertexIdx, uint32_t key, const WeightType& val) {
        BPlusNode* node = static_cast<BPlusNode*>(ve.queryPointer);
        while (!node->isLeaf) {
            uint32_t pos = static_cast<uint32_t>(
                std::upper_bound(node->keys, node->keys + node->numKeys, key) - node->keys);
            node = node->children[pos];
        }
        SplitResult sr = bplusInsertLeaf(node, key, val, vertexIdx);
        ++ve.size;
        if (!sr.split) return;
        ++ve.level; // one leaf became two

        BPlusNode* left = node;
        while (sr.split) {
            BPlusNode* parent = left->parent;
            if (!parent) {
                BPlusNode* newRoot = new BPlusNode(false, vertexIdx);
                newRoot->keys[0] = sr.promotedKey;
                newRoot->children[0] = left;
                newRoot->children[1] = sr.rightSibling;
                newRoot->numKeys = 1;
                left->parent = newRoot;
                sr.rightSibling->parent = newRoot;
                ve.queryPointer = newRoot;
                return;
            }
            sr = bplusInsertInternal(parent, sr.promotedKey, sr.rightSibling, vertexIdx);
            left = parent;
        }
    }

    // Reorganizes a level-0 vertex's small-chunk chain into a fresh B+ tree
    // (Sec 4.2). The leftmost leaf is created once here and never changes
    // identity afterwards: standard B+ tree leaf splits always keep the
    // lower half in the original node object and put the upper half into a
    // newly allocated right sibling, so the very first leaf we create stays
    // leftmost for the lifetime of the tree. That means Traversal pointer
    // (ve.traversalPointer) never needs to be updated again.
    //
    // NOTE: `items` below is a local, temporary working buffer used once to
    // sort/dedupe the vertex's edges while rebuilding -- it is not part of
    // the persistent CBList structure touched by hot-path queries, so using
    // std::vector here does not reintroduce the locality problem described
    // at the top of this file.
    void promoteToBPlusTree(uint32_t uId, uint32_t newKey, const WeightType& newVal) {
        VertexEntry& ve = vertexTable[uId];

        std::vector<std::pair<uint32_t, WeightType>> items;
        SmallChunk* c = static_cast<SmallChunk*>(ve.traversalPointer);
        while (c) {
            for (uint32_t i = 0; i < c->count; ++i) items.emplace_back(c->dst[i], c->weight[i]);
            SmallChunk* nxt = c->next;
            delete c;
            c = nxt;
        }
        items.emplace_back(newKey, newVal);
        std::sort(items.begin(), items.end());
        items.erase(std::unique(items.begin(), items.end(),
                        [](const auto& a, const auto& b) { return a.first == b.first; }),
                    items.end());

        BPlusNode* leaf = new BPlusNode(true, uId);
        ve.traversalPointer = leaf;  // leftmost leaf: fixed for the tree's lifetime
        ve.queryPointer = leaf;      // initial root == the single leaf
        ve.level = 1;
        ve.size = 0;

        for (auto& kv : items) {
            bplusInsert(ve, uId, kv.first, kv.second);
        }
    }

    void insertDirectedEdge(uint32_t uId, uint32_t vId, const WeightType& weight) {
        VertexEntry& ve = vertexTable[uId];

        // Edge already present -> modify weight in place, no structural change.
        if (tryUpdateExisting(ve, vId, weight)) return;

        if (ve.level == 0) {
            SmallChunk* head = static_cast<SmallChunk*>(ve.traversalPointer);
            SmallChunk* tail = head;
            std::size_t numChunks = 0;
            while (tail) {
                ++numChunks;
                if (!tail->next) break;
                tail = tail->next;
            }

            // "the destination nodes are inserted into the chunk with the
            // update of the graph" (Sec 4.2): prefer appending to the tail.
            if (tail && tail->insertSorted(vId, weight)) {
                ++ve.size;
                gtChainDirty = true;
                return;
            }
            if (numChunks < SmallChunkPromotionThreshold) {
                SmallChunk* nc = new SmallChunk(uId);
                nc->insertSorted(vId, weight);
                if (!head) {
                    ve.traversalPointer = ve.queryPointer = nc;
                } else {
                    tail->next = nc;
                }
                ++ve.size;
                gtChainDirty = true;
                return;
            }
            promoteToBPlusTree(uId, vId, weight);
            gtChainDirty = true;
            return;
        }

        bplusInsert(ve, uId, vId, weight);
        gtChainDirty = true;
    }

    bool findEdgeWeight(const VertexType& u, const VertexType& v, WeightType& outW) const {
        auto itU = idMap.find(u);
        auto itV = idMap.find(v);
        if (itU == idMap.end() || itV == idMap.end()) return false;
        uint32_t uId = itU->second, vId = itV->second;
        const VertexEntry& ve = vertexTable[uId];

        if (ve.level == 0) {
            for (SmallChunk* c = static_cast<SmallChunk*>(ve.queryPointer); c; c = c->next) {
                if (c->find(vId, &outW)) return true;
            }
            return false;
        }

        // read_edge(v_src, v_dst) via the Query pointer, O(log D) (Sec 4.2).
        const BPlusNode* node = static_cast<const BPlusNode*>(ve.queryPointer);
        while (!node->isLeaf) {
            uint32_t pos = static_cast<uint32_t>(
                std::upper_bound(node->keys, node->keys + node->numKeys, vId) - node->keys);
            node = node->children[pos];
        }
        uint32_t lo = 0, hi = node->numKeys;
        while (lo < hi) {
            uint32_t mid = lo + (hi - lo) / 2;
            if (node->keys[mid] == vId) { outW = node->vals[mid]; return true; }
            if (node->keys[mid] < vId) lo = mid + 1; else hi = mid;
        }
        return false;
    }

    // Rebuilds the Global Traversal Chain (Sec 4.3) by splicing the tail of
    // each non-empty vertex's local neighborhood chain to the head of the
    // next non-empty vertex's local chain, in logical-id order. Rebuilding
    // lazily (on the next traverseEntireGraph() call after a structural
    // change) avoids maintaining the splice incrementally through every
    // possible chunk/B+ split, at the cost of an O(V) rebuild.
    void rebuildGTChain() const {
        gtHead = nullptr;
        GTChainNode* prevTail = nullptr;
        for (uint32_t i = 0; i < vertexTable.size(); ++i) {
            const VertexEntry& ve = vertexTable[i];
            if (ve.size == 0) continue; // nothing to splice for empty vertices

            GTChainNode* head = static_cast<GTChainNode*>(ve.traversalPointer);
            if (!gtHead) gtHead = head;
            if (prevTail) prevTail->gtNext = head;

            GTChainNode* tail;
            if (ve.level == 0) {
                SmallChunk* c = static_cast<SmallChunk*>(head);
                while (c->next) c = c->next;
                tail = c;
            } else {
                BPlusNode* l = static_cast<BPlusNode*>(head);
                while (l->next) l = l->next;
                tail = l;
            }
            tail->gtNext = nullptr;
            prevTail = tail;
        }
        gtChainDirty = false;
    }

public:
    explicit CGGGraph(bool directed = false) : Base(directed) {}

    CGGGraph(const CGGGraph&) = delete;
    CGGGraph& operator=(const CGGGraph&) = delete;
    CGGGraph(CGGGraph&&) = default;
    CGGGraph& operator=(CGGGraph&&) = default;

    ~CGGGraph() {
        for (auto& ve : vertexTable) {
            if (ve.level == 0) {
                SmallChunk* c = static_cast<SmallChunk*>(ve.traversalPointer);
                while (c) {
                    SmallChunk* nxt = c->next;
                    delete c;
                    c = nxt;
                }
            } else if (ve.queryPointer) {
                // Free every node reachable from the root exactly once. (Leaf
                // nodes are also reachable via the leaf chain, so we dedupe
                // with a visited set rather than freeing while descending.)
                std::vector<BPlusNode*> stack{static_cast<BPlusNode*>(ve.queryPointer)};
                std::unordered_set<BPlusNode*> visited;
                while (!stack.empty()) {
                    BPlusNode* n = stack.back();
                    stack.pop_back();
                    if (!n || visited.count(n)) continue;
                    visited.insert(n);
                    if (!n->isLeaf) {
                        for (uint32_t i = 0; i < n->numKeys + 1; ++i) stack.push_back(n->children[i]);
                    }
                }
                for (auto* n : visited) delete n;
            }
        }
    }

    void reserve(std::size_t vertexCapacity, std::size_t /*edgeCapacityHint*/ = 0) {
        // Edges are stored per-vertex in chunks/B+ tree nodes rather than in
        // one flat buffer, so there is no single edge-capacity to reserve.
        idMap.reserve(vertexCapacity);
        reverseIdMap.reserve(vertexCapacity);
        vertexTable.reserve(vertexCapacity);
    }

    void addVertex(const VertexType& u) {
        getOrRegisterVertex(u);
    }

    bool hasVertex(const VertexType& u) const {
        return idMap.find(u) != idMap.end();
    }

    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        uint32_t uId = getOrRegisterVertex(u);
        uint32_t vId = getOrRegisterVertex(v);
        insertDirectedEdge(uId, vId, weight);
        if (!this->isDirected && uId != vId) {
            insertDirectedEdge(vId, uId, weight);
        }
    }

    void addEdgeDynamic(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        // GastCoCo differentiates individual updates (vertex-level locks,
        // handled directly on CBList) from batched updates (routed through
        // coroutines by the execution layer, Sec 5.1 "Coroutines in Graph
        // Update"). That distinction lives in the execution layer, which is
        // out of scope for this storage-layer class, so both entry points
        // perform the same CBList-level insertion here.
        addEdge(u, v, weight);
    }

    bool hasEdge(const VertexType& u, const VertexType& v) const {
        WeightType dummy{};
        return findEdgeWeight(u, v, dummy);
    }

    bool hasEdge(const VertexType& u, const VertexType& v, const WeightType& w) const {
        WeightType found{};
        return findEdgeWeight(u, v, found) && found == w;
    }

    std::size_t getDegree(const VertexType& u) const {
        auto it = idMap.find(u);
        if (it == idMap.end()) return 0;
        return vertexTable[it->second].size;
    }

    // scan_edges(v_src) via the Traversal pointer (Sec 4.2/4.3): sequential
    // access over a small-chunk chain, or over a B+ tree's leaf chain.
    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& callback) const {
        auto it = idMap.find(u);
        if (it == idMap.end()) return;
        const VertexEntry& ve = vertexTable[it->second];

        if (ve.level == 0) {
            for (SmallChunk* c = static_cast<SmallChunk*>(ve.traversalPointer); c; c = c->next) {
                for (uint32_t i = 0; i < c->count; ++i) {
                    callback(reverseIdMap[c->dst[i]], c->weight[i]);
                }
            }
        } else {
            for (BPlusNode* leaf = static_cast<BPlusNode*>(ve.traversalPointer); leaf; leaf = leaf->next) {
                for (uint32_t i = 0; i < leaf->numKeys; ++i) {
                    callback(reverseIdMap[leaf->keys[i]], leaf->vals[i]);
                }
            }
        }
    }

    // scan_vertices() + scan_edges(v_src) over the whole graph via the
    // prefetch-friendly Global Traversal Chain (Sec 4.3).
    template <typename Callback>
    void traverseEntireGraph(Callback&& callback) const {
        if (gtChainDirty) rebuildGTChain();

        GTChainNode* node = gtHead;
        while (node) {
            const VertexType& src = reverseIdMap[node->vertexIdx];
            if (node->kind == ChainKind::Chunk) {
                auto* c = static_cast<SmallChunk*>(node);
                for (uint32_t i = 0; i < c->count; ++i) {
                    callback(src, reverseIdMap[c->dst[i]], c->weight[i]);
                }
                node = c->next ? static_cast<GTChainNode*>(c->next) : c->gtNext;
            } else {
                auto* l = static_cast<BPlusNode*>(node);
                for (uint32_t i = 0; i < l->numKeys; ++i) {
                    callback(src, reverseIdMap[l->keys[i]], l->vals[i]);
                }
                node = l->next ? static_cast<GTChainNode*>(l->next) : l->gtNext;
            }
        }
    }

    std::size_t numVertices() const {
        return reverseIdMap.size();
    }

    std::size_t numEdges() const {
        std::size_t total = 0;
        for (const auto& ve : vertexTable) total += ve.size;
        return this->isDirected ? total : total / 2;
    }

    // ---- Introspection helpers (not required by GraphReq, but useful for
    // inspecting/debugging the structure, mirroring the getters exposed by
    // RawGraph/DynamicCSRGraph). ----
    const std::unordered_map<VertexType, uint32_t>& getIdMap() const { return idMap; }
    const std::vector<VertexType>& getReverseIdMap() const { return reverseIdMap; }

    // 0 => small-chunk chain; >0 => number of leaf nodes in the vertex's B+ tree.
    uint32_t getVertexLevel(const VertexType& u) const {
        auto it = idMap.find(u);
        if (it == idMap.end()) throw std::out_of_range("CGGGraph::getVertexLevel: no such vertex");
        return vertexTable[it->second].level;
    }

    // Diagnostic: node sizes/alignment, to verify the cache-alignment claims
    // made in the comments above at compile/run time if desired.
    static constexpr std::size_t smallChunkNodeSize() { return sizeof(SmallChunk); }
    static constexpr std::size_t bPlusNodeSize() { return sizeof(BPlusNode); }
    static constexpr std::size_t smallChunkAlignment() { return alignof(SmallChunk); }
    static constexpr std::size_t bPlusNodeAlignment() { return alignof(BPlusNode); }
};

static_assert(GraphReq<CGGGraph<int, double>, int, double>);

#endif // CGG_GRAPH_HPP