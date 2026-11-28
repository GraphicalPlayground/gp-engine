// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/PixelFormatEnums.hpp"

namespace gp::math
{

namespace detail
{

/// @brief Computes the clamped texel dimension at a given mip level.
/// @param[in] baseDimension Base mip-0 dimension in texels.
/// @param[in] mipIndex Zero-based mip level index.
/// @return Dimension at `mipIndex` clamped to at least `1`, or `0` if `baseDimension` is `0`.
[[nodiscard]] inline constexpr gp::UInt32
    computeMipDimension(const gp::UInt32 baseDimension, const gp::UInt32 mipIndex) noexcept
{
    if (baseDimension == 0u)
    {
        return 0u;
    }

    if (mipIndex >= 32u)
    {
        return 1u;
    }

    const gp::UInt32 shifted = baseDimension >> mipIndex;
    return shifted > 0u ? shifted : 1u;
}

}   // namespace detail

/// @brief Immutable compile-time metadata describing the encoded memory layout of a `PixelFormat`.
/// @details Unlike Unreal Engine's mutable global `GPixelFormats[]` table, `PixelFormatInfo` stores strictly
///          static, API-agnostic mathematical properties (block geometry, byte stride, channel masks, numeric
///          encoding, and compression family). Runtime GPU capabilities and native API format handles live
///          separately in the RHI layer.
struct PixelFormatInfo final
{
public:
    /// @brief Canonical ASCII name matching the `PixelFormat` enumerator (e.g., `"R8G8B8A8_UNorm"`).
    gp::StringView name{};

    /// @brief Format enumerator described by this entry.
    PixelFormat format{ PixelFormat::Unknown };

    /// @brief Width of a single compression/storage block in texels (`1` for uncompressed formats).
    gp::UInt8 blockSizeX{ 0u };

    /// @brief Height of a single compression/storage block in texels (`1` for uncompressed formats).
    gp::UInt8 blockSizeY{ 0u };

    /// @brief Depth of a single compression/storage block in texels (`1` for uncompressed and 2D BC/ETC2/ASTC).
    gp::UInt8 blockSizeZ{ 0u };

    /// @brief Byte size of one encoded storage block (`bytesPerPixel` for uncompressed formats).
    gp::UInt8 blockBytes{ 0u };

    /// @brief Number of logical scalar components encoded per texel.
    gp::UInt8 numComponents{ 0u };

    /// @brief Bitmask of active channels present in this format.
    PixelFormatChannelFlags channels{ PixelFormatChannelFlags::None };

    /// @brief Numeric interpretation of channel values.
    PixelFormatComponentType componentType{ PixelFormatComponentType::None };

    /// @brief Hardware block-compression family, or `PixelFormatCompression::None`.
    PixelFormatCompression compression{ PixelFormatCompression::None };

    /// @brief True for high-dynamic-range color formats (floating-point, shared-exponent, BC6H, ASTC HDR).
    bool isHDR{ false };

    /// @brief True when hardware sRGB gamma views/decode are valid for this format.
    bool supportsSRGB{ false };

public:
    /// @brief Checks whether this descriptor represents a usable, non-unknown format.
    /// @return True when `format != PixelFormat::Unknown` and block dimensions/bytes are non-zero.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return format != PixelFormat::Unknown && blockSizeX > 0u && blockSizeY > 0u && blockSizeZ > 0u &&
               blockBytes > 0u;
    }

    /// @brief Computes the number of encoded blocks required to cover `width` texels (ceiling division).
    /// @param[in] width Image width in texels.
    /// @return Block count along X, or `0` if `width` or `blockSizeX` is zero.
    [[nodiscard]] constexpr gp::UInt64 getBlockCountForWidth(const gp::UInt32 width) const noexcept
    {
        if (width == 0u || blockSizeX == 0u)
        {
            return 0ull;
        }

        const gp::UInt64 blockX = static_cast<gp::UInt64>(blockSizeX);
        return (static_cast<gp::UInt64>(width) + blockX - 1ull) / blockX;
    }

    /// @brief Computes the number of encoded blocks required to cover `height` texels (ceiling division).
    /// @param[in] height Image height in texels.
    /// @return Block count along Y, or `0` if `height` or `blockSizeY` is zero.
    [[nodiscard]] constexpr gp::UInt64 getBlockCountForHeight(const gp::UInt32 height) const noexcept
    {
        if (height == 0u || blockSizeY == 0u)
        {
            return 0ull;
        }

        const gp::UInt64 blockY = static_cast<gp::UInt64>(blockSizeY);
        return (static_cast<gp::UInt64>(height) + blockY - 1ull) / blockY;
    }

