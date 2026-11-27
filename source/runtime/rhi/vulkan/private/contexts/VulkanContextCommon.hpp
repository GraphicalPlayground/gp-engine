// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/VulkanCommandEnums.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "contexts/VulkanContextDesc.hpp"
#include "contexts/VulkanContextEnums.hpp"
#include "contexts/VulkanContextTypes.hpp"
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Payload, command buffer and sync point bookkeeping shared by every Vulkan command context.
/// @details A context records into an ordered list of `VulkanPayload`s. Each payload walks the phases
///          Wait -> UpdateReservedResources -> Execute -> Signal; requesting an earlier phase than the current one
///          closes the payload and opens the next, which preserves the GPU ordering the caller expressed on the CPU.
///          Command buffers are acquired lazily from a pool owned by the context and begun on first use.
///          `finalize()` closes everything and moves the payloads into a `VulkanPlatformCommandList` that the
///          submitter owns from then on.
/// @note A context is recorded by one thread at a time. Nothing here is internally synchronized.
class GP_RHI_VULKAN_API VulkanContextCommon
{
public:
    /// @brief Constructs a context bound to `services.queue` and acquires a command buffer pool from it.
    /// @param[in] services Device-level services; `device` and `queue` are required.
    /// @param[in] commandBufferType Primary for top-level contexts, Parallel for render pass slices.
    VulkanContextCommon(const VulkanContextServices& services, VulkanCommandBufferType commandBufferType) noexcept;

    /// @brief Discards unfinalized payloads (their command buffers are ended and recycled) and releases the pool.
    virtual ~VulkanContextCommon() noexcept;

    VulkanContextCommon(const VulkanContextCommon&) = delete;
    VulkanContextCommon& operator=(const VulkanContextCommon&) = delete;
    VulkanContextCommon(VulkanContextCommon&&) = delete;
    VulkanContextCommon& operator=(VulkanContextCommon&&) = delete;

public:
    /// @brief Returns the payload to append `phase` work to, opening a new payload when ordering requires it.
    /// @param[in] phase Phase the caller is about to record.
    /// @return Payload whose current phase is now `phase`.
    [[nodiscard]] VulkanPayload& getPayload(VulkanContextPhase phase) noexcept;

    /// @brief Returns the command buffer being recorded, acquiring and beginning one when none is active.
    /// @return Active command buffer, or `nullptr` when the pool could not provide one.
    [[nodiscard]] VulkanCommandBuffer* getCommandBuffer() noexcept;

    /// @brief Reports whether a begun, not yet ended command buffer is active.
    /// @return True when `getCommandBuffer()` would not acquire a new buffer.
    [[nodiscard]] bool hasActiveCommandBuffer() const noexcept;

    /// @brief Closes the current payload and opens a fresh one in the Wait phase.
    void newPayload() noexcept;

    /// @brief Ends the current payload's command buffer and appends pending sync points to it.
    /// @note A command buffer still inside a render pass is force-closed through `endActiveRenderPass()` with a
    ///       warning; leaving a pass open across a payload boundary is a caller bug.
    void endPayload() noexcept;

    /// @brief Closes the context and moves every payload into `outCommandList`.
    /// @details Pending sync points and the context sync point land in the last payload's Signal phase, so the
    ///          context sync point completes only after every command recorded here.
    /// @param[out] outCommandList Receives the payloads in submission order.
    void finalize(VulkanPlatformCommandList& outCommandList) noexcept;

    /// @brief Finalizes, submits through the services submitter and optionally waits.
    /// @param[in] flags `WaitForCompletion` blocks until the GPU finished the flushed work.
    /// @return True when submission happened (and, if requested, completion was observed in time).
    bool flushCommands(VulkanFlushFlags flags = VulkanFlushFlags::None) noexcept;

    /// @brief Adds `syncPoint` to the Signal phase so it fires when the GPU reaches this point.
    /// @param[in] syncPoint Sync point to signal; ignored when null.
    void signalSyncPoint(const VulkanSyncPointRef& syncPoint) noexcept;

    /// @brief Defers `syncPoint` to the end of the current payload instead of splitting it now.
    /// @param[in] syncPoint Sync point to signal at the next `endPayload()`; ignored when null.
    void addPendingSyncPoint(const VulkanSyncPointRef& syncPoint) noexcept;

    /// @brief Appends every deferred sync point to the current payload's Signal phase.
    void flushPendingSyncPoints() noexcept;

    /// @brief Adds `syncPoint` to the current payload's submission events (signaled on CPU submit).
    /// @param[in] syncPoint Sync point to signal at submission time; ignored when null.
    void addSubmissionEvent(const VulkanSyncPointRef& syncPoint) noexcept;

