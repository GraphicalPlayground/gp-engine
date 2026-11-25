// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::rhi::vulkan
{

/// @brief Image-based variable rate shading encoding reported by the device.
/// @details Decides which Vulkan extension consumes a `sync::ResourceAccess::ShadingRateSource` texture:
///          `VK_KHR_fragment_shading_rate` reads a palette of discrete rates, `VK_EXT_fragment_density_map` reads
///          fractional densities.
enum class VulkanShadingRateImageType : gp::UInt8
{
    /// @brief Attachment-based variable rate shading is unavailable.
    NotSupported = 0,

    /// @brief Each texel stores one discrete shading rate (`VK_KHR_fragment_shading_rate`).
    Palette,

    /// @brief Each texel stores a fractional fragment density (`VK_EXT_fragment_density_map`).
    Fractional
};

/// @brief Concrete Vulkan object kind a `sync::TransitionInfo` resolves to before barrier generation.
enum class VulkanBarrierResourceType : gp::UInt8
{
    /// @brief The transition references no resource; it is skipped.
    None = 0,

    /// @brief A `VkImage` that may need a layout transition.
    Texture,

    /// @brief A CPU readback "texture" that is really backed by a buffer, so it never needs an image barrier.
    ReadbackTexture,

    /// @brief A `VkBuffer` range.
    Buffer,

    /// @brief A ray tracing acceleration structure, synchronized through a global memory barrier.
    BVH
};

}   // namespace gp::rhi::vulkan
