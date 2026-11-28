// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanTextureReadback.hpp"
#include "barriers/VulkanPipelineBarrier.hpp"
#include "commands/CommandListImmediate.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "memory/pointers/RefCountedPtr.hpp"
#include "memory/VulkanMemoryManager.hpp"
#include "memory/VulkanStagingManager.hpp"
#include "resources/IVulkanCommandContextProvider.hpp"
#include "resources/IVulkanTransferCommandRecorder.hpp"
#include "resources/Texture.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureSubresourceHelpers.hpp"
#include "resources/TextureUtils.hpp"
#include "resources/VulkanResourceServices.hpp"
#include "resources/VulkanSurfaceConversion.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "resources/VulkanTextureUsage.hpp"
#include "RHIPixelFormat.hpp"
#include "sync/VulkanSyncPoint.hpp"
#include "VulkanCommon.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Reference-counted handle to a Vulkan texture.
using VulkanTextureRef = gp::RefCountedPtr<VulkanTexture>;

/// @brief Subresource and box a readback copies.
struct SurfaceReadbackRequest final
{
    gp::UInt32 mipIndex{ 0u };
    gp::UInt32 arrayLayer{ 0u };
    resource::IntRect rect{};
    gp::Int32 zMin{ 0 };
    gp::Int32 zMax{ 1 };
    bool outputStencil{ false };
};

/// @brief Host-visible texels of a readback, valid until `releaseSurfaceReadback()`.
struct SurfaceReadback final
{
    const gp::UInt8* data{ nullptr };
    gp::UInt64 rowPitch{ 0u };
    gp::UInt64 slicePitch{ 0u };
    gp::UInt32 width{ 0u };
    gp::UInt32 height{ 0u };
    gp::UInt32 depth{ 0u };
    PixelFormat format{ PixelFormat::Unknown };
    VulkanStagingBuffer* stagingBuffer{ nullptr };
};

/// @brief Downcasts an RHI texture to the Vulkan implementation.
/// @param[in] texture RHI texture; must be a `VulkanTexture`.
/// @return Vulkan texture.
[[nodiscard]] VulkanTexture& toVulkanTexture(resource::Texture* const texture) noexcept
{
    GP_CHECK(texture != nullptr);
    return *static_cast<VulkanTexture*>(texture);
}

/// @brief Reports whether a depth/stencil format has a stencil aspect.
/// @param[in] format Format to test.
/// @return True for combined depth/stencil formats.
[[nodiscard]] bool hasStencilAspect(const PixelFormat format) noexcept
{
    return format == PixelFormat::D24_UNorm_S8_UInt || format == PixelFormat::D32_Float_S8_UInt;
}

/// @brief Clamps `value` to `[low, high]`.
/// @param[in] value Value to clamp.
/// @param[in] low Lower bound.
/// @param[in] high Upper bound (not below `low`).
/// @return Clamped value.
[[nodiscard]] gp::Int32 clampInt(const gp::Int32 value, const gp::Int32 low, const gp::Int32 high) noexcept
{
    return gp::math::min(gp::math::max(value, low), high);
}

