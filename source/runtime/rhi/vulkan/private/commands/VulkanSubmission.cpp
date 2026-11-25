// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/VulkanSubmission.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "commands/VulkanQueue.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "sync/VulkanFence.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanPayload::~VulkanPayload() noexcept
{
    if (fence != nullptr)
    {
        fence->getOwner().releaseFence(fence);
    }
}

void VulkanPayload::preExecute() noexcept
{
    if (preExecuteCallback.isBound())
    {
        preExecuteCallback(queue.getHandle());
    }
}

void VulkanPayload::merge(VulkanPayload& other) noexcept
{
    GP_CHECK(other.signalSemaphores.isEmpty());
    GP_CHECK(other.waitSemaphores.isEmpty());
    GP_CHECK(other.submissionEvents.isEmpty());
    GP_CHECK(!other.preExecuteCallback.isBound());

    if (!other.commandBuffers.isEmpty())
    {
        GP_CHECKF(
            other.commandBuffers[0u] == nullptr ||
                other.commandBuffers[0u]->getCommandBufferType() != VulkanCommandBufferType::Primary,
            "Merging payloads is only valid for Secondary or Parallel command buffers."
        );

        for (gp::USize index = 0u; index < other.commandBuffers.size(); ++index)
        {
            commandBuffers.pushBack(other.commandBuffers[index]);
        }
        other.commandBuffers.clear();
    }

    for (gp::USize index = 0u; index < other.syncPoints.size(); ++index)
    {
        syncPoints.pushBack(std::move(other.syncPoints[index]));
    }
    other.syncPoints.clear();
}

void VulkanPayload::complete() noexcept
{
    for (gp::USize index = 0u; index < commandBuffers.size(); ++index)
    {
        VulkanCommandBuffer* const cmdBuffer = commandBuffers[index];
        if (cmdBuffer != nullptr)
        {
            cmdBuffer->reset();
        }
    }
    commandBuffers.clear();

    for (gp::USize index = 0u; index < syncPoints.size(); ++index)
    {
        if (syncPoints[index])
        {
            syncPoints[index]->signal();
        }
    }
    syncPoints.clear();
}

VulkanPlatformCommandList::~VulkanPlatformCommandList() noexcept
{
    for (gp::USize index = 0u; index < payloads.size(); ++index)
    {
        delete payloads[index];
    }
    payloads.clear();
}

}   // namespace gp::rhi::vulkan
