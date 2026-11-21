// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/base/system/FiberContextEnums.hpp"
#include "platforms/base/system/FiberContextTypes.hpp"

namespace gp::platform
{

/// @brief Plain-old-data (POD) configuration descriptor for initializing a platform `FiberContext`
struct FiberContextDesc
{
    /// @brief Mandatory stateless entry function executed when the fiber context is switched into for the first time.
    FiberContextEntryFunction entry{ nullptr };

    /// @brief Opaque user payload pointer forwarded to `entry(userData)`.
    void* userData{ nullptr };

    /// @brief Total virtual stack size in bytes to reserve for the fiber (clamped to at least `kMinFiberStackSize`).
    gp::USize stackReserveSize{ kDefaultFiberStackSize };

    /// @brief Initial committed physical stack bytes (0 commits the entire `stackReserveSize` upfront).
    gp::USize stackCommitSize{ 0u };

    /// @brief Bitmask flags controlling hardware guard pages and floating-point/SIMD state preservation.
    FiberStackFlags flags{ FiberStackFlags::Default };
};

}   // namespace gp::platform
