// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "renderpass/VulkanRenderTargetCaps.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/RHIGlobals.hpp"
#include "device/VulkanDevice.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "platform/VulkanPlatform.hpp"

namespace gp::rhi::vulkan
{

bool VulkanRenderTargetCaps::hasValidShadingRateDataType() const noexcept
{
    return (vrsImageDataType == pipeline::VRSImageDataType::Palette) ||
           (vrsImageDataType == pipeline::VRSImageDataType::Fractional);
}

VkImageLayout VulkanRenderTargetCaps::getShadingRateImageLayout() const noexcept
{
    switch (vrsImageDataType)
    {
    case pipeline::VRSImageDataType::Palette:
        return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
    case pipeline::VRSImageDataType::Fractional:
        return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
    case pipeline::VRSImageDataType::NotSupported:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

bool VulkanRenderTargetCaps::usesSeparateDepthStencilLayouts() const noexcept
{
    return supportsParallelRendering && !requiresMergedDepthStencilLayout;
}

VulkanRenderTargetCaps makeVulkanRenderTargetCaps(const VulkanDevice& device) noexcept
{
    const OptionalVulkanDeviceExtensions& extensions = device.getOptionalExtensions();
    const OptionalVulkanDeviceExtensionProperties& properties = device.getOptionalExtensionProperties();
    const VariableRateShadingCapabilities& vrs = getRHIGlobals().variableRateShading;

    VulkanRenderTargetCaps caps{};
    caps.supportsDepthStencilResolve = extensions.hasKHRDepthStencilResolve != 0u;
    caps.supportsAttachmentVariableRateShading = vrs.supportsAttachment;
    caps.supportsFragmentShadingRateAttachment =
        (extensions.hasKHRFragmentShadingRate != 0u) &&
        (properties.fragmentShadingRateFeatures.attachmentFragmentShadingRate == VK_TRUE);
    caps.hasKHRRenderPass2 = extensions.hasKHRRenderPass2 != 0u;
    caps.hasKHRMultiview = extensions.hasKHRMultiview != 0u;
    caps.hasEXTFragmentDensityMap = extensions.hasEXTFragmentDensityMap != 0u;
    caps.hasEXTLoadStoreOpNone = extensions.hasEXTLoadStoreOpNone != 0u;
    caps.hasKHRDynamicRendering = extensions.hasKHRDynamicRendering != 0u;
    caps.supportsParallelRendering = device.supportsParallelRendering();
    caps.requiresDepthStencilFullWrite = VulkanPlatform::requiresDepthStencilFullWrite();
    caps.requiresMergedDepthStencilLayout = VulkanPlatform::requiresMergedDepthStencilLayout();

    // Shading rate attachments win over fragment density maps: they carry an explicit rate per tile.
    if (caps.supportsFragmentShadingRateAttachment)
    {
        caps.vrsImageDataType = pipeline::VRSImageDataType::Palette;
        caps.vrsImageFormat = PixelFormat::R8_UInt;
    }
    else if (
        (extensions.hasEXTFragmentDensityMap != 0u) &&
        (properties.fragmentDensityMapFeatures.fragmentDensityMap == VK_TRUE)
    )
    {
        caps.vrsImageDataType = pipeline::VRSImageDataType::Fractional;
        caps.vrsImageFormat = PixelFormat::R8G8_UNorm;
    }

    // The smallest tile keeps the shading rate image at its highest resolution.
    caps.vrsTileWidth = vrs.imageTileMinWidth;
    caps.vrsTileHeight = vrs.imageTileMinHeight;
    return caps;
}

}   // namespace gp::rhi::vulkan
