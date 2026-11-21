// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/system/UnixFiberContext.hpp"

namespace gp::platform::apple
{

/// @brief Apple platform execution context alias inheriting the POSIX `x86_64`/`ARM64` register-frame implementation.
using FiberContext = gp::platform::unix::FiberContext;

}   // namespace gp::platform::apple
