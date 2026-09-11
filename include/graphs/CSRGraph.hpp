#ifndef CSR_GRAPH_HPP
#define CSR_GRAPH_HPP

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>
#include <span>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <limits>
#include "Concepts.hpp"

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

    std::vector<uint64_t> offsets;
    std::vector<uint32_t> column_indices;
    std::vector<WeightType> values;

    std::vector<RawEdge> edge_buffer;
    bool is_finalized{false};
    bool is_directed{false};

    uint32_t get_or_register_vertex(const VertexType& u) {
        auto it = id_map.find(u);
        if (it != id_map.end()) return it->second;

        uint32_t new_id = static_cast<uint32_t>(reverse_id_map.size());
        id_map[u] = new_id;
        reverse_id_map.push_back(u);
        return new_id;
    }

public:
    explicit CSRGraph(bool directed = false) : is_directed(directed) {}

    void reserve(size_t vertex_capacity, size_t edge_capacity = 0) {
        id_map.reserve(vertex_capacity);
        reverse_id_map.reserve(vertex_capacity);
        if (edge_capacity > 0) {
            edge_buffer.reserve(edge_capacity);
        }
    }

    void add_vertex(const VertexType& u) {
        get_or_register_vertex(u);
        is_finalized = false;
    }
    void addVertex(const VertexType& u) { add_vertex(u); }

    bool has_vertex(const VertexType& u) const {
        return id_map.find(u) != id_map.end();
    }
    bool hasVertex(const VertexType& u) const { return has_vertex(u); }

    void add_edge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        uint32_t u_id = get_or_register_vertex(u);
        uint32_t v_id = get_or_register_vertex(v);

        edge_buffer.push_back({u_id, v_id, weight});
        if (!is_directed) {
            edge_buffer.push_back({v_id, u_id, weight});
        }
        is_finalized = false;
    }

    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        add_edge(u, v, weight);
    }

    void finalize() {
        if (is_finalized) return;

        const size_t v_count = reverse_id_map.size();
        const size_t e_count = edge_buffer.size();

        offsets.assign(v_count + 1, 0);
        column_indices.resize(e_count);
        values.resize(e_count);

        for (const auto& e : edge_buffer) {
            offsets[e.src + 1]++;
        }
        for (size_t i = 0; i < v_count; ++i) {
            offsets[i + 1] += offsets[i];
        }

        std::vector<uint64_t> cursor = offsets;
        for (const auto& e : edge_buffer) {
            uint64_t idx = cursor[e.src]++;
            column_indices[idx] = e.dst;
            values[idx] = e.weight;
        }

        for (size_t i = 0; i < v_count; ++i) {
            uint64_t start = offsets[i];
            uint64_t end = offsets[i + 1];
            if (start == end) continue;

            std::vector<std::pair<uint32_t, WeightType>> neighbors(end - start);
            for (uint64_t j = start; j < end; ++j) {
                neighbors[j - start] = {column_indices[j], values[j]};
            }
            std::sort(neighbors.begin(), neighbors.end());

            for (uint64_t j = start; j < end; ++j) {
                column_indices[j] = neighbors[j - start].first;
                values[j] = neighbors[j - start].second;
            }
        }

        edge_buffer.clear();
        edge_buffer.shrink_to_fit();
        is_finalized = true;
    }

    bool has_edge(const VertexType& u, const VertexType& v) const {
        if (!is_finalized) return false;

        auto it_u = id_map.find(u);
        auto it_v = id_map.find(v);
        if (it_u == id_map.end() || it_v == id_map.end()) return false;

        uint32_t u_id = it_u->second;
        uint32_t v_id = it_v->second;

        auto first = column_indices.begin() + offsets[u_id];
        auto last = column_indices.begin() + offsets[u_id + 1];
        return std::binary_search(first, last, v_id);
    }
    bool hasEdge(const VertexType& u, const VertexType& v) const { return has_edge(u, v); }

    size_t get_degree(const VertexType& u) const {
        auto it = id_map.find(u);
        if (it == id_map.end()) return 0;
        if (!is_finalized) return 0;
        uint32_t u_id = it->second;
        return offsets[u_id + 1] - offsets[u_id];
    }
    size_t getDegree(const VertexType& u) const { return get_degree(u); }

    template <typename Callback>
    void for_each_neighbor(const VertexType& u, Callback&& callback) const {
        if (!is_finalized) return;
        auto it = id_map.find(u);
        if (it == id_map.end()) return;

        uint32_t u_id = it->second;
        uint64_t start = offsets[u_id];
        uint64_t end = offsets[u_id + 1];

        for (uint64_t i = start; i < end; ++i) {
            callback(reverse_id_map[column_indices[i]], values[i]);
        }
    }

    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& callback) const {
        for_each_neighbor(u, std::forward<Callback>(callback));
    }

    
    template <typename Callback>
    void traverse_entire_graph(Callback&& callback) const {
        if (!is_finalized) return;
        const size_t e_count = column_indices.size();
        for (size_t i = 0; i < e_count; ++i) {
            callback(reverse_id_map[column_indices[i]], values[i]);
        }
    }

    template <typename Callback>
    void traverseEntireGraph(Callback&& callback) const {
        traverse_entire_graph(std::forward<Callback>(callback));
    }

    
    template <typename Callback>
    void bfs(const VertexType& start, Callback&& callback) const {
        if (!is_finalized || !has_vertex(start)) return;
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
        if (!is_finalized || !has_vertex(start)) return;
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
        if (!is_finalized || !has_vertex(start)) return dist;

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

    size_t vertex_count() const { return reverse_id_map.size(); }
    size_t numVertices() const { return vertex_count(); }

    size_t edge_count() const {
        size_t raw_edges = is_finalized ? column_indices.size() : edge_buffer.size();
        return is_directed ? raw_edges : raw_edges / 2;
    }
    size_t numEdges() const { return edge_count(); }
};

#endif // CSR_GRAPH_HPP