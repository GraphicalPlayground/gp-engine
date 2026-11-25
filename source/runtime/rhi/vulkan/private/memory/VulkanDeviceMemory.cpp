// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanDeviceMemory.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/Memory.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarBudgetPercentageScale(
    "r.Vulkan.BudgetScale",
    100,
    "Percentage scaling of VK_EXT_memory_budget heap budgets in [0..100].",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarForceCoherentOperations(
    "r.Vulkan.ForceCoherentOperations",
    0,
    "When non-zero, forces flush and invalidate operations even on host-coherent memory.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarFakeMemoryLimitMB(
    "r.Vulkan.FakeMemoryLimit",
    0,
    "Artificially limits primary device-local heap allocations to N MB for testing (0 disables).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

/// @brief Rounds `value` up to the nearest multiple of `alignment` (supports arbitrary positive alignment).
/// @param[in] value Input byte count.
/// @param[in] alignment Positive alignment granularity in bytes.
/// @return Rounded-up byte count.
[[nodiscard]] constexpr VkDeviceSize alignArbitraryUp(const VkDeviceSize value, const VkDeviceSize alignment) noexcept
{
    if (alignment <= 1u)
    {
        return value;
    }
    const VkDeviceSize remainder = value % alignment;
    return (remainder == 0u) ? value : (value + (alignment - remainder));
}

}   // namespace

VulkanDeviceMemoryAllocation::~VulkanDeviceMemoryAllocation() noexcept
{
    GP_CHECKF(
        m_freedBySystem != 0u, "VulkanDeviceMemoryAllocation must be released via VulkanDeviceMemoryManager::free()."
    );
}

void* VulkanDeviceMemoryAllocation::map(const VkDeviceSize size, const VkDeviceSize offset) noexcept
{
    if (m_canBeMapped == 0u || m_deviceHandle == VK_NULL_HANDLE || m_handle == VK_NULL_HANDLE ||
        dynamic_api::vkMapMemory == nullptr)
    {
        return nullptr;
    }

    if (m_mappedPointer == nullptr)
    {
        if (size != VK_WHOLE_SIZE && (offset > m_size || size > (m_size - offset)))
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "VulkanDeviceMemoryAllocation::map() out of range (offset={}, size={}, allocSize={}).",
                offset,
                size,
                m_size
            );
            return nullptr;
        }

        const VkResult result = vkMapMemory(m_deviceHandle, m_handle, offset, size, 0u, &m_mappedPointer);
        if (result != VK_SUCCESS)
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkMapMemory failed with {}.", vulkanResultToString(result));
            m_mappedPointer = nullptr;
            return nullptr;
        }
    }

    return m_mappedPointer;
}

void VulkanDeviceMemoryAllocation::unmap() noexcept
{
    if (m_mappedPointer != nullptr && m_deviceHandle != VK_NULL_HANDLE && m_handle != VK_NULL_HANDLE &&
        dynamic_api::vkUnmapMemory != nullptr)
    {
        vkUnmapMemory(m_deviceHandle, m_handle);
        m_mappedPointer = nullptr;
    }
}

void VulkanDeviceMemoryAllocation::flushMappedMemory(const VkDeviceSize offset, const VkDeviceSize size) noexcept
{
    const bool forceCoherent = g_cvarForceCoherentOperations.getValueOnAnyThread(true) != 0;
    if (!isCoherent() || forceCoherent)
    {
        if (!isMapped() || m_deviceHandle == VK_NULL_HANDLE || m_handle == VK_NULL_HANDLE ||
            dynamic_api::vkFlushMappedMemoryRanges == nullptr)
        {
            return;
        }

        if (offset > m_size || (size != VK_WHOLE_SIZE && size > (m_size - offset)))
        {
            return;
        }

        VkMappedMemoryRange range = zeroVulkanStruct<VkMappedMemoryRange>(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
        range.memory = m_handle;
        range.offset = offset;
        range.size = size;

        const VkResult result = vkFlushMappedMemoryRanges(m_deviceHandle, 1u, &range);
        if (result != VK_SUCCESS)
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkFlushMappedMemoryRanges failed with {}.", vulkanResultToString(result));
        }
    }
}

