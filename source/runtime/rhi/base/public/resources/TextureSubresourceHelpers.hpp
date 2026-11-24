// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/Memory.hpp"
#include "resources/TextureCreateDesc.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "resources/TextureInitializer.hpp"
#include "RHIPixelFormat.hpp"

namespace gp::rhi::resource
{

/// @brief Flattens `(faceIndex, arrayIndex)` into a linear hardware array layer index for `desc`.
/// @param[in] desc Texture descriptor describing dimensionality.
/// @param[in] faceIndex Zero-based cubemap face index (`0..5` for cube textures).
/// @param[in] arrayIndex Zero-based array slice index.
/// @return `faceIndex + arrayIndex * 6` for cubemaps, `arrayIndex` for 2D arrays, or `0` for single surfaces.
[[nodiscard]] inline constexpr gp::UInt32
    getCombinedArrayIndex(const TextureDesc& desc, const gp::UInt32 faceIndex, const gp::UInt32 arrayIndex) noexcept
{
    if (desc.isTextureCube())
    {
        return faceIndex + (arrayIndex * kCubeFaceCount);
    }

    if (desc.isTextureArray())
    {
        return arrayIndex;
    }

    return 0u;
}

/// @brief Computes the combined hardware array layer index referenced by a `TextureLockArgs` request.
/// @param[in] desc Texture descriptor.
/// @param[in] arguments Texture lock coordinates carrying `faceIndex` and `arrayIndex`.
/// @return Combined linear array layer index.
[[nodiscard]] inline constexpr gp::UInt32
    getLockArrayIndex(const TextureDesc& desc, const TextureLockArgs& arguments) noexcept
{
    return getCombinedArrayIndex(desc, static_cast<gp::UInt32>(arguments.faceIndex), arguments.arrayIndex);
}

/// @brief Counts the number of addressable subresources in `desc` starting at `firstMipIndex`.
/// @param[in] desc Texture descriptor.
/// @param[in] firstMipIndex First resident mip level to include (`0` counts the entire mip chain).
/// @return `arraySize * faceCount * remainingMipCount`, or `0` when `firstMipIndex >= desc.numMips`.
[[nodiscard]] inline constexpr gp::UInt32
    getSubresourceCount(const TextureDesc& desc, const gp::Int32 firstMipIndex = 0) noexcept
{
    const gp::Int32 safeFirstMip = gp::math::max<gp::Int32>(firstMipIndex, 0);
    const gp::UInt32 faceCount = desc.isTextureCube() ? kCubeFaceCount : 1u;
    const gp::Int32 numMips = static_cast<gp::Int32>(desc.numMips);
    const gp::UInt32 mipCount = (safeFirstMip < numMips) ? static_cast<gp::UInt32>(numMips - safeFirstMip) : 0u;

    return static_cast<gp::UInt32>(desc.arraySize) * faceCount * mipCount;
}

/// @brief Invokes `callback(subresourceIndex, combinedArrayIndex)` for every subresource of `desc` from
///        `firstMipIndex` onward.
/// @details A partially resident texture only commits memory starting at its first resident mip level. Iteration order
///          is outer array slice, middle cube face, inner mip level.
/// @tparam InCallbackType Callable invocable with `(TextureSubresourceIndex, gp::UInt32)`.
/// @param[in] desc Texture descriptor describing dimensions, array count, and mip chain.
/// @param[in] firstMipIndex First mip index to visit.
/// @param[in,out] callback Visitor invoked once per subresource.
template <typename InCallbackType>
inline void
    enumerateSubresources(const TextureDesc& desc, const gp::Int32 firstMipIndex, InCallbackType&& callback) noexcept
{
    const gp::Int32 safeFirstMip = gp::math::max<gp::Int32>(firstMipIndex, 0);
    const gp::Int32 faceCount = desc.isTextureCube() ? static_cast<gp::Int32>(kCubeFaceCount) : 1;
    const gp::Int32 arraySize = static_cast<gp::Int32>(desc.arraySize);
    const gp::Int32 numMips = static_cast<gp::Int32>(desc.numMips);

    for (gp::Int32 arrayIndex = 0; arrayIndex < arraySize; ++arrayIndex)
    {
        for (gp::Int32 faceIndex = 0; faceIndex < faceCount; ++faceIndex)
        {
            for (gp::Int32 mipIndex = safeFirstMip; mipIndex < numMips; ++mipIndex)
            {
                TextureSubresourceIndex subresourceIndex{};
                subresourceIndex.mipIndex = mipIndex;
                subresourceIndex.arrayIndex = arrayIndex;
                subresourceIndex.faceIndex = faceIndex;

                callback(
                    subresourceIndex,
                    getCombinedArrayIndex(desc, static_cast<gp::UInt32>(faceIndex), static_cast<gp::UInt32>(arrayIndex))
                );
            }
        }
    }
}

/// @brief Copies a 2D texture surface row-by-row between buffers with potentially different row pitches.
/// @details Divides `sizeY` by the format's `blockSizeY` to handle both uncompressed and block-compressed textures.
/// @param[in] source Source surface bytes.
/// @param[out] dest Destination surface bytes.
/// @param[in] sizeY Surface height in texels.
/// @param[in] format Pixel format governing block height.
/// @param[in] sourceStride Row pitch in bytes of `source`.
/// @param[in] destStride Row pitch in bytes of `dest`.
inline void copyTextureData2D(
    const void* const source,
    void* const dest,
    const gp::UInt32 sizeY,
    const PixelFormat format,
    const gp::UInt32 sourceStride,
    const gp::UInt32 destStride
) noexcept
{
    if (source == nullptr || dest == nullptr || sizeY == 0u || !isValidPixelFormat(format))
    {
        return;
    }

    const PixelFormatInfo& info = getPixelFormatInfo(format);
    if (info.blockSizeY == 0u)
    {
        return;
    }

    const gp::UInt64 blockSizeY = static_cast<gp::UInt64>(info.blockSizeY);
    const gp::UInt64 numBlocksY = (static_cast<gp::UInt64>(sizeY) + blockSizeY - 1u) / blockSizeY;

    if (sourceStride == destStride || sourceStride == 0u || destStride == 0u)
    {
        const gp::UInt64 pitch =
            (sourceStride != 0u) ? static_cast<gp::UInt64>(sourceStride) : static_cast<gp::UInt64>(destStride);
        gp::memory::copyMemory(dest, source, static_cast<gp::USize>(numBlocksY * pitch));
        return;
    }

    const gp::USize rowCopyBytes = static_cast<gp::USize>(gp::math::min(sourceStride, destStride));
    const gp::UInt8* srcRow = static_cast<const gp::UInt8*>(source);
    gp::UInt8* dstRow = static_cast<gp::UInt8*>(dest);

    for (gp::UInt64 row = 0u; row < numBlocksY; ++row)
    {
        gp::memory::copyMemory(dstRow, srcRow, rowCopyBytes);
        srcRow += sourceStride;
        dstRow += destStride;
    }
}

/// @brief Copies one subresource from a `TextureDataProvider` into `dest`, repitching row-by-row when pitches differ.
/// @details Uses a single flat `copyMemory()` when either pitch is zero or both pitches match. Otherwise loops over
///          volume slices and copies bounded block rows via `copyTextureData2D()` so neither source nor destination is
///          ever read or written out of bounds.
/// @param[in] source Source data pointer, byte size, and row pitch from the provider.
/// @param[out] dest Destination subresource buffer.
/// @param[in] destSize Capacity in bytes of `dest`.
/// @param[in] destStride Destination row pitch in bytes.
/// @param[in] destRows Subresource height in texels (not block rows).
/// @param[in] destDepth Volume depth slice count (`1` for 2D/array/cube textures).
/// @param[in] format Pixel format governing block height.
inline void copyDataProviderSubresource(
    const TextureDataProvider::SubresourceData& source,
    void* const dest,
    const gp::UInt64 destSize,
    const gp::UInt32 destStride,
    const gp::UInt32 destRows,
    const gp::UInt32 destDepth,
    const PixelFormat format
) noexcept
{
    if (source.data == nullptr || dest == nullptr || source.size == 0u || destSize == 0u)
    {
        return;
    }

    if (source.rowPitch == 0u || destStride == 0u || destStride == source.rowPitch)
    {
        gp::memory::copyMemory(
            dest, source.data, static_cast<gp::USize>(gp::math::min<gp::UInt64>(source.size, destSize))
        );
        return;
    }

    const gp::UInt32 blockSizeY = static_cast<gp::UInt32>(getPixelFormatInfo(format).blockSizeY);
    if (blockSizeY == 0u)
    {
        return;
    }

    const gp::UInt64 blockRows = (static_cast<gp::UInt64>(destRows) + blockSizeY - 1u) / blockSizeY;
    const gp::UInt64 srcSlicePitch = static_cast<gp::UInt64>(source.rowPitch) * blockRows;
    const gp::UInt64 destSlicePitch = static_cast<gp::UInt64>(destStride) * blockRows;
    const gp::UInt32 sliceCount = gp::math::max<gp::UInt32>(destDepth, 1u);

    for (gp::UInt32 slice = 0u; slice < sliceCount; ++slice)
    {
        const gp::UInt64 srcOffset = static_cast<gp::UInt64>(slice) * srcSlicePitch;
        const gp::UInt64 destOffset = static_cast<gp::UInt64>(slice) * destSlicePitch;
        if (srcOffset >= source.size || destOffset >= destSize)
        {
            break;
        }

        const gp::UInt64 safeBlockRows = gp::math::min<gp::UInt64>(
            blockRows,
            gp::math::min<gp::UInt64>((source.size - srcOffset) / source.rowPitch, (destSize - destOffset) / destStride)
        );
        if (safeBlockRows == 0u)
        {
            break;
        }

        copyTextureData2D(
            static_cast<const gp::UInt8*>(source.data) + srcOffset,
            static_cast<gp::UInt8*>(dest) + destOffset,
            static_cast<gp::UInt32>(safeBlockRows * blockSizeY),
            format,
            source.rowPitch,
            destStride
        );
    }
}

}   // namespace gp::rhi::resource
