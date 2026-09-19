#ifndef CONCEPTS_HPP
#define CONCEPTS_HPP

// static_assert(GraphReq<...>), numeric constrains the weight type.

#include <concepts>
#include <cstddef>

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <typename T, typename VertexType, typename WeightType>
concept GraphReq = Numeric<WeightType> && requires(T obj, const T cobj, const VertexType& u, const VertexType& v, const WeightType& w){
    obj.addVertex(u);
    obj.addEdge(u, v, w);
    obj.addEdgeDynamic(u, v, w);
    { cobj.hasVertex(u) } -> std::same_as<bool>;
    { cobj.hasEdge(u, v) } -> std::same_as<bool>;
    { cobj.hasEdge(u, v, w) } -> std::same_as<bool>;
    { cobj.getDegree(u) } -> std::same_as<std::size_t>;
    cobj.forEachNeighbor(u, [](const VertexType&, const WeightType&) {});
    cobj.traverseEntireGraph([](const VertexType&, const VertexType&, const WeightType&) {});
    { cobj.numVertices() } -> std::same_as<std::size_t>;
    { cobj.numEdges() } -> std::same_as<std::size_t>;
};

#endif