void VulkanDeviceMemoryAllocation::invalidateMappedMemory(const VkDeviceSize offset, const VkDeviceSize size) noexcept
{
    const bool forceCoherent = g_cvarForceCoherentOperations.getValueOnAnyThread(true) != 0;
    if (!isCoherent() || forceCoherent)
    {
        if (!isMapped() || m_deviceHandle == VK_NULL_HANDLE || m_handle == VK_NULL_HANDLE ||
            dynamic_api::vkInvalidateMappedMemoryRanges == nullptr)
        {
            return;
        }

        if (offset > m_size || (size != VK_WHOLE_SIZE && size > (m_size - offset)))
        {
            return;
        }

        VkMappedMemoryRange range = zeroVulkanStruct<VkMappedMemoryRange>(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
        range.memory = m_handle;
        range.offset = offset;
        range.size = size;

        const VkResult result = vkInvalidateMappedMemoryRanges(m_deviceHandle, 1u, &range);
        if (result != VK_SUCCESS)
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkInvalidateMappedMemoryRanges failed with {}.", vulkanResultToString(result));
        }
    }
}

VkDeviceSize adjustToNonCoherentAtomSize(
    const VkDeviceSize requestedOffset,
    const VkDeviceSize requestedSize,
    const VkDeviceSize wholeAllocationSize,
    const VkDeviceSize nonCoherentAtomSize
) noexcept
{
    if (requestedOffset >= wholeAllocationSize || requestedSize == 0u)
    {
        return 0u;
    }

    if (requestedSize == VK_WHOLE_SIZE)
    {
        return VK_WHOLE_SIZE;
    }

    const VkDeviceSize maxRemaining = wholeAllocationSize - requestedOffset;
    const VkDeviceSize alignedSize = alignArbitraryUp(requestedSize, nonCoherentAtomSize);
    return gp::math::min<VkDeviceSize>(maxRemaining, alignedSize);
}

void flushMappedMemoryRange(
    const VulkanDevice& device,
    VulkanDeviceMemoryAllocation& allocation,
    const VkDeviceSize offset,
    const VkDeviceSize size
) noexcept
{
    const bool forceCoherent = g_cvarForceCoherentOperations.getValueOnAnyThread(true) != 0;
    if (!allocation.isCoherent() || forceCoherent)
    {
        const VkDeviceSize atomSize = gp::math::max<VkDeviceSize>(device.getLimits().nonCoherentAtomSize, 1u);
        const VkDeviceSize alignedOffset = (offset / atomSize) * atomSize;
        const VkDeviceSize adjustedSize =
            (size == VK_WHOLE_SIZE) ? ((alignedOffset < allocation.getSize()) ? VK_WHOLE_SIZE : 0u)
                                    : adjustToNonCoherentAtomSize(
                                          alignedOffset, size + (offset - alignedOffset), allocation.getSize(), atomSize
                                      );
        if (adjustedSize > 0u)
        {
            allocation.flushMappedMemory(alignedOffset, adjustedSize);
        }
    }
}

void invalidateMappedMemoryRange(
    const VulkanDevice& device,
    VulkanDeviceMemoryAllocation& allocation,
    const VkDeviceSize offset,
    const VkDeviceSize size
) noexcept
{
    const bool forceCoherent = g_cvarForceCoherentOperations.getValueOnAnyThread(true) != 0;
    if (!allocation.isCoherent() || forceCoherent)
    {
        const VkDeviceSize atomSize = gp::math::max<VkDeviceSize>(device.getLimits().nonCoherentAtomSize, 1u);
        const VkDeviceSize alignedOffset = (offset / atomSize) * atomSize;
        const VkDeviceSize adjustedSize =
            (size == VK_WHOLE_SIZE) ? ((alignedOffset < allocation.getSize()) ? VK_WHOLE_SIZE : 0u)
                                    : adjustToNonCoherentAtomSize(
                                          alignedOffset, size + (offset - alignedOffset), allocation.getSize(), atomSize
                                      );
        if (adjustedSize > 0u)
        {
            allocation.invalidateMappedMemory(alignedOffset, adjustedSize);
        }
    }
}

VulkanDeviceMemoryManager::VulkanDeviceMemoryManager() noexcept
{
    gp::memory::zeroMemory(&m_memoryBudget, sizeof(m_memoryBudget));
    gp::memory::zeroMemory(&m_memoryProperties, sizeof(m_memoryProperties));
}

