#include <gtest/gtest.h>
#include <string>
#include "Graph.hpp"

TEST(GraphTest, AddEdgeAndGetNeighbors) {
    Graph<int, double> graph;
    graph.addEdge(1, 2, 5.5);

    auto neighbors1 = graph.getNeighbors(1);
    ASSERT_EQ(neighbors1.size(), 1);
    EXPECT_EQ(neighbors1[0].first, 2);
    EXPECT_DOUBLE_EQ(neighbors1[0].second, 5.5);

    auto neighbors2 = graph.getNeighbors(2);
    ASSERT_EQ(neighbors2.size(), 1);
    EXPECT_EQ(neighbors2[0].first, 1);
    EXPECT_DOUBLE_EQ(neighbors2[0].second, 5.5);
}

TEST(GraphTest, NonExistentVertexReturnsEmpty) {
    Graph<std::string, int> graph;
    graph.addEdge("Koprivnica", "Ludbreg", 50);

    auto neighbors = graph.getNeighbors("Durdevec");

    EXPECT_TRUE(neighbors.empty());
}

TEST(GraphTest, MultipleNeighbors) {
    Graph<std::string, int> graph;
    graph.addEdge("A", "B", 10);
    graph.addEdge("A", "C", 20);
    graph.addEdge("A", "D", 30);

    auto neighbors = graph.getNeighbors("A");

    ASSERT_EQ(neighbors.size(), 3);

    EXPECT_EQ(neighbors[0].first, "B");
    EXPECT_EQ(neighbors[0].second, 10);

    EXPECT_EQ(neighbors[1].first, "C");
    EXPECT_EQ(neighbors[1].second, 20);

    EXPECT_EQ(neighbors[2].first, "D");
    EXPECT_EQ(neighbors[2].second, 30);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}