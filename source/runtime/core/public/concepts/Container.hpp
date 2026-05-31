// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <ranges>

namespace gp::concepts
{

/// @brief Concept to check if a type provides iterators for ranged-based loops.
/// @note In the GP engine, most of our custom containers will support this concept
/// to enable the clean 'for (auto& item : container)' syntax.
template <typename T>
concept IsIterable = std::ranges::range<T>;

/// @brief Concept to check if an iterable type guarantees contiguous memory layout.
/// @note Pedagogical Note: Contiguous memory is the "Holy Grail" of performance. It ensures
/// high cache locality and allows the CPU's hardware prefetcher to load data before it's needed,
/// which is critical for hot loops in renderers and physics systems.
template <typename T>
concept IsContiguousRange = std::ranges::contiguous_range<T>;

/// @brief Concept to check if an iterable type knows its size in constant time.
/// @note Knowing the size in O(1) allows for optimizations like pre-allocating memory
/// (reserving) before copying data.
template <typename T>
concept IsSizedRange = std::ranges::sized_range<T>;

}   // namespace gp::concepts
