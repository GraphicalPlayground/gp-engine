// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandForward.hpp"       // IWYU pragma: keep
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"                  // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Builds one `VkImageCopy` per mip for an image-to-image copy.
/// @details A zero `info.size` means "the whole source mip": the extent becomes the source extent shifted down by
///          `sourceMipIndex`, clamped to one texel. Each following region steps both mips by one and halves offsets
///          and extents, so a single `vkCmdCopyImage` call copies the whole mip chain.
/// @param[in] info Copy description from the RHI.
/// @param[in] sourceExtent Extent of the source texture's mip 0 (width, height, depth).
/// @param[in] sourceAspect Full aspect mask of the source image.
/// @param[in] destAspect Full aspect mask of the destination image.
/// @param[out] outRegions Receives `max(1, info.numMips)` regions; cleared first.
GP_RHI_VULKAN_API void buildVulkanImageCopyRegions(
    const command::CopyTextureInfo& info,
    const resource::IntVector& sourceExtent,
    VkImageAspectFlags sourceAspect,
    VkImageAspectFlags destAspect,
    gp::Vector<VkImageCopy>& outRegions
) noexcept;

/// @brief Builds one `VkBufferImageCopy` per mip for a copy into a CPU readback texture.
/// @details Readback textures are linear host buffers laid out mip after mip; region `i` targets
///          `readback.mipOffsets[destMipIndex + i]` with a tightly packed row length equal to the mip width.
/// @param[in] info Copy description from the RHI.
/// @param[in] sourceExtent Extent of the source texture's mip 0.
/// @param[in] sourceAspect Full aspect mask of the source image.
/// @param[in] readback Destination readback buffer and its mip offsets.
/// @param[out] outRegions Receives `max(1, info.numMips)` regions; cleared first.
GP_RHI_VULKAN_API void buildVulkanReadbackCopyRegions(
    const command::CopyTextureInfo& info,
    const resource::IntVector& sourceExtent,
    VkImageAspectFlags sourceAspect,
    const VulkanCpuReadbackBuffer& readback,
    gp::Vector<VkBufferImageCopy>& outRegions
) noexcept;

/// @brief Builds the `VkBufferCopy` of a buffer-to-buffer region, folding in each buffer's sub-allocation offset.
/// @param[in] sourceBaseOffset Sub-allocation offset of the source buffer inside its `VkBuffer`.
/// @param[in] sourceOffset Caller offset into the source buffer.
/// @param[in] destBaseOffset Sub-allocation offset of the destination buffer.
/// @param[in] destOffset Caller offset into the destination buffer.
/// @param[in] numBytes Bytes to copy.
/// @param[out] outRegion Receives the region when the copy is not empty.
/// @return False when `numBytes` is zero and nothing should be recorded.
[[nodiscard]] GP_RHI_VULKAN_API bool buildVulkanBufferCopyRegion(
    gp::UInt64 sourceBaseOffset,
    gp::UInt64 sourceOffset,
    gp::UInt64 destBaseOffset,
    gp::UInt64 destOffset,
    gp::UInt64 numBytes,
    VkBufferCopy& outRegion
) noexcept;

/// @brief Records an image-to-image copy between images already in transfer layouts.
/// @param[in,out] recorder Transfer recorder of the active command buffer.
/// @param[in] sourceImage Image in `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`.
/// @param[in] destImage Image in `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`.
/// @param[in] regions Regions from `buildVulkanImageCopyRegions()`.
GP_RHI_VULKAN_API void recordVulkanImageCopy(
    IVulkanTransferCommandRecorder& recorder,
    VkImage sourceImage,
    VkImage destImage,
    gp::ConstVectorView<VkImageCopy> regions
) noexcept;

/// @brief Records an image-to-readback-buffer copy and makes the result visible to host reads.
/// @param[in,out] recorder Transfer recorder of the active command buffer.
/// @param[in] sourceImage Image in `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`.
/// @param[in] readbackBuffer Host-visible destination buffer.
/// @param[in] regions Regions from `buildVulkanReadbackCopyRegions()`.
GP_RHI_VULKAN_API void recordVulkanReadbackCopy(
    IVulkanTransferCommandRecorder& recorder,
    VkImage sourceImage,
    VkBuffer readbackBuffer,
    gp::ConstVectorView<VkBufferImageCopy> regions
) noexcept;

/// @brief Records a buffer-to-buffer copy fenced by full memory barriers on both sides.
/// @details Buffer copies are not tracked by the transition system, so the copy waits for every prior access and
///          publishes its writes to every later one.
/// @param[in,out] recorder Transfer recorder of the active command buffer.
/// @param[in] sourceBuffer Source `VkBuffer`.
/// @param[in] destBuffer Destination `VkBuffer`.
/// @param[in] region Region from `buildVulkanBufferCopyRegion()`.
GP_RHI_VULKAN_API void recordVulkanBufferCopy(
    IVulkanTransferCommandRecorder& recorder, VkBuffer sourceBuffer, VkBuffer destBuffer, const VkBufferCopy& region
) noexcept;

}   // namespace gp::rhi::vulkan
