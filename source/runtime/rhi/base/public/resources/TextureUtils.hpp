// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIPixelFormat.hpp"

namespace gp::rhi::resource
{

/// @brief Computes the `(width, height, depth)` texel extent of `desc` at `mipIndex`.
/// @param[in] desc Texture descriptor.
/// @param[in] mipIndex Zero-based mip level index.
/// @return Texel dimensions clamped to at least `1` per active axis.
[[nodiscard]] inline constexpr IntVector calculateMipExtent(const TextureDesc& desc, const gp::UInt32 mipIndex) noexcept
{
    const gp::UInt32 clampedShift = (mipIndex < 32u) ? mipIndex : 31u;
    const gp::Int32 mipW = gp::math::max<gp::Int32>(1, desc.extent.x >> clampedShift);
    const gp::Int32 mipH = gp::math::max<gp::Int32>(1, desc.extent.y >> clampedShift);
    const gp::Int32 mipD =
        desc.isTexture3D() ? gp::math::max<gp::Int32>(1, static_cast<gp::Int32>(desc.depth) >> clampedShift) : 1;
    return IntVector(mipW, mipH, mipD);
}

/// @brief Computes the block-grid dimensions `(blocksX, blocksY, blocksZ)` of `desc` at `mipIndex`.
/// @param[in] desc Texture descriptor.
/// @param[in] mipIndex Zero-based mip level index.
/// @return Block dimensions along X, Y, and Z, or `(0, 0, 0)` when `desc.format` is invalid.
[[nodiscard]] inline constexpr IntVector
    calculateMipBlockCount(const TextureDesc& desc, const gp::UInt32 mipIndex) noexcept
{
    if (!isValidPixelFormat(desc.format))
    {
        return IntVector(0, 0, 0);
    }

    const PixelFormatInfo& info = getPixelFormatInfo(desc.format);
    if (info.blockBytes == 0u || info.blockSizeX == 0u || info.blockSizeY == 0u || info.blockSizeZ == 0u)
    {
        return IntVector(0, 0, 0);
    }

    const IntVector extent = calculateMipExtent(desc, mipIndex);
    const gp::Int32 blocksX =
        (extent.x + static_cast<gp::Int32>(info.blockSizeX) - 1) / static_cast<gp::Int32>(info.blockSizeX);
    const gp::Int32 blocksY =
        (extent.y + static_cast<gp::Int32>(info.blockSizeY) - 1) / static_cast<gp::Int32>(info.blockSizeY);
    const gp::Int32 blocksZ =
        (extent.z + static_cast<gp::Int32>(info.blockSizeZ) - 1) / static_cast<gp::Int32>(info.blockSizeZ);
    return IntVector(blocksX, blocksY, blocksZ);
}

/// @brief Computes tightly packed byte size for a 2D or 3D image of `(sizeX, sizeY, sizeZ)` texels in `format`.
/// @param[in] sizeX Width in texels.
/// @param[in] sizeY Height in texels.
/// @param[in] sizeZ Depth in texels (`0` is treated as `1` for 2D images).
/// @param[in] format Pixel format describing block geometry and byte stride.
/// @return Tightly packed byte size, or `0` if `format` is invalid or dimensions are zero.
[[nodiscard]] inline constexpr gp::UInt64 calculateImageBytes(
    const gp::UInt32 sizeX, const gp::UInt32 sizeY, const gp::UInt32 sizeZ, const PixelFormat format
) noexcept
{
    if (!isValidPixelFormat(format) || sizeX == 0u || sizeY == 0u)
    {
        return 0u;
    }

    const PixelFormatInfo& info = getPixelFormatInfo(format);
    if (info.blockBytes == 0u || info.blockSizeX == 0u || info.blockSizeY == 0u || info.blockSizeZ == 0u)
    {
        return 0u;
    }

    const gp::UInt64 blocksX =
        (static_cast<gp::UInt64>(sizeX) + static_cast<gp::UInt64>(info.blockSizeX) - 1u) / info.blockSizeX;
    const gp::UInt64 blocksY =
        (static_cast<gp::UInt64>(sizeY) + static_cast<gp::UInt64>(info.blockSizeY) - 1u) / info.blockSizeY;
    const gp::UInt64 depthTexels = (sizeZ > 0u) ? static_cast<gp::UInt64>(sizeZ) : 1u;
    const gp::UInt64 blocksZ = (depthTexels + static_cast<gp::UInt64>(info.blockSizeZ) - 1u) / info.blockSizeZ;

    return blocksX * blocksY * blocksZ * static_cast<gp::UInt64>(info.blockBytes);
}

/// @brief Computes the tightly packed byte size of one array slice / cube face at `mipIndex`.
/// @param[in] desc Texture descriptor.
/// @param[in] mipIndex Zero-based mip level index.
/// @return Tightly packed byte size for that mip slice, or `0` on invalid input.
[[nodiscard]] inline constexpr gp::UInt64
    calculateTextureMipSize(const TextureDesc& desc, const gp::UInt32 mipIndex) noexcept
{
    if (!isValidPixelFormat(desc.format) || desc.extent.x <= 0 || desc.extent.y <= 0 ||
        mipIndex >= static_cast<gp::UInt32>(desc.numMips))
    {
        return 0u;
    }

    const IntVector extent = calculateMipExtent(desc, mipIndex);
    return calculateImageBytes(
        static_cast<gp::UInt32>(extent.x),
        static_cast<gp::UInt32>(extent.y),
        static_cast<gp::UInt32>(extent.z),
        desc.format
    );
}

/// @brief Computes the tightly packed byte size across all mip levels of a single slice/face in `desc`.
/// @param[in] desc Texture descriptor.
/// @return Byte size of one complete mip chain.
[[nodiscard]] inline gp::UInt64 calculateTexturePlaneSize(const TextureDesc& desc) noexcept
{
    if (!isValidPixelFormat(desc.format) || desc.extent.x <= 0 || desc.extent.y <= 0 || desc.numMips == 0u)
    {
        return 0u;
    }

    gp::UInt64 perSliceBytes = 0u;
    for (gp::UInt32 mip = 0u; mip < static_cast<gp::UInt32>(desc.numMips); ++mip)
    {
        perSliceBytes += calculateTextureMipSize(desc, mip);
    }
    return perSliceBytes;
}

/// @brief Computes total tightly packed byte size across all mips, array slices, and cube faces of `desc`.
/// @param[in] desc Texture descriptor describing dimensions, format, mip count, and array slices.
/// @return Total tightly packed byte size across all subresources.
[[nodiscard]] inline gp::UInt64 calculateTextureSize(const TextureDesc& desc) noexcept
{
    const gp::UInt64 perSliceBytes = calculateTexturePlaneSize(desc);
    if (perSliceBytes == 0u)
    {
        return 0u;
    }

    const gp::UInt64 sliceCount =
        static_cast<gp::UInt64>(desc.arraySize) * (desc.isTextureCube() ? static_cast<gp::UInt64>(kCubeFaceCount) : 1u);
    return perSliceBytes * sliceCount;
}

/// @brief Computes linear byte offset, row pitch, and subresource size for `(faceIndex, arrayIndex, mipIndex)`.
/// @param[in] desc Texture descriptor.
/// @param[in] faceIndex Zero-based cubemap face index (`0` for non-cube textures).
/// @param[in] arrayIndex Zero-based array slice index.
/// @param[in] mipIndex Zero-based mip level index.
/// @param[out] outRowStride Receives the row pitch in bytes for the target subresource.
/// @param[out] outSubresourceSize Receives the total byte size of the target subresource.
/// @return Byte offset from the start of a tightly packed full-texture buffer.
[[nodiscard]] inline gp::UInt64 calculateSubresourceOffset(
    const TextureDesc& desc,
    const gp::UInt32 faceIndex,
    const gp::UInt32 arrayIndex,
    const gp::UInt32 mipIndex,
    gp::UInt64& outRowStride,
    gp::UInt64& outSubresourceSize
) noexcept
{
    outRowStride = 0u;
    outSubresourceSize = 0u;

    const gp::UInt32 numFaces = desc.isTextureCube() ? static_cast<gp::UInt32>(kCubeFaceCount) : 1u;
    const gp::UInt32 numArrays = (desc.arraySize > 0u) ? static_cast<gp::UInt32>(desc.arraySize) : 1u;
    if (!isValidPixelFormat(desc.format) || desc.extent.x <= 0 || desc.extent.y <= 0 ||
        mipIndex >= static_cast<gp::UInt32>(desc.numMips) || faceIndex >= numFaces || arrayIndex >= numArrays)
    {
        return 0u;
    }

    const PixelFormatInfo& info = getPixelFormatInfo(desc.format);
    if (info.blockBytes == 0u || info.blockSizeX == 0u || info.blockSizeY == 0u || info.blockSizeZ == 0u)
    {
        return 0u;
    }

    gp::UInt64 perSliceBytes = 0u;
    gp::UInt64 offsetInSlice = 0u;

    for (gp::UInt32 mip = 0u; mip < static_cast<gp::UInt32>(desc.numMips); ++mip)
    {
        const IntVector extent = calculateMipExtent(desc, mip);
        const gp::UInt64 blocksX =
            (static_cast<gp::UInt64>(extent.x) + static_cast<gp::UInt64>(info.blockSizeX) - 1u) / info.blockSizeX;
        const gp::UInt64 mipSize = calculateImageBytes(
            static_cast<gp::UInt32>(extent.x),
            static_cast<gp::UInt32>(extent.y),
            static_cast<gp::UInt32>(extent.z),
            desc.format
        );

        if (mip < mipIndex)
        {
            offsetInSlice += mipSize;
        }
        else if (mip == mipIndex)
        {
            outRowStride = blocksX * static_cast<gp::UInt64>(info.blockBytes);
            outSubresourceSize = mipSize;
        }

        perSliceBytes += mipSize;
    }

    const gp::UInt64 linearSlice =
        (static_cast<gp::UInt64>(arrayIndex) * numFaces) + static_cast<gp::UInt64>(faceIndex);
    return (linearSlice * perSliceBytes) + offsetInSlice;
}

/// @brief Convenience overload computing subresource offset for `(mipIndex, arrayIndex)` with `faceIndex = 0`.
/// @param[in] desc Texture descriptor.
/// @param[in] mipIndex Zero-based mip level index.
/// @param[in] arrayIndex Zero-based array slice (or cube face index for non-array cubemaps).
/// @param[out] outRowStride Receives the row pitch in bytes for the target subresource.
/// @param[out] outSubresourceSize Receives the total byte size of the target subresource.
/// @return Byte offset from the start of a tightly packed full-texture buffer.
[[nodiscard]] inline gp::UInt64 calculateSubresourceOffset(
    const TextureDesc& desc,
    const gp::UInt32 mipIndex,
    const gp::UInt32 arrayIndex,
    gp::UInt64& outRowStride,
    gp::UInt64& outSubresourceSize
) noexcept
{
    if (desc.isTextureCube() && desc.arraySize == 1u)
    {
        return calculateSubresourceOffset(desc, arrayIndex, 0u, mipIndex, outRowStride, outSubresourceSize);
    }
    return calculateSubresourceOffset(desc, 0u, arrayIndex, mipIndex, outRowStride, outSubresourceSize);
}

}   // namespace gp::rhi::resource
