// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanTempBlockAllocator.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "threading/sync/SharedLock.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarTempBlockSizeKB(
    "r.Vulkan.TempBlockSizeKB",
    4096,
    "Size in KB of the temporary blocks used for single use uniform buffer allocations and copies (default: 4096KB).",
    gp::console::ConsoleVarFlags::ReadOnly
);

/// @brief Rounds `value` up to `alignment` (a power of two).
/// @param[in] value Value to align.
/// @param[in] alignment Power-of-two alignment.
/// @return Aligned value.
[[nodiscard]] constexpr gp::UInt32 alignUp(const gp::UInt32 value, const gp::UInt32 alignment) noexcept
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

}   // namespace

VulkanTempBlockAllocator::VulkanTempBlockAllocator(
    VulkanMemoryManager& memoryManager,
    const gp::UInt32 blockSize,
    const gp::UInt32 blockAlignment,
    const VkBufferUsageFlags bufferUsage
) noexcept
    : m_memoryManager(memoryManager)
    , m_blockSize(blockSize)
    , m_blockAlignment(gp::math::max<gp::UInt32>(blockAlignment, 1u))
    , m_bufferUsage(bufferUsage)
{
    GP_CHECKF((m_blockAlignment & (m_blockAlignment - 1u)) == 0u, "Temp block alignment must be a power of two.");
    m_currentBlock = allocateBlock();
}

VulkanTempBlockAllocator::~VulkanTempBlockAllocator() noexcept
{
    for (gp::USize index = 0u; index < m_availableBlocks.size(); ++index)
    {
        destroyBlock(m_availableBlocks[index]);
    }
    m_availableBlocks.clear();

    for (gp::USize index = 0u; index < m_busyBlocks.size(); ++index)
    {
        destroyBlock(m_busyBlocks[index]);
    }
    m_busyBlocks.clear();

    destroyBlock(m_currentBlock);
    m_currentBlock = nullptr;
}

gp::UInt32 VulkanTempBlockAllocator::computeDefaultBlockAlignment(const VulkanDevice& device) noexcept
{
    gp::UInt32 alignment =
        gp::math::max<gp::UInt32>(static_cast<gp::UInt32>(device.getLimits().minUniformBufferOffsetAlignment), 16u);
    if (device.getOptionalExtensions().hasRayTracingPipeline != 0u)
    {
        alignment = gp::math::max<gp::UInt32>(
            device.getOptionalExtensionProperties().rayTracingPipelineProps.shaderGroupBaseAlignment, alignment
        );
    }
    return alignment;
}

