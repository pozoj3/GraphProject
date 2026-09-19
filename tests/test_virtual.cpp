#include <gtest/gtest.h>
#include <string>
#include <memory>

#include "vgraphs/VirtualRawGraph.hpp"
#include "vgraphs/VirtualCSRGraph.hpp"

TEST(VirtualGraphTest, PolymorphicBehaviorRaw) {
    // VirtualRawGraph can be used polymorphic base pointer
    std::unique_ptr<VirtualBaseGraph<std::string, double>> graph = 
        std::make_unique<VirtualRawGraph<std::string, double>>(true);

    graph->addEdge("Zg", "St", 400.0);
    graph->addEdge("Zg", "Ri", 160.0);

    EXPECT_TRUE(graph->hasEdge("Zg", "St"));
    EXPECT_EQ(graph->numVertices(), 3);
    
    size_t count = 0;
    graph->forEachNeighbor("Zg", [&](const std::string&, double) {
        count++;
    });
    EXPECT_EQ(count, 2);
}

TEST(VirtualGraphTest, PolymorphicBehaviorCSR) {
    // verifies that VirtualCSRGraph is correctly built from VirtualRawGraph
    VirtualRawGraph<std::string, double> raw(true);
    raw.addEdge("A", "B", 10.0);
    raw.addEdge("A", "C", 20.0);

    std::unique_ptr<VirtualBaseGraph<std::string, double>> graph = 
        std::make_unique<VirtualCSRGraph<std::string, double>>(std::move(raw));

    EXPECT_TRUE(graph->hasEdge("A", "B"));
    EXPECT_TRUE(graph->hasEdge("A", "C"));
    EXPECT_EQ(graph->numVertices(), 3);
    EXPECT_EQ(graph->numEdges(), 2);
}