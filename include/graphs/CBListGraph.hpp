#pragma once

// Chunked adjacency list: each vertex is a chain of alignas(64) chunks of 14
//entries (exactly 192 bytes with double weights)

#include <cstdint>
#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>
#include <cstddef>
#include "Concepts.hpp"
#include "BaseGraph.hpp"
#include "RawGraph.hpp"

template <typename WeightType, size_t ChunkSize = 14>
class ChunkArena {
public:

    struct alignas(64) Chunk {
        static constexpr uint32_t Capacity = static_cast<uint32_t>(ChunkSize);

        uint32_t targets[ChunkSize];
        WeightType weights[ChunkSize];
        uint32_t count{0};
        Chunk* next{nullptr};
    };

    ChunkArena() = default;
    ~ChunkArena() = default;

    ChunkArena(ChunkArena&& other) noexcept = default;
    ChunkArena& operator=(ChunkArena&& other) noexcept = default;

    ChunkArena(const ChunkArena&) = delete;
    ChunkArena& operator=(const ChunkArena&) = delete;

    Chunk* allocate() {
        storage_.push_back(std::make_unique<Chunk>());
        return storage_.back().get();
    }

    void clear() noexcept {
        storage_.clear();
    }

    size_t size() const noexcept {
        return storage_.size();
    }

private:
    std::vector<std::unique_ptr<Chunk>> storage_;
};

template <typename VertexType = uint32_t, Numeric WeightType = double>
class CBListGraph : public BaseGraph<CBListGraph<VertexType, WeightType>, VertexType, WeightType> {
public:
    using Base = BaseGraph<CBListGraph<VertexType, WeightType>, VertexType, WeightType>;
    using Chunk = typename ChunkArena<WeightType>::Chunk;

    struct NodeHead {
        Chunk* first{nullptr};
        Chunk* last{nullptr};
    };

    explicit CBListGraph(bool directed = true)
        : Base(directed), num_edges_(0) {}

    explicit CBListGraph(RawGraph<VertexType, WeightType>&& raw)
        : CBListGraph(true) {
        this->reserve(raw.numVertices(), raw.getEdgeList().size());

        for (const VertexType& vertex : raw.getReverseIdMap()) {
            addVertex(vertex);
        }
        raw.traverseEntireGraph([&](const VertexType& u, const VertexType& v, const WeightType& w) {
            this->addEdge(u, v, w);
        });
    }

    ~CBListGraph() = default;

    CBListGraph(CBListGraph&& other) noexcept = default;
    CBListGraph& operator=(CBListGraph&& other) noexcept = default;

    CBListGraph(const CBListGraph&) = delete;
    CBListGraph& operator=(const CBListGraph&) = delete;

    void reserve(size_t num_vertices, size_t num_edges = 0) {
        (void)num_edges;
        heads_.reserve(num_vertices);
        vertex_to_id_.reserve(num_vertices);
        id_to_vertex_.reserve(num_vertices);
    }

    uint32_t addVertex(const VertexType& v) {
        auto it = vertex_to_id_.find(v);
        if (it != vertex_to_id_.end()) {
            return it->second;
        }
        uint32_t new_id = static_cast<uint32_t>(id_to_vertex_.size());
        vertex_to_id_[v] = new_id;
        id_to_vertex_.push_back(v);
        heads_.push_back(NodeHead{nullptr, nullptr});
        return new_id;
    }

    bool hasVertex(const VertexType& v) const {
        return vertex_to_id_.find(v) != vertex_to_id_.end();
    }

    std::size_t getDegree(const VertexType& v) const {
        auto it = vertex_to_id_.find(v);
        if (it == vertex_to_id_.end()) return 0;
        uint32_t u_id = it->second;

        std::size_t degree = 0;
        const Chunk* curr = heads_[u_id].first;
        while (curr != nullptr) {
            degree += curr->count;
            curr = curr->next;
        }
        return degree;
    }

