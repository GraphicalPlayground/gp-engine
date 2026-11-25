// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/IVulkanBarrierCommandRecorder.hpp"
#include "barriers/VulkanBarrierContext.hpp"
#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"     // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "RHIVulkan.hpp"
#include "sync/SyncForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Records one `synchronization2` barrier batch, as a split barrier, a single call, or several batched calls.
/// @details Nothing is recorded when every view is empty. With `barrierEvent`, the batch is the begin or end half of
///          a split barrier. Otherwise it is one `vkCmdPipelineBarrier2KHR`, split into chunks of
///          `settings.maxBarriersPerBatch` when that limit is positive and reached.
/// @param[in] context Barrier context.
/// @param[in] isBeginTransition Selects the split-barrier half.
/// @param[in] memoryBarriers Global memory barriers.
/// @param[in] bufferBarriers Buffer barriers.
/// @param[in] imageBarriers Image barriers.
/// @param[in] barrierEvent Split-barrier event, or `VK_NULL_HANDLE`.
GP_RHI_VULKAN_API void submitVulkanSync2Barriers(
    const VulkanBarrierContext& context,
    bool isBeginTransition,
    gp::ConstVectorView<VkMemoryBarrier2> memoryBarriers,
    gp::ConstVectorView<VkBufferMemoryBarrier2> bufferBarriers,
    gp::ConstVectorView<VkImageMemoryBarrier2> imageBarriers,
    VkEvent barrierEvent = VK_NULL_HANDLE
) noexcept;

/// @brief Records `dependencyInfo` as several `vkCmdPipelineBarrier2KHR` calls of at most `maxBarriersPerBatch`
///        barriers of one kind.
/// @details `barrierPtr` and `barrierCount` point into `batchDependencyInfo`; they are advanced chunk by chunk and
///          reset to null and zero afterwards, so the next kind of barrier can be batched with the same structure.
/// @tparam InBarrierType `VkMemoryBarrier2`, `VkBufferMemoryBarrier2` or `VkImageMemoryBarrier2`.
/// @param[in] context Barrier context.
/// @param[in,out] batchDependencyInfo Dependency structure reused for every chunk.
/// @param[in,out] barrierPtr Barrier pointer field of `batchDependencyInfo` for this kind.
/// @param[in,out] barrierCount Barrier count field of `batchDependencyInfo` for this kind.
/// @param[in] totalBarrierCount Number of barriers of this kind.
template <typename InBarrierType>
void sendVulkanBatchedBarriers(
    const VulkanBarrierContext& context,
    VkDependencyInfo& batchDependencyInfo,
    const InBarrierType*& barrierPtr,
    gp::UInt32& barrierCount,
    const gp::Int32 totalBarrierCount
) noexcept
{
    const gp::Int32 maxBarriersPerBatch = context.settings.maxBarriersPerBatch;
    GP_CHECK(maxBarriersPerBatch > 0);

    for (gp::Int32 batchStartIndex = 0; batchStartIndex < totalBarrierCount; batchStartIndex += maxBarriersPerBatch)
    {
        const gp::Int32 remaining = totalBarrierCount - batchStartIndex;
        barrierCount = static_cast<gp::UInt32>((remaining < maxBarriersPerBatch) ? remaining : maxBarriersPerBatch);
        context.recorder->pipelineBarrier2(batchDependencyInfo);
        barrierPtr += barrierCount;
    }
    barrierPtr = nullptr;
    barrierCount = 0u;
}

/// @brief Records transitions on a device without `synchronization2` (single queue only).
/// @details Each transition becomes one `vkCmdPipelineBarrier` with merged stage masks. Depth and stencil barriers
///          are merged into one barrier per image, and the tracked per-plane access of each texture is updated.
/// @param[in] context Barrier context.
/// @param[in] transitions Transitions to record.
GP_RHI_VULKAN_API void processVulkanTransitionLegacy(
    const VulkanBarrierContext& context, gp::ConstVectorView<const sync::Transition*> transitions
) noexcept;

/// @brief Records the begin or end half of transitions on a device with `synchronization2`.
/// @details Same-queue transitions without an event are only recorded at end. Split transitions record each half
///          with their event. Cross-queue 1..1 transitions record queue family ownership transfers when needed (or
///          only on the graphics queue when not). For 1..N and N..1 transitions, the layout change is recorded on the
///          single pipeline and the rest is covered by the semaphore.
/// @param[in] context Barrier context.
/// @param[in] transitions Transitions to record.
/// @param[in] isBeginTransition True for the begin half.
GP_RHI_VULKAN_API void processVulkanTransitionSync2(
    const VulkanBarrierContext& context,
    gp::ConstVectorView<const sync::Transition*> transitions,
    bool isBeginTransition
) noexcept;

/// @brief Queues the cross-queue semaphore of each transition as a signal (begin) or a wait (end).
/// @param[in] context Barrier context.
/// @param[in] transitions Transitions to process.
/// @param[in] isBeginTransition True to signal, false to wait.
GP_RHI_VULKAN_API void processVulkanCrossQueueSemaphores(
    const VulkanBarrierContext& context,
    gp::ConstVectorView<const sync::Transition*> transitions,
    bool isBeginTransition
) noexcept;

/// @brief Collects reserved buffer commits carried by transitions and hands them to the recorder.
/// @param[in] context Barrier context.
/// @param[in] transitions Transitions to scan.
GP_RHI_VULKAN_API void handleVulkanReservedResourceCommits(
    const VulkanBarrierContext& context, gp::ConstVectorView<const sync::Transition*> transitions
) noexcept;

/// @brief Records the begin half of `transitions`.
/// @details With `synchronization2`, release barriers are recorded first, then the cross-queue semaphores are
///          signaled. The legacy path has nothing to record at begin (no split barriers, no async compute).
/// @param[in] context Barrier context.
/// @param[in] transitions Transitions to begin.
GP_RHI_VULKAN_API void beginVulkanTransitions(
    const VulkanBarrierContext& context, gp::ConstVectorView<const sync::Transition*> transitions
) noexcept;

/// @brief Records the end half of `transitions`.
/// @details With `synchronization2`, cross-queue semaphores are waited on first, reserved buffer commits are queued,
///          then the acquire barriers are recorded. The legacy path records the whole barrier here.
/// @param[in] context Barrier context.
/// @param[in] transitions Transitions to end.
GP_RHI_VULKAN_API void endVulkanTransitions(
    const VulkanBarrierContext& context, gp::ConstVectorView<const sync::Transition*> transitions
) noexcept;

}   // namespace gp::rhi::vulkan
