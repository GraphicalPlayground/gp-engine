// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanImageLayout.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"

namespace gp::rhi::vulkan
{

VulkanImageLayout::VulkanImageLayout(
    const VkImageLayout initialLayout,
    const gp::UInt32 inNumMips,
    const gp::UInt32 inNumLayers,
    const VkImageAspectFlags aspect
) noexcept
    : numMips(inNumMips)
    , numLayers(inNumLayers)
    , numPlanes((aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) ? 2u : 1u)
    , mainLayout(initialLayout)
{}

VkImageLayout VulkanImageLayout::getSubresLayout(
    const gp::UInt32 layer, const gp::UInt32 mip, const VkImageAspectFlagBits aspect
) const noexcept
{
    return getSubresLayout(layer, mip, (aspect == VK_IMAGE_ASPECT_STENCIL_BIT) ? numPlanes - 1u : 0u);
}

VkImageLayout
    VulkanImageLayout::getSubresLayout(gp::UInt32 layer, const gp::UInt32 mip, const gp::UInt32 plane) const noexcept
{
    if (subresLayouts.isEmpty())
    {
        return mainLayout;
    }

    if (layer == ~0u)
    {
        layer = 0u;
    }

    GP_CHECK(plane < numPlanes && layer < numLayers && mip < numMips);
    return subresLayouts[(plane * numLayers * numMips) + (layer * numMips) + mip];
}

bool VulkanImageLayout::areSubresourcesSameLayout(
    const VkImageLayout layout, const VkImageSubresourceRange& subresourceRange
) const noexcept
{
    if (subresLayouts.isEmpty())
    {
        return mainLayout == layout;
    }

    const gp::UInt32 firstPlane = getFirstPlane(subresourceRange.aspectMask);
    const gp::UInt32 lastPlane = getLastPlane(subresourceRange.aspectMask);

    const gp::UInt32 firstLayer = subresourceRange.baseArrayLayer;
    const gp::UInt32 layerCount = getSubresRangeLayerCount(subresourceRange);

    const gp::UInt32 firstMip = subresourceRange.baseMipLevel;
    const gp::UInt32 mipCount = getSubresRangeMipCount(subresourceRange);

    for (gp::UInt32 plane = firstPlane; plane < lastPlane; ++plane)
    {
        for (gp::UInt32 layer = firstLayer; layer < firstLayer + layerCount; ++layer)
        {
            for (gp::UInt32 mip = firstMip; mip < firstMip + mipCount; ++mip)
            {
                if (subresLayouts[plane * (numLayers * numMips) + layer * numMips + mip] != layout)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

gp::UInt32 VulkanImageLayout::getSubresRangeLayerCount(const VkImageSubresourceRange& subresourceRange) const noexcept
{
    GP_CHECK(subresourceRange.baseArrayLayer < numLayers);
    return (subresourceRange.layerCount == VK_REMAINING_ARRAY_LAYERS) ? (numLayers - subresourceRange.baseArrayLayer)
                                                                      : subresourceRange.layerCount;
}

gp::UInt32 VulkanImageLayout::getSubresRangeMipCount(const VkImageSubresourceRange& subresourceRange) const noexcept
{
    GP_CHECK(subresourceRange.baseMipLevel < numMips);
    return (subresourceRange.levelCount == VK_REMAINING_MIP_LEVELS) ? (numMips - subresourceRange.baseMipLevel)
                                                                    : subresourceRange.levelCount;
}

void VulkanImageLayout::collapseSubresLayoutsIfSame() noexcept
{
    if (subresLayouts.isEmpty())
    {
        return;
    }

    const VkImageLayout layout = subresLayouts[0];
    const gp::UInt32 subresLayoutCount = numPlanes * numLayers * numMips;
    for (gp::UInt32 index = 1u; index < subresLayoutCount; ++index)
    {
        if (subresLayouts[index] != layout)
        {
            return;
        }
    }

    mainLayout = layout;
    subresLayouts.reset();
}

void VulkanImageLayout::set(const VkImageLayout layout, const VkImageSubresourceRange& subresourceRange) noexcept
{
    GP_CHECK((layout != VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) && (layout != VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL));

    const gp::UInt32 firstPlane = getFirstPlane(subresourceRange.aspectMask);
    const gp::UInt32 lastPlane = getLastPlane(subresourceRange.aspectMask);

    const gp::UInt32 firstLayer = subresourceRange.baseArrayLayer;
    const gp::UInt32 layerCount = getSubresRangeLayerCount(subresourceRange);

    const gp::UInt32 firstMip = subresourceRange.baseMipLevel;
    const gp::UInt32 mipCount = getSubresRangeMipCount(subresourceRange);

    if (firstPlane == 0u && lastPlane == numPlanes && firstLayer == 0u && layerCount == numLayers && firstMip == 0u &&
        mipCount == numMips)
    {
        // The whole image moves to one layout.
        mainLayout = layout;
        subresLayouts.reset();
        return;
    }

    if (subresLayouts.isEmpty())
    {
        const gp::UInt32 subresLayoutCount = numPlanes * numLayers * numMips;
        subresLayouts.resize(subresLayoutCount, mainLayout);
    }

    for (gp::UInt32 plane = firstPlane; plane < lastPlane; ++plane)
    {
        for (gp::UInt32 layer = firstLayer; layer < firstLayer + layerCount; ++layer)
        {
            for (gp::UInt32 mip = firstMip; mip < firstMip + mipCount; ++mip)
            {
                subresLayouts[plane * (numLayers * numMips) + layer * numMips + mip] = layout;
            }
        }
    }

    // The update may have left every subresource in the same layout; fold the table back if so.
    collapseSubresLayoutsIfSame();
}

gp::UInt32 VulkanImageLayout::getFirstPlane(const VkImageAspectFlags aspectMask) const noexcept
{
    return (aspectMask == VK_IMAGE_ASPECT_STENCIL_BIT) ? numPlanes - 1u : 0u;
}

gp::UInt32 VulkanImageLayout::getLastPlane(const VkImageAspectFlags aspectMask) const noexcept
{
    return ((aspectMask & VK_IMAGE_ASPECT_STENCIL_BIT) != 0u) ? numPlanes : 1u;
}

}   // namespace gp::rhi::vulkan
