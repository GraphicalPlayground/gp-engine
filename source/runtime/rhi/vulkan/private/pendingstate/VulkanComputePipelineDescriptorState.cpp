// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanComputePipelineDescriptorState.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSetsLayout.hpp"
#include "descriptors/VulkanDescriptorSetsTypes.hpp"
#include "descriptors/VulkanPipelineLayout.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "pipeline/VulkanComputePipelineState.hpp"
#include "shaders/VulkanShader.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Returns the pipeline layout of a compute pipeline.
/// @param[in] pipeline Compute pipeline; must have a layout.
/// @return Pipeline layout.
[[nodiscard]] const VulkanPipelineLayout& getComputePipelineLayout(const VulkanComputePipelineState& pipeline) noexcept
{
    GP_CHECKF(pipeline.getLayout() != nullptr, "A compute pipeline without a layout has no descriptor state.");
    return *pipeline.getLayout();
}

/// @brief Returns the code header of a compute pipeline's shader.
/// @param[in] pipeline Compute pipeline; must have a shader.
/// @return Code header.
[[nodiscard]] const VulkanShaderHeader& getComputeHeader(const VulkanComputePipelineState& pipeline) noexcept
{
    GP_CHECKF(pipeline.getVulkanShader() != nullptr, "A compute pipeline without a shader has no descriptor state.");
    return pipeline.getVulkanShader()->getCodeHeader();
}

/// @brief Places the compute header in the compute stage slot.
/// @param[in] computeHeader Code header of the compute shader.
/// @return Per-stage header table.
[[nodiscard]] VulkanUniformBufferGatherInfo makeComputeHeaders(const VulkanShaderHeader& computeHeader) noexcept
{
    VulkanUniformBufferGatherInfo headers{};
    headers.codeHeaders[static_cast<gp::USize>(shader_stage::Stage::Compute)] = &computeHeader;
    return headers;
}

}   // namespace

VulkanComputePipelineDescriptorState::VulkanComputePipelineDescriptorState(
    VulkanDevice& device,
    const VulkanComputePipelineState& pipeline,
    const VulkanDescriptorDefaults& defaults,
    const bool useDescriptorCache
) noexcept
    : VulkanComputePipelineDescriptorState(
          device,
          getComputePipelineLayout(pipeline).getDescriptorSetsLayout(),
          getComputePipelineLayout(pipeline).getPipelineLayout(),
          getComputeHeader(pipeline),
          defaults,
          useDescriptorCache
      )
{
    m_pipeline = &pipeline;
}

VulkanComputePipelineDescriptorState::VulkanComputePipelineDescriptorState(
    VulkanDevice& device,
    const VulkanDescriptorSetsLayout& layout,
    const VkPipelineLayout pipelineLayout,
    const VulkanShaderHeader& computeHeader,
    const VulkanDescriptorDefaults& defaults,
    const bool useDescriptorCache
) noexcept
    : VulkanCommonPipelineDescriptorState(
          device, layout, pipelineLayout, makeComputeHeaders(computeHeader), defaults, useDescriptorCache
      )
{
    GP_CHECKF(
        layout.getBindPoint() == VK_PIPELINE_BIND_POINT_COMPUTE, "A compute descriptor state needs a compute layout."
    );
}

}   // namespace gp::rhi::vulkan
