// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanReadbackStagingBuffer.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "memory/VulkanStagingManager.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "sync/GPUFence.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

VulkanReadbackStagingBuffer::~VulkanReadbackStagingBuffer() noexcept
{
    if (m_stagingBuffer != nullptr)
    {
        GP_CHECK(m_stagingManager != nullptr);
        m_stagingManager->releaseBuffer(VulkanSyncPointRef{}, m_stagingBuffer);
    }
}

void* VulkanReadbackStagingBuffer::lock(const gp::UInt32 offset, const gp::UInt32 numBytes) noexcept
{
    GP_CHECKF(!m_isLocked, "Staging buffer is already locked.");
    m_isLocked = true;

    const gp::UInt64 endOffset = static_cast<gp::UInt64>(offset) + numBytes;
    GP_CHECKF(endOffset <= m_queuedNumBytes, "Lock reads beyond the bytes queued in the staging buffer.");

    if (m_stagingBuffer == nullptr)
    {
        return nullptr;
    }

    // Host-cached memory: drop stale cache lines so the CPU sees what the GPU wrote.
    m_stagingBuffer->invalidateMappedMemory();
    return static_cast<gp::UInt8*>(m_stagingBuffer->getMappedPointer()) + offset;
}

void VulkanReadbackStagingBuffer::unlock() noexcept
{
    GP_CHECKF(m_isLocked, "Unlocking a staging buffer that is not locked.");
    m_isLocked = false;
}

gp::UInt64 VulkanReadbackStagingBuffer::getGPUSizeBytes() const noexcept
{
    return (m_stagingBuffer != nullptr) ? m_stagingBuffer->getSize() : 0u;
}

void VulkanReadbackStagingBuffer::copyFromBuffer(
    IVulkanTransferCommandRecorder& recorder,
    const VulkanBuffer& sourceBuffer,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes
) noexcept
{
    static_cast<void>(
        GP_ENSURE_MSG(recorder.isOutsideRenderPass(), "Copying to a staging buffer inside a render pass.")
    );

    if (m_stagingBuffer == nullptr || m_stagingBuffer->getSize() < numBytes)
    {
        if (m_stagingBuffer != nullptr)
        {
            m_stagingManager->releaseBuffer(VulkanSyncPointRef{}, m_stagingBuffer);
        }

        m_stagingManager = sourceBuffer.getServices().stagingManager;
        GP_CHECK(m_stagingManager != nullptr);
        m_stagingBuffer = m_stagingManager->acquireBuffer(
            numBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT
        );
        GP_CHECKF(m_stagingBuffer != nullptr, "Failed to acquire a readback staging buffer.");
    }

    m_queuedNumBytes = numBytes;

    VkBufferCopy region{};
    region.srcOffset = static_cast<VkDeviceSize>(offset) + sourceBuffer.getOffset();
    region.dstOffset = 0u;
    region.size = numBytes;
    recorder.copyBuffer(
        sourceBuffer.getHandle(), m_stagingBuffer->getHandle(), gp::ConstVectorView<VkBufferCopy>(&region, 1u)
    );

    // Make the transfer write visible to host reads; lock() invalidates the CPU cache.
    VkMemoryBarrier barrierAfter = zeroVulkanStruct<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
    barrierAfter.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierAfter.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    recorder.memoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, barrierAfter);
}

resource::StagingBufferRef createVulkanStagingBuffer() noexcept
{
    return resource::StagingBufferRef(new VulkanReadbackStagingBuffer());
}

void* lockVulkanStagingBuffer(
    resource::StagingBuffer* const stagingBuffer,
    const sync::GPUFence* const fence,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes
) noexcept
{
    GP_CHECK(stagingBuffer != nullptr);
    GP_CHECKF(fence == nullptr || fence->poll(), "The GPU fence guarding the staging buffer has not been signaled.");
    return static_cast<VulkanReadbackStagingBuffer*>(stagingBuffer)->lock(offset, numBytes);
}

void unlockVulkanStagingBuffer(resource::StagingBuffer* const stagingBuffer) noexcept
{
    GP_CHECK(stagingBuffer != nullptr);
    static_cast<VulkanReadbackStagingBuffer*>(stagingBuffer)->unlock();
}

}   // namespace gp::rhi::vulkan