VkBufferUsageFlags VulkanTempBlockAllocator::computeDefaultBufferUsage(const VulkanDevice& device) noexcept
{
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (device.getOptionalExtensions().hasBufferDeviceAddress != 0u)
    {
        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    if (device.getOptionalExtensions().hasRayTracingPipeline != 0u)
    {
        usage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    }
    return usage;
}

gp::UInt32 VulkanTempBlockAllocator::getConfiguredBlockSize() noexcept
{
    const gp::Int32 sizeKB = g_cvarTempBlockSizeKB.getValueOnAnyThread();
    return (sizeKB > 0) ? static_cast<gp::UInt32>(sizeKB) * 1024u : kDefaultBlockSize;
}

gp::UInt8* VulkanTempBlockAllocator::allocate(
    const gp::UInt32 size,
    const gp::UInt32 alignment,
    const VulkanSyncPointRef* const contextSyncPoint,
    VulkanAllocation& outAllocation,
    VkDescriptorAddressInfoEXT* const outDescriptorAddressInfo
) noexcept
{
    GP_CHECKF(alignment <= m_blockAlignment, "Temp block allocation alignment exceeds the block alignment.");
    static_cast<void>(alignment);

    const InternalAllocation alloc = allocateInternal(size, contextSyncPoint);
    if (alloc.block == nullptr)
    {
        return nullptr;
    }

    outAllocation = VulkanAllocation{};
    outAllocation.reference(alloc.block->allocation);
    outAllocation.vulkanHandle = reinterpret_cast<gp::UInt64>(alloc.block->buffer);
    outAllocation.size = size;
    outAllocation.offset += alloc.offset;
    outAllocation.isTempBlockAllocator = (contextSyncPoint == nullptr) ? 1u : 0u;

    if (outDescriptorAddressInfo != nullptr)
    {
        *outDescriptorAddressInfo =
            zeroVulkanStruct<VkDescriptorAddressInfoEXT>(VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT);
        outDescriptorAddressInfo->address = alloc.block->bufferAddress + alloc.offset;
        outDescriptorAddressInfo->range = size;
    }

    return alloc.block->mappedPointer + alloc.offset;
}

gp::UInt8* VulkanTempBlockAllocator::allocate(
    const gp::UInt32 size,
    const VulkanSyncPointRef& contextSyncPoint,
    VkDescriptorBufferBindingInfoEXT& outBindingInfo,
    VkDeviceSize& outOffset
) noexcept
{
    const InternalAllocation alloc = allocateInternal(size, &contextSyncPoint);
    if (alloc.block == nullptr)
    {
        return nullptr;
    }

    outBindingInfo =
        zeroVulkanStruct<VkDescriptorBufferBindingInfoEXT>(VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT);
    outBindingInfo.address = alloc.block->bufferAddress;
    outBindingInfo.usage = m_bufferUsage;
    outOffset = alloc.offset;
    return alloc.block->mappedPointer + alloc.offset;
}

gp::UInt8* VulkanTempBlockAllocator::allocate(
    const gp::UInt32 size, const VulkanSyncPointRef& contextSyncPoint, VkStridedDeviceAddressRegionKHR& outRegion
) noexcept
{
    const InternalAllocation alloc = allocateInternal(size, &contextSyncPoint);
    if (alloc.block == nullptr)
    {
        return nullptr;
    }

    outRegion.deviceAddress = alloc.block->bufferAddress + alloc.offset;
    outRegion.size = size;
    outRegion.stride = outRegion.size;
    return alloc.block->mappedPointer + alloc.offset;
}

void VulkanTempBlockAllocator::freeReference(const VulkanAllocation& allocation) noexcept
{
    gp::thread::SharedLock<gp::thread::SharedMutex> lock(m_lock);

    if ((m_currentBlock != nullptr) && isAllocationFromBlock(allocation, *m_currentBlock))
    {
        m_currentBlock->references.fetchSub(1u);
        return;
    }

    for (gp::USize index = 0u; index < m_busyBlocks.size(); ++index)
    {
        VulkanTempMemoryBlock* const block = m_busyBlocks[index];
        if (isAllocationFromBlock(allocation, *block))
        {
            GP_CHECK(block->references.load() > 0u);
            block->references.fetchSub(1u);
            return;
        }
    }

    GP_CHECKF(false, "Freed allocation's block was not found in the temp block allocator.");
}

void VulkanTempBlockAllocator::updateBlocks() noexcept
{
    gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_lock);

    for (gp::USize reverse = m_busyBlocks.size(); reverse > 0u; --reverse)
    {
        const gp::USize index = reverse - 1u;
        VulkanTempMemoryBlock* const block = m_busyBlocks[index];

        bool isBlockReady = (block->references.load() == 0u);
        if (isBlockReady)
        {
            for (gp::USize syncIndex = 0u; syncIndex < block->syncPoints.size(); ++syncIndex)
            {
                const VulkanSyncPointRef& syncPoint = block->syncPoints[syncIndex];
                if (syncPoint && !syncPoint->isComplete())
                {
                    isBlockReady = false;
                    break;
                }
            }
        }

        if (isBlockReady)
        {
            block->syncPoints.clear();
            block->currentOffset.store(0u);
            m_busyBlocks.removeAtSwap(index);
            m_availableBlocks.pushBack(block);
        }
    }
}

gp::USize VulkanTempBlockAllocator::getBusyBlockCount() const noexcept
{
    gp::thread::SharedLock<gp::thread::SharedMutex> lock(m_lock);
    return m_busyBlocks.size();
}

gp::USize VulkanTempBlockAllocator::getAvailableBlockCount() const noexcept
{
    gp::thread::SharedLock<gp::thread::SharedMutex> lock(m_lock);
    return m_availableBlocks.size();
}

gp::UInt32 VulkanTempBlockAllocator::getCurrentBlockReferences() const noexcept
{
    gp::thread::SharedLock<gp::thread::SharedMutex> lock(m_lock);
    return (m_currentBlock != nullptr) ? m_currentBlock->references.load() : 0u;
}

