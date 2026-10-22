// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/apple/system/AppleThread.hpp"

namespace gp::platform::macos
{

/// @brief
class Thread : public gp::platform::apple::Thread
{};

}   // namespace gp::platform::macos
