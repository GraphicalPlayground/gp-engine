// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/VulkanCommandEnums.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanQueueFamilySelection.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Hardware Vulkan command queue wrapper managing submission, timeline/fence completion, and command-pool reuse.
/// @details Supports `Graphics`, `AsyncCompute`, and `Transfer` roles. Submission queues payloads in FIFO order, gates
///          submissions on binary wait-semaphore readiness, dispatches via `vkQueueSubmit` or `vkQueueSubmit2KHR`, and
///          completes in-flight payloads via timeline semaphore counter queries or pooled `VulkanFence` objects.
class GP_RHI_VULKAN_API VulkanQueue final
{
public:
    /// @brief Constructs a `VulkanQueue` for `familyIndex`, `queueType`, and `queueIndex`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] fenceManager Fence pool manager used when timeline semaphores are disabled or unavailable.
    /// @param[in] familyIndex Hardware queue family index.
    /// @param[in] queueType Queue role (`Graphics`, `AsyncCompute`, or `Transfer`).
    /// @param[in] queueIndex Queue index within `familyIndex` (defaults to `0`).
    VulkanQueue(
        VulkanDevice& device,
        VulkanFenceManager& fenceManager,
        gp::UInt32 familyIndex,
        VulkanQueueType queueType,
        gp::UInt32 queueIndex = 0u
    ) noexcept;

    /// @brief Destructor. Destroys all pooled command buffer pools and any uncompleted payloads.
    ~VulkanQueue() noexcept;

    VulkanQueue(const VulkanQueue&) = delete;
    VulkanQueue& operator=(const VulkanQueue&) = delete;
    VulkanQueue(VulkanQueue&&) = delete;
    VulkanQueue& operator=(VulkanQueue&&) = delete;

    /// @brief Computes valid `VkPipelineStageFlags` and `VkAccessFlags` supported by a queue family and device feature
    ///        set.
    /// @param[in] queueFlags `VkQueueFlags` reported by `VkQueueFamilyProperties::queueFlags`.
    /// @param[in] coreFeatures Enabled Vulkan 1.0 `VkPhysicalDeviceFeatures`.
    /// @param[in] optionalExtensions Enabled optional device extensions.
    /// @param[out] outSupportedStages Resulting supported pipeline stage bitmask.
    /// @param[out] outSupportedAccess Resulting supported memory access bitmask.
    static void computeSupportedStagesAndAccess(
        VkQueueFlags queueFlags,
        const VkPhysicalDeviceFeatures& coreFeatures,
        const OptionalVulkanDeviceExtensions& optionalExtensions,
        VkPipelineStageFlags& outSupportedStages,
        VkAccessFlags& outSupportedAccess
    ) noexcept;

    /// @brief Returns the queue role (`Graphics`, `AsyncCompute`, or `Transfer`).
    /// @return `VulkanQueueType`.
    [[nodiscard]] VulkanQueueType getQueueType() const noexcept
    {
        return m_queueType;
    }

    /// @brief Returns the hardware queue family index.
    /// @return Queue family index.
    [[nodiscard]] gp::UInt32 getFamilyIndex() const noexcept
    {
        return m_familyIndex;
    }

    /// @brief Returns the queue index within `m_familyIndex`.
    /// @return Queue index.
    [[nodiscard]] gp::UInt32 getQueueIndex() const noexcept
    {
        return m_queueIndex;
    }

    /// @brief Returns the native `VkQueue` handle.
    /// @return Native `VkQueue`, or `VK_NULL_HANDLE` if the device is invalid.
    [[nodiscard]] VkQueue getHandle() const noexcept
    {
        return m_queue;
    }

