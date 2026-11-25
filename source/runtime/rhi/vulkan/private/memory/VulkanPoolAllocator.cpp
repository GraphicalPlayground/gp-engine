// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanPoolAllocator.hpp"
#include "allocators/MemoryPool.hpp"
#include "allocators/PoolAllocationData.hpp"
#include "allocators/PoolAllocator.hpp"
#include "allocators/PoolAllocatorEnums.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "memory/VulkanDeviceMemory.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/RecursiveMutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarPoolDefragMode(
    "r.Vulkan.PoolAllocator.Defrag",
    static_cast<gp::Int32>(VulkanPoolDefragMode::All),
    "Relocate pooled allocations to compact device-local memory pools (0=Off, 1=All, 2=ImagesOnly, 3=BuffersOnly).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::thread::Atomic<gp::UInt32> g_bufferHandleIdCounter{ 0u };

/// @brief Rounds `value` up to the next power of two (returns `1` for `0` or `1`).
/// @param[in] value Input 32-bit unsigned integer.
/// @return Power-of-two upper bound as 64-bit integer.
[[nodiscard]] constexpr gp::UInt64 roundUpToPowerOfTwo(const gp::UInt32 value) noexcept
{
    if (value <= 1u)
    {
        return 1u;
    }

    gp::UInt64 power = 1u;
    while (power < static_cast<gp::UInt64>(value))
    {
        power <<= 1u;
    }
    return power;
}

}   // namespace

VulkanPoolDefragMode getVulkanPoolDefragMode() noexcept
{
    const gp::Int32 rawValue = g_cvarPoolDefragMode.getValueOnAnyThread(true);
    const gp::Int32 clamped = gp::math::clamp<gp::Int32>(
        rawValue,
        static_cast<gp::Int32>(VulkanPoolDefragMode::Off),
        static_cast<gp::Int32>(VulkanPoolDefragMode::BuffersOnly)
    );
    return static_cast<VulkanPoolDefragMode>(clamped);
}

VulkanMemoryPool::VulkanMemoryPool(
    VulkanDevice& device,
    VulkanDeviceMemoryManager& deviceMemoryManager,
    const VulkanPoolConfig& config,
    const gp::Int16 poolIndex,
    const gp::UInt64 poolSize,
    const gp::UInt32 poolAlignment,
    const gp::rhi::allocator::PoolFreeListOrder freeListOrder
) noexcept
    : gp::rhi::allocator::MemoryPool(poolIndex, poolSize, poolAlignment, config.resourceTypes, freeListOrder)
    , m_device(device)
    , m_deviceMemoryManager(deviceMemoryManager)
    , m_config(config)
    , m_memoryTypeIndex(config.memoryTypeIndex)
{}

VulkanMemoryPool::~VulkanMemoryPool() noexcept
{
    releaseNativeResources();
}

