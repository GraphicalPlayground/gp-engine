// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "views/VulkanViewForward.hpp"   // IWYU pragma: keep
#include "VulkanThirdParty.hpp"          // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief State of a `VulkanView` that holds no native object.
struct VulkanInvalidatedViewState final
{
    /// @brief True when the view held a native object before it was invalidated.
    /// @details A descriptor that was already written once may be in use by in-flight work, so a rebuild must update
    ///          it through the pipeline contexts instead of writing it in place.
    bool wasInitialized{ false };
};

/// @brief Native state of a typed (texel) buffer view.
/// @details The creation parameters are kept next to the handle so the `VkBufferView` can be (re)created lazily.
struct VulkanTypedBufferView final
{
    /// @brief Native texel buffer view, or `VK_NULL_HANDLE` until it is created.
    VkBufferView view{ VK_NULL_HANDLE };

    /// @brief Buffer the view reads from (the pool buffer for sub-allocated buffers).
    VkBuffer buffer{ VK_NULL_HANDLE };

    /// @brief Texel format of the view.
    VkFormat format{ VK_FORMAT_UNDEFINED };

    /// @brief Byte offset inside `buffer`, allocation offset included.
    gp::UInt32 offset{ 0u };

    /// @brief Byte range covered by the view.
    gp::UInt32 range{ 0u };

    /// @brief Unique id used to key descriptor set caches; `0` excludes the view (volatile source buffers).
    gp::UInt32 viewId{ 0u };
};

/// @brief Native state of a structured or raw buffer view.
/// @details Storage buffers need no Vulkan view object: the descriptor is written from the buffer range directly.
struct VulkanStructuredBufferView final
{
    /// @brief Buffer the view reads from (the pool buffer for sub-allocated buffers).
    VkBuffer buffer{ VK_NULL_HANDLE };

    /// @brief Id of the allocation backing `buffer`; changes whenever the buffer is renamed.
    gp::UInt32 handleId{ 0u };

    /// @brief Byte offset inside `buffer`, allocation offset included.
    gp::UInt32 offset{ 0u };

    /// @brief Byte size covered by the view.
    gp::UInt32 size{ 0u };
};

/// @brief Native state of an acceleration structure view.
struct VulkanAccelerationStructureView final
{
    /// @brief Top-level acceleration structure created over the buffer range.
    VkAccelerationStructureKHR handle{ VK_NULL_HANDLE };
};

/// @brief Native state of a texture view.
struct VulkanTextureView final
{
    /// @brief Native image view.
    VkImageView view{ VK_NULL_HANDLE };

    /// @brief Image the view was created on.
    VkImage image{ VK_NULL_HANDLE };

    /// @brief Unique id used to key descriptor set caches.
    gp::UInt32 viewId{ 0u };
};

}   // namespace gp::rhi::vulkan
