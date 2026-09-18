#include <benchmark/benchmark.h>
#include <cstdint>
#include <vector>
#include <string>
#include <type_traits>
#include <memory>

// Graph implementations
#include "graphs/NaiveGraph.hpp"
#include "graphs/CBListGraph.hpp"
#include "graphs/DynamicCSRGraph.hpp"
#include "graphs/RawGraph.hpp"
#include "graphs/StaticCSRGraph.hpp"
#include "vgraphs/VirtualRawGraph.hpp"
#include "vgraphs/VirtualCSRGraph.hpp"

// Generators
#include "graphs/GraphGenerator.hpp"

using namespace GraphGenerator;

// ============================================================================
// HELPER FUNCTIONS AND STRUCTURES
// ============================================================================

// Helper function to build a graph (handles StaticCSRGraph move semantics)
template <typename GraphType>
GraphType buildGraph(uint32_t num_vertices, const std::vector<RawEdge<double>>& edges) {
    if constexpr (std::is_constructible_v<GraphType, RawGraph<uint32_t, double>&&>) {
        RawGraph<uint32_t, double> raw(true);
        raw.reserve(num_vertices, edges.size());
        for (const auto& e : edges) {
            raw.addEdge(e.u, e.v, e.weight);
        }
        return GraphType(std::move(raw));
    } else {
        GraphType graph(true);
        if constexpr (requires { graph.reserve(num_vertices, edges.size()); }) {
            graph.reserve(num_vertices, edges.size());
        } else if constexpr (requires { graph.reserve(num_vertices); }) {
            graph.reserve(num_vertices);
        }
        for (const auto& e : edges) {
            graph.addEdge(e.u, e.v, e.weight);
        }
        return graph;
    }
}
// ============================================================================
// 1. VERTEX AND EDGE INSERTION (MUTATIONS)
// ============================================================================

