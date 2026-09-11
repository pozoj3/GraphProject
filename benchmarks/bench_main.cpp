#include <benchmark/benchmark.h>
#include <cstdint>
#include <vector>

#include "graphs/CBListGraph.hpp"
#include "graphs/CSRGraph.hpp"
#include "graphs/NaiveGraph.hpp"
#include "graphs/GraphGenerator.hpp"

// 1. Initial edge insertion benchmarks
static void BM_EdgeInsertion_Naive(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto edges = generateRandomGraph(num_vertices, num_edges, 42);

    for (auto _ : state) {
        Graph<uint32_t, double> graph;
        for (const auto& edge : edges) {
            graph.addEdge(edge.u, edge.v, edge.weight);
        }
        benchmark::DoNotOptimize(graph);
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}
BENCHMARK(BM_EdgeInsertion_Naive)->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);

static void BM_EdgeInsertion_CBList(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto edges = generateRandomGraph(num_vertices, num_edges, 42);

    for (auto _ : state) {
        CBListGraph<uint32_t, double> graph(true);
        graph.reserve(num_vertices);
        for (const auto& edge : edges) {
            graph.addEdge(edge.u, edge.v, edge.weight);
        }
        benchmark::DoNotOptimize(graph.numEdges());
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}
BENCHMARK(BM_EdgeInsertion_CBList)->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);

// 2. Neighbor traversal benchmark
template <typename GraphType>
static void BM_NeighborTraversal(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 16;
    const auto edges = generateRandomGraph(num_vertices, num_edges, 12345);

    GraphType graph;
    if constexpr (requires { graph.reserve(num_vertices, num_edges); }) {
        graph.reserve(num_vertices, num_edges);
    }
    for (const auto& edge : edges) {
        graph.addEdge(edge.u, edge.v, edge.weight);
    }
    if constexpr (requires { graph.finalize(); }) {
        graph.finalize();
    }

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
BENCHMARK_TEMPLATE(BM_NeighborTraversal, Graph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_NeighborTraversal, CSRGraph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_NeighborTraversal, CBListGraph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);

// 3. Entire graph traversal benchmark
template <typename GraphType>
static void BM_TraverseEntireGraph(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 16;
    const auto edges = generateRandomGraph(num_vertices, num_edges, 12345);

    GraphType graph;
    if constexpr (requires { graph.reserve(num_vertices, num_edges); }) {
        graph.reserve(num_vertices, num_edges);
    }
    for (const auto& edge : edges) {
        graph.addEdge(edge.u, edge.v, edge.weight);
    }
    if constexpr (requires { graph.finalize(); }) {
        graph.finalize();
    }

    for (auto _ : state) {
        uint64_t checksum = 0;
        graph.traverseEntireGraph([&](uint32_t neighbor, double weight) {
            checksum += neighbor + static_cast<uint64_t>(weight);
        });
        benchmark::DoNotOptimize(checksum);
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}
BENCHMARK_TEMPLATE(BM_TraverseEntireGraph, Graph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_TraverseEntireGraph, CSRGraph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_TraverseEntireGraph, CBListGraph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);

// 4. BFS traversal benchmark
template <typename GraphType>
static void BM_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto edges = generateRandomGraph(num_vertices, num_edges, 999);

    GraphType graph;
    if constexpr (requires { graph.reserve(num_vertices, num_edges); }) {
        graph.reserve(num_vertices, num_edges);
    }
    for (const auto& edge : edges) {
        graph.addEdge(edge.u, edge.v, edge.weight);
    }
    if constexpr (requires { graph.finalize(); }) {
        graph.finalize();
    }

    for (auto _ : state) {
        size_t visited_count = 0;
        graph.bfs(0, [&](uint32_t) {
            ++visited_count;
        });
        benchmark::DoNotOptimize(visited_count);
    }
    state.SetItemsProcessed(state.iterations() * num_vertices);
}
BENCHMARK_TEMPLATE(BM_BFS, Graph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 16384)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_BFS, CSRGraph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 16384)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_BFS, CBListGraph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 16384)->Unit(benchmark::kMillisecond);

