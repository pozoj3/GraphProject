#include <gtest/gtest.h>
#include <string>
#include <set>
#include <stdexcept>

#include "graphs/NaiveGraph.hpp"
#include "graphs/CBListGraph.hpp"
#include "graphs/DynamicCSRGraph.hpp"
#include "graphs/RawGraph.hpp"
#include "graphs/CSRGraph.hpp"
#include "graphs/StaticCSRGraph.hpp"
#include "graphs/GCCGraph.hpp"


template <typename T>
class EdgeCaseTest : public ::testing::Test {};

using MutableImplementations = ::testing::Types<
    NaiveGraph<std::string, double>,
    CBListGraph<std::string, double>,
    DynamicCSRGraph<std::string, double>,
    RawGraph<std::string, double>,
    GCCGraph<std::string, double>
>;
TYPED_TEST_SUITE(EdgeCaseTest, MutableImplementations);

TYPED_TEST(EdgeCaseTest, HighDegreeVertexKeepsAllNeighbors) {
    TypeParam graph(true);
    const int N = 50;
    for (int i = 0; i < N; ++i) graph.addEdge("hub", "n" + std::to_string(i), 1.0);

    EXPECT_EQ(graph.getDegree("hub"), static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) EXPECT_TRUE(graph.hasEdge("hub", "n" + std::to_string(i)));

    std::size_t visited = 0;
    graph.forEachNeighbor("hub", [&](const std::string&, double) { ++visited; });
    EXPECT_EQ(visited, static_cast<std::size_t>(N));
}

TYPED_TEST(EdgeCaseTest, UndirectedSelfLoopStoredOnce) {
    TypeParam graph(false);
    graph.addEdge("A", "A", 1.0);
    EXPECT_TRUE(graph.hasEdge("A", "A"));
    EXPECT_EQ(graph.getDegree("A"), 1u);
}

TYPED_TEST(EdgeCaseTest, DijkstraOmitsUnreachableVertices) {
    TypeParam graph(true);
    graph.addEdge("A", "B", 1.0);
    graph.addVertex("Island"); // unreachable from A

    auto dist = graph.dijkstra("A");
    EXPECT_DOUBLE_EQ(dist.at("A"), 0.0);
    EXPECT_DOUBLE_EQ(dist.at("B"), 1.0);
    EXPECT_EQ(dist.find("Island"), dist.end());
}

TYPED_TEST(EdgeCaseTest, DijkstraFromMissingStartIsEmpty) {
    TypeParam graph(true);
    graph.addEdge("A", "B", 1.0);
    auto dist = graph.dijkstra("DoesNotExist");
    EXPECT_TRUE(dist.empty());
}

TYPED_TEST(EdgeCaseTest, DfsVisitsAllReachable) {
    TypeParam graph(true);
    graph.addEdge("A", "B", 1.0);
    graph.addEdge("A", "C", 1.0);
    graph.addEdge("C", "D", 1.0);

    std::set<std::string> seen;
    graph.dfs("A", [&](const std::string& v) { seen.insert(v); });
    EXPECT_EQ(seen.size(), 4u);
}

TYPED_TEST(EdgeCaseTest, EmptyGraphIsWellDefined) {
    TypeParam graph(true);
    EXPECT_EQ(graph.numVertices(), 0u);
    EXPECT_EQ(graph.numEdges(), 0u);
    EXPECT_FALSE(graph.hasVertex("x"));
    EXPECT_EQ(graph.getDegree("x"), 0u);
    std::size_t count = 0;
    graph.traverseEntireGraph([&](const std::string&, const std::string&, double) { ++count; });
    EXPECT_EQ(count, 0u);
}

// preseration of real vertexes, for static csr
TEST(ConversionCtorTest, NaiveFromRawWithStringVerticesAndIsolated) {
    RawGraph<std::string, double> raw(true);
    raw.addEdge("alpha", "beta", 2.0);
    raw.addVertex("island");

    NaiveGraph<std::string, double> g(std::move(raw));
    EXPECT_TRUE(g.hasVertex("alpha"));
    EXPECT_TRUE(g.hasVertex("island"));
    EXPECT_TRUE(g.hasEdge("alpha", "beta"));
    EXPECT_EQ(g.getDegree("island"), 0u);
}