template <typename GraphType>
static void BM_EdgeInsertion(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto edges = generateErdosRenyi<double>(num_vertices, num_edges, 42);

    for (auto _ : state) {
        GraphType graph(true);
        if constexpr (requires { graph.reserve(num_vertices, num_edges); }) {
            graph.reserve(num_vertices, num_edges);
        }
        for (const auto& edge : edges) {
            graph.addEdge(edge.u, edge.v, edge.weight);
        }
        benchmark::DoNotOptimize(graph.numEdges());
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}

BENCHMARK_TEMPLATE(BM_EdgeInsertion, NaiveGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 65536);
BENCHMARK_TEMPLATE(BM_EdgeInsertion, CBListGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 65536);
BENCHMARK_TEMPLATE(BM_EdgeInsertion, DynamicCSRGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 4096);
BENCHMARK_TEMPLATE(BM_EdgeInsertion, RawGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 65536);


// ============================================================================
// 2. READ-ONLY AND ALGORITHMS (ALL GRAPHS)
// ============================================================================

template <typename GraphType>
static void BM_EdgeLookup(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 16;
    const auto edges = generateErdosRenyi<double>(num_vertices, num_edges, 12345);
    GraphType graph = buildGraph<GraphType>(num_vertices, edges);

    for (auto _ : state) {
        uint64_t found_count = 0;
        for (const auto& edge : edges) {
            if (graph.hasEdge(edge.u, edge.v)) ++found_count;
        }
        benchmark::DoNotOptimize(found_count);
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}

template <typename GraphType>
static void BM_NeighborTraversal(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 16;
    const auto edges = generateErdosRenyi<double>(num_vertices, num_edges, 12345);
    GraphType graph = buildGraph<GraphType>(num_vertices, edges);

    for (auto _ : state) {
        uint64_t checksum = 0;
        for (uint32_t u = 0; u < num_vertices; ++u) {
            graph.forEachNeighbor(u, [&](uint32_t neighbor, double weight) {
                checksum += neighbor + static_cast<uint64_t>(weight);
            });
        }
        benchmark::DoNotOptimize(checksum);
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}

template <typename GraphType>
static void BM_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto edges = generateErdosRenyi<double>(num_vertices, num_edges, 999);
    GraphType graph = buildGraph<GraphType>(num_vertices, edges);

    for (auto _ : state) {
        size_t visited_count = 0;
        graph.bfs(0, [&](uint32_t) { ++visited_count; });
        benchmark::DoNotOptimize(visited_count);
    }
    state.SetItemsProcessed(state.iterations() * num_vertices);
}

template <typename GraphType>
static void BM_Dijkstra(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto edges = generateErdosRenyi<double>(num_vertices, num_edges, 54321);
    GraphType graph = buildGraph<GraphType>(num_vertices, edges);

    for (auto _ : state) {
        auto dist = graph.dijkstra(0);
        benchmark::DoNotOptimize(dist);
    }
    state.SetItemsProcessed(state.iterations() * num_vertices);
}

#define REGISTER_ALGORITHMS(BenchName) \
    BENCHMARK_TEMPLATE(BenchName, NaiveGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384); \
    BENCHMARK_TEMPLATE(BenchName, CBListGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384); \
    BENCHMARK_TEMPLATE(BenchName, DynamicCSRGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384); \
    BENCHMARK_TEMPLATE(BenchName, RawGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384); \
    BENCHMARK_TEMPLATE(BenchName, StaticCSRGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384);

REGISTER_ALGORITHMS(BM_EdgeLookup)
REGISTER_ALGORITHMS(BM_NeighborTraversal)
REGISTER_ALGORITHMS(BM_BFS)
REGISTER_ALGORITHMS(BM_Dijkstra)


// ============================================================================
// 3. BIGCLASS: DIRECT vs. EXTERNAL STORAGE
// ============================================================================

struct BigClass {
    uint32_t id;
    char heavy_data[128]; // Heavy memory footprint
    bool operator==(const BigClass& other) const { return id == other.id; }
};

namespace std {
    template <> struct hash<BigClass> {
        size_t operator()(const BigClass& k) const { return hash<uint32_t>()(k.id); }
    };
}

// Approach 1: Slow way (BigClass directly in the graph)
static void BM_BigClass_Direct_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const auto edges = generateErdosRenyi<double>(num_vertices, num_vertices * 8, 111);
    
    std::vector<BigClass> vertices(num_vertices);
    for (uint32_t i = 0; i < num_vertices; ++i) vertices[i].id = i;

    DynamicCSRGraph<BigClass, double> graph(true);
    for (const auto& e : edges) graph.addEdge(vertices[e.u], vertices[e.v], e.weight);

    for (auto _ : state) {
        size_t count = 0;
        graph.bfs(vertices[0], [&](const BigClass&) { ++count; });
        benchmark::DoNotOptimize(count);
    }

    state.SetItemsProcessed(state.iterations() * num_vertices);
}
BENCHMARK(BM_BigClass_Direct_BFS)->RangeMultiplier(4)->Range(256, 1024);

// Approach 2: Fast way (Uint32 in graph, BigClass in external vector)
static void BM_BigClass_External_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const auto edges = generateErdosRenyi<double>(num_vertices, num_vertices * 8, 111);
    
    std::vector<BigClass> external_data(num_vertices);
    for (uint32_t i = 0; i < num_vertices; ++i) external_data[i].id = i;

    DynamicCSRGraph<uint32_t, double> graph(true);
    for (const auto& e : edges) graph.addEdge(e.u, e.v, e.weight);

    for (auto _ : state) {
        size_t count = 0;
        uint64_t dummy_sum = 0;
        graph.bfs(0, [&](uint32_t v_id) { 
            ++count; 
            dummy_sum += external_data[v_id].id;
        });
        benchmark::DoNotOptimize(count);
        benchmark::DoNotOptimize(dummy_sum);
    }

    state.SetItemsProcessed(state.iterations() * num_vertices);
}
BENCHMARK(BM_BigClass_External_BFS)->RangeMultiplier(4)->Range(256, 1024);


// ============================================================================
// 4. SPECIFIC TOPOLOGIES (RMAT, GRID, DENSE/SPARSE)
// ============================================================================

// Using StaticCSRGraph for these tests as it is optimal for read-only workloads
using FastGraph = StaticCSRGraph<uint32_t, double>;

