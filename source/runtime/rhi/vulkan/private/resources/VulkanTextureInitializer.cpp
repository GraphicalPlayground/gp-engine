// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanTextureInitializer.hpp"
#include "barriers/VulkanPipelineBarrier.hpp"
#include "commands/CommandListBase.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "memory/VulkanStagingManager.hpp"
#include "resources/IVulkanCommandContextProvider.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureInitializerHelpers.hpp"
#include "resources/TextureSubresourceHelpers.hpp"
#include "resources/TextureUtils.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanTextureUsage.hpp"
#include "RHIDefinitions.hpp"
#include "RHIVulkan.hpp"
#include "utils/VulkanDebugBarrier.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Reference-counted handle to a Vulkan texture.
using VulkanTextureRef = gp::RefCountedPtr<VulkanTexture>;

/// @brief Acquires a host-visible staging buffer large enough for every subresource of `desc`.
/// @param[in] services Texture services.
/// @param[in] desc Texture description.
/// @param[out] outSize Receives the byte size of the packed data.
/// @return Staging buffer, or `nullptr` when the texture holds no data.
[[nodiscard]] VulkanStagingBuffer* acquireUploadBuffer(
    const VulkanResourceServices& services, const resource::TextureDesc& desc, gp::UInt64& outSize
) noexcept
{
    outSize = resource::calculateTextureSize(desc);
    if (outSize == 0u)
    {
        return nullptr;
    }

    VulkanStagingBuffer* const stagingBuffer =
        services.stagingManager->acquireBuffer(static_cast<gp::UInt32>(outSize), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    GP_CHECKF(stagingBuffer != nullptr, "Failed to acquire a texture upload staging buffer.");
    return stagingBuffer;
}

/// @brief Builds the initializer exposing `stagingBuffer` with the mip-major subresource layout.
/// @param[in,out] cmdList Command list driving the initialization.
/// @param[in] texture Texture being initialized; already referenced by the caller.
/// @param[in] stagingBuffer Staging buffer the caller writes into; uploaded on finalize.
/// @param[in] stagingSize Byte size of the packed data.
/// @return Initializer that uploads on finalize.
[[nodiscard]] resource::TextureInitializer createStagedTextureInitializer(
    command::CommandListBase& cmdList,
    const VulkanTextureRef& texture,
    VulkanStagingBuffer* const stagingBuffer,
    const gp::UInt64 stagingSize
) noexcept
{
    void* const stagingData = stagingBuffer->getMappedPointer();

    return resource::BaseTextureInitializer(
        cmdList,
        texture.get(),
        stagingData,
        stagingSize,
        [heldTexture = texture,
         heldStagingBuffer =
             stagingBuffer](command::CommandListBase& finalizeCmdList) mutable noexcept -> resource::TextureRef
    {
        recordVulkanTextureUpload(finalizeCmdList, *heldTexture, heldStagingBuffer);
        heldTexture->registerPoolOwner(finalizeCmdList);
        return resource::TextureRef(std::move(heldTexture));
    },
        [heldTexture = texture,
         stagingData](const resource::TextureSubresourceIndex index) noexcept -> resource::TextureSubresourceInitializer
    {
        const resource::TextureDesc& desc = heldTexture->getDesc();
        const gp::UInt32 layerIndex = resource::getCombinedArrayIndex(
            desc, static_cast<gp::UInt32>(index.faceIndex), static_cast<gp::UInt32>(index.arrayIndex)
        );

        gp::UInt64 rowStride = 0u;
        gp::UInt64 subresourceSize = 0u;
        const gp::UInt64 offset = calculateVulkanSubresourceOffset(
            desc, static_cast<gp::UInt32>(index.mipIndex), layerIndex, rowStride, subresourceSize
        );

        resource::TextureSubresourceInitializer result{};
        if (subresourceSize > 0u)
        {
            result.writableData = static_cast<gp::UInt8*>(stagingData) + offset;
            result.stride = rowStride;
            result.size = subresourceSize;
        }
        return result;
    }
    );
}

/// @brief Drops the initial data of a texture that cannot receive it.
/// @param[in] createDesc Creation description.
void discardInitialData(const resource::TextureCreateDesc& createDesc) noexcept
{
    if (createDesc.initAction == resource::TextureInitAction::BulkData && createDesc.bulkData != nullptr)
    {
        createDesc.bulkData->discard();
    }
}

}   // namespace

