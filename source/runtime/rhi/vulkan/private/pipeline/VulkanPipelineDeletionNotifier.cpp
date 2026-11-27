// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanPipelineDeletionNotifier.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"
#include "pipeline/IVulkanPipelineDeletionListener.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Process-wide listener slot; acquire/release ordering publishes the listener's state to destructors.
gp::thread::Atomic<IVulkanPipelineDeletionListener*> g_pipelineDeletionListener{ nullptr };

}   // namespace

IVulkanPipelineDeletionListener*
    setVulkanPipelineDeletionListener(IVulkanPipelineDeletionListener* const listener) noexcept
{
    return g_pipelineDeletionListener.exchange(listener, gp::memory::MemoryOrder::AcqRel);
}

IVulkanPipelineDeletionListener* getVulkanPipelineDeletionListener() noexcept
{
    return g_pipelineDeletionListener.load(gp::memory::MemoryOrder::Acquire);
}

void notifyVulkanGraphicsPipelineDeleted(const VulkanGraphicsPipelineState& pipeline) noexcept
{
    IVulkanPipelineDeletionListener* const listener = getVulkanPipelineDeletionListener();
    if (listener != nullptr)
    {
        listener->onGraphicsPipelineDeleted(pipeline);
    }
}

void notifyVulkanComputePipelineDeleted(const VulkanComputePipelineState& pipeline) noexcept
{
    IVulkanPipelineDeletionListener* const listener = getVulkanPipelineDeletionListener();
    if (listener != nullptr)
    {
        listener->onComputePipelineDeleted(pipeline);
    }
}

}   // namespace gp::rhi::vulkan
