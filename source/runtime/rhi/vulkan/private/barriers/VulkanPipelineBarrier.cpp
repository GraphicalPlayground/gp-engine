// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanPipelineBarrier.hpp"
#include "barriers/VulkanAccessTranslation.hpp"   // IWYU pragma: keep
#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "barriers/VulkanBarrierUtils.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "memory/allocators/InlineAllocator.hpp"   // IWYU pragma: keep
#include "sync/SyncEnums.hpp"
#include "translation/VulkanResourceEnums.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

void VulkanPipelineBarrier::addMemoryBarrier(
    const VkAccessFlags srcAccessFlags,
    const VkAccessFlags dstAccessFlags,
    const VkPipelineStageFlags srcStageMask,
    const VkPipelineStageFlags dstStageMask
) noexcept
{
    if (memoryBarriers.isEmpty())
    {
        VkMemoryBarrier2& newBarrier = memoryBarriers.emplaceBack();
        zeroVulkanStruct(newBarrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER_2);
    }

    // Every dependency is folded into a single barrier.
    VkMemoryBarrier2& memoryBarrier = memoryBarriers[0];

    mergeBarrierAccessMask(memoryBarrier, srcAccessFlags, dstAccessFlags);
    memoryBarrier.srcStageMask |= srcStageMask;
    memoryBarrier.dstStageMask |= dstStageMask;
}

void VulkanPipelineBarrier::addFullImageLayoutTransition(
    const VulkanBarrierResource& texture, VkImageLayout srcLayout, VkImageLayout dstLayout
) noexcept
{
    const bool isDepthOrStencil = texture.isDepthOrStencilAspect;

    const VkPipelineStageFlags srcStageMask = getVkStageFlagsForLayout(srcLayout, isDepthOrStencil);
    const VkPipelineStageFlags dstStageMask = getVkStageFlagsForLayout(dstLayout, isDepthOrStencil);

    const VkAccessFlags srcAccessFlags = getVkAccessMaskForLayout(srcLayout, isDepthOrStencil);
    const VkAccessFlags dstAccessFlags = getVkAccessMaskForLayout(dstLayout, isDepthOrStencil);

    const VkImageSubresourceRange subresourceRange = makeSubresourceRange(texture.fullAspectMask);
    if (texture.isDepthOrStencilAspect)
    {
        srcLayout = getMergedDepthStencilLayout(srcLayout, srcLayout);
        dstLayout = getMergedDepthStencilLayout(dstLayout, dstLayout);
    }

    VkImageMemoryBarrier2& imageBarrier = imageBarriers.emplaceBack();
    setupImageBarrier(
        imageBarrier,
        texture.image,
        srcStageMask,
        dstStageMask,
        srcAccessFlags,
        dstAccessFlags,
        srcLayout,
        dstLayout,
        subresourceRange
    );
}