    void insertDirectedEdge(uint32_t u_id, uint32_t v_id, WeightType w) {
        NodeHead& head = heads_[u_id];
        if (head.last == nullptr || head.last->count >= Chunk::Capacity) {
            Chunk* chunk = arena_.allocate();
            chunk->next = nullptr;
            chunk->count = 0;
            if (head.first == nullptr) {
                head.first = chunk;
            } else {
                head.last->next = chunk;
            }
            head.last = chunk;
        }

        head.last->targets[head.last->count] = v_id;
        head.last->weights[head.last->count] = w;
        ++head.last->count;
    }

    void addEdge(const VertexType& u, const VertexType& v, const WeightType& w = 1) {
        uint32_t u_id = addVertex(u);
        uint32_t v_id = addVertex(v);

        insertDirectedEdge(u_id, v_id, w);
        if (!this->isDirected && u_id != v_id) {
            insertDirectedEdge(v_id, u_id, w);
        }
        ++num_edges_;
    }

    void addEdgeDynamic(const VertexType& u, const VertexType& v, const WeightType& w = 1) {
        addEdge(u, v, w);
    }

    bool hasEdge(const VertexType& u, const VertexType& v) const {
        auto it_u = vertex_to_id_.find(u);
        auto it_v = vertex_to_id_.find(v);
        if (it_u == vertex_to_id_.end() || it_v == vertex_to_id_.end()) {
            return false;
        }
        uint32_t u_id = it_u->second;
        uint32_t v_id = it_v->second;

        const Chunk* curr = heads_[u_id].first;
        while (curr != nullptr) {
            for (uint32_t i = 0; i < curr->count; ++i) {
                if (curr->targets[i] == v_id) {
                    return true;
                }
            }
            curr = curr->next;
        }
        return false;
    }

    bool hasEdge(const VertexType& u, const VertexType& v, const WeightType& w) const {
        auto it_u = vertex_to_id_.find(u);
        auto it_v = vertex_to_id_.find(v);
        if (it_u == vertex_to_id_.end() || it_v == vertex_to_id_.end()) {
            return false;
        }
        uint32_t u_id = it_u->second;
        uint32_t v_id = it_v->second;

        const Chunk* curr = heads_[u_id].first;
        while (curr != nullptr) {
            for (uint32_t i = 0; i < curr->count; ++i) {
                if (curr->targets[i] == v_id && curr->weights[i] == w) {
                    return true;
                }
            }
            curr = curr->next;
        }
        return false;
    }

    template <typename Callback>
    void forEachNeighbor(const VertexType& u, Callback&& cb) const {
        auto it = vertex_to_id_.find(u);
        if (it == vertex_to_id_.end()) return;
        uint32_t u_id = it->second;

        const Chunk* curr = heads_[u_id].first;
        while (curr != nullptr) {
            for (uint32_t i = 0; i < curr->count; ++i) {
                cb(id_to_vertex_[curr->targets[i]], curr->weights[i]);
            }
            curr = curr->next;
        }
    }

    template <typename Callback>
    void traverseEntireGraph(Callback&& cb) const {
        for (size_t u_id = 0; u_id < heads_.size(); ++u_id) {
            const Chunk* curr = heads_[u_id].first;
            while (curr != nullptr) {
                for (uint32_t i = 0; i < curr->count; ++i) {
                    cb(id_to_vertex_[u_id], id_to_vertex_[curr->targets[i]], curr->weights[i]);
                }
                curr = curr->next;
            }
        }
    }

    std::size_t numVertices() const { return id_to_vertex_.size(); }
    std::size_t numEdges() const { return static_cast<std::size_t>(num_edges_); }

    std::size_t memoryUsageBytes() const {
        return arena_.size() * sizeof(Chunk)
             + heads_.capacity() * sizeof(NodeHead);
    }

private:
    uint64_t num_edges_;
    std::vector<NodeHead> heads_;
    std::unordered_map<VertexType, uint32_t> vertex_to_id_;
    std::vector<VertexType> id_to_vertex_;
    ChunkArena<WeightType> arena_;
};

static_assert(GraphReq<CBListGraph<int, double>, int, double>);
