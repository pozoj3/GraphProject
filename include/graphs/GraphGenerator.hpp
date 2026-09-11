#ifndef GRAPH_GENERATOR_HPP
#define GRAPH_GENERATOR_HPP

#include <vector>
#include <random>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace GraphGenerator {

// Struktura sirovog brida za punjenje u grafove
template <typename WeightType = double>
struct RawEdge {
    uint32_t u;
    uint32_t v;
    WeightType weight;
};

/**
 * 1. Erdős-Rényi Generator (Uniformna slučajna razdioba)
 * Pogodno za općenite benchmarke obilaska i umetanja.
 */
template <typename WeightType = double>
inline std::vector<RawEdge<WeightType>> generateErdosRenyi(
    uint32_t num_vertices, 
    uint64_t num_edges, 
    uint32_t seed = 42,
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
        
        // Izbjegavanje petlji na samog sebe
        while (u == v && num_vertices > 1) {
            v = vertex_dist(rng);
        }

        edges.push_back({u, v, static_cast<WeightType>(weight_dist(rng))});
    }

    return edges;
}

/**
 * 2. R-MAT Generator (Scale-Free / Power-Law grafovi)
 * Oponaša stvarne grafove (LiveJournal, Orkut) gdje manji broj čvorova
 * ima ogroman broj bridova (high-degree čvorovi).
 * Standardni Graph500 parametri: a=0.57, b=0.19, c=0.19, d=0.05.
 */
template <typename WeightType = double>
inline std::vector<RawEdge<WeightType>> generateRMAT(
    uint32_t num_vertices,
    uint64_t num_edges,
    double a = 0.57,
    double b = 0.19,
    double c = 0.19,
    uint32_t seed = 42,
    WeightType min_weight = static_cast<WeightType>(1.0),
    WeightType max_weight = static_cast<WeightType>(100.0)
) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    std::uniform_real_distribution<double> weight_dist(static_cast<double>(min_weight), static_cast<double>(max_weight));

    std::vector<RawEdge<WeightType>> edges;
    edges.reserve(num_edges);

    uint32_t scale = static_cast<uint32_t>(std::ceil(std::log2(num_vertices)));

    for (uint64_t e = 0; e < num_edges; ++e) {
        uint32_t u = 0;
        uint32_t v = 0;

        for (uint32_t bit = 0; bit < scale; ++bit) {
            double p = prob_dist(rng);
            uint32_t bit_val = 1 << (scale - 1 - bit);

            if (p < a) {
                // Kvadrant Top-Left: u += 0, v += 0
            } else if (p < a + b) {
                // Kvadrant Top-Right: u += 0, v += 1
                v += bit_val;
            } else if (p < a + b + c) {
                // Kvadrant Bottom-Left: u += 1, v += 0
                u += bit_val;
            } else {
                // Kvadrant Bottom-Right: u += 1, v += 1
                u += bit_val;
                v += bit_val;
            }
        }

        u = u % num_vertices;
        v = v % num_vertices;

        if (u == v && num_vertices > 1) {
            v = (v + 1) % num_vertices;
        }

        edges.push_back({u, v, static_cast<WeightType>(weight_dist(rng))});
    }

    return edges;
}

/**
 * 3. Cyclic Chain Generator (Pointer-Chasing simulacija)
 * Povezuje čvorove u jedan nasumično ispremješani zatvoreni lanac.
 * Korisno za izravno testiranje latencije predmemorije (L1/L2/L3 skokovi).
 */
template <typename WeightType = double>
inline std::vector<RawEdge<WeightType>> generatePointerChasingChain(
    uint32_t num_vertices,
    uint32_t seed = 42
) {
    std::vector<uint32_t> indices(num_vertices);
    for (uint32_t i = 0; i < num_vertices; ++i) {
        indices[i] = i;
    }

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

// Globalni defaultni wrapper za benchmarke
template <typename WeightType = double>
inline std::vector<RawEdge<WeightType>> generateRandomGraph(
    uint32_t num_vertices, 
    uint64_t num_edges, 
    uint32_t seed = 42
) {
    return generateErdosRenyi<WeightType>(num_vertices, num_edges, seed);
}

} // namespace GraphGenerator

// Globalni alias radi kompatibilnosti s bench_main.cpp
using GraphGenerator::generateRandomGraph;
using GraphGenerator::generateRMAT;
using GraphGenerator::generateErdosRenyi;
using GraphGenerator::generatePointerChasingChain;

#endif // GRAPH_GENERATOR_HPP