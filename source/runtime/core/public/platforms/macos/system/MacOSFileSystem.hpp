// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/apple/system/AppleFileSystem.hpp"

namespace gp::platform::macos
{

/// @brief macOS implementation of Layer 0 OS file and directory utilities (ADR-0060).
class GP_CORE_API FileSystem : public gp::platform::apple::FileSystem
{
public:
    FileSystem() = delete;
};

}   // namespace gp::platform::macos
