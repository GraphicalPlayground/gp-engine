// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanTransitionProcessor.hpp"
#include "barriers/IVulkanBarrierCommandRecorder.hpp"
#include "barriers/VulkanBarrierContext.hpp"
#include "barriers/VulkanBarrierConversion.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "barriers/VulkanTransitionData.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/Transition.hpp"
#include "sync/TransitionDesc.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Tests whether two pipeline masks share at least one pipeline.
/// @param[in] pipelines Pipeline mask to test.
/// @param[in] pipeline Pipeline bits to look for.
/// @return True when any bit of `pipeline` is set in `pipelines`.
[[nodiscard]] constexpr bool
    hasAnyPipeline(const sync::PipelineStage pipelines, const sync::PipelineStage pipeline) noexcept
{
    return (pipelines & pipeline) != sync::PipelineStage::None;
}

/// @brief Returns the Vulkan payload of a transition.
/// @param[in] transition Transition.
/// @return Payload stored after the transition.
[[nodiscard]] const VulkanTransitionData* getTransitionData(const sync::Transition* const transition) noexcept
{
    return transition->getPrivateData<VulkanTransitionData>();
}

}   // namespace

void submitVulkanSync2Barriers(
    const VulkanBarrierContext& context,
    const bool isBeginTransition,
    const gp::ConstVectorView<VkMemoryBarrier2> memoryBarriers,
    const gp::ConstVectorView<VkBufferMemoryBarrier2> bufferBarriers,
    const gp::ConstVectorView<VkImageMemoryBarrier2> imageBarriers,
    const VkEvent barrierEvent
) noexcept
{
    if (memoryBarriers.isEmpty() && bufferBarriers.isEmpty() && imageBarriers.isEmpty())
    {
        return;
    }

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

    const gp::Int32 maxBarriersPerBatch = context.settings.maxBarriersPerBatch;
    const gp::Int32 totalBarrierCount =
        static_cast<gp::Int32>(memoryBarriers.size() + bufferBarriers.size() + imageBarriers.size());

    if (barrierEvent != VK_NULL_HANDLE)
    {
        if (isBeginTransition)
        {
            context.recorder->beginSplitBarrier(barrierEvent, dependencyInfo);
        }
        else
        {
            context.recorder->endSplitBarrier(barrierEvent, dependencyInfo);
        }
    }
    else if ((maxBarriersPerBatch <= 0) || (totalBarrierCount < maxBarriersPerBatch))
    {
        context.recorder->pipelineBarrier2(dependencyInfo);
    }
    else
    {
        VkDependencyInfo batchDependencyInfo = dependencyInfo;
        batchDependencyInfo.memoryBarrierCount = 0u;
        batchDependencyInfo.bufferMemoryBarrierCount = 0u;
        batchDependencyInfo.imageMemoryBarrierCount = 0u;

        sendVulkanBatchedBarriers(
            context,
            batchDependencyInfo,
            batchDependencyInfo.pMemoryBarriers,
            batchDependencyInfo.memoryBarrierCount,
            static_cast<gp::Int32>(dependencyInfo.memoryBarrierCount)
        );
        sendVulkanBatchedBarriers(
            context,
            batchDependencyInfo,
            batchDependencyInfo.pBufferMemoryBarriers,
            batchDependencyInfo.bufferMemoryBarrierCount,
            static_cast<gp::Int32>(dependencyInfo.bufferMemoryBarrierCount)
        );
        sendVulkanBatchedBarriers(
            context,
            batchDependencyInfo,
            batchDependencyInfo.pImageMemoryBarriers,
            batchDependencyInfo.imageMemoryBarrierCount,
            static_cast<gp::Int32>(dependencyInfo.imageMemoryBarrierCount)
        );
    }
}

