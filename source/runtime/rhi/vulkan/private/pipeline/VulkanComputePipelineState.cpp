// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanComputePipelineState.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "hashing/XxHash.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "pipeline/VulkanPipelineDeletionNotifier.hpp"
#include "shaders/VulkanBaseShader.hpp"

namespace gp::rhi::vulkan
{

VulkanComputePipelineState::VulkanComputePipelineState(
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    VulkanPipelineStateCacheManager* const owner,
    shader::ComputeShader* const computeShader,
    const shader::SpecializationContainer& specializationConstants,
    const gp::UInt64 key
) noexcept
    : VulkanCommonPipelineState(device, deferredDeletionQueue, owner)
    , m_computeShader(computeShader, gp::kRetainRef)
    , m_vulkanShader(static_cast<VulkanComputeShader*>(computeShader))
    , m_specializationConstants(specializationConstants)
    , m_key(key)
{}

VulkanComputePipelineState::~VulkanComputePipelineState() noexcept
{
    // Contexts compare raw pointers to skip redundant binds; clear them before this address can be reused.
    notifyVulkanComputePipelineDeleted(*this);
}

void VulkanComputePipelineState::bind(VulkanCommandBuffer& commandBuffer) const noexcept
{
    if ((m_pipeline == VK_NULL_HANDLE) || (dynamic_api::vkCmdBindPipeline == nullptr))
    {
        return;
    }
    if (commandBuffer.getCurrentComputePipeline() != m_pipeline)
    {
        vkCmdBindPipeline(commandBuffer.getHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
        commandBuffer.setCurrentComputePipeline(m_pipeline);
    }
}

shader::ComputeShader* VulkanComputePipelineState::getComputeShader() const noexcept
{
    return m_computeShader.get();
}

gp::UInt64 computeVulkanComputePipelineKey(
    const gp::UInt64 shaderKey, const shader::SpecializationContainer& specializationConstants
) noexcept
{
    gp::hashing::XxHash64 hasher(shaderKey);
    if (!specializationConstants.isEmpty())
    {
        hasher.update(
            specializationConstants.data(), specializationConstants.size() * sizeof(shader::SpecializationKey)
        );
    }
    return hasher.finalize();
}

}   // namespace gp::rhi::vulkan
