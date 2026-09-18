#ifndef BASE_GRAPH_HPP
#define BASE_GRAPH_HPP

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>
#include <algorithm>
#include <utility>
#include <stdexcept>
#include "Concepts.hpp"

template <typename DerivedGraph, typename VertexType, Numeric WeightType = double>
class BaseGraph {
private:
    const DerivedGraph& derived() const {
        return *static_cast<const DerivedGraph*>(this);
    }
    DerivedGraph& derived() {
        return *static_cast<DerivedGraph*>(this);
    }

protected:
    bool isDirected;

public:
    explicit BaseGraph(bool directed = false) : isDirected(directed) {}

    bool getIsDirected() const { return isDirected; }

    template <typename Callback>
    void bfs(const VertexType& start, Callback&& callback) const {
        if (!derived().hasVertex(start)) return;
        std::unordered_set<VertexType> visited;
        std::queue<VertexType> q;

        visited.insert(start);
        q.push(start);

        while (!q.empty()) {
            VertexType current = q.front();
            q.pop();
            callback(current);

            derived().forEachNeighbor(current, [&](const VertexType& nxt, const WeightType&) {
                if (!visited.contains(nxt)) {
                    visited.insert(nxt);
                    q.push(nxt);
                }
            });
        }
    }

    template <typename Callback>
    void dfs(const VertexType& start, Callback&& callback) const {
        if (!derived().hasVertex(start)) return;
        std::unordered_set<VertexType> visited;
        std::stack<VertexType> s;

        s.push(start);

        while (!s.empty()) {
            VertexType current = s.top();
            s.pop();

            if (visited.contains(current)) continue;
            visited.insert(current);
            callback(current);

            derived().forEachNeighbor(current, [&](const VertexType& nxt, const WeightType&) {
                if (!visited.contains(nxt)) {
                    s.push(nxt);
                }
            });
        }
    }

    std::unordered_map<VertexType, WeightType> dijkstra(const VertexType& start) const {
        std::unordered_map<VertexType, WeightType> dist;
        if (!derived().hasVertex(start)) return dist;

        using DistPair = std::pair<WeightType, VertexType>;
        std::priority_queue<DistPair, std::vector<DistPair>, std::greater<DistPair>> pq;

        dist[start] = WeightType(0);
        pq.push({WeightType(0), start});

        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();

            if (d > dist[u]) continue;

            derived().forEachNeighbor(u, [&](const VertexType& v, const WeightType& weight) {
                if (weight < WeightType(0)) {
                    throw std::invalid_argument("Dijkstra ne podrzava negativne tezine!");
                }
                WeightType new_d = d + weight;
                if (auto it = dist.find(v); it == dist.end() || new_d < it->second) {
                    dist.insert_or_assign(it, v, new_d);
                    pq.push({new_d, v});
                }
            });
        }
        return dist;
    }
};

#endif // BASE_GRAPH_HPP