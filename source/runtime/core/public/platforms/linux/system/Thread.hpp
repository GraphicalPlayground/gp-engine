// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/system/Thread.hpp"

namespace gp::platform::linux
{

/// @brief
class Thread : public gp::platform::unix::Thread
{};

}   // namespace gp::platform::linux