void VulkanMemoryPool::init() noexcept
{
    if (!m_device.isValid())
    {
        return;
    }

    const bool isManualSubAllocation =
        (m_config.strategy == gp::rhi::allocator::ResourceAllocationStrategy::ManualSubAllocation);
    gp::Float32 priority = kVulkanMemoryHighestPriority;
    VkDeviceSize allocationSize = m_poolSize;

    if (isManualSubAllocation)
    {
        m_buffer = createVulkanBuffer(m_device, m_poolSize, m_config.bufferUsageFlags);
        if (m_buffer == VK_NULL_HANDLE || dynamic_api::vkGetBufferMemoryRequirements == nullptr)
        {
            releaseNativeResources();
            return;
        }

        VkMemoryRequirements memReqs{};
        vkGetBufferMemoryRequirements(m_device.getHandle(), m_buffer, &memReqs);

        if (m_deviceMemoryManager.getMemoryTypeFromProperties(
                memReqs.memoryTypeBits, m_config.memoryPropertyFlags, &m_memoryTypeIndex
            ) != VK_SUCCESS ||
            memReqs.alignment > m_poolAlignment)
        {
            releaseNativeResources();
            return;
        }

        allocationSize = gp::math::max<VkDeviceSize>(m_poolSize, memReqs.size);
        priority = VulkanMemoryManager::calculateBufferPriority(m_config.bufferUsageFlags);
    }

    m_memoryAllocation = m_deviceMemoryManager.alloc(true, allocationSize, m_memoryTypeIndex, nullptr, priority, false);
    if (m_memoryAllocation == nullptr)
    {
        releaseNativeResources();
        return;
    }

    if (m_buffer != VK_NULL_HANDLE)
    {
        if (dynamic_api::vkBindBufferMemory == nullptr)
        {
            releaseNativeResources();
            return;
        }

        const VkResult bindResult =
            vkBindBufferMemory(m_device.getHandle(), m_buffer, m_memoryAllocation->getHandle(), 0u);
        if (bindResult != VK_SUCCESS)
        {
            GP_LOG_ERROR(
                LogVulkanRHI, "vkBindBufferMemory failed for pool buffer ({}).", vulkanResultToString(bindResult)
            );
            releaseNativeResources();
            return;
        }
    }

    if (m_memoryAllocation->canBeMapped())
    {
        [[maybe_unused]] void* const mapped = m_memoryAllocation->map(allocationSize, 0u);
    }

    m_bufferId = generateVulkanBufferHandleId();

    gp::rhi::allocator::MemoryPool::init();
}

void VulkanMemoryPool::destroy() noexcept
{
    gp::rhi::allocator::MemoryPool::destroy();
    releaseNativeResources();
}

void VulkanMemoryPool::releaseNativeResources() noexcept
{
    if (m_buffer != VK_NULL_HANDLE && m_device.isValid() && dynamic_api::vkDestroyBuffer != nullptr)
    {
        vkDestroyBuffer(m_device.getHandle(), m_buffer, GP_VULKAN_CPU_ALLOCATOR);
        m_buffer = VK_NULL_HANDLE;
    }

    if (m_memoryAllocation != nullptr)
    {
        m_deviceMemoryManager.free(m_memoryAllocation);
    }
}

void* VulkanMemoryPool::getMappedPointer() const noexcept
{
    return (m_memoryAllocation != nullptr) ? m_memoryAllocation->getMappedPointer() : nullptr;
}

void VulkanMemoryPool::flushMappedMemory(const VkDeviceSize offset, const VkDeviceSize size) const noexcept
{
    if (m_memoryAllocation != nullptr)
    {
        flushMappedMemoryRange(m_device, *m_memoryAllocation, offset, size);
    }
}

void VulkanMemoryPool::invalidateMappedMemory(const VkDeviceSize offset, const VkDeviceSize size) const noexcept
{
    if (m_memoryAllocation != nullptr)
    {
        invalidateMappedMemoryRange(m_device, *m_memoryAllocation, offset, size);
    }
}

VulkanPoolAllocator::VulkanPoolAllocator(
    VulkanDevice& device,
    VulkanDeviceMemoryManager& deviceMemoryManager,
    const VulkanPoolConfig& config,
    const gp::UInt32 allocatorIndex
) noexcept
    : gp::rhi::allocator::PoolAllocator(
          config.poolSize,
          config.poolAlignment,
          config.maxAllocationSize,
          gp::rhi::allocator::PoolFreeListOrder::SortBySize,
          config.isDeviceLocal,
          gp::rhi::allocator::PoolAllocationStrategy::Immediate,
          1
      )
    , m_device(device)
    , m_deviceMemoryManager(deviceMemoryManager)
    , m_config(config)
    , m_allocatorIndex(allocatorIndex)
{
    GP_CHECKF(
        config.maxAllocationSize <= config.poolSize,
        "VulkanPoolAllocator max allocation size exceeds configured pool size."
    );
    GP_CHECKF(
        config.strategy == gp::rhi::allocator::ResourceAllocationStrategy::PlacedResource ||
            config.getResourceType() == VulkanResourceType::Buffer,
        "Image pools must use PlacedResource strategy."
    );

    initialize(config.resourceTypes);
}