void VulkanPipelineBarrier::addImageLayoutTransition(
    const VkImage image,
    const VkImageLayout srcLayout,
    const VkImageLayout dstLayout,
    const VkImageSubresourceRange& subresourceRange
) noexcept
{
    const bool isDepthOrStencil =
        (subresourceRange.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0u;

    const VkPipelineStageFlags srcStageMask = getVkStageFlagsForLayout(srcLayout, isDepthOrStencil);
    const VkPipelineStageFlags dstStageMask = getVkStageFlagsForLayout(dstLayout, isDepthOrStencil);

    const VkAccessFlags srcAccessFlags = getVkAccessMaskForLayout(srcLayout, isDepthOrStencil);
    const VkAccessFlags dstAccessFlags = getVkAccessMaskForLayout(dstLayout, isDepthOrStencil);

    VkImageMemoryBarrier2& imageBarrier = imageBarriers.emplaceBack();
    setupImageBarrier(
        imageBarrier,
        image,
        srcStageMask,
        dstStageMask,
        srcAccessFlags,
        dstAccessFlags,
        srcLayout,
        dstLayout,
        subresourceRange
    );
}

void VulkanPipelineBarrier::execute(const VkCommandBuffer cmdBuffer) noexcept
{
    if (isEmpty())
    {
        return;
    }

    VkPipelineStageFlags srcStageMask = 0u;
    VkPipelineStageFlags dstStageMask = 0u;

    gp::InlineVector<VkMemoryBarrier, 1u> tempMemoryBarriers{};
    downgradeBarrierArray(tempMemoryBarriers, memoryBarriers, srcStageMask, dstStageMask);

    gp::Vector<VkBufferMemoryBarrier> tempBufferBarriers{};
    downgradeBarrierArray(tempBufferBarriers, bufferBarriers, srcStageMask, dstStageMask);

    gp::InlineVector<VkImageMemoryBarrier, 2u> tempImageBarriers{};
    downgradeBarrierArray(tempImageBarriers, imageBarriers, srcStageMask, dstStageMask);
    mergeDepthStencilLayouts(tempImageBarriers);

    vkCmdPipelineBarrier(
        cmdBuffer,
        srcStageMask,
        dstStageMask,
        0u,
        static_cast<gp::UInt32>(tempMemoryBarriers.size()),
        tempMemoryBarriers.data(),
        static_cast<gp::UInt32>(tempBufferBarriers.size()),
        tempBufferBarriers.data(),
        static_cast<gp::UInt32>(tempImageBarriers.size()),
        tempImageBarriers.data()
    );
}

void VulkanPipelineBarrier::execute(VulkanCommandBuffer& cmdBuffer) noexcept
{
    if (isEmpty())
    {
        return;
    }

    if (cmdBuffer.getDevice().supportsParallelRendering())
    {
        VkDependencyInfo dependencyInfo{};
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependencyInfo.pNext = nullptr;
        dependencyInfo.dependencyFlags = 0u;
        dependencyInfo.memoryBarrierCount = static_cast<gp::UInt32>(memoryBarriers.size());
        dependencyInfo.pMemoryBarriers = memoryBarriers.data();
        dependencyInfo.bufferMemoryBarrierCount = static_cast<gp::UInt32>(bufferBarriers.size());
        dependencyInfo.pBufferMemoryBarriers = bufferBarriers.data();
        dependencyInfo.imageMemoryBarrierCount = static_cast<gp::UInt32>(imageBarriers.size());
        dependencyInfo.pImageMemoryBarriers = imageBarriers.data();
        vkCmdPipelineBarrier2KHR(cmdBuffer.getHandle(), &dependencyInfo);
    }
    else
    {
        // Fall back to the legacy barrier types.
        execute(cmdBuffer.getHandle());
    }
}

VkImageSubresourceRange VulkanPipelineBarrier::makeSubresourceRange(
    const VkImageAspectFlags aspectMask,
    const gp::UInt32 firstMip,
    const gp::UInt32 numMips,
    const gp::UInt32 firstLayer,
    const gp::UInt32 numLayers
) noexcept
{
    VkImageSubresourceRange range{};
    range.aspectMask = aspectMask;
    range.baseMipLevel = firstMip;
    range.levelCount = numMips;
    range.baseArrayLayer = firstLayer;
    range.layerCount = numLayers;
    return range;
}

VkImageLayout VulkanPipelineBarrier::getDefaultLayout(
    const VulkanBarrierResource& texture,
    const sync::ResourceAccess desiredAccess,
    const bool supportsParallelRendering,
    const VulkanShadingRateImageType shadingRateImageType
) noexcept
{
    using sync::ResourceAccess;

    switch (desiredAccess)
    {
    case ResourceAccess::SRVCompute:
    case ResourceAccess::SRVGraphics:
    case ResourceAccess::SRVGraphicsNonPixel:
    case ResourceAccess::SRVGraphicsPixel:
    case ResourceAccess::SRVMask:
    {
        if (texture.isDepthOrStencilAspect)
        {
            if (supportsParallelRendering)
            {
                return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
            }
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
        return texture.supportsSampling ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
    }

    case ResourceAccess::UAVCompute:
    case ResourceAccess::UAVGraphics:
    case ResourceAccess::UAVMask:
        return VK_IMAGE_LAYOUT_GENERAL;

    case ResourceAccess::CopySrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ResourceAccess::CopyDest:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    case ResourceAccess::DSVRead:
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    case ResourceAccess::DSVWrite:
        return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

    case ResourceAccess::ShadingRateSource:
    {
        if (shadingRateImageType == VulkanShadingRateImageType::Palette)
        {
            return VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
        }
        else if (shadingRateImageType == VulkanShadingRateImageType::Fractional)
        {
            return VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT;
        }
        // No shading rate support: handled like any other unexpected state.
        GP_CHECKF(false, "getDefaultLayout() received ShadingRateSource but the device has no shading rate image.");
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    default:
        GP_CHECKF(
            false,
            "getDefaultLayout() received an unsupported access state (%x).",
            static_cast<gp::UInt32>(desiredAccess)
        );
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

VkImageLayout VulkanPipelineBarrier::getDepthOrStencilLayout(const sync::ResourceAccess access) noexcept
{
    return ::gp::rhi::vulkan::getDepthOrStencilLayout(access);
}

void vulkanSetImageLayout(
    VulkanCommandBuffer& cmdBuffer,
    const VkImage image,
    const VkImageLayout oldLayout,
    const VkImageLayout newLayout,
    const VkImageSubresourceRange& subresourceRange
) noexcept
{
    VulkanPipelineBarrier barrier{};
    barrier.addImageLayoutTransition(image, oldLayout, newLayout, subresourceRange);
    barrier.execute(cmdBuffer);
}

}   // namespace gp::rhi::vulkan
