// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::memory
{

/// @brief Default alignment for memory allocations. This value is used when no specific alignment is provided.
/// If the default is specified, the allocator applies to engine rules.
/// Blocks >= 16 bytes will be 16-byte-aligned, Blocks < 16 will be 8-byte aligned. If the allocator does not support
/// allocation alignment, the alignment will be ignored.
static constexpr gp::UInt32 kDefaultAlignment = 0;

/// @brief Minimum alignment for memory allocations. This value is the smallest alignment that can be requested from an
/// allocator.
static constexpr gp::UInt32 kMinAlignment = 8;

}   // namespace gp::memory
