// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanGraphicsPipelineState.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Maps a graphics shader frequency to its pipeline stage slot.
/// @param[in] frequency Shader frequency.
/// @param[out] outStage Stage slot.
/// @return False when `frequency` is not a graphics stage.
[[nodiscard]] bool toGraphicsStage(const shader::ShaderFrequency frequency, shader_stage::Stage& outStage) noexcept
{
    switch (frequency)
    {
    case shader::ShaderFrequency::Vertex:
        outStage = shader_stage::Stage::Vertex;
        return true;
    case shader::ShaderFrequency::Pixel:
        outStage = shader_stage::Stage::Pixel;
        return true;
    case shader::ShaderFrequency::Geometry:
        outStage = shader_stage::Stage::Geometry;
        return true;
    case shader::ShaderFrequency::Mesh:
        outStage = shader_stage::Stage::Mesh;
        return true;
    case shader::ShaderFrequency::Amplification:
        outStage = shader_stage::Stage::Task;
        return true;
    default:
        break;
    }
    return false;
}

}   // namespace

VulkanGraphicsPipelineState::VulkanGraphicsPipelineState(
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    VulkanPipelineStateCacheManager* const owner,
    VulkanGraphicsPipelineDesc&& desc,
    VulkanPipelineKey&& key
) noexcept
    : VulkanCommonPipelineState(device, deferredDeletionQueue, owner)
    , m_desc(std::move(desc))
    , m_key(std::move(key))
{}

VulkanGraphicsPipelineState::~VulkanGraphicsPipelineState() noexcept
{
    GP_ASSERT(!m_isInLRU, "A graphics pipeline was destroyed while still linked in the LRU.");
}

void VulkanGraphicsPipelineState::bind(VulkanCommandBuffer& commandBuffer) const noexcept
{
    if ((m_pipeline == VK_NULL_HANDLE) || (dynamic_api::vkCmdBindPipeline == nullptr))
    {
        return;
    }
    vkCmdBindPipeline(commandBuffer.getHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
}

shader::GraphicsShader* VulkanGraphicsPipelineState::getShader(const shader::ShaderFrequency frequency) const noexcept
{
    shader_stage::Stage stage = shader_stage::Stage::Vertex;
    if (!toGraphicsStage(frequency, stage))
    {
        return nullptr;
    }
    return m_rhiShaders[static_cast<gp::USize>(stage)].get();
}

}   // namespace gp::rhi::vulkan
