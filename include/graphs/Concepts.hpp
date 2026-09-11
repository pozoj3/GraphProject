#ifndef CONCEPTS_HPP
#define CONCEPTS_HPP

#include <concepts>

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

#endif // CONCEPTS_HPP