    /// @brief Makes the next recorded work wait on `semaphore` at `waitFlags`.
    /// @param[in] waitFlags Pipeline stages that must wait.
    /// @param[in] semaphore Semaphore to wait on; ignored when null.
    void addWaitSemaphore(VkPipelineStageFlags waitFlags, const VulkanSemaphoreRef& semaphore) noexcept;

    /// @brief Makes the current work signal `semaphore` once it completes.
    /// @param[in] semaphore Semaphore to signal; ignored when null.
    void addSignalSemaphore(const VulkanSemaphoreRef& semaphore) noexcept;

    /// @brief Returns the lazily-created sync point that completes with this context's work.
    /// @details Transient allocations (uniform ring, staging) tie their lifetime to this sync point.
    /// @return Context sync point, valid until the next `finalize()`.
    [[nodiscard]] const VulkanSyncPointRef& getContextSyncPoint() noexcept;

    /// @brief Sets the render pass continued by parallel command buffers.
    /// @param[in] info Parent render pass description, or `nullptr` for top-level recording.
    void setParallelRenderPassInfo(const VulkanParallelRenderPassInfo* info) noexcept;

    /// @brief Returns the device the context records for.
    /// @return Logical device.
    [[nodiscard]] VulkanDevice& getDevice() const noexcept
    {
        return *m_services.device;
    }

    /// @brief Returns the queue the context's payloads target.
    /// @return Target queue.
    [[nodiscard]] VulkanQueue& getQueue() const noexcept
    {
        return *m_services.queue;
    }

    /// @brief Returns the borrowed services bundle.
    /// @return Services passed at construction.
    [[nodiscard]] const VulkanContextServices& getServices() const noexcept
    {
        return m_services;
    }

    /// @brief Returns the phase of the current payload.
    /// @return Current phase; Wait when no payload is open.
    [[nodiscard]] VulkanContextPhase getCurrentPhase() const noexcept
    {
        return m_currentPhase;
    }

    /// @brief Returns how many payloads are recorded but not yet finalized.
    /// @return Payload count.
    [[nodiscard]] gp::USize getNumPayloads() const noexcept
    {
        return m_payloads.size();
    }

    /// @brief Returns how many sync points wait for the next `endPayload()`.
    /// @return Deferred sync point count.
    [[nodiscard]] gp::USize getNumPendingSyncPoints() const noexcept
    {
        return m_pendingSyncPoints.size();
    }

    /// @brief Returns the command buffer type this context acquires.
    /// @return Primary or Parallel.
    [[nodiscard]] VulkanCommandBufferType getCommandBufferType() const noexcept
    {
        return m_commandBufferType;
    }

protected:
    /// @brief Ends a render pass left open when a payload closes.
    /// @param[in,out] commandBuffer Command buffer inside a render pass.
    /// @note Derived contexts override this so dynamic rendering ends with `vkCmdEndRendering`.
    virtual void endActiveRenderPass(VulkanCommandBuffer& commandBuffer) noexcept;

    /// @brief Called after a fresh command buffer begins, before any command is recorded into it.
    /// @param[in,out] commandBuffer Newly begun command buffer.
    virtual void onCommandBufferBegun(VulkanCommandBuffer& commandBuffer) noexcept;

    /// @brief Called by `finalize()` with the payload that will carry the context sync point.
    /// @param[in,out] signalPayload Last payload of the context, in its Signal phase.
    virtual void onFinalize(VulkanPayload& signalPayload) noexcept;

private:
    /// @brief Acquires, begins and appends a command buffer to the Execute payload.
    /// @param[in,out] payload Payload in the Execute phase.
    /// @return New command buffer, or `nullptr` on failure.
    [[nodiscard]] VulkanCommandBuffer* prepareNewCommandBuffer(VulkanPayload& payload) noexcept;

    /// @brief Ends, recycles and deletes every payload without submitting it.
    void discardPayloads() noexcept;

private:
    VulkanContextServices m_services{};
    VulkanCommandBufferType m_commandBufferType{ VulkanCommandBufferType::Primary };
    VulkanCommandBufferPool* m_commandBufferPool{ nullptr };
    gp::Vector<VulkanPayload*> m_payloads{};
    gp::Vector<VulkanSyncPointRef> m_pendingSyncPoints{};
    VulkanSyncPointRef m_contextSyncPoint{};
    VulkanParallelRenderPassInfo m_parallelRenderPassInfo{};
    VulkanContextPhase m_currentPhase{ VulkanContextPhase::Wait };
    bool m_hasParallelRenderPassInfo{ false };
};

}   // namespace gp::rhi::vulkan
