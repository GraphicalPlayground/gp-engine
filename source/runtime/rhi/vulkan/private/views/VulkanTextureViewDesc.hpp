// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "RHIPixelFormat.hpp"
#include "RHIVulkan.hpp"
#include "views/VulkanViewForward.hpp"   // IWYU pragma: keep
#include "VulkanThirdParty.hpp"          // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Backend description of a `VkImageView` derived from an RHI texture view.
/// @details Sits between the RHI `ViewDesc::Texture*::ViewInfo` and `VkImageViewCreateInfo`: it keeps the engine
///          format next to the native one (the swizzle and the stencil-visualization path need it) and the usage
///          subset the view is restricted to. Texture-owned internal views are keyed by `getHash()`.
struct GP_RHI_VULKAN_API VulkanTextureViewDesc final
{
    /// @brief Native view type (2D, 2D array, cube, ...).
    VkImageViewType viewType{ VK_IMAGE_VIEW_TYPE_MAX_ENUM };

    /// @brief Image aspects the view covers.
    VkImageAspectFlags aspectFlags{ VK_IMAGE_ASPECT_NONE };

    /// @brief Engine format of the view; drives the component swizzle.
    PixelFormat pixelFormat{ PixelFormat::Unknown };

    /// @brief Native format of the view.
    VkFormat format{ VK_FORMAT_UNDEFINED };

    /// @brief First mip level.
    gp::UInt32 firstMip{ 0u };

    /// @brief Number of mip levels.
    gp::UInt32 numMips{ VK_REMAINING_MIP_LEVELS };

    /// @brief First array layer (cube faces count as layers).
    gp::UInt32 arraySliceIndex{ 0u };

    /// @brief Number of array layers (cube faces count as layers).
    gp::UInt32 numArraySlices{ VK_REMAINING_ARRAY_LAYERS };

    /// @brief Usage subset the view is restricted to, or `0` to inherit the image usage.
    VkImageUsageFlags imageUsageFlags{ 0u };

    /// @brief YCbCr conversion to chain on the view, or `VK_NULL_HANDLE`.
    VkSamplerYcbcrConversion samplerYcbcrConversion{ VK_NULL_HANDLE };

    /// @brief Forces the identity swizzle (required for storage images).
    bool useIdentitySwizzle{ false };

    /// @brief Computes a hash over every field.
    /// @return 64-bit hash; equal descriptions hash equally.
    [[nodiscard]] gp::UInt64 getHash() const noexcept;

    /// @brief Compares every field.
    /// @param[in] other Description to compare against.
    /// @return True when both descriptions create the same native view.
    [[nodiscard]] bool operator==(const VulkanTextureViewDesc& other) const noexcept = default;
};

}   // namespace gp::rhi::vulkan
