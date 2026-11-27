// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanCopyCommands.hpp"
#include "commands/CommandTypes.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "RHIDefinitions.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Returns `value >> shift`, clamped to at least one.
/// @param[in] value Extent at mip 0.
/// @param[in] shift Mip level.
/// @return Extent of mip `shift`.
[[nodiscard]] gp::UInt32 mipExtent(const gp::Int32 value, const gp::UInt32 shift) noexcept
{
    const gp::UInt32 unsignedValue = (value > 0) ? static_cast<gp::UInt32>(value) : 1u;
    const gp::UInt32 shifted = (shift < 32u) ? (unsignedValue >> shift) : 0u;
    return gp::math::max<gp::UInt32>(1u, shifted);
}

/// @brief Returns the extent to copy for the first mip of `info`.
/// @param[in] info Copy description.
/// @param[in] sourceExtent Source mip 0 extent.
/// @return Copy extent of the first region.
[[nodiscard]] VkExtent3D
    firstCopyExtent(const command::CopyTextureInfo& info, const resource::IntVector& sourceExtent) noexcept
{
    const bool isWholeMip = (info.size.x == 0) && (info.size.y == 0) && (info.size.z == 0);
    if (isWholeMip)
    {
        return VkExtent3D{ mipExtent(sourceExtent.x, info.sourceMipIndex),
                           mipExtent(sourceExtent.y, info.sourceMipIndex),
                           mipExtent(sourceExtent.z, info.sourceMipIndex) };
    }
    return VkExtent3D{ mipExtent(info.size.x, 0u), mipExtent(info.size.y, 0u), mipExtent(info.size.z, 0u) };
}

/// @brief Halves every extent component, clamped to one.
/// @param[in,out] extent Extent stepped to the next mip.
void halveExtent(VkExtent3D& extent) noexcept
{
    extent.width = gp::math::max<gp::UInt32>(1u, extent.width / 2u);
    extent.height = gp::math::max<gp::UInt32>(1u, extent.height / 2u);
    extent.depth = gp::math::max<gp::UInt32>(1u, extent.depth / 2u);
}

/// @brief Halves every offset component.
/// @param[in,out] offset Offset stepped to the next mip.
void halveOffset(VkOffset3D& offset) noexcept
{
    offset.x /= 2;
    offset.y /= 2;
    offset.z /= 2;
}

}   // namespace

void buildVulkanImageCopyRegions(
    const command::CopyTextureInfo& info,
    const resource::IntVector& sourceExtent,
    const VkImageAspectFlags sourceAspect,
    const VkImageAspectFlags destAspect,
    gp::Vector<VkImageCopy>& outRegions
) noexcept
{
    outRegions.clear();
    const gp::UInt32 numMips = gp::math::max<gp::UInt32>(1u, info.numMips);
    const gp::UInt32 numSlices = gp::math::max<gp::UInt32>(1u, info.numSlices);

    VkImageCopy region{};
    region.extent = firstCopyExtent(info, sourceExtent);
    region.srcSubresource.aspectMask = sourceAspect;
    region.srcSubresource.mipLevel = info.sourceMipIndex;
    region.srcSubresource.baseArrayLayer = info.sourceSliceIndex;
    region.srcSubresource.layerCount = numSlices;
    region.srcOffset = VkOffset3D{ info.sourcePosition.x, info.sourcePosition.y, info.sourcePosition.z };
    region.dstSubresource.aspectMask = destAspect;
    region.dstSubresource.mipLevel = info.destMipIndex;
    region.dstSubresource.baseArrayLayer = info.destSliceIndex;
    region.dstSubresource.layerCount = numSlices;
    region.dstOffset = VkOffset3D{ info.destPosition.x, info.destPosition.y, info.destPosition.z };

    outRegions.reserve(numMips);
    for (gp::UInt32 mip = 0u; mip < numMips; ++mip)
    {
        outRegions.pushBack(region);
        ++region.srcSubresource.mipLevel;
        ++region.dstSubresource.mipLevel;
        halveExtent(region.extent);
        halveOffset(region.srcOffset);
        halveOffset(region.dstOffset);
    }
}

