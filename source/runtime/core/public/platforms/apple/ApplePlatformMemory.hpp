// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/GenericPlatformMemory.hpp"

namespace gp::platform::apple
{

/// @brief Apple-specific implementation of the memory management platform.
/// @note This class is not `final` to allow for platform-specific overrides in derived classes (e.g., macOS).
class Memory : public gp::platform::generic::Memory
{
private:
    /// @brief Deleted constructor prevent instantiation of the Memory class, as it is intended to be used.
    Memory() = delete;

    /// @brief Deleted destructor prevent instantiation of the Memory class, as it is intended to be used.
    ~Memory() = delete;
};

}   // namespace gp::platform::apple
