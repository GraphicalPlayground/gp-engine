// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanTransitionData.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/Transition.hpp"
#include "sync/TransitionDesc.hpp"
#include "sync/VulkanSemaphore.hpp"
#include <new>   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

gp::USize getVulkanTransitionPrivateDataSize() noexcept
{
    return sizeof(VulkanTransitionData);
}

gp::USize getVulkanTransitionPrivateDataAlign() noexcept
{
    return alignof(VulkanTransitionData);
}

bool needsVulkanCrossQueueSemaphore(
    const sync::PipelineStage srcPipelines,
    const sync::PipelineStage dstPipelines,
    const sync::TransitionCreateFlags flags,
    const bool needsQueueFamilyOwnershipTransfers
) noexcept
{
    const bool hasNoFence = (flags & sync::TransitionCreateFlags::NoFence) != sync::TransitionCreateFlags::None;
    return (srcPipelines != dstPipelines) && (needsQueueFamilyOwnershipTransfers || !hasNoFence);
}

bool canUseVulkanSplitBarrier(
    const sync::PipelineStage srcPipelines,
    const sync::PipelineStage dstPipelines,
    const sync::TransitionCreateFlags flags,
    const VulkanBarrierSettings& settings,
    const bool supportsParallelRendering
) noexcept
{
    const bool hasNoSplit = (flags & sync::TransitionCreateFlags::NoSplit) != sync::TransitionCreateFlags::None;
    return settings.allowSplitBarriers && (settings.maxBarriersPerBatch <= 0) && supportsParallelRendering &&
           (srcPipelines == dstPipelines) && !hasNoSplit;
}

bool vulkanTransitionIncludesHostSync(const gp::ConstVectorView<sync::TransitionInfo> transitionInfos) noexcept
{
    for (const sync::TransitionInfo& info: transitionInfos)
    {
        if (((info.accessBefore & sync::ResourceAccess::CPURead) != sync::ResourceAccess::None) ||
            ((info.accessAfter & sync::ResourceAccess::CPURead) != sync::ResourceAccess::None))
        {
            return true;
        }
    }
    return false;
}

void createVulkanTransition(
    VulkanDevice& device,
    sync::Transition* const transition,
    const sync::TransitionCreateInfo& createInfo,
    const VulkanBarrierSettings& settings
) noexcept
{
    GP_CHECKF(transition != nullptr, "createVulkanTransition() requires a non-null transition.");

    const sync::PipelineStage srcPipelines = createInfo.srcPipelines;
    const sync::PipelineStage dstPipelines = createInfo.dstPipelines;

    VulkanTransitionData* const data =
        ::new (transition->getPrivateData<VulkanTransitionData>()) VulkanTransitionData();
    data->transitionInfos.append(createInfo.transitionInfos.data(), createInfo.transitionInfos.size());
    data->srcPipelines = srcPipelines;
    data->dstPipelines = dstPipelines;

    // When queue ownership transfers are needed, cross-queue transitions always get a semaphore so the release and
    // acquire barriers are submitted in order.
    if (needsVulkanCrossQueueSemaphore(
            srcPipelines, dstPipelines, createInfo.flags, device.needsQueueFamilyOwnershipTransfers()
        ))
    {
        data->semaphore = VulkanSemaphore::create(device);
    }

    // On a single queue, split the barrier when allowed and supported.
    if (canUseVulkanSplitBarrier(
            srcPipelines, dstPipelines, createInfo.flags, settings, device.supportsParallelRendering()
        ))
    {
        // A host stage dependency cannot go through an event, so such transitions stay unsplit.
        const bool includesHostSync = vulkanTransitionIncludesHostSync(
            gp::ConstVectorView<sync::TransitionInfo>(data->transitionInfos.data(), data->transitionInfos.size())
        );

        data->eventHandle = !includesHostSync ? device.getBarrierEvent() : VK_NULL_HANDLE;
    }
}

void releaseVulkanTransition(sync::Transition* const transition) noexcept
{
    GP_CHECKF(transition != nullptr, "releaseVulkanTransition() requires a non-null transition.");
    transition->getPrivateData<VulkanTransitionData>()->~VulkanTransitionData();
}

}   // namespace gp::rhi::vulkan
