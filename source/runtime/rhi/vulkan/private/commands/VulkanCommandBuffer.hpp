// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/VulkanCommandEnums.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Default inactivity window (10 seconds in nanoseconds) before idle command buffers are considered stale.
inline constexpr gp::UInt64 kDefaultStaleCommandBufferTimeoutNs = 10'000'000'000ULL;

/// @brief Recording state machine and dynamic state cache wrapping a native `VkCommandBuffer`.
/// @details Instances are allocated, recycled, and destroyed exclusively by their owning `VulkanCommandBufferPool`.
///          Tracks recording lifecycle (`ReadyForBegin` -> `IsInsideBegin` -> `IsInsideRenderPass` -> `HasEnded` ->
///          `Submitted` -> `NeedReset`), secondary buffer execution, split-barrier event resets, and cached dynamic
///          viewport/scissor/stencil state.
class GP_RHI_VULKAN_API VulkanCommandBuffer final
{
public:
    VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
    VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;
    VulkanCommandBuffer(VulkanCommandBuffer&&) = delete;
    VulkanCommandBuffer& operator=(VulkanCommandBuffer&&) = delete;

    /// @brief Returns the native `VkCommandBuffer` handle.
    /// @return Native `VkCommandBuffer`, or `VK_NULL_HANDLE` when not allocated.
    [[nodiscard]] VkCommandBuffer getHandle() const noexcept
    {
        return m_handle;
    }

