// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/apple/system/AppleFiberContext.hpp"

namespace gp::platform::macos
{

/// @brief macOS platform execution context alias inheriting the Apple/POSIX `x86_64`/`ARM64` register-frame
///        implementation.
using FiberContext = gp::platform::apple::FiberContext;

}   // namespace gp::platform::macos
