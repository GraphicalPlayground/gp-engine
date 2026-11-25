// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/IComputeContext.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "templates/Function.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Atomic unit of GPU work queued for submission to a `VulkanQueue` and tracked until interrupt completion.
/// @details Bundles command buffers, wait/signal semaphores, submission-time and GPU-completion `VulkanSyncPoint`
///          handles, timeline value or pooled `VulkanFence`, and an optional pre-submit callback.
struct GP_RHI_VULKAN_API VulkanPayload final
{
    /// @brief Constructs a payload bound to `targetQueue`.
    /// @param[in] targetQueue Target `VulkanQueue` that will submit and complete this payload.
    explicit VulkanPayload(VulkanQueue& targetQueue) noexcept
        : queue(targetQueue)
    {}

    /// @brief Destructor. Releases any attached `VulkanFence` back to its owning `VulkanFenceManager`.
    ~VulkanPayload() noexcept;

    VulkanPayload(const VulkanPayload&) = delete;
    VulkanPayload& operator=(const VulkanPayload&) = delete;
    VulkanPayload(VulkanPayload&&) = delete;
    VulkanPayload& operator=(VulkanPayload&&) = delete;

    /// @brief Target queue owning submission and completion of this payload.
    VulkanQueue& queue;

    /// @brief Pipeline stage wait masks corresponding 1:1 with `waitSemaphores`.
    gp::Vector<VkPipelineStageFlags> waitFlags{};

    /// @brief Semaphores that must be signaled before this payload's command buffers execute.
    gp::Vector<VulkanSemaphoreRef> waitSemaphores{};

    /// @brief Recorded command buffers submitted in order by this payload.
    gp::Vector<VulkanCommandBuffer*> commandBuffers{};

    /// @brief Semaphores signaled upon GPU completion of this payload.
    gp::Vector<VulkanSemaphoreRef> signalSemaphores{};

    /// @brief Sync points signaled on the CPU immediately after the payload is submitted to the driver queue.
    gp::Vector<VulkanSyncPointRef> submissionEvents{};

    /// @brief Sync points signaled on the CPU after the GPU finishes executing this payload.
    gp::Vector<VulkanSyncPointRef> syncPoints{};

    /// @brief Monotonic queue timeline semaphore counter value assigned during submission (0 when fences are used).
    gp::UInt64 timelineSemaphoreValue{ 0u };

    /// @brief Pooled `VulkanFence` tracking completion when timeline semaphores are not active.
    VulkanFence* fence{ nullptr };

    /// @brief Optional callback executed on the submission thread immediately prior to `vkQueueSubmit`.
    gp::UniqueFunction<void(VkQueue)> preExecuteCallback{};

    /// @brief Invokes `preExecuteCallback` (if bound) with the target queue's native `VkQueue` handle.
    void preExecute() noexcept;

    /// @brief Merges command buffers and completion sync points from a secondary or parallel payload into this payload.
    /// @note `other` must not carry wait or signal semaphores, and must not contain `Primary` command buffers.
    /// @param[in,out] other Source payload whose command buffers and sync points are appended and drained.
    void merge(VulkanPayload& other) noexcept;

    /// @brief Marks this payload finished on the GPU: resets all command buffers and signals every `syncPoints` entry.
    void complete() noexcept;
};

/// @brief Concrete `gp::rhi::command::IPlatformCommandList` carrying a batch of finalized `VulkanPayload` objects.
/// @details Owned by the RHI command list submission pipeline until handed to `VulkanQueue::enqueuePayload()`. Any
///          unsubmitted payloads still present at destruction time are automatically deleted to prevent leaks.
class GP_RHI_VULKAN_API VulkanPlatformCommandList final : public gp::rhi::command::IPlatformCommandList
{
public:
    /// @brief Default constructor.
    VulkanPlatformCommandList() noexcept = default;

    /// @brief Destructor. Deletes any payloads still owned by this list.
    virtual ~VulkanPlatformCommandList() noexcept override;

    VulkanPlatformCommandList(const VulkanPlatformCommandList&) = delete;
    VulkanPlatformCommandList& operator=(const VulkanPlatformCommandList&) = delete;
    VulkanPlatformCommandList(VulkanPlatformCommandList&&) = delete;
    VulkanPlatformCommandList& operator=(VulkanPlatformCommandList&&) = delete;

    /// @brief Ordered list of finalized payloads ready for queue submission.
    gp::Vector<VulkanPayload*> payloads{};
};

}   // namespace gp::rhi::vulkan
