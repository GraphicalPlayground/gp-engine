// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "views/VulkanImageViewCreateInfo.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "RHIPixelFormat.hpp"
#include "translation/VulkanPixelFormat.hpp"
#include "views/VulkanTextureViewDesc.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

VulkanImageViewCreateInfo::VulkanImageViewCreateInfo(
    const VulkanDevice& device, const VkImage image, const VulkanTextureViewDesc& viewDesc
) noexcept
{
    zeroVulkanStruct(createInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    createInfo.image = image;
    createInfo.viewType = viewDesc.viewType;
    createInfo.format = viewDesc.format;

    // Each extension struct is pushed at the front of the chain, so the order of the blocks below does not matter.
    if ((device.getOptionalExtensions().hasEXTASTCDecodeMode != 0u) && isASTCLDRFormat(viewDesc.format) &&
        !isASTCSRGBFormat(viewDesc.format))
    {
        zeroVulkanStruct(m_decodeMode, VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT);
        m_decodeMode.decodeMode = VK_FORMAT_R8G8B8A8_UNORM;
        m_decodeMode.pNext = createInfo.pNext;
        createInfo.pNext = &m_decodeMode;
    }

    if (viewDesc.samplerYcbcrConversion != VK_NULL_HANDLE)
    {
        zeroVulkanStruct(m_samplerYcbcrConversionInfo, VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO);
        m_samplerYcbcrConversionInfo.conversion = viewDesc.samplerYcbcrConversion;
        m_samplerYcbcrConversionInfo.pNext = createInfo.pNext;
        createInfo.pNext = &m_samplerYcbcrConversionInfo;
    }

    if (viewDesc.useIdentitySwizzle)
    {
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    }
    else
    {
        createInfo.components = device.getFormatComponentMapping(viewDesc.pixelFormat);
    }

    GP_CHECKF(viewDesc.numMips != 0xFFFFFFFFu, "Texture view mip count must be resolved before view creation.");
    GP_CHECKF(
        (viewDesc.arraySliceIndex != 0xFFFFFFFFu) && (viewDesc.numArraySlices != 0xFFFFFFFFu),
        "Texture view array range must be resolved before view creation."
    );
    createInfo.subresourceRange.aspectMask = viewDesc.aspectFlags;
    createInfo.subresourceRange.baseMipLevel = viewDesc.firstMip;
    createInfo.subresourceRange.levelCount = viewDesc.numMips;
    createInfo.subresourceRange.baseArrayLayer = viewDesc.arraySliceIndex;
    createInfo.subresourceRange.layerCount = viewDesc.numArraySlices;

    // `X24_G8_UInt` is how the renderer asks for the stencil plane of a depth-stencil texture: the view keeps the
    // depth-stencil format but only exposes the stencil aspect.
    if (viewDesc.pixelFormat == PixelFormat::X24_G8_UInt)
    {
        GP_CHECKF(viewDesc.format != VK_FORMAT_UNDEFINED, "Stencil views need a depth-stencil view format.");
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    // Tell the driver the view only serves a subset of the image usage (helps performance and format compatibility).
    if (viewDesc.imageUsageFlags != 0u)
    {
        zeroVulkanStruct(m_imageViewUsageCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);
        m_imageViewUsageCreateInfo.usage = viewDesc.imageUsageFlags;
        m_imageViewUsageCreateInfo.pNext = createInfo.pNext;
        createInfo.pNext = &m_imageViewUsageCreateInfo;
    }
}

}   // namespace gp::rhi::vulkan
