// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanAccessTranslation.hpp"   // IWYU pragma: keep
#include "barriers/VulkanBarrierTypes.hpp"
#include "CoreMinimal.hpp"                        // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"                   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Fills every field of a `synchronization2` image barrier with queue ownership left untouched.
/// @param[out] outBarrier Barrier to fill.
/// @param[in] image Target image.
/// @param[in] srcStageFlags Source stage mask.
/// @param[in] dstStageFlags Destination stage mask.
/// @param[in] srcAccessFlags Source access mask.
/// @param[in] dstAccessFlags Destination access mask.
/// @param[in] srcLayout Layout before the barrier.
/// @param[in] dstLayout Layout after the barrier.
/// @param[in] subresourceRange Subresources covered by the barrier.
GP_RHI_VULKAN_API void setupImageBarrier(
    VkImageMemoryBarrier2& outBarrier,
    VkImage image,
    VkPipelineStageFlags srcStageFlags,
    VkPipelineStageFlags dstStageFlags,
    VkAccessFlags srcAccessFlags,
    VkAccessFlags dstAccessFlags,
    VkImageLayout srcLayout,
    VkImageLayout dstLayout,
    const VkImageSubresourceRange& subresourceRange
) noexcept;

/// @brief Fills a `synchronization2` image barrier that covers every mip and layer of `aspectMask`.
/// @param[out] outBarrier Barrier to fill.
/// @param[in] image Target image.
/// @param[in] srcStageFlags Source stage mask.
/// @param[in] dstStageFlags Destination stage mask.
/// @param[in] srcAccessFlags Source access mask.
/// @param[in] dstAccessFlags Destination access mask.
/// @param[in] srcLayout Layout before the barrier.
/// @param[in] dstLayout Layout after the barrier.
/// @param[in] aspectMask Aspects covered by the barrier.
GP_RHI_VULKAN_API void setupImageBarrierEntireRes(
    VkImageMemoryBarrier2& outBarrier,
    VkImage image,
    VkPipelineStageFlags srcStageFlags,
    VkPipelineStageFlags dstStageFlags,
    VkAccessFlags srcAccessFlags,
    VkAccessFlags dstAccessFlags,
    VkImageLayout srcLayout,
    VkImageLayout dstLayout,
    VkImageAspectFlags aspectMask
) noexcept;

/// @brief Adds access masks to a merged global memory barrier, skipping them when the source only read memory.
/// @details A read-only source has nothing to make available, so an execution dependency carried by the stage masks
///          is enough. Only write sources contribute their access masks.
/// @tparam InMemoryBarrierType `VkMemoryBarrier` or `VkMemoryBarrier2`.
/// @param[in,out] memoryBarrier Barrier accumulating every merged transition.
/// @param[in] srcAccessFlags Source access mask of the merged transition.
/// @param[in] dstAccessFlags Destination access mask of the merged transition.
template <typename InMemoryBarrierType>
void mergeBarrierAccessMask(
    InMemoryBarrierType& memoryBarrier, const VkAccessFlags srcAccessFlags, const VkAccessFlags dstAccessFlags
) noexcept
{
    const bool srcAccessIsRead = ((srcAccessFlags & (~kVulkanReadAccessMask)) == 0u);
    if (!srcAccessIsRead)
    {
        memoryBarrier.srcAccessMask |= srcAccessFlags;
        memoryBarrier.dstAccessMask |= dstAccessFlags;
    }
}

/// @brief Converts a `synchronization2` memory barrier into its legacy form (stage masks are dropped).
/// @param[out] outBarrier Legacy barrier to fill.
/// @param[in] inBarrier Source barrier.
GP_RHI_VULKAN_API void downgradeBarrier(VkMemoryBarrier& outBarrier, const VkMemoryBarrier2& inBarrier) noexcept;

/// @brief Converts a `synchronization2` buffer barrier into its legacy form (stage masks are dropped).
/// @param[out] outBarrier Legacy barrier to fill.
/// @param[in] inBarrier Source barrier.
GP_RHI_VULKAN_API void
    downgradeBarrier(VkBufferMemoryBarrier& outBarrier, const VkBufferMemoryBarrier2& inBarrier) noexcept;