/// @brief Copies a box of a subresource to host memory.
/// @details `CPUReadback` textures are read in place. Other textures are copied into a host-cached staging buffer
///          on the command list context, followed by a `TRANSFER -> HOST` barrier; the list is then submitted and the
///          thread blocks until the GPU is idle.
/// @param[in,out] cmdList Immediate command list.
/// @param[in,out] texture Source texture.
/// @param[in] request Subresource and box; the box is clamped to the mip.
/// @param[out] outReadback Host texels on success.
/// @return False when the texture or format cannot be read back, or the box is empty.
[[nodiscard]] bool readSurfaceToHost(
    command::CommandListImmediate& cmdList,
    VulkanTexture& texture,
    const SurfaceReadbackRequest& request,
    SurfaceReadback& outReadback
) noexcept
{
    const resource::TextureDesc& desc = texture.getDesc();
    if (request.mipIndex >= gp::math::max<gp::UInt32>(1u, desc.numMips))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Surface readback of mip {} the texture does not have.", request.mipIndex);
        return false;
    }
    if (desc.numSamples > 1u)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Surface readback of a multisampled texture is not supported; resolve it first.");
        return false;
    }

    const bool readsStencil = request.outputStencil && hasStencilAspect(desc.format);
    const PixelFormat texelFormat = readsStencil ? PixelFormat::R8_UNorm : desc.format;
    const gp::UInt32 texelBytes = getVulkanSurfaceTexelBytes(texelFormat);
    if (texelBytes == 0u)
    {
        GP_LOG_ERROR(
            LogVulkanRHI, "Surface readback does not support format {}.", static_cast<gp::UInt32>(desc.format)
        );
        return false;
    }

    const resource::IntVector mipExtent = resource::calculateMipExtent(desc, request.mipIndex);
    const gp::Int32 minX = clampInt(request.rect.min.x, 0, mipExtent.x);
    const gp::Int32 minY = clampInt(request.rect.min.y, 0, mipExtent.y);
    const gp::Int32 maxX = clampInt(request.rect.max.x, minX, mipExtent.x);
    const gp::Int32 maxY = clampInt(request.rect.max.y, minY, mipExtent.y);
    const gp::Int32 minZ = clampInt(request.zMin, 0, mipExtent.z);
    const gp::Int32 maxZ = clampInt(request.zMax, minZ, mipExtent.z);
    if (maxX == minX || maxY == minY || maxZ == minZ)
    {
        return false;
    }

    outReadback.width = static_cast<gp::UInt32>(maxX - minX);
    outReadback.height = static_cast<gp::UInt32>(maxY - minY);
    outReadback.depth = static_cast<gp::UInt32>(maxZ - minZ);
    outReadback.format = texelFormat;

    const VulkanResourceServices& services = texture.getServices();
    const VulkanCpuReadbackBuffer* const readback = texture.getCpuReadbackBuffer();
    if (readback != nullptr)
    {
        // The readback buffer holds tightly packed rows of the 2D mip.
        gp::UInt64 rowStride = 0u;
        gp::UInt64 subresourceSize = 0u;
        static_cast<void>(calculateVulkanSubresourceOffset(desc, request.mipIndex, 0u, rowStride, subresourceSize));

        VulkanMemoryManager& memoryManager = *services.memoryManager;
        texture.getAllocation().invalidateMappedMemory(memoryManager);
        const gp::UInt8* const base =
            static_cast<const gp::UInt8*>(texture.getAllocation().getMappedPointer(memoryManager));
        if (base == nullptr)
        {
            return false;
        }

        outReadback.data = base + readback->mipOffsets[request.mipIndex] + static_cast<gp::UInt64>(minY) * rowStride +
                           static_cast<gp::UInt64>(minX) * texelBytes;
        outReadback.rowPitch = rowStride;
        outReadback.slicePitch = subresourceSize;
        return true;
    }

    if (texture.getImage() == VK_NULL_HANDLE)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Surface readback of a texture without an image.");
        return false;
    }
    GP_CHECKF(services.contextProvider != nullptr, "Surface readback requires a command context provider.");

    outReadback.rowPitch = static_cast<gp::UInt64>(outReadback.width) * texelBytes;
    outReadback.slicePitch = outReadback.rowPitch * outReadback.height;
    const gp::UInt64 stagingSize = outReadback.slicePitch * outReadback.depth;
    outReadback.stagingBuffer = services.stagingManager->acquireBuffer(
        static_cast<gp::UInt32>(stagingSize), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT
    );
    GP_CHECKF(outReadback.stagingBuffer != nullptr, "Failed to acquire a surface readback staging buffer.");

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = readsStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : texture.getPartialAspectMask();
    region.imageSubresource.mipLevel = request.mipIndex;
    region.imageSubresource.baseArrayLayer = request.arrayLayer;
    region.imageSubresource.layerCount = 1u;
    region.imageOffset = VkOffset3D{ minX, minY, minZ };
    region.imageExtent = VkExtent3D{ outReadback.width, outReadback.height, outReadback.depth };

    const VkImageSubresourceRange range = VulkanPipelineBarrier::makeSubresourceRange(
        texture.getFullAspectMask(), request.mipIndex, 1u, request.arrayLayer, 1u
    );

    cmdList.enqueueLambda([heldTexture = VulkanTextureRef(&texture),
                           stagingHandle = outReadback.stagingBuffer->getHandle(),
                           region,
                           range](command::CommandListImmediate& executingCmdList) noexcept
    {
        IVulkanTransferCommandRecorder* const recorder =
            heldTexture->getServices().contextProvider->getCommandListRecorder(executingCmdList);
        GP_CHECK(recorder != nullptr);

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

    services.contextProvider->submitAndBlockUntilGPUIdle(cmdList);
    outReadback.stagingBuffer->invalidateMappedMemory();
    outReadback.data = static_cast<const gp::UInt8*>(outReadback.stagingBuffer->getMappedPointer());
    return outReadback.data != nullptr;
}

/// @brief Releases the staging buffer of a readback, if any.
/// @details The GPU finished with it before the readback returned, so no sync point is needed.
/// @param[in] texture Texture that was read.
/// @param[in,out] readback Readback to release; its data becomes invalid.
void releaseSurfaceReadback(const VulkanTexture& texture, SurfaceReadback& readback) noexcept
{
    if (readback.stagingBuffer != nullptr)
    {
        texture.getServices().stagingManager->releaseBuffer(VulkanSyncPointRef{}, readback.stagingBuffer);
    }
    readback.data = nullptr;
}

/// @brief Builds the readback request of one 2D subresource.
/// @param[in] texture Source texture.
/// @param[in] rect Texel rectangle.
/// @param[in] cubeFace Cube face.
/// @param[in] arrayIndex Array slice.
/// @param[in] mipIndex Mip level.
/// @param[in] outputStencil True to read the stencil aspect.
/// @return Request for slice `0`.
[[nodiscard]] SurfaceReadbackRequest makeSurfaceRequest(
    const VulkanTexture& texture,
    const resource::IntRect& rect,
    const resource::CubeFace cubeFace,
    const gp::Int32 arrayIndex,
    const gp::Int32 mipIndex,
    const bool outputStencil
) noexcept
{
    SurfaceReadbackRequest request{};
    request.mipIndex = static_cast<gp::UInt32>(gp::math::max(mipIndex, 0));
    request.arrayLayer = resource::getCombinedArrayIndex(
        texture.getDesc(), static_cast<gp::UInt32>(cubeFace), static_cast<gp::UInt32>(gp::math::max(arrayIndex, 0))
    );
    request.rect = rect;
    request.outputStencil = outputStencil;
    return request;
}

}   // namespace

