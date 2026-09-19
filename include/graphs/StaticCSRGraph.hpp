#ifndef STATIC_CSR_GRAPH_HPP
#define STATIC_CSR_GRAPH_HPP

// Immutable CSR built once from a RawGraph/DynamicCSRGraph, throws error if mutated

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <stdexcept>
#include "Concepts.hpp"
#include "BaseGraph.hpp"
#include "RawGraph.hpp"
#include "DynamicCSRGraph.hpp"

template <typename VertexType, Numeric WeightType = double>
class StaticCSRGraph : public BaseGraph<StaticCSRGraph<VertexType, WeightType>, VertexType, WeightType> {
private:
    std::unordered_map<VertexType, uint32_t> idMap;
    std::vector<VertexType> reverseIdMap;

    std::vector<uint64_t> offsets;
    std::vector<uint32_t> columnIndices;
    std::vector<WeightType> values;

    struct InternalEdge {
        uint32_t src;
        uint32_t dst;
        WeightType weight;
    };

    void buildFromEdgeList(const std::vector<InternalEdge>& edgeList) {
        const std::size_t vCount = reverseIdMap.size();
        const std::size_t eCount = edgeList.size();

        offsets.assign(vCount + 1, 0);
        columnIndices.resize(eCount);
        values.resize(eCount);

        for (const auto& e : edgeList) {
            offsets[e.src + 1]++;
        }
        for (std::size_t i = 0; i < vCount; ++i) {
            offsets[i + 1] += offsets[i];
        }

        std::vector<uint64_t> cursor = offsets;
        for (const auto& e : edgeList) {
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
    using Base = BaseGraph<StaticCSRGraph<VertexType, WeightType>, VertexType, WeightType>;

    explicit StaticCSRGraph(bool directed = false) : Base(directed) {
        offsets.assign(1, 0);
    }

    explicit StaticCSRGraph(RawGraph<VertexType, WeightType>&& raw)
        : Base(true) {
        std::size_t vCount = raw.numVertices();

        idMap.reserve(vCount);
        reverseIdMap.reserve(vCount);

        for (const VertexType& vertex : raw.getReverseIdMap()) {
            uint32_t id = static_cast<uint32_t>(reverseIdMap.size());
            idMap[vertex] = id;
            reverseIdMap.push_back(vertex);
        }

        std::vector<InternalEdge> edges;
        edges.reserve(raw.getEdgeList().size());
        raw.traverseEntireGraph([&](const VertexType& u, const VertexType& v, const WeightType& w) {
            edges.push_back({idMap.at(u), idMap.at(v), w});
        });

        buildFromEdgeList(edges);
    }

    explicit StaticCSRGraph(const RawGraph<VertexType, WeightType>& raw)
        : StaticCSRGraph(RawGraph<VertexType, WeightType>(raw)) {}

    explicit StaticCSRGraph(const DynamicCSRGraph<VertexType, WeightType>& dyn)
        : Base(dyn.getIsDirected()), idMap(dyn.getIdMap()), reverseIdMap(dyn.getReverseIdMap()),
          offsets(dyn.getOffsets()), columnIndices(dyn.getColumnIndices()), values(dyn.getValues()) {}

    explicit StaticCSRGraph(DynamicCSRGraph<VertexType, WeightType>&& dyn)
        : Base(dyn.getIsDirected()),
          idMap(std::move(const_cast<std::unordered_map<VertexType, uint32_t>&>(dyn.getIdMap()))),
          reverseIdMap(std::move(const_cast<std::vector<VertexType>&>(dyn.getReverseIdMap()))),
          offsets(std::move(const_cast<std::vector<uint64_t>&>(dyn.getOffsets()))),
          columnIndices(std::move(const_cast<std::vector<uint32_t>&>(dyn.getColumnIndices()))),
          values(std::move(const_cast<std::vector<WeightType>&>(dyn.getValues()))) {}

    void addVertex(const VertexType&) {
        throw std::logic_error("StaticCSRGraph je statican i ne podrzava dodavanje vrhova!");
    }

    void addEdge(const VertexType&, const VertexType&, const WeightType& = 1) {
        throw std::logic_error("StaticCSRGraph je statican i ne podrzava dodavanje bridova!");
    }

    void addEdgeDynamic(const VertexType&, const VertexType&, const WeightType& = 1) {
        throw std::logic_error("StaticCSRGraph je statican i ne podrzava dodavanje bridova!");
    }

    bool hasVertex(const VertexType& u) const {
        return idMap.find(u) != idMap.end();
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

    std::size_t memoryUsageBytes() const {
        return offsets.capacity() * sizeof(uint64_t)
             + columnIndices.capacity() * sizeof(uint32_t)
             + values.capacity() * sizeof(WeightType);
    }
};

static_assert(GraphReq<StaticCSRGraph<int, double>, int, double>);

#endif