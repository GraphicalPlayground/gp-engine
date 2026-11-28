// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanSurfaceConversion.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "memory/Memory.hpp"
#include "RHIPixelFormat.hpp"
#include <bit>
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Writes a value into a texel buffer without alignment requirements.
template <typename InValueType>
void storeValue(gp::UInt8* const destination, const InValueType value) noexcept
{
    gp::memory::copyMemory(destination, &value, sizeof(InValueType));
}

[[nodiscard]] bool isNaN(const gp::Float32 value) noexcept
{
    return value != value;
}

[[nodiscard]] bool isHalfNaN(const gp::UInt16 half) noexcept
{
    return (half & 0x7C00u) == 0x7C00u && (half & 0x03FFu) != 0u;
}

}   // namespace

TEST(VulkanHalfFloatTest, DecodesReferenceValues)
{
    EXPECT_FLOAT_EQ(halfToFloat(0x0000u), 0.0f);
    EXPECT_FLOAT_EQ(halfToFloat(kHalfOne), 1.0f);
    EXPECT_FLOAT_EQ(halfToFloat(0xC000u), -2.0f);
    EXPECT_FLOAT_EQ(halfToFloat(0x3800u), 0.5f);
    EXPECT_FLOAT_EQ(halfToFloat(0x7BFFu), 65504.0f);
    EXPECT_FLOAT_EQ(halfToFloat(0x0400u), 6.103515625e-05f);
    EXPECT_FLOAT_EQ(halfToFloat(0x0001u), 5.9604644775390625e-08f);
}

TEST(VulkanHalfFloatTest, DecodesSignedZeroInfinityAndNaN)
{
    EXPECT_EQ(std::bit_cast<gp::UInt32>(halfToFloat(0x8000u)), 0x80000000u);
    EXPECT_EQ(std::bit_cast<gp::UInt32>(halfToFloat(0x7C00u)), 0x7F800000u);
    EXPECT_EQ(std::bit_cast<gp::UInt32>(halfToFloat(0xFC00u)), 0xFF800000u);
    EXPECT_TRUE(isNaN(halfToFloat(0x7E00u)));
    EXPECT_TRUE(isNaN(halfToFloat(0x7C01u)));
}

TEST(VulkanHalfFloatTest, EveryNonNaNHalfRoundTrips)
{
    for (gp::UInt32 bits = 0u; bits <= 0xFFFFu; ++bits)
    {
        const gp::UInt16 half = static_cast<gp::UInt16>(bits);
        if (isHalfNaN(half))
        {
            EXPECT_TRUE(isHalfNaN(floatToHalf(halfToFloat(half)))) << "half 0x" << std::hex << bits;
            continue;
        }
        EXPECT_EQ(floatToHalf(halfToFloat(half)), half) << "half 0x" << std::hex << bits;
    }
}

TEST(VulkanHalfFloatTest, EncodeRoundsToNearestEven)
{
    // 1 + 2^-11 is exactly between 1.0 and the next half: ties go to the even mantissa (1.0).
    EXPECT_EQ(floatToHalf(1.0f + 0.00048828125f), kHalfOne);
    // 1 + 3 * 2^-11 is between mantissas 1 and 2: ties go to 2.
    EXPECT_EQ(floatToHalf(1.0f + 3.0f * 0.00048828125f), 0x3C02u);
    // Just above the midpoint rounds up.
    EXPECT_EQ(floatToHalf(1.0f + 0.000489f), 0x3C01u);
}

TEST(VulkanHalfFloatTest, EncodeOverflowsToInfinityPastTheLargestHalf)
{
    EXPECT_EQ(floatToHalf(65504.0f), 0x7BFFu);
    EXPECT_EQ(floatToHalf(65519.0f), 0x7BFFu);
    EXPECT_EQ(floatToHalf(65520.0f), 0x7C00u);
    EXPECT_EQ(floatToHalf(-1.0e10f), 0xFC00u);
    EXPECT_EQ(floatToHalf(std::bit_cast<gp::Float32>(0x7F800000u)), 0x7C00u);
    EXPECT_TRUE(isHalfNaN(floatToHalf(std::bit_cast<gp::Float32>(0x7FC00000u))));
}

