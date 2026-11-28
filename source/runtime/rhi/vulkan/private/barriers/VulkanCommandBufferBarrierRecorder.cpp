// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanCommandBufferBarrierRecorder.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "sync/VulkanSemaphore.hpp"

namespace gp::rhi::vulkan
{

VulkanCommandBufferBarrierRecorder::VulkanCommandBufferBarrierRecorder(VulkanCommandBuffer& cmdBuffer) noexcept
    : m_cmdBuffer(cmdBuffer)
{}

void VulkanCommandBufferBarrierRecorder::pipelineBarrier(
    const VkPipelineStageFlags srcStageMask,
    const VkPipelineStageFlags dstStageMask,
    const VkDependencyFlags dependencyFlags,
    const gp::ConstVectorView<VkMemoryBarrier> memoryBarriers,
    const gp::ConstVectorView<VkBufferMemoryBarrier> bufferBarriers,
    const gp::ConstVectorView<VkImageMemoryBarrier> imageBarriers
) noexcept
{
    vkCmdPipelineBarrier(
        m_cmdBuffer.getHandle(),
        srcStageMask,
        dstStageMask,
        dependencyFlags,
        static_cast<gp::UInt32>(memoryBarriers.size()),
        memoryBarriers.data(),
        static_cast<gp::UInt32>(bufferBarriers.size()),
        bufferBarriers.data(),
        static_cast<gp::UInt32>(imageBarriers.size()),
        imageBarriers.data()
    );
}

void VulkanCommandBufferBarrierRecorder::pipelineBarrier2(const VkDependencyInfo& dependencyInfo) noexcept
{
    vkCmdPipelineBarrier2KHR(m_cmdBuffer.getHandle(), &dependencyInfo);
}

void VulkanCommandBufferBarrierRecorder::beginSplitBarrier(
    const VkEvent event, const VkDependencyInfo& dependencyInfo
) noexcept
{
    static_cast<void>(GP_ENSURE_MSG(
        m_cmdBuffer.beginSplitBarrier(event, dependencyInfo),
        "beginSplitBarrier() could not be recorded into the command buffer."
    ));
}

void VulkanCommandBufferBarrierRecorder::endSplitBarrier(
    const VkEvent event, const VkDependencyInfo& dependencyInfo
) noexcept
{
    static_cast<void>(GP_ENSURE_MSG(
        m_cmdBuffer.endSplitBarrier(event, dependencyInfo),
        "endSplitBarrier() could not be recorded into the command buffer."
    ));
}

void VulkanCommandBufferBarrierRecorder::releaseBarrierEvent(const VkEvent event) noexcept
{
    m_cmdBuffer.getDevice().releaseBarrierEvent(event);
}

void VulkanCommandBufferBarrierRecorder::addSignalSemaphore(const VulkanSemaphoreRef& semaphore) noexcept
{
    m_signalSemaphores.pushBack(semaphore);
}

void VulkanCommandBufferBarrierRecorder::addWaitSemaphore(
    const VkPipelineStageFlags waitStageMask, const VulkanSemaphoreRef& semaphore
) noexcept
{
    m_waitStageMasks.pushBack(waitStageMask);
    m_waitSemaphores.pushBack(semaphore);
}

void VulkanCommandBufferBarrierRecorder::addReservedResourceCommits(
    const gp::ConstVectorView<VulkanCommitReservedResourceDesc> commits
) noexcept
{
    m_reservedResourceCommits.append(commits.data(), commits.size());
}

void VulkanCommandBufferBarrierRecorder::clearPendingSubmissionState() noexcept
{
    m_signalSemaphores.clear();
    m_waitSemaphores.clear();
    m_waitStageMasks.clear();
    m_reservedResourceCommits.clear();
}

}   // namespace gp::rhi::vulkan