VulkanPoolAllocator::~VulkanPoolAllocator() noexcept
{
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_blocksMutex);
        for (gp::USize index = 0u; index < m_blocks.size(); ++index)
        {
            delete m_blocks[index].data;
            m_blocks[index].data = nullptr;
        }
        m_blocks.clear();
        m_freeBlockIndices.clear();
        m_liveBlockCount = 0u;
    }

    destroy();
}

bool VulkanPoolAllocator::supportsAllocation(const VulkanPoolConfig& config) const noexcept
{
    if (m_config.strategy != config.strategy || m_config.resourceTypes != config.resourceTypes)
    {
        return false;
    }

    if (m_config.strategy == gp::rhi::allocator::ResourceAllocationStrategy::ManualSubAllocation)
    {
        return ((m_config.bufferUsageFlags & config.bufferUsageFlags) == config.bufferUsageFlags) &&
               ((m_config.memoryPropertyFlags & config.memoryPropertyFlags) == config.memoryPropertyFlags);
    }

    return m_config.memoryTypeIndex == config.memoryTypeIndex;
}

bool VulkanPoolAllocator::tryAllocate(
    VulkanAllocation& outAllocation,
    const gp::UInt32 size,
    const gp::UInt32 alignment,
    const VulkanAllocationType allocType,
    const VulkanAllocationMetaType metaType
) noexcept
{
    GP_CHECK(outAllocation.getType() == VulkanAllocationType::Empty);

    if (size == 0u || size > m_maxAllocationSize || alignment > m_poolAlignment)
    {
        return false;
    }

    gp::UniquePtr<gp::rhi::allocator::PoolAllocationData> allocationData(new gp::rhi::allocator::PoolAllocationData());
    allocationData->reset();

    if (!tryAllocateInternal(size, alignment, m_config.resourceTypes, *allocationData))
    {
        return false;
    }

    gp::rhi::allocator::PoolAllocationData* const block = allocationData.release();

    gp::UInt32 bufferIdForAlloc = 0u;
    VkBuffer bufferForAlloc = VK_NULL_HANDLE;
    {
        gp::thread::UniqueLock<gp::thread::RecursiveMutex> poolLock(m_mutex);
        const gp::Int16 blockPoolIdx = block->getPoolIndex();
        const VulkanMemoryPool* const pool =
            (blockPoolIdx >= 0 && static_cast<gp::USize>(blockPoolIdx) < m_pools.size())
                ? static_cast<const VulkanMemoryPool*>(m_pools[static_cast<gp::USize>(blockPoolIdx)])
                : nullptr;
        if (pool != nullptr)
        {
            bufferIdForAlloc = pool->getBufferId();
            bufferForAlloc = pool->getBufferHandle();
        }
        block->unlock();
    }

    gp::UInt32 blockIndex = 0u;
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_blocksMutex);
        if (!m_freeBlockIndices.isEmpty())
        {
            blockIndex = m_freeBlockIndices.back();
            m_freeBlockIndices.popBack();
            m_blocks[blockIndex].data = block;
        }
        else
        {
            blockIndex = static_cast<gp::UInt32>(m_blocks.size());
            m_blocks.pushBack(BlockSlot{ block });
        }
        ++m_liveBlockCount;
    }

    outAllocation.init(
        allocType,
        metaType,
        reinterpret_cast<gp::UInt64>(bufferForAlloc),
        size,
        block->getOffset(),
        m_allocatorIndex,
        blockIndex,
        bufferIdForAlloc
    );
    outAllocation.isPoolAllocated = 1u;
    return true;
}

void VulkanPoolAllocator::free(VulkanAllocation& allocation) noexcept
{
    freeBatch(gp::VectorView<VulkanAllocation>(&allocation, 1u));
}

