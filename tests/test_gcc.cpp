#include <gtest/gtest.h>
#include <set>
#include <stdexcept>

#include "graphs/GCCGraph.hpp"

// These tests exercise storage representations 

TEST(GCCGraphTest, BasicInsertAndQuery) {
    GCCGraph<int, double> g(true);
    g.addEdge(1, 2, 3.0);
    g.addEdge(1, 3, 4.0);

    EXPECT_TRUE(g.hasVertex(1));
    EXPECT_FALSE(g.hasVertex(42));
    EXPECT_TRUE(g.hasEdge(1, 2));
    EXPECT_TRUE(g.hasEdge(1, 3));
    EXPECT_FALSE(g.hasEdge(2, 1));
    EXPECT_TRUE(g.hasEdge(1, 2, 3.0));
    EXPECT_FALSE(g.hasEdge(1, 2, 999.0));
    EXPECT_EQ(g.getDegree(1), 2u);
    EXPECT_EQ(g.numVertices(), 3u);
    EXPECT_EQ(g.numEdges(), 2u);
}

TEST(GCCGraphTest, DuplicateEdgeUpdatesWeightInPlace) {
    //existing edge must overwrite the weight
    GCCGraph<int, double> g(true);
    g.addEdge(1, 2, 3.0);
    g.addEdge(1, 2, 9.0);

    EXPECT_EQ(g.getDegree(1), 1u);
    EXPECT_TRUE(g.hasEdge(1, 2, 9.0));
    EXPECT_FALSE(g.hasEdge(1, 2, 3.0));
}

TEST(GCCGraphTest, StaysSmallChunkForLowDegree) {
    GCCGraph<int, double> g(true);
    g.addEdge(0, 1, 1.0);
    g.addEdge(0, 2, 1.0);
    EXPECT_EQ(g.getVertexLevel(0), 0u);
}

TEST(GCCGraphTest, PromotesToBPlusTreeForHighDegree) {
    GCCGraph<int, double> g(true);
    const int N = 100;
    for (int i = 1; i <= N; ++i) g.addEdge(0, i, static_cast<double>(i));

    EXPECT_GT(g.getVertexLevel(0), 0u); // promoted
    EXPECT_EQ(g.getDegree(0), static_cast<std::size_t>(N));

    for (int i = 1; i <= N; ++i) {
        EXPECT_TRUE(g.hasEdge(0, i));
        EXPECT_TRUE(g.hasEdge(0, i, static_cast<double>(i)));
    }

    std::set<int> seen;
    std::size_t count = 0;
    g.forEachNeighbor(0, [&](int v, double) { seen.insert(v); ++count; });
    EXPECT_EQ(count, static_cast<std::size_t>(N));
    EXPECT_EQ(seen.size(), static_cast<std::size_t>(N));
}

TEST(GCCGraphTest, DuplicateUpdateWorksAfterPromotion) {
    GCCGraph<int, double> g(true);
    const int N = 100;
    for (int i = 1; i <= N; ++i) g.addEdge(0, i, 1.0);
    ASSERT_GT(g.getVertexLevel(0), 0u);

    g.addEdge(0, 50, 123.0); 
    EXPECT_EQ(g.getDegree(0), static_cast<std::size_t>(N));
    EXPECT_TRUE(g.hasEdge(0, 50, 123.0));
    EXPECT_FALSE(g.hasEdge(0, 50, 1.0));
}

TEST(GCCGraphTest, TraverseEntireGraphViaGTChainMixed) {
    //B+ tree vertex and one small-chunk vertex
    GCCGraph<int, double> g(true);
    for (int i = 1; i <= 50; ++i) g.addEdge(0, i, 1.0);
    g.addEdge(1000, 1, 2.0);
    g.addEdge(1000, 2, 2.0);

    std::size_t count = 0;
    double sum = 0.0;
    g.traverseEntireGraph([&](int, int, double w) { ++count; sum += w; });

    EXPECT_EQ(count, g.numEdges());
    EXPECT_EQ(count, 52u);
    EXPECT_DOUBLE_EQ(sum, 50 * 1.0 + 2 * 2.0);
}

TEST(GCCGraphTest, UndirectedSymmetry) {
    GCCGraph<int, double> g(false);
    g.addEdge(1, 2, 5.0);
    EXPECT_TRUE(g.hasEdge(1, 2));
    EXPECT_TRUE(g.hasEdge(2, 1));
    EXPECT_EQ(g.numEdges(), 1u);
}

TEST(GCCGraphTest, SelfLoopDirectedStoredOnce) {
    GCCGraph<int, double> g(true);
    g.addEdge(7, 7, 1.0);
    EXPECT_TRUE(g.hasEdge(7, 7));
    EXPECT_EQ(g.getDegree(7), 1u);
    EXPECT_EQ(g.numEdges(), 1u);
}

TEST(GCCGraphTest, InheritsBaseAlgorithms) {
    // bfs/dfs/dijkstra come from BaseGraph via CRTP
    GCCGraph<int, double> g(true);
    g.addEdge(1, 2, 1.0);
    g.addEdge(1, 3, 1.0);
    g.addEdge(3, 4, 1.0);

    std::set<int> bfs_seen;
    g.bfs(1, [&](int v) { bfs_seen.insert(v); });
    EXPECT_EQ(bfs_seen.size(), 4u);

    std::set<int> dfs_seen;
    g.dfs(1, [&](int v) { dfs_seen.insert(v); });
    EXPECT_EQ(dfs_seen.size(), 4u);

    auto dist = g.dijkstra(1);
    EXPECT_DOUBLE_EQ(dist[1], 0.0);
    EXPECT_DOUBLE_EQ(dist[4], 2.0);
}

TEST(GCCGraphTest, GetVertexLevelThrowsForMissingVertex) {
    GCCGraph<int, double> g(true);
    EXPECT_THROW((void)g.getVertexLevel(999), std::out_of_range);
}

TEST(GCCGraphTest, NodesAreCacheLineAligned) {
    // hardwere locality
    using G = GCCGraph<int, double>;
    EXPECT_EQ(G::smallChunkAlignment(), 64u);
    EXPECT_EQ(G::bPlusNodeAlignment(), 64u);
    EXPECT_EQ(G::smallChunkNodeSize() % 64u, 0u);
    EXPECT_EQ(G::bPlusNodeSize() % 64u, 0u);
}
