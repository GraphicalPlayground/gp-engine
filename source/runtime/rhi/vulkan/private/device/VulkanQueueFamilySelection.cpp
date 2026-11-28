// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanQueueFamilySelection.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanAllowAsyncCompute(
    "r.Vulkan.AllowAsyncCompute",
    1,
    "0 to disable async compute queue, 1 to allow async compute queue if available (default).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanAllowTransferQueue(
    "r.Vulkan.AllowTransferQueue",
    0,
    "0 to disable dedicated transfer queue (default), 1 to allow transfer queue if available.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanAllowPresentOnComputeQueue(
    "r.Vulkan.AllowPresentOnComputeQueue",
    0,
    "0 to present on the graphics queue (default), 1 to allow presenting on the compute queue if available.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

}   // namespace

VulkanQueueFamilySelectionConfig
    resolveQueueFamilySelectionConfig(const bool supportsParallelRendering, const bool hasMaintenance9) noexcept
{
    VulkanQueueFamilySelectionConfig config{};
    config.allowAsyncCompute = (g_cvarVulkanAllowAsyncCompute.getValueOnAnyThread(true) != 0);
    config.allowPresentOnComputeQueue = (g_cvarVulkanAllowPresentOnComputeQueue.getValueOnAnyThread(true) != 0);
    config.forceAsyncCompute = gp::CommandLine::hasArgument("ForceAsyncCompute");
    config.disableAsyncCompute = gp::CommandLine::hasArgument("DisableAsyncCompute");
    config.allowTransferQueue = (g_cvarVulkanAllowTransferQueue.getValueOnAnyThread(true) != 0);
    config.supportsParallelRendering = supportsParallelRendering;
    config.hasMaintenance9 = hasMaintenance9;
    return config;
}

VulkanQueueFamilyIndices selectQueueFamilies(
    const gp::ConstVectorView<VkQueueFamilyProperties> queueFamilyProps, const VulkanQueueFamilySelectionConfig& config
) noexcept
{
    VulkanQueueFamilyIndices indices{};
    indices.activeQueueFamilies.reserve(kVulkanQueueTypeCount);

    const bool asyncComputePermitted =
        (config.allowAsyncCompute || config.allowPresentOnComputeQueue || config.forceAsyncCompute) &&
        !config.disableAsyncCompute && config.supportsParallelRendering && config.hasMaintenance9;

    const bool transferPermitted =
        config.allowTransferQueue && config.supportsParallelRendering && config.hasMaintenance9;

    for (gp::USize familyIndex = 0u; familyIndex < queueFamilyProps.size(); ++familyIndex)
    {
        const VkQueueFamilyProperties& props = queueFamilyProps[familyIndex];
        if (props.queueCount == 0u)
        {
            continue;
        }

        const gp::Int32 familyInt = static_cast<gp::Int32>(familyIndex);
        bool selected = false;

        if ((props.queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
        {
            if (indices.graphicsFamily == -1)
            {
                indices.graphicsFamily = familyInt;
                selected = true;
            }
        }

        if ((props.queueFlags & VK_QUEUE_COMPUTE_BIT) == VK_QUEUE_COMPUTE_BIT)
        {
            if (indices.asyncComputeFamily == -1 && asyncComputePermitted && (indices.graphicsFamily != familyInt))
            {
                indices.asyncComputeFamily = familyInt;
                selected = true;
            }
        }

        if ((props.queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT)
        {
            const bool isDedicatedTransfer = (props.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) == 0u;
            if (indices.transferFamily == -1 && isDedicatedTransfer && transferPermitted &&
                (indices.graphicsFamily != familyInt) && (indices.asyncComputeFamily != familyInt))
            {
                indices.transferFamily = familyInt;
                selected = true;
            }
        }

        if (selected)
        {
            indices.activeQueueFamilies.pushBack(static_cast<gp::UInt32>(familyIndex));
        }
    }

    return indices;
}

void buildDeviceQueueCreateInfos(
    const gp::ConstVectorView<VkQueueFamilyProperties> queueFamilyProps,
    const VulkanQueueFamilyIndices& indices,
    gp::Vector<VkDeviceQueueCreateInfo>& outCreateInfos,
    gp::Vector<gp::Float32>& outPriorities
) noexcept
{
    outCreateInfos.clear();
    outPriorities.clear();

    gp::UInt32 totalPriorities = 0u;
    for (const gp::UInt32 familyIndex: indices.activeQueueFamilies)
    {
        if (familyIndex < queueFamilyProps.size())
        {
            totalPriorities += queueFamilyProps[familyIndex].queueCount;
        }
    }

    outCreateInfos.reserve(indices.activeQueueFamilies.size());
    outPriorities.resize(totalPriorities, 1.0f);

    gp::USize priorityOffset = 0u;
    for (const gp::UInt32 familyIndex: indices.activeQueueFamilies)
    {
        if (familyIndex >= queueFamilyProps.size())
        {
            continue;
        }

        const gp::UInt32 queueCount = queueFamilyProps[familyIndex].queueCount;
        if (queueCount == 0u)
        {
            continue;
        }

        VkDeviceQueueCreateInfo createInfo =
            zeroVulkanStruct<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
        createInfo.queueFamilyIndex = familyIndex;
        createInfo.queueCount = queueCount;
        createInfo.pQueuePriorities = &outPriorities[priorityOffset];
        outCreateInfos.pushBack(createInfo);

        priorityOffset += queueCount;
    }
}

}   // namespace gp::rhi::vulkan
