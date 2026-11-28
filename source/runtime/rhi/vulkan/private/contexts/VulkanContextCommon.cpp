// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanContextCommon.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanQueue.hpp"
#include "commands/VulkanSubmission.hpp"
#include "contexts/IVulkanPayloadSubmitter.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "logging/LogMacros.hpp"
#include "sync/VulkanSyncEnums.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanContextCommon::VulkanContextCommon(
    const VulkanContextServices& services, const VulkanCommandBufferType commandBufferType
) noexcept
    : m_services(services)
    , m_commandBufferType(commandBufferType)
{
    GP_CHECKF(m_services.device != nullptr, "VulkanContextCommon requires a device.");
    GP_CHECKF(m_services.queue != nullptr, "VulkanContextCommon requires a queue.");
    m_commandBufferPool = m_services.queue->acquireCommandBufferPool(commandBufferType);
}

VulkanContextCommon::~VulkanContextCommon() noexcept
{
    discardPayloads();
    if (m_commandBufferPool != nullptr)
    {
        m_services.queue->releaseCommandBufferPool(m_commandBufferPool);
        m_commandBufferPool = nullptr;
    }
}

VulkanPayload& VulkanContextCommon::getPayload(const VulkanContextPhase phase) noexcept
{
    if (m_payloads.isEmpty() || (phase < m_currentPhase))
    {
        newPayload();
    }
    m_currentPhase = phase;
    return *m_payloads.back();
}

VulkanCommandBuffer* VulkanContextCommon::getCommandBuffer() noexcept
{
    VulkanPayload& payload = getPayload(VulkanContextPhase::Execute);
    if (!payload.commandBuffers.isEmpty())
    {
        VulkanCommandBuffer* const current = payload.commandBuffers.back();
        if ((current != nullptr) && current->hasBegun())
        {
            return current;
        }
    }
    return prepareNewCommandBuffer(payload);
}

bool VulkanContextCommon::hasActiveCommandBuffer() const noexcept
{
    if (m_payloads.isEmpty() || (m_currentPhase != VulkanContextPhase::Execute))
    {
        return false;
    }
    const VulkanPayload& payload = *m_payloads.back();
    return !payload.commandBuffers.isEmpty() && (payload.commandBuffers.back() != nullptr) &&
           payload.commandBuffers.back()->hasBegun();
}

void VulkanContextCommon::newPayload() noexcept
{
    endPayload();

    VulkanPayload* const payload = new VulkanPayload(*m_services.queue);
    payload->queryPoolManager = m_services.queryPoolManager;
    m_payloads.pushBack(payload);
    m_currentPhase = VulkanContextPhase::Wait;
}

void VulkanContextCommon::endPayload() noexcept
{
    if (m_payloads.isEmpty())
    {
        return;
    }

    // Flushing can open a payload, so only flush once one exists to avoid recursing into newPayload().
    flushPendingSyncPoints();

    VulkanPayload& payload = *m_payloads.back();
    if (payload.commandBuffers.isEmpty())
    {
        return;
    }

    VulkanCommandBuffer* const commandBuffer = payload.commandBuffers.back();
    if ((commandBuffer == nullptr) || commandBuffer->hasEnded() || !commandBuffer->hasBegun())
    {
        return;
    }

    if (commandBuffer->isInsideRenderPass() && (m_commandBufferType == VulkanCommandBufferType::Primary))
    {
        GP_LOG_WARNING(LogVulkanRHI, "Payload closed inside an open render pass; ending the pass implicitly.");
        endActiveRenderPass(*commandBuffer);
    }

    if (!commandBuffer->end())
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to end a context command buffer.");
    }
}

void VulkanContextCommon::finalize(VulkanPlatformCommandList& outCommandList) noexcept
{
    flushPendingSyncPoints();

    if (m_contextSyncPoint)
    {
        VulkanPayload& signalPayload = getPayload(VulkanContextPhase::Signal);
        signalPayload.syncPoints.pushBack(m_contextSyncPoint);
        m_contextSyncPoint.reset();
    }

    if (!m_payloads.isEmpty())
    {
        onFinalize(getPayload(VulkanContextPhase::Signal));
    }

    endPayload();

    for (gp::USize index = 0u; index < m_payloads.size(); ++index)
    {
        outCommandList.payloads.pushBack(m_payloads[index]);
    }
    m_payloads.clear();
    m_currentPhase = VulkanContextPhase::Wait;
}

bool VulkanContextCommon::flushCommands(const VulkanFlushFlags flags) noexcept
{
    if (m_services.submitter == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "flushCommands() called on a context without a payload submitter.");
        return false;
    }

    const bool waitForCompletion = (flags & VulkanFlushFlags::WaitForCompletion) != VulkanFlushFlags::None;
    VulkanSyncPointRef completion{};
    if (waitForCompletion)
    {
        completion = VulkanSyncPoint::create(VulkanSyncPointType::GPUAndCPU, "ContextFlush");
        signalSyncPoint(completion);
    }

    VulkanPlatformCommandList commandList{};
    finalize(commandList);
    m_services.submitter->submitPayloads(commandList.payloads);

    if (completion)
    {
        return m_services.submitter->waitForSyncPoint(*completion, kVulkanContextFlushTimeoutNs);
    }
    return true;
}

