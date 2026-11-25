// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanStagingManager.hpp"
#include "console/ConsoleManager.hpp"   // IWYU pragma: keep
#include "console/IConsoleVariable.hpp"
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Rounds `value` up to a multiple of `alignment`.
/// @param[in] value Input byte count.
/// @param[in] alignment Positive alignment granularity.
/// @return Rounded-up byte count.
[[nodiscard]] constexpr gp::UInt32 alignArbitraryU32(const gp::UInt32 value, const gp::UInt32 alignment) noexcept
{
    if (alignment <= 1u)
    {
        return value;
    }
    const gp::UInt32 remainder = value % alignment;
    return (remainder == 0u) ? value : (value + (alignment - remainder));
}

/// @brief Reads `r.Vulkan.NumFramesToWaitForResourceDelete` (defaults to 0 if unregistered).
/// @return Non-negative frame lag count.
[[nodiscard]] gp::UInt32 getDeletionFrameLag() noexcept
{
    gp::console::IConsoleVariable* const cvar =
        gp::console::ConsoleManager::get().findConsoleVariable("r.Vulkan.NumFramesToWaitForResourceDelete");
    if (cvar == nullptr)
    {
        return 0u;
    }
    return static_cast<gp::UInt32>(gp::math::max<gp::Int32>(0, cvar->getInt()));
}

}   // namespace

VulkanStagingBuffer::VulkanStagingBuffer(VulkanDevice& device, VulkanMemoryManager& memoryManager) noexcept
    : m_device(device)
    , m_memoryManager(memoryManager)
{}

VulkanStagingBuffer::~VulkanStagingBuffer() noexcept
{
    destroy();
}

void* VulkanStagingBuffer::getMappedPointer() const noexcept
{
    return m_allocation.getMappedPointer(m_memoryManager);
}

VkDeviceMemory VulkanStagingBuffer::getDeviceMemoryHandle() const noexcept
{
    return m_allocation.getDeviceMemoryHandle(m_memoryManager);
}

void VulkanStagingBuffer::flushMappedMemory() const noexcept
{
    m_allocation.flushMappedMemory(m_memoryManager);
}

void VulkanStagingBuffer::invalidateMappedMemory() const noexcept
{
    m_allocation.invalidateMappedMemory(m_memoryManager);
}

void VulkanStagingBuffer::destroy() noexcept
{
    if (m_buffer != VK_NULL_HANDLE && m_device.isValid() && dynamic_api::vkDestroyBuffer != nullptr)
    {
        vkDestroyBuffer(m_device.getHandle(), m_buffer, GP_VULKAN_CPU_ALLOCATOR);
        m_buffer = VK_NULL_HANDLE;
    }

    if (m_allocation.hasAllocation())
    {
        m_memoryManager.freeVulkanAllocation(m_allocation, VulkanFreeFlags::DontDefer);
    }
}

VulkanStagingManager::~VulkanStagingManager() noexcept
{
    deinit();
}

void VulkanStagingManager::init(VulkanDevice* const device, VulkanMemoryManager* const memoryManager) noexcept
{
    m_device = device;
    m_memoryManager = memoryManager;
}

void VulkanStagingManager::deinit() noexcept
{
    processPendingFree(true, true, 0u);

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (!m_usedStagingBuffers.isEmpty() || !m_pendingFreeStagingBuffers.isEmpty() || !m_freeStagingBuffers.isEmpty())
    {
        GP_LOG_WARNING(
            LogVulkanRHI,
            "VulkanStagingManager::deinit() detected remaining buffers (used={}, pending={}, free={}).",
            m_usedStagingBuffers.size(),
            m_pendingFreeStagingBuffers.size(),
            m_freeStagingBuffers.size()
        );
    }

    m_device = nullptr;
    m_memoryManager = nullptr;
}

