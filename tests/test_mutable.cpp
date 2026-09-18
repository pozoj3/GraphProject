#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <algorithm>

#include "graphs/NaiveGraph.hpp"
#include "graphs/CBListGraph.hpp"
#include "graphs/DynamicCSRGraph.hpp"
#include "graphs/RawGraph.hpp"

template <typename T>
class MutableGraphTest : public ::testing::Test {};

using MutableGraphImplementations = ::testing::Types<
    NaiveGraph<std::string, double>,
    CBListGraph<std::string, double>,
    DynamicCSRGraph<std::string, double>,
    RawGraph<std::string, double>
>;

TYPED_TEST_SUITE(MutableGraphTest, MutableGraphImplementations);

TYPED_TEST(MutableGraphTest, DirectedEdgeCountsAndLookup) {
    // Verifies that adding directed edges updates counts and allows successful lookups.
    TypeParam graph(true);
    graph.addEdge("A", "B", 10.0);
    graph.addEdge("A", "C", 20.0);

    EXPECT_EQ(graph.numVertices(), 3);
    EXPECT_EQ(graph.numEdges(), 2);
    EXPECT_TRUE(graph.hasEdge("A", "B"));
    EXPECT_TRUE(graph.hasEdge("A", "C"));
    EXPECT_FALSE(graph.hasEdge("B", "A"));
}

TYPED_TEST(MutableGraphTest, NeighborIteration) {
    // Verifies neighbor iteration visits all connected vertices with correct weights.
    TypeParam graph(true);
    graph.addEdge("A", "B", 5.0);
    graph.addEdge("A", "C", 15.0);

    std::vector<std::pair<std::string, double>> neighbors;
    graph.forEachNeighbor("A", [&](const std::string& target, double weight) {
        neighbors.emplace_back(target, weight);
    });

    EXPECT_EQ(neighbors.size(), 2);
    std::sort(neighbors.begin(), neighbors.end());
    EXPECT_EQ(neighbors[0].first, "B");
    EXPECT_EQ(neighbors[1].first, "C");
}

TYPED_TEST(MutableGraphTest, IsolatedVertexAndDegree) {
    // Verifies that isolated vertices are correctly added and have a degree of zero.
    TypeParam graph(true);
    graph.addVertex("Isolated");
    graph.addEdge("A", "B", 1.0);

    EXPECT_TRUE(graph.hasVertex("Isolated"));
    EXPECT_TRUE(graph.hasVertex("A"));
    EXPECT_FALSE(graph.hasVertex("NonExisting"));
    EXPECT_EQ(graph.getDegree("A"), 1);
    EXPECT_EQ(graph.getDegree("Isolated"), 0);
}

TYPED_TEST(MutableGraphTest, TraverseEntireGraph) {
    // Verifies that traversing the entire graph visits all edges exactly once.
    TypeParam graph(true);
    graph.addEdge("A", "B", 1.0);
    graph.addEdge("B", "C", 2.0);

    size_t count = 0;
    double sum_weights = 0.0;
    graph.traverseEntireGraph([&](const std::string&, const std::string&, double weight) {
        ++count;
        sum_weights += weight;
    });

    EXPECT_EQ(count, 2);
    EXPECT_DOUBLE_EQ(sum_weights, 3.0);
}

TYPED_TEST(MutableGraphTest, BFSAndDFS) {
    // Verifies that BFS and DFS visit all reachable nodes in a connected component.
    TypeParam graph(true);
    graph.addEdge("A", "B", 1.0);
    graph.addEdge("A", "C", 1.0);

    std::vector<std::string> bfs_order;
    graph.bfs("A", [&](const std::string& node) {
        bfs_order.push_back(node);
    });
    EXPECT_EQ(bfs_order.size(), 3);
    EXPECT_EQ(bfs_order[0], "A");
}

TYPED_TEST(MutableGraphTest, DijkstraShortestPath) {
    // Verifies that Dijkstra's algorithm correctly finds the shortest paths.
    TypeParam graph(true);
    graph.addEdge("A", "B", 4.0);
    graph.addEdge("A", "C", 1.0);
    graph.addEdge("C", "B", 1.0);

    auto dist = graph.dijkstra("A");
    EXPECT_DOUBLE_EQ(dist["A"], 0.0);
    EXPECT_DOUBLE_EQ(dist["C"], 1.0);
    EXPECT_DOUBLE_EQ(dist["B"], 2.0);
}

TYPED_TEST(MutableGraphTest, DijkstraThrowsOnNegativeWeight) {
    // Verifies that Dijkstra's algorithm throws std::invalid_argument when encountering negative weights.
    TypeParam graph(true);
    graph.addEdge("A", "B", -5.0);

    EXPECT_THROW(graph.dijkstra("A"), std::invalid_argument);
}

TYPED_TEST(MutableGraphTest, UndirectedGraphSymmetry) {
    // Verifies that undirected graphs implicitly create reverse edges for symmetry.
    TypeParam graph(false);
    graph.addEdge("A", "B", 10.0);

    EXPECT_TRUE(graph.hasEdge("A", "B"));
    EXPECT_TRUE(graph.hasEdge("B", "A"));
    EXPECT_EQ(graph.numEdges(), 1); 
}