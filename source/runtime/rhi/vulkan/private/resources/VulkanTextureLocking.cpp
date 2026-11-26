// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanTextureLocking.hpp"
#include "barriers/VulkanPipelineBarrier.hpp"
#include "commands/CommandListBase.hpp"
#include "commands/CommandListImmediate.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "memory/VulkanStagingManager.hpp"
#include "resources/IVulkanCommandContextProvider.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureSubresourceHelpers.hpp"
#include "resources/TextureUtils.hpp"
#include "resources/VulkanPendingTextureLocks.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "resources/VulkanTextureUsage.hpp"
#include "RHIPixelFormat.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "utils/VulkanDebugBarrier.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Reference-counted handle to a Vulkan texture.
using VulkanTextureRef = gp::RefCountedPtr<VulkanTexture>;

/// @brief Downcasts an RHI texture to the Vulkan implementation.
/// @param[in] texture RHI texture; must be a `VulkanTexture`.
/// @return Vulkan texture.
[[nodiscard]] VulkanTexture& toVulkanTexture(resource::Texture* const texture) noexcept
{
    GP_CHECK(texture != nullptr);
    return *static_cast<VulkanTexture*>(texture);
}

/// @brief Returns the context provider of `services`, asserting that one is bound.
/// @param[in] services Texture services.
/// @return Context provider.
[[nodiscard]] IVulkanCommandContextProvider& getContextProvider(const VulkanResourceServices& services) noexcept
{
    GP_CHECKF(services.contextProvider != nullptr, "Recording texture commands requires a command context provider.");
    return *services.contextProvider;
}

/// @brief Builds the copy region of one whole subresource.
/// @param[in] texture Texture holding the subresource.
/// @param[in] mipIndex Mip level.
/// @param[in] arrayLayer Combined array layer.
/// @return Region starting at offset `0` of a tightly packed buffer.
[[nodiscard]] VkBufferImageCopy
    makeSubresourceRegion(const VulkanTexture& texture, const gp::UInt32 mipIndex, const gp::UInt32 arrayLayer) noexcept
{
    const resource::IntVector mipExtent = resource::calculateMipExtent(texture.getDesc(), mipIndex);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = texture.getPartialAspectMask();
    region.imageSubresource.mipLevel = mipIndex;
    region.imageSubresource.baseArrayLayer = arrayLayer;
    region.imageSubresource.layerCount = 1u;
    region.imageExtent.width = static_cast<gp::UInt32>(mipExtent.x);
    region.imageExtent.height = static_cast<gp::UInt32>(mipExtent.y);
    region.imageExtent.depth = static_cast<gp::UInt32>(mipExtent.z);
    return region;
}

/// @brief Returns the subresource range a copy region touches, on every aspect.
/// @param[in] texture Texture holding the subresources.
/// @param[in] region Copy region.
/// @return Range for the layout transitions around the copy.
[[nodiscard]] VkImageSubresourceRange
    makeRegionRange(const VulkanTexture& texture, const VkBufferImageCopy& region) noexcept
{
    return VulkanPipelineBarrier::makeSubresourceRange(
        texture.getFullAspectMask(),
        region.imageSubresource.mipLevel,
        1u,
        region.imageSubresource.baseArrayLayer,
        region.imageSubresource.layerCount
    );
}

/// @brief Records the copy of `stagingBuffer` into `region` of the texture on the command list context.
/// @details `tracked layout -> TRANSFER_DST`, the copy, then back to the tracked layout. The staging buffer is
///          released once the command buffer completes.
/// @param[in,out] cmdList Command list the copy is deferred on.
/// @param[in,out] texture Destination texture.
/// @param[in] stagingBuffer Staging buffer holding the data.
/// @param[in] region Destination region.
void enqueueTextureWrite(
    command::CommandListBase& cmdList,
    VulkanTexture& texture,
    VulkanStagingBuffer* const stagingBuffer,
    const VkBufferImageCopy& region
) noexcept
{
    GP_CHECK(stagingBuffer != nullptr);
    stagingBuffer->flushMappedMemory();

    cmdList.enqueueLambda([texture = VulkanTextureRef(&texture),
                           stagingBuffer,
                           region](command::CommandListBase& executingCmdList) mutable noexcept
    {
        const VulkanResourceServices& services = texture->getServices();
        IVulkanTransferCommandRecorder* const recorder =
            getContextProvider(services).getCommandListRecorder(executingCmdList);
        GP_CHECK(recorder != nullptr);
        GP_CHECK(recorder->isOutsideRenderPass());

        recorder->debugHeavyWeightBarrier(kVulkanDebugBarrierTextureUpload);

        const VkImageSubresourceRange range = makeRegionRange(*texture, region);
        const VkImageLayout trackedLayout = texture->getTrackedLayout();
        recorder->imageLayoutTransition(
            texture->getImage(), trackedLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range
        );
        recorder->copyBufferToImage(
            stagingBuffer->getHandle(),
            texture->getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            gp::ConstVectorView<VkBufferImageCopy>(&region, 1u)
        );
        recorder->imageLayoutTransition(
            texture->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, trackedLayout, range
        );

        VulkanStagingBuffer* releasedBuffer = stagingBuffer;
        services.stagingManager->releaseBuffer(recorder->getCompletionSyncPoint(), releasedBuffer);
    });
}