TEST(VulkanHalfFloatTest, EncodeUnderflowsToSubnormalsAndZero)
{
    // 2^-25 is the midpoint between zero and the smallest subnormal: ties to the even zero.
    EXPECT_EQ(floatToHalf(2.98023223876953125e-08f), 0x0000u);
    EXPECT_EQ(floatToHalf(2.98023223876953125e-08f * 1.5f), 0x0001u);
    EXPECT_EQ(floatToHalf(1.0e-10f), 0x0000u);
    EXPECT_EQ(floatToHalf(-1.0e-10f), 0x8000u);
    EXPECT_EQ(floatToHalf(3.0517578125e-05f), 0x0200u);
}

TEST(VulkanSurfaceConversionTest, TexelBytesMatchTheCopiedLayout)
{
    EXPECT_EQ(getVulkanSurfaceTexelBytes(PixelFormat::R8_UNorm), 1u);
    EXPECT_EQ(getVulkanSurfaceTexelBytes(PixelFormat::B5G6R5_UNorm), 2u);
    EXPECT_EQ(getVulkanSurfaceTexelBytes(PixelFormat::B8G8R8A8_UNorm), 4u);
    EXPECT_EQ(getVulkanSurfaceTexelBytes(PixelFormat::R16G16B16A16_Float), 8u);
    EXPECT_EQ(getVulkanSurfaceTexelBytes(PixelFormat::R32G32B32A32_Float), 16u);
    // Only the depth aspect is copied out of combined depth/stencil formats.
    EXPECT_EQ(getVulkanSurfaceTexelBytes(PixelFormat::D24_UNorm_S8_UInt), 4u);
    EXPECT_EQ(getVulkanSurfaceTexelBytes(PixelFormat::D32_Float_S8_UInt), 4u);
}

TEST(VulkanSurfaceConversionTest, RejectsCompressedSignedAndIntegerFormats)
{
    EXPECT_FALSE(isVulkanSurfaceConversionSupported(PixelFormat::Unknown));
    EXPECT_FALSE(isVulkanSurfaceConversionSupported(PixelFormat::BC1_UNorm));
    EXPECT_FALSE(isVulkanSurfaceConversionSupported(PixelFormat::R8_SNorm));
    EXPECT_FALSE(isVulkanSurfaceConversionSupported(PixelFormat::R32_UInt));
    EXPECT_FALSE(isVulkanSurfaceConversionSupported(PixelFormat::R9G9B9E5_SharedExp));
    EXPECT_TRUE(isVulkanSurfaceConversionSupported(PixelFormat::R11G11B10_Float));
    EXPECT_TRUE(isVulkanSurfaceConversionSupported(PixelFormat::D16_UNorm));

    const gp::UInt8 texel[16]{};
    gp::math::Color color{};
    gp::math::LinearColor linear{};
    EXPECT_FALSE(convertVulkanSurfaceToColor(PixelFormat::BC1_UNorm, texel, 16u, 1u, 1u, false, &color));
    EXPECT_FALSE(convertVulkanSurfaceToLinearColor(PixelFormat::BC1_UNorm, texel, 16u, 1u, 1u, false, &linear));
}

TEST(VulkanSurfaceConversionTest, RejectsNullPointers)
{
    const gp::UInt8 texel[4]{};
    gp::math::Color color{};
    EXPECT_FALSE(convertVulkanSurfaceToColor(PixelFormat::R8G8B8A8_UNorm, nullptr, 4u, 1u, 1u, false, &color));
    EXPECT_FALSE(convertVulkanSurfaceToColor(PixelFormat::R8G8B8A8_UNorm, texel, 4u, 1u, 1u, false, nullptr));
}

TEST(VulkanSurfaceConversionTest, EightBitFormatsKeepTheirBytes)
{
    const gp::UInt8 texel[4] = { 10u, 20u, 30u, 40u };
    gp::math::Color color{};

    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R8G8B8A8_UNorm, texel, 4u, 1u, 1u, true, &color));
    EXPECT_EQ(color, gp::math::Color(10u, 20u, 30u, 40u));

    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::B8G8R8A8_UNorm, texel, 4u, 1u, 1u, true, &color));
    EXPECT_EQ(color, gp::math::Color(30u, 20u, 10u, 40u));

    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R8_UNorm, texel, 4u, 1u, 1u, true, &color));
    EXPECT_EQ(color, gp::math::Color(10u, 10u, 10u, 255u));

    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::A8_UNorm, texel, 4u, 1u, 1u, true, &color));
    EXPECT_EQ(color, gp::math::Color(0u, 0u, 0u, 10u));

    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R8G8_UNorm, texel, 4u, 1u, 1u, true, &color));
    EXPECT_EQ(color, gp::math::Color(10u, 20u, 0u, 255u));
}

