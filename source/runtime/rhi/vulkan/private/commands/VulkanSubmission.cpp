// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/VulkanSubmission.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "commands/VulkanQueue.hpp"
#include "CoreMinimal.hpp"
#include "descriptors/VulkanDescriptorPoolsManager.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "queries/VulkanQueryRecording.hpp"
#include "queries/VulkanQueryTypes.hpp"
#include "sync/VulkanFence.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanPayload::~VulkanPayload() noexcept
{
    if ((queryPoolManager != nullptr) && !queryPools.isEmpty())
    {
        releaseVulkanPayloadQueries(*queryPoolManager, queryPools);
    }

    releaseDescriptorPoolSets();

    if (fence != nullptr)
    {
        fence->getOwner().releaseFence(fence);
    }
}

void VulkanPayload::releaseDescriptorPoolSets() noexcept
{
    if (descriptorPoolSetContainers.isEmpty())
    {
        return;
    }

    GP_CHECKF(descriptorPoolsManager != nullptr, "A payload with descriptor pool sets needs a pools manager.");
    if (descriptorPoolsManager != nullptr)
    {
        for (VulkanDescriptorPoolSetContainer* const container: descriptorPoolSetContainers)
        {
            if (container != nullptr)
            {
                descriptorPoolsManager->releasePoolSet(*container);
            }
        }
    }
    descriptorPoolSetContainers.clear();
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

    if (!other.queryPools.isEmpty())
    {
        GP_CHECK(
            (queryPoolManager == nullptr) || (other.queryPoolManager == nullptr) ||
            (queryPoolManager == other.queryPoolManager)
        );
        if (queryPoolManager == nullptr)
        {
            queryPoolManager = other.queryPoolManager;
        }
        queryPools.append(other.queryPools);
    }

    if (!other.descriptorPoolSetContainers.isEmpty())
    {
        GP_CHECK(
            (descriptorPoolsManager == nullptr) || (other.descriptorPoolsManager == nullptr) ||
            (descriptorPoolsManager == other.descriptorPoolsManager)
        );
        if (descriptorPoolsManager == nullptr)
        {
            descriptorPoolsManager = other.descriptorPoolsManager;
        }
        for (VulkanDescriptorPoolSetContainer* const container: other.descriptorPoolSetContainers)
        {
            descriptorPoolSetContainers.pushBack(container);
        }
        other.descriptorPoolSetContainers.clear();
    }

    for (const VulkanCommitReservedResourceDesc& commit: other.reservedResourceCommits)
    {
        reservedResourceCommits.pushBack(commit);
    }
    other.reservedResourceCommits.clear();
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

    // Results must be written before the sync points that publish them are signaled.
    if (!queryPools.isEmpty())
    {
        GP_CHECKF(queryPoolManager != nullptr, "A payload with query pools needs a query pool manager.");
        if (queryPoolManager != nullptr)
        {
            resolveVulkanPayloadQueries(*queryPoolManager, queryPools);
        }
    }

    for (gp::USize index = 0u; index < syncPoints.size(); ++index)
    {
        if (syncPoints[index])
        {
            syncPoints[index]->signal();
        }
    }
    syncPoints.clear();

    releaseDescriptorPoolSets();
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