    /// @brief Reports whether this queue holds a valid native `VkQueue` handle.
    /// @return True when `m_queue != VK_NULL_HANDLE`.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_queue != VK_NULL_HANDLE;
    }

    /// @brief Returns the mask of `VkPipelineStageFlags` legal on this queue.
    /// @return Supported pipeline stage bitmask.
    [[nodiscard]] VkPipelineStageFlags getSupportedStageBits() const noexcept
    {
        return m_supportedStages;
    }

    /// @brief Returns the mask of `VkAccessFlags` legal on this queue.
    /// @return Supported access bitmask.
    [[nodiscard]] VkAccessFlags getSupportedAccessBits() const noexcept
    {
        return m_supportedAccess;
    }

    /// @brief Returns the queue's timeline semaphore, or a null ref when timeline semaphores are inactive.
    /// @return Const reference to `VulkanSemaphoreRef`.
    [[nodiscard]] const VulkanSemaphoreRef& getTimelineSemaphore() const noexcept
    {
        return m_timelineSemaphore;
    }

    /// @brief Reports whether this queue tracks completion via a timeline semaphore rather than per-submit fences.
    /// @return True when `m_timelineSemaphore` is valid.
    [[nodiscard]] bool usesTimelineSemaphore() const noexcept
    {
        return static_cast<bool>(m_timelineSemaphore) && m_timelineSemaphore->isValid();
    }

    /// @brief Returns the most recently assigned timeline counter value submitted on this queue.
    /// @return Last submitted timeline value.
    [[nodiscard]] gp::UInt64 getSubmittedTimelineValue() const noexcept
    {
        return m_timelineSemaphoreValue.load();
    }

    /// @brief Returns the highest timeline counter value observed complete on the GPU.
    /// @return Last completed timeline value.
    [[nodiscard]] gp::UInt64 getCompletedTimelineValue() const noexcept
    {
        return m_completedTimelineValue.load();
    }

    /// @brief Acquires an idle `VulkanCommandBufferPool` for `type` or creates a new one for `m_familyIndex`.
    /// @param[in] type Command buffer role (`Primary`, `Secondary`, or `Parallel`).
    /// @return Pointer to a valid `VulkanCommandBufferPool`, or `nullptr` if creation failed.
    [[nodiscard]] VulkanCommandBufferPool* acquireCommandBufferPool(VulkanCommandBufferType type) noexcept;

    /// @brief Returns a previously acquired `VulkanCommandBufferPool` to this queue's free pool list.
    /// @param[in] pool Pool to return (ignored if null).
    void releaseCommandBufferPool(VulkanCommandBufferPool* pool) noexcept;

    /// @brief Enqueues `payload` for submission on this queue (takes ownership of `payload`).
    /// @param[in] payload Heap-allocated `VulkanPayload` (ignored if null).
    void enqueuePayload(VulkanPayload* payload) noexcept;

    /// @brief Submits all dependency-ready queued payloads to the native `VkQueue`.
    /// @details Stops at the first payload waiting on a non-external binary semaphore not yet present in
    ///          `signaledSemaphores`, allowing cross-queue callers to round-robin across queues without deadlock.
    /// @param[in,out] signaledSemaphores Cross-queue tracker of signaled binary semaphores.
    /// @param[in] useSubmit2 When true and `vkQueueSubmit2KHR` is available, dispatches using Synchronization2.
    /// @return Number of payloads submitted during this call.
    gp::Int32
        submitQueuedPayloads(VulkanSignaledSemaphoreTracker& signaledSemaphores, bool useSubmit2 = false) noexcept;

    /// @brief Directly submits `submitInfos` to `m_queue` via `vkQueueSubmit`, optionally signaling `fence`.
    /// @param[in] submitInfos Span of `VkSubmitInfo` descriptors.
    /// @param[in] fence Optional `VulkanFence` signaled upon completion.
    /// @return `VkResult` returned by `vkQueueSubmit` (or `VK_ERROR_INITIALIZATION_FAILED` if queue is invalid).
    [[nodiscard]] VkResult submit(gp::ConstVectorView<VkSubmitInfo> submitInfos, VulkanFence* fence = nullptr) noexcept;

    /// @brief Directly submits `submitInfos` to `m_queue` via `vkQueueSubmit2KHR`, optionally signaling `fence`.
    /// @param[in] submitInfos Span of `VkSubmitInfo2` descriptors.
    /// @param[in] fence Optional `VulkanFence` signaled upon completion.
    /// @return `VkResult` returned by `vkQueueSubmit2KHR` (or `VK_ERROR_EXTENSION_NOT_PRESENT` if unavailable).
    [[nodiscard]] VkResult
        submit2(gp::ConstVectorView<VkSubmitInfo2> submitInfos, VulkanFence* fence = nullptr) noexcept;

    /// @brief Polls (or waits up to `timeoutNanoseconds` on) in-flight payloads, completing and deleting finished ones.
    /// @param[in] timeoutNanoseconds Optional wait timeout in nanoseconds (`0` performs a non-blocking poll).
    /// @return Number of payloads completed during this call.
    gp::Int32 processInterruptQueue(gp::UInt64 timeoutNanoseconds = 0u) noexcept;

    /// @brief Blocks until all work submitted to `m_queue` has finished (`vkQueueWaitIdle`).
    void waitUntilIdle() const noexcept;

    /// @brief Returns the number of payloads currently queued and waiting to be submitted.
    /// @return Pending submission count.
    [[nodiscard]] gp::USize getPendingSubmissionCount() const noexcept;

    /// @brief Returns the number of payloads currently submitted and awaiting GPU completion.
    /// @return Pending interrupt count.
    [[nodiscard]] gp::USize getPendingInterruptCount() const noexcept;

