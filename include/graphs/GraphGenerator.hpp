#ifndef GRAPH_GENERATOR_HPP
#define GRAPH_GENERATOR_HPP

#include <vector>
#include <random>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace GraphGenerator {

template <typename WeightType = double>
struct RawEdge {
    uint32_t u;
    uint32_t v;
    WeightType weight;
};

// Generates a random graph using the Erdős-Rényi model with uniform distribution (allows duplicate edges).
template <typename WeightType = double>
inline std::vector<RawEdge<WeightType>> generateErdosRenyi(
    uint32_t num_vertices, uint64_t num_edges, uint32_t seed = 42,
    WeightType min_weight = static_cast<WeightType>(1.0),
    WeightType max_weight = static_cast<WeightType>(100.0)
) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> vertex_dist(0, num_vertices - 1);
    std::uniform_real_distribution<double> weight_dist(static_cast<double>(min_weight), static_cast<double>(max_weight));

    std::vector<RawEdge<WeightType>> edges;
    edges.reserve(num_edges);

    for (uint64_t i = 0; i < num_edges; ++i) {
        uint32_t u = vertex_dist(rng);
        uint32_t v = vertex_dist(rng);
        while (u == v && num_vertices > 1) { v = vertex_dist(rng); }
        edges.push_back({u, v, static_cast<WeightType>(weight_dist(rng))});
    }
    return edges;
}

// Generates a random graph without duplicate edges (strict) and assigns weights using a Gaussian (normal) distribution.
template <typename WeightType = double>
inline std::vector<RawEdge<WeightType>> generateErdosRenyiStrictGaussian(
    uint32_t num_vertices, uint64_t num_edges, uint32_t seed = 42,
    double mean_weight = 50.0, double stddev_weight = 15.0
) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> vertex_dist(0, num_vertices - 1);
    std::normal_distribution<double> weight_dist(mean_weight, stddev_weight);

    auto hash_pair = [](const std::pair<uint32_t, uint32_t>& p) {
        return std::hash<uint32_t>()(p.first) ^ (std::hash<uint32_t>()(p.second) << 1);
    };
    std::unordered_set<std::pair<uint32_t, uint32_t>, decltype(hash_pair)> seen(0, hash_pair);

    std::vector<RawEdge<WeightType>> edges;
    edges.reserve(num_edges);

    while (edges.size() < num_edges) {
        uint32_t u = vertex_dist(rng);
        uint32_t v = vertex_dist(rng);
        
        if (u == v) continue; 
        if (seen.insert({u, v}).second) { 
            double w = std::max(1.0, weight_dist(rng)); 
            edges.push_back({u, v, static_cast<WeightType>(w)});
        }
    }
    return edges;
}

// Generates a scale-free graph using the R-MAT model, ideal for simulating social networks where a few vertices have many connections.
template <typename WeightType = double>
inline std::vector<RawEdge<WeightType>> generateRMAT(
    uint32_t num_vertices, uint64_t num_edges, double a = 0.57, double b = 0.19, double c = 0.19,
    uint32_t seed = 42, WeightType min_weight = static_cast<WeightType>(1.0), WeightType max_weight = static_cast<WeightType>(100.0)
) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    std::uniform_real_distribution<double> weight_dist(static_cast<double>(min_weight), static_cast<double>(max_weight));

    std::vector<RawEdge<WeightType>> edges;
    edges.reserve(num_edges);
    uint32_t scale = static_cast<uint32_t>(std::ceil(std::log2(num_vertices)));
    
    uint32_t mask = (1 << scale) - 1; 

    for (uint64_t e = 0; e < num_edges; ++e) {
        uint32_t u = 0, v = 0;
        for (uint32_t bit = 0; bit < scale; ++bit) {
            double p = prob_dist(rng);
            uint32_t bit_val = 1 << (scale - 1 - bit);
            if (p > a) {
                if (p < a + b) v += bit_val;
                else if (p < a + b + c) u += bit_val;
                else { u += bit_val; v += bit_val; }
            }
        }
        
        u = u & mask; v = v & mask;
        if (u >= num_vertices) u %= num_vertices;
        if (v >= num_vertices) v %= num_vertices;

        if (u == v && num_vertices > 1) v = (v + 1) % num_vertices;
        edges.push_back({u, v, static_cast<WeightType>(weight_dist(rng))});
    }
    return edges;
}

// Generates a 2D grid (mesh) graph where each vertex is connected to its immediate up, down, left, and right neighbors.
template <typename WeightType = double>
inline std::vector<RawEdge<WeightType>> generateGrid2D(
    uint32_t width, uint32_t height, uint32_t seed = 42,
    WeightType min_weight = static_cast<WeightType>(1.0), WeightType max_weight = static_cast<WeightType>(10.0)
) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> weight_dist(static_cast<double>(min_weight), static_cast<double>(max_weight));
    
    std::vector<RawEdge<WeightType>> edges;
    edges.reserve((width * height) * 4); 

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t u = y * width + x;
            if (x < width - 1) {
                uint32_t v = y * width + (x + 1);
                edges.push_back({u, v, static_cast<WeightType>(weight_dist(rng))});
                edges.push_back({v, u, static_cast<WeightType>(weight_dist(rng))}); 
            }
            if (y < height - 1) {
                uint32_t v = (y + 1) * width + x;
                edges.push_back({u, v, static_cast<WeightType>(weight_dist(rng))});
                edges.push_back({v, u, static_cast<WeightType>(weight_dist(rng))}); 
            }
        }
    }
    return edges;
}

// Generates a single, closed cyclic chain of all vertices in a randomized order to test memory access latency (cache misses).
template <typename WeightType = double>
inline std::vector<RawEdge<WeightType>> generatePointerChasingChain(
    uint32_t num_vertices, uint32_t seed = 42
) {
    std::vector<uint32_t> indices(num_vertices);
    for (uint32_t i = 0; i < num_vertices; ++i) { indices[i] = i; }

    std::mt19937_64 rng(seed);
    std::shuffle(indices.begin(), indices.end(), rng);

    std::vector<RawEdge<WeightType>> edges;
    edges.reserve(num_vertices);

    for (size_t i = 0; i < num_vertices - 1; ++i) {
        edges.push_back({indices[i], indices[i + 1], static_cast<WeightType>(1.0)});
    }
    edges.push_back({indices[num_vertices - 1], indices[0], static_cast<WeightType>(1.0)});

    return edges;
}

} // namespace GraphGenerator
#endif // GRAPH_GENERATOR_HPP