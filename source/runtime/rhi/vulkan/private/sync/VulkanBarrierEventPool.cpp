// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/VulkanBarrierEventPool.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanBarrierEventPool::~VulkanBarrierEventPool() noexcept
{
    GP_CHECKF(m_events.isEmpty(), "VulkanBarrierEventPool destroyed while still holding pooled events.");
}

VkEvent VulkanBarrierEventPool::acquire(const VkDevice device) noexcept
{
    VkEvent handle = VK_NULL_HANDLE;

    // Reuse a pooled event when one is available.
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        if (!m_events.isEmpty())
        {
            handle = m_events.back();
            m_events.popBack();
        }
    }

    // Otherwise create a new one.
    if (handle == VK_NULL_HANDLE)
    {
        if (device == VK_NULL_HANDLE || dynamic_api::vkCreateEvent == nullptr)
        {
            return VK_NULL_HANDLE;
        }

        VkEventCreateInfo info = zeroVulkanStruct<VkEventCreateInfo>(VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
        info.flags = VK_EVENT_CREATE_DEVICE_ONLY_BIT;
        const VkResult result = vkCreateEvent(device, &info, GP_VULKAN_CPU_ALLOCATOR, &handle);
        if (result != VK_SUCCESS)
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkCreateEvent failed with {}.", vulkanResultToString(result));
            return VK_NULL_HANDLE;
        }

        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        ++m_createdCount;
    }

    return handle;
}

void VulkanBarrierEventPool::release(const VkEvent event) noexcept
{
    if (event == VK_NULL_HANDLE)
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_events.pushBack(event);
}

void VulkanBarrierEventPool::destroy(const VkDevice device) noexcept
{
    gp::Vector<VkEvent> eventsToDestroy{};
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        eventsToDestroy = std::move(m_events);
        m_events.clear();
    }

    if (device == VK_NULL_HANDLE || dynamic_api::vkDestroyEvent == nullptr)
    {
        return;
    }

    for (gp::USize index = 0u; index < eventsToDestroy.size(); ++index)
    {
        vkDestroyEvent(device, eventsToDestroy[index], GP_VULKAN_CPU_ALLOCATOR);
    }
}

gp::USize VulkanBarrierEventPool::getPooledCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_events.size();
}

gp::USize VulkanBarrierEventPool::getCreatedCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_createdCount;
}

}   // namespace gp::rhi::vulkan