bool readVulkanSurfaceData(
    command::CommandListImmediate& cmdList,
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::Color>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept
{
    outData.clear();
    VulkanTexture& vulkanTexture = toVulkanTexture(texture);
    const SurfaceReadbackRequest request =
        makeSurfaceRequest(vulkanTexture, rect, flags.cubeFace, flags.arrayIndex, flags.mipIndex, flags.outputStencil);

    SurfaceReadback readback{};
    if (!readSurfaceToHost(cmdList, vulkanTexture, request, readback))
    {
        releaseSurfaceReadback(vulkanTexture, readback);
        return false;
    }

    outData.resizeUninitialized(static_cast<gp::USize>(readback.width) * readback.height);
    const bool isConverted = convertVulkanSurfaceToColor(
        readback.format,
        readback.data,
        static_cast<gp::UInt32>(readback.rowPitch),
        readback.width,
        readback.height,
        flags.linearToGamma,
        outData.data()
    );
    releaseSurfaceReadback(vulkanTexture, readback);
    if (!isConverted)
    {
        outData.clear();
    }
    return isConverted;
}

bool readVulkanSurfaceData(
    command::CommandListImmediate& cmdList,
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<gp::math::LinearColor>& outData,
    const resource::ReadSurfaceDataFlags& flags
) noexcept
{
    outData.clear();
    VulkanTexture& vulkanTexture = toVulkanTexture(texture);
    const SurfaceReadbackRequest request =
        makeSurfaceRequest(vulkanTexture, rect, flags.cubeFace, flags.arrayIndex, flags.mipIndex, flags.outputStencil);

    SurfaceReadback readback{};
    if (!readSurfaceToHost(cmdList, vulkanTexture, request, readback))
    {
        releaseSurfaceReadback(vulkanTexture, readback);
        return false;
    }

    const bool isSRGB = hasAnyTextureFlags(vulkanTexture.getDesc().flags, resource::TextureCreateFlags::SRGB);
    outData.resizeUninitialized(static_cast<gp::USize>(readback.width) * readback.height);
    const bool isConverted = convertVulkanSurfaceToLinearColor(
        readback.format,
        readback.data,
        static_cast<gp::UInt32>(readback.rowPitch),
        readback.width,
        readback.height,
        isSRGB,
        outData.data()
    );
    releaseSurfaceReadback(vulkanTexture, readback);
    if (!isConverted)
    {
        outData.clear();
    }
    return isConverted;
}

bool readVulkanSurfaceFloatData(
    command::CommandListImmediate& cmdList,
    resource::Texture* const texture,
    const resource::IntRect& rect,
    gp::Vector<resource::Float16Color>& outData,
    const resource::CubeFace cubeFace,
    const gp::Int32 arrayIndex,
    const gp::Int32 mipIndex
) noexcept
{
    outData.clear();
    VulkanTexture& vulkanTexture = toVulkanTexture(texture);
    const SurfaceReadbackRequest request =
        makeSurfaceRequest(vulkanTexture, rect, cubeFace, arrayIndex, mipIndex, false);

    SurfaceReadback readback{};
    if (!readSurfaceToHost(cmdList, vulkanTexture, request, readback))
    {
        releaseSurfaceReadback(vulkanTexture, readback);
        return false;
    }

    outData.resizeUninitialized(static_cast<gp::USize>(readback.width) * readback.height);
    const bool isConverted = convertVulkanSurfaceToFloat16Color(
        readback.format,
        readback.data,
        static_cast<gp::UInt32>(readback.rowPitch),
        readback.width,
        readback.height,
        outData.data()
    );
    releaseSurfaceReadback(vulkanTexture, readback);
    if (!isConverted)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Float surface readback requires a float format.");
        outData.clear();
    }
    return isConverted;
}