void processVulkanTransitionLegacy(
    const VulkanBarrierContext& context, const gp::ConstVectorView<const sync::Transition*> transitions
) noexcept
{
    for (const sync::Transition* const transition: transitions)
    {
        const VulkanTransitionData* const data = getTransitionData(transition);

        const bool isSingleQueue =
            sync::isSinglePipelineStage(data->srcPipelines) && (data->srcPipelines == data->dstPipelines);
        GP_CHECKF(isSingleQueue, "Devices without support for Sync2 should not be using async compute.");

#if GP_DO_GUARD_SLOW
        GP_CHECKF(
            hasAnyPipeline(data->srcPipelines, context.executingPipeline) &&
                hasAnyPipeline(data->dstPipelines, context.executingPipeline),
            "The pipelines for this transition are [%u -> %u], but it's submitted on the [%u] queue.",
            static_cast<gp::UInt32>(data->srcPipelines),
            static_cast<gp::UInt32>(data->dstPipelines),
            static_cast<gp::UInt32>(context.executingPipeline)
        );
#endif

        VulkanLegacyBarrierArrays barrierArrays{};
        convertTransitionToBarriers(context, *data, barrierArrays);

        // Merge depth and stencil barriers.
        for (gp::USize index = 0u; index < barrierArrays.imageExtraData.size(); ++index)
        {
            const VulkanImageBarrierExtraData* const remainingExtras = &barrierArrays.imageExtraData[index];
            VkImageMemoryBarrier* const remainingBarriers = &barrierArrays.imageBarriers[index];

            if (remainingExtras->baseTexture.isDepthOrStencilAspect &&
                (remainingExtras->baseTexture.allPlanesTrackedAccess != nullptr))
            {
                // Plane slices 0 and 1 address depth and stencil; any other value covers both planes.
                if (remainingExtras->planeSlice < 2u)
                {
                    remainingExtras->baseTexture.allPlanesTrackedAccess[remainingExtras->planeSlice] =
                        remainingExtras->planeAccess;
                }
                else
                {
                    remainingExtras->baseTexture.allPlanesTrackedAccess[0] = remainingExtras->planeAccess;
                    remainingExtras->baseTexture.allPlanesTrackedAccess[1] = remainingExtras->planeAccess;
                }
            }

            if ((remainingBarriers->image != VK_NULL_HANDLE) && (remainingBarriers->subresourceRange.aspectMask != 0u))
            {
                const gp::Int32 remainingCount = static_cast<gp::Int32>(barrierArrays.imageExtraData.size() - index);
                mergePlanes(remainingBarriers, remainingExtras, remainingCount);
            }
        }

        // Merging depth and stencil leaves the absorbed barrier with an empty aspect mask; drop it.
        for (gp::USize dstIndex = 0u; dstIndex < barrierArrays.imageBarriers.size();)
        {
            if ((barrierArrays.imageBarriers[dstIndex].image == VK_NULL_HANDLE) ||
                (barrierArrays.imageBarriers[dstIndex].subresourceRange.aspectMask == 0u))
            {
                barrierArrays.imageBarriers.removeAtSwap(dstIndex);
            }
            else
            {
                ++dstIndex;
            }
        }

        // Submit merged stage masks with the barrier arrays.
        if (!barrierArrays.memoryBarriers.isEmpty() || !barrierArrays.bufferBarriers.isEmpty() ||
            !barrierArrays.imageBarriers.isEmpty())
        {
            context.recorder->pipelineBarrier(
                barrierArrays.srcStageMask,
                barrierArrays.dstStageMask,
                0u,
                barrierArrays.memoryBarriers,
                barrierArrays.bufferBarriers,
                barrierArrays.imageBarriers
            );
        }
    }
}

