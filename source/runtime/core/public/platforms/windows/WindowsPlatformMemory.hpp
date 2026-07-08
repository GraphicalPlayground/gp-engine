// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/GenericPlatformMemory.hpp"

namespace gp::platform::windows
{

/// @brief Windows-specific implementation of the memory management platform.
class Memory final : public gp::platform::generic::Memory
{
private:
    /// @brief Deleted constructor prevent instantiation of the Memory class, as it is intended to be used.
    Memory() = delete;

    /// @brief Deleted destructor prevent instantiation of the Memory class, as it is intended to be used.
    ~Memory() = delete;

public:
    /// @brief Get the platform-specific memory constants, such as page size, and other relevant parameters.
    /// @return The platform-specific memory constants.
    static GP_CORE_API memory::PlatformConstants getPlatformConstants();
};

}   // namespace gp::platform::windows