TEST(VulkanSurfaceConversionTest, HonorsTheSourcePitch)
{
    // 2x2 R8 surface with 2 bytes of row padding.
    const gp::UInt8 surface[8] = { 1u, 2u, 0xEEu, 0xEEu, 3u, 4u, 0xEEu, 0xEEu };
    gp::math::Color colors[4]{};
    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R8_UNorm, surface, 4u, 2u, 2u, false, colors));
    EXPECT_EQ(colors[0].r, 1u);
    EXPECT_EQ(colors[1].r, 2u);
    EXPECT_EQ(colors[2].r, 3u);
    EXPECT_EQ(colors[3].r, 4u);
}

TEST(VulkanSurfaceConversionTest, TenBitFormatsRequantizeAndSwizzle)
{
    const gp::UInt32 bits = 1023u | (0u << 10u) | (512u << 20u) | (3u << 30u);
    gp::UInt8 texel[4]{};
    storeValue(texel, bits);
    gp::math::Color color{};

    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R10G10B10A2_UNorm, texel, 4u, 1u, 1u, false, &color));
    EXPECT_EQ(color, gp::math::Color(255u, 0u, 128u, 255u));

    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::B10G10R10A2_UNorm, texel, 4u, 1u, 1u, false, &color));
    EXPECT_EQ(color, gp::math::Color(128u, 0u, 255u, 255u));
}

TEST(VulkanSurfaceConversionTest, SixteenBitAndPackedUNormFormatsRequantize)
{
    gp::UInt8 texel[8]{};
    storeValue<gp::UInt16>(texel, 0xFFFFu);
    storeValue<gp::UInt16>(texel + 2u, 0u);
    storeValue<gp::UInt16>(texel + 4u, 32768u);
    storeValue<gp::UInt16>(texel + 6u, 0xFFFFu);
    gp::math::Color color{};
    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R16G16B16A16_UNorm, texel, 8u, 1u, 1u, false, &color));
    EXPECT_EQ(color, gp::math::Color(255u, 0u, 128u, 255u));

    // R in the top 5 bits, G in the middle 6, B in the low 5.
    storeValue<gp::UInt16>(texel, static_cast<gp::UInt16>((31u << 11u) | (0u << 5u) | 16u));
    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::B5G6R5_UNorm, texel, 2u, 1u, 1u, false, &color));
    EXPECT_EQ(color, gp::math::Color(255u, 0u, 132u, 255u));
}

TEST(VulkanSurfaceConversionTest, DepthFormatsBecomeGray)
{
    gp::UInt8 texel[4]{};
    gp::math::Color color{};

    // Stencil bits in the top byte are ignored.
    storeValue<gp::UInt32>(texel, 0xAB000000u | 0xFFFFFFu);
    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::D24_UNorm_S8_UInt, texel, 4u, 1u, 1u, true, &color));
    EXPECT_EQ(color, gp::math::Color(255u, 255u, 255u, 255u));

    storeValue<gp::Float32>(texel, 0.5f);
    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::D32_Float, texel, 4u, 1u, 1u, true, &color));
    EXPECT_EQ(color, gp::math::Color(128u, 128u, 128u, 255u));

    storeValue<gp::UInt16>(texel, 0u);
    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::D16_UNorm, texel, 2u, 1u, 1u, true, &color));
    EXPECT_EQ(color, gp::math::Color(0u, 0u, 0u, 255u));
}

TEST(VulkanSurfaceConversionTest, SingleFloatSurfacesAreNormalizedToTheirRange)
{
    gp::UInt8 surface[12]{};
    storeValue<gp::Float32>(surface, -2.0f);
    storeValue<gp::Float32>(surface + 4u, 0.0f);
    storeValue<gp::Float32>(surface + 8u, 2.0f);

    gp::math::Color colors[3]{};
    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R32_Float, surface, 12u, 3u, 1u, true, colors));
    EXPECT_EQ(colors[0], gp::math::Color(0u, 0u, 0u, 255u));
    EXPECT_EQ(colors[1], gp::math::Color(128u, 128u, 128u, 255u));
    EXPECT_EQ(colors[2], gp::math::Color(255u, 255u, 255u, 255u));
}