VulkanDeviceMemoryManager::~VulkanDeviceMemoryManager() noexcept
{
    deinit();
}

void VulkanDeviceMemoryManager::init(VulkanDevice* const device) noexcept
{
    if (device == nullptr || !device->isValid())
    {
        return;
    }

    m_device = device;
    m_numAllocations = 0u;
    m_peakNumAllocations = 0u;

    updateMemoryProperties();

    m_primaryHeapIndex = -1;
    gp::UInt64 primaryHeapSize = 0u;
    gp::UInt32 nonLocalHeaps = 0u;

    for (gp::UInt32 index = 0u; index < m_memoryProperties.memoryHeapCount; ++index)
    {
        if ((m_memoryProperties.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u)
        {
            if (m_memoryProperties.memoryHeaps[index].size > primaryHeapSize)
            {
                m_primaryHeapIndex = static_cast<gp::Int32>(index);
                primaryHeapSize = m_memoryProperties.memoryHeaps[index].size;
            }
        }
        else
        {
            ++nonLocalHeaps;
        }
    }

    if (nonLocalHeaps == 0u)
    {
        m_primaryHeapIndex = -1;
        m_hasUnifiedMemory = true;
    }
    else
    {
        m_hasUnifiedMemory = false;
    }

    m_supportsMemoryless = false;
    for (gp::UInt32 index = 0u; index < m_memoryProperties.memoryTypeCount && !m_supportsMemoryless; ++index)
    {
        m_supportsMemoryless =
            (m_memoryProperties.memoryTypes[index].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0u;
    }

    m_heapInfos.clear();
    m_heapInfos.resize(m_memoryProperties.memoryHeapCount);
}

void VulkanDeviceMemoryManager::deinit() noexcept
{
    if (m_device == nullptr)
    {
        return;
    }

    trimMemory(true, 0u);

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        for (gp::USize index = 0u; index < m_heapInfos.size(); ++index)
        {
            if (!m_heapInfos[index].allocations.isEmpty())
            {
                GP_LOG_WARNING(
                    LogVulkanRHI,
                    "VulkanDeviceMemoryManager::deinit() found {} unfreed allocations on heap {}.",
                    m_heapInfos[index].allocations.size(),
                    index
                );
            }
        }
        m_heapInfos.clear();
        m_cachedBlocks.clear();
        m_numAllocations = 0u;
    }

    m_device = nullptr;
}

void VulkanDeviceMemoryManager::updateMemoryProperties() noexcept
{
    if (m_device == nullptr || m_device->getPhysicalHandle() == VK_NULL_HANDLE)
    {
        return;
    }

    const bool hasMemoryBudget = (m_device->getOptionalExtensions().hasMemoryBudget != 0u) &&
                                 (dynamic_api::vkGetPhysicalDeviceMemoryProperties2 != nullptr);

    VkPhysicalDeviceMemoryBudgetPropertiesEXT queriedBudget{};
    VkPhysicalDeviceMemoryProperties queriedProps{};

    if (hasMemoryBudget)
    {
        zeroVulkanStruct(queriedBudget, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT);
        VkPhysicalDeviceMemoryProperties2 memoryProps2 =
            zeroVulkanStruct<VkPhysicalDeviceMemoryProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2);
        memoryProps2.pNext = &queriedBudget;
        vkGetPhysicalDeviceMemoryProperties2(m_device->getPhysicalHandle(), &memoryProps2);
        queriedProps = memoryProps2.memoryProperties;

        const gp::Int32 rawScale = g_cvarBudgetPercentageScale.getValueOnAnyThread(true);
        const gp::UInt64 budgetScale = static_cast<gp::UInt64>(gp::math::clamp<gp::Int32>(rawScale, 0, 100));
        for (gp::UInt32 heap = 0u; heap < VK_MAX_MEMORY_HEAPS; ++heap)
        {
            queriedBudget.heapBudget[heap] = (queriedBudget.heapBudget[heap] * budgetScale) / 100u;
        }
    }
    else if (dynamic_api::vkGetPhysicalDeviceMemoryProperties != nullptr)
    {
        gp::memory::zeroMemory(&queriedBudget, sizeof(queriedBudget));
        vkGetPhysicalDeviceMemoryProperties(m_device->getPhysicalHandle(), &queriedProps);
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_memoryBudget = queriedBudget;
    m_memoryProperties = queriedProps;

    VulkanMemoryStats stats{};
    for (gp::UInt32 heapIndex = 0u; heapIndex < m_memoryProperties.memoryHeapCount; ++heapIndex)
    {
        const bool isLocalHeap =
            (m_memoryProperties.memoryHeaps[heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u;
        gp::UInt64 heapBudget = 0u;
        gp::UInt64 heapUsed = 0u;

        if (hasMemoryBudget)
        {
            heapBudget = m_memoryBudget.heapBudget[heapIndex];
            heapUsed = m_memoryBudget.heapUsage[heapIndex];
        }
        else
        {
            heapBudget = m_memoryProperties.memoryHeaps[heapIndex].size;
            heapUsed = (heapIndex < m_heapInfos.size()) ? m_heapInfos[heapIndex].usedSize : 0u;
        }

        if (isLocalHeap)
        {
            stats.budgetLocal += heapBudget;
            stats.usedLocal += heapUsed;
        }
        else
        {
            stats.budgetSystem += heapBudget;
            stats.usedSystem += heapUsed;
        }
    }

    stats.availableLocal = (stats.usedLocal < stats.budgetLocal) ? (stats.budgetLocal - stats.usedLocal) : 0u;
    stats.availableSystem = (stats.usedSystem < stats.budgetSystem) ? (stats.budgetSystem - stats.usedSystem) : 0u;
    stats.demotedLocal = (stats.usedLocal > stats.budgetLocal) ? (stats.usedLocal - stats.budgetLocal) : 0u;
    stats.demotedSystem = (stats.usedSystem > stats.budgetSystem) ? (stats.usedSystem - stats.budgetSystem) : 0u;

    m_memoryStats = stats;
}

bool VulkanDeviceMemoryManager::supportsMemoryType(const VkMemoryPropertyFlags properties) const noexcept
{
    for (gp::UInt32 index = 0u; index < m_memoryProperties.memoryTypeCount; ++index)
    {
        if (m_memoryProperties.memoryTypes[index].propertyFlags == properties)
        {
            return true;
        }
    }
    return false;
}

void VulkanDeviceMemoryManager::getPrimaryHeapStatus(gp::UInt64& outAllocated, gp::UInt64& outLimit) const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    if (m_primaryHeapIndex < 0 || static_cast<gp::USize>(m_primaryHeapIndex) >= m_heapInfos.size())
    {
        outAllocated = 0u;
        outLimit = 1u;
        return;
    }

    const gp::UInt32 heapIndex = static_cast<gp::UInt32>(m_primaryHeapIndex);
    outAllocated = m_heapInfos[heapIndex].usedSize;
    outLimit = gp::math::max<gp::UInt64>(getBaseHeapSize(heapIndex), 1u);
}

VkResult VulkanDeviceMemoryManager::getMemoryTypeFromProperties(
    gp::UInt32 typeBits, const VkMemoryPropertyFlags properties, gp::UInt32* const outTypeIndex
) const noexcept
{
    if (outTypeIndex == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    for (gp::UInt32 index = 0u; index < m_memoryProperties.memoryTypeCount && typeBits != 0u; ++index)
    {
        if ((typeBits & 1u) != 0u)
        {
            if ((m_memoryProperties.memoryTypes[index].propertyFlags & properties) == properties)
            {
                *outTypeIndex = index;
                return VK_SUCCESS;
            }
        }
        typeBits >>= 1u;
    }

    return VK_ERROR_FEATURE_NOT_PRESENT;
}

VkResult VulkanDeviceMemoryManager::getMemoryTypeFromPropertiesExcluding(
    gp::UInt32 typeBits,
    const VkMemoryPropertyFlags properties,
    const gp::UInt32 excludeTypeIndex,
    gp::UInt32* const outTypeIndex
) const noexcept
{
    if (outTypeIndex == nullptr)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    for (gp::UInt32 index = 0u; index < m_memoryProperties.memoryTypeCount && typeBits != 0u; ++index)
    {
        if ((typeBits & 1u) != 0u)
        {
            if ((m_memoryProperties.memoryTypes[index].propertyFlags & properties) == properties &&
                index != excludeTypeIndex)
            {
                *outTypeIndex = index;
                return VK_SUCCESS;
            }
        }
        typeBits >>= 1u;
    }

    return VK_ERROR_FEATURE_NOT_PRESENT;
}

VulkanDeviceMemoryManager::BlockBucket& VulkanDeviceMemoryManager::findOrAddBucketLocked(const BlockKey& key) noexcept
{
    for (gp::USize index = 0u; index < m_cachedBlocks.size(); ++index)
    {
        if (m_cachedBlocks[index].key == key)
        {
            return m_cachedBlocks[index];
        }
    }

    m_cachedBlocks.pushBack(BlockBucket{ key, {} });
    return m_cachedBlocks.back();
}

VulkanDeviceMemoryAllocation* VulkanDeviceMemoryManager::alloc(
    const bool canFail,
    const VkDeviceSize allocationSize,
    const gp::UInt32 memoryTypeBits,
    const VkMemoryPropertyFlags memoryPropertyFlags,
    void* const dedicatedAllocateInfo,
    const gp::Float32 priority,
    const bool isExternal
) noexcept
{
    gp::UInt32 memoryTypeIndex = 0xFFFFFFFFu;
    const VkResult findResult = getMemoryTypeFromProperties(memoryTypeBits, memoryPropertyFlags, &memoryTypeIndex);
    if (findResult != VK_SUCCESS)
    {
        if (!canFail)
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "Failed to find Vulkan memory type (typeBits=0x{:x}, props=0x{:x}).",
                memoryTypeBits,
                memoryPropertyFlags
            );
        }
        return nullptr;
    }

    return alloc(canFail, allocationSize, memoryTypeIndex, dedicatedAllocateInfo, priority, isExternal);
}

VulkanDeviceMemoryAllocation* VulkanDeviceMemoryManager::alloc(
    const bool canFail,
    const VkDeviceSize allocationSize,
    const gp::UInt32 memoryTypeIndex,
    void* const dedicatedAllocateInfo,
    const gp::Float32 priority,
    const bool isExternal
) noexcept
{
    if (m_device == nullptr || !m_device->isValid() || dynamic_api::vkAllocateMemory == nullptr ||
        allocationSize == 0u || memoryTypeIndex >= m_memoryProperties.memoryTypeCount)
    {
        return nullptr;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);

    if (dedicatedAllocateInfo == nullptr && !isExternal)
    {
        const BlockKey key{ memoryTypeIndex, allocationSize };
        for (gp::USize bucketIndex = 0u; bucketIndex < m_cachedBlocks.size(); ++bucketIndex)
        {
            BlockBucket& bucket = m_cachedBlocks[bucketIndex];
            if (bucket.key == key && !bucket.freeBlocks.isEmpty())
            {
                VulkanDeviceMemoryAllocation* const reused = bucket.freeBlocks.back().allocation;
                bucket.freeBlocks.popBack();
                return reused;
            }
        }
    }

    VkMemoryAllocateInfo allocInfo = zeroVulkanStruct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    allocInfo.allocationSize = allocationSize;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    VkMemoryPriorityAllocateInfoEXT priorityInfo =
        zeroVulkanStruct<VkMemoryPriorityAllocateInfoEXT>(VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT);
    if (m_device->getOptionalExtensions().hasMemoryPriority != 0u)
    {
        priorityInfo.priority = gp::math::clamp<gp::Float32>(priority, 0.0f, 1.0f);
        addToPNext(allocInfo, priorityInfo);
    }

    if (dedicatedAllocateInfo != nullptr)
    {
        VkMemoryDedicatedAllocateInfo* const dedicatedInfo =
            static_cast<VkMemoryDedicatedAllocateInfo*>(dedicatedAllocateInfo);
        dedicatedInfo->pNext = allocInfo.pNext;
        allocInfo.pNext = dedicatedInfo;
    }

    VkExportMemoryAllocateInfo exportInfo =
        zeroVulkanStruct<VkExportMemoryAllocateInfo>(VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO);
    if (isExternal)
    {
#if GP_PLATFORM_WINDOWS
        exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
        exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
        addToPNext(allocInfo, exportInfo);
    }

    VkMemoryAllocateFlagsInfo flagsInfo =
        zeroVulkanStruct<VkMemoryAllocateFlagsInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);
    if (m_device->getOptionalExtensions().hasBufferDeviceAddress != 0u)
    {
        flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        addToPNext(allocInfo, flagsInfo);
    }

    const gp::UInt32 heapIndex = m_memoryProperties.memoryTypes[memoryTypeIndex].heapIndex;
    const gp::Int32 fakeLimitMB = g_cvarFakeMemoryLimitMB.getValueOnAnyThread(true);

    VkDeviceMemory handle = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;
    if (m_primaryHeapIndex >= 0 && heapIndex == static_cast<gp::UInt32>(m_primaryHeapIndex) && fakeLimitMB > 0 &&
        heapIndex < m_heapInfos.size() &&
        (static_cast<gp::UInt64>(fakeLimitMB) << 20u) < m_heapInfos[heapIndex].usedSize)
    {
        result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    else
    {
        result = vkAllocateMemory(m_device->getHandle(), &allocInfo, GP_VULKAN_CPU_ALLOCATOR, &handle);
    }

    if (result != VK_SUCCESS)
    {
        if (canFail)
        {
            GP_LOG_WARNING(
                LogVulkanRHI,
                "Failed to allocate Vulkan device memory ({} bytes, memType={}, result={}).",
                allocationSize,
                memoryTypeIndex,
                vulkanResultToString(result)
            );
        }
        else
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "Out of Vulkan memory ({} bytes, memType={}, result={}).",
                allocationSize,
                memoryTypeIndex,
                vulkanResultToString(result)
            );
        }
        return nullptr;
    }

    const VkMemoryPropertyFlags propFlags = m_memoryProperties.memoryTypes[memoryTypeIndex].propertyFlags;

    VulkanDeviceMemoryAllocation* const newAlloc = new VulkanDeviceMemoryAllocation();
    newAlloc->m_deviceHandle = m_device->getHandle();
    newAlloc->m_handle = handle;
    newAlloc->m_size = allocationSize;
    newAlloc->m_memoryTypeIndex = memoryTypeIndex;
    newAlloc->m_canBeMapped = ((propFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u) ? 1u : 0u;
    newAlloc->m_isCoherent = ((propFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0u) ? 1u : 0u;
    newAlloc->m_isCached = ((propFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0u) ? 1u : 0u;
    newAlloc->m_dedicatedMemory = (dedicatedAllocateInfo != nullptr) ? 1u : 0u;
    newAlloc->m_isExternal = isExternal ? 1u : 0u;

    ++m_numAllocations;
    m_peakNumAllocations = gp::math::max(m_numAllocations, m_peakNumAllocations);

    if (m_numAllocations == m_device->getLimits().maxMemoryAllocationCount)
    {
        GP_LOG_WARNING(LogVulkanRHI, "Reached physical device maxMemoryAllocationCount ({}).", m_numAllocations);
    }

    if (heapIndex < m_heapInfos.size())
    {
        HeapInfo& heapInfo = m_heapInfos[heapIndex];
        heapInfo.allocations.pushBack(newAlloc);
        heapInfo.usedSize += allocationSize;
        heapInfo.peakSize = gp::math::max(heapInfo.peakSize, heapInfo.usedSize);
    }

    return newAlloc;
}

void VulkanDeviceMemoryManager::free(VulkanDeviceMemoryAllocation*& allocation, const gp::UInt32 currentFrame) noexcept
{
    if (allocation == nullptr)
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    GP_CHECK(allocation->m_handle != VK_NULL_HANDLE);
    GP_CHECK(allocation->m_freedBySystem == 0u);

    if (allocation->m_dedicatedMemory == 0u && allocation->m_isExternal == 0u)
    {
        const BlockKey key{ allocation->m_memoryTypeIndex, allocation->m_size };
        BlockBucket& bucket = findOrAddBucketLocked(key);
        bucket.freeBlocks.pushBack(FreeBlock{ allocation, currentFrame });
        allocation = nullptr;
        return;
    }

    freeInternalLocked(allocation);
    allocation = nullptr;
}

void VulkanDeviceMemoryManager::freeInternalLocked(VulkanDeviceMemoryAllocation* const allocation) noexcept
{
    if (allocation == nullptr)
    {
        return;
    }

    allocation->unmap();

    if (m_device != nullptr && m_device->isValid() && allocation->m_handle != VK_NULL_HANDLE &&
        dynamic_api::vkFreeMemory != nullptr)
    {
        vkFreeMemory(m_device->getHandle(), allocation->m_handle, GP_VULKAN_CPU_ALLOCATOR);
    }

    if (m_numAllocations > 0u)
    {
        --m_numAllocations;
    }

    if (allocation->m_memoryTypeIndex < m_memoryProperties.memoryTypeCount)
    {
        const gp::UInt32 heapIndex = m_memoryProperties.memoryTypes[allocation->m_memoryTypeIndex].heapIndex;
        if (heapIndex < m_heapInfos.size())
        {
            HeapInfo& heapInfo = m_heapInfos[heapIndex];
            heapInfo.usedSize =
                (heapInfo.usedSize >= allocation->m_size) ? (heapInfo.usedSize - allocation->m_size) : 0u;
            for (gp::USize idx = 0u; idx < heapInfo.allocations.size(); ++idx)
            {
                if (heapInfo.allocations[idx] == allocation)
                {
                    const gp::USize lastIdx = heapInfo.allocations.size() - 1u;
                    if (idx != lastIdx)
                    {
                        heapInfo.allocations[idx] = heapInfo.allocations[lastIdx];
                    }
                    heapInfo.allocations.popBack();
                    break;
                }
            }
        }
    }

    allocation->m_handle = VK_NULL_HANDLE;
    allocation->m_freedBySystem = 1u;
    delete allocation;
}

void VulkanDeviceMemoryManager::trimMemory(const bool fullTrim, const gp::UInt32 currentFrame) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);

    constexpr gp::UInt32 kFrameThresholdFull = 100u;
    constexpr gp::UInt32 kFrameThresholdPartial = 10u;
    constexpr gp::UInt32 kLargeThresholdPartial = 5u;
    constexpr gp::UInt32 kSmallThresholdPartial = 10u;
    constexpr VkDeviceSize kSmallPageSize = 8ULL << 20u;

    for (gp::USize bucketIndex = 0u; bucketIndex < m_cachedBlocks.size(); ++bucketIndex)
    {
        BlockBucket& bucket = m_cachedBlocks[bucketIndex];
        const gp::UInt32 thresholdPartial =
            (bucket.key.blockSize <= kSmallPageSize) ? kSmallThresholdPartial : kLargeThresholdPartial;

        gp::UInt32 abovePartialThreshold = 0u;
        gp::USize index = 0u;
        while (index < bucket.freeBlocks.size())
        {
            const FreeBlock& freeBlock = bucket.freeBlocks[index];
            const bool expiredFull =
                (currentFrame >= freeBlock.frameFreed) && ((currentFrame - freeBlock.frameFreed) > kFrameThresholdFull);
            if (fullTrim || expiredFull)
            {
                freeInternalLocked(freeBlock.allocation);
                bucket.freeBlocks.erase(bucket.freeBlocks.begin() + static_cast<gp::IntPtr>(index));
                continue;
            }

            if (currentFrame >= freeBlock.frameFreed && (currentFrame - freeBlock.frameFreed) > kFrameThresholdPartial)
            {
                ++abovePartialThreshold;
            }
            ++index;
        }

        if (abovePartialThreshold > thresholdPartial)
        {
            gp::UInt32 blocksToFree = abovePartialThreshold - thresholdPartial;
            index = 0u;
            while (index < bucket.freeBlocks.size() && blocksToFree > 0u)
            {
                const FreeBlock& freeBlock = bucket.freeBlocks[index];
                if (currentFrame >= freeBlock.frameFreed &&
                    (currentFrame - freeBlock.frameFreed) > kFrameThresholdPartial)
                {
                    freeInternalLocked(freeBlock.allocation);
                    bucket.freeBlocks.erase(bucket.freeBlocks.begin() + static_cast<gp::IntPtr>(index));
                    --blocksToFree;
                    continue;
                }
                ++index;
            }
        }
    }
}