    /// @brief Reports whether this wrapper owns a valid native `VkCommandBuffer`.
    /// @return True when `m_handle != VK_NULL_HANDLE`.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_handle != VK_NULL_HANDLE;
    }

    /// @brief Returns the command buffer role (`Primary`, `Secondary`, or `Parallel`).
    /// @return `VulkanCommandBufferType`.
    [[nodiscard]] VulkanCommandBufferType getCommandBufferType() const noexcept
    {
        return m_commandBufferType;
    }

    /// @brief Returns the device that owns this command buffer.
    /// @return Owning `VulkanDevice`.
    [[nodiscard]] VulkanDevice& getDevice() const noexcept
    {
        return m_device;
    }

    /// @brief Returns the current recording/submission lifecycle state.
    /// @return `VulkanCommandBufferState`.
    [[nodiscard]] VulkanCommandBufferState getState() const noexcept
    {
        return m_state;
    }

    /// @brief Reports whether the command buffer is currently open for recording outside a render pass.
    /// @return True when state is `IsInsideBegin`.
    [[nodiscard]] bool isOutsideRenderPass() const noexcept
    {
        return m_state == VulkanCommandBufferState::IsInsideBegin;
    }

    /// @brief Reports whether the command buffer is currently recording inside a render pass or dynamic rendering.
    /// @return True when state is `IsInsideRenderPass`.
    [[nodiscard]] bool isInsideRenderPass() const noexcept
    {
        return m_state == VulkanCommandBufferState::IsInsideRenderPass;
    }

    /// @brief Reports whether `begin()` has been called and `end()` has not yet been called.
    /// @return True when state is `IsInsideBegin` or `IsInsideRenderPass`.
    [[nodiscard]] bool hasBegun() const noexcept
    {
        return (m_state == VulkanCommandBufferState::IsInsideBegin) ||
               (m_state == VulkanCommandBufferState::IsInsideRenderPass);
    }

    /// @brief Reports whether `end()` has completed and the buffer is ready for queue submission.
    /// @return True when state is `HasEnded`.
    [[nodiscard]] bool hasEnded() const noexcept
    {
        return m_state == VulkanCommandBufferState::HasEnded;
    }

    /// @brief Reports whether the command buffer has been submitted and is still in flight on the GPU.
    /// @return True when state is `Submitted`.
    [[nodiscard]] bool isSubmitted() const noexcept
    {
        return m_state == VulkanCommandBufferState::Submitted;
    }

    /// @brief Reports whether the command buffer has been allocated from its command pool.
    /// @return True when state is not `NotAllocated`.
    [[nodiscard]] bool isAllocated() const noexcept
    {
        return m_state != VulkanCommandBufferState::NotAllocated;
    }

    /// @brief Returns the owning `VulkanCommandBufferPool`.
    /// @return Mutable reference to `VulkanCommandBufferPool`.
    [[nodiscard]] VulkanCommandBufferPool& getOwner() noexcept
    {
        return m_owner;
    }

    /// @brief Returns the const owning `VulkanCommandBufferPool`.
    /// @return Const reference to `VulkanCommandBufferPool`.
    [[nodiscard]] const VulkanCommandBufferPool& getOwner() const noexcept
    {
        return m_owner;
    }

    /// @brief Begins command recording, resetting the native handle first if state is `NeedReset`.
    /// @details For `Secondary` command buffers, populates `VkCommandBufferInheritanceInfo` and sets
    ///          `VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT` when `renderPassHandle != VK_NULL_HANDLE`.
    /// @param[in] renderPassHandle Optional `VkRenderPass` inherited by a `Secondary` command buffer.
    /// @param[in] subpass Subpass index within `renderPassHandle` (ignored when `renderPassHandle == VK_NULL_HANDLE`).
    /// @param[in] framebufferHandle Optional `VkFramebuffer` hint for secondary inheritance.
    /// @return True when `vkBeginCommandBuffer` succeeded and state transitioned to `IsInsideBegin`.
    [[nodiscard]] bool begin(
        VkRenderPass renderPassHandle = VK_NULL_HANDLE,
        gp::UInt32 subpass = 0u,
        VkFramebuffer framebufferHandle = VK_NULL_HANDLE
    ) noexcept;

    /// @brief Ends command recording, flushing any pending split-barrier event resets before `vkEndCommandBuffer`.
    /// @return True when `vkEndCommandBuffer` succeeded and state transitioned to `HasEnded`.
    [[nodiscard]] bool end() noexcept;

    /// @brief Marks a finished (`HasEnded`) command buffer as submitted to a hardware queue.
    void setSubmitted() noexcept;

    /// @brief Resets CPU tracking state after GPU completion, recursively resetting any executed secondary buffers.
    /// @details Transitions `Submitted` (or `HasEnded`) to `NeedReset` and returns the buffer to its owning pool's
    ///          free list.
    void reset() noexcept;

    /// @brief Begins a render pass object and transitions state from `IsInsideBegin` to `IsInsideRenderPass`.
    /// @details Records `vkCmdBeginRenderPass2` when `VK_KHR_create_renderpass2` is enabled, `vkCmdBeginRenderPass`
    ///          otherwise, matching the entry point the render pass was created with.
    /// @param[in] beginInfo Populated `VkRenderPassBeginInfo`.
    /// @param[in] contents Inline or secondary command buffer execution mode.
    /// @return True when the command was recorded; false if the command buffer was not in `IsInsideBegin`.
    [[nodiscard]] bool beginRenderPass(
        const VkRenderPassBeginInfo& beginInfo, VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE
    ) noexcept;

    /// @brief Ends the active render pass object and transitions state back to `IsInsideBegin`.
    /// @details Records `vkCmdEndRenderPass2` when `VK_KHR_create_renderpass2` is enabled.
    /// @return True when the command was recorded; false if not in `IsInsideRenderPass`.
    [[nodiscard]] bool endRenderPass() noexcept;

    /// @brief Begins dynamic rendering (`VK_KHR_dynamic_rendering`) and transitions to `IsInsideRenderPass`.
    /// @param[in] renderingInfo Populated `VkRenderingInfo`.
    /// @return True when the command was recorded; false if not in `IsInsideBegin` or entry point is unavailable.
    [[nodiscard]] bool beginDynamicRendering(const VkRenderingInfo& renderingInfo) noexcept;

    /// @brief Ends dynamic rendering (`VK_KHR_dynamic_rendering`) and transitions back to `IsInsideBegin`.
    /// @return True when the command was recorded; false if not in `IsInsideRenderPass` or entry point is unavailable.
    [[nodiscard]] bool endDynamicRendering() noexcept;

    /// @brief Executes a completed `Secondary` command buffer inside this `Primary` or `Parallel` command buffer.
    /// @param[in,out] secondaryCmdBuffer Secondary command buffer in `HasEnded` state (marked `Submitted` on success).
    /// @return True when `vkCmdExecuteCommands` was recorded; false on invalid state or type.
    [[nodiscard]] bool executeSecondaryCommandBuffer(VulkanCommandBuffer* secondaryCmdBuffer) noexcept;

    /// @brief Signals a split-barrier `VkEvent` at `srcStageMask`.
    /// @param[in] event Native `VkEvent` handle.
    /// @param[in] srcStageMask Source pipeline stage mask.
    /// @return True when recorded; false if outside `IsInsideBegin` or invalid handle.
    [[nodiscard]] bool beginSplitBarrier(VkEvent event, VkPipelineStageFlags srcStageMask) noexcept;

    /// @brief Waits on a split-barrier `VkEvent` and queues it for reset when the command buffer ends.
    /// @param[in] event Native `VkEvent` handle.
    /// @param[in] srcStageMask Source pipeline stage mask.
    /// @param[in] dstStageMask Destination pipeline stage mask.
    /// @param[in] memoryBarrier Optional memory barrier applied during the wait.
    /// @return True when recorded; false if outside `IsInsideBegin` or invalid handle.
    [[nodiscard]] bool endSplitBarrier(
        VkEvent event,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        const VkMemoryBarrier* memoryBarrier = nullptr
    ) noexcept;

    /// @brief Signals a pooled split-barrier event with `vkCmdSetEvent2KHR`, recording the source half of
    ///        `dependencyInfo`.
    /// @param[in] event Event obtained from `VulkanDevice::getBarrierEvent()`.
    /// @param[in] dependencyInfo Barriers describing the first synchronization scope.
    /// @return True when recorded; false if outside `IsInsideBegin`, the handle is null or the entry point is missing.
    [[nodiscard]] bool beginSplitBarrier(VkEvent event, const VkDependencyInfo& dependencyInfo) noexcept;

    /// @brief Waits on a pooled split-barrier event with `vkCmdWaitEvents2KHR`.
    /// @details The event is reset when this command buffer ends and handed back to the device pool once the buffer
    ///          is recycled after GPU completion.
    /// @param[in] event Event previously passed to `beginSplitBarrier()`.
    /// @param[in] dependencyInfo Barriers matching the ones recorded at `beginSplitBarrier()`.
    /// @return True when recorded; false if outside `IsInsideBegin`, the handle is null or the entry point is missing.
    [[nodiscard]] bool endSplitBarrier(VkEvent event, const VkDependencyInfo& dependencyInfo) noexcept;

    /// @brief Returns the pooled split-barrier events waiting for this command buffer to be recycled.
    /// @return Read-only view of the events that will go back to the device pool.
    [[nodiscard]] gp::ConstVectorView<VkEvent> getPendingPooledBarrierEvents() const noexcept
    {
        return gp::ConstVectorView<VkEvent>(m_pooledBarrierEvents.data(), m_pooledBarrierEvents.size());
    }

    /// @brief Appends a semaphore wait dependency to be consumed when this command buffer is submitted.
    /// @param[in] waitFlags Pipeline stage flags at which the wait occurs.
    /// @param[in] semaphore Semaphore to wait on.
    void addWaitSemaphore(VkPipelineStageFlags waitFlags, const VulkanSemaphoreRef& semaphore) noexcept;

    /// @brief Clears all pending wait semaphores and flags attached to this command buffer.
    void clearWaitSemaphores() noexcept;

    /// @brief Returns the pending wait stage masks attached to this command buffer.
    /// @return Read-only view of `VkPipelineStageFlags`.
    [[nodiscard]] gp::ConstVectorView<VkPipelineStageFlags> getWaitFlags() const noexcept
    {
        return gp::ConstVectorView<VkPipelineStageFlags>(m_waitFlags.data(), m_waitFlags.size());
    }

    /// @brief Returns the pending wait semaphores attached to this command buffer.
    /// @return Read-only view of `VulkanSemaphoreRef`.
    [[nodiscard]] gp::ConstVectorView<VulkanSemaphoreRef> getWaitSemaphores() const noexcept
    {
        return gp::ConstVectorView<VulkanSemaphoreRef>(m_waitSemaphores.data(), m_waitSemaphores.size());
    }

    /// @brief Returns the monotonic timestamp (in nanoseconds) of the last recording or submission activity.
    /// @return Last active timestamp in nanoseconds.
    [[nodiscard]] gp::UInt64 getLastActiveTimestampNs() const noexcept
    {
        return m_lastActiveTimestampNs;
    }

    /// @brief Overrides the last active timestamp (primarily used by stale-cleanup diagnostics and tests).
    /// @param[in] timestampNs Monotonic timestamp in nanoseconds.
    void setLastActiveTimestampNs(const gp::UInt64 timestampNs) noexcept
    {
        m_lastActiveTimestampNs = timestampNs;
    }

    /// @brief Marks dynamic state (viewport, scissor, stencil reference) dirty so the next draw re-emits it.
    void markDynamicStateDirty() noexcept
    {
        m_needsDynamicStateSet = true;
    }

    /// @brief Reports whether cached dynamic state needs to be re-emitted before the next draw.
    /// @return True when dirty.
    [[nodiscard]] bool needsDynamicStateSet() const noexcept
    {
        return m_needsDynamicStateSet;
    }

    /// @brief Returns the currently bound graphics pipeline handle cached on this command buffer.
    /// @return Cached `VkPipeline` handle, or `VK_NULL_HANDLE`.
    [[nodiscard]] VkPipeline getCurrentGraphicsPipeline() const noexcept
    {
        return m_currentGraphicsPipeline;
    }

    /// @brief Updates the cached graphics pipeline handle on this command buffer.
    /// @param[in] pipeline Bound `VkPipeline`.
    void setCurrentGraphicsPipeline(const VkPipeline pipeline) noexcept
    {
        m_currentGraphicsPipeline = pipeline;
    }

    /// @brief Returns the currently bound compute pipeline handle cached on this command buffer.
    /// @return Cached `VkPipeline` handle, or `VK_NULL_HANDLE`.
    [[nodiscard]] VkPipeline getCurrentComputePipeline() const noexcept
    {
        return m_currentComputePipeline;
    }

    /// @brief Updates the cached compute pipeline handle on this command buffer.
    /// @param[in] pipeline Bound `VkPipeline`.
    void setCurrentComputePipeline(const VkPipeline pipeline) noexcept
    {
        m_currentComputePipeline = pipeline;
    }

    /// @brief Reports whether this command buffer belongs to a parallel boundary context.
    /// @return True when marked as a boundary context.
    [[nodiscard]] bool isBoundaryContext() const noexcept
    {
        return m_isBoundaryContext;
    }

    /// @brief Sets whether this command buffer belongs to a parallel boundary context.
    /// @param[in] isBoundary Boundary flag.
    void setBoundaryContext(const bool isBoundary) noexcept
    {
        m_isBoundaryContext = isBoundary;
    }