TEST(VulkanSurfaceConversionTest, ConstantFloatSurfaceKeepsItsValue)
{
    gp::UInt8 surface[8]{};
    storeValue<gp::Float32>(surface, 0.5f);
    storeValue<gp::Float32>(surface + 4u, 0.5f);

    gp::math::Color colors[2]{};
    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R32_Float, surface, 8u, 2u, 1u, true, colors));
    EXPECT_EQ(colors[0].r, 128u);
    EXPECT_EQ(colors[1].r, 128u);
}

TEST(VulkanSurfaceConversionTest, Rgba32FloatNormalizesColorAndClampsAlpha)
{
    gp::UInt8 surface[32]{};
    const gp::Float32 values[8] = { 0.0f, 4.0f, 8.0f, 2.0f, 8.0f, 0.0f, 4.0f, -1.0f };
    for (gp::UInt32 index = 0u; index < 8u; ++index)
    {
        storeValue<gp::Float32>(surface + index * 4u, values[index]);
    }

    gp::math::Color colors[2]{};
    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R32G32B32A32_Float, surface, 32u, 2u, 1u, true, colors));
    EXPECT_EQ(colors[0], gp::math::Color(0u, 128u, 255u, 255u));
    EXPECT_EQ(colors[1], gp::math::Color(255u, 0u, 128u, 0u));
}

TEST(VulkanSurfaceConversionTest, NonFiniteValuesDoNotWidenTheRange)
{
    gp::UInt8 surface[12]{};
    storeValue<gp::Float32>(surface, 0.0f);
    storeValue<gp::Float32>(surface + 4u, std::bit_cast<gp::Float32>(0x7F800000u));
    storeValue<gp::Float32>(surface + 8u, 1.0f);

    gp::math::Color colors[3]{};
    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R32_Float, surface, 12u, 3u, 1u, true, colors));
    EXPECT_EQ(colors[0].r, 0u);
    EXPECT_EQ(colors[1].r, 255u);
    EXPECT_EQ(colors[2].r, 255u);
}

TEST(VulkanSurfaceConversionTest, HalfFloatColorsOptionallyGammaEncode)
{
    gp::UInt8 texel[8]{};
    storeValue<gp::UInt16>(texel, kHalfOne);
    storeValue<gp::UInt16>(texel + 2u, 0x3800u);
    storeValue<gp::UInt16>(texel + 4u, 0u);
    storeValue<gp::UInt16>(texel + 6u, kHalfOne);
    gp::math::Color color{};

    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R16G16B16A16_Float, texel, 8u, 1u, 1u, false, &color));
    EXPECT_EQ(color, gp::math::Color(255u, 128u, 0u, 255u));

    ASSERT_TRUE(convertVulkanSurfaceToColor(PixelFormat::R16G16B16A16_Float, texel, 8u, 1u, 1u, true, &color));
    EXPECT_EQ(color, gp::math::LinearColor(1.0f, 0.5f, 0.0f, 1.0f).toColorSRGB());
}

TEST(VulkanSurfaceConversionTest, LinearEightBitDecodesSRGBOnlyWhenAsked)
{
    const gp::UInt8 texel[4] = { 255u, 128u, 51u, 255u };
    gp::math::LinearColor color{};

    ASSERT_TRUE(convertVulkanSurfaceToLinearColor(PixelFormat::R8G8B8A8_UNorm, texel, 4u, 1u, 1u, false, &color));
    EXPECT_FLOAT_EQ(color.r, 1.0f);
    EXPECT_NEAR(color.g, 128.0f / 255.0f, 1.0e-6f);
    EXPECT_NEAR(color.b, 0.2f, 1.0e-6f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);

    ASSERT_TRUE(convertVulkanSurfaceToLinearColor(PixelFormat::R8G8B8A8_UNorm, texel, 4u, 1u, 1u, true, &color));
    EXPECT_FLOAT_EQ(color.r, 1.0f);
    EXPECT_NEAR(color.g, 0.2158605f, 1.0e-4f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);
}

