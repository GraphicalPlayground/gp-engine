// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/color/PixelFormat.hpp"
#include <gtest/gtest.h>

namespace gp::math::tests
{

static_assert(kPixelFormatCount == static_cast<gp::USize>(PixelFormat::Count), "Table size must match enum Count.");
static_assert(!isValidPixelFormat(PixelFormat::Unknown), "Unknown format must not be valid.");
static_assert(isValidPixelFormat(PixelFormat::R8G8B8A8_UNorm), "R8G8B8A8_UNorm must be valid.");
static_assert(
    getPixelFormatInfo(PixelFormat::R8G8B8A8_UNorm).get2DImageSizeInBytes(16u, 16u) == 1024ull,
    "Constexpr 2D uncompressed image size calculation failed."
);
static_assert(
    getPixelFormatInfo(PixelFormat::BC1_UNorm).get2DImageSizeInBytes(4u, 4u) == 8ull,
    "Constexpr 2D BC1 single block size calculation failed."
);
static_assert(
    toString(PixelFormat::B8G8R8A8_UNorm) == gp::StringView("B8G8R8A8_UNorm"), "Constexpr toString lookup failed."
);
static_assert(
    pixelFormatFromString("b8g8r8a8_unorm") == PixelFormat::B8G8R8A8_UNorm,
    "Constexpr case-insensitive pixelFormatFromString lookup failed."
);
static_assert(isBlockCompressed(PixelFormat::BC7_UNorm), "BC7_UNorm must be block compressed.");
static_assert(isDepthOrStencilFormat(PixelFormat::D24_UNorm_S8_UInt), "D24_UNorm_S8_UInt must be depth-stencil.");
static_assert(!isHDRFormat(PixelFormat::D32_Float), "Depth-only D32_Float must not be classified as HDR color.");
static_assert(isHDRFormat(PixelFormat::R16G16B16A16_Float), "R16G16B16A16_Float must be HDR.");

TEST(PixelFormatTableTest, EveryEnumeratorMatchesItsTableEntry)
{
    for (gp::USize i = 0u; i < kPixelFormatCount; ++i)
    {
        const PixelFormat format = static_cast<PixelFormat>(i);
        const PixelFormatInfo& info = getPixelFormatInfo(format);

        EXPECT_EQ(info.format, format);
        EXPECT_FALSE(info.name.isEmpty());

        if (format == PixelFormat::Unknown)
        {
            EXPECT_FALSE(info.isValid());
            EXPECT_FALSE(isValidPixelFormat(format));
            EXPECT_EQ(info.blockBytes, 0u);
        }
        else
        {
            EXPECT_TRUE(info.isValid());
            EXPECT_TRUE(isValidPixelFormat(format));
            EXPECT_GT(info.blockSizeX, 0u);
            EXPECT_GT(info.blockSizeY, 0u);
            EXPECT_GT(info.blockSizeZ, 0u);
            EXPECT_GT(info.blockBytes, 0u);
            EXPECT_GT(info.numComponents, 0u);
        }
    }
}

TEST(PixelFormatTableTest, OutOfRangeEnumClampsSafelyToUnknown)
{
    const PixelFormat outOfRangeA = PixelFormat::Count;
    const PixelFormat outOfRangeB = static_cast<PixelFormat>(250u);

    EXPECT_EQ(getPixelFormatInfo(outOfRangeA).format, PixelFormat::Unknown);
    EXPECT_EQ(getPixelFormatInfo(outOfRangeB).format, PixelFormat::Unknown);
    EXPECT_FALSE(isValidPixelFormat(outOfRangeA));
    EXPECT_FALSE(isValidPixelFormat(outOfRangeB));
    EXPECT_EQ(toString(outOfRangeB), gp::StringView("Unknown"));
}

TEST(PixelFormatStringTest, RoundTripsAllValidFormats)
{
    for (gp::USize i = 1u; i < kPixelFormatCount; ++i)
    {
        const PixelFormat format = static_cast<PixelFormat>(i);
        const gp::StringView name = toString(format);

        EXPECT_EQ(pixelFormatFromString(name), format);
    }
}

TEST(PixelFormatStringTest, CaseInsensitiveLookupAndInvalidStrings)
{
    EXPECT_EQ(pixelFormatFromString("r8g8b8a8_unorm"), PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(pixelFormatFromString("BC7_UNORM"), PixelFormat::BC7_UNorm);
    EXPECT_EQ(pixelFormatFromString("astc_6x6_float"), PixelFormat::ASTC_6x6_Float);
    EXPECT_EQ(pixelFormatFromString("d32_float_s8_uint"), PixelFormat::D32_Float_S8_UInt);

    EXPECT_EQ(pixelFormatFromString(""), PixelFormat::Unknown);
    EXPECT_EQ(pixelFormatFromString("Unknown"), PixelFormat::Unknown);
    EXPECT_EQ(pixelFormatFromString("PF_NotARealFormat"), PixelFormat::Unknown);
}

TEST(PixelFormatSizeTest, Uncompressed2DImageAndMipChain)
{
    const PixelFormatInfo& rgba8 = getPixelFormatInfo(PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(rgba8.getBlockCountForWidth(128u), 128ull);
    EXPECT_EQ(rgba8.getBlockCountForHeight(64u), 64ull);
    EXPECT_EQ(rgba8.get2DImageSizeInBytes(128u, 64u), 128ull * 64ull * 4ull);

    // 8x4 texture: mip 0 = 8x4 (128 B), mip 1 = 4x2 (32 B), mip 2 = 2x1 (8 B), mip 3 = 1x1 (4 B)
    EXPECT_EQ(rgba8.get2DTextureMipSizeInBytes(8u, 4u, 0u), 128ull);
    EXPECT_EQ(rgba8.get2DTextureMipSizeInBytes(8u, 4u, 1u), 32ull);
    EXPECT_EQ(rgba8.get2DTextureMipSizeInBytes(8u, 4u, 2u), 8ull);
    EXPECT_EQ(rgba8.get2DTextureMipSizeInBytes(8u, 4u, 3u), 4ull);
    // Clamped past smallest mip (still 1x1) and beyond 32-bit shift width
    EXPECT_EQ(rgba8.get2DTextureMipSizeInBytes(8u, 4u, 10u), 4ull);
    EXPECT_EQ(rgba8.get2DTextureMipSizeInBytes(8u, 4u, 40u), 4ull);

    EXPECT_EQ(rgba8.get2DTextureSizeInBytes(8u, 4u, 4u), 128ull + 32ull + 8ull + 4ull);
}

TEST(PixelFormatSizeTest, BlockCompressed2DCeilingRounding)
{
    const PixelFormatInfo& bc1 = getPixelFormatInfo(PixelFormat::BC1_UNorm);
    EXPECT_EQ(bc1.blockSizeX, 4u);
    EXPECT_EQ(bc1.blockSizeY, 4u);
    EXPECT_EQ(bc1.blockBytes, 8u);

    // Non-multiples of 4 round up to whole blocks: 5x5 -> 2x2 blocks -> 4 * 8 = 32 B
    EXPECT_EQ(bc1.getBlockCountForWidth(5u), 2ull);
    EXPECT_EQ(bc1.getBlockCountForHeight(5u), 2ull);
    EXPECT_EQ(bc1.get2DImageSizeInBytes(5u, 5u), 32ull);
    // 1x1 mip still occupies one full 4x4 block (8 B)
    EXPECT_EQ(bc1.get2DImageSizeInBytes(1u, 1u), 8ull);

    // ASTC 6x6: 13x7 -> ceil(13/6)=3 x ceil(7/6)=2 -> 6 blocks * 16 B = 96 B
    const PixelFormatInfo& astc6x6 = getPixelFormatInfo(PixelFormat::ASTC_6x6_UNorm);
    EXPECT_EQ(astc6x6.getBlockCountForWidth(13u), 3ull);
    EXPECT_EQ(astc6x6.getBlockCountForHeight(7u), 2ull);
    EXPECT_EQ(astc6x6.get2DImageSizeInBytes(13u, 7u), 96ull);
}

TEST(PixelFormatSizeTest, Volume3DImageAndMipChain)
{
    const PixelFormatInfo& rgba16f = getPixelFormatInfo(PixelFormat::R16G16B16A16_Float);
    EXPECT_EQ(rgba16f.blockBytes, 8u);

    // 4x4x4 = 64 texels * 8 B = 512 B; mip 1 = 2x2x2 = 64 B; mip 2 = 1x1x1 = 8 B
    EXPECT_EQ(rgba16f.get3DImageSizeInBytes(4u, 4u, 4u), 512ull);
    EXPECT_EQ(rgba16f.get3DTextureMipSizeInBytes(4u, 4u, 4u, 1u), 64ull);
    EXPECT_EQ(rgba16f.get3DTextureMipSizeInBytes(4u, 4u, 4u, 2u), 8ull);
    EXPECT_EQ(rgba16f.get3DTextureSizeInBytes(4u, 4u, 4u, 3u), 512ull + 64ull + 8ull);
}

TEST(PixelFormatSizeTest, ZeroDimensionsAndUnknownFormatReturnZero)
{
    const PixelFormatInfo& unknown = getPixelFormatInfo(PixelFormat::Unknown);
    const PixelFormatInfo& rgba8 = getPixelFormatInfo(PixelFormat::R8G8B8A8_UNorm);

    EXPECT_EQ(unknown.get2DImageSizeInBytes(64u, 64u), 0ull);
    EXPECT_EQ(unknown.get3DImageSizeInBytes(64u, 64u, 64u), 0ull);

    EXPECT_EQ(rgba8.get2DImageSizeInBytes(0u, 64u), 0ull);
    EXPECT_EQ(rgba8.get2DImageSizeInBytes(64u, 0u), 0ull);
    EXPECT_EQ(rgba8.get2DTextureSizeInBytes(64u, 64u, 0u), 0ull);
    EXPECT_EQ(rgba8.get3DImageSizeInBytes(64u, 64u, 0u), 0ull);
    EXPECT_EQ(rgba8.get3DTextureSizeInBytes(64u, 64u, 64u, 0u), 0ull);
}

TEST(PixelFormatPredicatesTest, CompressionFamilyPredicates)
{
    EXPECT_FALSE(isBlockCompressed(PixelFormat::R8G8B8A8_UNorm));
    EXPECT_FALSE(requiresBlock4Alignment(PixelFormat::R8G8B8A8_UNorm));

    EXPECT_TRUE(isBCBlockCompressed(PixelFormat::BC3_UNorm));
    EXPECT_TRUE(isBlockCompressed(PixelFormat::BC3_UNorm));
    EXPECT_TRUE(requiresBlock4Alignment(PixelFormat::BC3_UNorm));
    EXPECT_FALSE(isETC2BlockCompressed(PixelFormat::BC3_UNorm));
    EXPECT_FALSE(isASTCBlockCompressed(PixelFormat::BC3_UNorm));

    EXPECT_TRUE(isETC2BlockCompressed(PixelFormat::ETC2_RGBA_UNorm));
    EXPECT_TRUE(isBlockCompressed(PixelFormat::ETC2_RGBA_UNorm));
    EXPECT_FALSE(requiresBlock4Alignment(PixelFormat::ETC2_RGBA_UNorm));

    EXPECT_TRUE(isASTCBlockCompressed(PixelFormat::ASTC_8x8_Float));
    EXPECT_TRUE(isBlockCompressed(PixelFormat::ASTC_8x8_Float));
    EXPECT_FALSE(requiresBlock4Alignment(PixelFormat::ASTC_8x8_Float));
}

TEST(PixelFormatPredicatesTest, NumericEncodingAndHDRPredicates)
{
    // Floating-point and HDR
    EXPECT_TRUE(isFloatFormat(PixelFormat::R16G16B16A16_Float));
    EXPECT_TRUE(isSignedFormat(PixelFormat::R16G16B16A16_Float));
    EXPECT_TRUE(isHDRFormat(PixelFormat::R16G16B16A16_Float));

    // Unsigned floats (R11G11B10_Float, BC6H_UFloat, R9G9B9E5_SharedExp)
    EXPECT_TRUE(isFloatFormat(PixelFormat::R11G11B10_Float));
    EXPECT_FALSE(isSignedFormat(PixelFormat::R11G11B10_Float));
    EXPECT_TRUE(isHDRFormat(PixelFormat::R11G11B10_Float));

    EXPECT_TRUE(isFloatFormat(PixelFormat::BC6H_UFloat));
    EXPECT_FALSE(isSignedFormat(PixelFormat::BC6H_UFloat));
    EXPECT_TRUE(isHDRFormat(PixelFormat::BC6H_UFloat));

    EXPECT_TRUE(isFloatFormat(PixelFormat::BC6H_SFloat));
    EXPECT_TRUE(isSignedFormat(PixelFormat::BC6H_SFloat));
    EXPECT_TRUE(isHDRFormat(PixelFormat::BC6H_SFloat));

    EXPECT_TRUE(isFloatFormat(PixelFormat::R9G9B9E5_SharedExp));
    EXPECT_FALSE(isSignedFormat(PixelFormat::R9G9B9E5_SharedExp));
    EXPECT_TRUE(isHDRFormat(PixelFormat::R9G9B9E5_SharedExp));

    // Integer formats
    EXPECT_TRUE(isIntegerFormat(PixelFormat::R32_UInt));
    EXPECT_FALSE(isSignedFormat(PixelFormat::R32_UInt));
    EXPECT_TRUE(isIntegerFormat(PixelFormat::R16G16_SInt));
    EXPECT_TRUE(isSignedFormat(PixelFormat::R16G16_SInt));
    EXPECT_FALSE(isNormalizedFormat(PixelFormat::R32_UInt));

    // Normalized formats
    EXPECT_TRUE(isNormalizedFormat(PixelFormat::R8G8B8A8_UNorm));
    EXPECT_FALSE(isSignedFormat(PixelFormat::R8G8B8A8_UNorm));
    EXPECT_TRUE(isNormalizedFormat(PixelFormat::R8G8B8A8_SNorm));
    EXPECT_TRUE(isSignedFormat(PixelFormat::R8G8B8A8_SNorm));

    // sRGB capability
    EXPECT_TRUE(supportsSRGB(PixelFormat::R8G8B8A8_UNorm));
    EXPECT_TRUE(supportsSRGB(PixelFormat::B8G8R8A8_UNorm));
    EXPECT_TRUE(supportsSRGB(PixelFormat::BC7_UNorm));
    EXPECT_TRUE(supportsSRGB(PixelFormat::ASTC_4x4_UNorm));
    EXPECT_FALSE(supportsSRGB(PixelFormat::R16G16B16A16_Float));
    EXPECT_FALSE(supportsSRGB(PixelFormat::R8G8B8A8_UInt));
}

TEST(PixelFormatPredicatesTest, DepthStencilAndChannelFlags)
{
    EXPECT_TRUE(isDepthFormat(PixelFormat::D32_Float));
    EXPECT_FALSE(isStencilFormat(PixelFormat::D32_Float));
    EXPECT_TRUE(isDepthOrStencilFormat(PixelFormat::D32_Float));
    EXPECT_FALSE(isHDRFormat(PixelFormat::D32_Float));

    EXPECT_TRUE(isDepthFormat(PixelFormat::D24_UNorm_S8_UInt));
    EXPECT_TRUE(isStencilFormat(PixelFormat::D24_UNorm_S8_UInt));
    EXPECT_TRUE(isDepthOrStencilFormat(PixelFormat::D24_UNorm_S8_UInt));

    EXPECT_FALSE(isDepthFormat(PixelFormat::X24_G8_UInt));
    EXPECT_TRUE(isStencilFormat(PixelFormat::X24_G8_UInt));
    EXPECT_TRUE(isDepthOrStencilFormat(PixelFormat::X24_G8_UInt));

    EXPECT_EQ(getPixelFormatChannels(PixelFormat::R8_UNorm), PixelFormatChannelFlags::R);
    EXPECT_EQ(getPixelFormatChannels(PixelFormat::A8_UNorm), PixelFormatChannelFlags::A);
    EXPECT_EQ(getPixelFormatChannels(PixelFormat::R16G16_Float), PixelFormatChannelFlags::RG);
    EXPECT_EQ(getPixelFormatChannels(PixelFormat::R11G11B10_Float), PixelFormatChannelFlags::RGB);
    EXPECT_EQ(getPixelFormatChannels(PixelFormat::B8G8R8A8_UNorm), PixelFormatChannelFlags::RGBA);
    EXPECT_EQ(getPixelFormatChannels(PixelFormat::D32_Float_S8_UInt), PixelFormatChannelFlags::DepthStencil);
}

}   // namespace gp::math::tests
