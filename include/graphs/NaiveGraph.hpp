#ifndef NAIVE_GRAPH_HPP
#define NAIVE_GRAPH_HPP

// Adjacency list: most basic, "control group"

#include <vector>
#include <unordered_map>
#include <utility>
#include <cstddef>
#include <algorithm>
#include "RawGraph.hpp"
#include "Concepts.hpp"
#include "BaseGraph.hpp"

template <typename VertexType, Numeric WeightType = double>
class NaiveGraph : public BaseGraph<NaiveGraph<VertexType, WeightType>, VertexType, WeightType> {
private:

    std::unordered_map<VertexType, std::vector<std::pair<VertexType, WeightType>>> adjList;

public:
    using Base = BaseGraph<NaiveGraph<VertexType, WeightType>, VertexType, WeightType>;

    explicit NaiveGraph(bool directed = false) : Base(directed) {}

    explicit NaiveGraph(RawGraph<VertexType, WeightType>&& raw)
        : NaiveGraph(true) {

        for (const VertexType& vertex : raw.getReverseIdMap()) {
            this->addVertex(vertex);
        }
        raw.traverseEntireGraph([&](const VertexType& u, const VertexType& v, const WeightType& w) {
            this->addEdge(u, v, w);
        });
    }

    void addVertex(const VertexType& u) {
        if (!adjList.contains(u)) {
            adjList[u] = {};
        }
    }

    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        addVertex(u);
        addVertex(v);

        adjList[u].emplace_back(v, weight);

        if (!this->isDirected && !(u == v)) {
            adjList[v].emplace_back(u, weight);
        }
    }

    void addEdgeDynamic(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        addEdge(u, v, weight);
    }

    bool hasVertex(const VertexType& u) const {
        return adjList.contains(u);
    }

    bool hasEdge(const VertexType& u, const VertexType& v) const {
        auto it = adjList.find(u);
        if (it == adjList.end()) return false;
        for (const auto& [neighbor, _] : it->second) {
            if (neighbor == v) return true;
        }
        return false;
    }

    bool hasEdge(const VertexType& u, const VertexType& v, const WeightType& w) const {
        auto it = adjList.find(u);
        if (it == adjList.end()) return false;
        for (const auto& [neighbor, weight] : it->second) {
            if (neighbor == v && weight == w) return true;
        }
        return false;
    }

    std::size_t getDegree(const VertexType& u) const {
        auto it = adjList.find(u);
        return (it != adjList.end()) ? it->second.size() : 0;
    }

    std::size_t numVertices() const {
        return adjList.size();
    }

    std::size_t numEdges() const {
        std::size_t total = 0;
        for (const auto& [_, edges] : adjList) {
            total += edges.size();
        }
        return this->isDirected ? total : total / 2;
    }

    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& callback) const {
        auto it = adjList.find(u);
        if (it == adjList.end()) return;
        for (const auto& [neighbor, weight] : it->second) {
            callback(neighbor, weight);
        }
    }

    template <typename Callback>
    void traverseEntireGraph(Callback&& callback) const {
        for (const auto& [u, neighbors] : adjList) {
            for (const auto& [v, weight] : neighbors) {
                callback(u, v, weight);
            }
        }
    }

    std::size_t memoryUsageBytes() const {
        std::size_t bytes = 0;
        for (const auto& [u, neighbors] : adjList) {
            bytes += neighbors.capacity() * sizeof(std::pair<VertexType, WeightType>);
        }
        return bytes;
    }
};

template <typename VertexType, Numeric WeightType = double>
using Graph = NaiveGraph<VertexType, WeightType>;

static_assert(GraphReq<NaiveGraph<int, double>, int, double>);

#endif