void VulkanPoolAllocator::freeBatch(gp::VectorView<VulkanAllocation> allocations) noexcept
{
    gp::Vector<gp::rhi::allocator::PoolAllocationData*> nodes{};
    nodes.reserve(allocations.size());

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_blocksMutex);
        for (gp::USize index = 0u; index < allocations.size(); ++index)
        {
            const VulkanAllocation& allocation = allocations[index];
            GP_CHECK(allocation.isPoolAllocated != 0u);
            GP_CHECK(allocation.allocatorIndex == m_allocatorIndex);

            if (allocation.allocationIndex < m_blocks.size() && m_blocks[allocation.allocationIndex].data != nullptr)
            {
                nodes.pushBack(m_blocks[allocation.allocationIndex].data);
                m_blocks[allocation.allocationIndex].data = nullptr;
                m_freeBlockIndices.pushBack(allocation.allocationIndex);
                if (m_liveBlockCount > 0u)
                {
                    --m_liveBlockCount;
                }
            }
        }
    }

    {
        gp::thread::UniqueLock<gp::thread::RecursiveMutex> poolLock(m_mutex);
        for (gp::USize index = 0u; index < nodes.size(); ++index)
        {
            gp::rhi::allocator::PoolAllocationData* const node = nodes[index];
            if (node == nullptr)
            {
                continue;
            }

            const gp::Int16 poolIndex = node->getPoolIndex();
            gp::rhi::allocator::PoolAllocationData releasedData{};
            releasedData.moveFrom(*node, true);
            deallocateInternal(releasedData);

            if (poolIndex >= 0 && static_cast<gp::USize>(poolIndex) < m_pools.size() &&
                m_pools[static_cast<gp::USize>(poolIndex)] != nullptr &&
                m_pools[static_cast<gp::USize>(poolIndex)]->isEmpty())
            {
                m_hasEmptyPools.store(true, gp::memory::MemoryOrder::Relaxed);
            }
        }
    }

    for (gp::USize index = 0u; index < nodes.size(); ++index)
    {
        delete nodes[index];
    }
}

void VulkanPoolAllocator::setAllocationOwner(
    const VulkanAllocation& allocation, gp::rhi::allocator::PoolResource* const owner
) noexcept
{
    if (allocation.isPoolAllocated == 0u || allocation.allocatorIndex != m_allocatorIndex || owner == nullptr)
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_blocksMutex);
    if (allocation.allocationIndex < m_blocks.size() && m_blocks[allocation.allocationIndex].data != nullptr)
    {
        m_blocks[allocation.allocationIndex].data->setOwner(owner);
    }
}

void VulkanPoolAllocator::transferAllocationOwner(
    const VulkanAllocation& allocation, gp::rhi::allocator::PoolResource* const newOwner
) noexcept
{
    if (allocation.isPoolAllocated == 0u || allocation.allocatorIndex != m_allocatorIndex || newOwner == nullptr)
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_blocksMutex);
    if (allocation.allocationIndex < m_blocks.size() && m_blocks[allocation.allocationIndex].data != nullptr)
    {
        gp::rhi::allocator::PoolAllocationData* const block = m_blocks[allocation.allocationIndex].data;
        if (block->getOwner() != nullptr)
        {
            block->transferOwner(newOwner);
        }
        else
        {
            block->setOwner(newOwner);
        }
    }
}

void VulkanPoolAllocator::clearAllocationOwner(const VulkanAllocation& allocation) noexcept
{
    if (allocation.isPoolAllocated == 0u || allocation.allocatorIndex != m_allocatorIndex)
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_blocksMutex);
    if (allocation.allocationIndex < m_blocks.size() && m_blocks[allocation.allocationIndex].data != nullptr)
    {
        gp::rhi::allocator::PoolAllocationData* const block = m_blocks[allocation.allocationIndex].data;
        if (block->getOwner() != nullptr)
        {
            block->clearOwner();
        }
    }
}

