// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/VulkanSemaphore.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "logging/LogMacros.hpp"
#include "memory/MemoryOrder.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "templates/NumericLimits.hpp"
#include "threading/atomics/Atomic.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Resolves `vkGetSemaphoreCounterValueKHR` or its Vulkan 1.2 core equivalent for `device`.
/// @param[in] device Active logical `VkDevice`.
/// @return Resolved function pointer, or `nullptr` if unavailable.
[[nodiscard]] PFN_vkGetSemaphoreCounterValue resolveGetCounterFn(const VkDevice device) noexcept
{
    if (dynamic_api::vkGetSemaphoreCounterValueKHR != nullptr)
    {
        return dynamic_api::vkGetSemaphoreCounterValueKHR;
    }
    if (device != VK_NULL_HANDLE)
    {
        return reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(
            VulkanLoader::getDeviceProcAddr(device, "vkGetSemaphoreCounterValue")
        );
    }
    return nullptr;
}

/// @brief Resolves `vkWaitSemaphoresKHR` or its Vulkan 1.2 core equivalent for `device`.
/// @param[in] device Active logical `VkDevice`.
/// @return Resolved function pointer, or `nullptr` if unavailable.
[[nodiscard]] PFN_vkWaitSemaphores resolveWaitSemaphoresFn(const VkDevice device) noexcept
{
    if (dynamic_api::vkWaitSemaphoresKHR != nullptr)
    {
        return dynamic_api::vkWaitSemaphoresKHR;
    }
    if (device != VK_NULL_HANDLE)
    {
        return reinterpret_cast<PFN_vkWaitSemaphores>(VulkanLoader::getDeviceProcAddr(device, "vkWaitSemaphores"));
    }
    return nullptr;
}

/// @brief Resolves `vkSignalSemaphoreKHR` or its Vulkan 1.2 core equivalent for `device`.
/// @param[in] device Active logical `VkDevice`.
/// @return Resolved function pointer, or `nullptr` if unavailable.
[[nodiscard]] PFN_vkSignalSemaphore resolveSignalSemaphoreFn(const VkDevice device) noexcept
{
    if (dynamic_api::vkSignalSemaphoreKHR != nullptr)
    {
        return dynamic_api::vkSignalSemaphoreKHR;
    }
    if (device != VK_NULL_HANDLE)
    {
        return reinterpret_cast<PFN_vkSignalSemaphore>(VulkanLoader::getDeviceProcAddr(device, "vkSignalSemaphore"));
    }
    return nullptr;
}

}   // namespace

VulkanSemaphore::VulkanSemaphore(
    VulkanDevice& device, const VulkanSemaphoreFlags flags, const gp::UInt64 initialTimelineValue
) noexcept
    : m_device(device)
    , m_handle(VK_NULL_HANDLE)
    , m_flags(flags)
{
    if (!m_device.isValid() || dynamic_api::vkCreateSemaphore == nullptr)
    {
        return;
    }

    VkSemaphoreCreateInfo createInfo = zeroVulkanStruct<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    VkSemaphoreTypeCreateInfo typeCreateInfo =
        zeroVulkanStruct<VkSemaphoreTypeCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO);

    if (isTimeline())
    {
        const bool timelineSupported = (m_device.getOptionalExtensions().hasKHRTimelineSemaphore != 0u) ||
                                       (m_device.getFeatures().getCore12().timelineSemaphore == VK_TRUE);
        if (!timelineSupported)
        {
            GP_LOG_ERROR(LogVulkanRHI, "Cannot create timeline semaphore: device lacks timeline semaphore support.");
            return;
        }

        typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeCreateInfo.initialValue = initialTimelineValue;
        addToPNext(createInfo, typeCreateInfo);
    }

    const VkResult result = vkCreateSemaphore(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_handle);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateSemaphore failed with {}.", vulkanResultToString(result));
        m_handle = VK_NULL_HANDLE;
    }
}

VulkanSemaphore::VulkanSemaphore(
    VulkanDevice& device, const VkSemaphore externalSemaphore, const VulkanSemaphoreFlags flags
) noexcept
    : m_device(device)
    , m_handle(externalSemaphore)
    , m_flags(flags)
{}

