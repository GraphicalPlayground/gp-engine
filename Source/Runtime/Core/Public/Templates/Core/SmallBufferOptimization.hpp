// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"

namespace GP
{

/// @brief Inline SBO capacity in bytes.
///
/// 32 bytes accommodates the most common binding categories without heap allocation:
///   - Free function pointer            (8  bytes on LP64)
///   - Instance ptr + member fn ptr     (8 + 8  = 16 bytes non-virtual; up to 24 on MSVC
///                                       with virtual/multiple-inheritance layouts)
///   - Lambda capturing a single `this` (8  bytes)
///   - Lambda capturing `this` + one scalar (16 bytes)
static constexpr SizeT kSBOCapacity = 32;

/// @brief Required alignment for the SBO buffer.
///        Matches the natural pointer alignment on all supported platforms.
static constexpr SizeT kSBOAlignment = alignof(MaxAlignT);

}   // namespace GP