private:
    /// @brief Constructs and allocates a command buffer from `owner`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] owner Owning `VulkanCommandBufferPool`.
    /// @param[in] commandBufferType Command buffer execution role.
    VulkanCommandBuffer(
        VulkanDevice& device, VulkanCommandBufferPool& owner, VulkanCommandBufferType commandBufferType
    ) noexcept;

    /// @brief Destroys the wrapper and frees its native `VkCommandBuffer` back to `m_owner`.
    ~VulkanCommandBuffer() noexcept;

    /// @brief Allocates the native `VkCommandBuffer` handle from `m_owner`.
    void allocateMemory() noexcept;

    /// @brief Frees the native `VkCommandBuffer` handle back to `m_owner`.
    void freeMemory() noexcept;

    /// @brief Clears cached dynamic state, pipelines, and split-barrier tracking fields.
    void resetDynamicState() noexcept;

    /// @brief Hands every pooled split-barrier event back to the device pool.
    /// @note Only call once the GPU is done with this command buffer.
    void releasePooledBarrierEvents() noexcept;

    VulkanDevice& m_device;
    VulkanCommandBufferPool& m_owner;
    VkCommandBuffer m_handle{ VK_NULL_HANDLE };
    VulkanCommandBufferType m_commandBufferType{ VulkanCommandBufferType::Primary };
    VulkanCommandBufferState m_state{ VulkanCommandBufferState::NotAllocated };

    gp::UInt64 m_lastActiveTimestampNs{ 0u };
    bool m_needsDynamicStateSet{ true };
    bool m_isBoundaryContext{ false };

    VkPipeline m_currentGraphicsPipeline{ VK_NULL_HANDLE };
    VkPipeline m_currentComputePipeline{ VK_NULL_HANDLE };

    gp::Vector<VkEvent> m_endedBarrierEvents{};
    gp::Vector<VkEvent> m_pooledBarrierEvents{};
    gp::Vector<VulkanCommandBuffer*> m_executedSecondaryBuffers{};
    gp::Vector<VkPipelineStageFlags> m_waitFlags{};
    gp::Vector<VulkanSemaphoreRef> m_waitSemaphores{};

    friend class VulkanCommandBufferPool;
};

