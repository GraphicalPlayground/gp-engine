// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/VectorView.hpp"       // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanThirdParty.hpp"                  // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Records the transfer and barrier commands that resource uploads, readbacks and moves need.
/// @details Resources never talk to a `VkCommandBuffer` directly. They ask an `IVulkanCommandContextProvider` for the
///          recorder of the right context (the immediate graphics context, or the upload context for first writes)
///          and record through this seam. Production code wraps a `VulkanCommandBuffer`; tests capture the calls.
/// @note Not thread-safe; a recorder belongs to a single context.
class IVulkanTransferCommandRecorder
{
public:
    /// @brief Virtual destructor.
    virtual ~IVulkanTransferCommandRecorder() noexcept = default;

    /// @brief Records a buffer-to-buffer copy.
    /// @param[in] srcBuffer Source buffer.
    /// @param[in] dstBuffer Destination buffer.
    /// @param[in] regions Copy regions.
    virtual void
        copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, gp::ConstVectorView<VkBufferCopy> regions) noexcept = 0;

    /// @brief Records a fill of `size` bytes at `offset` with the 32-bit pattern `data`.
    /// @param[in] dstBuffer Destination buffer.
    /// @param[in] offset Byte offset; must be a multiple of 4.
    /// @param[in] size Byte count; a multiple of 4 or `VK_WHOLE_SIZE`.
    /// @param[in] data Repeated 32-bit pattern.
    virtual void fillBuffer(VkBuffer dstBuffer, VkDeviceSize offset, VkDeviceSize size, gp::UInt32 data) noexcept = 0;

    /// @brief Records a pipeline barrier with a single global memory barrier.
    /// @param[in] srcStageMask Stages that must finish first.
    /// @param[in] dstStageMask Stages that wait.
    /// @param[in] barrier Access masks to make available and visible.
    virtual void memoryBarrier(
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, const VkMemoryBarrier& barrier
    ) noexcept = 0;

    /// @brief Records a buffer-to-image copy.
    /// @param[in] srcBuffer Source (staging) buffer.
    /// @param[in] dstImage Destination image.
    /// @param[in] dstLayout Layout of `dstImage` while the copy executes (`TRANSFER_DST_OPTIMAL` or `GENERAL`).
    /// @param[in] regions Copy regions.
    virtual void copyBufferToImage(
        VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstLayout, gp::ConstVectorView<VkBufferImageCopy> regions
    ) noexcept = 0;

    /// @brief Records an image-to-buffer copy.
    /// @param[in] srcImage Source image.
    /// @param[in] srcLayout Layout of `srcImage` while the copy executes (`TRANSFER_SRC_OPTIMAL` or `GENERAL`).
    /// @param[in] dstBuffer Destination (readback) buffer.
    /// @param[in] regions Copy regions.
    virtual void copyImageToBuffer(
        VkImage srcImage, VkImageLayout srcLayout, VkBuffer dstBuffer, gp::ConstVectorView<VkBufferImageCopy> regions
    ) noexcept = 0;

    /// @brief Records an image-to-image copy.
    /// @param[in] srcImage Source image.
    /// @param[in] srcLayout Layout of `srcImage` during the copy.
    /// @param[in] dstImage Destination image.
    /// @param[in] dstLayout Layout of `dstImage` during the copy.
    /// @param[in] regions Copy regions.
    virtual void copyImage(
        VkImage srcImage,
        VkImageLayout srcLayout,
        VkImage dstImage,
        VkImageLayout dstLayout,
        gp::ConstVectorView<VkImageCopy> regions
    ) noexcept = 0;

    /// @brief Records an image layout transition; stage and access masks are derived from the two layouts.
    /// @param[in] image Target image.
    /// @param[in] oldLayout Layout before the barrier.
    /// @param[in] newLayout Layout after the barrier.
    /// @param[in] range Subresources to transition.
    virtual void imageLayoutTransition(
        VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, const VkImageSubresourceRange& range
    ) noexcept = 0;

    /// @brief Records a color clear of `range`.
    /// @param[in] image Target image.
    /// @param[in] layout Layout of `image` during the clear.
    /// @param[in] color Clear color.
    /// @param[in] range Subresources to clear.
    virtual void clearColorImage(
        VkImage image, VkImageLayout layout, const VkClearColorValue& color, const VkImageSubresourceRange& range
    ) noexcept = 0;

    /// @brief Records a depth/stencil clear of `range`.
    /// @param[in] image Target image.
    /// @param[in] layout Layout of `image` during the clear.
    /// @param[in] value Clear depth and stencil.
    /// @param[in] range Subresources to clear.
    virtual void clearDepthStencilImage(
        VkImage image, VkImageLayout layout, const VkClearDepthStencilValue& value, const VkImageSubresourceRange& range
    ) noexcept = 0;

    /// @brief Records a full heavyweight barrier when `r.Vulkan.DebugBarrier` enables `categoryMask`.
    /// @param[in] categoryMask Debug category bit of the call site.
    virtual void debugHeavyWeightBarrier(gp::Int32 categoryMask) noexcept = 0;

    /// @brief Reports whether no render pass is open, so transfer commands are legal.
    /// @return True outside a render pass.
    [[nodiscard]] virtual bool isOutsideRenderPass() const noexcept = 0;

    /// @brief Returns the sync point signaled when the recorded commands complete on the GPU.
    /// @return Completion sync point, or null when the work completes synchronously.
    [[nodiscard]] virtual VulkanSyncPointRef getCompletionSyncPoint() const noexcept = 0;
};

}   // namespace gp::rhi::vulkan