// 5. Dijkstra benchmark
template <typename GraphType>
static void BM_Dijkstra(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto edges = generateRandomGraph(num_vertices, num_edges, 54321);

    GraphType graph;
    if constexpr (requires { graph.reserve(num_vertices, num_edges); }) {
        graph.reserve(num_vertices, num_edges);
    }
    for (const auto& edge : edges) {
        graph.addEdge(edge.u, edge.v, edge.weight);
    }
    if constexpr (requires { graph.finalize(); }) {
        graph.finalize();
    }

    for (auto _ : state) {
        auto dist = graph.dijkstra(0);
        benchmark::DoNotOptimize(dist);
    }
    state.SetItemsProcessed(state.iterations() * num_vertices);
}
BENCHMARK_TEMPLATE(BM_Dijkstra, Graph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 16384)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_Dijkstra, CSRGraph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 16384)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_Dijkstra, CBListGraph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 16384)->Unit(benchmark::kMillisecond);

// 6. Dynamic Edge Insertion benchmark (only Naive and CBList)
template <typename GraphType>
static void BM_DynamicEdgeAddition(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t base_edges = static_cast<uint64_t>(num_vertices) * 4;
    const uint64_t dynamic_edges = static_cast<uint64_t>(num_vertices) * 2;

    const auto base = generateRandomGraph(num_vertices, base_edges, 111);
    const auto dynamic_batch = generateRandomGraph(num_vertices, dynamic_edges, 222);

    for (auto _ : state) {
        state.PauseTiming();
        GraphType graph;
        for (const auto& edge : base) {
            graph.addEdge(edge.u, edge.v, edge.weight);
        }
        state.ResumeTiming();

        for (const auto& edge : dynamic_batch) {
            graph.addEdgeDynamic(edge.u, edge.v, edge.weight);
        }
        benchmark::DoNotOptimize(graph.numEdges());
    }
    state.SetItemsProcessed(state.iterations() * dynamic_edges);
}
BENCHMARK_TEMPLATE(BM_DynamicEdgeAddition, Graph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 32768)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_DynamicEdgeAddition, CBListGraph<uint32_t, double>)
    ->RangeMultiplier(4)->Range(1024, 32768)->Unit(benchmark::kMillisecond);


// 7. Veliki graf s kompleksnim tipom vrha (dugački stringovi)
template <typename GraphType>
static void BM_LargeComplexVertex(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 16;
    const auto raw_edges = generateRandomGraph(num_vertices, num_edges, 333);

    // Pre-generiranje kompleksnih stringova kako ne bi usporavali samu petlju benchmarka
    std::vector<std::pair<std::string, std::string>> string_edges;
    string_edges.reserve(num_edges);
    for (const auto& e : raw_edges) {
        string_edges.emplace_back(
            "https://complex-domain.com/api/v1/users/profile/data?id=" + std::to_string(e.u),
            "https://complex-domain.com/api/v1/users/profile/data?id=" + std::to_string(e.v)
        );
    }

    for (auto _ : state) {
        GraphType graph(true);
        if constexpr (requires { graph.reserve(num_vertices, num_edges); }) {
            graph.reserve(num_vertices, num_edges);
        }
        for (size_t i = 0; i < num_edges; ++i) {
            graph.addEdge(string_edges[i].first, string_edges[i].second, raw_edges[i].weight);
        }
        benchmark::DoNotOptimize(graph.numEdges());
    }
    state.SetItemsProcessed(state.iterations() * num_edges);
}

// Skala ide do 1.048.576 vrhova (1M vrhova, ~16M bridova)
BENCHMARK_TEMPLATE(BM_LargeComplexVertex, Graph<std::string, double>)
    ->RangeMultiplier(4)->Range(16384, 1048576)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_LargeComplexVertex, CSRGraph<std::string, double>)
    ->RangeMultiplier(4)->Range(16384, 1048576)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_LargeComplexVertex, CBListGraph<std::string, double>)
    ->RangeMultiplier(4)->Range(16384, 1048576)->Unit(benchmark::kMillisecond);


