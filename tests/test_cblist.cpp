#include <gtest/gtest.h>
#include <string>

#include "graphs/CBListGraph.hpp"

TEST(CBListGraphTest, UndirectedEdgeLookup) {
    CBListGraph<std::string, double> graph(false);
    graph.addEdge("London", "Paris", 25.0);

    EXPECT_TRUE(graph.hasEdge("London", "Paris"));
    EXPECT_TRUE(graph.hasEdge("Paris", "London"));
    EXPECT_EQ(graph.numVertices(), 2);
    EXPECT_EQ(graph.numEdges(), 1);
}

TEST(CBListGraphTest, ChunkOverflowAndGlobalTraversal) {
    CBListGraph<int, double> graph(true);

    for (int target = 10; target <= 15; ++target) {
        graph.addEdge(1, target, static_cast<double>(target - 9));
    }

    EXPECT_TRUE(graph.hasEdge(1, 10));
    EXPECT_TRUE(graph.hasEdge(1, 15));
    EXPECT_FALSE(graph.hasEdge(1, 99));

    size_t neighbor_count = 0;
    graph.forEachNeighbor(1, [&](int, double) {
        ++neighbor_count;
    });
    EXPECT_EQ(neighbor_count, 6);

    size_t total_edges = 0;
    graph.traverseEntireGraph([&](uint32_t, double) {
        ++total_edges;
    });
    EXPECT_EQ(total_edges, 6);
}