/// @brief Converts a `synchronization2` image barrier into its legacy form (stage masks are dropped).
/// @param[out] outBarrier Legacy barrier to fill.
/// @param[in] inBarrier Source barrier.
GP_RHI_VULKAN_API void
    downgradeBarrier(VkImageMemoryBarrier& outBarrier, const VkImageMemoryBarrier2& inBarrier) noexcept;

/// @brief Appends the legacy form of `srcBarrier` to `targetArray` and folds its stage masks into the merged masks.
/// @tparam InDstArrayType Vector of legacy barriers.
/// @tparam InBarrierType `synchronization2` barrier type.
/// @param[in,out] targetArray Legacy barrier array receiving the converted barrier.
/// @param[in] srcBarrier Barrier to convert.
/// @param[in,out] mergedSrcStageMask Accumulated source stage mask for `vkCmdPipelineBarrier`.
/// @param[in,out] mergedDstStageMask Accumulated destination stage mask for `vkCmdPipelineBarrier`.
template <typename InDstArrayType, typename InBarrierType>
void downgradeBarrier(
    InDstArrayType& targetArray,
    const InBarrierType& srcBarrier,
    VkPipelineStageFlags& mergedSrcStageMask,
    VkPipelineStageFlags& mergedDstStageMask
) noexcept
{
    auto& dstBarrier = targetArray.emplaceBack();
    downgradeBarrier(dstBarrier, srcBarrier);
    mergedSrcStageMask |= static_cast<VkPipelineStageFlags>(srcBarrier.srcStageMask);
    mergedDstStageMask |= static_cast<VkPipelineStageFlags>(srcBarrier.dstStageMask);
}

/// @brief Appends the legacy form of every barrier in `srcArray` to `targetArray`, merging their stage masks.
/// @tparam InDstArrayType Vector of legacy barriers.
/// @tparam InSrcArrayType Vector of `synchronization2` barriers.
/// @param[in,out] targetArray Legacy barrier array receiving the converted barriers.
/// @param[in] srcArray Barriers to convert.
/// @param[in,out] mergedSrcStageMask Accumulated source stage mask for `vkCmdPipelineBarrier`.
/// @param[in,out] mergedDstStageMask Accumulated destination stage mask for `vkCmdPipelineBarrier`.
template <typename InDstArrayType, typename InSrcArrayType>
void downgradeBarrierArray(
    InDstArrayType& targetArray,
    const InSrcArrayType& srcArray,
    VkPipelineStageFlags& mergedSrcStageMask,
    VkPipelineStageFlags& mergedDstStageMask
) noexcept
{
    targetArray.reserve(targetArray.size() + srcArray.size());
    for (const auto& srcBarrier: srcArray)
    {
        auto& dstBarrier = targetArray.emplaceBack();
        downgradeBarrier(dstBarrier, srcBarrier);
        mergedSrcStageMask |= static_cast<VkPipelineStageFlags>(srcBarrier.srcStageMask);
        mergedDstStageMask |= static_cast<VkPipelineStageFlags>(srcBarrier.dstStageMask);
    }
}

/// @brief Replaces per-plane `synchronization2` layouts on depth/stencil barriers with combined legacy layouts.
/// @details Barriers recorded internally by the backend have no per-plane tracking, so both aspects are assumed to
///          share a layout: `READ_ONLY_OPTIMAL` becomes `DEPTH_STENCIL_READ_ONLY_OPTIMAL` and `ATTACHMENT_OPTIMAL`
///          becomes `DEPTH_STENCIL_ATTACHMENT_OPTIMAL`.
/// @tparam InBarrierArrayType Vector of `VkImageMemoryBarrier`.
/// @param[in,out] targetArray Image barriers to patch in place.
template <typename InBarrierArrayType>
void mergeDepthStencilLayouts(InBarrierArrayType& targetArray) noexcept
{
    for (auto& barrier: targetArray)
    {
        if ((barrier.subresourceRange.aspectMask & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0u)
        {
            if (barrier.newLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
            {
                barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            }
            else if (barrier.newLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
            {
                barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            if (barrier.oldLayout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)
            {
                barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            }
            else if (barrier.oldLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL)
            {
                barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
        }
    }
}

}   // namespace gp::rhi::vulkan