void recordVulkanTextureUpload(
    command::CommandListBase& cmdList, VulkanTexture& texture, VulkanStagingBuffer*& stagingBuffer
) noexcept
{
    GP_CHECK(stagingBuffer != nullptr);
    GP_CHECKF(texture.getImage() != VK_NULL_HANDLE, "Uploading into a texture without an image.");

    stagingBuffer->flushMappedMemory();

    VulkanStagingBuffer* const heldStagingBuffer = stagingBuffer;
    stagingBuffer = nullptr;

    cmdList.enqueueLambda([texture = VulkanTextureRef(&texture),
                           heldStagingBuffer](command::CommandListBase& executingCmdList) mutable noexcept
    {
        const VulkanResourceServices& services = texture->getServices();
        GP_CHECKF(services.contextProvider != nullptr, "Uploading texture data requires a command context provider.");

        IVulkanTransferCommandRecorder* const recorder = services.contextProvider->getUploadRecorder(executingCmdList);
        GP_CHECK(recorder != nullptr);
        GP_CHECK(recorder->isOutsideRenderPass());

        recorder->debugHeavyWeightBarrier(kVulkanDebugBarrierTextureUpload);

        const resource::TextureDesc& desc = texture->getDesc();
        const gp::UInt32 mipCount = gp::math::max<gp::UInt32>(1u, desc.numMips);
        const gp::UInt32 layerCount = getVulkanTextureLayerCount(desc);

        // One region per mip: in the mip-major layout every layer of a mip is contiguous, which is exactly how
        // Vulkan reads a multi-layer region with a tightly packed row length.
        VkBufferImageCopy regions[kMaxTextureMipCount]{};
        for (gp::UInt32 mipIndex = 0u; mipIndex < mipCount; ++mipIndex)
        {
            gp::UInt64 rowStride = 0u;
            gp::UInt64 subresourceSize = 0u;
            const resource::IntVector mipExtent = resource::calculateMipExtent(desc, mipIndex);

            VkBufferImageCopy& region = regions[mipIndex];
            region.bufferOffset = calculateVulkanSubresourceOffset(desc, mipIndex, 0u, rowStride, subresourceSize);
            region.bufferRowLength = 0u;
            region.bufferImageHeight = 0u;
            region.imageSubresource.aspectMask = texture->getPartialAspectMask();
            region.imageSubresource.mipLevel = mipIndex;
            region.imageSubresource.baseArrayLayer = 0u;
            region.imageSubresource.layerCount = layerCount;
            region.imageExtent.width = static_cast<gp::UInt32>(mipExtent.x);
            region.imageExtent.height = static_cast<gp::UInt32>(mipExtent.y);
            region.imageExtent.depth = static_cast<gp::UInt32>(mipExtent.z);
        }

        const VkImageSubresourceRange fullRange =
            VulkanPipelineBarrier::makeSubresourceRange(texture->getFullAspectMask());
        recorder->imageLayoutTransition(
            texture->getImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, fullRange
        );
        recorder->copyBufferToImage(
            heldStagingBuffer->getHandle(),
            texture->getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            gp::ConstVectorView<VkBufferImageCopy>(regions, mipCount)
        );
        recorder->imageLayoutTransition(
            texture->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->getDefaultLayout(), fullRange
        );

        VulkanStagingBuffer* releasedBuffer = heldStagingBuffer;
        services.stagingManager->releaseBuffer(recorder->getCompletionSyncPoint(), releasedBuffer);
    });
}

resource::TextureInitializer createVulkanTextureInitializer(
    const VulkanResourceServices& services,
    command::CommandListBase& cmdList,
    const resource::TextureCreateDesc& createDesc
) noexcept
{
    // Hold a reference for the whole function: deferred lambdas below reference the texture before the initializer
    // does.
    const VulkanTextureRef texture(new VulkanTexture(services, cmdList, createDesc));

    if (createDesc.initAction == resource::TextureInitAction::Default)
    {
        texture->registerPoolOwner(cmdList);
        return resource::DefaultTextureInitializer(cmdList, texture.get());
    }

    if (texture->getImage() == VK_NULL_HANDLE)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "Texture '{}' cannot receive initial data (CPU readback or creation failure).",
            createDesc.debugName
        );
        discardInitialData(createDesc);
        texture->registerPoolOwner(cmdList);
        return resource::DefaultTextureInitializer(cmdList, texture.get());
    }

    gp::UInt64 stagingSize = 0u;
    VulkanStagingBuffer* stagingBuffer = acquireUploadBuffer(services, texture->getDesc(), stagingSize);
    if (stagingBuffer == nullptr)
    {
        discardInitialData(createDesc);
        texture->registerPoolOwner(cmdList);
        return resource::DefaultTextureInitializer(cmdList, texture.get());
    }

    if (createDesc.initAction == resource::TextureInitAction::BulkData)
    {
        GP_CHECKF(createDesc.bulkData != nullptr, "BulkData initialization requires bulk data.");

        const gp::UInt64 bulkSize = createDesc.bulkData->getResourceBulkDataSize();
        GP_CHECKF(bulkSize <= stagingSize, "Bulk data is larger than the texture.");
        gp::memory::copyMemory(
            stagingBuffer->getMappedPointer(),
            createDesc.bulkData->getResourceBulkData(),
            static_cast<gp::USize>(gp::math::min(bulkSize, stagingSize))
        );
        createDesc.bulkData->discard();

        recordVulkanTextureUpload(cmdList, *texture, stagingBuffer);
        texture->registerPoolOwner(cmdList);
        return resource::DefaultTextureInitializer(cmdList, texture.get());
    }

    if (createDesc.initAction == resource::TextureInitAction::Initializer)
    {
        return createStagedTextureInitializer(cmdList, texture, stagingBuffer, stagingSize);
    }

    if (createDesc.initAction == resource::TextureInitAction::DataProvider)
    {
        GP_CHECKF(createDesc.dataProvider != nullptr, "DataProvider initialization requires a data provider.");

        resource::TextureInitializer initializer =
            createStagedTextureInitializer(cmdList, texture, stagingBuffer, stagingSize);
        resource::fillTextureInitializerFromDataProvider(initializer, texture->getDesc(), *createDesc.dataProvider);
        return initializer;
    }

    services.stagingManager->releaseBuffer(VulkanSyncPointRef{}, stagingBuffer);
    return resource::handleUnknownTextureInitializerInitAction(cmdList, createDesc);
}

}   // namespace gp::rhi::vulkan
