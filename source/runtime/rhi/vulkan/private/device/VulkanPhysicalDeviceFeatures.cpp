// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanPhysicalDeviceFeatures.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "CoreMinimal.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanRobustBufferAccess(
    "r.Vulkan.RobustBufferAccess",
    1,
    "0 to disable robust buffer access, 1 to enable (default).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanAllowImageRobustness(
    "r.Vulkan.AllowImageRobustness",
    1,
    "0 to disable robust image access, 1 to allow when supported (default).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanAllowSparseResidency(
    "r.Vulkan.AllowSparseResidency",
    1,
    "Toggles sparse residency for buffers and images (0=disable, 1=allow when supported).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

}   // namespace

VulkanPhysicalDeviceFeatures::VulkanPhysicalDeviceFeatures() noexcept
{
    m_core10 = zeroVulkanStruct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    m_core11 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan11Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
    m_core12 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
    m_core13 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
    m_core14 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan14Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES);
}

void VulkanPhysicalDeviceFeatures::query(const VkPhysicalDevice physicalDevice, const gp::UInt32 apiVersion) noexcept
{
    m_apiVersion = apiVersion;

    m_core10 = zeroVulkanStruct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    m_core11 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan11Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
    m_core12 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
    m_core13 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
    m_core14 =
        zeroVulkanStruct<VkPhysicalDeviceVulkan14Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES);

    if (physicalDevice == VK_NULL_HANDLE)
    {
        return;
    }

    if (dynamic_api::vkGetPhysicalDeviceFeatures2 != nullptr)
    {
        if (m_apiVersion >= VK_API_VERSION_1_2)
        {
            // Vulkan 1.1 feature struct was introduced in Vulkan 1.2.
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

        vkGetPhysicalDeviceFeatures2(physicalDevice, &m_core10);

        // Detach internal pNext links so copying or re-chaining in addToCreateDevice is always clean.
        m_core10.pNext = nullptr;
        m_core11.pNext = nullptr;
        m_core12.pNext = nullptr;
        m_core13.pNext = nullptr;
        m_core14.pNext = nullptr;
    }
    else if (dynamic_api::vkGetPhysicalDeviceFeatures != nullptr)
    {
        vkGetPhysicalDeviceFeatures(physicalDevice, &m_core10.features);
    }

    removeUnusedFeatures();
}

void VulkanPhysicalDeviceFeatures::removeUnusedFeatures() noexcept
{
    if (g_cvarVulkanRobustBufferAccess.getValueOnAnyThread(true) == 0)
    {
        m_core10.features.robustBufferAccess = VK_FALSE;
    }

    if (g_cvarVulkanAllowImageRobustness.getValueOnAnyThread(true) == 0)
    {
        m_core13.robustImageAccess = VK_FALSE;
    }

    if (g_cvarVulkanAllowSparseResidency.getValueOnAnyThread(true) == 0)
    {
        m_core10.features.shaderResourceResidency = VK_FALSE;
        m_core10.features.shaderResourceMinLod = VK_FALSE;
        m_core10.features.sparseBinding = VK_FALSE;
        m_core10.features.sparseResidencyBuffer = VK_FALSE;
        m_core10.features.sparseResidencyImage2D = VK_FALSE;
        m_core10.features.sparseResidencyImage3D = VK_FALSE;
        m_core10.features.sparseResidency2Samples = VK_FALSE;
        m_core10.features.sparseResidency4Samples = VK_FALSE;
        m_core10.features.sparseResidency8Samples = VK_FALSE;
        m_core10.features.sparseResidencyAliased = VK_FALSE;
    }

    // Disable core features not used by the engine.
    m_core11.multiviewTessellationShader = VK_FALSE;
    m_core13.inlineUniformBlock = VK_FALSE;
    m_core13.descriptorBindingInlineUniformBlockUpdateAfterBind = VK_FALSE;
    m_core13.privateData = VK_FALSE;
    m_core14.rectangularLines = VK_FALSE;
    m_core14.bresenhamLines = VK_FALSE;
    m_core14.smoothLines = VK_FALSE;
    m_core14.stippledRectangularLines = VK_FALSE;
    m_core14.stippledBresenhamLines = VK_FALSE;
    m_core14.stippledSmoothLines = VK_FALSE;
}

void VulkanPhysicalDeviceFeatures::addToCreateDevice(VkDeviceCreateInfo& deviceInfo, const bool useCoreStructs) noexcept
{
    if ((m_apiVersion >= VK_API_VERSION_1_2) && useCoreStructs)
    {
        deviceInfo.pEnabledFeatures = nullptr;
        m_core10.pNext = nullptr;
        m_core11.pNext = nullptr;
        m_core12.pNext = nullptr;
        m_core13.pNext = nullptr;
        m_core14.pNext = nullptr;

        addToPNext(deviceInfo, m_core10);
        addToPNext(deviceInfo, m_core11);
        addToPNext(deviceInfo, m_core12);

        if (m_apiVersion >= VK_API_VERSION_1_3)
        {
            addToPNext(deviceInfo, m_core13);
        }

        if (m_apiVersion >= VK_API_VERSION_1_4)
        {
            addToPNext(deviceInfo, m_core14);
        }
    }
    else
    {
        deviceInfo.pEnabledFeatures = &m_core10.features;
    }
}

}   // namespace gp::rhi::vulkan
