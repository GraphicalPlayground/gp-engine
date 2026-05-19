// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <ranges>

namespace gp::concepts
{

/// @brief Concept to check if a type provides iterators for ranged-based loops.
template <typename T>
concept IsIterable = std::ranges::range<T>;

/// @brief Concept to check if an iterable type guarantees contiguous memory layout.
template <typename T>
concept IsContiguousRange = std::ranges::contiguous_range<T>;

/// @brief Concept to check if an iterable type knows its size in constant time.
template <typename T>
concept IsSizedRange = std::ranges::sized_range<T>;

}   // namespace gp::concepts
