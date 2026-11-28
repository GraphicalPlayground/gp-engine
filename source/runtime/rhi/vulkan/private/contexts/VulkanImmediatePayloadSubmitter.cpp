// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanImmediatePayloadSubmitter.hpp"
#include "commands/VulkanQueue.hpp"
#include "commands/VulkanSubmission.hpp"
#include "CoreMinimal.hpp"
#include "platforms/base/system/Time.hpp"
#include "sync/VulkanSyncPoint.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Per-queue interrupt poll slice so one idle queue cannot starve the others (one millisecond).
constexpr gp::UInt64 kInterruptPollSliceNs = 1'000'000ULL;

}   // namespace

VulkanImmediatePayloadSubmitter::VulkanImmediatePayloadSubmitter(const bool useSubmit2) noexcept
    : m_useSubmit2(useSubmit2)
{}

void VulkanImmediatePayloadSubmitter::submitPayloads(gp::Vector<VulkanPayload*>& payloads) noexcept
{
    gp::Vector<VulkanQueue*> touchedQueues{};
    for (gp::USize index = 0u; index < payloads.size(); ++index)
    {
        VulkanPayload* const payload = payloads[index];
        if (payload == nullptr)
        {
            continue;
        }

        VulkanQueue& queue = payload->queue;
        rememberQueue(queue);

        bool isTouched = false;
        for (VulkanQueue* const touched: touchedQueues)
        {
            isTouched = isTouched || (touched == &queue);
        }
        if (!isTouched)
        {
            touchedQueues.pushBack(&queue);
        }

        // Ownership moves to the queue, which deletes the payload once its interrupt completes.
        queue.enqueuePayload(payload);
        ++m_numSubmittedPayloads;
    }
    payloads.clear();

    for (VulkanQueue* const queue: touchedQueues)
    {
        static_cast<void>(queue->submitQueuedPayloads(m_signaledSemaphores, m_useSubmit2));
    }
}

bool VulkanImmediatePayloadSubmitter::waitForSyncPoint(
    const VulkanSyncPoint& syncPoint, const gp::UInt64 timeoutNs
) noexcept
{
    const gp::UInt64 startNs = gp::platform::Time::nowNanoseconds();
    while (!syncPoint.isComplete())
    {
        for (VulkanQueue* const queue: m_knownQueues)
        {
            // Waits may be satisfied by payloads blocked on semaphores of another queue; resubmit first.
            static_cast<void>(queue->submitQueuedPayloads(m_signaledSemaphores, m_useSubmit2));
            static_cast<void>(queue->processInterruptQueue(kInterruptPollSliceNs));
        }

        if (m_knownQueues.isEmpty() || ((gp::platform::Time::nowNanoseconds() - startNs) >= timeoutNs))
        {
            return syncPoint.isComplete();
        }
    }
    return true;
}

void VulkanImmediatePayloadSubmitter::rememberQueue(VulkanQueue& queue) noexcept
{
    for (VulkanQueue* const known: m_knownQueues)
    {
        if (known == &queue)
        {
            return;
        }
    }
    m_knownQueues.pushBack(&queue);
}

}   // namespace gp::rhi::vulkan
