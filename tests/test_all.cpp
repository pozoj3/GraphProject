#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>

#include "graphs/CBListGraph.hpp"
#include "graphs/CSRGraph.hpp"
#include "graphs/NaiveGraph.hpp"

// Helper to conditionally finalize CSR graphs before queries
template <typename GraphType>
void finalizeIfSupported(GraphType& graph) {
    if constexpr (requires { graph.finalize(); }) {
        graph.finalize();
    }
}

template <typename T>
class CommonGraphTest : public ::testing::Test {};

// Graph implementations subjected to the common typed test suite
using GraphImplementations = ::testing::Types<
    NaiveGraph<std::string, double>,
    CSRGraph<std::string, double>,
    CBListGraph<std::string, double>
>;

TYPED_TEST_SUITE(CommonGraphTest, GraphImplementations);

// Verify edge insertion counts, directionality, and direct lookups
TYPED_TEST(CommonGraphTest, DirectedEdgeCountsAndLookup) {
    TypeParam graph(true);
    graph.addEdge("A", "B", 10.0);
    graph.addEdge("A", "C", 20.0);
    finalizeIfSupported(graph);

    EXPECT_EQ(graph.numVertices(), 3);
    EXPECT_EQ(graph.numEdges(), 2);
    EXPECT_TRUE(graph.hasEdge("A", "B"));
    EXPECT_TRUE(graph.hasEdge("A", "C"));
    EXPECT_FALSE(graph.hasEdge("B", "A"));
}

// Verify outgoing neighbor traversal callback and edge weights
TYPED_TEST(CommonGraphTest, NeighborIteration) {
    TypeParam graph(true);
    graph.addEdge("A", "B", 5.0);
    graph.addEdge("A", "C", 15.0);
    finalizeIfSupported(graph);

    std::vector<std::pair<std::string, double>> neighbors;
    graph.forEachNeighbor("A", [&](const std::string& target, double weight) {
        neighbors.emplace_back(target, weight);
    });

    EXPECT_EQ(neighbors.size(), 2);
    std::sort(neighbors.begin(), neighbors.end());
    EXPECT_EQ(neighbors[0].first, "B");
    EXPECT_EQ(neighbors[1].first, "C");
}

// Verify isolated vertex handling and out-degree queries
TYPED_TEST(CommonGraphTest, IsolatedVertexAndDegree) {
    TypeParam graph(true);
    graph.addVertex("Isolated");
    graph.addEdge("A", "B", 1.0);
    graph.addEdge("A", "C", 2.0);
    finalizeIfSupported(graph);

    EXPECT_TRUE(graph.hasVertex("Isolated"));
    EXPECT_TRUE(graph.hasVertex("A"));
    EXPECT_FALSE(graph.hasVertex("NonExisting"));

    EXPECT_EQ(graph.getDegree("A"), 2);
    EXPECT_EQ(graph.getDegree("B"), 0);
    EXPECT_EQ(graph.getDegree("Isolated"), 0);
    EXPECT_EQ(graph.getDegree("NonExisting"), 0);
}

// Verify iterating through all edges in the entire graph
TYPED_TEST(CommonGraphTest, TraverseEntireGraph) {
    TypeParam graph(true);
    graph.addEdge("A", "B", 1.0);
    graph.addEdge("B", "C", 2.0);
    graph.addEdge("C", "D", 3.0);
    finalizeIfSupported(graph);

    size_t count = 0;
    double sum_weights = 0.0;
    graph.traverseEntireGraph([&](const std::string&, double weight) {
        ++count;
        sum_weights += weight;
    });

    EXPECT_EQ(count, 3);
    EXPECT_DOUBLE_EQ(sum_weights, 6.0);
}

// Verify reachability order and node coverage for BFS and DFS
TYPED_TEST(CommonGraphTest, BFSAndDFS) {
    TypeParam graph(true);
    // Tree topology: A -> B, A -> C, B -> D, C -> E
    graph.addEdge("A", "B", 1.0);
    graph.addEdge("A", "C", 1.0);
    graph.addEdge("B", "D", 1.0);
    graph.addEdge("C", "E", 1.0);
    finalizeIfSupported(graph);

    std::vector<std::string> bfs_order;
    graph.bfs("A", [&](const std::string& node) {
        bfs_order.push_back(node);
    });

    EXPECT_EQ(bfs_order.size(), 5);
    EXPECT_EQ(bfs_order[0], "A");

    std::vector<std::string> dfs_order;
    graph.dfs("A", [&](const std::string& node) {
        dfs_order.push_back(node);
    });

    EXPECT_EQ(dfs_order.size(), 5);
    EXPECT_EQ(dfs_order[0], "A");
}

// Verify shortest path computations and unreachable vertex behavior
TYPED_TEST(CommonGraphTest, DijkstraShortestPath) {
    TypeParam graph(true);
    // Path through C (1.0 + 1.0 = 2.0) is shorter than direct edge A -> B (4.0)
    graph.addEdge("A", "B", 4.0);
    graph.addEdge("A", "C", 1.0);
    graph.addEdge("C", "B", 1.0);
    graph.addEdge("B", "D", 2.0);
    graph.addVertex("Unreachable");
    finalizeIfSupported(graph);

    auto dist = graph.dijkstra("A");

    EXPECT_DOUBLE_EQ(dist["A"], 0.0);
    EXPECT_DOUBLE_EQ(dist["C"], 1.0);
    EXPECT_DOUBLE_EQ(dist["B"], 2.0);
    EXPECT_DOUBLE_EQ(dist["D"], 4.0);
    EXPECT_FALSE(dist.contains("Unreachable"));
}

// Dynamic graph suite (excludes CSRGraph as it requires rebuild/finalization)
template <typename T>
class DynamicGraphTest : public ::testing::Test {};

using DynamicGraphImplementations = ::testing::Types<
    NaiveGraph<std::string, double>,
    CBListGraph<std::string, double>
>;

TYPED_TEST_SUITE(DynamicGraphTest, DynamicGraphImplementations);

// Verify runtime edge insertion after initial construction and query execution
TYPED_TEST(DynamicGraphTest, DynamicEdgeAddition) {
    TypeParam graph(true);
    graph.addEdge("A", "B", 1.0);
    EXPECT_TRUE(graph.hasEdge("A", "B"));
    EXPECT_FALSE(graph.hasEdge("B", "C"));

    // Insert new edge dynamically on the fly
    graph.addEdgeDynamic("B", "C", 3.5);
    EXPECT_TRUE(graph.hasEdge("B", "C"));
    EXPECT_EQ(graph.getDegree("B"), 1);

    // Verify algorithms pick up dynamic additions immediately
    auto dist = graph.dijkstra("A");
    EXPECT_DOUBLE_EQ(dist["C"], 4.5);
}