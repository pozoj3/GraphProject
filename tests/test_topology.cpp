#include <gtest/gtest.h>
#include <vector>
#include <set>
#include <cstdint>

#include "graphs/NaiveGraph.hpp"
#include "graphs/CBListGraph.hpp"
#include "graphs/DynamicCSRGraph.hpp"
#include "graphs/StaticCSRGraph.hpp"
#include "graphs/RawGraph.hpp"
#include "graphs/GCCGraph.hpp"
#include "graphs/GraphGenerator.hpp"

using GraphGenerator::RawEdge;

namespace {

// Builds a directed graph of GraphType from a generated edge list
template <typename GraphType>
void checkStructure(const std::vector<RawEdge<double>>& edges, uint32_t V) {
    GraphType graph(true);
    if constexpr (requires { graph.reserve(V, edges.size()); }) {
        graph.reserve(V, edges.size());
    }
    std::set<std::pair<uint32_t, uint32_t>> unique_edges;
    for (const auto& e : edges) {
        graph.addEdge(e.u, e.v, e.weight);
        unique_edges.insert({e.u, e.v});
    }

    for (const auto& e : edges) {
        EXPECT_TRUE(graph.hasEdge(e.u, e.v))
            << "missing edge " << e.u << "->" << e.v;
    }
    EXPECT_EQ(graph.numEdges(), unique_edges.size());

    // self loops were not generated, so every hasEdge(u,u) must be false.
    for (uint32_t u = 0; u < V; ++u) {
        EXPECT_FALSE(graph.hasEdge(u, u));
    }
}


void checkAllStructures(const std::vector<RawEdge<double>>& edges, uint32_t V) {
    checkStructure<NaiveGraph<uint32_t, double>>(edges, V);
    checkStructure<CBListGraph<uint32_t, double>>(edges, V);
    checkStructure<DynamicCSRGraph<uint32_t, double>>(edges, V);
    checkStructure<RawGraph<uint32_t, double>>(edges, V);
    checkStructure<GCCGraph<uint32_t, double>>(edges, V);
}

} // namespace

TEST(TopologyTest, SparseGraphIsFaithfulAcrossStructures) {
    const uint32_t V = 300;
    auto edges = GraphGenerator::generateErdosRenyiStrictGaussian<double>(V, V * 2, 11);
    checkAllStructures(edges, V);
}

TEST(TopologyTest, DenseGraphIsFaithfulAcrossStructures) {
    const uint32_t V = 120;
    auto edges = GraphGenerator::generateErdosRenyiStrictGaussian<double>(V, V * 30, 22);
    checkAllStructures(edges, V);
}

// StaticCSR de-duplicates parallel edges
TEST(TopologyTest, StaticCSRDeduplicatesWhileListGraphsKeepParallelEdges) {
    RawGraph<uint32_t, double> raw(true);
    NaiveGraph<uint32_t, double> naive(true);
    for (int i = 0; i < 5; ++i) {          // 5 parallel copies of the same edge
        raw.addEdge(0, 1, 1.0);
        naive.addEdge(0, 1, 1.0);
    }
    StaticCSRGraph<uint32_t, double> csr(RawGraph<uint32_t, double>(std::move(raw)));

    EXPECT_EQ(csr.getDegree(0), 1u);       
    EXPECT_EQ(naive.getDegree(0), 5u);     
}

// 2d grid is fully connected, BFS/Dijkstra from a corner must reach every cell
TEST(TopologyTest, Grid2DIsFullyConnected) {
    const uint32_t dim = 10;
    const uint32_t V = dim * dim;
    auto edges = GraphGenerator::generateGrid2D<double>(dim, dim);

    StaticCSRGraph<uint32_t, double> csr(
        [&] {
            RawGraph<uint32_t, double> raw(true);
            for (const auto& e : edges) raw.addEdge(e.u, e.v, e.weight);
            return raw;
        }());

    std::set<uint32_t> visited;
    csr.bfs(0, [&](uint32_t v) { visited.insert(v); });
    EXPECT_EQ(visited.size(), V);

    auto dist = csr.dijkstra(0);
    EXPECT_EQ(dist.size(), V); 
}

TEST(TopologyTest, RMATEndpointsStayInRange) {
    const uint32_t V = 64;
    auto edges = GraphGenerator::generateRMAT<double>(V, V * 8, 0.57, 0.19, 0.19, 5);
    GCCGraph<uint32_t, double> g(true);
    for (const auto& e : edges) {
        ASSERT_LT(e.u, V);
        ASSERT_LT(e.v, V);
        g.addEdge(e.u, e.v, e.weight);
    }
    for (const auto& e : edges) EXPECT_TRUE(g.hasEdge(e.u, e.v));
}