VulkanSemaphore::~VulkanSemaphore() noexcept
{
    if (m_handle != VK_NULL_HANDLE && !isExternallyOwned() && m_device.isValid() &&
        dynamic_api::vkDestroySemaphore != nullptr)
    {
        vkDestroySemaphore(m_device.getHandle(), m_handle, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_handle = VK_NULL_HANDLE;
}

VulkanSemaphoreRef VulkanSemaphore::create(
    VulkanDevice& device, const VulkanSemaphoreFlags flags, const gp::UInt64 initialTimelineValue
) noexcept
{
    return gp::makeShared<VulkanSemaphore>(device, flags, initialTimelineValue);
}

VulkanSemaphoreRef VulkanSemaphore::createExternal(
    VulkanDevice& device, const VkSemaphore externalSemaphore, const VulkanSemaphoreFlags flags
) noexcept
{
    return gp::makeShared<VulkanSemaphore>(device, externalSemaphore, flags);
}

bool VulkanSemaphore::getTimelineSemaphoreValue(gp::UInt64& outValue) const noexcept
{
    if (!isTimeline() || m_handle == VK_NULL_HANDLE || !m_device.isValid())
    {
        return false;
    }

    const PFN_vkGetSemaphoreCounterValue getCounterFn = resolveGetCounterFn(m_device.getHandle());
    if (getCounterFn == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkGetSemaphoreCounterValue entry point is unavailable.");
        return false;
    }

    gp::UInt64 tempValue = 0u;
    const VkResult result = getCounterFn(m_device.getHandle(), m_handle, &tempValue);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkGetSemaphoreCounterValue failed with {}.", vulkanResultToString(result));
        return false;
    }

    if (tempValue == gp::NumericLimits<gp::UInt64>::max())
    {
        static gp::thread::Atomic<gp::Int32> warningCount{ 0 };
        if (warningCount.fetchAdd(1, gp::memory::MemoryOrder::Relaxed) < 10)
        {
            GP_LOG_WARNING(
                LogVulkanRHI, "Timeline semaphore counter returned UINT64_MAX (possible Vulkan device lost)."
            );
        }
        return false;
    }

    outValue = tempValue;
    return true;
}

bool VulkanSemaphore::waitForTimelineSemaphoreValue(
    const gp::UInt64 value, const gp::UInt64 timeoutNanoseconds
) const noexcept
{
    if (!isTimeline() || m_handle == VK_NULL_HANDLE || !m_device.isValid())
    {
        return false;
    }

    const PFN_vkWaitSemaphores waitFn = resolveWaitSemaphoresFn(m_device.getHandle());
    if (waitFn == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkWaitSemaphores entry point is unavailable.");
        return false;
    }

    VkSemaphoreWaitInfo waitInfo = zeroVulkanStruct<VkSemaphoreWaitInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO);
    waitInfo.semaphoreCount = 1u;
    waitInfo.pSemaphores = &m_handle;
    waitInfo.pValues = &value;

    const VkResult result = waitFn(m_device.getHandle(), &waitInfo, timeoutNanoseconds);
    switch (result)
    {
    case VK_SUCCESS:
        return true;

    case VK_TIMEOUT:
        return false;

    default:
        GP_LOG_ERROR(LogVulkanRHI, "vkWaitSemaphores failed with {}.", vulkanResultToString(result));
        return false;
    }
}

bool VulkanSemaphore::signalTimelineSemaphoreValue(const gp::UInt64 value) const noexcept
{
    if (!isTimeline() || m_handle == VK_NULL_HANDLE || !m_device.isValid())
    {
        return false;
    }

    const PFN_vkSignalSemaphore signalFn = resolveSignalSemaphoreFn(m_device.getHandle());
    if (signalFn == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkSignalSemaphore entry point is unavailable.");
        return false;
    }

    VkSemaphoreSignalInfo signalInfo = zeroVulkanStruct<VkSemaphoreSignalInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO);
    signalInfo.semaphore = m_handle;
    signalInfo.value = value;

    const VkResult result = signalFn(m_device.getHandle(), &signalInfo);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkSignalSemaphore failed with {}.", vulkanResultToString(result));
        return false;
    }

    return true;
}

}   // namespace gp::rhi::vulkan
