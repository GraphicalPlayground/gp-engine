// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief CPU mapping state of a `VulkanBuffer`.
/// @details `Locked` means the caller writes into (or reads from) a staging buffer that `unlock()` must resolve.
///          `PersistentMapping` means the caller got a pointer straight into host-visible buffer memory, so `unlock()`
///          has nothing to do.
enum class VulkanBufferLockStatus : gp::UInt8
{
    Unlocked = 0u,
    Locked,
    PersistentMapping
};

/// @brief Memory accounting bucket a buffer's bytes are charged to.
/// @details A buffer is charged to the first matching bucket, in declaration order.
enum class VulkanBufferStatCategory : gp::UInt8
{
    ReservedUncommitted = 0u,
    Vertex,
    Index,
    AccelerationStructure,
    ByteAddress,
    DrawIndirect,
    Structured,
    Misc,
    Count
};

/// @brief Number of `VulkanBufferStatCategory` buckets.
inline constexpr gp::USize kVulkanBufferStatCategoryCount = static_cast<gp::USize>(VulkanBufferStatCategory::Count);

}   // namespace gp::rhi::vulkan

