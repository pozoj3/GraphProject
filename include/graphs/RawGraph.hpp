#ifndef RAW_GRAPH_HPP
#define RAW_GRAPH_HPP

#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstddef>
#include <utility>
#include "Concepts.hpp"
#include "BaseGraph.hpp"

template <typename VertexType, Numeric WeightType = double>
class RawGraph : public BaseGraph<RawGraph<VertexType, WeightType>, VertexType, WeightType> {
public:
    struct RawEdge {
        uint32_t src;
        uint32_t dst;
        WeightType weight;
    };

private:
    std::unordered_map<VertexType, uint32_t> idMap;
    std::vector<VertexType> reverseIdMap;
    std::vector<RawEdge> edgeList;

    uint32_t getOrRegisterVertex(const VertexType& u) {
        auto it = idMap.find(u);
        if (it != idMap.end()) return it->second;

        uint32_t newId = static_cast<uint32_t>(reverseIdMap.size());
        idMap[u] = newId;
        reverseIdMap.push_back(u);
        return newId;
    }

public:
    using Base = BaseGraph<RawGraph<VertexType, WeightType>, VertexType, WeightType>;

    explicit RawGraph(bool directed = false) : Base(directed) {}

    void reserve(std::size_t vertexCapacity, std::size_t edgeCapacity = 0) {
        idMap.reserve(vertexCapacity);
        reverseIdMap.reserve(vertexCapacity);
        if (edgeCapacity > 0) {
            edgeList.reserve(edgeCapacity);
        }
    }

    void addVertex(const VertexType& u) {
        getOrRegisterVertex(u);
    }

    bool hasVertex(const VertexType& u) const {
        return idMap.find(u) != idMap.end();
    }

    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        uint32_t uId = getOrRegisterVertex(u);
        uint32_t vId = getOrRegisterVertex(v);

        edgeList.push_back({uId, vId, weight});
        if (!this->isDirected && uId != vId) {
            edgeList.push_back({vId, uId, weight});
        }
    }

    void addEdgeDynamic(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        addEdge(u, v, weight);
    }

    bool hasEdge(const VertexType& u, const VertexType& v) const {
        auto itU = idMap.find(u);
        auto itV = idMap.find(v);
        if (itU == idMap.end() || itV == idMap.end()) return false;

        uint32_t uId = itU->second;
        uint32_t vId = itV->second;

        for (const auto& e : edgeList) {
            if (e.src == uId && e.dst == vId) return true;
        }
        return false;
    }

    bool hasEdge(const VertexType& u, const VertexType& v, const WeightType& w) const {
        auto itU = idMap.find(u);
        auto itV = idMap.find(v);
        if (itU == idMap.end() || itV == idMap.end()) return false;

        uint32_t uId = itU->second;
        uint32_t vId = itV->second;

        for (const auto& e : edgeList) {
            if (e.src == uId && e.dst == vId && e.weight == w) return true;
        }
        return false;
    }

    std::size_t getDegree(const VertexType& u) const {
        auto it = idMap.find(u);
        if (it == idMap.end()) return 0;
        uint32_t uId = it->second;

        std::size_t deg = 0;
        for (const auto& e : edgeList) {
            if (e.src == uId) ++deg;
        }
        return deg;
    }

    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& callback) const {
        auto it = idMap.find(u);
        if (it == idMap.end()) return;
        uint32_t uId = it->second;

        for (const auto& e : edgeList) {
            if (e.src == uId) {
                callback(reverseIdMap[e.dst], e.weight);
            }
        }
    }

    template <typename Callback>
    void traverseEntireGraph(Callback&& callback) const {
        for (const auto& e : edgeList) {
            callback(reverseIdMap[e.src], reverseIdMap[e.dst], e.weight);
        }
    }

    std::size_t numVertices() const {
        return reverseIdMap.size();
    }

    std::size_t numEdges() const {
        return this->isDirected ? edgeList.size() : edgeList.size() / 2;
    }

    const std::unordered_map<VertexType, uint32_t>& getIdMap() const { return idMap; }
    const std::vector<VertexType>& getReverseIdMap() const { return reverseIdMap; }
    const std::vector<RawEdge>& getEdgeList() const { return edgeList; }
};

static_assert(GraphReq<RawGraph<int, double>, int, double>);

#endif // RAW_GRAPH_HPP