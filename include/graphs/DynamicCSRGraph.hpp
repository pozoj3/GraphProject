#ifndef DYNAMIC_CSR_GRAPH_HPP
#define DYNAMIC_CSR_GRAPH_HPP

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <utility>
#include "Concepts.hpp"
#include "BaseGraph.hpp"
#include "RawGraph.hpp"

template <typename VertexType, Numeric WeightType = double>
class DynamicCSRGraph : public BaseGraph<DynamicCSRGraph<VertexType, WeightType>, VertexType, WeightType> {
public:
    struct RawEdge {
        uint32_t src;
        uint32_t dst;
        WeightType weight;
    };

private:
    std::unordered_map<VertexType, uint32_t> idMap;
    std::vector<VertexType> reverseIdMap;

    std::vector<uint64_t> offsets;
    std::vector<uint32_t> columnIndices;
    std::vector<WeightType> values;

    std::vector<RawEdge> edgeBuffer;

    uint32_t getOrRegisterVertex(const VertexType& u) {
        auto it = idMap.find(u);
        if (it != idMap.end()) return it->second;

        uint32_t newId = static_cast<uint32_t>(reverseIdMap.size());
        idMap[u] = newId;
        reverseIdMap.push_back(u);
        return newId;
    }

    void rebuild() {
        const std::size_t vCount = reverseIdMap.size();
        const std::size_t eCount = edgeBuffer.size();

        offsets.assign(vCount + 1, 0);
        columnIndices.resize(eCount);
        values.resize(eCount);

        for (const auto& e : edgeBuffer) {
            offsets[e.src + 1]++;
        }
        for (std::size_t i = 0; i < vCount; ++i) {
            offsets[i + 1] += offsets[i];
        }

        std::vector<uint64_t> cursor = offsets;
        for (const auto& e : edgeBuffer) {
            uint64_t idx = cursor[e.src]++;
            columnIndices[idx] = e.dst;
            values[idx] = e.weight;
        }

        std::vector<uint32_t> newColumnIndices;
        std::vector<WeightType> newValues;
        std::vector<uint64_t> newOffsets(vCount + 1, 0);

        newColumnIndices.reserve(eCount);
        newValues.reserve(eCount);

        for (std::size_t i = 0; i < vCount; ++i) {
            uint64_t start = offsets[i];
            uint64_t end = offsets[i + 1];

            if (start < end) {
                std::vector<std::pair<uint32_t, WeightType>> neighbors(end - start);
                for (uint64_t j = start; j < end; ++j) {
                    neighbors[j - start] = {columnIndices[j], values[j]};
                }

                std::sort(neighbors.begin(), neighbors.end());

                auto lastUnique = std::unique(neighbors.begin(), neighbors.end(),
                    [](const auto& a, const auto& b) {
                        return a.first == b.first;
                    });
                neighbors.erase(lastUnique, neighbors.end());

                for (const auto& [dst, w] : neighbors) {
                    newColumnIndices.push_back(dst);
                    newValues.push_back(w);
                }
            }
            newOffsets[i + 1] = newColumnIndices.size();
        }

        columnIndices = std::move(newColumnIndices);
        values = std::move(newValues);
        offsets = std::move(newOffsets);
    }

public:
    using Base = BaseGraph<DynamicCSRGraph<VertexType, WeightType>, VertexType, WeightType>;

    explicit DynamicCSRGraph(bool directed = false) : Base(directed) {
        offsets.assign(1, 0);
    }
    
