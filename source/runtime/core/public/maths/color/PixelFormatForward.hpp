// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::math
{

/// @brief Canonical engine pixel format describing encoded memory layout and block dimensions.
enum class PixelFormat : gp::UInt8;

/// @brief Bitmask flags describing the active color, depth, or stencil channels of a `PixelFormat`.
enum class PixelFormatChannelFlags : gp::UInt8;

/// @brief Numeric encoding kind of individual channels within a `PixelFormat`.
enum class PixelFormatComponentType : gp::UInt8;

/// @brief Block-compression family used by a `PixelFormat`.
enum class PixelFormatCompression : gp::UInt8;

struct PixelFormatInfo;

}   // namespace gp::math
