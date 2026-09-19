#ifndef GCC_GRAPH_HPP
#define GCC_GRAPH_HPP

// only storage layer of the gastcoco: per-vertex small chunks promoted to a per-vertex B+ tree, threaded by a global traversal chain

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
          std::size_t SmallChunkCapacity = 4,
          std::size_t BPlusTreeOrder = 8>
class GCCGraph : public BaseGraph<GCCGraph<VertexType, WeightType, SmallChunkCapacity, BPlusTreeOrder>,
                                   VertexType, WeightType> {
    static_assert(SmallChunkCapacity >= 1, "SmallChunkCapacity must be >= 1");
    static_assert(BPlusTreeOrder >= 3, "BPlusTreeOrder must be >= 3");

public:
    using Base = BaseGraph<GCCGraph, VertexType, WeightType>;

    static constexpr std::size_t SmallChunkPromotionThreshold = 4;

private:
    enum class ChainKind : uint8_t { Chunk, Leaf };

    struct GTChainNode {
        ChainKind kind;
        uint32_t vertexIdx;
        GTChainNode* gtNext = nullptr;

        GTChainNode(ChainKind k, uint32_t v) : kind(k), vertexIdx(v) {}
    };

    struct alignas(64) SmallChunk : GTChainNode {
        uint32_t count = 0;
        uint32_t dst[SmallChunkCapacity];
        WeightType weight[SmallChunkCapacity];
        SmallChunk* next = nullptr;

        explicit SmallChunk(uint32_t v) : GTChainNode(ChainKind::Chunk, v) {}

        bool full() const { return count == SmallChunkCapacity; }

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

    struct alignas(64) BPlusNode : GTChainNode {
        static constexpr std::size_t KeyCap = BPlusTreeOrder;
        static constexpr std::size_t ChildCap = BPlusTreeOrder + 1;

        bool isLeaf;
        uint32_t numKeys = 0;
        uint32_t keys[KeyCap];
        WeightType vals[KeyCap];
        BPlusNode* children[ChildCap];
        BPlusNode* next = nullptr;
        BPlusNode* parent = nullptr;

        BPlusNode(bool leaf, uint32_t v) : GTChainNode(ChainKind::Leaf, v), isLeaf(leaf) {}
    };

    struct SplitResult {
        bool split = false;
        uint32_t promotedKey = 0;
        BPlusNode* rightSibling = nullptr;
    };

    struct VertexEntry {
        std::size_t size = 0;
        uint32_t level = 0;
        bool deleteFlag = false;
        void* traversalPointer = nullptr;
        void* queryPointer = nullptr;
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
        ++ve.level;

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
        ve.traversalPointer = leaf;
        ve.queryPointer = leaf;
        ve.level = 1;
        ve.size = 0;

        for (auto& kv : items) {
            bplusInsert(ve, uId, kv.first, kv.second);
        }
    }

    void insertDirectedEdge(uint32_t uId, uint32_t vId, const WeightType& weight) {
        VertexEntry& ve = vertexTable[uId];

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

    void rebuildGTChain() const {
        gtHead = nullptr;
        GTChainNode* prevTail = nullptr;
        for (uint32_t i = 0; i < vertexTable.size(); ++i) {
            const VertexEntry& ve = vertexTable[i];
            if (ve.size == 0) continue;

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
    explicit GCCGraph(bool directed = false) : Base(directed) {}

    GCCGraph(const GCCGraph&) = delete;
    GCCGraph& operator=(const GCCGraph&) = delete;
    GCCGraph(GCCGraph&&) = default;
    GCCGraph& operator=(GCCGraph&&) = default;

    ~GCCGraph() {
        for (auto& ve : vertexTable) {
            if (ve.level == 0) {
                SmallChunk* c = static_cast<SmallChunk*>(ve.traversalPointer);
                while (c) {
                    SmallChunk* nxt = c->next;
                    delete c;
                    c = nxt;
                }
            } else if (ve.queryPointer) {

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

    void reserve(std::size_t vertexCapacity, std::size_t  = 0) {

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

    const std::unordered_map<VertexType, uint32_t>& getIdMap() const { return idMap; }
    const std::vector<VertexType>& getReverseIdMap() const { return reverseIdMap; }

    uint32_t getVertexLevel(const VertexType& u) const {
        auto it = idMap.find(u);
        if (it == idMap.end()) throw std::out_of_range("GCCGraph::getVertexLevel: no such vertex");
        return vertexTable[it->second].level;
    }

    static constexpr std::size_t smallChunkNodeSize() { return sizeof(SmallChunk); }
    static constexpr std::size_t bPlusNodeSize() { return sizeof(BPlusNode); }
    static constexpr std::size_t smallChunkAlignment() { return alignof(SmallChunk); }
    static constexpr std::size_t bPlusNodeAlignment() { return alignof(BPlusNode); }

    std::size_t memoryUsageBytes() const {
        std::size_t bytes = vertexTable.capacity() * sizeof(VertexEntry);
        for (const auto& ve : vertexTable) {
            if (ve.level == 0) {
                for (SmallChunk* c = static_cast<SmallChunk*>(ve.traversalPointer); c; c = c->next) {
                    bytes += sizeof(SmallChunk);
                }
            } else if (ve.queryPointer) {
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
                bytes += visited.size() * sizeof(BPlusNode);
            }
        }
        return bytes;
    }
};

static_assert(GraphReq<GCCGraph<int, double>, int, double>);

#endif