#ifndef CBLIST_GRAPH_HPP
#define CBLIST_GRAPH_HPP

#include <iostream>
#include <vector>
#include <unordered_map>
#include <concepts>
#include <span>
#include <memory>
#include <cstdint>
#include <algorithm>

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <Numeric WeightType>
struct Edge {
    uint32_t target;
    WeightType weight;
};

// Blok susjeda točno poravnat na keš liniju (64B)
template <Numeric WeightType, size_t CAPACITY = 4>
struct alignas(64) SmallChunk {
    uint32_t size{0};
    Edge<WeightType> edges[CAPACITY];
    SmallChunk* next_chunk{nullptr}; // Za uvezivanje u GTChain

    bool is_full() const { return size >= CAPACITY; }

    bool add_edge(uint32_t target, WeightType weight) {
        if (is_full()) return false;
        edges[size++] = {target, weight};
        return true;
    }
};

template <Numeric WeightType>
struct alignas(64) VertexRecord {
    uint32_t degree{0};
    uint8_t level{0}; // 0 = SmallChunk
    bool is_deleted{false};
    SmallChunk<WeightType>* chunk_head{nullptr};
    SmallChunk<WeightType>* traversal_ptr{nullptr};
};

template <typename VertexType, Numeric WeightType = double>
class CBListGraph {
private:
    std::unordered_map<VertexType, uint32_t> id_map;
    std::vector<VertexType> reverse_id_map;
    std::vector<VertexRecord<WeightType>> vertex_table;

    SmallChunk<WeightType>* gtchain_head{nullptr};
    SmallChunk<WeightType>* gtchain_tail{nullptr};

    uint64_t num_edges_count{0};
    bool is_directed{false};

    uint32_t get_or_create_vertex(const VertexType& u) {
        auto it = id_map.find(u);
        if (it != id_map.end()) return it->second;

        uint32_t new_id = static_cast<uint32_t>(vertex_table.size());
        id_map[u] = new_id;
        reverse_id_map.push_back(u);

        auto* chunk = new SmallChunk<WeightType>();
        VertexRecord<WeightType> record;
        record.degree = 0;
        record.level = 0;
        record.chunk_head = chunk;
        record.traversal_ptr = chunk;
        vertex_table.push_back(record);

        if (!gtchain_head) {
            gtchain_head = chunk;
            gtchain_tail = chunk;
        } else {
            gtchain_tail->next_chunk = chunk;
            gtchain_tail = chunk;
        }
        return new_id;
    }

    void add_directed_edge_internal(uint32_t u_id, uint32_t v_id, WeightType weight) {
        auto& record = vertex_table[u_id];
        auto* curr = record.chunk_head;

        if (curr->add_edge(v_id, weight)) {
            record.degree++;
            num_edges_count++;
            return;
        }

        // Prelazak u novi chunk ako je trenutni pun
        auto* new_chunk = new SmallChunk<WeightType>();
        new_chunk->add_edge(v_id, weight);
        new_chunk->next_chunk = curr->next_chunk;
        curr->next_chunk = new_chunk;

        if (gtchain_tail == curr) {
            gtchain_tail = new_chunk;
        }

        record.degree++;
        num_edges_count++;
    }

public:
    explicit CBListGraph(bool directed = false) : is_directed(directed) {}

    ~CBListGraph() {
        SmallChunk<WeightType>* curr = gtchain_head;
        while (curr) {
            SmallChunk<WeightType>* next = curr->next_chunk;
            delete curr;
            curr = next;
        }
    }

    void reserve(size_t num_vertices) {
        vertex_table.reserve(num_vertices);
        reverse_id_map.reserve(num_vertices);
        id_map.reserve(num_vertices);
    }

    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        uint32_t u_id = get_or_create_vertex(u);
        uint32_t v_id = get_or_create_vertex(v);

        add_directed_edge_internal(u_id, v_id, weight);
        if (!is_directed) {
            add_directed_edge_internal(v_id, u_id, weight);
        }
    }

    bool hasEdge(const VertexType& u, const VertexType& v) const {
        auto it_u = id_map.find(u);
        auto it_v = id_map.find(v);
        if (it_u == id_map.end() || it_v == id_map.end()) return false;

        uint32_t u_id = it_u->second;
        uint32_t target_id = it_v->second;
        const auto& record = vertex_table[u_id];

        SmallChunk<WeightType>* curr = record.chunk_head;
        while (curr) {
            for (uint32_t i = 0; i < curr->size; ++i) {
                if (curr->edges[i].target == target_id) return true;
            }
            if (record.level == 0) break;
            curr = curr->next_chunk;
        }
        return false;
    }

    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& callback) const {
        auto it = id_map.find(u);
        if (it == id_map.end()) return;

        uint32_t u_id = it->second;
        const auto& record = vertex_table[u_id];
        SmallChunk<WeightType>* curr = record.chunk_head;

        while (curr) {
            if (curr->next_chunk) {
                __builtin_prefetch(curr->next_chunk, 0, 1);
            }
            for (uint32_t i = 0; i < curr->size; ++i) {
                callback(reverse_id_map[curr->edges[i].target], curr->edges[i].weight);
            }
            if (record.level == 0) break;
            curr = curr->next_chunk;
        }
    }

    // Globalni prolaz cijelim grafom preko GTChain lanca
    template <typename Callback>
    void traverseEntireGraph(Callback&& callback) const {
        SmallChunk<WeightType>* curr = gtchain_head;
        while (curr) {
            if (curr->next_chunk) {
                __builtin_prefetch(curr->next_chunk, 0, 3);
            }
            for (uint32_t i = 0; i < curr->size; ++i) {
                callback(curr->edges[i].target, curr->edges[i].weight);
            }
            curr = curr->next_chunk;
        }
    }

    size_t numVertices() const { return vertex_table.size(); }
    size_t numEdges() const { return is_directed ? num_edges_count : num_edges_count / 2; }
};

#endif // CBLIST_GRAPH_HPP