VulkanTempMemoryBlock* VulkanTempBlockAllocator::allocateBlock() noexcept
{
    VulkanDevice& device = m_memoryManager.getDevice();

    VulkanTempMemoryBlock* const block = new VulkanTempMemoryBlock();
    block->buffer = createVulkanBuffer(device, m_blockSize, m_bufferUsage);
    if (block->buffer == VK_NULL_HANDLE)
    {
        GP_LOG_ERROR(LogVulkanRHI, "VulkanTempBlockAllocator failed to create a {} byte block buffer.", m_blockSize);
        delete block;
        return nullptr;
    }

    const VulkanAllocationFlags allocFlags = VulkanAllocationFlags::HostVisible | VulkanAllocationFlags::PreferBAR |
                                             VulkanAllocationFlags::AutoBind | VulkanAllocationFlags::Dedicated;
    if (!m_memoryManager.allocateBufferMemory(
            block->allocation, block->buffer, allocFlags, "VulkanTempBlockAllocator", m_blockAlignment
        ))
    {
        GP_LOG_ERROR(
            LogVulkanRHI, "VulkanTempBlockAllocator failed to allocate {} bytes of block memory.", m_blockSize
        );
        vkDestroyBuffer(device.getHandle(), block->buffer, GP_VULKAN_CPU_ALLOCATOR);
        delete block;
        return nullptr;
    }

    block->mappedPointer = static_cast<gp::UInt8*>(block->allocation.getMappedPointer(m_memoryManager));

    const bool wantsAddress = (m_bufferUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0u;
    if (wantsAddress && (device.getOptionalExtensions().hasBufferDeviceAddress != 0u) &&
        (dynamic_api::vkGetBufferDeviceAddressKHR != nullptr))
    {
        VkBufferDeviceAddressInfo addressInfo =
            zeroVulkanStruct<VkBufferDeviceAddressInfo>(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
        addressInfo.buffer = block->buffer;
        block->bufferAddress = dynamic_api::vkGetBufferDeviceAddressKHR(device.getHandle(), &addressInfo);
    }

    return block;
}

void VulkanTempBlockAllocator::destroyBlock(VulkanTempMemoryBlock* const block) noexcept
{
    if (block == nullptr)
    {
        return;
    }

    if (block->buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_memoryManager.getDevice().getHandle(), block->buffer, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_memoryManager.freeVulkanAllocation(block->allocation, VulkanFreeFlags::DontDefer);
    delete block;
}

VulkanTempBlockAllocator::InternalAllocation VulkanTempBlockAllocator::allocateInternal(
    const gp::UInt32 size, const VulkanSyncPointRef* const contextSyncPoint
) noexcept
{
    const gp::UInt32 alignedSize = alignUp(size, m_blockAlignment);
    GP_CHECKF(
        alignedSize < m_blockSize,
        "Requested size of {} ({} aligned) is too large for a temp block of {} bytes.",
        size,
        alignedSize,
        m_blockSize
    );

    // Parallel path: bump the current block when it already tracks this context (or for referenced allocations).
    {
        gp::thread::SharedLock<gp::thread::SharedMutex> lock(m_lock);
        if (m_currentBlock == nullptr)
        {
            return InternalAllocation{};
        }

        if ((contextSyncPoint == nullptr) || hasSyncPoint(*m_currentBlock, *contextSyncPoint))
        {
            const gp::UInt32 allocOffset = m_currentBlock->currentOffset.fetchAdd(alignedSize);
            if (allocOffset + size < m_blockSize)
            {
                if (contextSyncPoint == nullptr)
                {
                    m_currentBlock->references.fetchAdd(1u);
                }
                return InternalAllocation{ m_currentBlock, allocOffset };
            }
        }
    }

    // Locked path: register a new context or switch to another block.
    gp::thread::UniqueLock<gp::thread::SharedMutex> lock(m_lock);

    // Another thread may have switched blocks while this one waited for the lock.
    gp::UInt32 allocOffset = m_currentBlock->currentOffset.fetchAdd(alignedSize);
    if (allocOffset + size < m_blockSize)
    {
        if (contextSyncPoint != nullptr)
        {
            addUniqueSyncPoint(*m_currentBlock, *contextSyncPoint);
        }
        else
        {
            m_currentBlock->references.fetchAdd(1u);
        }
        return InternalAllocation{ m_currentBlock, allocOffset };
    }

    VulkanTempMemoryBlock* nextBlock = nullptr;
    if (!m_availableBlocks.isEmpty())
    {
        nextBlock = m_availableBlocks.back();
        m_availableBlocks.popBack();
    }
    else
    {
        nextBlock = allocateBlock();
    }

    if (nextBlock == nullptr)
    {
        return InternalAllocation{};
    }

    m_busyBlocks.pushBack(m_currentBlock);
    m_currentBlock = nextBlock;

    if (contextSyncPoint != nullptr)
    {
        addUniqueSyncPoint(*m_currentBlock, *contextSyncPoint);
    }
    else
    {
        m_currentBlock->references.fetchAdd(1u);
    }

    allocOffset = m_currentBlock->currentOffset.fetchAdd(alignedSize);
    GP_CHECK(allocOffset == 0u);
    return InternalAllocation{ m_currentBlock, allocOffset };
}

bool VulkanTempBlockAllocator::hasSyncPoint(
    const VulkanTempMemoryBlock& block, const VulkanSyncPointRef& syncPoint
) noexcept
{
    for (gp::USize index = 0u; index < block.syncPoints.size(); ++index)
    {
        if (block.syncPoints[index].get() == syncPoint.get())
        {
            return true;
        }
    }
    return false;
}

void VulkanTempBlockAllocator::addUniqueSyncPoint(
    VulkanTempMemoryBlock& block, const VulkanSyncPointRef& syncPoint
) noexcept
{
    if (!hasSyncPoint(block, syncPoint))
    {
        block.syncPoints.pushBack(syncPoint);
    }
}

bool VulkanTempBlockAllocator::isAllocationFromBlock(
    const VulkanAllocation& allocation, const VulkanTempMemoryBlock& block
) noexcept
{
    return (allocation.vulkanHandle == reinterpret_cast<gp::UInt64>(block.buffer)) &&
           (allocation.allocatorIndex == block.allocation.allocatorIndex) &&
           (allocation.allocationIndex == block.allocation.allocationIndex);
}

}   // namespace gp::rhi::vulkan