TEST(ConversionCtorTest, CBListFromRawWithStringVertices) {
    RawGraph<std::string, double> raw(true);
    raw.addEdge("alpha", "beta", 2.0);
    raw.addVertex("island");

    CBListGraph<std::string, double> g(std::move(raw));
    EXPECT_TRUE(g.hasVertex("island"));
    EXPECT_TRUE(g.hasEdge("alpha", "beta"));
    EXPECT_EQ(g.getDegree("island"), 0u);
}

TEST(ConversionCtorTest, DynamicCSRFromRawPreservesIdentity) {
    RawGraph<std::string, double> raw(true);
    raw.addEdge("x", "y", 1.0);
    raw.addVertex("z");

    DynamicCSRGraph<std::string, double> g(std::move(raw));
    EXPECT_TRUE(g.hasVertex("z"));
    EXPECT_TRUE(g.hasEdge("x", "y"));
    EXPECT_EQ(g.numVertices(), 3u);
}

TEST(ConversionCtorTest, StaticCSRFromRawPreservesIdentityAndIsolated) {
    RawGraph<std::string, double> raw(true);
    raw.addEdge("alpha", "gamma", 3.0);
    raw.addVertex("island");

    StaticCSRGraph<std::string, double> g(std::move(raw));
    EXPECT_TRUE(g.hasVertex("alpha"));
    EXPECT_TRUE(g.hasVertex("gamma"));
    EXPECT_TRUE(g.hasVertex("island"));
    EXPECT_TRUE(g.hasEdge("alpha", "gamma"));
    EXPECT_EQ(g.getDegree("island"), 0u);
    EXPECT_EQ(g.numVertices(), 3u);
}


// csr test (outdated)
TEST(CSRGraphEdgeCaseTest, DegreeAndLookupZeroBeforeFinalize) {
    CSRGraph<int, double> g(true);
    g.addEdge(0, 1, 1.0);
    EXPECT_EQ(g.getDegree(0), 0u);
    EXPECT_FALSE(g.hasEdge(0, 1));

    g.finalize();
    EXPECT_EQ(g.getDegree(0), 1u);
    EXPECT_TRUE(g.hasEdge(0, 1));
}

TEST(CSRGraphEdgeCaseTest, DeduplicatesParallelEdges) {
    CSRGraph<int, double> g(true);
    g.addEdge(0, 1, 1.0);
    g.addEdge(0, 1, 2.0); // duplicate destination
    g.addEdge(0, 2, 3.0);
    g.finalize();

    EXPECT_EQ(g.getDegree(0), 2u); 
    EXPECT_TRUE(g.hasEdge(0, 1));
    EXPECT_TRUE(g.hasEdge(0, 2));
}

TEST(CSRGraphEdgeCaseTest, ReFinalizeIsIdempotent) {
    CSRGraph<int, double> g(true);
    g.addEdge(0, 1, 1.0);
    g.addEdge(0, 2, 2.0);
    g.finalize();
    const std::size_t deg = g.getDegree(0);

    g.finalize(); 
    EXPECT_EQ(g.getDegree(0), deg);
    EXPECT_TRUE(g.hasEdge(0, 1));
    EXPECT_TRUE(g.hasEdge(0, 2));
}

// chunk check and multiple chunk check for cblist
TEST(CBListEdgeCaseTest, ChunkIsCacheLineAligned) {
    using Chunk = ChunkArena<double>::Chunk;
    EXPECT_EQ(alignof(Chunk), 64u);
    EXPECT_EQ(sizeof(Chunk) % 64u, 0u);
}

TEST(CBListEdgeCaseTest, SpansMultipleChunks) {
    CBListGraph<int, double> g(true);
    const int N = 40;
    for (int i = 1; i <= N; ++i) g.addEdge(0, i, static_cast<double>(i));

    EXPECT_EQ(g.getDegree(0), static_cast<std::size_t>(N));
    for (int i = 1; i <= N; ++i) EXPECT_TRUE(g.hasEdge(0, i, static_cast<double>(i)));
}
