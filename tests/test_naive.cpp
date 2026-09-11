#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

#include "graphs/NaiveGraph.hpp"

TEST(NaiveGraphTest, AddDirectedEdge) {
    Graph<int, double> graph(true);
    graph.addEdge(1, 2, 5.5);

    EXPECT_EQ(graph.numVertices(), 2);
    EXPECT_EQ(graph.numEdges(), 1);
    EXPECT_TRUE(graph.hasVertex(1));
    EXPECT_TRUE(graph.hasVertex(2));
    EXPECT_TRUE(graph.hasEdge(1, 2));
    EXPECT_FALSE(graph.hasEdge(2, 1));
}

TEST(NaiveGraphTest, QueryMissingVerticesAndEdges) {
    Graph<std::string, int> graph(true);
    graph.addEdge("London", "Paris", 50);

    EXPECT_TRUE(graph.hasVertex("London"));
    EXPECT_TRUE(graph.hasVertex("Paris"));
    EXPECT_FALSE(graph.hasVertex("Berlin"));

    EXPECT_TRUE(graph.hasEdge("London", "Paris"));
    EXPECT_FALSE(graph.hasEdge("London", "Berlin"));
    EXPECT_FALSE(graph.hasEdge("Rome", "Berlin"));

    size_t callback_invocations = 0;
    graph.forEachNeighbor("Rome", [&](const std::string&, int ) {
        ++callback_invocations;
    });
    EXPECT_EQ(callback_invocations, 0);
}

TEST(NaiveGraphTest, IterateMultipleNeighbors) {
    Graph<std::string, int> graph(true);
    graph.addEdge("A", "B", 10);
    graph.addEdge("A", "C", 20);
    graph.addEdge("A", "D", 30);

    EXPECT_EQ(graph.numVertices(), 4);
    EXPECT_EQ(graph.numEdges(), 3);

    std::vector<std::pair<std::string, int>> neighbors;
    graph.forEachNeighbor("A", [&](const std::string& target, int weight) {
        neighbors.emplace_back(target, weight);
    });

    EXPECT_EQ(neighbors.size(), 3);
    std::sort(neighbors.begin(), neighbors.end());

    EXPECT_EQ(neighbors[0].first, "B");
    EXPECT_EQ(neighbors[0].second, 10);

    EXPECT_EQ(neighbors[1].first, "C");
    EXPECT_EQ(neighbors[1].second, 20);

    EXPECT_EQ(neighbors[2].first, "D");
    EXPECT_EQ(neighbors[2].second, 30);
}

TEST(NaiveGraphTest, UndirectedEdgeSymmetry) {
    Graph<std::string, double> graph(false);
    graph.addEdge("London", "Paris", 400.0);

    EXPECT_EQ(graph.numVertices(), 2);
    EXPECT_EQ(graph.numEdges(), 1);
    EXPECT_TRUE(graph.hasEdge("London", "Paris"));
    EXPECT_TRUE(graph.hasEdge("Paris", "London"));

    size_t london_degree = 0;
    graph.forEachNeighbor("London", [&](const std::string& target, double weight) {
        ++london_degree;
        EXPECT_EQ(target, "Paris");
        EXPECT_DOUBLE_EQ(weight, 400.0);
    });
    EXPECT_EQ(london_degree, 1);

    size_t paris_degree = 0;
    graph.forEachNeighbor("Paris", [&](const std::string& target, double weight) {
        ++paris_degree;
        EXPECT_EQ(target, "London");
        EXPECT_DOUBLE_EQ(weight, 400.0);
    });
    EXPECT_EQ(paris_degree, 1);
}

TEST(NaiveGraphTest, AddIsolatedVertex) {
    Graph<int, int> graph(true);
    graph.addVertex(100);

    EXPECT_TRUE(graph.hasVertex(100));
    EXPECT_EQ(graph.numVertices(), 1);
    EXPECT_EQ(graph.numEdges(), 0);

    size_t neighbor_count = 0;
    graph.forEachNeighbor(100, [&](int, int) {
        ++neighbor_count;
    });
    EXPECT_EQ(neighbor_count, 0);
}