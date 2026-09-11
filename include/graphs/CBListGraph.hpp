#ifndef CBLIST_GRAPH_HPP
#define CBLIST_GRAPH_HPP

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <limits>
#include "Concepts.hpp"

// SoA (Structure of Arrays) Chunk sa savršenim 192B poravnanjem (3x64B cache linije)
template <Numeric WeightType, size_t CAPACITY = 14>
struct alignas(64) EdgeChunk {
    EdgeChunk* next_local{nullptr};   // 8 bytes
    EdgeChunk* next_global{nullptr};  // 8 bytes
    uint32_t count{0};                // 4 bytes
    uint32_t padding{0};              // 4 bytes (poravnanje na 8B granicu)
    uint32_t destinations[CAPACITY];  // 14 * 4 = 56 bytes
    WeightType weights[CAPACITY];     // 14 * 8 = 112 bytes
                                      // Ukupno: 16 + 8 + 56 + 112 = 192 bytes

    bool is_full() const noexcept { return count >= CAPACITY; }

    // Sortirano umetanje unutar chunka radi brze pretrage
    bool push(uint32_t dest, WeightType weight) noexcept {
        if (is_full()) return false;
        
        int i = count - 1;
        while (i >= 0 && destinations[i] > dest) {
            destinations[i + 1] = destinations[i];
            weights[i + 1] = weights[i];
            i--;
        }
        destinations[i + 1] = dest;
        weights[i + 1] = weight;
        count++;
        return true;
    }
};

// Brzi blok-alokator koji drži chunkove fizički zajedno u RAM-u
template <Numeric WeightType, size_t CAPACITY = 14>
class ChunkArena {
private:
    static constexpr size_t CHUNKS_PER_BLOCK = 65536;
    std::vector<EdgeChunk<WeightType, CAPACITY>*> blocks;
    size_t current_chunk_idx{CHUNKS_PER_BLOCK};

public:
    ~ChunkArena() {
        for (auto* block : blocks) {
            delete[] block;
        }
    }

    EdgeChunk<WeightType, CAPACITY>* allocate() {
        if (current_chunk_idx >= CHUNKS_PER_BLOCK) {
            // value-initialization (zgrade) osigurava da su pokazivači i count na 0
            blocks.push_back(new EdgeChunk<WeightType, CAPACITY>[CHUNKS_PER_BLOCK]());
            current_chunk_idx = 0;
        }
        return &blocks.back()[current_chunk_idx++];
    }
};

template <Numeric WeightType>
struct VertexNode {
    uint32_t degree{0};
    EdgeChunk<WeightType>* head{nullptr};
    EdgeChunk<WeightType>* tail{nullptr};
};

template <typename VertexType, Numeric WeightType = double>
class CBListGraph {
private:
    std::unordered_map<VertexType, uint32_t> id_map;
    std::vector<VertexType> reverse_id_map;
    std::vector<VertexNode<WeightType>> vertex_table;

    ChunkArena<WeightType> allocator;
    EdgeChunk<WeightType>* global_head{nullptr};
    EdgeChunk<WeightType>* global_tail{nullptr};

    uint64_t total_edges{0};
    bool is_directed{false};

    uint32_t get_or_register_vertex(const VertexType& u) {
        auto it = id_map.find(u);
        if (it != id_map.end()) return it->second;

        uint32_t new_id = static_cast<uint32_t>(vertex_table.size());
        id_map[u] = new_id;
        reverse_id_map.push_back(u);

        // Lijena alokacija: ne alociramo chunk dok ne dođe stvarni brid
        vertex_table.push_back({.degree = 0, .head = nullptr, .tail = nullptr});
        return new_id;
    }

    void insert_directed_edge(uint32_t src_id, uint32_t dst_id, WeightType weight) {
        auto& node = vertex_table[src_id];
        
        // Inicijalizacija prvog chunka ako je vrh bio prazan
        if (!node.tail) {
            auto* new_chunk = allocator.allocate();
            new_chunk->push(dst_id, weight);
            node.head = new_chunk;
            node.tail = new_chunk;

            if (!global_head) {
                global_head = new_chunk;
                global_tail = new_chunk;
            } else {
                global_tail->next_global = new_chunk;
                global_tail = new_chunk;
            }
            node.degree++;
            total_edges++;
            return;
        }

        auto* active_tail = node.tail;
        if (active_tail->push(dst_id, weight)) {
            node.degree++;
            total_edges++;
            return;
        }

        // Chunk je pun, alociraj novi
        auto* new_chunk = allocator.allocate();
        new_chunk->push(dst_id, weight);

        active_tail->next_local = new_chunk;
        node.tail = new_chunk;

        global_tail->next_global = new_chunk;
        global_tail = new_chunk;

        node.degree++;
        total_edges++;
    }

public:
    explicit CBListGraph(bool directed = false) : is_directed(directed) {}
    ~CBListGraph() = default; // ChunkArena automatski čisti svu memoriju

    void reserve(size_t vertex_capacity) {
        vertex_table.reserve(vertex_capacity);
        reverse_id_map.reserve(vertex_capacity);
        id_map.reserve(vertex_capacity);
    }

    void add_vertex(const VertexType& u) { get_or_register_vertex(u); }
    void addVertex(const VertexType& u) { add_vertex(u); }

