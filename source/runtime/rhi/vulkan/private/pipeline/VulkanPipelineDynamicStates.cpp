// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanPipelineDynamicStates.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "pipeline/VulkanGraphicsPipelineDesc.hpp"

namespace gp::rhi::vulkan
{

VulkanDynamicStateFeatures makeVulkanDynamicStateFeatures(const VulkanDevice& device) noexcept
{
    const OptionalVulkanDeviceExtensions& extensions = device.getOptionalExtensions();

    VulkanDynamicStateFeatures features{};
    features.hasExtendedDynamicState1 = extensions.hasEXTExtendedDynamicState1 != 0u;
    features.hasExtendedDynamicState2 = extensions.hasEXTExtendedDynamicState2 != 0u;
    features.hasExtendedDynamicState3 = extensions.hasEXTExtendedDynamicState3 != 0u;
    features.hasVertexInputDynamicState = extensions.hasEXTVertexInputDynamicState != 0u;
    return features;
}

VulkanDynamicStateList
    fillVulkanDynamicStates(const VulkanDynamicStateFeatures& features, const VulkanGraphicsPipelineDesc& desc) noexcept
{
    const bool hasMeshShader = desc.hasMeshShader();

    VulkanDynamicStateList list{};

    // Core: the state cache sets these per draw on every device.
    list.add(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    list.add(VK_DYNAMIC_STATE_DEPTH_BOUNDS);

    if (features.hasExtendedDynamicState1)
    {
        // Mesh pipelines have no input assembly stage, so their topology cannot be dynamic.
        if (!hasMeshShader)
        {
            list.add(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT);
        }
        list.add(VK_DYNAMIC_STATE_CULL_MODE_EXT);
        list.add(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT);
        list.add(VK_DYNAMIC_STATE_STENCIL_OP_EXT);
        list.add(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT);
        list.add(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT);
        list.add(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT);
        list.add(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT);
        list.add(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT);
        list.add(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT);
    }
    else
    {
        list.add(VK_DYNAMIC_STATE_VIEWPORT);
        list.add(VK_DYNAMIC_STATE_SCISSOR);
    }

    if (features.hasExtendedDynamicState2)
    {
        list.add(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT);
        list.add(VK_DYNAMIC_STATE_DEPTH_BIAS);
    }

    if (features.hasExtendedDynamicState3)
    {
        // Per-attachment blend states are meaningless without color attachments.
        if (!desc.colorAttachmentStates.isEmpty())
        {
            list.add(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT);
            list.add(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
            list.add(VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT);
        }
        list.add(VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
        list.add(VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT);
        list.add(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT);
    }

    if (features.hasVertexInputDynamicState && !hasMeshShader)
    {
        list.add(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT);
    }

    return list;
}

}   // namespace gp::rhi::vulkan
