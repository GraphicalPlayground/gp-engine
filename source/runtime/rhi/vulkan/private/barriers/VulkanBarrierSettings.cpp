// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanBarrierSettings.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierEnums.hpp"
#include "commands/VulkanQueue.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "CoreMinimal.hpp"
#include "device/RHIFeatureHelpers.hpp"
#include "device/RHIGlobals.hpp"
#include "device/VulkanDevice.hpp"
#include "device/VulkanQueueFamilySelection.hpp"
#include "extensions/VulkanExtensionTypes.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarUseMemoryBarrierOpt(
    "r.Vulkan.UseMemoryBarrierOpt",
    1,
    "Simplify buffer barriers and image barriers without layout transitions to a memory barrier.\n"
    " 0: Do not collapse to a single memory barrier, useful for tracking single resource transitions in external "
    "tools\n"
    " 1: Collapse to a memory barrier when appropriate (default)",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarMaxBarriersPerBatch(
    "r.Vulkan.MaxBarriersPerBatch",
    -1,
    "Will limit the number of barriers sent per batch\n"
    " <=0: Do not limit (default)\n"
    " >0: Limit to the specified number\n",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarAllowSplitBarriers(
    "r.Vulkan.AllowSplitBarriers",
    1,
    "Controls the use of split barriers\n"
    " 0: Disable split barriers\n"
    " 1: Allow split barriers using Synchronization2 events (default)\n",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarAllowConcurrentImage(
    "r.Vulkan.AllowConcurrentImage",
    0,
    "When async compute is supported: \n"
    " 0 to use queue family ownership transfers with images (default)\n"
    " 1 to use sharing mode concurrent with images",
    gp::console::ConsoleVarFlags::ReadOnly | gp::console::ConsoleVarFlags::RenderThreadSafe
);

/// @brief Picks the shading rate image encoding exposed by the enabled extensions.
/// @param[in] extensions Enabled optional device extensions.
/// @param[in] properties Retained extension feature structures.
/// @return `Palette` for attachment fragment shading rate, `Fractional` for fragment density maps, else
///         `NotSupported`.
[[nodiscard]] VulkanShadingRateImageType resolveShadingRateImageType(
    const OptionalVulkanDeviceExtensions& extensions, const OptionalVulkanDeviceExtensionProperties& properties
) noexcept
{
    if ((extensions.hasKHRFragmentShadingRate != 0u) &&
        (properties.fragmentShadingRateFeatures.attachmentFragmentShadingRate == VK_TRUE))
    {
        return VulkanShadingRateImageType::Palette;
    }

    if ((extensions.hasEXTFragmentDensityMap != 0u) &&
        (properties.fragmentDensityMapFeatures.fragmentDensityMap == VK_TRUE))
    {
        return VulkanShadingRateImageType::Fractional;
    }

    return VulkanShadingRateImageType::NotSupported;
}

}   // namespace

VulkanBarrierSettings getVulkanBarrierSettings() noexcept
{
    VulkanBarrierSettings settings{};
    settings.useMemoryBarrierOpt = g_cvarUseMemoryBarrierOpt.getValueOnRenderThread() != 0;
    settings.maxBarriersPerBatch = g_cvarMaxBarriersPerBatch.getValueOnRenderThread();
    settings.allowSplitBarriers = g_cvarAllowSplitBarriers.getValueOnRenderThread() != 0;
    settings.allowConcurrentImage = g_cvarAllowConcurrentImage.getValueOnRenderThread() != 0;
    return settings;
}

VkPipelineStageFlags
    computeVulkanGraphicsPipelineStageBits(const bool geometryShaderEnabled, const bool meshShaderEnabled) noexcept
{
    // Vertex and fragment stages are supported by every device.
    VkPipelineStageFlags stageBits = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    if (geometryShaderEnabled)
    {
        stageBits |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    }

    if (meshShaderEnabled)
    {
        stageBits |= VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;
    }

    return stageBits;
}

VulkanBarrierTranslationDesc makeVulkanBarrierTranslationDesc(const VulkanDevice& device) noexcept
{
    const OptionalVulkanDeviceExtensions& extensions = device.getOptionalExtensions();

    VulkanBarrierTranslationDesc desc{};
    desc.graphicsPipelineStageBits = computeVulkanGraphicsPipelineStageBits(
        device.getFeatures().getCore10().geometryShader == VK_TRUE, extensions.hasEXTMeshShader != 0u
    );
    desc.supportsRayTracingShaders = getRHIGlobals().rayTracing.supported && areRayTracingShadersEnabled();
    desc.shadingRateImageType = resolveShadingRateImageType(extensions, device.getOptionalExtensionProperties());

    // Dedicated non-pixel SRV tracking is not exposed by the RHI yet, so the conservative stripping path stays on.
    desc.supportsDedicatedSRVGraphicsNonPixelAccess = false;
    return desc;
}

VulkanBarrierDeviceDesc makeVulkanBarrierDeviceDesc(const VulkanDevice& device) noexcept
{
    const VulkanQueueFamilyIndices& indices = device.getQueueFamilyIndices();
    const gp::ConstVectorView<VkQueueFamilyProperties> familyProps = device.getQueueFamilyProperties();

    VulkanBarrierDeviceDesc desc{};
    desc.supportsParallelRendering = device.supportsParallelRendering();
    desc.needsQueueFamilyOwnershipTransfers = indices.needsQueueFamilyOwnershipTransfers;
    desc.hasAsyncComputeQueue = device.hasAsyncComputeQueue();
    desc.hasKHRMaintenance9 = device.getOptionalExtensions().hasKHRMaintenance9 != 0u;
    desc.graphicsQueueFamilyIndex =
        (indices.graphicsFamily >= 0) ? static_cast<gp::UInt32>(indices.graphicsFamily) : 0u;
    desc.computeQueueFamilyIndex = (desc.hasAsyncComputeQueue && indices.asyncComputeFamily >= 0)
                                       ? static_cast<gp::UInt32>(indices.asyncComputeFamily)
                                       : desc.graphicsQueueFamilyIndex;

    if (desc.computeQueueFamilyIndex < familyProps.size())
    {
        VulkanQueue::computeSupportedStagesAndAccess(
            familyProps[desc.computeQueueFamilyIndex].queueFlags,
            device.getFeatures().getCore10(),
            device.getOptionalExtensions(),
            desc.computeSupportedStages,
            desc.computeSupportedAccess
        );
    }

    return desc;
}

}   // namespace gp::rhi::vulkan