void VulkanContextCommon::signalSyncPoint(const VulkanSyncPointRef& syncPoint) noexcept
{
    if (!syncPoint)
    {
        return;
    }
    getPayload(VulkanContextPhase::Signal).syncPoints.pushBack(syncPoint);
}

void VulkanContextCommon::addPendingSyncPoint(const VulkanSyncPointRef& syncPoint) noexcept
{
    if (syncPoint)
    {
        m_pendingSyncPoints.pushBack(syncPoint);
    }
}

void VulkanContextCommon::flushPendingSyncPoints() noexcept
{
    if (m_pendingSyncPoints.isEmpty())
    {
        return;
    }

    VulkanPayload& payload = getPayload(VulkanContextPhase::Signal);
    for (gp::USize index = 0u; index < m_pendingSyncPoints.size(); ++index)
    {
        payload.syncPoints.pushBack(std::move(m_pendingSyncPoints[index]));
    }
    m_pendingSyncPoints.clear();
}

void VulkanContextCommon::addSubmissionEvent(const VulkanSyncPointRef& syncPoint) noexcept
{
    if (!syncPoint)
    {
        return;
    }
    VulkanPayload& payload = m_payloads.isEmpty() ? getPayload(VulkanContextPhase::Wait) : *m_payloads.back();
    payload.submissionEvents.pushBack(syncPoint);
}

void VulkanContextCommon::addWaitSemaphore(
    const VkPipelineStageFlags waitFlags, const VulkanSemaphoreRef& semaphore
) noexcept
{
    if (!semaphore)
    {
        return;
    }
    VulkanPayload& payload = getPayload(VulkanContextPhase::Wait);
    payload.waitFlags.pushBack(waitFlags);
    payload.waitSemaphores.pushBack(semaphore);
}

void VulkanContextCommon::addSignalSemaphore(const VulkanSemaphoreRef& semaphore) noexcept
{
    if (semaphore)
    {
        getPayload(VulkanContextPhase::Signal).signalSemaphores.pushBack(semaphore);
    }
}

const VulkanSyncPointRef& VulkanContextCommon::getContextSyncPoint() noexcept
{
    if (!m_contextSyncPoint)
    {
        m_contextSyncPoint = VulkanSyncPoint::create(VulkanSyncPointType::Context, "ContextSyncPoint");
    }
    return m_contextSyncPoint;
}

void VulkanContextCommon::setParallelRenderPassInfo(const VulkanParallelRenderPassInfo* const info) noexcept
{
    m_hasParallelRenderPassInfo = (info != nullptr);
    m_parallelRenderPassInfo = (info != nullptr) ? *info : VulkanParallelRenderPassInfo{};
}

void VulkanContextCommon::endActiveRenderPass(VulkanCommandBuffer& commandBuffer) noexcept
{
    static_cast<void>(commandBuffer.endRenderPass());
}

void VulkanContextCommon::onCommandBufferBegun(VulkanCommandBuffer& commandBuffer) noexcept
{
    static_cast<void>(commandBuffer);
}

void VulkanContextCommon::onFinalize(VulkanPayload& signalPayload) noexcept
{
    static_cast<void>(signalPayload);
}

VulkanCommandBuffer* VulkanContextCommon::prepareNewCommandBuffer(VulkanPayload& payload) noexcept
{
    if (m_commandBufferPool == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Context has no command buffer pool; cannot record commands.");
        return nullptr;
    }

    VulkanCommandBuffer* const commandBuffer = m_commandBufferPool->acquireReadyCommandBuffer();
    if (commandBuffer == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Command buffer pool is exhausted.");
        return nullptr;
    }

    const bool isParallel = m_hasParallelRenderPassInfo;
    const bool hasBegun = isParallel ? commandBuffer->begin(
                                           m_parallelRenderPassInfo.renderPass,
                                           m_parallelRenderPassInfo.subpass,
                                           m_parallelRenderPassInfo.framebuffer
                                       )
                                     : commandBuffer->begin();
    if (!hasBegun)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to begin a context command buffer.");
        commandBuffer->reset();
        return nullptr;
    }

    payload.commandBuffers.pushBack(commandBuffer);
    onCommandBufferBegun(*commandBuffer);
    return commandBuffer;
}

void VulkanContextCommon::discardPayloads() noexcept
{
    m_pendingSyncPoints.clear();
    m_contextSyncPoint.reset();

    for (gp::USize payloadIndex = 0u; payloadIndex < m_payloads.size(); ++payloadIndex)
    {
        VulkanPayload* const payload = m_payloads[payloadIndex];
        for (VulkanCommandBuffer* const commandBuffer: payload->commandBuffers)
        {
            if (commandBuffer == nullptr)
            {
                continue;
            }
            if (commandBuffer->isInsideRenderPass() && (m_commandBufferType == VulkanCommandBufferType::Primary))
            {
                endActiveRenderPass(*commandBuffer);
            }
            if (commandBuffer->hasBegun())
            {
                static_cast<void>(commandBuffer->end());
            }
            commandBuffer->reset();
        }
        payload->commandBuffers.clear();
        delete payload;
    }
    m_payloads.clear();
    m_currentPhase = VulkanContextPhase::Wait;
}

}   // namespace gp::rhi::vulkan
