// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::memory
{

/// @brief Hardware properties of the target CPU that are known at compile time.
/// @note The values come from the per-platform macros in `platforms/<platform>/<Platform>Platform.hpp`. Use these
///       typed constants in C++ code and keep the macros for the preprocessor.
struct HardwareConstants final
{
    /// @brief Size in bytes of an L1 data cache line.
    /// @details Two variables written by different threads should sit at least this far apart, otherwise every write
    ///          invalidates the other core's copy of the line (false sharing). It is 128 on Apple Silicon and 64 on
    ///          the other supported targets.
    static constexpr gp::USize cacheLineSize = GP_PLATFORM_CACHE_LINE_SIZE;
};

}   // namespace gp::memory
