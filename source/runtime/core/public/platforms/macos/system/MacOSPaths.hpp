// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/apple/system/ApplePaths.hpp"

namespace gp::platform::macos
{

/// @brief macOS implementation of OS directory discovery and lexical path utilities (child of
///        `gp::platform::apple::Paths`, ADR-0043).
class GP_CORE_API Paths : public gp::platform::apple::Paths
{
public:
    Paths() = delete;
};

}   // namespace gp::platform::macos