/// @brief Pool owning a `VkCommandPool` and recycling `VulkanCommandBuffer` objects for a single queue family and role.
class GP_RHI_VULKAN_API VulkanCommandBufferPool final
{
public:
    /// @brief Constructs an uninitialized pool bound to `device` and `commandBufferType`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] commandBufferType Role of command buffers allocated by this pool.
    VulkanCommandBufferPool(VulkanDevice& device, VulkanCommandBufferType commandBufferType) noexcept;

    /// @brief Destructor. Automatically frees all owned command buffers and destroys the native `VkCommandPool`.
    ~VulkanCommandBufferPool() noexcept;

    VulkanCommandBufferPool(const VulkanCommandBufferPool&) = delete;
    VulkanCommandBufferPool& operator=(const VulkanCommandBufferPool&) = delete;
    VulkanCommandBufferPool(VulkanCommandBufferPool&&) = delete;
    VulkanCommandBufferPool& operator=(VulkanCommandBufferPool&&) = delete;

    /// @brief Creates the native `VkCommandPool` for `queueFamilyIndex` with
    /// `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`.
    /// @param[in] queueFamilyIndex Target hardware queue family index.
    /// @return True when `vkCreateCommandPool` succeeded; false otherwise.
    [[nodiscard]] bool create(gp::UInt32 queueFamilyIndex) noexcept;