void VulkanPoolAllocator::releaseEmptyPools(
    const gp::UInt32 frameLag, const bool forceFree, const gp::UInt32 currentFrame
) noexcept
{
    if (!forceFree && !m_hasEmptyPools.load(gp::memory::MemoryOrder::Relaxed))
    {
        return;
    }

    gp::Vector<VulkanMemoryPool*> poolsToRelease{};
    {
        gp::thread::UniqueLock<gp::thread::RecursiveMutex> poolLock(m_mutex);
        bool emptyPoolsRemain = false;

        for (gp::USize poolIndex = 0u; poolIndex < m_pools.size(); ++poolIndex)
        {
            VulkanMemoryPool* const pool = static_cast<VulkanMemoryPool*>(m_pools[poolIndex]);
            if (pool == nullptr)
            {
                continue;
            }

            bool release = !pool->isValid() || (forceFree && pool->isEmpty());
            if (!release && !pool->isEmpty())
            {
                pool->setFrameEmptied(VulkanMemoryPool::kActiveFrameSentinel);
            }
            else if (!release)
            {
                if (pool->getFrameEmptied() == VulkanMemoryPool::kActiveFrameSentinel)
                {
                    pool->setFrameEmptied(currentFrame);
                }
                else
                {
                    release = (currentFrame >= pool->getFrameEmptied()) &&
                              ((currentFrame - pool->getFrameEmptied()) > frameLag);
                }
                emptyPoolsRemain = emptyPoolsRemain || !release;
            }

            if (release)
            {
                poolsToRelease.pushBack(pool);
                m_pools[poolIndex] = nullptr;
            }
        }

        m_hasEmptyPools.store(emptyPoolsRemain, gp::memory::MemoryOrder::Relaxed);
    }

    for (gp::USize index = 0u; index < poolsToRelease.size(); ++index)
    {
        VulkanMemoryPool* const pool = poolsToRelease[index];
        if (!pool->isValid())
        {
            pool->setPoolIndex(gp::rhi::allocator::MemoryPool::kInvalidPoolIndex);
        }
        delete pool;
    }
}

gp::UInt32 VulkanPoolAllocator::getEffectiveMemoryTypeIndex() noexcept
{
    if (m_config.strategy == gp::rhi::allocator::ResourceAllocationStrategy::PlacedResource)
    {
        return m_config.memoryTypeIndex;
    }

    gp::thread::UniqueLock<gp::thread::RecursiveMutex> poolLock(m_mutex);
    for (gp::USize index = 0u; index < m_pools.size(); ++index)
    {
        const VulkanMemoryPool* const pool = static_cast<const VulkanMemoryPool*>(m_pools[index]);
        if (pool != nullptr && pool->isValid())
        {
            return pool->getMemoryTypeIndex();
        }
    }

    return 0xFFFFFFFFu;
}

VulkanMemoryPool* VulkanPoolAllocator::getPool(const VulkanAllocation& allocation) noexcept
{
    if (allocation.isPoolAllocated == 0u || allocation.allocatorIndex != m_allocatorIndex)
    {
        return nullptr;
    }

    gp::Int16 poolIndex = gp::rhi::allocator::MemoryPool::kInvalidPoolIndex;
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_blocksMutex);
        if (allocation.allocationIndex < m_blocks.size() && m_blocks[allocation.allocationIndex].data != nullptr)
        {
            poolIndex = m_blocks[allocation.allocationIndex].data->getPoolIndex();
        }
    }

    if (poolIndex < 0)
    {
        return nullptr;
    }

    gp::thread::UniqueLock<gp::thread::RecursiveMutex> poolLock(m_mutex);
    const gp::USize uPoolIndex = static_cast<gp::USize>(poolIndex);
    return (uPoolIndex < m_pools.size()) ? static_cast<VulkanMemoryPool*>(m_pools[uPoolIndex]) : nullptr;
}