TEST(VulkanSurfaceConversionTest, LinearFloatFormatsAreCopiedAndMissingChannelsFilled)
{
    gp::UInt8 texel[8]{};
    gp::math::LinearColor color{};

    storeValue<gp::UInt16>(texel, 0x4000u);
    ASSERT_TRUE(convertVulkanSurfaceToLinearColor(PixelFormat::R16_Float, texel, 2u, 1u, 1u, false, &color));
    EXPECT_FLOAT_EQ(color.r, 2.0f);
    EXPECT_FLOAT_EQ(color.g, 0.0f);
    EXPECT_FLOAT_EQ(color.b, 0.0f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);

    storeValue<gp::Float32>(texel, -3.5f);
    storeValue<gp::Float32>(texel + 4u, 100.0f);
    ASSERT_TRUE(convertVulkanSurfaceToLinearColor(PixelFormat::R32G32_Float, texel, 8u, 1u, 1u, false, &color));
    EXPECT_FLOAT_EQ(color.r, -3.5f);
    EXPECT_FLOAT_EQ(color.g, 100.0f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);

    storeValue<gp::UInt16>(texel, 0xFFFFu);
    ASSERT_TRUE(convertVulkanSurfaceToLinearColor(PixelFormat::D16_UNorm, texel, 2u, 1u, 1u, false, &color));
    EXPECT_FLOAT_EQ(color.r, 1.0f);
}

TEST(VulkanSurfaceConversionTest, LinearPackedFloatDecodesEveryChannel)
{
    // R = 1.0 (exponent 15), G = 0.5 (exponent 14), B = 2.0 (exponent 16, 5-bit mantissa).
    const gp::UInt32 bits = (15u << 6u) | ((14u << 6u) << 11u) | ((16u << 5u) << 22u);
    gp::UInt8 texel[4]{};
    storeValue(texel, bits);

    gp::math::LinearColor color{};
    ASSERT_TRUE(convertVulkanSurfaceToLinearColor(PixelFormat::R11G11B10_Float, texel, 4u, 1u, 1u, false, &color));
    EXPECT_FLOAT_EQ(color.r, 1.0f);
    EXPECT_FLOAT_EQ(color.g, 0.5f);
    EXPECT_FLOAT_EQ(color.b, 2.0f);
    EXPECT_FLOAT_EQ(color.a, 1.0f);
}

TEST(VulkanSurfaceConversionTest, HalfSurfacesAreCopiedBitForBit)
{
    gp::UInt8 texel[8]{};
    storeValue<gp::UInt16>(texel, 0x7E01u);
    storeValue<gp::UInt16>(texel + 2u, 0x8000u);
    storeValue<gp::UInt16>(texel + 4u, 0x0001u);
    storeValue<gp::UInt16>(texel + 6u, 0x7C00u);

    resource::Float16Color color{};
    ASSERT_TRUE(convertVulkanSurfaceToFloat16Color(PixelFormat::R16G16B16A16_Float, texel, 8u, 1u, 1u, &color));
    EXPECT_EQ(color, resource::Float16Color(0x7E01u, 0x8000u, 0x0001u, 0x7C00u));

    ASSERT_TRUE(convertVulkanSurfaceToFloat16Color(PixelFormat::R16G16_Float, texel, 4u, 1u, 1u, &color));
    EXPECT_EQ(color, resource::Float16Color(0x7E01u, 0x8000u, 0u, kHalfOne));
}

TEST(VulkanSurfaceConversionTest, SingleFloatSurfacesAreReencoded)
{
    gp::UInt8 surface[8]{};
    storeValue<gp::Float32>(surface, 1.0f);
    storeValue<gp::Float32>(surface + 4u, -2.0f);

    resource::Float16Color colors[2]{};
    ASSERT_TRUE(convertVulkanSurfaceToFloat16Color(PixelFormat::R32_Float, surface, 8u, 2u, 1u, colors));
    EXPECT_EQ(colors[0], resource::Float16Color(kHalfOne, 0u, 0u, kHalfOne));
    EXPECT_EQ(colors[1], resource::Float16Color(0xC000u, 0u, 0u, kHalfOne));
}

TEST(VulkanSurfaceConversionTest, Float16RejectsNonFloatFormats)
{
    const gp::UInt8 texel[4]{};
    resource::Float16Color color{};
    EXPECT_FALSE(convertVulkanSurfaceToFloat16Color(PixelFormat::R8G8B8A8_UNorm, texel, 4u, 1u, 1u, &color));
    EXPECT_FALSE(convertVulkanSurfaceToFloat16Color(PixelFormat::D32_Float, texel, 4u, 1u, 1u, &color));
}

}   // namespace gp::rhi::vulkan::tests
