// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/apple/system/AppleFutex.hpp"

namespace gp::platform::macos
{

/// @brief macOS address-wait implementation (inherits Darwin `__ulock_wait` / `__ulock_wake` from `apple::Futex`).
class GP_CORE_API Futex : public gp::platform::apple::Futex
{
public:
    Futex() = delete;
};

}   // namespace gp::platform::macos