private:
    /// @brief Populates `m_supportedStages` and `m_supportedAccess` from the owning device's queue family properties.
    void fillSupportedStageBits() noexcept;

    /// @brief Returns true when every binary wait semaphore in `payload` is externally signaled or tracked in
    ///        `signaledSemaphores`.
    /// @param[in] payload Candidate payload.
    /// @param[in] signaledSemaphores Cross-queue signaled semaphore tracker.
    /// @return True when ready to submit.
    [[nodiscard]] static bool areWaitSemaphoresReady(
        const VulkanPayload& payload, const VulkanSignaledSemaphoreTracker& signaledSemaphores
    ) noexcept;

    /// @brief Submits a single `VulkanPayload` using legacy `vkQueueSubmit`.
    /// @param[in,out] payload Payload to submit.
    /// @return `VkResult` from `vkQueueSubmit`.
    [[nodiscard]] VkResult submitPayloadLegacy(VulkanPayload& payload) noexcept;

    /// @brief Submits a single `VulkanPayload` using `vkQueueSubmit2KHR`.
    /// @param[in,out] payload Payload to submit.
    /// @return `VkResult` from `vkQueueSubmit2KHR`.
    [[nodiscard]] VkResult submitPayloadSync2(VulkanPayload& payload) noexcept;

    VulkanDevice& m_device;
    VulkanFenceManager& m_fenceManager;
    VkQueue m_queue{ VK_NULL_HANDLE };
    gp::UInt32 m_familyIndex{ 0u };
    gp::UInt32 m_queueIndex{ 0u };
    VulkanQueueType m_queueType{ VulkanQueueType::Graphics };

    VkPipelineStageFlags m_supportedStages{ 0u };
    VkAccessFlags m_supportedAccess{ 0u };

    VulkanSemaphoreRef m_timelineSemaphore{};
    gp::thread::Atomic<gp::UInt64> m_timelineSemaphoreValue{ 0u };
    gp::thread::Atomic<gp::UInt64> m_completedTimelineValue{ 0u };

    mutable gp::thread::Mutex m_queueExecutionMutex{};

    mutable gp::thread::Mutex m_poolMutex{};
    gp::Array<gp::Vector<VulkanCommandBufferPool*>, kVulkanCommandBufferTypeCount> m_freeCommandBufferPools{};

    mutable gp::thread::Mutex m_submissionMutex{};
    gp::Vector<VulkanPayload*> m_pendingSubmission{};

    mutable gp::thread::Mutex m_interruptConsumerMutex{};
    mutable gp::thread::Mutex m_interruptMutex{};
    gp::Vector<VulkanPayload*> m_pendingInterrupt{};
};

}   // namespace gp::rhi::vulkan