// 8. BFS pretraga na velikom grafu s kompleksnim stringovima
template <typename GraphType>
static void BM_LargeComplexVertex_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto raw_edges = generateRandomGraph(num_vertices, num_edges, 777);

    // Pre-generiranje imena da se ne gubi vrijeme u petlji
    std::vector<std::string> vertex_names(num_vertices);
    for (uint32_t i = 0; i < num_vertices; ++i) {
        vertex_names[i] = "https://complex-domain.com/api/v1/users/profile/data?id=" + std::to_string(i);
    }

    GraphType graph(true);
    if constexpr (requires { graph.reserve(num_vertices, num_edges); }) {
        graph.reserve(num_vertices, num_edges);
    }
    for (const auto& e : raw_edges) {
        graph.addEdge(vertex_names[e.u], vertex_names[e.v], e.weight);
    }
    if constexpr (requires { graph.finalize(); }) {
        graph.finalize();
    }

    const std::string start_node = vertex_names[0];

    for (auto _ : state) {
        size_t visited_count = 0;
        graph.bfs(start_node, [&](const std::string&) {
            ++visited_count;
        });
        benchmark::DoNotOptimize(visited_count);
    }
    state.SetItemsProcessed(state.iterations() * num_vertices);
}

BENCHMARK_TEMPLATE(BM_LargeComplexVertex_BFS, Graph<std::string, double>)
    ->RangeMultiplier(4)->Range(1024, 262144)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_LargeComplexVertex_BFS, CSRGraph<std::string, double>)
    ->RangeMultiplier(4)->Range(1024, 262144)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_LargeComplexVertex_BFS, CBListGraph<std::string, double>)
    ->RangeMultiplier(4)->Range(1024, 262144)->Unit(benchmark::kMillisecond);


    // Teška struktura od ~264 bajta
struct DummyStruct {
    uint64_t id;
    char padding[256]; // Zauzima puno memorije

    bool operator==(const DummyStruct& other) const {
        return id == other.id;
    }
};

// Specijalizacija hasha za DummyStruct
namespace std {
    template <>
    struct hash<DummyStruct> {
        size_t operator()(const DummyStruct& k) const {
            return hash<uint64_t>()(k.id);
        }
    };
}

// 9. BFS pretraga s teškom strukturom (DummyStruct)
template <typename GraphType>
static void BM_HeavyVertex_BFS(benchmark::State& state) {
    const auto num_vertices = static_cast<uint32_t>(state.range(0));
    const uint64_t num_edges = static_cast<uint64_t>(num_vertices) * 8;
    const auto raw_edges = generateRandomGraph(num_vertices, num_edges, 444);

    // Pre-generiranje teških vrhova
    std::vector<DummyStruct> vertices(num_vertices);
    for (uint32_t i = 0; i < num_vertices; ++i) {
        vertices[i].id = i;
        // Padding ostaje neinicijaliziran ili prazan, bitna je samo veličina
    }

    GraphType graph(true);
    if constexpr (requires { graph.reserve(num_vertices, num_edges); }) {
        graph.reserve(num_vertices, num_edges);
    }
    for (const auto& e : raw_edges) {
        graph.addEdge(vertices[e.u], vertices[e.v], e.weight);
    }
    if constexpr (requires { graph.finalize(); }) {
        graph.finalize();
    }

    const DummyStruct start_node = vertices[0];

    for (auto _ : state) {
        size_t visited_count = 0;
        graph.bfs(start_node, [&](const DummyStruct&) {
            ++visited_count;
        });
        benchmark::DoNotOptimize(visited_count);
    }
    state.SetItemsProcessed(state.iterations() * num_vertices);
}

// Smanjujemo gornju granicu na 65536 jer DummyStruct guta puno RAM-a
BENCHMARK_TEMPLATE(BM_HeavyVertex_BFS, Graph<DummyStruct, double>)
    ->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_HeavyVertex_BFS, CSRGraph<DummyStruct, double>)
    ->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_HeavyVertex_BFS, CBListGraph<DummyStruct, double>)
    ->RangeMultiplier(4)->Range(1024, 65536)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();