// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/VulkanDeferredDeletionQueue.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/VulkanDeviceMemory.hpp"
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

gp::console::AutoConsoleVariable<gp::Int32> g_cvarNumFramesToWaitForResourceDelete(
    "r.Vulkan.NumFramesToWaitForResourceDelete",
    0,
    "Number of extra frames to hold deleted Vulkan resources before freeing them.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

}   // namespace

VulkanDeferredDeletionQueue::VulkanDeferredDeletionQueue(
    VulkanDevice& device, VulkanDeviceMemoryManager& deviceMemoryManager, VulkanMemoryManager& memoryManager
) noexcept
    : m_device(device)
    , m_deviceMemoryManager(deviceMemoryManager)
    , m_memoryManager(memoryManager)
{
    const gp::Int32 framesToWait =
        gp::math::max<gp::Int32>(0, g_cvarNumFramesToWaitForResourceDelete.getValueOnAnyThread(true));
    const gp::USize bucketCount = (framesToWait > 0) ? static_cast<gp::USize>(framesToWait + 2) : 1u;
    m_entriesPerFrame.resize(bucketCount);
}

VulkanDeferredDeletionQueue::~VulkanDeferredDeletionQueue() noexcept
{
    releaseResources(true, nullptr, 0u);
}

void VulkanDeferredDeletionQueue::enqueueGenericResource(const EntryType type, const gp::UInt64 handle) noexcept
{
    if (handle == 0u)
    {
        return;
    }

    Entry entry{};
    entry.structureType = type;
    entry.handle = handle;

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_entriesPerFrame[m_currentFrameIndex].pushBack(entry);
}

void VulkanDeferredDeletionQueue::enqueueFunc(gp::UniqueFunction<void(VulkanDevice&)>&& func) noexcept
{
    if (!func.isBound())
    {
        return;
    }

    Entry entry{};
    entry.structureType = EntryType::Func;
    entry.func = new gp::UniqueFunction<void(VulkanDevice&)>(std::move(func));

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_entriesPerFrame[m_currentFrameIndex].pushBack(entry);
}

void VulkanDeferredDeletionQueue::enqueueDeviceAllocation(
    VulkanDeviceMemoryAllocation* const deviceMemoryAllocation
) noexcept
{
    if (deviceMemoryAllocation == nullptr)
    {
        return;
    }

    Entry entry{};
    entry.structureType = EntryType::DeviceMemoryAllocation;
    entry.deviceMemoryAllocation = deviceMemoryAllocation;

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_entriesPerFrame[m_currentFrameIndex].pushBack(entry);
}

void VulkanDeferredDeletionQueue::enqueueResourceAllocation(VulkanAllocation& allocation) noexcept
{
    if (!allocation.hasAllocation())
    {
        return;
    }

    allocation.disown();

    Entry entry{};
    entry.structureType = EntryType::ResourceAllocation;
    entry.allocation = allocation;

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_entriesPerFrame[m_currentFrameIndex].pushBack(entry);
}

void VulkanDeferredDeletionQueue::flushRHIDeletionQueue(const bool includeExtendedLifetimeResources) noexcept
{
    gp::rhi::resource::ResourceDeletionQueue::flushPendingDeletes(includeExtendedLifetimeResources);
}

void VulkanDeferredDeletionQueue::releaseResources(
    const bool deleteImmediately, VulkanSyncPointRef completionSyncPoint, const gp::UInt32 currentFrame
) noexcept
{
    if (deleteImmediately)
    {
        [[maybe_unused]] const gp::UInt32 flushedBatches = processCompletedBatches(true);
    }
    else
    {
        flushRHIDeletionQueue(true);
        [[maybe_unused]] const gp::UInt32 retiredBatches = processCompletedBatches(false);
    }

    gp::Vector<Entry> harvestedEntries{};
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);

        if (deleteImmediately)
        {
            for (gp::USize bucketIndex = 0u; bucketIndex < m_entriesPerFrame.size(); ++bucketIndex)
            {
                gp::Vector<Entry>& bucket = m_entriesPerFrame[bucketIndex];
                for (gp::USize entryIndex = 0u; entryIndex < bucket.size(); ++entryIndex)
                {
                    harvestedEntries.pushBack(bucket[entryIndex]);
                }
                bucket.clear();
            }
        }
        else if (m_entriesPerFrame.size() == 1u)
        {
            harvestedEntries = std::move(m_entriesPerFrame[0u]);
            m_entriesPerFrame[0u].clear();
        }
        else if (m_lastReleaseFrameNumber != currentFrame)
        {
            m_lastReleaseFrameNumber = currentFrame;
            m_currentFrameIndex = static_cast<gp::UInt32>((m_currentFrameIndex + 1u) % m_entriesPerFrame.size());
            harvestedEntries = std::move(m_entriesPerFrame[m_currentFrameIndex]);
            m_entriesPerFrame[m_currentFrameIndex].clear();
        }
    }

    if (harvestedEntries.isEmpty())
    {
        return;
    }

    if (deleteImmediately || !completionSyncPoint || completionSyncPoint->isComplete())
    {
        releaseResourcesImmediately(harvestedEntries);
    }
    else
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        m_inFlightBatches.pushBack(InFlightBatch{ std::move(completionSyncPoint), std::move(harvestedEntries) });
    }
}

