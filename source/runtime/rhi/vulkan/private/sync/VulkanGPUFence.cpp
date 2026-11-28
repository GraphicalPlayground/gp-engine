// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/VulkanGPUFence.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanGPUFence::VulkanGPUFence(VulkanDevice& device, const gp::Name name, const bool useTimelineSemaphores) noexcept
    : gp::rhi::sync::GPUFence(name)
    , m_device(device)
{
    if (!useTimelineSemaphores && m_device.isValid() && dynamic_api::vkCreateEvent != nullptr)
    {
        VkEventCreateInfo eventInfo = zeroVulkanStruct<VkEventCreateInfo>(VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
        const VkResult createResult =
            vkCreateEvent(m_device.getHandle(), &eventInfo, GP_VULKAN_CPU_ALLOCATOR, &m_event);
        if (createResult != VK_SUCCESS)
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkCreateEvent failed with {}.", vulkanResultToString(createResult));
            m_event = VK_NULL_HANDLE;
        }
        else if (dynamic_api::vkResetEvent != nullptr)
        {
            const VkResult resetResult = vkResetEvent(m_device.getHandle(), m_event);
            if (resetResult != VK_SUCCESS)
            {
                GP_LOG_WARNING(LogVulkanRHI, "vkResetEvent failed with {}.", vulkanResultToString(resetResult));
            }
        }
    }
}

VulkanGPUFence::~VulkanGPUFence() noexcept
{
    if (m_event != VK_NULL_HANDLE && m_device.isValid() && dynamic_api::vkDestroyEvent != nullptr)
    {
        vkDestroyEvent(m_device.getHandle(), m_event, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_event = VK_NULL_HANDLE;
}

void VulkanGPUFence::clear() noexcept
{
    m_completedSyncPoint.reset();
    m_submittedSyncPoint.reset();

    if (m_event != VK_NULL_HANDLE)
    {
        if (m_device.isValid() && dynamic_api::vkResetEvent != nullptr)
        {
            const VkResult result = vkResetEvent(m_device.getHandle(), m_event);
            if (result != VK_SUCCESS)
            {
                GP_LOG_WARNING(LogVulkanRHI, "vkResetEvent in clear() failed with {}.", vulkanResultToString(result));
            }
        }
    }
}

bool VulkanGPUFence::poll() const noexcept
{
    if (m_completedSyncPoint != nullptr && m_completedSyncPoint->isComplete())
    {
        return true;
    }

    if (m_event != VK_NULL_HANDLE && m_submittedSyncPoint != nullptr && m_submittedSyncPoint->isComplete() &&
        m_device.isValid() && dynamic_api::vkGetEventStatus != nullptr)
    {
        return vkGetEventStatus(m_device.getHandle(), m_event) == VK_EVENT_SET;
    }

    return false;
}

void VulkanGPUFence::wait(
    gp::rhi::command::CommandListImmediate& cmdList, const gp::rhi::GPUMask gpuMask
) const noexcept
{
    static_cast<void>(cmdList);
    static_cast<void>(gpuMask);

    if (!poll() && m_completedSyncPoint != nullptr)
    {
        m_completedSyncPoint->wait();
    }
}

void VulkanGPUFence::setSyncPoints(
    VulkanSyncPointRef submittedSyncPoint, VulkanSyncPointRef completedSyncPoint
) noexcept
{
    m_submittedSyncPoint = std::move(submittedSyncPoint);
    m_completedSyncPoint = std::move(completedSyncPoint);
}

}   // namespace gp::rhi::vulkan
