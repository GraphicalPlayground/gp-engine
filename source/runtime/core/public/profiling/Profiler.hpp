// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

#if GP_USE_TRACY_PROFILER
    #include "profiling/TracyProfiler.hpp"   // IWYU pragma: export
#else
    #include "profiling/NullProfiler.hpp"    // IWYU pragma: export
#endif

/// @brief Helper macro to create a uint32 color from RGB values, packed as 0xRRGGBB.
/// @param r Red component (0-255)
/// @param g Green component (0-255)
/// @param b Blue component (0-255)
/// @return Packed uint32 color in the format 0xRRGGBB
#define GP_U32_COLOR(r, g, b) ((static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b))

#define GP_U32_COLOR_RED       GP_U32_COLOR(220,  50,  50)
#define GP_U32_COLOR_GREEN     GP_U32_COLOR( 50, 200,  50)
#define GP_U32_COLOR_BLUE      GP_U32_COLOR( 50, 130, 220)
#define GP_U32_COLOR_YELLOW    GP_U32_COLOR(230, 210,  30)
#define GP_U32_COLOR_ORANGE    GP_U32_COLOR(230, 130,  30)
#define GP_U32_COLOR_PURPLE    GP_U32_COLOR(160,  50, 220)
#define GP_U32_COLOR_CYAN      GP_U32_COLOR( 30, 200, 210)
#define GP_U32_COLOR_WHITE     GP_U32_COLOR(255, 255, 255)
#define GP_U32_COLOR_GRAY      GP_U32_COLOR(150, 150, 150)
