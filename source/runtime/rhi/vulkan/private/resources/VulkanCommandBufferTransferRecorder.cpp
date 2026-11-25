// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanCommandBufferTransferRecorder.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "utils/VulkanDebugBarrier.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanCommandBufferTransferRecorder::VulkanCommandBufferTransferRecorder(
    VulkanCommandBuffer& commandBuffer, VulkanSyncPointRef completionSyncPoint
) noexcept
    : m_commandBuffer(&commandBuffer)
    , m_completionSyncPoint(std::move(completionSyncPoint))
{}

void VulkanCommandBufferTransferRecorder::copyBuffer(
    const VkBuffer srcBuffer, const VkBuffer dstBuffer, const gp::ConstVectorView<VkBufferCopy> regions
) noexcept
{
    GP_CHECK(m_commandBuffer != nullptr);
    if (regions.isEmpty())
    {
        return;
    }
    vkCmdCopyBuffer(
        m_commandBuffer->getHandle(), srcBuffer, dstBuffer, static_cast<gp::UInt32>(regions.size()), regions.data()
    );
}

void VulkanCommandBufferTransferRecorder::fillBuffer(
    const VkBuffer dstBuffer, const VkDeviceSize offset, const VkDeviceSize size, const gp::UInt32 data
) noexcept
{
    GP_CHECK(m_commandBuffer != nullptr);
    vkCmdFillBuffer(m_commandBuffer->getHandle(), dstBuffer, offset, size, data);
}

void VulkanCommandBufferTransferRecorder::memoryBarrier(
    const VkPipelineStageFlags srcStageMask, const VkPipelineStageFlags dstStageMask, const VkMemoryBarrier& barrier
) noexcept
{
    GP_CHECK(m_commandBuffer != nullptr);
    vkCmdPipelineBarrier(
        m_commandBuffer->getHandle(), srcStageMask, dstStageMask, 0u, 1u, &barrier, 0u, nullptr, 0u, nullptr
    );
}

void VulkanCommandBufferTransferRecorder::debugHeavyWeightBarrier(const gp::Int32 categoryMask) noexcept
{
    GP_CHECK(m_commandBuffer != nullptr);
    vulkan::debugHeavyWeightBarrier(m_commandBuffer->getHandle(), categoryMask);
}

bool VulkanCommandBufferTransferRecorder::isOutsideRenderPass() const noexcept
{
    return (m_commandBuffer != nullptr) && m_commandBuffer->isOutsideRenderPass();
}

VulkanSyncPointRef VulkanCommandBufferTransferRecorder::getCompletionSyncPoint() const noexcept
{
    return m_completionSyncPoint;
}

void VulkanCommandBufferTransferRecorder::rebind(
    VulkanCommandBuffer& commandBuffer, VulkanSyncPointRef completionSyncPoint
) noexcept
{
    m_commandBuffer = &commandBuffer;
    m_completionSyncPoint = std::move(completionSyncPoint);
}

}   // namespace gp::rhi::vulkan