VulkanStagingBuffer* VulkanStagingManager::acquireBuffer(
    gp::UInt32 size, VkBufferUsageFlags usageFlags, const VkMemoryPropertyFlagBits memoryReadFlags
) noexcept
{
    if (m_device == nullptr || m_memoryManager == nullptr || !m_device->isValid() || size == 0u)
    {
        return nullptr;
    }

    const bool isHostCached = (memoryReadFlags == VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
    const gp::UInt32 atomSize =
        static_cast<gp::UInt32>(gp::math::max<VkDeviceSize>(m_device->getLimits().nonCoherentAtomSize, 1u));
    if (isHostCached)
    {
        size = alignArbitraryU32(size, atomSize);
    }

    if ((usageFlags & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)) != 0u)
    {
        usageFlags |= (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    }

    if (m_device->getOptionalExtensions().hasBufferDeviceAddress != 0u)
    {
        usageFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        for (gp::USize index = 0u; index < m_freeStagingBuffers.size(); ++index)
        {
            VulkanStagingBuffer* const candidate = m_freeStagingBuffers[index].stagingBuffer;
            if (candidate != nullptr && candidate->getSize() == size &&
                candidate->getMemoryReadFlags() == memoryReadFlags)
            {
                const gp::USize lastIndex = m_freeStagingBuffers.size() - 1u;
                if (index != lastIndex)
                {
                    m_freeStagingBuffers[index] = m_freeStagingBuffers[lastIndex];
                }
                m_freeStagingBuffers.popBack();
                m_usedStagingBuffers.pushBack(candidate);
                return candidate;
            }
        }
    }

    VulkanStagingBuffer* const stagingBuffer = new VulkanStagingBuffer(*m_device, *m_memoryManager);
    stagingBuffer->m_memoryReadFlags = memoryReadFlags;
    stagingBuffer->m_bufferSize = size;
    stagingBuffer->m_buffer = createVulkanBuffer(*m_device, size, usageFlags);
    if (stagingBuffer->m_buffer == VK_NULL_HANDLE)
    {
        delete stagingBuffer;
        return nullptr;
    }

    gp::UInt32 forcedMinAlignment = 16u;
    const bool isAmd = (m_device->getVendorId() == gp::rhi::GpuVendorId::Amd);
    if (isHostCached || isAmd)
    {
        forcedMinAlignment = alignArbitraryU32(forcedMinAlignment, atomSize);
    }

    const VulkanAllocationFlags allocFlags =
        VulkanAllocationFlags::AutoBind |
        (isHostCached ? VulkanAllocationFlags::HostCached : VulkanAllocationFlags::HostVisible);

    if (!m_memoryManager->allocateBufferMemory(
            stagingBuffer->m_allocation, stagingBuffer->m_buffer, allocFlags, "StagingBuffer", forcedMinAlignment
        ))
    {
        delete stagingBuffer;
        return nullptr;
    }

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        m_usedStagingBuffers.pushBack(stagingBuffer);
        m_usedMemory += stagingBuffer->getSize();
        m_peakUsedMemory = gp::math::max(m_usedMemory, m_peakUsedMemory);
    }

    return stagingBuffer;
}

void VulkanStagingManager::releaseBuffer(
    VulkanSyncPointRef syncPoint, VulkanStagingBuffer*& stagingBuffer, const gp::UInt32 currentFrame
) noexcept
{
    if (stagingBuffer == nullptr)
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    for (gp::USize index = 0u; index < m_usedStagingBuffers.size(); ++index)
    {
        if (m_usedStagingBuffers[index] == stagingBuffer)
        {
            const gp::USize lastIndex = m_usedStagingBuffers.size() - 1u;
            if (index != lastIndex)
            {
                m_usedStagingBuffers[index] = m_usedStagingBuffers[lastIndex];
            }
            m_usedStagingBuffers.popBack();
            break;
        }
    }

    if (syncPoint && !syncPoint->isComplete())
    {
        bool added = false;
        for (gp::USize index = 0u; index < m_pendingFreeStagingBuffers.size(); ++index)
        {
            if (m_pendingFreeStagingBuffers[index].syncPoint.get() == syncPoint.get())
            {
                m_pendingFreeStagingBuffers[index].buffers.pushBack(stagingBuffer);
                added = true;
                break;
            }
        }
        if (!added)
        {
            PendingFreeBatch batch{};
            batch.syncPoint = std::move(syncPoint);
            batch.buffers.pushBack(stagingBuffer);
            m_pendingFreeStagingBuffers.pushBack(std::move(batch));
        }
    }
    else
    {
        m_freeStagingBuffers.pushBack(FreeEntry{ stagingBuffer, currentFrame });
    }

    stagingBuffer = nullptr;
}

void VulkanStagingManager::processPendingFree(
    const bool immediately, const bool freeToOS, const gp::UInt32 currentFrame
) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    processPendingFreeLocked(immediately, freeToOS, currentFrame);
}