void processVulkanTransitionSync2(
    const VulkanBarrierContext& context,
    const gp::ConstVectorView<const sync::Transition*> transitions,
    const bool isBeginTransition
) noexcept
{
    gp::Vector<VkImageMemoryBarrier2> tempImageBarriers{};

    const bool useOwnershipTransfers = context.device.hasAsyncComputeQueue && !context.settings.allowConcurrentImage &&
                                       context.device.needsQueueFamilyOwnershipTransfers;
    for (const sync::Transition* const transition: transitions)
    {
        const VulkanTransitionData* const data = getTransitionData(transition);

        const bool isSingleQueue =
            sync::isSinglePipelineStage(data->srcPipelines) && (data->srcPipelines == data->dstPipelines);
        const sync::PipelineStage targetPipeline = isBeginTransition ? data->srcPipelines : data->dstPipelines;
        const sync::PipelineStage otherPipeline = isBeginTransition ? data->dstPipelines : data->srcPipelines;
        const sync::PipelineStage executingPipeline = context.executingPipeline;

        GP_CHECKF(
            hasAnyPipeline(targetPipeline, executingPipeline),
            "The %s pipelines for this transition are [%u], but it's submitted on the [%u] queue.",
            isBeginTransition ? "SRC" : "DST",
            static_cast<gp::UInt32>(targetPipeline),
            static_cast<gp::UInt32>(executingPipeline)
        );

        // A single-queue barrier that is not split is only recorded at end.
        if (isSingleQueue && isBeginTransition && (data->eventHandle == VK_NULL_HANDLE))
        {
            continue;
        }

        VulkanSync2BarrierArrays barrierArrays{};
        convertTransitionToBarriers(context, *data, barrierArrays);

        // Split barriers always stay on one queue and are recorded right away.
        if (data->eventHandle != VK_NULL_HANDLE)
        {
            GP_CHECKF(isSingleQueue, "Split barriers must remain on same queue!");

            if (!barrierArrays.memoryBarriers.isEmpty() || !barrierArrays.bufferBarriers.isEmpty() ||
                !barrierArrays.imageBarriers.isEmpty())
            {
                submitVulkanSync2Barriers(
                    context,
                    isBeginTransition,
                    barrierArrays.memoryBarriers,
                    barrierArrays.bufferBarriers,
                    barrierArrays.imageBarriers,
                    data->eventHandle
                );
            }
            else if (!isBeginTransition)
            {
                // Some transitions produce no barrier at all (a Discard destination, for example); the event still
                // has to go back to the pool.
                context.recorder->releaseBarrierEvent(data->eventHandle);
            }

            continue;
        }

        // Same-queue and single-queue to single-queue transfers are recorded directly, without copies.
        if (sync::isSinglePipelineStage(data->srcPipelines) && sync::isSinglePipelineStage(data->dstPipelines))
        {
            // A cross-queue 1..1 transition keeps the layout change in the same barrier as the queue transfer.
            if (data->srcPipelines != data->dstPipelines)
            {
                if (useOwnershipTransfers)
                {
                    // Buffer barriers only need the flags the compute queue does not support removed.
                    if (executingPipeline == sync::PipelineStage::AsyncCompute)
                    {
                        maskSupportedAsyncFlags(context.device, barrierArrays.bufferBarriers, true, true, true);
                    }

                    patchCrossPipeTransitions(
                        barrierArrays.imageBarriers, context, data->srcPipelines, data->dstPipelines, isBeginTransition
                    );
                }
                else
                {
                    // Without ownership transfers, record the barrier once, on the graphics queue, where every stage
                    // is supported.
                    GP_CHECK(
                        (targetPipeline == sync::PipelineStage::Graphics) ||
                        (otherPipeline == sync::PipelineStage::Graphics)
                    );
                    if (executingPipeline != sync::PipelineStage::Graphics)
                    {
                        continue;
                    }
                }
            }

            submitVulkanSync2Barriers(
                context,
                isBeginTransition,
                barrierArrays.memoryBarriers,
                barrierArrays.bufferBarriers,
                barrierArrays.imageBarriers,
                data->eventHandle
            );
            continue;
        }

        // For 1..N or N..1 transitions, the layout change is recorded on the single pipeline; the semaphore covers
        // the rest.
        const bool needsPreLayoutChange =
            (isBeginTransition &&
             (sync::isSinglePipelineStage(data->srcPipelines) && !sync::isSinglePipelineStage(data->dstPipelines) &&
              (data->srcPipelines == executingPipeline)));
        const bool needsRelease =
            (isBeginTransition && (needsPreLayoutChange || (sync::isSinglePipelineStage(data->dstPipelines) &&
                                                            !sync::isSinglePipelineStage(data->srcPipelines) &&
                                                            (data->dstPipelines != executingPipeline))));
        const bool needsPostLayoutChange =
            (!isBeginTransition &&
             (sync::isSinglePipelineStage(data->dstPipelines) && !sync::isSinglePipelineStage(data->srcPipelines) &&
              (data->dstPipelines == executingPipeline)));
        const bool needsAcquire =
            (!isBeginTransition && (needsPostLayoutChange || (sync::isSinglePipelineStage(data->srcPipelines) &&
                                                              !sync::isSinglePipelineStage(data->dstPipelines) &&
                                                              (data->srcPipelines != executingPipeline))));

        // Images without concurrent sharing get a copy of their barriers so the layout change can be removed and
        // the queue family ownership transfer patched in. Buffers need no ownership transfer (VK_KHR_maintenance9
        // is required).
        if (useOwnershipTransfers && (needsRelease || needsAcquire) && !context.settings.allowConcurrentImage)
        {
            tempImageBarriers = barrierArrays.imageBarriers;
            patchCrossPipeTransitions(
                tempImageBarriers, context, data->srcPipelines, data->dstPipelines, isBeginTransition
            );
        }

        if (needsPreLayoutChange)
        {
            // Remove flags the compute queue does not support when recording there.
            if (executingPipeline == sync::PipelineStage::AsyncCompute)
            {
                maskSupportedAsyncFlags(context.device, barrierArrays.bufferBarriers, false, true, true);
                maskSupportedAsyncFlags(context.device, barrierArrays.imageBarriers, false, true, false);
            }

            submitVulkanSync2Barriers(
                context,
                isBeginTransition,
                barrierArrays.memoryBarriers,
                barrierArrays.bufferBarriers,
                barrierArrays.imageBarriers
            );
        }

        if (!tempImageBarriers.isEmpty())
        {
            // No ownership transfers for buffers.
            submitVulkanSync2Barriers(
                context,
                isBeginTransition,
                barrierArrays.memoryBarriers,
                gp::ConstVectorView<VkBufferMemoryBarrier2>{},
                tempImageBarriers
            );
            tempImageBarriers.clear();
        }

        if (needsPostLayoutChange)
        {
            // Remove flags the compute queue does not support when recording there.
            if (executingPipeline == sync::PipelineStage::AsyncCompute)
            {
                maskSupportedAsyncFlags(context.device, barrierArrays.bufferBarriers, true, false, true);
                maskSupportedAsyncFlags(context.device, barrierArrays.imageBarriers, true, false, false);
            }

            submitVulkanSync2Barriers(
                context,
                isBeginTransition,
                barrierArrays.memoryBarriers,
                barrierArrays.bufferBarriers,
                barrierArrays.imageBarriers
            );
        }
    }
}