    /// @brief Computes the number of encoded blocks required to cover `depth` texels (ceiling division).
    /// @param[in] depth Image depth in texels.
    /// @return Block count along Z, or `0` if `depth` or `blockSizeZ` is zero.
    [[nodiscard]] constexpr gp::UInt64 getBlockCountForDepth(const gp::UInt32 depth) const noexcept
    {
        if (depth == 0u || blockSizeZ == 0u)
        {
            return 0ull;
        }

        const gp::UInt64 blockZ = static_cast<gp::UInt64>(blockSizeZ);
        return (static_cast<gp::UInt64>(depth) + blockZ - 1ull) / blockZ;
    }

    /// @brief Computes the tightly packed byte size of a single 2D image surface.
    /// @details Does not include GPU row-pitch alignment or console tiled/packed mip-tail padding.
    /// @param[in] width Surface width in texels.
    /// @param[in] height Surface height in texels.
    /// @return Encoded surface storage size in bytes.
    [[nodiscard]] constexpr gp::UInt64
        get2DImageSizeInBytes(const gp::UInt32 width, const gp::UInt32 height) const noexcept
    {
        return getBlockCountForWidth(width) * getBlockCountForHeight(height) * static_cast<gp::UInt64>(blockBytes);
    }

    /// @brief Computes the tightly packed byte size of a specific mip level of a 2D texture.
    /// @param[in] textureWidth Mip-0 width in texels.
    /// @param[in] textureHeight Mip-0 height in texels.
    /// @param[in] mipIndex Zero-based mip level index.
    /// @return Encoded mip level size in bytes, or `0` if base dimensions are zero.
    [[nodiscard]] constexpr gp::UInt64 get2DTextureMipSizeInBytes(
        const gp::UInt32 textureWidth, const gp::UInt32 textureHeight, const gp::UInt32 mipIndex
    ) const noexcept
    {
        if (textureWidth == 0u || textureHeight == 0u)
        {
            return 0ull;
        }

        const gp::UInt32 mipWidth = detail::computeMipDimension(textureWidth, mipIndex);
        const gp::UInt32 mipHeight = detail::computeMipDimension(textureHeight, mipIndex);
        return get2DImageSizeInBytes(mipWidth, mipHeight);
    }

    /// @brief Computes the total tightly packed byte size of a 2D texture across `mipCount` mip levels.
    /// @param[in] textureWidth Mip-0 width in texels.
    /// @param[in] textureHeight Mip-0 height in texels.
    /// @param[in] mipCount Number of mip levels starting from mip 0.
    /// @return Cumulative encoded size across all `mipCount` levels in bytes.
    [[nodiscard]] constexpr gp::UInt64 get2DTextureSizeInBytes(
        const gp::UInt32 textureWidth, const gp::UInt32 textureHeight, const gp::UInt32 mipCount
    ) const noexcept
    {
        if (textureWidth == 0u || textureHeight == 0u || mipCount == 0u)
        {
            return 0ull;
        }

        gp::UInt64 totalBytes = 0ull;
        for (gp::UInt32 mipIndex = 0u; mipIndex < mipCount; ++mipIndex)
        {
            totalBytes += get2DTextureMipSizeInBytes(textureWidth, textureHeight, mipIndex);
        }

        return totalBytes;
    }

    /// @brief Computes the tightly packed byte size of a single 3D volume image.
    /// @details Does not include GPU row-pitch or slice-pitch alignment padding.
    /// @param[in] width Volume width in texels.
    /// @param[in] height Volume height in texels.
    /// @param[in] depth Volume depth in texels.
    /// @return Encoded volume storage size in bytes.
    [[nodiscard]] constexpr gp::UInt64
        get3DImageSizeInBytes(const gp::UInt32 width, const gp::UInt32 height, const gp::UInt32 depth) const noexcept
    {
        return getBlockCountForWidth(width) * getBlockCountForHeight(height) * getBlockCountForDepth(depth) *
               static_cast<gp::UInt64>(blockBytes);
    }

