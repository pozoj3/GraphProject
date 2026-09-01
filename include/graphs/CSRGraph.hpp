#ifndef CSR_GRAPH_HPP
#define CSR_GRAPH_HPP

#include <iostream>
#include <vector>
#include <unordered_map>
#include <concepts>
#include <span>
#include <algorithm>
#include <cstdint>

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <typename VertexType, Numeric WeightType = double>
class CSRGraph {
private:
    struct RawEdge {
        uint32_t src;
        uint32_t dst;
        WeightType weight;
    };

    std::unordered_map<VertexType, uint32_t> id_map;
    std::vector<VertexType> reverse_id_map;

    // Glavni CSR ravni nizovi (kontinuirana memorija)
    std::vector<uint64_t> row_offsets;
    std::vector<uint32_t> col_indices;
    std::vector<WeightType> weights;

    std::vector<RawEdge> edge_buffer; // Privremeni buffer za fazu gradnje
    bool is_finalized{false};
    bool is_directed{false};

    uint32_t get_or_create_vertex(const VertexType& u) {
        auto it = id_map.find(u);
        if (it != id_map.end()) return it->second;
        uint32_t new_id = static_cast<uint32_t>(reverse_id_map.size());
        id_map[u] = new_id;
        reverse_id_map.push_back(u);
        return new_id;
    }

public:
    explicit CSRGraph(bool directed = false) : is_directed(directed) {}

    void reserve(size_t num_vertices, size_t num_edges) {
        id_map.reserve(num_vertices);
        reverse_id_map.reserve(num_vertices);
        edge_buffer.reserve(is_directed ? num_edges : num_edges * 2);
    }

    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        if (is_finalized) return;
        uint32_t u_id = get_or_create_vertex(u);
        uint32_t v_id = get_or_create_vertex(v);

        edge_buffer.push_back({u_id, v_id, weight});
        if (!is_directed) {
            edge_buffer.push_back({v_id, u_id, weight});
        }
    }

    void finalize() {
        if (is_finalized) return;

        size_t V = reverse_id_map.size();
        size_t E = edge_buffer.size();

        row_offsets.assign(V + 1, 0);
        col_indices.resize(E);
        weights.resize(E);

        // Prebrojavanje stupnjeva za računanje offseta
        for (const auto& e : edge_buffer) {
            row_offsets[e.src + 1]++;
        }
        for (size_t i = 0; i < V; ++i) {
            row_offsets[i + 1] += row_offsets[i];
        }

        std::vector<uint64_t> current_offsets = row_offsets;
        for (const auto& e : edge_buffer) {
            uint64_t dest_idx = current_offsets[e.src]++;
            col_indices[dest_idx] = e.dst;
            weights[dest_idx] = e.weight;
        }

        // Sortiranje susjeda svakog vrha za brzi binarni lookup
        for (size_t i = 0; i < V; ++i) {
            uint64_t start = row_offsets[i];
            uint64_t end = row_offsets[i + 1];
            if (start == end) continue;

            std::vector<std::pair<uint32_t, WeightType>> temp(end - start);
            for (uint64_t j = start; j < end; ++j) {
                temp[j - start] = {col_indices[j], weights[j]};
            }
            std::sort(temp.begin(), temp.end(), [](const auto& a, const auto& b) {
                return a.first < b.first;
            });
            for (uint64_t j = start; j < end; ++j) {
                col_indices[j] = temp[j - start].first;
                weights[j] = temp[j - start].second;
            }
        }

        // Čišćenje privremenog buffera da se oslobodi RAM
        edge_buffer.clear();
        edge_buffer.shrink_to_fit();
        is_finalized = true;
    }

    bool hasEdge(const VertexType& u, const VertexType& v) const {
        if (!is_finalized) return false;
        auto it_u = id_map.find(u);
        auto it_v = id_map.find(v);
        if (it_u == id_map.end() || it_v == id_map.end()) return false;

        uint32_t u_id = it_u->second;
        uint32_t v_id = it_v->second;
        uint64_t start = row_offsets[u_id];
        uint64_t end = row_offsets[u_id + 1];

        // Binarna pretraga na sortiranim susjedima
        auto it = std::lower_bound(col_indices.begin() + start, col_indices.begin() + end, v_id);
        return (it != col_indices.begin() + end && *it == v_id);
    }

    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& callback) const {
        if (!is_finalized) return;
        auto it = id_map.find(u);
        if (it == id_map.end()) return;

        uint32_t u_id = it->second;
        uint64_t start = row_offsets[u_id];
        uint64_t end = row_offsets[u_id + 1];

        for (uint64_t i = start; i < end; ++i) {
            callback(reverse_id_map[col_indices[i]], weights[i]);
        }
    }

    size_t numVertices() const { return reverse_id_map.size(); }
    size_t numEdges() const { return col_indices.size() / (is_directed ? 1 : 2); }
};

#endif // CSR_GRAPH_HPP