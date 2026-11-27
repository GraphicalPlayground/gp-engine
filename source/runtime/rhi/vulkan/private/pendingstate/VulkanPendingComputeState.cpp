// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanPendingComputeState.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSettings.hpp"
#include "logging/LogMacros.hpp"
#include "pendingstate/VulkanComputePipelineDescriptorState.hpp"
#include "pipeline/VulkanComputePipelineState.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanPendingComputeState::VulkanPendingComputeState(
    VulkanDevice& device, IVulkanPendingStateContext& context, const VulkanDescriptorDefaults& defaults
) noexcept
    : m_device(device)
    , m_context(context)
    , m_defaults(defaults)
    , m_useDescriptorCache(useVulkanDescriptorCache())
{}

VulkanPendingComputeState::~VulkanPendingComputeState() noexcept = default;

void VulkanPendingComputeState::setComputePipeline(const VulkanComputePipelineState& pipeline) noexcept
{
    if (m_currentPipeline == &pipeline)
    {
        return;
    }

    VulkanComputePipelineDescriptorState* state = nullptr;
    for (Entry& entry: m_pipelineStates)
    {
        if (entry.pipeline == &pipeline)
        {
            state = entry.state.get();
            break;
        }
    }
    if (state == nullptr)
    {
        Entry entry{};
        entry.pipeline = &pipeline;
        entry.state =
            gp::makeUnique<VulkanComputePipelineDescriptorState>(m_device, pipeline, m_defaults, m_useDescriptorCache);
        state = entry.state.get();
        m_pipelineStates.pushBack(std::move(entry));
    }

    m_currentPipeline = &pipeline;
    m_currentState = state;
    m_currentState->reset();
}

bool VulkanPendingComputeState::prepareForDispatch(VulkanCommandBuffer& commandBuffer) noexcept
{
    if ((m_currentPipeline == nullptr) || (m_currentState == nullptr))
    {
        GP_LOG_WARNING(LogVulkanRHI, "Dispatch without a compute pipeline.");
        return false;
    }

    const bool hasDescriptorSets = m_currentState->updateDescriptorSets(m_context);
    if (!hasDescriptorSets && (m_currentState->getUsedSetsMask() != 0u))
    {
        return false;
    }

    m_currentPipeline->bind(commandBuffer);
    if (hasDescriptorSets)
    {
        m_currentState->bindDescriptorSets(commandBuffer.getHandle());
    }
    return true;
}

void VulkanPendingComputeState::notifyDeletedPipeline(const VulkanComputePipelineState& pipeline) noexcept
{
    if (m_currentPipeline == &pipeline)
    {
        reset();
    }
    for (gp::USize index = 0u; index < static_cast<gp::USize>(m_pipelineStates.size()); ++index)
    {
        if (m_pipelineStates[index].pipeline == &pipeline)
        {
            m_pipelineStates.removeAt(index);
            return;
        }
    }
}

void VulkanPendingComputeState::reset() noexcept
{
    m_currentPipeline = nullptr;
    m_currentState = nullptr;
}

}   // namespace gp::rhi::vulkan