static void BM_Topology_Dijkstra_Sparse(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const auto edges = generateErdosRenyi<double>(num_vertices, num_vertices * 2, 1); // Sparse
    FastGraph graph = buildGraph<FastGraph>(num_vertices, edges);

    for (auto _ : state) {
        auto dist = graph.dijkstra(0);
        benchmark::DoNotOptimize(dist);
    }

    state.SetItemsProcessed(state.iterations() * num_vertices);
}
BENCHMARK(BM_Topology_Dijkstra_Sparse)->RangeMultiplier(4)->Range(1024, 16384);

static void BM_Topology_Dijkstra_Dense(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const auto edges = generateErdosRenyi<double>(num_vertices, num_vertices * 32, 2); // Dense
    FastGraph graph = buildGraph<FastGraph>(num_vertices, edges);

    for (auto _ : state) {
        auto dist = graph.dijkstra(0);
        benchmark::DoNotOptimize(dist);
    }

    state.SetItemsProcessed(state.iterations() * num_vertices);
}
BENCHMARK(BM_Topology_Dijkstra_Dense)->RangeMultiplier(4)->Range(1024, 16384);

static void BM_Topology_BFS_RMAT(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const auto edges = generateRMAT<double>(num_vertices, num_vertices * 8); // Scale-free, influencers
    FastGraph graph = buildGraph<FastGraph>(num_vertices, edges);

    for (auto _ : state) {
        size_t count = 0;
        graph.bfs(0, [&](uint32_t) { ++count; });
        benchmark::DoNotOptimize(count);
    }

    state.SetItemsProcessed(state.iterations() * num_vertices);
}
BENCHMARK(BM_Topology_BFS_RMAT)->RangeMultiplier(4)->Range(1024, 16384);

static void BM_Topology_Dijkstra_Grid2D(benchmark::State& state) {
    const auto dim = static_cast<uint32_t>(std::sqrt(state.range(0)));
    const auto num_vertices = dim * dim;
    const auto edges = generateGrid2D<double>(dim, dim); // Manhattan mesh
    FastGraph graph = buildGraph<FastGraph>(num_vertices, edges);

    for (auto _ : state) {
        auto dist = graph.dijkstra(0);
        benchmark::DoNotOptimize(dist);
    }

    state.SetItemsProcessed(state.iterations() * num_vertices);
}
// Sending roughly the same number of vertices (dim * dim)
BENCHMARK(BM_Topology_Dijkstra_Grid2D)->RangeMultiplier(4)->Range(1024, 16384);


// ============================================================================
// 5. POLYMORPHISM COST: TEMPLATE vs. VIRTUAL
// ============================================================================

static void BM_Polymorphism_Template_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const auto edges = generateErdosRenyi<double>(num_vertices, num_vertices * 8, 99);
    FastGraph graph = buildGraph<FastGraph>(num_vertices, edges);

    for (auto _ : state) {
        size_t count = 0;
        graph.bfs(0, [&](uint32_t) { ++count; });
        benchmark::DoNotOptimize(count);
    }

    state.SetItemsProcessed(state.iterations() * num_vertices);
}
BENCHMARK(BM_Polymorphism_Template_BFS)->RangeMultiplier(4)->Range(1024, 16384);

static void BM_Polymorphism_Virtual_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const auto edges = generateErdosRenyi<double>(num_vertices, num_vertices * 8, 99);
    
    // Building VirtualCSRGraph from VirtualRawGraph[cite: 9]
    VirtualRawGraph<uint32_t, double> raw(true);
    raw.reserve(num_vertices, edges.size());
    for (const auto& e : edges) raw.addEdge(e.u, e.v, e.weight);
    
    std::unique_ptr<VirtualBaseGraph<uint32_t, double>> virtual_graph = 
        std::make_unique<VirtualCSRGraph<uint32_t, double>>(std::move(raw));

    for (auto _ : state) {
        size_t count = 0;
        virtual_graph->bfs(0, [&](uint32_t) { ++count; });
        benchmark::DoNotOptimize(count);
    }

    state.SetItemsProcessed(state.iterations() * num_vertices);
}
BENCHMARK(BM_Polymorphism_Virtual_BFS)->RangeMultiplier(4)->Range(1024, 16384);

BENCHMARK_MAIN();