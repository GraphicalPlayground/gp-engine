// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanGraphicsPipelineDescriptorState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "descriptors/VulkanPipelineLayout.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "pendingstate/IVulkanPendingStateContext.hpp"
#include "pipeline/VulkanGraphicsPipelineState.hpp"
#include "pipeline/VulkanPipelineTypes.hpp"
#include "shaders/VulkanShader.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Returns the pipeline layout of a graphics pipeline.
/// @param[in] pipeline Graphics pipeline; must have a layout.
/// @return Pipeline layout.
[[nodiscard]] const VulkanPipelineLayout&
    getGraphicsPipelineLayout(const VulkanGraphicsPipelineState& pipeline) noexcept
{
    GP_CHECKF(pipeline.getLayout() != nullptr, "A graphics pipeline without a layout has no descriptor state.");
    return *pipeline.getLayout();
}

/// @brief Collects the code header of every stage of a graphics pipeline.
/// @param[in] pipeline Graphics pipeline.
/// @return Per-stage header table; null entries for unused stages.
[[nodiscard]] VulkanUniformBufferGatherInfo makeGraphicsHeaders(const VulkanGraphicsPipelineState& pipeline) noexcept
{
    VulkanUniformBufferGatherInfo headers{};
    for (gp::UInt32 stage = 0u; stage < kNumGraphicsPipelineStages; ++stage)
    {
        const VulkanShader* const shader = pipeline.getVulkanShader(static_cast<shader_stage::Stage>(stage));
        headers.codeHeaders[stage] = (shader != nullptr) ? &shader->getCodeHeader() : nullptr;
    }
    return headers;
}

}   // namespace

VulkanGraphicsPipelineDescriptorState::VulkanGraphicsPipelineDescriptorState(
    VulkanDevice& device,
    const VulkanGraphicsPipelineState& pipeline,
    const VulkanDescriptorDefaults& defaults,
    const bool useDescriptorCache
) noexcept
    : VulkanGraphicsPipelineDescriptorState(
          device,
          getGraphicsPipelineLayout(pipeline).getDescriptorSetsLayout(),
          getGraphicsPipelineLayout(pipeline).getPipelineLayout(),
          makeGraphicsHeaders(pipeline),
          defaults,
          useDescriptorCache
      )
{
    m_pipeline = &pipeline;
}

VulkanGraphicsPipelineDescriptorState::VulkanGraphicsPipelineDescriptorState(
    VulkanDevice& device,
    const VulkanDescriptorSetsLayout& layout,
    const VkPipelineLayout pipelineLayout,
    const VulkanUniformBufferGatherInfo& codeHeaders,
    const VulkanDescriptorDefaults& defaults,
    const bool useDescriptorCache
) noexcept
    : VulkanCommonPipelineDescriptorState(device, layout, pipelineLayout, codeHeaders, defaults, useDescriptorCache)
{
    GP_CHECKF(
        layout.getBindPoint() == VK_PIPELINE_BIND_POINT_GRAPHICS, "A graphics descriptor state needs a graphics layout."
    );
}

bool VulkanGraphicsPipelineDescriptorState::hasInputAttachments() const noexcept
{
    return getDescriptorSetsLayout().hasInputAttachments();
}

bool VulkanGraphicsPipelineDescriptorState::updateInputAttachments(const IVulkanPendingStateContext& context) noexcept
{
    if (!hasInputAttachments())
    {
        return false;
    }

    bool hasChanged = false;
    for (gp::UInt32 stage = 0u; stage < kNumGraphicsPipelineStages; ++stage)
    {
        const VulkanShaderHeader* const header = getCodeHeader(static_cast<shader_stage::Stage>(stage));
        if (header == nullptr)
        {
            continue;
        }
        for (const VulkanShaderInputAttachmentInfo& attachmentInfo: header->inputAttachmentInfos)
        {
            VulkanTextureView view{};
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
            if (!context.getInputAttachmentView(attachmentInfo.type, view, layout))
            {
                // Outside a render pass the binding keeps its default view; the draw is invalid anyway.
                continue;
            }
            hasChanged = setInputAttachment(
                             static_cast<shader_stage::Stage>(stage), attachmentInfo.bindingIndex, view, layout
                         ) ||
                         hasChanged;
        }
    }
    return hasChanged;
}

}   // namespace gp::rhi::vulkan