/// @brief Packs a box of blocks from a strided source into a tightly packed staging buffer.
/// @param[out] destination Staging memory.
/// @param[in] source First source byte of the box.
/// @param[in] rowBytes Bytes of one packed row of blocks.
/// @param[in] rowCount Rows of blocks per slice.
/// @param[in] sliceCount Slices.
/// @param[in] sourceRowPitch Source byte pitch between rows.
/// @param[in] sourceSlicePitch Source byte pitch between slices.
void packRows(
    gp::UInt8* const destination,
    const gp::UInt8* const source,
    const gp::UInt64 rowBytes,
    const gp::UInt32 rowCount,
    const gp::UInt32 sliceCount,
    const gp::UInt64 sourceRowPitch,
    const gp::UInt64 sourceSlicePitch
) noexcept
{
    gp::UInt8* destinationRow = destination;
    for (gp::UInt32 slice = 0u; slice < sliceCount; ++slice)
    {
        const gp::UInt8* sourceRow = source + (static_cast<gp::UInt64>(slice) * sourceSlicePitch);
        for (gp::UInt32 row = 0u; row < rowCount; ++row)
        {
            gp::memory::copyMemory(destinationRow, sourceRow, static_cast<gp::USize>(rowBytes));
            destinationRow += rowBytes;
            sourceRow += sourceRowPitch;
        }
    }
}

