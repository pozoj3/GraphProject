#include <gtest/gtest.h>
#include <set>
#include <utility>

#include "graphs/GraphGenerator.hpp"

using GraphGenerator::RawEdge;

TEST(GraphGeneratorTest, ErdosRenyiEdgeCountAndNoSelfLoops) {
    //are there self loops
    auto edges = GraphGenerator::generateErdosRenyi<double>(100, 500, 42);
    EXPECT_EQ(edges.size(), 500u);
    for (const auto& e : edges) {
        EXPECT_NE(e.u, e.v); 
        EXPECT_LT(e.u, 100u);
        EXPECT_LT(e.v, 100u);
    }
}

TEST(GraphGeneratorTest, ErdosRenyiStrictHasNoDuplicatesOrSelfLoops) {
    auto edges = GraphGenerator::generateErdosRenyiStrictGaussian<double>(100, 400, 7);
    EXPECT_EQ(edges.size(), 400u);

    std::set<std::pair<uint32_t, uint32_t>> seen;
    for (const auto& e : edges) {
        EXPECT_NE(e.u, e.v);
        EXPECT_GE(e.weight, 1.0);
        auto inserted = seen.insert({e.u, e.v});
        EXPECT_TRUE(inserted.second) << "duplicate edge produced by strict generator";
    }
}

TEST(GraphGeneratorTest, RMATEndpointsInRange) {
    const uint32_t V = 16; // power of two for the rmat quadrant recursion
    auto edges = GraphGenerator::generateRMAT<double>(V, 200, 0.57, 0.19, 0.19, 123);
    EXPECT_EQ(edges.size(), 200u);
    for (const auto& e : edges) {
        EXPECT_LT(e.u, V);
        EXPECT_LT(e.v, V);
        EXPECT_NE(e.u, e.v);
    }
}

TEST(GraphGeneratorTest, Grid2DHasExpectedBidirectionalEdgeCount) {
    // is edge count good
    const uint32_t w = 4, h = 3;
    auto edges = GraphGenerator::generateGrid2D<double>(w, h);
    const uint32_t expected = 2u * (h * (w - 1) + w * (h - 1));
    EXPECT_EQ(edges.size(), expected);
    for (const auto& e : edges) {
        EXPECT_LT(e.u, w * h);
        EXPECT_LT(e.v, w * h);
    }
}

TEST(GraphGeneratorTest, PointerChasingIsSingleCycle) {
    const uint32_t V = 50;
    auto edges = GraphGenerator::generatePointerChasingChain<double>(V, 42);
    EXPECT_EQ(edges.size(), V); // V-1 chain links + 1 closing edge

    // Every vertex is the source of exactly one edge and the target of exactly
    std::set<uint32_t> sources, targets;
    for (const auto& e : edges) {
        EXPECT_TRUE(sources.insert(e.u).second);
        EXPECT_TRUE(targets.insert(e.v).second);
    }
    EXPECT_EQ(sources.size(), V);
    EXPECT_EQ(targets.size(), V);
}

TEST(GraphGeneratorTest, DeterministicForFixedSeed) {
    auto a = GraphGenerator::generateErdosRenyi<double>(64, 300, 2024);
    auto b = GraphGenerator::generateErdosRenyi<double>(64, 300, 2024);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].u, b[i].u);
        EXPECT_EQ(a[i].v, b[i].v);
        EXPECT_DOUBLE_EQ(a[i].weight, b[i].weight);
    }
}
