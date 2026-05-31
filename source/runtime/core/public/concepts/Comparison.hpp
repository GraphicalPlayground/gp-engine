// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <compare>
#include <concepts>

namespace gp::concepts
{

/// @brief Concept to check if a type can be equality compared.
/// @note Equality comparison is used everywhere, from hash table lookups to state verification
/// in our animation systems.
template <typename T>
concept IsEqualityComparable = std::equality_comparable<T>;

/// @brief Concept to check if a type can be three-way compared.
/// @note Pedagogical Note: The C++20 three-way comparison operator (<=>) allows us to define all
/// six relational operators (<, <=, >, >=, ==, !=) in one go, simplifying our code and making
/// sorting operations (like Z-sorting in a 2D renderer) much easier to maintain.
template <typename T>
concept IsThreeWayComparable = std::three_way_comparable<T>;

/// @brief Concept to check if a type can be compared with another type for equality.
template <typename T, typename U>
concept IsEqualityComparableWith = std::equality_comparable_with<T, U>;

}   // namespace gp::concepts
