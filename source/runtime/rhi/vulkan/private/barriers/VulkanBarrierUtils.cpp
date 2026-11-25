// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanBarrierUtils.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

void setupImageBarrier(
    VkImageMemoryBarrier2& outBarrier,
    const VkImage image,
    const VkPipelineStageFlags srcStageFlags,
    const VkPipelineStageFlags dstStageFlags,
    const VkAccessFlags srcAccessFlags,
    const VkAccessFlags dstAccessFlags,
    const VkImageLayout srcLayout,
    const VkImageLayout dstLayout,
    const VkImageSubresourceRange& subresourceRange
) noexcept
{
    outBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    outBarrier.pNext = nullptr;
    outBarrier.srcStageMask = srcStageFlags;
    outBarrier.dstStageMask = dstStageFlags;
    outBarrier.srcAccessMask = srcAccessFlags;
    outBarrier.dstAccessMask = dstAccessFlags;
    outBarrier.oldLayout = srcLayout;
    outBarrier.newLayout = dstLayout;
    outBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outBarrier.image = image;
    outBarrier.subresourceRange = subresourceRange;
}

void setupImageBarrierEntireRes(
    VkImageMemoryBarrier2& outBarrier,
    const VkImage image,
    const VkPipelineStageFlags srcStageFlags,
    const VkPipelineStageFlags dstStageFlags,
    const VkAccessFlags srcAccessFlags,
    const VkAccessFlags dstAccessFlags,
    const VkImageLayout srcLayout,
    const VkImageLayout dstLayout,
    const VkImageAspectFlags aspectMask
) noexcept
{
    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = aspectMask;
    subresourceRange.baseMipLevel = 0u;
    subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    subresourceRange.baseArrayLayer = 0u;
    subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    setupImageBarrier(
        outBarrier,
        image,
        srcStageFlags,
        dstStageFlags,
        srcAccessFlags,
        dstAccessFlags,
        srcLayout,
        dstLayout,
        subresourceRange
    );
}

void downgradeBarrier(VkMemoryBarrier& outBarrier, const VkMemoryBarrier2& inBarrier) noexcept
{
    outBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    outBarrier.pNext = inBarrier.pNext;
    outBarrier.srcAccessMask = static_cast<VkAccessFlags>(inBarrier.srcAccessMask);
    outBarrier.dstAccessMask = static_cast<VkAccessFlags>(inBarrier.dstAccessMask);
}

void downgradeBarrier(VkBufferMemoryBarrier& outBarrier, const VkBufferMemoryBarrier2& inBarrier) noexcept
{
    outBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    outBarrier.pNext = inBarrier.pNext;
    outBarrier.srcAccessMask = static_cast<VkAccessFlags>(inBarrier.srcAccessMask);
    outBarrier.dstAccessMask = static_cast<VkAccessFlags>(inBarrier.dstAccessMask);
    outBarrier.srcQueueFamilyIndex = inBarrier.srcQueueFamilyIndex;
    outBarrier.dstQueueFamilyIndex = inBarrier.dstQueueFamilyIndex;
    outBarrier.buffer = inBarrier.buffer;
    outBarrier.offset = inBarrier.offset;
    outBarrier.size = inBarrier.size;
}

void downgradeBarrier(VkImageMemoryBarrier& outBarrier, const VkImageMemoryBarrier2& inBarrier) noexcept
{
    outBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    outBarrier.pNext = inBarrier.pNext;
    outBarrier.srcAccessMask = static_cast<VkAccessFlags>(inBarrier.srcAccessMask);
    outBarrier.dstAccessMask = static_cast<VkAccessFlags>(inBarrier.dstAccessMask);
    outBarrier.oldLayout = inBarrier.oldLayout;
    outBarrier.newLayout = inBarrier.newLayout;
    outBarrier.srcQueueFamilyIndex = inBarrier.srcQueueFamilyIndex;
    outBarrier.dstQueueFamilyIndex = inBarrier.dstQueueFamilyIndex;
    outBarrier.image = inBarrier.image;
    outBarrier.subresourceRange = inBarrier.subresourceRange;
}

}   // namespace gp::rhi::vulkan
