// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/apple/ApplePlatformMemory.hpp"

namespace gp::platform::macos
{

/// @brief macOS-specific implementation of the memory management platform.
class Memory final : public gp::platform::apple::Memory
{
private:
    /// @brief Deleted constructor prevent instantiation of the Memory class, as it is intended to be used.
    Memory() = delete;

    /// @brief Deleted destructor prevent instantiation of the Memory class, as it is intended to be used.
    ~Memory() = delete;
};

}   // namespace gp::platform::macos
