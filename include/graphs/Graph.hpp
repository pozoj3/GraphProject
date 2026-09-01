#ifndef NAIVE_GRAPH_HPP
#define NAIVE_GRAPH_HPP

#include <iostream>
#include <vector>
#include <unordered_map>
#include <concepts>
#include <algorithm>

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <typename VertexType, Numeric WeightType = double>
class NaiveGraph {
private:
    std::unordered_map<VertexType, std::vector<std::pair<VertexType, WeightType>>> adjList;
    bool is_directed;

public:
    explicit NaiveGraph(bool directed = false) : is_directed(directed) {}

    void addVertex(const VertexType& u) {
        if (adjList.find(u) == adjList.end()) {
            adjList[u] = {};
        }
    }

    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        adjList[u].push_back({v, weight});
        if (!is_directed) {
            adjList[v].push_back({u, weight});
        }
    }

    bool hasVertex(const VertexType& u) const {
        return adjList.find(u) != adjList.end();
    }

    bool hasEdge(const VertexType& u, const VertexType& v) const {
        auto it = adjList.find(u);
        if (it == adjList.end()) return false;
        for (const auto& edge : it->second) {
            if (edge.first == v) return true;
        }
        return false;
    }

    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& callback) const {
        auto it = adjList.find(u);
        if (it == adjList.end()) return;
        for (const auto& [neighbor, weight] : it->second) {
            callback(neighbor, weight);
        }
    }

    size_t numVertices() const { return adjList.size(); }
    
    size_t numEdges() const {
        size_t count = 0;
        for (const auto& [_, edges] : adjList) {
            count += edges.size();
        }
        return is_directed ? count : count / 2;
    }
};

#endif // NAIVE_GRAPH_HPP