    /// @brief Computes the tightly packed byte size of a specific mip level of a 3D volume texture.
    /// @param[in] textureWidth Mip-0 width in texels.
    /// @param[in] textureHeight Mip-0 height in texels.
    /// @param[in] textureDepth Mip-0 depth in texels.
    /// @param[in] mipIndex Zero-based mip level index.
    /// @return Encoded mip level size in bytes, or `0` if any base dimension is zero.
    [[nodiscard]] constexpr gp::UInt64 get3DTextureMipSizeInBytes(
        const gp::UInt32 textureWidth,
        const gp::UInt32 textureHeight,
        const gp::UInt32 textureDepth,
        const gp::UInt32 mipIndex
    ) const noexcept
    {
        if (textureWidth == 0u || textureHeight == 0u || textureDepth == 0u)
        {
            return 0ull;
        }

        const gp::UInt32 mipWidth = detail::computeMipDimension(textureWidth, mipIndex);
        const gp::UInt32 mipHeight = detail::computeMipDimension(textureHeight, mipIndex);
        const gp::UInt32 mipDepth = detail::computeMipDimension(textureDepth, mipIndex);
        return get3DImageSizeInBytes(mipWidth, mipHeight, mipDepth);
    }

    /// @brief Computes the total tightly packed byte size of a 3D texture across `mipCount` mip levels.
    /// @param[in] textureWidth Mip-0 width in texels.
    /// @param[in] textureHeight Mip-0 height in texels.
    /// @param[in] textureDepth Mip-0 depth in texels.
    /// @param[in] mipCount Number of mip levels starting from mip 0.
    /// @return Cumulative encoded size across all `mipCount` levels in bytes.
    [[nodiscard]] constexpr gp::UInt64 get3DTextureSizeInBytes(
        const gp::UInt32 textureWidth,
        const gp::UInt32 textureHeight,
        const gp::UInt32 textureDepth,
        const gp::UInt32 mipCount
    ) const noexcept
    {
        if (textureWidth == 0u || textureHeight == 0u || textureDepth == 0u || mipCount == 0u)
        {
            return 0ull;
        }

        gp::UInt64 totalBytes = 0ull;
        for (gp::UInt32 mipIndex = 0u; mipIndex < mipCount; ++mipIndex)
        {
            totalBytes += get3DTextureMipSizeInBytes(textureWidth, textureHeight, textureDepth, mipIndex);
        }

        return totalBytes;
    }
};

