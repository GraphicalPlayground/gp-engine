// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierContext.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "barriers/VulkanBarrierTypes.hpp"
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/allocators/InlineAllocator.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <type_traits>

namespace gp::rhi::vulkan
{

/// @brief Side data kept with each legacy image barrier for depth/stencil plane merging.
/// @details The legacy barrier path must transition depth and stencil together. It needs the texture (to look for the
///          barrier on the other plane and to read the other plane's tracked access) and the access each plane ends
///          in, so the tracked per-plane state can be updated once the barriers are merged.
struct VulkanImageBarrierExtraData
{
    /// @brief Texture the barrier applies to.
    VulkanBarrierResource baseTexture{};

    /// @brief True when the barrier acquires aliased memory (its source state is `Discard`).
    bool isAliasingBarrier{ false };

    /// @brief Plane transitioned by the barrier (`0` depth, `1` stencil, anything else for both).
    gp::UInt8 planeSlice{ 0u };

    /// @brief Access the plane ends in.
    sync::ResourceAccess planeAccess{ sync::ResourceAccess::Unknown };
};

/// @brief Batch of legacy barrier arrays recorded by `vkCmdPipelineBarrier` with merged stage masks.
struct VulkanLegacyBarrierArrays
{
    /// @brief Merged global memory barrier (zero or one entry).
    gp::InlineVector<VkMemoryBarrier, 1u> memoryBarriers{};

    /// @brief Buffer barriers.
    gp::Vector<VkBufferMemoryBarrier> bufferBarriers{};

    /// @brief Image barriers.
    gp::Vector<VkImageMemoryBarrier> imageBarriers{};

    /// @brief One entry per image barrier, at the same index.
    gp::Vector<VulkanImageBarrierExtraData> imageExtraData{};

    /// @brief Union of every barrier source stage mask.
    VkPipelineStageFlags srcStageMask{ 0u };

    /// @brief Union of every barrier destination stage mask.
    VkPipelineStageFlags dstStageMask{ 0u };
};

/// @brief Batch of `synchronization2` barrier arrays recorded by `vkCmdPipelineBarrier2KHR`.
struct VulkanSync2BarrierArrays
{
    /// @brief Merged global memory barrier (zero or one entry).
    gp::InlineVector<VkMemoryBarrier2, 1u> memoryBarriers{};

    /// @brief Buffer barriers.
    gp::Vector<VkBufferMemoryBarrier2> bufferBarriers{};

