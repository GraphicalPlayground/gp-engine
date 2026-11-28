// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanWrappedPipelineStates.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanPipelineStateCacheManager.hpp"

namespace gp::rhi::vulkan
{

VulkanWrappedGraphicsPipelineState::VulkanWrappedGraphicsPipelineState(
    VulkanGraphicsPipelineState* const state
) noexcept
    : pipeline::GraphicsPipelineState()
    , m_state(state, gp::kRetainRef)
{}

VulkanWrappedGraphicsPipelineState::~VulkanWrappedGraphicsPipelineState() noexcept
{
    VulkanPipelineStateCacheManager* const owner = m_state.isValid() ? m_state->getOwner() : nullptr;
    if (owner != nullptr)
    {
        owner->releaseGraphicsWrapper(*m_state);
    }
}

shader::GraphicsShader*
    VulkanWrappedGraphicsPipelineState::getShader(const shader::ShaderFrequency frequency) const noexcept
{
    return m_state.isValid() ? m_state->getShader(frequency) : nullptr;
}

VulkanWrappedComputePipelineState::VulkanWrappedComputePipelineState(VulkanComputePipelineState* const state) noexcept
    : pipeline::ComputePipelineState(state->getComputeShader())
    , m_state(state, gp::kRetainRef)
{
    setPipelineValid(state->isValid());
}

VulkanWrappedComputePipelineState::~VulkanWrappedComputePipelineState() noexcept
{
    VulkanPipelineStateCacheManager* const owner = m_state.isValid() ? m_state->getOwner() : nullptr;
    if (owner != nullptr)
    {
        owner->releaseComputeWrapper(*m_state);
    }
}

}   // namespace gp::rhi::vulkan
