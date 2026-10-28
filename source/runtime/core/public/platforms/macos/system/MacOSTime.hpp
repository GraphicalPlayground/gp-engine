// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/apple/system/AppleTime.hpp"

namespace gp::platform::macos
{

/// @brief macOS hardware timekeeping implementation (child of `gp::platform::apple::Time`).
/// @details Inherits Mach `CLOCK_UPTIME_RAW`, `mach_wait_until`, and POSIX calendar primitives from `apple::Time`.
class GP_CORE_API Time : public gp::platform::apple::Time
{
public:
    Time() = delete;
};

}   // namespace gp::platform::macos
