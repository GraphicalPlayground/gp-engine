// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/VulkanCommandEnums.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "contexts/IVulkanPayloadSubmitter.hpp"
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief `IVulkanPayloadSubmitter` that submits straight to the target queues and polls interrupts.
/// @details Each payload is enqueued on its own `VulkanPayload::queue`, then every touched queue is drained with
///          `submitQueuedPayloads()`. Waiting polls `processInterruptQueue()` on those queues, which completes payloads
///          (resetting command buffers, resolving queries, signaling sync points) on the calling thread.
/// @note Not thread-safe; intended for the single-threaded bring-up path and unit tests. The dynamic RHI replaces
///       it with a submitter backed by its submission and interrupt threads.
class GP_RHI_VULKAN_API VulkanImmediatePayloadSubmitter final : public IVulkanPayloadSubmitter
{
public:
    /// @brief Constructs a submitter.
    /// @param[in] useSubmit2 True to submit through `vkQueueSubmit2` (`synchronization2`).
    explicit VulkanImmediatePayloadSubmitter(bool useSubmit2 = false) noexcept;

    /// @brief Destroys the submitter; payloads already handed to queues stay owned by them.
    virtual ~VulkanImmediatePayloadSubmitter() noexcept override = default;

    VulkanImmediatePayloadSubmitter(const VulkanImmediatePayloadSubmitter&) = delete;
    VulkanImmediatePayloadSubmitter& operator=(const VulkanImmediatePayloadSubmitter&) = delete;
    VulkanImmediatePayloadSubmitter(VulkanImmediatePayloadSubmitter&&) = delete;
    VulkanImmediatePayloadSubmitter& operator=(VulkanImmediatePayloadSubmitter&&) = delete;

public:
    /// @copydoc IVulkanPayloadSubmitter::submitPayloads()
    /// @note Null payloads are skipped; submission errors are logged by the queue.
    virtual void submitPayloads(gp::Vector<VulkanPayload*>& payloads) noexcept override;

    /// @copydoc IVulkanPayloadSubmitter::waitForSyncPoint()
    virtual bool waitForSyncPoint(const VulkanSyncPoint& syncPoint, gp::UInt64 timeoutNs) noexcept override;

    /// @brief Returns how many distinct queues this submitter has submitted to.
    /// @return Number of queues polled by `waitForSyncPoint()`.
    [[nodiscard]] gp::USize getNumKnownQueues() const noexcept
    {
        return m_knownQueues.size();
    }

    /// @brief Returns the total number of payloads handed to queues so far.
    /// @return Cumulative submitted payload count.
    [[nodiscard]] gp::UInt64 getNumSubmittedPayloads() const noexcept
    {
        return m_numSubmittedPayloads;
    }

private:
    /// @brief Appends `queue` to the polled set when it is not already present.
    /// @param[in] queue Queue a payload was enqueued on.
    void rememberQueue(VulkanQueue& queue) noexcept;

private:
    gp::Vector<VulkanQueue*> m_knownQueues{};
    VulkanSignaledSemaphoreTracker m_signaledSemaphores{};
    gp::UInt64 m_numSubmittedPayloads{ 0u };
    bool m_useSubmit2{ false };
};

}   // namespace gp::rhi::vulkan