gp::UInt64 VulkanDeviceMemoryManager::getTotalMemory(const bool gpu) const noexcept
{
    gp::UInt64 totalMemory = 0u;
    for (gp::UInt32 index = 0u; index < m_memoryProperties.memoryHeapCount; ++index)
    {
        const bool isGpuHeap = (m_memoryProperties.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u;
        if (isGpuHeap == gpu)
        {
            totalMemory += m_memoryProperties.memoryHeaps[index].size;
        }
    }
    return totalMemory;
}

VkDeviceSize VulkanDeviceMemoryManager::getBaseHeapSize(const gp::UInt32 heapIndex) const noexcept
{
    if (heapIndex >= m_memoryProperties.memoryHeapCount)
    {
        return 0u;
    }

    VkDeviceSize baseSize = m_memoryProperties.memoryHeaps[heapIndex].size;
    if (m_device != nullptr && m_device->getOptionalExtensions().hasMemoryBudget != 0u &&
        m_memoryBudget.heapBudget[heapIndex] > 0u)
    {
        baseSize = m_memoryBudget.heapBudget[heapIndex];
    }

    const gp::Int32 fakeLimitMB = g_cvarFakeMemoryLimitMB.getValueOnAnyThread(true);
    if (m_primaryHeapIndex >= 0 && heapIndex == static_cast<gp::UInt32>(m_primaryHeapIndex) && fakeLimitMB > 0)
    {
        baseSize = gp::math::min<VkDeviceSize>(baseSize, static_cast<VkDeviceSize>(fakeLimitMB) << 20u);
    }

    return baseSize;
}

gp::UInt32 VulkanDeviceMemoryManager::getHeapIndex(const gp::UInt32 memoryTypeIndex) const noexcept
{
    if (memoryTypeIndex >= m_memoryProperties.memoryTypeCount)
    {
        return 0u;
    }
    return m_memoryProperties.memoryTypes[memoryTypeIndex].heapIndex;
}

VkMemoryPropertyFlags VulkanDeviceMemoryManager::getEvictedMemoryProperties() const noexcept
{
    if (m_device != nullptr && m_device->getVendorId() == gp::rhi::GpuVendorId::Amd)
    {
        return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    return 0u;
}

void VulkanDeviceMemoryManager::getMemoryDump(gp::Vector<VulkanResourceHeapStats>& outDeviceHeapsStats) const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    outDeviceHeapsStats.clear();

    for (gp::USize heapIndex = 0u; heapIndex < m_heapInfos.size(); ++heapIndex)
    {
        VulkanResourceHeapStats stat{};
        stat.totalMemory = m_memoryProperties.memoryHeaps[heapIndex].size;

        for (gp::UInt32 typeIndex = 0u; typeIndex < m_memoryProperties.memoryTypeCount; ++typeIndex)
        {
            if (m_memoryProperties.memoryTypes[typeIndex].heapIndex == heapIndex)
            {
                stat.memoryFlags |= m_memoryProperties.memoryTypes[typeIndex].propertyFlags;
            }
        }

        const HeapInfo& heapInfo = m_heapInfos[heapIndex];
        for (gp::USize allocIdx = 0u; allocIdx < heapInfo.allocations.size(); ++allocIdx)
        {
            const VulkanDeviceMemoryAllocation* const allocPtr = heapInfo.allocations[allocIdx];
            if (allocPtr != nullptr)
            {
                stat.bufferAllocations += 1u;
                stat.usedBufferMemory += allocPtr->getSize();
                stat.pages += 1u;
            }
        }

        for (gp::USize bucketIdx = 0u; bucketIdx < m_cachedBlocks.size(); ++bucketIdx)
        {
            const BlockBucket& bucket = m_cachedBlocks[bucketIdx];
            if (bucket.key.memoryTypeIndex < m_memoryProperties.memoryTypeCount &&
                m_memoryProperties.memoryTypes[bucket.key.memoryTypeIndex].heapIndex == heapIndex)
            {
                for (gp::USize fbIdx = 0u; fbIdx < bucket.freeBlocks.size(); ++fbIdx)
                {
                    const VulkanDeviceMemoryAllocation* const allocPtr = bucket.freeBlocks[fbIdx].allocation;
                    if (allocPtr != nullptr)
                    {
                        stat.imageAllocations += 1u;
                        stat.usedImageMemory += allocPtr->getSize();
                        stat.pages += 1u;
                    }
                }
            }
        }

        outDeviceHeapsStats.pushBack(stat);
    }
}

gp::UInt32 VulkanDeviceMemoryManager::getNumAllocations() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_numAllocations;
}

gp::UInt32 VulkanDeviceMemoryManager::getPeakNumAllocations() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_peakNumAllocations;
}

}   // namespace gp::rhi::vulkan
