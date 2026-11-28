// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "views/VulkanViewForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Kind of native object currently held by a `VulkanView`.
/// @details A view is a tagged union: the kind selects which storage arm is live. `Null` is both the state of a view
///          that was never initialized and the state right after `invalidate()`; `VulkanView::isInitialized()` tells
///          the two apart.
enum class VulkanViewKind : gp::UInt8
{
    /// @brief No native object (null buffer view, or invalidated before a rebuild).
    Null = 0,

    /// @brief `VkBufferView` over a texel buffer (typed buffer SRV/UAV).
    TypedBuffer,

    /// @brief `VkImageView` over an image (texture SRV/UAV).
    Texture,

    /// @brief Raw `(VkBuffer, offset, size)` range bound as a storage buffer (structured/raw SRV/UAV).
    StructuredBuffer,

    /// @brief `VkAccelerationStructureKHR` built over a buffer range (ray tracing scene SRV).
    AccelerationStructure
};

}   // namespace gp::rhi::vulkan
