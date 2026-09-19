#include <benchmark/benchmark.h>
#include <cstdint>
#include <vector>
#include <string>
#include <type_traits>
#include <memory>
#include <cmath>

#include "graphs/NaiveGraph.hpp"
#include "graphs/CBListGraph.hpp"
#include "graphs/DynamicCSRGraph.hpp"
#include "graphs/StaticCSRGraph.hpp"
#include "graphs/RawGraph.hpp"
#include "graphs/GCCGraph.hpp"
#include "vgraphs/VirtualRawGraph.hpp"
#include "vgraphs/VirtualCSRGraph.hpp"

#include "graphs/GraphGenerator.hpp"

using namespace GraphGenerator;

// HELPERS
//comparison suites below use the strict generator, no dupiclates and no selfloops, also all wights are postive

template <typename WeightType = double>
static std::vector<RawEdge<WeightType>> makeUniform(uint32_t num_vertices,
                                                    uint64_t num_edges,
                                                    uint32_t seed) {
    return generateErdosRenyiStrictGaussian<WeightType>(num_vertices, num_edges, seed);
}

// Builds a graph from an edge list
template <typename GraphType>
GraphType buildGraph(uint32_t num_vertices, const std::vector<RawEdge<double>>& edges) {
    if constexpr (std::is_constructible_v<GraphType, RawGraph<uint32_t, double>&&>) {
        RawGraph<uint32_t, double> raw(true);
        raw.reserve(num_vertices, edges.size());
        for (const auto& e : edges) raw.addEdge(e.u, e.v, e.weight);
        return GraphType(std::move(raw));
    } else {
        GraphType graph(true);
        if constexpr (requires { graph.reserve(num_vertices, edges.size()); }) {
            graph.reserve(num_vertices, edges.size());
        } else if constexpr (requires { graph.reserve(num_vertices); }) {
            graph.reserve(num_vertices);
        }
        for (const auto& e : edges) graph.addEdge(e.u, e.v, e.weight);
        return graph;
    }
}

#define REG_MUTABLE(Bench) \
    BENCHMARK_TEMPLATE(Bench, NaiveGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 65536); \
    BENCHMARK_TEMPLATE(Bench, CBListGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 65536); \
    BENCHMARK_TEMPLATE(Bench, GCCGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 65536); \
    BENCHMARK_TEMPLATE(Bench, RawGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 65536); \
    BENCHMARK_TEMPLATE(Bench, DynamicCSRGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 4096);


#define REG_COMPARE(Bench) \
    BENCHMARK_TEMPLATE(Bench, NaiveGraph<uint32_t, double>)->RangeMultiplier(4)->Range(256, 16384); \
    BENCHMARK_TEMPLATE(Bench, CBListGraph<uint32_t, double>)->RangeMultiplier(4)->Range(256, 16384); \
    BENCHMARK_TEMPLATE(Bench, GCCGraph<uint32_t, double>)->RangeMultiplier(4)->Range(256, 16384); \
    BENCHMARK_TEMPLATE(Bench, DynamicCSRGraph<uint32_t, double>)->RangeMultiplier(4)->Range(256, 16384); \
    BENCHMARK_TEMPLATE(Bench, StaticCSRGraph<uint32_t, double>)->RangeMultiplier(4)->Range(256, 16384); \
    BENCHMARK_TEMPLATE(Bench, RawGraph<uint32_t, double>)->RangeMultiplier(4)->Range(256, 2048);

static constexpr uint32_t kDensityVertices = 4096;
#define REG_DENSITY(Bench) \
    BENCHMARK_TEMPLATE(Bench, NaiveGraph<uint32_t, double>)->Arg(2)->Arg(8)->Arg(32)->Arg(128); \
    BENCHMARK_TEMPLATE(Bench, CBListGraph<uint32_t, double>)->Arg(2)->Arg(8)->Arg(32)->Arg(128); \
    BENCHMARK_TEMPLATE(Bench, GCCGraph<uint32_t, double>)->Arg(2)->Arg(8)->Arg(32)->Arg(128); \
    BENCHMARK_TEMPLATE(Bench, DynamicCSRGraph<uint32_t, double>)->Arg(2)->Arg(8)->Arg(32)->Arg(128); \
    BENCHMARK_TEMPLATE(Bench, StaticCSRGraph<uint32_t, double>)->Arg(2)->Arg(8)->Arg(32)->Arg(128);