void buildVulkanReadbackCopyRegions(
    const command::CopyTextureInfo& info,
    const resource::IntVector& sourceExtent,
    const VkImageAspectFlags sourceAspect,
    const VulkanCpuReadbackBuffer& readback,
    gp::Vector<VkBufferImageCopy>& outRegions
) noexcept
{
    outRegions.clear();
    const gp::UInt32 numMips = gp::math::max<gp::UInt32>(1u, info.numMips);

    VkExtent3D extent = firstCopyExtent(info, sourceExtent);
    VkOffset3D offset{ info.sourcePosition.x, info.sourcePosition.y, info.sourcePosition.z };
    outRegions.reserve(numMips);
    for (gp::UInt32 mip = 0u; mip < numMips; ++mip)
    {
        const gp::UInt32 destMip = info.destMipIndex + mip;
        if (destMip >= kMaxTextureMipCount)
        {
            break;
        }

        VkBufferImageCopy region{};
        region.bufferOffset = static_cast<VkDeviceSize>(readback.mipOffsets[destMip]);
        region.bufferRowLength = extent.width;
        region.bufferImageHeight = extent.height;
        region.imageSubresource.aspectMask = sourceAspect;
        region.imageSubresource.mipLevel = info.sourceMipIndex + mip;
        region.imageSubresource.baseArrayLayer = info.sourceSliceIndex;
        region.imageSubresource.layerCount = 1u;
        region.imageOffset = offset;
        region.imageExtent = extent;
        outRegions.pushBack(region);

        halveExtent(extent);
        halveOffset(offset);
    }
}

bool buildVulkanBufferCopyRegion(
    const gp::UInt64 sourceBaseOffset,
    const gp::UInt64 sourceOffset,
    const gp::UInt64 destBaseOffset,
    const gp::UInt64 destOffset,
    const gp::UInt64 numBytes,
    VkBufferCopy& outRegion
) noexcept
{
    if (numBytes == 0u)
    {
        return false;
    }
    outRegion.srcOffset = static_cast<VkDeviceSize>(sourceBaseOffset + sourceOffset);
    outRegion.dstOffset = static_cast<VkDeviceSize>(destBaseOffset + destOffset);
    outRegion.size = static_cast<VkDeviceSize>(numBytes);
    return true;
}

void recordVulkanImageCopy(
    IVulkanTransferCommandRecorder& recorder,
    const VkImage sourceImage,
    const VkImage destImage,
    const gp::ConstVectorView<VkImageCopy> regions
) noexcept
{
    if (regions.isEmpty())
    {
        return;
    }
    recorder.copyImage(
        sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions
    );
}

void recordVulkanReadbackCopy(
    IVulkanTransferCommandRecorder& recorder,
    const VkImage sourceImage,
    const VkBuffer readbackBuffer,
    const gp::ConstVectorView<VkBufferImageCopy> regions
) noexcept
{
    if (regions.isEmpty())
    {
        return;
    }
    recorder.copyImageToBuffer(sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readbackBuffer, regions);

    VkMemoryBarrier barrier{};
    zeroVulkanStruct(barrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    recorder.memoryBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT, barrier);
}

void recordVulkanBufferCopy(
    IVulkanTransferCommandRecorder& recorder,
    const VkBuffer sourceBuffer,
    const VkBuffer destBuffer,
    const VkBufferCopy& region
) noexcept
{
    VkMemoryBarrier before{};
    zeroVulkanStruct(before, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
    before.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    before.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    recorder.memoryBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, before);

    recorder.copyBuffer(sourceBuffer, destBuffer, gp::ConstVectorView<VkBufferCopy>(&region, 1u));

    VkMemoryBarrier after{};
    zeroVulkanStruct(after, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
    after.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    after.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    recorder.memoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, after);
}

}   // namespace gp::rhi::vulkan
