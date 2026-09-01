#include <gtest/gtest.h>
#include <string>
#include "graphengine/CBListGraph.hpp"

TEST(CBListGraphTest, BasicAddAndLookup) {
    CBListGraph<std::string, double> graph(false); // Neusmjeren
    graph.addEdge("Koprivnica", "Ludbreg", 25.0);

    EXPECT_TRUE(graph.hasEdge("Koprivnica", "Ludbreg"));
    EXPECT_TRUE(graph.hasEdge("Ludbreg", "Koprivnica"));
    EXPECT_EQ(graph.numVertices(), 2);
    EXPECT_EQ(graph.numEdges(), 1);
}

TEST(CBListGraphTest, SmallChunkOverflowAndGTChain) {
    CBListGraph<int, double> graph(true); // Usmjeren
    // Dodajemo 6 bridova (kapacitet jednog SmallChunka je 4)
    graph.addEdge(1, 10, 1.0);
    graph.addEdge(1, 11, 2.0);
    graph.addEdge(1, 12, 3.0);
    graph.addEdge(1, 13, 4.0);
    graph.addEdge(1, 14, 5.0);
    graph.addEdge(1, 15, 6.0);

    EXPECT_TRUE(graph.hasEdge(1, 10));
    EXPECT_TRUE(graph.hasEdge(1, 15));
    EXPECT_FALSE(graph.hasEdge(1, 99));

    int count = 0;
    graph.forEachNeighbor(1, [&](int /*v*/, double /*w*/) {
        count++;
    });
    EXPECT_EQ(count, 6);

    // Globalni prolaz kroz GTChain
    size_t global_edges = 0;
    graph.traverseEntireGraph([&](uint32_t /*target*/, double /*weight*/) {
        global_edges++;
    });
    EXPECT_EQ(global_edges, 6);
}