// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanPhysicalDeviceProperties.hpp"
#include "CoreMinimal.hpp"
#include "device/VulkanPhysicalDeviceFeatures.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

VulkanPhysicalDeviceProperties::VulkanPhysicalDeviceProperties() noexcept
{
    m_core10 = zeroVulkanStruct<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    m_core11 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan11Properties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES);
    m_core12 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan12Properties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES);
    m_core13 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan13Properties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES);
    m_core14 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan14Properties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES);
    m_id = zeroVulkanStruct<VkPhysicalDeviceIDProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
    m_subgroup =
        zeroVulkanStruct<VkPhysicalDeviceSubgroupProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES);
}

void VulkanPhysicalDeviceProperties::query(const VkPhysicalDevice physicalDevice, const gp::UInt32 apiVersion) noexcept
{
    m_apiVersion = apiVersion;

    m_core10 = zeroVulkanStruct<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    m_core11 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan11Properties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES);
    m_core12 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan12Properties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES);
    m_core13 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan13Properties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES);
    m_core14 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan14Properties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES);
    m_id = zeroVulkanStruct<VkPhysicalDeviceIDProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES);
    m_subgroup =
        zeroVulkanStruct<VkPhysicalDeviceSubgroupProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES);

    if (physicalDevice == VK_NULL_HANDLE)
    {
        return;
    }

    if (dynamic_api::vkGetPhysicalDeviceProperties2 != nullptr)
    {
        addToPNext(m_core10, m_id);
        addToPNext(m_core10, m_subgroup);

        if (m_apiVersion >= VK_API_VERSION_1_2)
        {
            addToPNext(m_core10, m_core11);
            addToPNext(m_core10, m_core12);
        }

        if (m_apiVersion >= VK_API_VERSION_1_3)
        {
            addToPNext(m_core10, m_core13);
        }

        if (m_apiVersion >= VK_API_VERSION_1_4)
        {
            addToPNext(m_core10, m_core14);
        }

        vkGetPhysicalDeviceProperties2(physicalDevice, &m_core10);

        // Detach internal pNext links so struct copies never hold dangling pointers.
        m_core10.pNext = nullptr;
        m_id.pNext = nullptr;
        m_subgroup.pNext = nullptr;
        m_core11.pNext = nullptr;
        m_core12.pNext = nullptr;
        m_core13.pNext = nullptr;
        m_core14.pNext = nullptr;
    }
    else if (dynamic_api::vkGetPhysicalDeviceProperties != nullptr)
    {
        vkGetPhysicalDeviceProperties(physicalDevice, &m_core10.properties);
    }

    const VkPhysicalDeviceProperties& props = m_core10.properties;
    GP_LOG_INFO(LogVulkanRHI, "- DeviceName: {}", props.deviceName);
    GP_LOG_INFO(
        LogVulkanRHI,
        "- API={}.{}.{} (0x{:x}) Driver=0x{:x} VendorId=0x{:x} DeviceId=0x{:x}",
        VK_VERSION_MAJOR(props.apiVersion),
        VK_VERSION_MINOR(props.apiVersion),
        VK_VERSION_PATCH(props.apiVersion),
        props.apiVersion,
        props.driverVersion,
        props.vendorID,
        props.deviceID
    );
    GP_LOG_INFO(
        LogVulkanRHI,
        "- MaxBoundDescriptorSets={} TimestampComputeAndGraphics={} TimestampPeriod={}",
        props.limits.maxBoundDescriptorSets,
        props.limits.timestampComputeAndGraphics,
        props.limits.timestampPeriod
    );
}

VulkanCoreFeaturesAndProperties VulkanPhysicalDeviceProperties::makeCoreFeaturesAndProperties(
    const VulkanPhysicalDeviceFeatures& features
) const noexcept
{
    VulkanCoreFeaturesAndProperties core{};
    if (m_apiVersion >= VK_API_VERSION_1_2)
    {
        core.features11 = &features.getCore11();
        core.features12 = &features.getCore12();
        core.properties11 = &m_core11;
        core.properties12 = &m_core12;
    }
    if (m_apiVersion >= VK_API_VERSION_1_3)
    {
        core.features13 = &features.getCore13();
        core.properties13 = &m_core13;
    }
    if (m_apiVersion >= VK_API_VERSION_1_4)
    {
        core.features14 = &features.getCore14();
        core.properties14 = &m_core14;
    }
    return core;
}

}   // namespace gp::rhi::vulkan
