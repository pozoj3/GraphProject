#ifndef VIRTUAL_BASE_GRAPH_HPP
#define VIRTUAL_BASE_GRAPH_HPP

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>
#include <functional>
#include <utility>
#include <stdexcept>
#include "../graphs/Concepts.hpp"

template <typename VertexType, Numeric WeightType = double>
class VirtualBaseGraph {
protected:
    bool isDirected;

public:
    using NeighborCallback = std::function<void(const VertexType&, const WeightType&)>;
    using EdgeCallback = std::function<void(const VertexType&, const VertexType&, const WeightType&)>;

    explicit VirtualBaseGraph(bool directed = false) : isDirected(directed) {}
    virtual ~VirtualBaseGraph() = default;


    virtual void addVertex(const VertexType& u) = 0;
    virtual void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) = 0;
    virtual void addEdgeDynamic(const VertexType& u, const VertexType& v, const WeightType& weight = 1) = 0;

    virtual bool hasVertex(const VertexType& u) const = 0;
    virtual bool hasEdge(const VertexType& u, const VertexType& v) const = 0;
    virtual bool hasEdge(const VertexType& u, const VertexType& v, const WeightType& w) const = 0;
    virtual std::size_t getDegree(const VertexType& u) const = 0;

    virtual void forEachNeighbor(const VertexType& u, const NeighborCallback& callback) const = 0;
    virtual void traverseEntireGraph(const EdgeCallback& callback) const = 0;

    virtual std::size_t numVertices() const = 0;
    virtual std::size_t numEdges() const = 0;

    bool getIsDirected() const { return isDirected; }

    void bfs(const VertexType& start, const std::function<void(const VertexType&)>& callback) const {
        if (!hasVertex(start)) return;
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

    void dfs(const VertexType& start, const std::function<void(const VertexType&)>& callback) const {
        if (!hasVertex(start)) return;
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
        if (!hasVertex(start)) return dist;

        using DistPair = std::pair<WeightType, VertexType>;
        std::priority_queue<DistPair, std::vector<DistPair>, std::greater<DistPair>> pq;

        dist[start] = WeightType(0);
        pq.push({WeightType(0), start});

        while (!pq.empty()) {
            auto [d, u] = pq.top();
            pq.pop();

            if (d > dist[u]) continue;

            forEachNeighbor(u, [&](const VertexType& v, const WeightType& weight) {
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

#endif // VIRTUAL_BASE_GRAPH_HPP