void processVulkanCrossQueueSemaphores(
    const VulkanBarrierContext& context,
    const gp::ConstVectorView<const sync::Transition*> transitions,
    const bool isBeginTransition
) noexcept
{
    const sync::PipelineStage executingPipeline = context.executingPipeline;

    for (const sync::Transition* const transition: transitions)
    {
        const VulkanTransitionData* const data = getTransitionData(transition);
        if (!data->semaphore)
        {
            continue;
        }

        GP_CHECK(data->srcPipelines != data->dstPipelines);
        if (isBeginTransition)
        {
            if ((sync::isSinglePipelineStage(data->srcPipelines) && (data->srcPipelines == executingPipeline)) ||
                (sync::isSinglePipelineStage(data->dstPipelines) && (data->dstPipelines != executingPipeline)))
            {
                context.recorder->addSignalSemaphore(data->semaphore);
            }
        }
        else
        {
            if ((sync::isSinglePipelineStage(data->dstPipelines) && (data->dstPipelines == executingPipeline)) ||
                (sync::isSinglePipelineStage(data->srcPipelines) && (data->srcPipelines != executingPipeline)))
            {
                context.recorder->addWaitSemaphore(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, data->semaphore);
            }
        }
    }
}

void handleVulkanReservedResourceCommits(
    const VulkanBarrierContext& context, const gp::ConstVectorView<const sync::Transition*> transitions
) noexcept
{
    gp::Vector<VulkanCommitReservedResourceDesc> reservedResourcesToCommit{};
    for (const sync::Transition* const transition: transitions)
    {
        const VulkanTransitionData* const data = getTransitionData(transition);
        for (const sync::TransitionInfo& info: data->transitionInfos)
        {
            if (info.commitInfo.hasValue())
            {
                if (info.type == sync::TransitionResourceType::Buffer)
                {
                    VulkanCommitReservedResourceDesc commitDesc{};
                    commitDesc.resource = info.buffer;
                    commitDesc.commitSizeInBytes = info.commitInfo.value().sizeInBytes;

                    GP_CHECKF(commitDesc.resource != nullptr, "VulkanCommitReservedResourceDesc::resource must be set");

                    reservedResourcesToCommit.pushBack(commitDesc);
                }
                else
                {
                    GP_CHECKF(false, "Reserved resource commits are only supported on buffers.");
                }
            }
        }
    }

    if (!reservedResourcesToCommit.isEmpty())
    {
        context.recorder->addReservedResourceCommits(reservedResourcesToCommit);
    }
}

void beginVulkanTransitions(
    const VulkanBarrierContext& context, const gp::ConstVectorView<const sync::Transition*> transitions
) noexcept
{
    if (context.device.supportsParallelRendering)
    {
        constexpr bool kIsBeginTransition = true;

        processVulkanTransitionSync2(context, transitions, kIsBeginTransition);

        // Signal the semaphores after the release barriers.
        processVulkanCrossQueueSemaphores(context, transitions, kIsBeginTransition);
    }
    else
    {
        // Legacy barriers have nothing to record at begin (no split barriers, no async compute).
    }
}

void endVulkanTransitions(
    const VulkanBarrierContext& context, const gp::ConstVectorView<const sync::Transition*> transitions
) noexcept
{
    if (context.device.supportsParallelRendering)
    {
        constexpr bool kIsBeginTransition = false;

        // Wait on the semaphores before the acquire barriers.
        processVulkanCrossQueueSemaphores(context, transitions, kIsBeginTransition);

        // Update reserved resource memory mappings.
        handleVulkanReservedResourceCommits(context, transitions);

        processVulkanTransitionSync2(context, transitions, kIsBeginTransition);
    }
    else
    {
        processVulkanTransitionLegacy(context, transitions);
    }
}

}   // namespace gp::rhi::vulkan