VulkanMemoryPool* VulkanPoolAllocator::getPoolForBlock(const gp::rhi::allocator::PoolAllocationData& block) noexcept
{
    const gp::Int16 poolIndex = block.getPoolIndex();
    if (poolIndex < 0)
    {
        return nullptr;
    }

    gp::thread::UniqueLock<gp::thread::RecursiveMutex> poolLock(m_mutex);
    const gp::USize uPoolIndex = static_cast<gp::USize>(poolIndex);
    return (uPoolIndex < m_pools.size()) ? static_cast<VulkanMemoryPool*>(m_pools[uPoolIndex]) : nullptr;
}

void VulkanPoolAllocator::getMemoryDump(VulkanResourceHeapStats& outStats) noexcept
{
    gp::UInt32 memoryAllocated = 0u;
    gp::UInt32 memoryUsed = 0u;
    gp::UInt32 memoryFree = 0u;
    gp::UInt32 memoryEndFree = 0u;
    gp::UInt32 alignmentWaste = 0u;
    gp::UInt32 allocatedPageCount = 0u;
    gp::UInt32 fullPageCount = 0u;
    updateMemoryStats(
        memoryAllocated, memoryUsed, memoryFree, memoryEndFree, alignmentWaste, allocatedPageCount, fullPageCount
    );

    outStats.totalMemory += memoryAllocated;
    outStats.pages += allocatedPageCount;

    const gp::USize liveBlockCount = getLiveBlockCount();
    if (isImagePool())
    {
        outStats.usedImageMemory += memoryUsed;
        outStats.imagePages += allocatedPageCount;
        outStats.imageAllocations += liveBlockCount;
    }
    else
    {
        outStats.usedBufferMemory += memoryUsed;
        outStats.bufferPages += allocatedPageCount;
        outStats.bufferAllocations += liveBlockCount;
    }
}

gp::USize VulkanPoolAllocator::getLiveBlockCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_blocksMutex);
    return m_liveBlockCount;
}

gp::rhi::allocator::MemoryPool* VulkanPoolAllocator::createNewPool(
    const gp::Int16 poolIndex,
    const gp::UInt32 minimumAllocationSize,
    [[maybe_unused]] const gp::rhi::allocator::PoolResourceTypes allocationResourceType
) noexcept
{
    gp::UInt64 newPoolSize = m_defaultPoolSize;
    if (minimumAllocationSize > newPoolSize)
    {
        newPoolSize = gp::math::min<gp::UInt64>(roundUpToPowerOfTwo(minimumAllocationSize), m_maxAllocationSize);
    }

    VulkanMemoryPool* const newPool = new VulkanMemoryPool(
        m_device, m_deviceMemoryManager, m_config, poolIndex, newPoolSize, m_poolAlignment, m_freeListOrder
    );
    newPool->init();

    if (!newPool->isValid())
    {
        GP_LOG_WARNING(
            LogVulkanRHI,
            "Failed to create {}-byte Vulkan memory pool for memory type {}; falling back to direct allocation.",
            newPoolSize,
            m_config.memoryTypeIndex
        );
    }

    m_hasEmptyPools.store(true, gp::memory::MemoryOrder::Relaxed);
    return newPool;
}

bool VulkanPoolAllocator::handleDefragRequest(
    [[maybe_unused]] const gp::rhi::allocator::ContextArray& contexts,
    [[maybe_unused]] gp::rhi::allocator::PoolAllocationData* const sourceBlock,
    gp::rhi::allocator::PoolAllocationData& tmpTargetBlock
) noexcept
{
    // Resource relocation callbacks are wired once concrete VulkanBuffer and VulkanTexture resources exist.
    // Release the reserved target range cleanly so no pool space is leaked.
    gp::rhi::allocator::PoolAllocationData targetToRelease{};
    targetToRelease.moveFrom(tmpTargetBlock, true);
    deallocateInternal(targetToRelease);
    return false;
}

gp::UInt32 generateVulkanBufferHandleId() noexcept
{
    return g_bufferHandleIdCounter.fetchAdd(1u, gp::memory::MemoryOrder::Relaxed) + 1u;
}

}   // namespace gp::rhi::vulkan