/// @brief Packs a texel box into a staging buffer and records its copy into one mip.
/// @param[in,out] cmdList Command list the copy is deferred on.
/// @param[in,out] texture Destination texture.
/// @param[in] mipIndex Destination mip.
/// @param[in] destOffset Destination texel offset.
/// @param[in] srcOffset Source texel offset.
/// @param[in] size Texel extent of the box.
/// @param[in] sourceRowPitch Source byte pitch between rows of blocks.
/// @param[in] sourceSlicePitch Source byte pitch between slices.
/// @param[in] sourceData First byte of the source.
void updateTextureBox(
    command::CommandListBase& cmdList,
    VulkanTexture& texture,
    const gp::UInt32 mipIndex,
    const VkOffset3D destOffset,
    const VkOffset3D srcOffset,
    const VkExtent3D size,
    const gp::UInt32 sourceRowPitch,
    const gp::UInt32 sourceSlicePitch,
    const gp::UInt8* const sourceData
) noexcept
{
    GP_CHECKF(sourceData != nullptr, "Updating a texture requires source data.");
    GP_CHECKF(texture.getImage() != VK_NULL_HANDLE, "Updating a texture without an image.");
    if (size.width == 0u || size.height == 0u || size.depth == 0u)
    {
        return;
    }

    const PixelFormatInfo& info = getPixelFormatInfo(texture.getDesc().format);
    const gp::UInt32 blocksX = gp::math::divideAndRoundUp(size.width, static_cast<gp::UInt32>(info.blockSizeX));
    const gp::UInt32 blocksY = gp::math::divideAndRoundUp(size.height, static_cast<gp::UInt32>(info.blockSizeY));
    const gp::UInt32 blocksZ = gp::math::divideAndRoundUp(size.depth, static_cast<gp::UInt32>(info.blockSizeZ));
    const gp::UInt64 rowBytes = static_cast<gp::UInt64>(blocksX) * info.blockBytes;
    const gp::UInt64 stagingSize = rowBytes * blocksY * blocksZ;

    const VulkanResourceServices& services = texture.getServices();
    VulkanStagingBuffer* const stagingBuffer =
        services.stagingManager->acquireBuffer(static_cast<gp::UInt32>(stagingSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    GP_CHECKF(stagingBuffer != nullptr, "Failed to acquire a texture update staging buffer.");

    const gp::UInt8* const sourceStart =
        sourceData + (static_cast<gp::UInt64>(srcOffset.z) * sourceSlicePitch) +
        (static_cast<gp::UInt64>(srcOffset.y / static_cast<gp::Int32>(info.blockSizeY)) * sourceRowPitch) +
        (static_cast<gp::UInt64>(srcOffset.x / static_cast<gp::Int32>(info.blockSizeX)) * info.blockBytes);
    packRows(
        static_cast<gp::UInt8*>(stagingBuffer->getMappedPointer()),
        sourceStart,
        rowBytes,
        blocksY,
        blocksZ,
        sourceRowPitch,
        sourceSlicePitch
    );

    VkBufferImageCopy region{};
    region.bufferOffset = 0u;
    region.bufferRowLength = blocksX * info.blockSizeX;
    region.bufferImageHeight = blocksY * info.blockSizeY;
    region.imageSubresource.aspectMask = texture.getPartialAspectMask();
    region.imageSubresource.mipLevel = mipIndex;
    region.imageSubresource.baseArrayLayer = 0u;
    region.imageSubresource.layerCount = 1u;
    region.imageOffset = destOffset;
    region.imageExtent = size;

    enqueueTextureWrite(cmdList, texture, stagingBuffer, region);
}

}   // namespace

resource::TextureLockResult
    lockVulkanTexture(command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args) noexcept
{
    VulkanTexture& texture = toVulkanTexture(args.texture);
    const resource::TextureDesc& desc = texture.getDesc();
    const gp::UInt32 arrayLayer = resource::getLockArrayIndex(desc, args);
    const gp::UInt32 mipIndex = args.mipIndex;
    GP_CHECKF(mipIndex < gp::math::max<gp::UInt32>(1u, desc.numMips), "Locking a mip the texture does not have.");

    gp::UInt64 rowStride = 0u;
    gp::UInt64 subresourceSize = 0u;
    static_cast<void>(calculateVulkanSubresourceOffset(desc, mipIndex, 0u, rowStride, subresourceSize));

    const VulkanResourceServices& services = texture.getServices();
    resource::TextureLockResult result{};
    result.stride = static_cast<gp::UInt32>(rowStride);

    // CPU readback textures are host memory already: hand out the mip inside the readback buffer.
    const VulkanCpuReadbackBuffer* const readback = texture.getCpuReadbackBuffer();
    if (readback != nullptr)
    {
        GP_CHECKF(args.lockMode == resource::ResourceLockMode::ReadOnly, "CPU readback textures are read only.");
        VulkanMemoryManager& memoryManager = *services.memoryManager;
        texture.getAllocation().invalidateMappedMemory(memoryManager);

        gp::UInt8* const base = static_cast<gp::UInt8*>(texture.getAllocation().getMappedPointer(memoryManager));
        result.data = (base != nullptr) ? (base + readback->mipOffsets[mipIndex]) : nullptr;
        result.byteCount = readback->mipSizes[mipIndex];
        return result;
    }

    GP_CHECKF(texture.getImage() != VK_NULL_HANDLE, "Locking a texture without an image.");

    VulkanStagingBuffer* stagingBuffer = nullptr;
    if (args.lockMode == resource::ResourceLockMode::ReadOnly)
    {
        stagingBuffer = services.stagingManager->acquireBuffer(
            static_cast<gp::UInt32>(subresourceSize),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT
        );
        GP_CHECKF(stagingBuffer != nullptr, "Failed to acquire a texture readback staging buffer.");

        const VkBufferImageCopy region = makeSubresourceRegion(texture, mipIndex, arrayLayer);
        cmdList.enqueueLambda([heldTexture = VulkanTextureRef(&texture),
                               stagingHandle = stagingBuffer->getHandle(),
                               region](command::CommandListImmediate& executingCmdList) noexcept
        {
            IVulkanTransferCommandRecorder* const recorder =
                getContextProvider(heldTexture->getServices()).getCommandListRecorder(executingCmdList);
            GP_CHECK(recorder != nullptr);

            const VkImageSubresourceRange range = makeRegionRange(*heldTexture, region);
            const VkImageLayout trackedLayout = heldTexture->getTrackedLayout();
            recorder->imageLayoutTransition(
                heldTexture->getImage(), trackedLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, range
            );
            recorder->copyImageToBuffer(
                heldTexture->getImage(),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                stagingHandle,
                gp::ConstVectorView<VkBufferImageCopy>(&region, 1u)
            );
            recorder->imageLayoutTransition(
                heldTexture->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, trackedLayout, range
            );

            // Make the copy visible to the host.
            VkMemoryBarrier barrier = zeroVulkanStruct<VkMemoryBarrier>(VK_STRUCTURE_TYPE_MEMORY_BARRIER);
            barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            recorder->memoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, barrier);
        });

        // The readback must be finished before the CPU reads the staging buffer.
        getContextProvider(services).submitAndBlockUntilGPUIdle(cmdList);
        stagingBuffer->invalidateMappedMemory();
    }
    else
    {
        stagingBuffer = services.stagingManager->acquireBuffer(
            static_cast<gp::UInt32>(subresourceSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        );
        GP_CHECKF(stagingBuffer != nullptr, "Failed to acquire a texture upload staging buffer.");
    }

    VulkanPendingTextureLock pendingLock{};
    pendingLock.stagingBuffer = stagingBuffer;
    pendingLock.mipIndex = mipIndex;
    pendingLock.arrayLayer = arrayLayer;
    pendingLock.lockMode = args.lockMode;
    getVulkanPendingTextureLocks().add(&texture, pendingLock);

    result.data = stagingBuffer->getMappedPointer();
    result.byteCount = subresourceSize;
    return result;
}

void unlockVulkanTexture(command::CommandListImmediate& cmdList, const resource::TextureLockArgs& args) noexcept
{
    VulkanTexture& texture = toVulkanTexture(args.texture);
    if (texture.getCpuReadbackBuffer() != nullptr)
    {
        return;
    }

    const gp::UInt32 arrayLayer = resource::getLockArrayIndex(texture.getDesc(), args);

    VulkanPendingTextureLock pendingLock{};
    const bool isFound = getVulkanPendingTextureLocks().take(&texture, args.mipIndex, arrayLayer, pendingLock);
    GP_CHECKF(isFound, "Mismatched texture lock/unlock.");
    if (!isFound)
    {
        return;
    }

    if (pendingLock.lockMode == resource::ResourceLockMode::ReadOnly)
    {
        // The readback completed before the lock returned; nothing on the GPU references the buffer anymore.
        texture.getServices().stagingManager->releaseBuffer(VulkanSyncPointRef{}, pendingLock.stagingBuffer);
        return;
    }

    enqueueTextureWrite(
        cmdList, texture, pendingLock.stagingBuffer, makeSubresourceRegion(texture, pendingLock.mipIndex, arrayLayer)
    );
}

void updateVulkanTexture2D(
    command::CommandListBase& cmdList,
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion2D& updateRegion,
    const gp::UInt32 sourcePitch,
    const gp::UInt8* const sourceData
) noexcept
{
    const VkOffset3D destOffset{ static_cast<gp::Int32>(updateRegion.destX),
                                 static_cast<gp::Int32>(updateRegion.destY),
                                 0 };
    const VkOffset3D srcOffset{ updateRegion.srcX, updateRegion.srcY, 0 };
    const VkExtent3D size{ updateRegion.width, updateRegion.height, 1u };
    updateTextureBox(
        cmdList, toVulkanTexture(texture), mipIndex, destOffset, srcOffset, size, sourcePitch, 0u, sourceData
    );
}

void updateVulkanTexture3D(
    command::CommandListBase& cmdList,
    resource::Texture* const texture,
    const gp::UInt32 mipIndex,
    const resource::UpdateTextureRegion3D& updateRegion,
    const gp::UInt32 sourceRowPitch,
    const gp::UInt32 sourceDepthPitch,
    const gp::UInt8* const sourceData
) noexcept
{
    const VkOffset3D destOffset{ static_cast<gp::Int32>(updateRegion.destX),
                                 static_cast<gp::Int32>(updateRegion.destY),
                                 static_cast<gp::Int32>(updateRegion.destZ) };
    const VkOffset3D srcOffset{ updateRegion.srcX, updateRegion.srcY, updateRegion.srcZ };
    const VkExtent3D size{ updateRegion.width, updateRegion.height, updateRegion.depth };
    updateTextureBox(
        cmdList,
        toVulkanTexture(texture),
        mipIndex,
        destOffset,
        srcOffset,
        size,
        sourceRowPitch,
        sourceDepthPitch,
        sourceData
    );
}

}   // namespace gp::rhi::vulkan
