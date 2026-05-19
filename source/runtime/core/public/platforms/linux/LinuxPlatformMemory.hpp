// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/unix/UnixPlatformMemory.hpp"

namespace gp::platform::linux
{

/// @brief Linux-specific implementation of the memory management platform.
class Memory final : public gp::platform::unix::Memory
{
private:
    /// @brief Deleted constructor prevent instantiation of the Memory class, as it is intended to be used.
    Memory() = delete;

    /// @brief Deleted destructor prevent instantiation of the Memory class, as it is intended to be used.
    ~Memory() = delete;
};

}   // namespace gp::platform::linux
