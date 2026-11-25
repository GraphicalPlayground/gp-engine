// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/IVulkanBarrierCommandRecorder.hpp"
#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "barriers/VulkanBarrierTypes.hpp"
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "containers/views/VectorView.hpp"     // IWYU pragma: keep
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/VulkanSemaphore.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief `IVulkanBarrierCommandRecorder` implementation recording into a `VulkanCommandBuffer`.
/// @details Barriers and split-barrier events go straight into the command buffer. Semaphores and reserved buffer
///          commits cannot be recorded in a command buffer, so they are queued here until the owning context moves
///          them into the next submission payload.
/// @note Not thread-safe; owned by a single command list context.
class GP_RHI_VULKAN_API VulkanCommandBufferBarrierRecorder final : public IVulkanBarrierCommandRecorder
{
public:
    /// @brief Binds the recorder to a command buffer.
    /// @param[in,out] cmdBuffer Command buffer in the recording state.
    explicit VulkanCommandBufferBarrierRecorder(VulkanCommandBuffer& cmdBuffer) noexcept;

    /// @brief Destructor.
    virtual ~VulkanCommandBufferBarrierRecorder() noexcept override = default;

    VulkanCommandBufferBarrierRecorder(const VulkanCommandBufferBarrierRecorder&) = delete;
    VulkanCommandBufferBarrierRecorder& operator=(const VulkanCommandBufferBarrierRecorder&) = delete;
    VulkanCommandBufferBarrierRecorder(VulkanCommandBufferBarrierRecorder&&) = delete;
    VulkanCommandBufferBarrierRecorder& operator=(VulkanCommandBufferBarrierRecorder&&) = delete;

    /// @copydoc IVulkanBarrierCommandRecorder::pipelineBarrier()
    /// @note Records `vkCmdPipelineBarrier` on the bound command buffer.
    virtual void pipelineBarrier(
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkDependencyFlags dependencyFlags,
        gp::ConstVectorView<VkMemoryBarrier> memoryBarriers,
        gp::ConstVectorView<VkBufferMemoryBarrier> bufferBarriers,
        gp::ConstVectorView<VkImageMemoryBarrier> imageBarriers
    ) noexcept override;

    /// @copydoc IVulkanBarrierCommandRecorder::pipelineBarrier2()
    /// @note Records `vkCmdPipelineBarrier2KHR` on the bound command buffer.
    virtual void pipelineBarrier2(const VkDependencyInfo& dependencyInfo) noexcept override;

    /// @copydoc IVulkanBarrierCommandRecorder::beginSplitBarrier()
    virtual void beginSplitBarrier(VkEvent event, const VkDependencyInfo& dependencyInfo) noexcept override;

    /// @copydoc IVulkanBarrierCommandRecorder::endSplitBarrier()
    /// @note The command buffer resets the event when it ends and returns it to the device pool once recycled.
    virtual void endSplitBarrier(VkEvent event, const VkDependencyInfo& dependencyInfo) noexcept override;

    /// @copydoc IVulkanBarrierCommandRecorder::releaseBarrierEvent()
    /// @note Returns the event to the pool of the command buffer's device right away.
    virtual void releaseBarrierEvent(VkEvent event) noexcept override;

    /// @copydoc IVulkanBarrierCommandRecorder::addSignalSemaphore()
    virtual void addSignalSemaphore(const VulkanSemaphoreRef& semaphore) noexcept override;

    /// @copydoc IVulkanBarrierCommandRecorder::addWaitSemaphore()
    virtual void
        addWaitSemaphore(VkPipelineStageFlags waitStageMask, const VulkanSemaphoreRef& semaphore) noexcept override;

    /// @copydoc IVulkanBarrierCommandRecorder::addReservedResourceCommits()
    virtual void
        addReservedResourceCommits(gp::ConstVectorView<VulkanCommitReservedResourceDesc> commits) noexcept override;

    /// @brief Returns the semaphores queued for signaling.
    /// @return Read-only view.
    [[nodiscard]] gp::ConstVectorView<VulkanSemaphoreRef> getSignalSemaphores() const noexcept
    {
        return m_signalSemaphores;
    }

    /// @brief Returns the semaphores queued for waiting.
    /// @return Read-only view, parallel to `getWaitStageMasks()`.
    [[nodiscard]] gp::ConstVectorView<VulkanSemaphoreRef> getWaitSemaphores() const noexcept
    {
        return m_waitSemaphores;
    }

    /// @brief Returns the wait stage mask of each queued wait semaphore.
    /// @return Read-only view, parallel to `getWaitSemaphores()`.
    [[nodiscard]] gp::ConstVectorView<VkPipelineStageFlags> getWaitStageMasks() const noexcept
    {
        return m_waitStageMasks;
    }

    /// @brief Returns the queued reserved buffer commits.
    /// @return Read-only view.
    [[nodiscard]] gp::ConstVectorView<VulkanCommitReservedResourceDesc> getReservedResourceCommits() const noexcept
    {
        return m_reservedResourceCommits;
    }

    /// @brief Clears every queued semaphore and commit once they have been moved into a submission.
    void clearPendingSubmissionState() noexcept;

    /// @brief Returns the bound command buffer.
    /// @return Command buffer.
    [[nodiscard]] VulkanCommandBuffer& getCommandBuffer() const noexcept
    {
        return m_cmdBuffer;
    }

private:
    VulkanCommandBuffer& m_cmdBuffer;
    gp::Vector<VulkanSemaphoreRef> m_signalSemaphores{};
    gp::Vector<VulkanSemaphoreRef> m_waitSemaphores{};
    gp::Vector<VkPipelineStageFlags> m_waitStageMasks{};
    gp::Vector<VulkanCommitReservedResourceDesc> m_reservedResourceCommits{};
};

}   // namespace gp::rhi::vulkan
