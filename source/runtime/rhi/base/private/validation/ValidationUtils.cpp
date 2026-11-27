// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "validation/ValidationUtils.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "resources/Buffer.hpp"
#include "resources/Texture.hpp"
#include "RHIPixelFormat.hpp"
#include "validation/ValidationTypes.hpp"
#include <cstdio>

namespace gp::rhi::validation
{

void ValidationUtils::validateCopyTexture(
    const resource::Texture* const sourceTexture,
    const resource::Texture* const destTexture,
    const command::CopyTextureInfo& copyInfo
) noexcept
{
    char msg[512]{};
    if (sourceTexture == nullptr || destTexture == nullptr)
    {
        reportValidationFailure("copyTexture called with a null sourceTexture or destTexture.");
        return;
    }

    const resource::TextureDesc& srcDesc = sourceTexture->getDesc();
    const resource::TextureDesc& dstDesc = destTexture->getDesc();

    if (srcDesc.isMultisample() || dstDesc.isMultisample())
    {
        const gp::String srcName = sourceTexture->getName().toString();
        const gp::String dstName = destTexture->getName().toString();
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "copyTexture from '%s' (samples=%u) to '%s' (samples=%u): multisampled textures cannot be copied via copyTexture.",
            srcName.cString(),
            static_cast<gp::UInt32>(srcDesc.numSamples),
            dstName.cString(),
            static_cast<gp::UInt32>(dstDesc.numSamples)
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    if (copyInfo.numMips == 0u || copyInfo.numSlices == 0u)
    {
        reportValidationFailure("copyTexture requires numMips >= 1 and numSlices >= 1.");
        return;
    }

    if ((copyInfo.sourceMipIndex + copyInfo.numMips) > static_cast<gp::UInt32>(srcDesc.numMips) ||
        (copyInfo.destMipIndex + copyInfo.numMips) > static_cast<gp::UInt32>(dstDesc.numMips))
    {
        const gp::String srcName = sourceTexture->getName().toString();
        const gp::String dstName = destTexture->getName().toString();
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "copyTexture mip range out of bounds: src='%s' [%u..%u) of %u, dst='%s' [%u..%u) of %u.",
            srcName.cString(),
            copyInfo.sourceMipIndex,
            copyInfo.sourceMipIndex + copyInfo.numMips,
            static_cast<gp::UInt32>(srcDesc.numMips),
            dstName.cString(),
            copyInfo.destMipIndex,
            copyInfo.destMipIndex + copyInfo.numMips,
            static_cast<gp::UInt32>(dstDesc.numMips)
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    const gp::UInt32 srcSlices = srcDesc.isTextureCube()
                                     ? (static_cast<gp::UInt32>(srcDesc.arraySize) * resource::kCubeFaceCount)
                                     : static_cast<gp::UInt32>(srcDesc.arraySize);
    const gp::UInt32 dstSlices = dstDesc.isTextureCube()
                                     ? (static_cast<gp::UInt32>(dstDesc.arraySize) * resource::kCubeFaceCount)
                                     : static_cast<gp::UInt32>(dstDesc.arraySize);
    if ((copyInfo.sourceSliceIndex + copyInfo.numSlices) > srcSlices ||
        (copyInfo.destSliceIndex + copyInfo.numSlices) > dstSlices)
    {
        const gp::String srcName = sourceTexture->getName().toString();
        const gp::String dstName = destTexture->getName().toString();
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "copyTexture slice range out of bounds: src='%s' [%u..%u) of %u, dst='%s' [%u..%u) of %u.",
            srcName.cString(),
            copyInfo.sourceSliceIndex,
            copyInfo.sourceSliceIndex + copyInfo.numSlices,
            srcSlices,
            dstName.cString(),
            copyInfo.destSliceIndex,
            copyInfo.destSliceIndex + copyInfo.numSlices,
            dstSlices
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
        return;
    }

    if (copyInfo.sourcePosition.x < 0 || copyInfo.sourcePosition.y < 0 || copyInfo.sourcePosition.z < 0 ||
        copyInfo.destPosition.x < 0 || copyInfo.destPosition.y < 0 || copyInfo.destPosition.z < 0 ||
        copyInfo.size.x < 0 || copyInfo.size.y < 0 || copyInfo.size.z < 0)
    {
        reportValidationFailure("copyTexture positions and size components must be non-negative.");
        return;
    }

    const resource::IntVector srcMipSize =
        sourceTexture->getMipDimensions(static_cast<gp::UInt8>(copyInfo.sourceMipIndex));
    const resource::IntVector dstMipSize = destTexture->getMipDimensions(static_cast<gp::UInt8>(copyInfo.destMipIndex));

    const gp::Int32 regionX = (copyInfo.size.x > 0) ? copyInfo.size.x : srcMipSize.x;
    const gp::Int32 regionY = (copyInfo.size.y > 0) ? copyInfo.size.y : srcMipSize.y;
    const gp::Int32 regionZ = (copyInfo.size.z > 0) ? copyInfo.size.z : srcMipSize.z;

    if ((copyInfo.sourcePosition.x + regionX) > srcMipSize.x || (copyInfo.sourcePosition.y + regionY) > srcMipSize.y ||
        (copyInfo.sourcePosition.z + regionZ) > srcMipSize.z || (copyInfo.destPosition.x + regionX) > dstMipSize.x ||
        (copyInfo.destPosition.y + regionY) > dstMipSize.y || (copyInfo.destPosition.z + regionZ) > dstMipSize.z)
    {
        const gp::String srcName = sourceTexture->getName().toString();
        const gp::String dstName = destTexture->getName().toString();
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "copyTexture region (%d, %d, %d) exceeds mip extent: src='%s' (%d, %d, %d), dst='%s' (%d, %d, %d).",
            regionX,
            regionY,
            regionZ,
            srcName.cString(),
            srcMipSize.x,
            srcMipSize.y,
            srcMipSize.z,
            dstName.cString(),
            dstMipSize.x,
            dstMipSize.y,
            dstMipSize.z
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }

    if (isValidPixelFormat(srcDesc.format))
    {
        const PixelFormatInfo& srcFmt = getPixelFormatInfo(srcDesc.format);
        const gp::Int32 bx = static_cast<gp::Int32>(gp::math::max<gp::UInt32>(1u, srcFmt.blockSizeX));
        const gp::Int32 by = static_cast<gp::Int32>(gp::math::max<gp::UInt32>(1u, srcFmt.blockSizeY));
        if ((copyInfo.sourcePosition.x % bx) != 0 || (copyInfo.sourcePosition.y % by) != 0)
        {
            reportValidationFailure("copyTexture sourcePosition is not aligned to compressed pixel format block size.");
        }
    }
}

void ValidationUtils::validateCopyBufferRegion(
    const resource::Buffer* const destBuffer,
    const gp::UInt64 dstOffset,
    const resource::Buffer* const sourceBuffer,
    const gp::UInt64 srcOffset,
    const gp::UInt64 numBytes
) noexcept
{
    if (destBuffer == nullptr || sourceBuffer == nullptr)
    {
        reportValidationFailure("copyBufferRegion called with a null sourceBuffer or destBuffer.");
        return;
    }

    if (destBuffer == sourceBuffer)
    {
        reportValidationFailure("copyBufferRegion cannot copy from and to the same Buffer instance.");
        return;
    }

    if ((dstOffset + numBytes) > static_cast<gp::UInt64>(destBuffer->getSize()) ||
        (srcOffset + numBytes) > static_cast<gp::UInt64>(sourceBuffer->getSize()))
    {
        char msg[256]{};
        const int len = std::snprintf(
            msg,
            sizeof(msg),
            "copyBufferRegion range out of bounds: src [offset=%llu, numBytes=%llu, size=%u], dst [offset=%llu, size=%u].",
            static_cast<unsigned long long>(srcOffset),
            static_cast<unsigned long long>(numBytes),
            destBuffer != nullptr ? sourceBuffer->getSize() : 0u,
            static_cast<unsigned long long>(dstOffset),
            destBuffer->getSize()
        );
        if (len > 0)
        {
            reportValidationFailure(gp::StringView(msg, static_cast<gp::USize>(len)));
        }
    }
}

}   // namespace gp::rhi::validation
