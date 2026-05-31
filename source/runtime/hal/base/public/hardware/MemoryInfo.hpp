// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief Contains detailed information about the system's memory.
struct MemoryInfo
{
    /// @brief Total physical memory in bytes.
    UInt64 totalPhysicalMemory{ 0 };

    /// @brief Available physical memory in bytes.
    UInt64 availablePhysicalMemory{ 0 };

    /// @brief Memory page size in bytes.
    UInt32 pageSize{ 0 };
};

}   // namespace gp::hal
