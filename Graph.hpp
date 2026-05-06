#ifndef GRAPHPROJECT_GRAPH_HPP
#define GRAPHPROJECT_GRAPH_HPP

#include <iostream>
#include <vector>
#include <unordered_map>
#include <concepts>

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <typename VertexType, Numeric WeightType>
class Graph {
private:
    std::unordered_map<VertexType, std::vector<std::pair<VertexType, WeightType>>> adjList;

public:
    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight) {
        adjList[u].push_back({v, weight});
        adjList[v].push_back({u, weight});
    }

    const std::vector<std::pair<VertexType, WeightType>>& getNeighbors(const VertexType& vertex) const {
        static const std::vector<std::pair<VertexType, WeightType>> empty_list;
        auto it = adjList.find(vertex);
        if (it != adjList.end()) {
            return it->second;
        }
        return empty_list;
    }

    void printGraph() const {
        for (const auto& pair : adjList) {
            std::cout << "Vertex " << pair.first << " is connected to: ";
            for (const auto& neighbor : pair.second) {
                std::cout << "[" << neighbor.first << " (weight: " << neighbor.second << ")] ";
            }
            std::cout << '\n';
        }
    }
};

#endif //GRAPHPROJECT_GRAPH_HPP
