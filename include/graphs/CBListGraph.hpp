#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <utility>
#include <functional>
#include <queue>
#include <limits>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <algorithm>
#include "RawGraph.hpp"

template <typename WeightType, size_t ChunkSize = 14>
class ChunkArena {
public:
    struct Chunk {
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

template <typename VertexType = uint32_t, typename WeightType = double>
class CBListGraph {
public:
    using Chunk = typename ChunkArena<WeightType>::Chunk;

    struct NodeHead {
        Chunk* first{nullptr};
        Chunk* last{nullptr};
    };

    explicit CBListGraph(RawGraph<VertexType, WeightType>&& raw)
        : CBListGraph(true) {
        this->reserve(raw.numVertices(), raw.numEdges());
        for (uint32_t u = 0; u < raw.numVertices(); ++u) {
            raw.forEachNeighbor(u, [&](const VertexType& v, WeightType w) {
                this->addEdge(u, v, w);
            });
        }
    }

    explicit CBListGraph(bool directed = true) 
        : directed_(directed), num_edges_(0) {}

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

    size_t getDegree(const VertexType& v) const {
        auto it = vertex_to_id_.find(v);
        if (it == vertex_to_id_.end()) return 0;
        uint32_t u_id = it->second;

        size_t degree = 0;
        const Chunk* curr = heads_[u_id].first;
        while (curr != nullptr) {
            degree += curr->count;
            curr = curr->next;
        }
        return degree;
    }

    void insertDirectedEdge(uint32_t u_id, uint32_t v_id, WeightType w) {
        NodeHead& head = heads_[u_id];
        if (head.last == nullptr || head.last->count >= 14) {
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

    void addEdge(const VertexType& u, const VertexType& v, const WeightType& w) {
        uint32_t u_id = addVertex(u);
        uint32_t v_id = addVertex(v);

        insertDirectedEdge(u_id, v_id, w);
        if (!directed_) {
            insertDirectedEdge(v_id, u_id, w);
        }
        ++num_edges_;
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

    template <typename Callback>
    void bfs(const VertexType& start, Callback&& cb) const {
        auto it = vertex_to_id_.find(start);
        if (it == vertex_to_id_.end()) return;

        std::vector<bool> visited(heads_.size(), false);
        std::queue<uint32_t> q;

        uint32_t start_id = it->second;
        visited[start_id] = true;
        q.push(start_id);

        while (!q.empty()) {
            uint32_t curr_id = q.front();
            q.pop();
            cb(id_to_vertex_[curr_id]);

            const Chunk* curr = heads_[curr_id].first;
            while (curr != nullptr) {
                for (uint32_t i = 0; i < curr->count; ++i) {
                    uint32_t nbr_id = curr->targets[i];
                    if (!visited[nbr_id]) {
                        visited[nbr_id] = true;
                        q.push(nbr_id);
                    }
                }
                curr = curr->next;
            }
        }
    }

    std::unordered_map<VertexType, WeightType> dijkstra(const VertexType& start) const {
        std::unordered_map<VertexType, WeightType> dist_map;
        for (const auto& v : id_to_vertex_) {
            dist_map[v] = std::numeric_limits<WeightType>::infinity();
        }

        auto it = vertex_to_id_.find(start);
        if (it == vertex_to_id_.end()) return dist_map;

        uint32_t start_id = it->second;
        std::vector<WeightType> dist(heads_.size(), std::numeric_limits<WeightType>::infinity());

        using Pair = std::pair<WeightType, uint32_t>;
        std::priority_queue<Pair, std::vector<Pair>, std::greater<Pair>> pq;

        dist[start_id] = 0;
        pq.emplace(0, start_id);

        while (!pq.empty()) {
            auto [d, u_id] = pq.top();
            pq.pop();

            if (d > dist[u_id]) continue;

            const Chunk* curr = heads_[u_id].first;
            while (curr != nullptr) {
                for (uint32_t i = 0; i < curr->count; ++i) {
                    WeightType w = curr->weights[i];
                    if (w < 0) {
                        throw std::invalid_argument("Dijkstra does not support negative edge weights.");
                    }
                    uint32_t v_id = curr->targets[i];
                    if (dist[u_id] + w < dist[v_id]) {
                        dist[v_id] = dist[u_id] + w;
                        pq.emplace(dist[v_id], v_id);
                    }
                }
                curr = curr->next;
            }
        }

        for (size_t i = 0; i < id_to_vertex_.size(); ++i) {
            dist_map[id_to_vertex_[i]] = dist[i];
        }
        return dist_map;
    }

    uint32_t numVertices() const { return static_cast<uint32_t>(id_to_vertex_.size()); }
    uint64_t numEdges() const { return num_edges_; }

private:
    bool directed_;
    uint64_t num_edges_;
    std::vector<NodeHead> heads_;
    std::unordered_map<VertexType, uint32_t> vertex_to_id_;
    std::vector<VertexType> id_to_vertex_;
    ChunkArena<WeightType> arena_;
};