    bool has_vertex(const VertexType& u) const {
        return id_map.find(u) != id_map.end();
    }
    bool hasVertex(const VertexType& u) const { return has_vertex(u); }

    void add_edge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        uint32_t u_id = get_or_register_vertex(u);
        uint32_t v_id = get_or_register_vertex(v);

        insert_directed_edge(u_id, v_id, weight);
        if (!is_directed) {
            insert_directed_edge(v_id, u_id, weight);
        }
    }
    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        add_edge(u, v, weight);
    }
    void add_edge_dynamic(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        add_edge(u, v, weight);
    }
    void addEdgeDynamic(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        add_edge_dynamic(u, v, weight);
    }

    bool has_edge(const VertexType& u, const VertexType& v) const {
        auto it_u = id_map.find(u);
        auto it_v = id_map.find(v);
        if (it_u == id_map.end() || it_v == id_map.end()) return false;

        uint32_t target_id = it_v->second;
        const auto& node = vertex_table[it_u->second];
        EdgeChunk<WeightType>* chunk = node.head;

        while (chunk) {
            for (uint32_t i = 0; i < chunk->count; ++i) {
                if (chunk->destinations[i] == target_id) return true;
                if (chunk->destinations[i] > target_id) break; // Rani izlaz zbog sortiranosti
            }
            chunk = chunk->next_local;
        }
        return false;
    }
    bool hasEdge(const VertexType& u, const VertexType& v) const { return has_edge(u, v); }

    size_t get_degree(const VertexType& u) const {
        auto it = id_map.find(u);
        if (it == id_map.end()) return 0;
        return vertex_table[it->second].degree;
    }
    size_t getDegree(const VertexType& u) const { return get_degree(u); }

    template <typename Callback>
    void for_each_neighbor(const VertexType& u, Callback&& callback) const {
        auto it = id_map.find(u);
        if (it == id_map.end()) return;

        const auto& node = vertex_table[it->second];
        EdgeChunk<WeightType>* chunk = node.head;

        while (chunk) {
            if (chunk->next_local) {
                __builtin_prefetch(chunk->next_local, 0, 1);
            }
            for (uint32_t i = 0; i < chunk->count; ++i) {
                callback(reverse_id_map[chunk->destinations[i]], chunk->weights[i]);
            }
            chunk = chunk->next_local;
        }
    }
    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& callback) const {
        for_each_neighbor(u, std::forward<Callback>(callback));
    }

    template <typename Callback>
    void traverse_entire_graph(Callback&& callback) const {
        EdgeChunk<WeightType>* chunk = global_head;
        while (chunk) {
            if (chunk->next_global) {
                __builtin_prefetch(chunk->next_global, 0, 3);
            }
            for (uint32_t i = 0; i < chunk->count; ++i) {
                callback(reverse_id_map[chunk->destinations[i]], chunk->weights[i]);
            }
            chunk = chunk->next_global;
        }
    }
    template <typename Callback>
    void traverseEntireGraph(Callback&& callback) const {
        traverse_entire_graph(std::forward<Callback>(callback));
    }

    template <typename Callback>
    void bfs(const VertexType& start, Callback&& callback) const {
        if (!has_vertex(start)) return;
        std::unordered_set<VertexType> visited;
        std::queue<VertexType> q;

        visited.insert(start);
        q.push(start);

        while (!q.empty()) {
            VertexType current = q.front();
            q.pop();
            callback(current);

            forEachNeighbor(current, [&](const VertexType& nxt, const WeightType&) {
                if (!visited.contains(nxt)) {
                    visited.insert(nxt);
                    q.push(nxt);
                }
            });
        }
    }

    template <typename Callback>
    void dfs(const VertexType& start, Callback&& callback) const {
        if (!has_vertex(start)) return;
        std::unordered_set<VertexType> visited;
        std::stack<VertexType> s;

        s.push(start);

        while (!s.empty()) {
            VertexType current = s.top();
            s.pop();

            if (visited.contains(current)) continue;
            visited.insert(current);
            callback(current);

            forEachNeighbor(current, [&](const VertexType& nxt, const WeightType&) {
                if (!visited.contains(nxt)) {
                    s.push(nxt);
                }
            });
        }
    }

    std::unordered_map<VertexType, WeightType> dijkstra(const VertexType& start) const {
        std::unordered_map<VertexType, WeightType> dist;
        if (!has_vertex(start)) return dist;

        using DistPair = std::pair<WeightType, VertexType>;
        std::priority_queue<DistPair, std::vector<DistPair>, std::greater<DistPair>> pq;

        dist[start] = WeightType(0);
        pq.push({WeightType(0), start});

        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();

            if (d > dist[u]) continue;

            forEachNeighbor(u, [&](const VertexType& v, const WeightType& weight) {
                WeightType new_d = d + weight;
                auto it = dist.find(v);
                if (it == dist.end() || new_d < it->second) {
                    dist[v] = new_d;
                    pq.push({new_d, v});
                }
            });
        }
        return dist;
    }

    size_t vertex_count() const { return vertex_table.size(); }
    size_t numVertices() const { return vertex_count(); }

    size_t edge_count() const { return is_directed ? total_edges : total_edges / 2; }
    size_t numEdges() const { return edge_count(); }
};

#endif // CBLIST_GRAPH_HPP