gp::UInt32 VulkanDeferredDeletionQueue::processCompletedBatches(const bool forceImmediate) noexcept
{
    gp::Vector<gp::Vector<Entry>> readyBatches{};

    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        gp::USize writeIndex = 0u;
        for (gp::USize readIndex = 0u; readIndex < m_inFlightBatches.size(); ++readIndex)
        {
            InFlightBatch& batch = m_inFlightBatches[readIndex];
            if (forceImmediate || !batch.syncPoint || batch.syncPoint->isComplete())
            {
                readyBatches.pushBack(std::move(batch.entries));
            }
            else
            {
                if (writeIndex != readIndex)
                {
                    m_inFlightBatches[writeIndex] = std::move(batch);
                }
                ++writeIndex;
            }
        }
        m_inFlightBatches.resize(writeIndex);
    }

    for (gp::USize index = 0u; index < readyBatches.size(); ++index)
    {
        releaseResourcesImmediately(readyBatches[index]);
    }

    return static_cast<gp::UInt32>(readyBatches.size());
}

void VulkanDeferredDeletionQueue::releaseResourcesImmediately(const gp::Vector<Entry>& entries) noexcept
{
    const VkDevice deviceHandle = m_device.getHandle();
    gp::Vector<VulkanAllocation> poolAllocationsToFree{};

    for (gp::USize index = 0u; index < entries.size(); ++index)
    {
        const Entry& entry = entries[index];
        switch (entry.structureType)
        {
#define GP_VK_DESTROY_CASE(TypeName)                                                                                 \
    case EntryType::TypeName:                                                                                        \
        if (deviceHandle != VK_NULL_HANDLE && dynamic_api::vkDestroy##TypeName != nullptr)                           \
        {                                                                                                            \
            vkDestroy##TypeName(deviceHandle, reinterpret_cast<Vk##TypeName>(entry.handle), GP_VULKAN_CPU_ALLOCATOR); \
        }                                                                                                            \
        break

            GP_VK_DESTROY_CASE(RenderPass);
            GP_VK_DESTROY_CASE(Buffer);
            GP_VK_DESTROY_CASE(BufferView);
            GP_VK_DESTROY_CASE(Image);
            GP_VK_DESTROY_CASE(ImageView);
            GP_VK_DESTROY_CASE(Pipeline);
            GP_VK_DESTROY_CASE(PipelineLayout);
            GP_VK_DESTROY_CASE(Framebuffer);
            GP_VK_DESTROY_CASE(DescriptorSetLayout);
            GP_VK_DESTROY_CASE(Sampler);
            GP_VK_DESTROY_CASE(Semaphore);
            GP_VK_DESTROY_CASE(ShaderModule);
            GP_VK_DESTROY_CASE(Event);
#undef GP_VK_DESTROY_CASE

        case EntryType::AccelerationStructure:
            if (deviceHandle != VK_NULL_HANDLE && dynamic_api::vkDestroyAccelerationStructureKHR != nullptr)
            {
                dynamic_api::vkDestroyAccelerationStructureKHR(
                    deviceHandle, reinterpret_cast<VkAccelerationStructureKHR>(entry.handle), GP_VULKAN_CPU_ALLOCATOR
                );
            }
            break;

        case EntryType::ResourceAllocation:
        {
            VulkanAllocation allocation = entry.allocation;
            if (allocation.isPoolAllocated != 0u)
            {
                poolAllocationsToFree.pushBack(allocation);
            }
            else
            {
                allocation.own();
                m_memoryManager.freeVulkanAllocation(allocation, VulkanFreeFlags::DontDefer);
            }
            break;
        }

        case EntryType::DeviceMemoryAllocation:
        {
            VulkanDeviceMemoryAllocation* deviceAlloc = entry.deviceMemoryAllocation;
            if (deviceAlloc != nullptr)
            {
                m_deviceMemoryManager.free(deviceAlloc);
            }
            break;
        }

        case EntryType::Func:
            if (entry.func != nullptr)
            {
                if (entry.func->isBound())
                {
                    (*entry.func)(m_device);
                }
                delete entry.func;
            }
            break;

        default:
            GP_CHECK(false);
            break;
        }
    }

    if (!poolAllocationsToFree.isEmpty())
    {
        m_memoryManager.freePoolAllocations(
            gp::VectorView<VulkanAllocation>(poolAllocationsToFree.data(), poolAllocationsToFree.size())
        );
    }
}

gp::USize VulkanDeferredDeletionQueue::getPendingEntryCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    gp::USize total = 0u;
    for (gp::USize index = 0u; index < m_entriesPerFrame.size(); ++index)
    {
        total += m_entriesPerFrame[index].size();
    }
    return total;
}

gp::USize VulkanDeferredDeletionQueue::getInFlightBatchCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_inFlightBatches.size();
}

}   // namespace gp::rhi::vulkan