bool read3DVulkanSurfaceFloatData(
    command::CommandListImmediate& cmdList,
    resource::Texture* const texture,
    const resource::IntRect& rect,
    const resource::IntPoint zMinMax,
    gp::Vector<resource::Float16Color>& outData
) noexcept
{
    outData.clear();
    VulkanTexture& vulkanTexture = toVulkanTexture(texture);
    if (vulkanTexture.getDesc().dimension != resource::TextureDimension::Texture3D)
    {
        GP_LOG_ERROR(LogVulkanRHI, "3D surface readback requires a 3D texture.");
        return false;
    }

    SurfaceReadbackRequest request{};
    request.rect = rect;
    request.zMin = zMinMax.x;
    request.zMax = zMinMax.y;

    SurfaceReadback readback{};
    if (!readSurfaceToHost(cmdList, vulkanTexture, request, readback))
    {
        releaseSurfaceReadback(vulkanTexture, readback);
        return false;
    }

    const gp::USize sliceTexels = static_cast<gp::USize>(readback.width) * readback.height;
    outData.resizeUninitialized(sliceTexels * readback.depth);
    bool isConverted = true;
    for (gp::UInt32 slice = 0u; slice < readback.depth && isConverted; ++slice)
    {
        isConverted = convertVulkanSurfaceToFloat16Color(
            readback.format,
            readback.data + static_cast<gp::UInt64>(slice) * readback.slicePitch,
            static_cast<gp::UInt32>(readback.rowPitch),
            readback.width,
            readback.height,
            outData.data() + static_cast<gp::USize>(slice) * sliceTexels
        );
    }
    releaseSurfaceReadback(vulkanTexture, readback);
    if (!isConverted)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Float surface readback requires a float format.");
        outData.clear();
    }
    return isConverted;
}

void mapVulkanStagingSurface(
    resource::Texture* const texture, void*& outData, gp::Int32& outWidth, gp::Int32& outHeight
) noexcept
{
    outData = nullptr;
    outWidth = 0;
    outHeight = 0;

    VulkanTexture& vulkanTexture = toVulkanTexture(texture);
    const VulkanCpuReadbackBuffer* const readback = vulkanTexture.getCpuReadbackBuffer();
    if (readback == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Mapping a staging surface requires a CPU readback texture.");
        return;
    }

    const resource::TextureDesc& desc = vulkanTexture.getDesc();
    gp::UInt64 rowStride = 0u;
    gp::UInt64 subresourceSize = 0u;
    static_cast<void>(calculateVulkanSubresourceOffset(desc, 0u, 0u, rowStride, subresourceSize));

    VulkanMemoryManager& memoryManager = *vulkanTexture.getServices().memoryManager;
    vulkanTexture.getAllocation().invalidateMappedMemory(memoryManager);
    gp::UInt8* const base = static_cast<gp::UInt8*>(vulkanTexture.getAllocation().getMappedPointer(memoryManager));
    if (base == nullptr)
    {
        return;
    }

    const PixelFormatInfo& info = getPixelFormatInfo(desc.format);
    outData = base + readback->mipOffsets[0];
    outWidth = static_cast<gp::Int32>((rowStride / info.blockBytes) * info.blockSizeX);
    outHeight = desc.extent.y;
}

void unmapVulkanStagingSurface(resource::Texture* const texture) noexcept
{
    GP_CHECKF(
        toVulkanTexture(texture).getCpuReadbackBuffer() != nullptr,
        "Unmapping a staging surface requires a CPU readback texture."
    );
}

void aliasVulkanTextureResources(resource::TextureRef& destTexture, resource::TextureRef& srcTexture) noexcept
{
    GP_CHECKF(destTexture.get() != nullptr && srcTexture.get() != nullptr, "Aliasing requires two textures.");
    toVulkanTexture(destTexture.get()).aliasTextureResources(toVulkanTexture(srcTexture.get()));
}

}   // namespace gp::rhi::vulkan