void VulkanStagingManager::processPendingFreeLocked(
    const bool immediately, const bool freeToOS, const gp::UInt32 currentFrame
) noexcept
{
    const gp::USize numOriginalFreeBuffers = m_freeStagingBuffers.size();

    gp::USize writeIndex = 0u;
    for (gp::USize readIndex = 0u; readIndex < m_pendingFreeStagingBuffers.size(); ++readIndex)
    {
        PendingFreeBatch& batch = m_pendingFreeStagingBuffers[readIndex];
        if (immediately || !batch.syncPoint || batch.syncPoint->isComplete())
        {
            for (gp::USize bufIdx = 0u; bufIdx < batch.buffers.size(); ++bufIdx)
            {
                m_freeStagingBuffers.pushBack(FreeEntry{ batch.buffers[bufIdx], currentFrame });
            }
        }
        else
        {
            if (writeIndex != readIndex)
            {
                m_pendingFreeStagingBuffers[writeIndex] = std::move(batch);
            }
            ++writeIndex;
        }
    }
    m_pendingFreeStagingBuffers.resize(writeIndex);

    if (freeToOS)
    {
        const gp::UInt32 frameLag = getDeletionFrameLag();
        const gp::USize checkCount = immediately ? m_freeStagingBuffers.size() : numOriginalFreeBuffers;
        for (gp::IntPtr index = static_cast<gp::IntPtr>(checkCount) - 1; index >= 0; --index)
        {
            const gp::USize uIndex = static_cast<gp::USize>(index);
            FreeEntry& entry = m_freeStagingBuffers[uIndex];
            const bool expired = (currentFrame >= entry.frameNumber) && ((currentFrame - entry.frameNumber) > frameLag);
            if (immediately || expired)
            {
                if (entry.stagingBuffer != nullptr)
                {
                    const gp::UInt64 bufSize = entry.stagingBuffer->getSize();
                    m_usedMemory = (m_usedMemory >= bufSize) ? (m_usedMemory - bufSize) : 0u;
                    entry.stagingBuffer->destroy();
                    delete entry.stagingBuffer;
                }

                const gp::USize lastIndex = m_freeStagingBuffers.size() - 1u;
                if (uIndex != lastIndex)
                {
                    m_freeStagingBuffers[uIndex] = m_freeStagingBuffers[lastIndex];
                }
                m_freeStagingBuffers.popBack();
            }
        }
    }
}

void VulkanStagingManager::getMemoryDump(VulkanResourceHeapStats& outStats) const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);

    for (gp::USize index = 0u; index < m_usedStagingBuffers.size(); ++index)
    {
        const VulkanStagingBuffer* const buffer = m_usedStagingBuffers[index];
        if (buffer != nullptr)
        {
            outStats.bufferAllocations += 1u;
            outStats.usedBufferMemory += buffer->getSize();
            outStats.totalMemory += buffer->getSize();
            outStats.memoryFlags |= buffer->getMemoryReadFlags() | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        }
    }

    for (gp::USize batchIdx = 0u; batchIdx < m_pendingFreeStagingBuffers.size(); ++batchIdx)
    {
        const PendingFreeBatch& batch = m_pendingFreeStagingBuffers[batchIdx];
        for (gp::USize bufIdx = 0u; bufIdx < batch.buffers.size(); ++bufIdx)
        {
            const VulkanStagingBuffer* const buffer = batch.buffers[bufIdx];
            if (buffer != nullptr)
            {
                outStats.bufferAllocations += 1u;
                outStats.usedBufferMemory += buffer->getSize();
                outStats.totalMemory += buffer->getSize();
                outStats.memoryFlags |= buffer->getMemoryReadFlags() | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            }
        }
    }

    for (gp::USize index = 0u; index < m_freeStagingBuffers.size(); ++index)
    {
        const VulkanStagingBuffer* const buffer = m_freeStagingBuffers[index].stagingBuffer;
        if (buffer != nullptr)
        {
            outStats.bufferAllocations += 1u;
            outStats.totalMemory += buffer->getSize();
            outStats.memoryFlags |= buffer->getMemoryReadFlags() | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        }
    }
}

gp::USize VulkanStagingManager::getUsedBufferCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_usedStagingBuffers.size();
}

gp::USize VulkanStagingManager::getPendingFreeBufferCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    gp::USize count = 0u;
    for (gp::USize index = 0u; index < m_pendingFreeStagingBuffers.size(); ++index)
    {
        count += m_pendingFreeStagingBuffers[index].buffers.size();
    }
    return count;
}

gp::USize VulkanStagingManager::getFreeBufferCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_freeStagingBuffers.size();
}

gp::UInt64 VulkanStagingManager::getUsedMemory() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_usedMemory;
}

gp::UInt64 VulkanStagingManager::getPeakUsedMemory() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_peakUsedMemory;
}

}   // namespace gp::rhi::vulkan
