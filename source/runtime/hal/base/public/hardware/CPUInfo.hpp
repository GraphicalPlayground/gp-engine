// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "hardware/SIMDInfo.hpp"

namespace gp::hal
{

/// @brief Contains detailed information about the CPU, including core counts, cache sizes, and SIMD capabilities.
struct CPUInfo
{
    /// @brief Number of logical cores (including Hyper-Threading).
    UInt32 logicalCoreCount{ 1 };

    /// @brief Number of physical cores (excluding Hyper-Threading).
    UInt32 physicalCoreCount{ 1 };

    /// @brief L1 cache size in bytes.
    UInt32 L1CacheSize{ 0 };

    /// @brief L2 cache size in bytes.
    UInt32 L2CacheSize{ 0 };

    /// @brief L3 cache size in bytes.
    UInt32 L3CacheSize{ 0 };

    /// @brief CPU brand/model name as a null-terminated string.
    char brand[64]{ 0 };

    /// @brief SIMD capabilities of the CPU.
    SIMDInfo simd;
};

}   // namespace gp::hal
