// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief `IVulkanTransferCommandRecorder` that records straight into a `VulkanCommandBuffer`.
/// @details Owned by a command context next to the command buffer it wraps. The completion sync point is the one the
///          context signals when its current payload retires, so staging buffers released through it are recycled
///          only once the GPU is done reading them.
/// @note Not thread-safe; owned by a single context.
class GP_RHI_VULKAN_API VulkanCommandBufferTransferRecorder final : public IVulkanTransferCommandRecorder
{
public:
    /// @brief Binds the recorder to `commandBuffer`.
    /// @param[in,out] commandBuffer Command buffer in the recording state.
    /// @param[in] completionSyncPoint Sync point signaled when the command buffer's payload completes.
    VulkanCommandBufferTransferRecorder(
        VulkanCommandBuffer& commandBuffer, VulkanSyncPointRef completionSyncPoint
    ) noexcept;

    /// @brief Destructor.
    virtual ~VulkanCommandBufferTransferRecorder() noexcept override = default;

    VulkanCommandBufferTransferRecorder(const VulkanCommandBufferTransferRecorder&) = delete;
    VulkanCommandBufferTransferRecorder& operator=(const VulkanCommandBufferTransferRecorder&) = delete;
    VulkanCommandBufferTransferRecorder(VulkanCommandBufferTransferRecorder&&) = delete;
    VulkanCommandBufferTransferRecorder& operator=(VulkanCommandBufferTransferRecorder&&) = delete;

    /// @copydoc IVulkanTransferCommandRecorder::copyBuffer()
    /// @note Records `vkCmdCopyBuffer`.
    virtual void
        copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, gp::ConstVectorView<VkBufferCopy> regions) noexcept override;

    /// @copydoc IVulkanTransferCommandRecorder::fillBuffer()
    /// @note Records `vkCmdFillBuffer`.
    virtual void
        fillBuffer(VkBuffer dstBuffer, VkDeviceSize offset, VkDeviceSize size, gp::UInt32 data) noexcept override;

    /// @copydoc IVulkanTransferCommandRecorder::memoryBarrier()
    /// @note Records `vkCmdPipelineBarrier` with one `VkMemoryBarrier`.
    virtual void memoryBarrier(
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, const VkMemoryBarrier& barrier
    ) noexcept override;

    /// @copydoc IVulkanTransferCommandRecorder::debugHeavyWeightBarrier()
    virtual void debugHeavyWeightBarrier(gp::Int32 categoryMask) noexcept override;

    /// @copydoc IVulkanTransferCommandRecorder::isOutsideRenderPass()
    [[nodiscard]] virtual bool isOutsideRenderPass() const noexcept override;

    /// @copydoc IVulkanTransferCommandRecorder::getCompletionSyncPoint()
    [[nodiscard]] virtual VulkanSyncPointRef getCompletionSyncPoint() const noexcept override;

    /// @brief Rebinds the recorder to a new command buffer and sync point (after the context rolls its payload).
    /// @param[in,out] commandBuffer Command buffer in the recording state.
    /// @param[in] completionSyncPoint Sync point of the new payload.
    void rebind(VulkanCommandBuffer& commandBuffer, VulkanSyncPointRef completionSyncPoint) noexcept;

private:
    VulkanCommandBuffer* m_commandBuffer{ nullptr };
    VulkanSyncPointRef m_completionSyncPoint{};
};

}   // namespace gp::rhi::vulkan