// clang-format off
/// @brief Immutable compile-time lookup table mapping every `PixelFormat` enumerator to its `PixelFormatInfo`.
inline constexpr gp::Array<PixelFormatInfo, kPixelFormatCount> kPixelFormatTable{
    //                Name                   Format                              BX  BY  BZ  BB  Comp Channels                               ComponentType                             Compression                     HDR    sRGB
    PixelFormatInfo{ "Unknown",              PixelFormat::Unknown,               0u, 0u, 0u,  0u, 0u, PixelFormatChannelFlags::None,         PixelFormatComponentType::None,         PixelFormatCompression::None, false, false },

    // 8-bit uncompressed
    PixelFormatInfo{ "R8_UNorm",             PixelFormat::R8_UNorm,              1u, 1u, 1u,  1u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R8_SNorm",             PixelFormat::R8_SNorm,              1u, 1u, 1u,  1u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::SNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R8_UInt",              PixelFormat::R8_UInt,               1u, 1u, 1u,  1u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R8_SInt",              PixelFormat::R8_SInt,               1u, 1u, 1u,  1u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::SInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "A8_UNorm",             PixelFormat::A8_UNorm,              1u, 1u, 1u,  1u, 1u, PixelFormatChannelFlags::A,            PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },

    PixelFormatInfo{ "R8G8_UNorm",           PixelFormat::R8G8_UNorm,            1u, 1u, 1u,  2u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R8G8_SNorm",           PixelFormat::R8G8_SNorm,            1u, 1u, 1u,  2u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::SNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R8G8_UInt",            PixelFormat::R8G8_UInt,             1u, 1u, 1u,  2u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R8G8_SInt",            PixelFormat::R8G8_SInt,             1u, 1u, 1u,  2u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::SInt,         PixelFormatCompression::None, false, false },

    PixelFormatInfo{ "R8G8B8_UNorm",         PixelFormat::R8G8B8_UNorm,          1u, 1u, 1u,  3u, 3u, PixelFormatChannelFlags::RGB,          PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },

    PixelFormatInfo{ "R8G8B8A8_UNorm",       PixelFormat::R8G8B8A8_UNorm,        1u, 1u, 1u,  4u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, true  },
    PixelFormatInfo{ "R8G8B8A8_SNorm",       PixelFormat::R8G8B8A8_SNorm,        1u, 1u, 1u,  4u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::SNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R8G8B8A8_UInt",        PixelFormat::R8G8B8A8_UInt,         1u, 1u, 1u,  4u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R8G8B8A8_SInt",        PixelFormat::R8G8B8A8_SInt,         1u, 1u, 1u,  4u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::SInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "B8G8R8A8_UNorm",       PixelFormat::B8G8R8A8_UNorm,        1u, 1u, 1u,  4u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, true  },

    // 16-bit uncompressed
    PixelFormatInfo{ "R16_UNorm",            PixelFormat::R16_UNorm,             1u, 1u, 1u,  2u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16_SNorm",            PixelFormat::R16_SNorm,             1u, 1u, 1u,  2u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::SNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16_UInt",             PixelFormat::R16_UInt,              1u, 1u, 1u,  2u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16_SInt",             PixelFormat::R16_SInt,              1u, 1u, 1u,  2u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::SInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16_Float",            PixelFormat::R16_Float,             1u, 1u, 1u,  2u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::Float,        PixelFormatCompression::None, true,  false },

    PixelFormatInfo{ "R16G16_UNorm",         PixelFormat::R16G16_UNorm,          1u, 1u, 1u,  4u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16G16_SNorm",         PixelFormat::R16G16_SNorm,          1u, 1u, 1u,  4u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::SNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16G16_UInt",          PixelFormat::R16G16_UInt,           1u, 1u, 1u,  4u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16G16_SInt",          PixelFormat::R16G16_SInt,           1u, 1u, 1u,  4u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::SInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16G16_Float",         PixelFormat::R16G16_Float,          1u, 1u, 1u,  4u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::Float,        PixelFormatCompression::None, true,  false },

    PixelFormatInfo{ "R16G16B16A16_UNorm",   PixelFormat::R16G16B16A16_UNorm,    1u, 1u, 1u,  8u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16G16B16A16_SNorm",   PixelFormat::R16G16B16A16_SNorm,    1u, 1u, 1u,  8u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::SNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16G16B16A16_UInt",    PixelFormat::R16G16B16A16_UInt,     1u, 1u, 1u,  8u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16G16B16A16_SInt",    PixelFormat::R16G16B16A16_SInt,     1u, 1u, 1u,  8u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::SInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R16G16B16A16_Float",   PixelFormat::R16G16B16A16_Float,    1u, 1u, 1u,  8u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::Float,        PixelFormatCompression::None, true,  false },

    // 32-bit and 64-bit uncompressed
    PixelFormatInfo{ "R32_UInt",             PixelFormat::R32_UInt,              1u, 1u, 1u,  4u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R32_SInt",             PixelFormat::R32_SInt,              1u, 1u, 1u,  4u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::SInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R32_Float",            PixelFormat::R32_Float,             1u, 1u, 1u,  4u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::Float,        PixelFormatCompression::None, true,  false },

    PixelFormatInfo{ "R32G32_UInt",          PixelFormat::R32G32_UInt,           1u, 1u, 1u,  8u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R32G32_SInt",          PixelFormat::R32G32_SInt,           1u, 1u, 1u,  8u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::SInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R32G32_Float",         PixelFormat::R32G32_Float,          1u, 1u, 1u,  8u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::Float,        PixelFormatCompression::None, true,  false },

    PixelFormatInfo{ "R32G32B32_UInt",       PixelFormat::R32G32B32_UInt,        1u, 1u, 1u, 12u, 3u, PixelFormatChannelFlags::RGB,          PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R32G32B32_SInt",       PixelFormat::R32G32B32_SInt,        1u, 1u, 1u, 12u, 3u, PixelFormatChannelFlags::RGB,          PixelFormatComponentType::SInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R32G32B32_Float",      PixelFormat::R32G32B32_Float,       1u, 1u, 1u, 12u, 3u, PixelFormatChannelFlags::RGB,          PixelFormatComponentType::Float,        PixelFormatCompression::None, true,  false },

    PixelFormatInfo{ "R32G32B32A32_UInt",    PixelFormat::R32G32B32A32_UInt,     1u, 1u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R32G32B32A32_SInt",    PixelFormat::R32G32B32A32_SInt,     1u, 1u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::SInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R32G32B32A32_Float",   PixelFormat::R32G32B32A32_Float,    1u, 1u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::Float,        PixelFormatCompression::None, true,  false },

    PixelFormatInfo{ "R64_UInt",             PixelFormat::R64_UInt,              1u, 1u, 1u,  8u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },

    // Packed and HDR special color formats
    PixelFormatInfo{ "B5G6R5_UNorm",         PixelFormat::B5G6R5_UNorm,          1u, 1u, 1u,  2u, 3u, PixelFormatChannelFlags::RGB,          PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "B5G5R5A1_UNorm",       PixelFormat::B5G5R5A1_UNorm,        1u, 1u, 1u,  2u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R10G10B10A2_UNorm",    PixelFormat::R10G10B10A2_UNorm,     1u, 1u, 1u,  4u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "B10G10R10A2_UNorm",    PixelFormat::B10G10R10A2_UNorm,     1u, 1u, 1u,  4u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "R11G11B10_Float",      PixelFormat::R11G11B10_Float,       1u, 1u, 1u,  4u, 3u, PixelFormatChannelFlags::RGB,          PixelFormatComponentType::UFloat,       PixelFormatCompression::None, true,  false },
    PixelFormatInfo{ "R9G9B9E5_SharedExp",   PixelFormat::R9G9B9E5_SharedExp,    1u, 1u, 1u,  4u, 3u, PixelFormatChannelFlags::RGB,          PixelFormatComponentType::SharedExp,    PixelFormatCompression::None, true,  false },

    // Depth and stencil formats
    PixelFormatInfo{ "D16_UNorm",            PixelFormat::D16_UNorm,             1u, 1u, 1u,  2u, 1u, PixelFormatChannelFlags::Depth,        PixelFormatComponentType::UNorm,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "D24_UNorm_S8_UInt",    PixelFormat::D24_UNorm_S8_UInt,     1u, 1u, 1u,  4u, 2u, PixelFormatChannelFlags::DepthStencil, PixelFormatComponentType::DepthStencil, PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "D32_Float",            PixelFormat::D32_Float,             1u, 1u, 1u,  4u, 1u, PixelFormatChannelFlags::Depth,        PixelFormatComponentType::Float,        PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "D32_Float_S8_UInt",    PixelFormat::D32_Float_S8_UInt,     1u, 1u, 1u,  8u, 2u, PixelFormatChannelFlags::DepthStencil, PixelFormatComponentType::DepthStencil, PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "X24_G8_UInt",          PixelFormat::X24_G8_UInt,           1u, 1u, 1u,  4u, 1u, PixelFormatChannelFlags::Stencil,      PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },
    PixelFormatInfo{ "X32_G8_UInt",          PixelFormat::X32_G8_UInt,           1u, 1u, 1u,  8u, 1u, PixelFormatChannelFlags::Stencil,      PixelFormatComponentType::UInt,         PixelFormatCompression::None, false, false },

    // BCn / DXTC block-compressed formats
    PixelFormatInfo{ "BC1_UNorm",            PixelFormat::BC1_UNorm,             4u, 4u, 1u,  8u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::BC,   false, true  },
    PixelFormatInfo{ "BC2_UNorm",            PixelFormat::BC2_UNorm,             4u, 4u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::BC,   false, true  },
    PixelFormatInfo{ "BC3_UNorm",            PixelFormat::BC3_UNorm,             4u, 4u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::BC,   false, true  },
    PixelFormatInfo{ "BC4_UNorm",            PixelFormat::BC4_UNorm,             4u, 4u, 1u,  8u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::UNorm,        PixelFormatCompression::BC,   false, false },
    PixelFormatInfo{ "BC5_UNorm",            PixelFormat::BC5_UNorm,             4u, 4u, 1u, 16u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::UNorm,        PixelFormatCompression::BC,   false, false },
    PixelFormatInfo{ "BC6H_UFloat",          PixelFormat::BC6H_UFloat,           4u, 4u, 1u, 16u, 3u, PixelFormatChannelFlags::RGB,          PixelFormatComponentType::UFloat,       PixelFormatCompression::BC,   true,  false },
    PixelFormatInfo{ "BC6H_SFloat",          PixelFormat::BC6H_SFloat,           4u, 4u, 1u, 16u, 3u, PixelFormatChannelFlags::RGB,          PixelFormatComponentType::Float,        PixelFormatCompression::BC,   true,  false },
    PixelFormatInfo{ "BC7_UNorm",            PixelFormat::BC7_UNorm,             4u, 4u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::BC,   false, true  },

    // ETC2 / EAC block-compressed formats
    PixelFormatInfo{ "ETC2_RGB_UNorm",       PixelFormat::ETC2_RGB_UNorm,        4u, 4u, 1u,  8u, 3u, PixelFormatChannelFlags::RGB,          PixelFormatComponentType::UNorm,        PixelFormatCompression::ETC2, false, true  },
    PixelFormatInfo{ "ETC2_RGBA_UNorm",      PixelFormat::ETC2_RGBA_UNorm,       4u, 4u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,         PixelFormatComponentType::UNorm,        PixelFormatCompression::ETC2, false, true  },
    PixelFormatInfo{ "ETC2_R11_EAC",         PixelFormat::ETC2_R11_EAC,          4u, 4u, 1u,  8u, 1u, PixelFormatChannelFlags::R,            PixelFormatComponentType::UNorm,        PixelFormatCompression::ETC2, false, false },
    PixelFormatInfo{ "ETC2_RG11_EAC",        PixelFormat::ETC2_RG11_EAC,         4u, 4u, 1u, 16u, 2u, PixelFormatChannelFlags::RG,           PixelFormatComponentType::UNorm,        PixelFormatCompression::ETC2, false, false },

    // ASTC block-compressed formats (LDR + HDR)
    PixelFormatInfo{ "ASTC_4x4_UNorm",       PixelFormat::ASTC_4x4_UNorm,        4u,  4u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,        PixelFormatComponentType::UNorm,        PixelFormatCompression::ASTC, false, true  },
    PixelFormatInfo{ "ASTC_6x6_UNorm",       PixelFormat::ASTC_6x6_UNorm,        6u,  6u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,        PixelFormatComponentType::UNorm,        PixelFormatCompression::ASTC, false, true  },
    PixelFormatInfo{ "ASTC_8x8_UNorm",       PixelFormat::ASTC_8x8_UNorm,        8u,  8u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,        PixelFormatComponentType::UNorm,        PixelFormatCompression::ASTC, false, true  },
    PixelFormatInfo{ "ASTC_10x10_UNorm",     PixelFormat::ASTC_10x10_UNorm,     10u, 10u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,        PixelFormatComponentType::UNorm,        PixelFormatCompression::ASTC, false, true  },
    PixelFormatInfo{ "ASTC_12x12_UNorm",     PixelFormat::ASTC_12x12_UNorm,     12u, 12u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,        PixelFormatComponentType::UNorm,        PixelFormatCompression::ASTC, false, true  },
    PixelFormatInfo{ "ASTC_4x4_Float",       PixelFormat::ASTC_4x4_Float,        4u,  4u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,        PixelFormatComponentType::Float,        PixelFormatCompression::ASTC, true,  false },
    PixelFormatInfo{ "ASTC_6x6_Float",       PixelFormat::ASTC_6x6_Float,        6u,  6u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,        PixelFormatComponentType::Float,        PixelFormatCompression::ASTC, true,  false },
    PixelFormatInfo{ "ASTC_8x8_Float",       PixelFormat::ASTC_8x8_Float,        8u,  8u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,        PixelFormatComponentType::Float,        PixelFormatCompression::ASTC, true,  false },
    PixelFormatInfo{ "ASTC_10x10_Float",     PixelFormat::ASTC_10x10_Float,     10u, 10u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,        PixelFormatComponentType::Float,        PixelFormatCompression::ASTC, true,  false },
    PixelFormatInfo{ "ASTC_12x12_Float",     PixelFormat::ASTC_12x12_Float,     12u, 12u, 1u, 16u, 4u, PixelFormatChannelFlags::RGBA,        PixelFormatComponentType::Float,        PixelFormatCompression::ASTC, true,  false }
};
// clang-format on

namespace detail
{

/// @brief Compile-time structural invariant verifier for `kPixelFormatTable`.
/// @return True when every slot index matches its `PixelFormat` enumerator and holds valid layout parameters.
[[nodiscard]] consteval bool validatePixelFormatTable() noexcept
{
    for (gp::USize i = 0u; i < kPixelFormatCount; ++i)
    {
        const PixelFormatInfo& info = kPixelFormatTable[i];
        if (info.format != static_cast<PixelFormat>(i))
        {
            return false;
        }

        if (info.name.isEmpty())
        {
            return false;
        }

        if (i == 0u)
        {
            if (info.isValid())
            {
                return false;
            }
        }
        else
        {
            if (!info.isValid() || info.numComponents == 0u || info.channels == PixelFormatChannelFlags::None ||
                info.componentType == PixelFormatComponentType::None)
            {
                return false;
            }
        }
    }

    return true;
}

static_assert(
    validatePixelFormatTable(), "kPixelFormatTable indices, names, or invariants are out of sync with PixelFormat."
);

}   // namespace detail

/// @brief Returns the immutable `PixelFormatInfo` entry for `format`.
/// @param[in] format Pixel format enumerator to query.
/// @return Reference to the matching `PixelFormatInfo`, or `kPixelFormatTable[0]` (`Unknown`) if out of range.
[[nodiscard]] inline constexpr const PixelFormatInfo& getPixelFormatInfo(const PixelFormat format) noexcept
{
    const gp::USize index = static_cast<gp::USize>(format);
    if (index >= kPixelFormatCount)
    {
        return kPixelFormatTable[0u];
    }

    return kPixelFormatTable[index];
}

/// @brief Checks whether `format` is a valid, usable pixel format (neither `Unknown` nor out of range).
/// @param[in] format Pixel format to test.
/// @return True when `format` represents a valid pixel format.
[[nodiscard]] inline constexpr bool isValidPixelFormat(const PixelFormat format) noexcept
{
    return getPixelFormatInfo(format).isValid();
}

/// @brief Returns the canonical ASCII name of `format` (e.g., `"R8G8B8A8_UNorm"`).
/// @param[in] format Pixel format enumerator to stringify.
/// @return Static string view naming `format`, or `"Unknown"` if invalid.
[[nodiscard]] inline constexpr gp::StringView toString(const PixelFormat format) noexcept
{
    return getPixelFormatInfo(format).name;
}

/// @brief Resolves a case-insensitive ASCII format name to its `PixelFormat` enumerator.
/// @param[in] name Format string to parse (e.g., `"B8G8R8A8_UNorm"` or `"bc7_unorm"`).
/// @return Matching `PixelFormat`, or `PixelFormat::Unknown` if unrecognized.
[[nodiscard]] inline constexpr PixelFormat pixelFormatFromString(const gp::StringView name) noexcept
{
    if (name.isEmpty())
    {
        return PixelFormat::Unknown;
    }

    for (gp::USize i = 1u; i < kPixelFormatCount; ++i)
    {
        if (kPixelFormatTable[i].name.equalsIgnoreCase(name))
        {
            return kPixelFormatTable[i].format;
        }
    }

    return PixelFormat::Unknown;
}

/// @brief Checks whether `format` uses BCn / DXTC 4x4 block compression.
/// @param[in] format Pixel format to test.
/// @return True for `BC1` through `BC7` formats.
[[nodiscard]] inline constexpr bool isBCBlockCompressed(const PixelFormat format) noexcept
{
    return getPixelFormatInfo(format).compression == PixelFormatCompression::BC;
}

/// @brief Checks whether `format` uses ETC2 / EAC block compression.
/// @param[in] format Pixel format to test.
/// @return True for `ETC2_*` formats.
[[nodiscard]] inline constexpr bool isETC2BlockCompressed(const PixelFormat format) noexcept
{
    return getPixelFormatInfo(format).compression == PixelFormatCompression::ETC2;
}

/// @brief Checks whether `format` uses ASTC block compression.
/// @param[in] format Pixel format to test.
/// @return True for `ASTC_*` LDR and HDR formats.
[[nodiscard]] inline constexpr bool isASTCBlockCompressed(const PixelFormat format) noexcept
{
    return getPixelFormatInfo(format).compression == PixelFormatCompression::ASTC;
}

/// @brief Checks whether `format` uses any hardware block-compression scheme (BCn, ETC2, or ASTC).
/// @param[in] format Pixel format to test.
/// @return True when `format` is block-compressed.
[[nodiscard]] inline constexpr bool isBlockCompressed(const PixelFormat format) noexcept
{
    return getPixelFormatInfo(format).compression != PixelFormatCompression::None;
}

/// @brief Checks whether `format` requires top-level texture dimensions aligned to 4x4 texel blocks.
/// @details BCn / DXTC hardware requires 4x4 alignment on mip 0; ASTC and ETC2 do not.
/// @param[in] format Pixel format to test.
/// @return True when 4x4 block alignment is required.
[[nodiscard]] inline constexpr bool requiresBlock4Alignment(const PixelFormat format) noexcept
{
    return isBCBlockCompressed(format);
}

/// @brief Checks whether `format` stores floating-point channel values (signed, unsigned, or shared-exponent).
/// @param[in] format Pixel format to test.
/// @return True for `Float`, `UFloat`, and `SharedExp` formats.
[[nodiscard]] inline constexpr bool isFloatFormat(const PixelFormat format) noexcept
{
    const PixelFormatComponentType type = getPixelFormatInfo(format).componentType;
    return type == PixelFormatComponentType::Float || type == PixelFormatComponentType::UFloat ||
           type == PixelFormatComponentType::SharedExp;
}

/// @brief Checks whether `format` stores unnormalized integer channel values.
/// @param[in] format Pixel format to test.
/// @return True for `UInt` and `SInt` formats.
[[nodiscard]] inline constexpr bool isIntegerFormat(const PixelFormat format) noexcept
{
    const PixelFormatComponentType type = getPixelFormatInfo(format).componentType;
    return type == PixelFormatComponentType::UInt || type == PixelFormatComponentType::SInt;
}

/// @brief Checks whether `format` stores normalized fixed-point channel values (`UNorm` or `SNorm`).
/// @param[in] format Pixel format to test.
/// @return True for `UNorm` and `SNorm` formats.
[[nodiscard]] inline constexpr bool isNormalizedFormat(const PixelFormat format) noexcept
{
    const PixelFormatComponentType type = getPixelFormatInfo(format).componentType;
    return type == PixelFormatComponentType::UNorm || type == PixelFormatComponentType::SNorm;
}

/// @brief Checks whether `format` can represent negative channel values.
/// @param[in] format Pixel format to test.
/// @return True for `SNorm`, `SInt`, and signed `Float` formats.
[[nodiscard]] inline constexpr bool isSignedFormat(const PixelFormat format) noexcept
{
    const PixelFormatComponentType type = getPixelFormatInfo(format).componentType;
    return type == PixelFormatComponentType::SNorm || type == PixelFormatComponentType::SInt ||
           type == PixelFormatComponentType::Float;
}

/// @brief Checks whether `format` is a high-dynamic-range color format.
/// @param[in] format Pixel format to test.
/// @return True for floating-point, shared-exponent, BC6H, and ASTC HDR color formats (excludes depth-only formats).
[[nodiscard]] inline constexpr bool isHDRFormat(const PixelFormat format) noexcept
{
    return getPixelFormatInfo(format).isHDR;
}

/// @brief Checks whether `format` contains a depth channel.
/// @param[in] format Pixel format to test.
/// @return True when `PixelFormatChannelFlags::Depth` is set.
[[nodiscard]] inline constexpr bool isDepthFormat(const PixelFormat format) noexcept
{
    return (getPixelFormatInfo(format).channels & PixelFormatChannelFlags::Depth) != PixelFormatChannelFlags::None;
}

/// @brief Checks whether `format` contains a stencil channel.
/// @param[in] format Pixel format to test.
/// @return True when `PixelFormatChannelFlags::Stencil` is set.
[[nodiscard]] inline constexpr bool isStencilFormat(const PixelFormat format) noexcept
{
    return (getPixelFormatInfo(format).channels & PixelFormatChannelFlags::Stencil) != PixelFormatChannelFlags::None;
}

/// @brief Checks whether `format` contains a depth channel, a stencil channel, or both.
/// @param[in] format Pixel format to test.
/// @return True when either depth or stencil channels are present.
[[nodiscard]] inline constexpr bool isDepthOrStencilFormat(const PixelFormat format) noexcept
{
    return (getPixelFormatInfo(format).channels & PixelFormatChannelFlags::DepthStencil) !=
           PixelFormatChannelFlags::None;
}

/// @brief Checks whether `format` supports hardware sRGB gamma decoding/encoding views.
/// @param[in] format Pixel format to test.
/// @return True when sRGB views are valid for `format`.
[[nodiscard]] inline constexpr bool supportsSRGB(const PixelFormat format) noexcept
{
    return getPixelFormatInfo(format).supportsSRGB;
}

/// @brief Returns the active channel bitmask for `format`.
/// @param[in] format Pixel format to query.
/// @return `PixelFormatChannelFlags` bitmask describing present channels.
[[nodiscard]] inline constexpr PixelFormatChannelFlags getPixelFormatChannels(const PixelFormat format) noexcept
{
    return getPixelFormatInfo(format).channels;
}

}   // namespace gp::math
