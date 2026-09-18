#include <gtest/gtest.h>
#include <string>

#include "graphs/StaticCSRGraph.hpp"
#include "graphs/DynamicCSRGraph.hpp"
#include "graphs/RawGraph.hpp"

TEST(StaticCSRGraphTest, ConstructionFromDynamicCSR) {
    // Verifies that StaticCSRGraph is correctly constructed from a DynamicCSRGraph.
    DynamicCSRGraph<std::string, double> dynGraph(true);
    dynGraph.addEdge("A", "B", 1.5);
    dynGraph.addEdge("A", "C", 2.0);

    StaticCSRGraph<std::string, double> staticGraph(std::move(dynGraph));

    EXPECT_EQ(staticGraph.numVertices(), 3);
    EXPECT_EQ(staticGraph.numEdges(), 2);
    EXPECT_TRUE(staticGraph.hasEdge("A", "B"));
}

TEST(StaticCSRGraphTest, ConstructionFromRawGraph) {
    // Verifies that StaticCSRGraph is correctly constructed from a RawGraph.
    RawGraph<std::string, double> rawGraph(true);
    rawGraph.addEdge("X", "Y", 5.0);

    StaticCSRGraph<std::string, double> staticGraph(std::move(rawGraph));

    EXPECT_EQ(staticGraph.numVertices(), 2);
    EXPECT_EQ(staticGraph.numEdges(), 1);
    EXPECT_TRUE(staticGraph.hasEdge("X", "Y"));
}

TEST(StaticCSRGraphTest, ImmutabilityThrowsExceptions) {
    // Verifies that any attempt to mutate StaticCSRGraph throws a std::logic_error.
    RawGraph<std::string, double> rawGraph(true);
    StaticCSRGraph<std::string, double> staticGraph(std::move(rawGraph));

    EXPECT_THROW(staticGraph.addVertex("A"), std::logic_error);
    EXPECT_THROW(staticGraph.addEdge("A", "B"), std::logic_error);
    EXPECT_THROW(staticGraph.addEdgeDynamic("A", "B"), std::logic_error);
}