    /// @brief Image barriers.
    gp::Vector<VkImageMemoryBarrier2> imageBarriers{};
};

/// @brief Converts the transition infos of `data` into Vulkan barriers appended to `outBarriers`.
/// @details For each resource:
///          1. Resolve `ResourceAccess::Unknown` from tracked state (`sync::ResourceState`).
///          2. Translate both access states into stage masks, access masks and layouts. Depth/stencil textures use
///             the per-plane tables.
///          3. On a single pipeline with no layout change, fold the dependency into one global memory barrier
///             (always for readback textures, otherwise only when `useMemoryBarrierOpt` is set).
///          4. Otherwise emit a buffer barrier or an image barrier for the requested subresources.
///          Transitions to `Discard` are skipped because discarding contents needs no barrier.
/// @note This overload fills legacy arrays: stage masks are merged into `srcStageMask` / `dstStageMask` and one
///       `VulkanImageBarrierExtraData` is recorded per image barrier.
/// @param[in] context Barrier context.
/// @param[in] data Transition payload.
/// @param[in,out] outBarriers Arrays receiving the barriers.
GP_RHI_VULKAN_API void convertTransitionToBarriers(
    const VulkanBarrierContext& context, const VulkanTransitionData& data, VulkanLegacyBarrierArrays& outBarriers
) noexcept;

/// @brief Converts the transition infos of `data` into `synchronization2` barriers appended to `outBarriers`.
/// @details Same conversion as the legacy overload; every barrier carries its own stage masks.
/// @param[in] context Barrier context.
/// @param[in] data Transition payload.
/// @param[in,out] outBarriers Arrays receiving the barriers.
GP_RHI_VULKAN_API void convertTransitionToBarriers(
    const VulkanBarrierContext& context, const VulkanTransitionData& data, VulkanSync2BarrierArrays& outBarriers
) noexcept;

/// @brief Folds the barrier on the other plane of a depth/stencil image into the first remaining image barrier.
/// @details Legacy barriers must transition depth and stencil together. When the barrier covers one plane only, the
///          remaining barriers are searched for the other plane: if found, its layouts are merged in and its aspect
///          mask is cleared so it gets removed; if not, the other plane keeps the layout of its tracked access.
/// @param[in,out] imageBarriers Remaining image barriers; `imageBarriers[0]` is the barrier being merged.
/// @param[in] remainingExtras Extra data matching `imageBarriers`.
/// @param[in] remainingCount Number of entries in both arrays.
GP_RHI_VULKAN_API void mergePlanes(
    VkImageMemoryBarrier* imageBarriers, const VulkanImageBarrierExtraData* remainingExtras, gp::Int32 remainingCount
) noexcept;

/// @brief Removes stage and access bits the async compute queue does not support.
/// @tparam InBarrierArrayType Vector of `synchronization2` buffer or image barriers.
/// @param[in] device Device snapshot holding the compute queue masks.
/// @param[in,out] barriers Barriers to patch.
/// @param[in] maskSrc Masks the source stage and access.
/// @param[in] maskDst Masks the destination stage and access.
/// @param[in] removeEmpty Removes barriers left with an empty source or destination stage mask.
template <typename InBarrierArrayType>
void maskSupportedAsyncFlags(
    const VulkanBarrierDeviceDesc& device,
    InBarrierArrayType& barriers,
    const bool maskSrc,
    const bool maskDst,
    const bool removeEmpty
) noexcept
{
    const VkPipelineStageFlags supportedComputeStageMask = device.computeSupportedStages;
    const VkAccessFlags supportedComputeAccessMask = device.computeSupportedAccess;

    for (gp::Int64 index = static_cast<gp::Int64>(barriers.size()) - 1; index >= 0; --index)
    {
        auto& barrier = barriers[static_cast<gp::USize>(index)];
        if (maskSrc)
        {
            barrier.srcStageMask &= supportedComputeStageMask;
            barrier.srcAccessMask &= supportedComputeAccessMask;
        }
        if (maskDst)
        {
            barrier.dstStageMask &= supportedComputeStageMask;
            barrier.dstAccessMask &= supportedComputeAccessMask;
        }
        if (removeEmpty && ((barrier.srcStageMask == 0u) || (barrier.dstStageMask == 0u)))
        {
            barriers.removeAtSwap(static_cast<gp::USize>(index));
        }
    }
}

/// @brief Patches barriers into queue family ownership release or acquire barriers.
/// @details Between two single pipelines the layout change stays in the same barrier, so one barrier pair does all
///          the work. When either side is `PipelineStage::All`, the barriers become a single graphics/compute
///          transfer: unsupported compute bits are stripped and the layout change is removed (it is recorded
///          separately on the single pipeline). A release clears the destination access, an acquire clears the
///          source access.
/// @tparam InBarrierArrayType Vector of `synchronization2` buffer or image barriers.
/// @param[in,out] barriers Barriers to patch.
/// @param[in] context Barrier context (executing pipeline and queue families).
/// @param[in] srcPipelines Source pipelines of the transition.
/// @param[in] dstPipelines Destination pipelines of the transition.
/// @param[in] isBeginTransition True on the release side, false on the acquire side.
template <typename InBarrierArrayType>
void patchCrossPipeTransitions(
    InBarrierArrayType& barriers,
    const VulkanBarrierContext& context,
    const sync::PipelineStage srcPipelines,
    const sync::PipelineStage dstPipelines,
    const bool isBeginTransition
) noexcept
{
    const sync::PipelineStage executingPipeline = context.executingPipeline;
    const gp::UInt32 graphicsFamilyIndex = context.device.graphicsQueueFamilyIndex;
    const gp::UInt32 computeFamilyIndex = context.device.computeQueueFamilyIndex;

    // Between two single pipelines, keep the layout change so all the work happens in one barrier.
    if (sync::isSinglePipelineStage(srcPipelines) && sync::isSinglePipelineStage(dstPipelines))
    {
        for (gp::USize index = 0u; index < barriers.size(); ++index)
        {
            auto& barrier = barriers[index];

            barrier.srcQueueFamilyIndex =
                (srcPipelines == sync::PipelineStage::Graphics) ? graphicsFamilyIndex : computeFamilyIndex;
            barrier.dstQueueFamilyIndex =
                (dstPipelines == sync::PipelineStage::Graphics) ? graphicsFamilyIndex : computeFamilyIndex;

            if (isBeginTransition)
            {
                // Release
                GP_CHECK(srcPipelines == executingPipeline);
                barrier.dstAccessMask = 0u;
            }
            else
            {
                // Acquire
                GP_CHECK(dstPipelines == executingPipeline);
                barrier.srcAccessMask = 0u;
            }
        }
    }
    else
    {
        // One side is PipelineStage::All; record a single graphics/compute transfer for now.
        // The transfer has a graphics source for Graphics -> All and All -> AsyncCompute.
        const bool srcIsGraphics =
            ((srcPipelines == sync::PipelineStage::Graphics) || (dstPipelines == sync::PipelineStage::AsyncCompute));

        // A PipelineStage::All transition may carry stages or accesses that some queues do not support.
        maskSupportedAsyncFlags(context.device, barriers, !srcIsGraphics, srcIsGraphics, false);

        for (gp::USize index = 0u; index < barriers.size(); ++index)
        {
            auto& barrier = barriers[index];

            if (srcIsGraphics)
            {
                barrier.srcQueueFamilyIndex = graphicsFamilyIndex;
                barrier.dstQueueFamilyIndex = computeFamilyIndex;
            }
            else
            {
                barrier.srcQueueFamilyIndex = computeFamilyIndex;
                barrier.dstQueueFamilyIndex = graphicsFamilyIndex;
            }

            // The layout change is recorded separately, on the single pipeline of a 1..N or N..1 transition.
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(barrier)>, VkImageMemoryBarrier2>)
            {
                if (sync::isSinglePipelineStage(srcPipelines))
                {
                    barrier.oldLayout = barrier.newLayout;
                }
                else if (sync::isSinglePipelineStage(dstPipelines))
                {
                    barrier.newLayout = barrier.oldLayout;
                }
            }

            if (isBeginTransition)
            {
                // Release the resource from the current queue.
                GP_CHECK((srcPipelines & executingPipeline) == executingPipeline);
                barrier.dstAccessMask = 0u;
            }
            else
            {
                // Acquire the resource on the current queue.
                GP_CHECK((dstPipelines & executingPipeline) == executingPipeline);
                barrier.srcAccessMask = 0u;
            }
        }
    }
}

}   // namespace gp::rhi::vulkan
