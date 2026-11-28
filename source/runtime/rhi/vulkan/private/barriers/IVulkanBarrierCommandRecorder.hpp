// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "barriers/VulkanBarrierTypes.hpp"
#include "containers/views/VectorView.hpp"     // IWYU pragma: keep
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Interface receiving the barrier, event and semaphore commands produced by transition processing.
/// @details Transition processing decides what to record; the recorder decides where it goes. The production
///          implementation writes into a `VulkanCommandBuffer` and queues semaphores for the next submission, while
///          tests capture every call to check the exact barriers produced.
class GP_RHI_VULKAN_API IVulkanBarrierCommandRecorder
{
public:
    /// @brief Virtual destructor.
    virtual ~IVulkanBarrierCommandRecorder() noexcept = default;

    /// @brief Records a legacy `vkCmdPipelineBarrier`.
    /// @param[in] srcStageMask Merged source stage mask.
    /// @param[in] dstStageMask Merged destination stage mask.
    /// @param[in] dependencyFlags Dependency flags.
    /// @param[in] memoryBarriers Global memory barriers.
    /// @param[in] bufferBarriers Buffer barriers.
    /// @param[in] imageBarriers Image barriers.
    virtual void pipelineBarrier(
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkDependencyFlags dependencyFlags,
        gp::ConstVectorView<VkMemoryBarrier> memoryBarriers,
        gp::ConstVectorView<VkBufferMemoryBarrier> bufferBarriers,
        gp::ConstVectorView<VkImageMemoryBarrier> imageBarriers
    ) noexcept = 0;

    /// @brief Records `vkCmdPipelineBarrier2KHR`.
    /// @param[in] dependencyInfo Barrier batch.
    virtual void pipelineBarrier2(const VkDependencyInfo& dependencyInfo) noexcept = 0;

    /// @brief Records the signal half of a split barrier (`vkCmdSetEvent2KHR`).
    /// @param[in] event Split-barrier event.
    /// @param[in] dependencyInfo Barrier batch.
    virtual void beginSplitBarrier(VkEvent event, const VkDependencyInfo& dependencyInfo) noexcept = 0;

    /// @brief Records the wait half of a split barrier (`vkCmdWaitEvents2KHR`).
    /// @param[in] event Split-barrier event.
    /// @param[in] dependencyInfo Barrier batch matching the begin half.
    virtual void endSplitBarrier(VkEvent event, const VkDependencyInfo& dependencyInfo) noexcept = 0;

    /// @brief Returns a split-barrier event that ended up recording no barrier.
    /// @param[in] event Event to return to the device pool.
    virtual void releaseBarrierEvent(VkEvent event) noexcept = 0;

    /// @brief Queues a semaphore to signal when the current work is submitted.
    /// @param[in] semaphore Semaphore to signal.
    virtual void addSignalSemaphore(const VulkanSemaphoreRef& semaphore) noexcept = 0;

    /// @brief Queues a semaphore to wait on before the current work runs.
    /// @param[in] waitStageMask Stages that wait on the semaphore.
    /// @param[in] semaphore Semaphore to wait on.
    virtual void addWaitSemaphore(VkPipelineStageFlags waitStageMask, const VulkanSemaphoreRef& semaphore) noexcept = 0;

    /// @brief Queues reserved buffer commits to apply before the current work is submitted.
    /// @param[in] commits Commit requests.
    virtual void addReservedResourceCommits(gp::ConstVectorView<VulkanCommitReservedResourceDesc> commits) noexcept = 0;
};

}   // namespace gp::rhi::vulkan
