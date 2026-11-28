// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierEnums.hpp"
#include "barriers/VulkanBarrierForward.hpp"       // IWYU pragma: keep
#include "barriers/VulkanBarrierTypes.hpp"
#include "containers/arrays/Vector.hpp"            // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/allocators/InlineAllocator.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Batched memory, buffer and image barriers recorded internally by the backend.
/// @details Used when the backend has to change an image layout or order memory accesses itself, outside the public
///          `beginTransitions()` / `endTransitions()` API (uploads, clears, swapchain handling). Barriers are stored in
///          `synchronization2` form and downgraded to `vkCmdPipelineBarrier` with merged stage masks when the device
///          lacks `VK_KHR_synchronization2`.
/// @note Not thread-safe; build and execute a barrier batch on a single thread.
struct GP_RHI_VULKAN_API VulkanPipelineBarrier
{
public:
    /// @brief Memory barrier storage; at most one merged barrier is ever kept, so it lives inline.
    using MemoryBarrierArrayType = gp::InlineVector<VkMemoryBarrier2, 1u>;

    /// @brief Image barrier storage; most internal batches touch one or two images.
    using ImageBarrierArrayType = gp::InlineVector<VkImageMemoryBarrier2, 2u>;

    /// @brief Buffer barrier storage.
    using BufferBarrierArrayType = gp::Vector<VkBufferMemoryBarrier2>;

public:
    /// @brief Constructs an empty barrier batch.
    VulkanPipelineBarrier() noexcept = default;

    /// @brief Folds an execution and memory dependency into the single merged global memory barrier.
    /// @details Access masks are only added when the source access writes memory; a read source needs only the
    ///          execution dependency carried by the stage masks.
    /// @param[in] srcAccessFlags Source access mask.
    /// @param[in] dstAccessFlags Destination access mask.
    /// @param[in] srcStageMask Source stage mask.
    /// @param[in] dstStageMask Destination stage mask.
    void addMemoryBarrier(
        VkAccessFlags srcAccessFlags,
        VkAccessFlags dstAccessFlags,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask
    ) noexcept;

    /// @brief Adds a layout transition covering every mip, layer and aspect of `texture`.
    /// @details Stage and access masks are derived from the layouts. Depth/stencil layouts are merged into one
    ///          layout valid for both planes.
    /// @param[in] texture Resolved texture (`image`, `fullAspectMask` and `isDepthOrStencilAspect` are used).
    /// @param[in] srcLayout Layout before the barrier.
    /// @param[in] dstLayout Layout after the barrier.
    void addFullImageLayoutTransition(
        const VulkanBarrierResource& texture, VkImageLayout srcLayout, VkImageLayout dstLayout
    ) noexcept;

    /// @brief Adds a layout transition for a subresource range of `image`.
    /// @param[in] image Target image.
    /// @param[in] srcLayout Layout before the barrier.
    /// @param[in] dstLayout Layout after the barrier.
    /// @param[in] subresourceRange Subresources to transition; the aspect mask decides depth/stencil handling.
    void addImageLayoutTransition(
        VkImage image, VkImageLayout srcLayout, VkImageLayout dstLayout, const VkImageSubresourceRange& subresourceRange
    ) noexcept;

    /// @brief Records the batch with legacy `vkCmdPipelineBarrier`, merging every stage mask into one pair.
    /// @note Per-plane `synchronization2` layouts on depth/stencil images are replaced with combined layouts.
    /// @param[in] cmdBuffer Command buffer in the recording state.
    void execute(VkCommandBuffer cmdBuffer) noexcept;

    /// @brief Records the batch with `vkCmdPipelineBarrier2KHR` when the device supports parallel rendering, or the
    ///        legacy path otherwise.
    /// @param[in] cmdBuffer Command buffer in the recording state.
    void execute(VulkanCommandBuffer& cmdBuffer) noexcept;

    /// @brief Reports whether the batch holds no barrier.
    /// @return True when every barrier array is empty.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        return memoryBarriers.isEmpty() && bufferBarriers.isEmpty() && imageBarriers.isEmpty();
    }

    /// @brief Builds a `VkImageSubresourceRange`.
    /// @param[in] aspectMask Aspects to cover.
    /// @param[in] firstMip First mip level.
    /// @param[in] numMips Mip count, or `VK_REMAINING_MIP_LEVELS`.
    /// @param[in] firstLayer First array layer.
    /// @param[in] numLayers Layer count, or `VK_REMAINING_ARRAY_LAYERS`.
    /// @return Filled subresource range.
    [[nodiscard]] static VkImageSubresourceRange makeSubresourceRange(
        VkImageAspectFlags aspectMask,
        gp::UInt32 firstMip = 0u,
        gp::UInt32 numMips = VK_REMAINING_MIP_LEVELS,
        gp::UInt32 firstLayer = 0u,
        gp::UInt32 numLayers = VK_REMAINING_ARRAY_LAYERS
    ) noexcept;

    /// @brief Returns the layout a texture uses for a single, well-known access state.
    /// @param[in] texture Resolved texture (`isDepthOrStencilAspect` and `supportsSampling` are used).
    /// @param[in] desiredAccess Access state; only SRV, UAV, copy, DSV and shading rate states are valid.
    /// @param[in] supportsParallelRendering True when per-plane `synchronization2` layouts are available.
    /// @param[in] shadingRateImageType Device shading rate image encoding.
    /// @return Layout for `desiredAccess`, or `VK_IMAGE_LAYOUT_UNDEFINED` (with a failed check) for other states.
    [[nodiscard]] static VkImageLayout getDefaultLayout(
        const VulkanBarrierResource& texture,
        sync::ResourceAccess desiredAccess,
        bool supportsParallelRendering,
        VulkanShadingRateImageType shadingRateImageType
    ) noexcept;

    /// @brief Returns the per-plane `synchronization2` layout of a depth or stencil plane.
    /// @param[in] access Access state of the plane.
    /// @return Layout for that plane.
    [[nodiscard]] static VkImageLayout getDepthOrStencilLayout(sync::ResourceAccess access) noexcept;

public:
    /// @brief Merged global memory barrier (zero or one entry).
    MemoryBarrierArrayType memoryBarriers{};

    /// @brief Image layout transitions.
    ImageBarrierArrayType imageBarriers{};

    /// @brief Buffer range barriers.
    BufferBarrierArrayType bufferBarriers{};
};

/// @brief Records a single image layout transition into `cmdBuffer`.
/// @note Does not update any layout tracking; callers that track layouts must update their `VulkanLayoutManager`.
/// @param[in] cmdBuffer Command buffer in the recording state.
/// @param[in] image Target image.
/// @param[in] oldLayout Layout before the barrier.
/// @param[in] newLayout Layout after the barrier.
/// @param[in] subresourceRange Subresources to transition.
GP_RHI_VULKAN_API void vulkanSetImageLayout(
    VulkanCommandBuffer& cmdBuffer,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    const VkImageSubresourceRange& subresourceRange
) noexcept;

}   // namespace gp::rhi::vulkan