    /// @brief Destroys all owned `VulkanCommandBuffer` instances and the native `VkCommandPool`.
    void destroy() noexcept;

    /// @brief Returns the native `VkCommandPool` handle.
    /// @return Native `VkCommandPool`, or `VK_NULL_HANDLE` if uninitialized.
    [[nodiscard]] VkCommandPool getHandle() const noexcept
    {
        return m_handle;
    }

    /// @brief Reports whether a live native `VkCommandPool` is owned.
    /// @return True when `m_handle != VK_NULL_HANDLE`.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_handle != VK_NULL_HANDLE;
    }

    /// @brief Returns the command buffer role served by this pool.
    /// @return `VulkanCommandBufferType`.
    [[nodiscard]] VulkanCommandBufferType getCommandBufferType() const noexcept
    {
        return m_commandBufferType;
    }

    /// @brief Returns the hardware queue family index this pool was created for.
    /// @return Queue family index.
    [[nodiscard]] gp::UInt32 getQueueFamilyIndex() const noexcept
    {
        return m_queueFamilyIndex;
    }

    /// @brief Acquires an idle `VulkanCommandBuffer` from the free list or allocates a new one from `m_handle`.
    /// @return Pointer to a `ReadyForBegin` or `NeedReset` command buffer, or `nullptr` if creation failed.
    [[nodiscard]] VulkanCommandBuffer* acquireReadyCommandBuffer() noexcept;

    /// @brief Returns a completed/reset command buffer to this pool's free list.
    /// @param[in] cmdBuffer Command buffer owned by this pool.
    void recycleCommandBuffer(VulkanCommandBuffer* cmdBuffer) noexcept;

    /// @brief Releases stale idle command buffers older than `staleTimeoutNs` and optionally trims pool memory.
    /// @param[in] trimMemory When true, invokes `vkTrimCommandPool` (if available) after reclaiming unused buffers.
    /// @param[in] staleTimeoutNs Inactivity threshold in nanoseconds (defaults to 10 seconds).
    /// @return Number of command buffers destroyed during this sweep.
    gp::UInt32 freeUnusedCmdBuffers(
        bool trimMemory = false, gp::UInt64 staleTimeoutNs = kDefaultStaleCommandBufferTimeoutNs
    ) noexcept;

    /// @brief Resets the entire native `VkCommandPool` via `vkResetCommandPool`.
    /// @param[in] flags Optional `VkCommandPoolResetFlags` (for example `VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT`).
    /// @return True when `vkResetCommandPool` succeeded; false otherwise.
    [[nodiscard]] bool resetPool(VkCommandPoolResetFlags flags = 0u) noexcept;

    /// @brief Returns the total number of live `VulkanCommandBuffer` objects owned by this pool.
    /// @return Owned command buffer count.
    [[nodiscard]] gp::USize getAllocatedCount() const noexcept;

    /// @brief Returns the number of idle command buffers currently waiting in the free list.
    /// @return Free command buffer count.
    [[nodiscard]] gp::USize getFreeCount() const noexcept;

private:
    /// @brief Appends `cmdBuffer` to `m_freeCmdBuffers` while `m_mutex` is already held by the caller.
    /// @param[in] cmdBuffer Command buffer owned by this pool.
    void recycleCommandBufferLocked(VulkanCommandBuffer* cmdBuffer) noexcept;

    VulkanDevice& m_device;
    VkCommandPool m_handle{ VK_NULL_HANDLE };
    VulkanCommandBufferType m_commandBufferType{ VulkanCommandBufferType::Primary };
    gp::UInt32 m_queueFamilyIndex{ 0u };

    mutable gp::thread::Mutex m_mutex{};
    gp::Vector<VulkanCommandBuffer*> m_cmdBuffers{};
    gp::Vector<VulkanCommandBuffer*> m_freeCmdBuffers{};

    friend class VulkanCommandBuffer;
};

}   // namespace gp::rhi::vulkan
