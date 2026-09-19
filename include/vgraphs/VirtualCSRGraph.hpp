#ifndef VIRTUAL_CSR_GRAPH_HPP
#define VIRTUAL_CSR_GRAPH_HPP

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <stdexcept>
#include "../graphs/Concepts.hpp"
#include "VirtualBaseGraph.hpp"
#include "VirtualRawGraph.hpp"

template <typename VertexType, Numeric WeightType = double>
class VirtualCSRGraph : public VirtualBaseGraph<VertexType, WeightType> {
private:
    std::unordered_map<VertexType, uint32_t> idMap;
    std::vector<VertexType> reverseIdMap;

    std::vector<uint64_t> offsets;
    std::vector<uint32_t> columnIndices;
    std::vector<WeightType> values;

    void buildFromEdgeList(const std::vector<typename VirtualRawGraph<VertexType, WeightType>::RawEdge>& edgeList) {
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
    using Base = VirtualBaseGraph<VertexType, WeightType>;
    using typename Base::NeighborCallback;
    using typename Base::EdgeCallback;

    explicit VirtualCSRGraph(bool directed = false) : Base(directed) {
        offsets.assign(1, 0);
    }

    explicit VirtualCSRGraph(const VirtualRawGraph<VertexType, WeightType>& raw)
        : Base(raw.getIsDirected()), idMap(raw.getIdMap()), reverseIdMap(raw.getReverseIdMap()) {
        buildFromEdgeList(raw.getEdgeList());
    }

    explicit VirtualCSRGraph(VirtualRawGraph<VertexType, WeightType>&& raw)
        : Base(raw.getIsDirected()),
          idMap(std::move(const_cast<std::unordered_map<VertexType, uint32_t>&>(raw.getIdMap()))),
          reverseIdMap(std::move(const_cast<std::vector<VertexType>&>(raw.getReverseIdMap()))) {
        buildFromEdgeList(raw.getEdgeList());
    }

    void addVertex(const VertexType&) override {
        throw std::logic_error("VirtualCSRGraph je statican i ne podrzava dodavanje vrhova!");
    }

    void addEdge(const VertexType&, const VertexType&, const WeightType& = 1) override {
        throw std::logic_error("VirtualCSRGraph je statican i ne podrzava dodavanje bridova!");
    }

    void addEdgeDynamic(const VertexType&, const VertexType&, const WeightType& = 1) override {
        throw std::logic_error("VirtualCSRGraph je statican i ne podrzava dodavanje bridova!");
    }

    bool hasVertex(const VertexType& u) const override {
        return idMap.find(u) != idMap.end();
    }

    bool hasEdge(const VertexType& u, const VertexType& v) const override {
        auto itU = idMap.find(u);
        auto itV = idMap.find(v);
        if (itU == idMap.end() || itV == idMap.end()) return false;

        uint32_t uId = itU->second;
        uint32_t vId = itV->second;

        auto first = columnIndices.begin() + offsets[uId];
        auto last = columnIndices.begin() + offsets[uId + 1];
        return std::binary_search(first, last, vId);
    }

    bool hasEdge(const VertexType& u, const VertexType& v, const WeightType& w) const override {
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

    std::size_t getDegree(const VertexType& u) const override {
        auto it = idMap.find(u);
        if (it == idMap.end()) return 0;
        uint32_t uId = it->second;
        return offsets[uId + 1] - offsets[uId];
    }

    void forEachNeighbor(const VertexType& u, const NeighborCallback& callback) const override {
        auto it = idMap.find(u);
        if (it == idMap.end()) return;

        uint32_t uId = it->second;
        uint64_t start = offsets[uId];
        uint64_t end = offsets[uId + 1];

        for (uint64_t i = start; i < end; ++i) {
            callback(reverseIdMap[columnIndices[i]], values[i]);
        }
    }

    void traverseEntireGraph(const EdgeCallback& callback) const override {
        for (std::size_t uId = 0; uId < reverseIdMap.size(); ++uId) {
            uint64_t start = offsets[uId];
            uint64_t end = offsets[uId + 1];
            for (uint64_t i = start; i < end; ++i) {
                callback(reverseIdMap[uId], reverseIdMap[columnIndices[i]], values[i]);
            }
        }
    }

    std::size_t numVertices() const override {
        return reverseIdMap.size();
    }

    std::size_t numEdges() const override {
        return this->isDirected ? columnIndices.size() : columnIndices.size() / 2;
    }
};

#endif