#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <algorithm>

#include "graphengine/NaiveGraph.hpp"
#include "graphengine/CSRGraph.hpp"
#include "graphengine/CBListGraph.hpp"

template <typename G>
void finalizeIfRequired(G& graph) {
    if constexpr (requires { graph.finalize(); }) {
        graph.finalize();
    }
}

template <typename T>
class CommonGraphTest : public ::testing::Test {};

using AllImplementations = ::testing::Types<
    NaiveGraph<std::string, double>,
    CSRGraph<std::string, double>,
    CBListGraph<std::string, double>
>;

TYPED_TEST_SUITE(CommonGraphTest, AllImplementations);

TYPED_TEST(CommonGraphTest, SymmetricBehaviorAndCounts) {
    TypeParam graph(true);
    graph.addEdge("A", "B", 10.0);
    graph.addEdge("A", "C", 20.0);
    finalizeIfRequired(graph);

    EXPECT_EQ(graph.numVertices(), 3);
    EXPECT_EQ(graph.numEdges(), 2);
    EXPECT_TRUE(graph.hasEdge("A", "B"));
    EXPECT_TRUE(graph.hasEdge("A", "C"));
    EXPECT_FALSE(graph.hasEdge("B", "A"));
}

TYPED_TEST(CommonGraphTest, TraverseNeighborsCorrectly) {
    TypeParam graph(true);
    graph.addEdge("A", "B", 5.0);
    graph.addEdge("A", "C", 15.0);
    finalizeIfRequired(graph);

    std::vector<std::pair<std::string, double>> neighbors;
    graph.forEachNeighbor("A", [&](const std::string& n, double w) {
        neighbors.push_back({n, w});
    });

    EXPECT_EQ(neighbors.size(), 2);
    std::sort(neighbors.begin(), neighbors.end());
    EXPECT_EQ(neighbors[0].first, "B");
    EXPECT_EQ(neighbors[1].first, "C");
}