// 1. INSERTION (mutable structures)
template <typename GraphType>
static void BM_EdgeInsertion(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto edges = makeUniform<double>(num_vertices, num_edges, 42);

    for (auto _ : state) {
        GraphType graph(true);
        if constexpr (requires { graph.reserve(num_vertices, num_edges); }) {
            graph.reserve(num_vertices, num_edges);
        }
        for (const auto& e : edges) graph.addEdge(e.u, e.v, e.weight);
        benchmark::DoNotOptimize(graph.numEdges());
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}
REG_MUTABLE(BM_EdgeInsertion)

// 2. READ-ONLY WORKLOADS vs SIZE (all structures)
template <typename GraphType>
static void BM_EdgeLookup(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 16;
    const auto edges = makeUniform<double>(num_vertices, num_edges, 12345);
    GraphType graph = buildGraph<GraphType>(num_vertices, edges);

    for (auto _ : state) {
        uint64_t found = 0;
        for (const auto& e : edges) if (graph.hasEdge(e.u, e.v)) ++found;
        benchmark::DoNotOptimize(found);
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}

template <typename GraphType>
static void BM_NeighborTraversal(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 16;
    const auto edges = makeUniform<double>(num_vertices, num_edges, 12345);
    GraphType graph = buildGraph<GraphType>(num_vertices, edges);

    for (auto _ : state) {
        uint64_t checksum = 0;
        for (uint32_t u = 0; u < num_vertices; ++u) {
            graph.forEachNeighbor(u, [&](uint32_t nb, double w) {
                checksum += nb + static_cast<uint64_t>(w);
            });
        }
        benchmark::DoNotOptimize(checksum);
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}

// Full sequential scan
template <typename GraphType>
static void BM_FullScan(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 16;
    const auto edges = makeUniform<double>(num_vertices, num_edges, 2024);
    GraphType graph = buildGraph<GraphType>(num_vertices, edges);

    for (auto _ : state) {
        uint64_t checksum = 0;
        graph.traverseEntireGraph([&](uint32_t u, uint32_t v, double w) {
            checksum += u + v + static_cast<uint64_t>(w);
        });
        benchmark::DoNotOptimize(checksum);
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}

template <typename GraphType>
static void BM_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto edges = makeUniform<double>(num_vertices, num_edges, 999);
    GraphType graph = buildGraph<GraphType>(num_vertices, edges);

    for (auto _ : state) {
        size_t visited = 0;
        graph.bfs(0, [&](uint32_t) { ++visited; });
        benchmark::DoNotOptimize(visited);
    }
    state.SetItemsProcessed(state.iterations() * num_vertices);
}

template <typename GraphType>
static void BM_Dijkstra(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto edges = makeUniform<double>(num_vertices, num_edges, 54321);
    GraphType graph = buildGraph<GraphType>(num_vertices, edges);

    for (auto _ : state) {
        auto dist = graph.dijkstra(0);
        benchmark::DoNotOptimize(dist);
    }
    state.SetItemsProcessed(state.iterations() * num_vertices);
}

REG_COMPARE(BM_EdgeLookup)
REG_COMPARE(BM_NeighborTraversal)
REG_COMPARE(BM_FullScan)
REG_COMPARE(BM_BFS)
REG_COMPARE(BM_Dijkstra)

// 3. DENSITY SWEEP (avg degree on the x-axis, fixed vertex count)
template <typename GraphType>
static void BM_Density_Dijkstra(benchmark::State& state) {
    const uint32_t V = kDensityVertices;
    const auto avg_degree = static_cast<uint64_t>(state.range(0));
    const auto edges = makeUniform<double>(V, static_cast<uint64_t>(V) * avg_degree, 7);
    GraphType graph = buildGraph<GraphType>(V, edges);

    for (auto _ : state) {
        auto dist = graph.dijkstra(0);
        benchmark::DoNotOptimize(dist);
    }
    state.SetItemsProcessed(state.iterations() * V);
}

template <typename GraphType>
static void BM_Density_Scan(benchmark::State& state) {
    const uint32_t V = kDensityVertices;
    const auto avg_degree = static_cast<uint64_t>(state.range(0));
    const uint64_t E = static_cast<uint64_t>(V) * avg_degree;
    const auto edges = makeUniform<double>(V, E, 8);
    GraphType graph = buildGraph<GraphType>(V, edges);

    for (auto _ : state) {
        uint64_t checksum = 0;
        graph.traverseEntireGraph([&](uint32_t u, uint32_t v, double) { checksum += u + v; });
        benchmark::DoNotOptimize(checksum);
    }
    state.SetItemsProcessed(state.iterations() * E);
}

REG_DENSITY(BM_Density_Dijkstra)
REG_DENSITY(BM_Density_Scan)

// 4. TOPOLOGIES (uniform, scale-free R-MAT, 2D grid) across all structures
template <typename GraphType>
static void BM_Uniform_BFS(benchmark::State& state) {
    const auto V = static_cast<uint32_t>(state.range(0));
    const auto edges = makeUniform<double>(V, static_cast<uint64_t>(V) * 8, 1);
    GraphType graph = buildGraph<GraphType>(V, edges);
    for (auto _ : state) {
        size_t c = 0;
        graph.bfs(0, [&](uint32_t) { ++c; });
        benchmark::DoNotOptimize(c);
    }
    state.SetItemsProcessed(state.iterations() * V);
}

template <typename GraphType>
static void BM_RMAT_BFS(benchmark::State& state) {
    const auto V = static_cast<uint32_t>(state.range(0));
    const auto edges = generateRMAT<double>(V, static_cast<uint64_t>(V) * 8); // scale-free hubs
    GraphType graph = buildGraph<GraphType>(V, edges);
    for (auto _ : state) {
        size_t c = 0;
        graph.bfs(0, [&](uint32_t) { ++c; });
        benchmark::DoNotOptimize(c);
    }
    state.SetItemsProcessed(state.iterations() * V);
}

template <typename GraphType>
static void BM_Grid_Dijkstra(benchmark::State& state) {
    const auto dim = static_cast<uint32_t>(std::sqrt(static_cast<double>(state.range(0))));
    const uint32_t V = dim * dim;
    const auto edges = generateGrid2D<double>(dim, dim); // Manhattan mesh
    GraphType graph = buildGraph<GraphType>(V, edges);
    for (auto _ : state) {
        auto dist = graph.dijkstra(0);
        benchmark::DoNotOptimize(dist);
    }
    state.SetItemsProcessed(state.iterations() * V);
}

REG_COMPARE(BM_Uniform_BFS)
REG_COMPARE(BM_RMAT_BFS)
REG_COMPARE(BM_Grid_Dijkstra)

// 5. POINTER-CHASING LATENCY (single-successor chain -> pure memory latency)
template <typename GraphType>
static void BM_PointerChase(benchmark::State& state) {
    const auto V = static_cast<uint32_t>(state.range(0));
    const auto edges = generatePointerChasingChain<double>(V, 7); // one closed cycle
    GraphType graph = buildGraph<GraphType>(V, edges);

    for (auto _ : state) {
        uint32_t cur = 0;
        uint64_t acc = 0;
        for (uint32_t i = 0; i < V; ++i) {
            uint32_t next = cur;
            graph.forEachNeighbor(cur, [&](uint32_t nb, double) { next = nb; });
            cur = next;
            acc += cur;
        }
        benchmark::DoNotOptimize(acc);
    }
    state.SetItemsProcessed(state.iterations() * V);
}
REG_COMPARE(BM_PointerChase)

// 6. MEMORY FOOTPRINT (bytes and bytes/edge reported as counters)
template <typename GraphType>
static void BM_Memory(benchmark::State& state) {
    const auto V = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(V) * 16;
    const auto edges = makeUniform<double>(V, num_edges, 314);
    GraphType graph = buildGraph<GraphType>(V, edges);

    const double bytes = static_cast<double>(graph.memoryUsageBytes());
    const double stored_edges = static_cast<double>(graph.numEdges());

    for (auto _ : state) {
        benchmark::DoNotOptimize(graph.memoryUsageBytes());
    }
    state.counters["bytes"] = benchmark::Counter(bytes);
    state.counters["edges"] = benchmark::Counter(stored_edges);
    state.counters["bytes_per_edge"] =
        benchmark::Counter(stored_edges > 0 ? bytes / stored_edges : 0.0);
}

BENCHMARK_TEMPLATE(BM_Memory, NaiveGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384);
BENCHMARK_TEMPLATE(BM_Memory, CBListGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384);
BENCHMARK_TEMPLATE(BM_Memory, GCCGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384);
BENCHMARK_TEMPLATE(BM_Memory, DynamicCSRGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384);
BENCHMARK_TEMPLATE(BM_Memory, StaticCSRGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384);
BENCHMARK_TEMPLATE(BM_Memory, RawGraph<uint32_t, double>)->RangeMultiplier(4)->Range(1024, 16384);

// 7. BIGCLASS: DIRECT vs. EXTERNAL STORAGE
struct BigClass {
    uint32_t id;
    char heavy_data[128];
    bool operator==(const BigClass& other) const { return id == other.id; }
};
namespace std {
    template <> struct hash<BigClass> {
        size_t operator()(const BigClass& k) const { return hash<uint32_t>()(k.id); }
    };
}

static void BM_BigClass_Direct_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const auto edges = makeUniform<double>(num_vertices, num_vertices * 8, 111);

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

static void BM_BigClass_External_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const auto edges = makeUniform<double>(num_vertices, num_vertices * 8, 111);

    std::vector<BigClass> external_data(num_vertices);
    for (uint32_t i = 0; i < num_vertices; ++i) external_data[i].id = i;

    DynamicCSRGraph<uint32_t, double> graph(true);
    for (const auto& e : edges) graph.addEdge(e.u, e.v, e.weight);

    for (auto _ : state) {
        size_t count = 0;
        uint64_t dummy_sum = 0;
        graph.bfs(0, [&](uint32_t v_id) { ++count; dummy_sum += external_data[v_id].id; });
        benchmark::DoNotOptimize(count);
        benchmark::DoNotOptimize(dummy_sum);
    }
    state.SetItemsProcessed(state.iterations() * num_vertices);
}
BENCHMARK(BM_BigClass_External_BFS)->RangeMultiplier(4)->Range(256, 1024);

// 8. POLYMORPHISM COST: TEMPLATE (CRTP) vs. VIRTUAL
using FastGraph = StaticCSRGraph<uint32_t, double>;

static void BM_Polymorphism_Template_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const auto edges = makeUniform<double>(num_vertices, num_vertices * 8, 99);
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
    const auto edges = makeUniform<double>(num_vertices, num_vertices * 8, 99);

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