    explicit DynamicCSRGraph(RawGraph<VertexType, WeightType>&& raw)
        : Base(true) {
        std::size_t vCount = raw.numVertices();
        std::size_t eCount = raw.numEdges();

        idMap.reserve(vCount);
        reverseIdMap.reserve(vCount);
        edgeBuffer.reserve(eCount);

        for (uint32_t u = 0; u < vCount; ++u) {
            getOrRegisterVertex(static_cast<VertexType>(u));
        }

        for (uint32_t u = 0; u < vCount; ++u) {
            raw.forEachNeighbor(static_cast<VertexType>(u), [&](const VertexType& v, WeightType w) {
                uint32_t uId = getOrRegisterVertex(static_cast<VertexType>(u));
                uint32_t vId = getOrRegisterVertex(v);
                edgeBuffer.push_back({uId, vId, w});
            });
        }

        rebuild();
    }

    void reserve(std::size_t vertexCapacity, std::size_t edgeCapacity = 0) {
        idMap.reserve(vertexCapacity);
        reverseIdMap.reserve(vertexCapacity);
        if (edgeCapacity > 0) {
            edgeBuffer.reserve(edgeCapacity);
        }
    }

    void addVertex(const VertexType& u) {
        if (!hasVertex(u)) {
            getOrRegisterVertex(u);
            rebuild();
        }
    }

    bool hasVertex(const VertexType& u) const {
        return idMap.find(u) != idMap.end();
    }

    void addEdge(const VertexType& u, const VertexType& v, const WeightType& weight = 1) {
        uint32_t uId = getOrRegisterVertex(u);
        uint32_t vId = getOrRegisterVertex(v);

        edgeBuffer.push_back({uId, vId, weight});
        if (!this->isDirected && uId != vId) {
            edgeBuffer.push_back({vId, uId, weight});
        }
        rebuild();
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

        auto first = columnIndices.begin() + offsets[uId];
        auto last = columnIndices.begin() + offsets[uId + 1];
        return std::binary_search(first, last, vId);
    }

    bool hasEdge(const VertexType& u, const VertexType& v, const WeightType& w) const {
        auto itU = idMap.find(u);
        auto itV = idMap.find(v);
        if (itU == idMap.end() || itV == idMap.end()) return false;

        uint32_t uId = itU->second;
        uint32_t vId = itV->second;

        uint64_t start = offsets[uId];
        uint64_t end = offsets[uId + 1];
        for (uint64_t i = start; i < end; ++i) {
            if (columnIndices[i] == vId && values[i] == w) return true;
        }
        return false;
    }

    std::size_t getDegree(const VertexType& u) const {
        auto it = idMap.find(u);
        if (it == idMap.end()) return 0;
        uint32_t uId = it->second;
        return offsets[uId + 1] - offsets[uId];
    }

    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& callback) const {
        auto it = idMap.find(u);
        if (it == idMap.end()) return;

        uint32_t uId = it->second;
        uint64_t start = offsets[uId];
        uint64_t end = offsets[uId + 1];

        for (uint64_t i = start; i < end; ++i) {
            callback(reverseIdMap[columnIndices[i]], values[i]);
        }
    }

    template <typename Callback>
    void traverseEntireGraph(Callback&& callback) const {
        for (std::size_t uId = 0; uId < reverseIdMap.size(); ++uId) {
            uint64_t start = offsets[uId];
            uint64_t end = offsets[uId + 1];
            for (uint64_t i = start; i < end; ++i) {
                callback(reverseIdMap[uId], reverseIdMap[columnIndices[i]], values[i]);
            }
        }
    }

    std::size_t numVertices() const {
        return reverseIdMap.size();
    }

    std::size_t numEdges() const {
        return this->isDirected ? columnIndices.size() : columnIndices.size() / 2;
    }

    const std::unordered_map<VertexType, uint32_t>& getIdMap() const { return idMap; }
    const std::vector<VertexType>& getReverseIdMap() const { return reverseIdMap; }
    const std::vector<uint64_t>& getOffsets() const { return offsets; }
    const std::vector<uint32_t>& getColumnIndices() const { return columnIndices; }
    const std::vector<WeightType>& getValues() const { return values; }
};

static_assert(GraphReq<DynamicCSRGraph<int, double>, int, double>);

#endif // DYNAMIC_CSR_GRAPH_HPP