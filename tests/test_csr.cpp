#include <gtest/gtest.h>
#include <string>

#include "graphs/CSRGraph.hpp"

TEST(CSRGraphTest, RequiresFinalizeBeforeLookup) {
    CSRGraph<std::string, double> graph(true);
    graph.reserve(3, 2);
    graph.addEdge("A", "B", 1.5);
    graph.addEdge("A", "C", 2.5);

    EXPECT_FALSE(graph.hasEdge("A", "B"));

    graph.finalize();

    EXPECT_TRUE(graph.hasEdge("A", "B"));
    EXPECT_TRUE(graph.hasEdge("A", "C"));
    EXPECT_FALSE(graph.hasEdge("B", "A"));
    EXPECT_EQ(graph.numVertices(), 3);
    EXPECT_EQ(graph.numEdges(), 2);
}

TEST(CSRGraphTest, NeighborIterationAndSinkVertex) {
    CSRGraph<int, double> graph(true);
    graph.addEdge(0, 1, 4.0);
    graph.addEdge(0, 2, 2.0);
    graph.addEdge(1, 2, 5.0);
    graph.addEdge(3, 0, 1.5);
    graph.finalize();

    size_t neighbors_of_0 = 0;
    graph.forEachNeighbor(0, [&](int, double) {
        ++neighbors_of_0;
    });
    EXPECT_EQ(neighbors_of_0, 2);

    size_t neighbors_of_2 = 0;
    graph.forEachNeighbor(2, [&](int, double) {
        ++neighbors_of_2;
    });
    EXPECT_EQ(neighbors_of_2, 0);
}