// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"               // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "views/VulkanViewForward.hpp"   // IWYU pragma: keep
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"          // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief `VkImageViewCreateInfo` that owns the storage of its `pNext` chain.
/// @details The chain may hold an ASTC decode mode, a YCbCr conversion and a usage restriction. Those structs live
///          inside this object and `createInfo.pNext` points at them, so the object cannot be copied or moved: a copy
///          would point at the original's storage.
struct GP_RHI_VULKAN_API VulkanImageViewCreateInfo final
{
public:
    /// @brief Builds the create info of a view of `image` described by `viewDesc`.
    /// @details
    ///          - Uncompressed-LDR ASTC views decode to 8 bits per channel when `VK_EXT_astc_decode_mode` is enabled,
    ///            which saves texture cache bandwidth.
    ///          - The swizzle is the identity for storage views and the format's default mapping otherwise.
    ///          - An `X24_G8_UInt` view reads the stencil aspect of a depth-stencil image (stencil visualization).
    ///          - A non-zero `viewDesc.imageUsageFlags` restricts the view usage, which lets drivers create views of
    ///            formats that would be invalid for the full image usage (for example sRGB storage).
    /// @param[in] device Device providing the enabled extensions and format swizzles.
    /// @param[in] image Image the view is created on.
    /// @param[in] viewDesc Backend view description.
    VulkanImageViewCreateInfo(
        const VulkanDevice& device, VkImage image, const VulkanTextureViewDesc& viewDesc
    ) noexcept;

    VulkanImageViewCreateInfo(const VulkanImageViewCreateInfo&) = delete;
    VulkanImageViewCreateInfo& operator=(const VulkanImageViewCreateInfo&) = delete;
    VulkanImageViewCreateInfo(VulkanImageViewCreateInfo&&) = delete;
    VulkanImageViewCreateInfo& operator=(VulkanImageViewCreateInfo&&) = delete;

    /// @brief Create info to pass to `vkCreateImageView()`; its `pNext` chain points into this object.
    VkImageViewCreateInfo createInfo{};

private:
    VkImageViewASTCDecodeModeEXT m_decodeMode{};
    VkSamplerYcbcrConversionInfo m_samplerYcbcrConversionInfo{};
    VkImageViewUsageCreateInfo m_imageViewUsageCreateInfo{};
};

}   // namespace gp::rhi::vulkan
