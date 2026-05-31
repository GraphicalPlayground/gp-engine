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
template <typename T>
concept IsEqualityComparable = std::equality_comparable<T>;

/// @brief Concept to check if a type can be three-way compared.
template <typename T>
concept IsThreeWayComparable = std::three_way_comparable<T>;

/// @brief Concept to check if a type can be compared with another type for equality.
template <typename T, typename U>
concept IsEqualityComparableWith = std::equality_comparable_with<T, U>;

}   // namespace gp::concepts
