#ifndef NAIVE_GRAPH_HPP
#define NAIVE_GRAPH_HPP

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>
#include <algorithm>
#include <utility>
#include <cstdint>
#include <limits>
#include "Concepts.hpp"

template <typename VertexType, Numeric WeightType = double>
class NaiveGraph {
private:
    std::unordered_map<VertexType, std::vector<std::pair<VertexType, WeightType>>> adj_list;
    bool is_directed;

public:
    explicit NaiveGraph(bool directed = false) : is_directed(directed) {}

    void add_vertex(const VertexType& u) {
        if (!adj_list.contains(u)) {
            adj_list[u] = {};
        }
    }

    void add_edge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        add_vertex(u);
        add_vertex(v);

        adj_list[u].emplace_back(v, weight);
        if (!is_directed) {
            adj_list[v].emplace_back(u, weight);
        }
    }

    void addVertex(const VertexType& u) { add_vertex(u); }
    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        add_edge(u, v, weight);
    }

    // dynamic edge/vertex adding (not nececssery)
    void add_edge_dynamic(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        add_edge(u, v, weight);
    }
    void addEdgeDynamic(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        add_edge_dynamic(u, v, weight);
    }

    bool has_vertex(const VertexType& u) const {
        return adj_list.contains(u);
    }

    bool has_edge(const VertexType& u, const VertexType& v) const {
        auto it = adj_list.find(u);
        if (it == adj_list.end()) return false;
        for (const auto& [neighbor, _] : it->second) {
            if (neighbor == v) return true;
        }
        return false;
    }

    bool hasVertex(const VertexType& u) const { return has_vertex(u); }
    bool hasEdge(const VertexType& u, const VertexType& v) const { return has_edge(u, v); }

    size_t get_degree(const VertexType& u) const {
        auto it = adj_list.find(u);
        return (it != adj_list.end()) ? it->second.size() : 0;
    }
    size_t getDegree(const VertexType& u) const { return get_degree(u); }

    template <typename Callback>
    void for_each_neighbor(const VertexType& u, Callback&& callback) const {
        auto it = adj_list.find(u);
        if (it == adj_list.end()) return;
        for (const auto& [neighbor, weight] : it->second) {
            callback(neighbor, weight);
        }
    }

    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& callback) const {
        for_each_neighbor(u, std::forward<Callback>(callback));
    }


    template <typename Callback>
    void traverse_entire_graph(Callback&& callback) const {
        for (const auto& [u, neighbors] : adj_list) {
            for (const auto& [v, weight] : neighbors) {
                callback(v, weight);
            }
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

    size_t vertex_count() const { return adj_list.size(); }
    size_t numVertices() const { return vertex_count(); }

    size_t edge_count() const {
        size_t total = 0;
        for (const auto& [_, edges] : adj_list) {
            total += edges.size();
        }
        return is_directed ? total : total / 2;
    }
    size_t numEdges() const { return edge_count(); }
};

template <typename VertexType, Numeric WeightType = double>
using Graph = NaiveGraph<VertexType, WeightType>;

#endif // NAIVE_GRAPH_HPP