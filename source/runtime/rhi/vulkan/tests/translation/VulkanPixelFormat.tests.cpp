// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "translation/VulkanPixelFormat.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIPixelFormat.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

[[nodiscard]] constexpr bool
    componentMappingEquals(const VkComponentMapping& lhs, const VkComponentMapping& rhs) noexcept
{
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

static_assert(pixelFormatToVkFormat(PixelFormat::Unknown) == VK_FORMAT_UNDEFINED);
static_assert(pixelFormatToVkFormat(PixelFormat::R8G8B8A8_UNorm, false) == VK_FORMAT_R8G8B8A8_UNORM);
static_assert(pixelFormatToVkFormat(PixelFormat::R8G8B8A8_UNorm, true) == VK_FORMAT_R8G8B8A8_SRGB);
static_assert(pixelFormatToVkFormat(PixelFormat::B8G8R8A8_UNorm, true) == VK_FORMAT_B8G8R8A8_SRGB);
static_assert(pixelFormatToVkFormat(PixelFormat::D32_Float_S8_UInt) == VK_FORMAT_D32_SFLOAT_S8_UINT);
static_assert(vkFormatToSRGB(VK_FORMAT_BC7_UNORM_BLOCK) == VK_FORMAT_BC7_SRGB_BLOCK);
static_assert(vkFormatToLinear(VK_FORMAT_BC7_SRGB_BLOCK) == VK_FORMAT_BC7_UNORM_BLOCK);
static_assert(isSRGBVkFormat(VK_FORMAT_R8G8B8A8_SRGB));
static_assert(!isSRGBVkFormat(VK_FORMAT_R8G8B8A8_UNORM));
static_assert(vkFormatHasDepth(VK_FORMAT_D24_UNORM_S8_UINT));
static_assert(vkFormatHasStencil(VK_FORMAT_D24_UNORM_S8_UINT));
static_assert(!vkFormatHasStencil(VK_FORMAT_D32_SFLOAT));
static_assert(getVkImageAspectFlags(PixelFormat::Unknown, true, true) == 0u);
static_assert(isASTCLDRFormat(VK_FORMAT_ASTC_4x4_UNORM_BLOCK));
static_assert(isASTCSRGBFormat(VK_FORMAT_ASTC_12x12_SRGB_BLOCK));
static_assert(isASTCHDRFormat(VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT));

}   // namespace

TEST(VulkanPixelFormatTest, Uncompressed8Bit16Bit32Bit64BitMappings)
{
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::Unknown), VK_FORMAT_UNDEFINED);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::Unknown, true), VK_FORMAT_UNDEFINED);
    EXPECT_EQ(pixelFormatToVkFormat(static_cast<PixelFormat>(kPixelFormatCount)), VK_FORMAT_UNDEFINED);
    EXPECT_EQ(pixelFormatToVkFormat(static_cast<PixelFormat>(255u)), VK_FORMAT_UNDEFINED);

    // 8-bit
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8_UNorm), VK_FORMAT_R8_UNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8_UNorm, true), VK_FORMAT_R8_SRGB);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8_SNorm), VK_FORMAT_R8_SNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8_UInt), VK_FORMAT_R8_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8_SInt), VK_FORMAT_R8_SINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::A8_UNorm), VK_FORMAT_R8_UNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::A8_UNorm, true), VK_FORMAT_R8_UNORM);

    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8_UNorm), VK_FORMAT_R8G8_UNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8_UNorm, true), VK_FORMAT_R8G8_SRGB);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8_SNorm), VK_FORMAT_R8G8_SNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8_UInt), VK_FORMAT_R8G8_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8_SInt), VK_FORMAT_R8G8_SINT);

    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8B8_UNorm), VK_FORMAT_R8G8B8_UNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8B8_UNorm, true), VK_FORMAT_R8G8B8_SRGB);

    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8B8A8_UNorm), VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8B8A8_UNorm, true), VK_FORMAT_R8G8B8A8_SRGB);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8B8A8_SNorm), VK_FORMAT_R8G8B8A8_SNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8B8A8_UInt), VK_FORMAT_R8G8B8A8_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R8G8B8A8_SInt), VK_FORMAT_R8G8B8A8_SINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::B8G8R8A8_UNorm), VK_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::B8G8R8A8_UNorm, true), VK_FORMAT_B8G8R8A8_SRGB);

    // 16-bit
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16_UNorm), VK_FORMAT_R16_UNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16_SNorm), VK_FORMAT_R16_SNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16_UInt), VK_FORMAT_R16_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16_SInt), VK_FORMAT_R16_SINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16_Float), VK_FORMAT_R16_SFLOAT);

    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16G16_UNorm), VK_FORMAT_R16G16_UNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16G16_SNorm), VK_FORMAT_R16G16_SNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16G16_UInt), VK_FORMAT_R16G16_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16G16_SInt), VK_FORMAT_R16G16_SINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16G16_Float), VK_FORMAT_R16G16_SFLOAT);

    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16G16B16A16_UNorm), VK_FORMAT_R16G16B16A16_UNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16G16B16A16_SNorm), VK_FORMAT_R16G16B16A16_SNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16G16B16A16_UInt), VK_FORMAT_R16G16B16A16_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16G16B16A16_SInt), VK_FORMAT_R16G16B16A16_SINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R16G16B16A16_Float), VK_FORMAT_R16G16B16A16_SFLOAT);

    // 32-bit and 64-bit
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32_UInt), VK_FORMAT_R32_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32_SInt), VK_FORMAT_R32_SINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32_Float), VK_FORMAT_R32_SFLOAT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32G32_UInt), VK_FORMAT_R32G32_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32G32_SInt), VK_FORMAT_R32G32_SINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32G32_Float), VK_FORMAT_R32G32_SFLOAT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32G32B32_UInt), VK_FORMAT_R32G32B32_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32G32B32_SInt), VK_FORMAT_R32G32B32_SINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32G32B32_Float), VK_FORMAT_R32G32B32_SFLOAT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32G32B32A32_UInt), VK_FORMAT_R32G32B32A32_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32G32B32A32_SInt), VK_FORMAT_R32G32B32A32_SINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R32G32B32A32_Float), VK_FORMAT_R32G32B32A32_SFLOAT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R64_UInt), VK_FORMAT_R64_UINT);
}

TEST(VulkanPixelFormatTest, PackedDepthStencilAndCompressedMappings)
{
    // Packed and HDR
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::B5G6R5_UNorm), VK_FORMAT_R5G6B5_UNORM_PACK16);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::B5G5R5A1_UNorm), VK_FORMAT_A1R5G5B5_UNORM_PACK16);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R10G10B10A2_UNorm), VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::B10G10R10A2_UNorm), VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R11G11B10_Float), VK_FORMAT_B10G11R11_UFLOAT_PACK32);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::R9G9B9E5_SharedExp), VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);

    // Depth and stencil
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::D16_UNorm), VK_FORMAT_D16_UNORM);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::D24_UNorm_S8_UInt), VK_FORMAT_D24_UNORM_S8_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::D32_Float), VK_FORMAT_D32_SFLOAT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::D32_Float_S8_UInt), VK_FORMAT_D32_SFLOAT_S8_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::X24_G8_UInt), VK_FORMAT_D24_UNORM_S8_UINT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::X32_G8_UInt), VK_FORMAT_D32_SFLOAT_S8_UINT);

    // BCn
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC1_UNorm), VK_FORMAT_BC1_RGBA_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC1_UNorm, true), VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC2_UNorm), VK_FORMAT_BC2_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC2_UNorm, true), VK_FORMAT_BC2_SRGB_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC3_UNorm), VK_FORMAT_BC3_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC3_UNorm, true), VK_FORMAT_BC3_SRGB_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC4_UNorm), VK_FORMAT_BC4_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC5_UNorm), VK_FORMAT_BC5_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC6H_UFloat), VK_FORMAT_BC6H_UFLOAT_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC6H_SFloat), VK_FORMAT_BC6H_SFLOAT_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC7_UNorm), VK_FORMAT_BC7_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::BC7_UNorm, true), VK_FORMAT_BC7_SRGB_BLOCK);

    // ETC2 / EAC
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ETC2_RGB_UNorm), VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ETC2_RGB_UNorm, true), VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ETC2_RGBA_UNorm), VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ETC2_RGBA_UNorm, true), VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ETC2_R11_EAC), VK_FORMAT_EAC_R11_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ETC2_RG11_EAC), VK_FORMAT_EAC_R11G11_UNORM_BLOCK);

    // ASTC LDR + HDR
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_4x4_UNorm), VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_4x4_UNorm, true), VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_6x6_UNorm), VK_FORMAT_ASTC_6x6_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_6x6_UNorm, true), VK_FORMAT_ASTC_6x6_SRGB_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_8x8_UNorm), VK_FORMAT_ASTC_8x8_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_8x8_UNorm, true), VK_FORMAT_ASTC_8x8_SRGB_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_10x10_UNorm), VK_FORMAT_ASTC_10x10_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_10x10_UNorm, true), VK_FORMAT_ASTC_10x10_SRGB_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_12x12_UNorm), VK_FORMAT_ASTC_12x12_UNORM_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_12x12_UNorm, true), VK_FORMAT_ASTC_12x12_SRGB_BLOCK);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_4x4_Float), VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_6x6_Float), VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_8x8_Float), VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_10x10_Float), VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT);
    EXPECT_EQ(pixelFormatToVkFormat(PixelFormat::ASTC_12x12_Float), VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT);
}

TEST(VulkanPixelFormatTest, ReverseLookupRoundTripsAllCanonicalAndAliasFormats)
{
    for (gp::USize index = 0u; index < kPixelFormatCount; ++index)
    {
        const PixelFormat format = static_cast<PixelFormat>(index);
        const VkFormat linearVk = pixelFormatToVkFormat(format, false);
        const VkFormat srgbVk = pixelFormatToVkFormat(format, true);

        const PixelFormat roundTripLinear = vkFormatToPixelFormat(linearVk);
        const PixelFormat roundTripSrgb = vkFormatToPixelFormat(srgbVk);

        if (format == PixelFormat::A8_UNorm)
        {
            EXPECT_EQ(roundTripLinear, PixelFormat::R8_UNorm);
            EXPECT_EQ(roundTripSrgb, PixelFormat::R8_UNorm);
        }
        else if (format == PixelFormat::X24_G8_UInt)
        {
            EXPECT_EQ(roundTripLinear, PixelFormat::D24_UNorm_S8_UInt);
            EXPECT_EQ(roundTripSrgb, PixelFormat::D24_UNorm_S8_UInt);
        }
        else if (format == PixelFormat::X32_G8_UInt)
        {
            EXPECT_EQ(roundTripLinear, PixelFormat::D32_Float_S8_UInt);
            EXPECT_EQ(roundTripSrgb, PixelFormat::D32_Float_S8_UInt);
        }
        else
        {
            EXPECT_EQ(roundTripLinear, format);
            EXPECT_EQ(roundTripSrgb, format);
        }
    }

    // Vulkan aliases and unmapped fallback formats
    EXPECT_EQ(vkFormatToPixelFormat(VK_FORMAT_A8B8G8R8_UNORM_PACK32), PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(vkFormatToPixelFormat(VK_FORMAT_A8B8G8R8_SRGB_PACK32), PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(vkFormatToPixelFormat(VK_FORMAT_BC1_RGB_UNORM_BLOCK), PixelFormat::BC1_UNorm);
    EXPECT_EQ(vkFormatToPixelFormat(VK_FORMAT_BC1_RGB_SRGB_BLOCK), PixelFormat::BC1_UNorm);
    EXPECT_EQ(vkFormatToPixelFormat(VK_FORMAT_R5G5B5A1_UNORM_PACK16), PixelFormat::B5G5R5A1_UNorm);
    EXPECT_EQ(vkFormatToPixelFormat(VK_FORMAT_X8_D24_UNORM_PACK32), PixelFormat::D24_UNorm_S8_UInt);
    EXPECT_EQ(vkFormatToPixelFormat(VK_FORMAT_D16_UNORM_S8_UINT), PixelFormat::Unknown);
    EXPECT_EQ(vkFormatToPixelFormat(VK_FORMAT_R16G16B16_SFLOAT), PixelFormat::Unknown);
    EXPECT_EQ(vkFormatToPixelFormat(VK_FORMAT_MAX_ENUM), PixelFormat::Unknown);
}

TEST(VulkanPixelFormatTest, SRGBConversionAndLinearNormalization)
{
    const VkFormat linearFormats[] = { VK_FORMAT_B8G8R8A8_UNORM,
                                       VK_FORMAT_A8B8G8R8_UNORM_PACK32,
                                       VK_FORMAT_R8_UNORM,
                                       VK_FORMAT_R8G8_UNORM,
                                       VK_FORMAT_R8G8B8_UNORM,
                                       VK_FORMAT_R8G8B8A8_UNORM,
                                       VK_FORMAT_BC1_RGB_UNORM_BLOCK,
                                       VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
                                       VK_FORMAT_BC2_UNORM_BLOCK,
                                       VK_FORMAT_BC3_UNORM_BLOCK,
                                       VK_FORMAT_BC7_UNORM_BLOCK,
                                       VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,
                                       VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK,
                                       VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_4x4_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_5x4_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_5x5_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_6x5_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_6x6_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_8x5_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_8x6_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_8x8_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_10x5_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_10x6_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_10x8_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_10x10_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_12x10_UNORM_BLOCK,
                                       VK_FORMAT_ASTC_12x12_UNORM_BLOCK };

    for (const VkFormat linear: linearFormats)
    {
        const VkFormat srgb = vkFormatToSRGB(linear, true);
        EXPECT_NE(srgb, linear);
        EXPECT_TRUE(isSRGBVkFormat(srgb));
        EXPECT_FALSE(isSRGBVkFormat(linear));
        EXPECT_EQ(vkFormatToLinear(srgb), linear);
        EXPECT_EQ(vkFormatToLinear(linear), linear);
    }

    // ES3.1 compatibility flag disables R8_SRGB promotion.
    EXPECT_EQ(vkFormatToSRGB(VK_FORMAT_R8_UNORM, false), VK_FORMAT_R8_UNORM);
    EXPECT_EQ(vkFormatToSRGB(VK_FORMAT_R16G16B16A16_SFLOAT, true), VK_FORMAT_R16G16B16A16_SFLOAT);
    EXPECT_EQ(vkFormatToSRGB(VK_FORMAT_D32_SFLOAT, true), VK_FORMAT_D32_SFLOAT);
}

TEST(VulkanPixelFormatTest, ComponentMappingsAndAllFallbackLists)
{
    EXPECT_TRUE(componentMappingEquals(getVulkanFormatComponentMapping(PixelFormat::Unknown), kComponentMappingRGBA));
    EXPECT_TRUE(
        componentMappingEquals(getVulkanFormatComponentMapping(static_cast<PixelFormat>(200u)), kComponentMappingRGBA)
    );
    EXPECT_TRUE(
        componentMappingEquals(getVulkanFormatComponentMapping(PixelFormat::R8G8B8A8_UNorm), kComponentMappingRGBA)
    );
    EXPECT_TRUE(componentMappingEquals(getVulkanFormatComponentMapping(PixelFormat::R8_UNorm), kComponentMappingR001));
    EXPECT_TRUE(componentMappingEquals(getVulkanFormatComponentMapping(PixelFormat::A8_UNorm), kComponentMapping000R));
    EXPECT_TRUE(
        componentMappingEquals(getVulkanFormatComponentMapping(PixelFormat::R8G8_UNorm), kComponentMappingRG01)
    );
    EXPECT_TRUE(
        componentMappingEquals(getVulkanFormatComponentMapping(PixelFormat::R8G8B8_UNorm), kComponentMappingRGB1)
    );
    EXPECT_TRUE(
        componentMappingEquals(getVulkanFormatComponentMapping(PixelFormat::D24_UNorm_S8_UInt), kComponentMappingRIII)
    );
    EXPECT_TRUE(
        componentMappingEquals(getVulkanFormatComponentMapping(PixelFormat::X24_G8_UInt), kComponentMappingRR01)
    );
    EXPECT_EQ(kComponentMappingR000.r, VK_COMPONENT_SWIZZLE_R);
    EXPECT_EQ(kComponentMappingR000.a, VK_COMPONENT_SWIZZLE_ZERO);

    EXPECT_TRUE(getVulkanFormatFallbackList(PixelFormat::Unknown).isEmpty());
    EXPECT_TRUE(getVulkanFormatFallbackList(static_cast<PixelFormat>(kPixelFormatCount)).isEmpty());

    const gp::ConstVectorView<VkFormat> r16Fallbacks = getVulkanFormatFallbackList(PixelFormat::R16_UNorm);
    ASSERT_EQ(r16Fallbacks.size(), 2u);
    EXPECT_EQ(r16Fallbacks[0], VK_FORMAT_R16_UNORM);
    EXPECT_EQ(r16Fallbacks[1], VK_FORMAT_R16_SFLOAT);

    const gp::ConstVectorView<VkFormat> r16g16Fallbacks = getVulkanFormatFallbackList(PixelFormat::R16G16_UNorm);
    ASSERT_EQ(r16g16Fallbacks.size(), 2u);
    EXPECT_EQ(r16g16Fallbacks[0], VK_FORMAT_R16G16_UNORM);
    EXPECT_EQ(r16g16Fallbacks[1], VK_FORMAT_R16G16_SFLOAT);

    const gp::ConstVectorView<VkFormat> rgba16UNormFallbacks =
        getVulkanFormatFallbackList(PixelFormat::R16G16B16A16_UNorm);
    ASSERT_EQ(rgba16UNormFallbacks.size(), 2u);
    EXPECT_EQ(rgba16UNormFallbacks[0], VK_FORMAT_R16G16B16A16_UNORM);
    EXPECT_EQ(rgba16UNormFallbacks[1], VK_FORMAT_R16G16B16A16_SFLOAT);

    const gp::ConstVectorView<VkFormat> rgba16SNormFallbacks =
        getVulkanFormatFallbackList(PixelFormat::R16G16B16A16_SNorm);
    ASSERT_EQ(rgba16SNormFallbacks.size(), 2u);
    EXPECT_EQ(rgba16SNormFallbacks[0], VK_FORMAT_R16G16B16A16_SNORM);
    EXPECT_EQ(rgba16SNormFallbacks[1], VK_FORMAT_R16G16B16A16_SFLOAT);

    const gp::ConstVectorView<VkFormat> r64Fallbacks = getVulkanFormatFallbackList(PixelFormat::R64_UInt);
    ASSERT_EQ(r64Fallbacks.size(), 2u);
    EXPECT_EQ(r64Fallbacks[0], VK_FORMAT_R64_UINT);
    EXPECT_EQ(r64Fallbacks[1], VK_FORMAT_R32G32_UINT);

    const gp::ConstVectorView<VkFormat> b5g5r5a1Fallbacks = getVulkanFormatFallbackList(PixelFormat::B5G5R5A1_UNorm);
    ASSERT_EQ(b5g5r5a1Fallbacks.size(), 3u);
    EXPECT_EQ(b5g5r5a1Fallbacks[0], VK_FORMAT_A1R5G5B5_UNORM_PACK16);
    EXPECT_EQ(b5g5r5a1Fallbacks[1], VK_FORMAT_R5G5B5A1_UNORM_PACK16);
    EXPECT_EQ(b5g5r5a1Fallbacks[2], VK_FORMAT_B8G8R8A8_UNORM);

    const gp::ConstVectorView<VkFormat> r11g11b10Fallbacks = getVulkanFormatFallbackList(PixelFormat::R11G11B10_Float);
    ASSERT_EQ(r11g11b10Fallbacks.size(), 3u);
    EXPECT_EQ(r11g11b10Fallbacks[0], VK_FORMAT_B10G11R11_UFLOAT_PACK32);
    EXPECT_EQ(r11g11b10Fallbacks[1], VK_FORMAT_R16G16B16_SFLOAT);
    EXPECT_EQ(r11g11b10Fallbacks[2], VK_FORMAT_R16G16B16A16_SFLOAT);

    const gp::ConstVectorView<VkFormat> d24Fallbacks = getVulkanFormatFallbackList(PixelFormat::D24_UNorm_S8_UInt);
    ASSERT_EQ(d24Fallbacks.size(), 3u);
    EXPECT_EQ(d24Fallbacks[0], VK_FORMAT_D24_UNORM_S8_UINT);
    EXPECT_EQ(d24Fallbacks[1], VK_FORMAT_D32_SFLOAT_S8_UINT);
    EXPECT_EQ(d24Fallbacks[2], VK_FORMAT_D16_UNORM_S8_UINT);

    const gp::ConstVectorView<VkFormat> x24Fallbacks = getVulkanFormatFallbackList(PixelFormat::X24_G8_UInt);
    ASSERT_EQ(x24Fallbacks.size(), 3u);
    EXPECT_EQ(x24Fallbacks[0], VK_FORMAT_D24_UNORM_S8_UINT);

    const gp::ConstVectorView<VkFormat> d32s8Fallbacks = getVulkanFormatFallbackList(PixelFormat::D32_Float_S8_UInt);
    ASSERT_EQ(d32s8Fallbacks.size(), 3u);
    EXPECT_EQ(d32s8Fallbacks[0], VK_FORMAT_D32_SFLOAT_S8_UINT);
    EXPECT_EQ(d32s8Fallbacks[1], VK_FORMAT_D24_UNORM_S8_UINT);
    EXPECT_EQ(d32s8Fallbacks[2], VK_FORMAT_D16_UNORM_S8_UINT);

    const gp::ConstVectorView<VkFormat> x32Fallbacks = getVulkanFormatFallbackList(PixelFormat::X32_G8_UInt);
    ASSERT_EQ(x32Fallbacks.size(), 3u);
    EXPECT_EQ(x32Fallbacks[0], VK_FORMAT_D32_SFLOAT_S8_UINT);

    const gp::ConstVectorView<VkFormat> rgbaFallbacks = getVulkanFormatFallbackList(PixelFormat::R8G8B8A8_UNorm);
    ASSERT_EQ(rgbaFallbacks.size(), 1u);
    EXPECT_EQ(rgbaFallbacks[0], VK_FORMAT_R8G8B8A8_UNORM);
}

TEST(VulkanPixelFormatTest, AspectFlagsDepthStencilAstcAndBitsPerPixel)
{
    EXPECT_EQ(getVkImageAspectFlags(PixelFormat::Unknown, true, true), 0u);
    EXPECT_EQ(getVkImageAspectFlags(static_cast<PixelFormat>(kPixelFormatCount), true, true), 0u);
    EXPECT_EQ(getVkImageAspectFlags(PixelFormat::R8G8B8A8_UNorm, true, true), VK_IMAGE_ASPECT_COLOR_BIT);
    EXPECT_EQ(getVkImageAspectFlags(PixelFormat::D16_UNorm, true, true), VK_IMAGE_ASPECT_DEPTH_BIT);
    EXPECT_EQ(getVkImageAspectFlags(PixelFormat::D32_Float, true, false), 0u);
    EXPECT_EQ(
        getVkImageAspectFlags(PixelFormat::D24_UNorm_S8_UInt, true, true),
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
    );
    EXPECT_EQ(getVkImageAspectFlags(PixelFormat::D24_UNorm_S8_UInt, false, true), VK_IMAGE_ASPECT_DEPTH_BIT);
    EXPECT_EQ(getVkImageAspectFlags(PixelFormat::D32_Float_S8_UInt, true, false), VK_IMAGE_ASPECT_STENCIL_BIT);
    EXPECT_EQ(getVkImageAspectFlags(PixelFormat::X24_G8_UInt, false, false), VK_IMAGE_ASPECT_STENCIL_BIT);
    EXPECT_EQ(getVkImageAspectFlags(PixelFormat::X32_G8_UInt, false, false), VK_IMAGE_ASPECT_STENCIL_BIT);

    EXPECT_EQ(getVkImageAspectFlags(VK_FORMAT_UNDEFINED, true, true), 0u);
    EXPECT_EQ(getVkImageAspectFlags(VK_FORMAT_R16G16B16A16_SFLOAT, true, true), VK_IMAGE_ASPECT_COLOR_BIT);
    EXPECT_EQ(getVkImageAspectFlags(VK_FORMAT_S8_UINT, true, true), VK_IMAGE_ASPECT_STENCIL_BIT);
    EXPECT_EQ(getVkImageAspectFlags(VK_FORMAT_S8_UINT, false, true), 0u);
    EXPECT_EQ(
        getVkImageAspectFlags(VK_FORMAT_D32_SFLOAT_S8_UINT, true, true),
        static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
    );
    EXPECT_EQ(getVkImageAspectFlags(VK_FORMAT_D32_SFLOAT_S8_UINT, false, true), VK_IMAGE_ASPECT_DEPTH_BIT);
    EXPECT_EQ(getVkImageAspectFlags(VK_FORMAT_D32_SFLOAT_S8_UINT, true, false), VK_IMAGE_ASPECT_STENCIL_BIT);
    EXPECT_EQ(getVkImageAspectFlags(VK_FORMAT_D32_SFLOAT_S8_UINT, false, false), 0u);

    EXPECT_TRUE(vkFormatHasDepth(VK_FORMAT_X8_D24_UNORM_PACK32));
    EXPECT_FALSE(vkFormatHasDepth(VK_FORMAT_S8_UINT));
    EXPECT_TRUE(vkFormatHasStencil(VK_FORMAT_S8_UINT));
    EXPECT_TRUE(vkFormatHasStencil(VK_FORMAT_D16_UNORM_S8_UINT));
    EXPECT_FALSE(vkFormatHasStencil(VK_FORMAT_D16_UNORM));

    // ASTC range boundaries and all 14 sRGB ASTC block sizes
    EXPECT_FALSE(isASTCLDRFormat(static_cast<VkFormat>(VK_FORMAT_ASTC_4x4_UNORM_BLOCK - 1)));
    EXPECT_TRUE(isASTCLDRFormat(VK_FORMAT_ASTC_4x4_UNORM_BLOCK));
    EXPECT_TRUE(isASTCLDRFormat(VK_FORMAT_ASTC_12x12_SRGB_BLOCK));
    EXPECT_FALSE(isASTCLDRFormat(static_cast<VkFormat>(VK_FORMAT_ASTC_12x12_SRGB_BLOCK + 1)));
    EXPECT_FALSE(isASTCLDRFormat(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT));

    const VkFormat allAstcSrgb[] = { VK_FORMAT_ASTC_4x4_SRGB_BLOCK,   VK_FORMAT_ASTC_5x4_SRGB_BLOCK,
                                     VK_FORMAT_ASTC_5x5_SRGB_BLOCK,   VK_FORMAT_ASTC_6x5_SRGB_BLOCK,
                                     VK_FORMAT_ASTC_6x6_SRGB_BLOCK,   VK_FORMAT_ASTC_8x5_SRGB_BLOCK,
                                     VK_FORMAT_ASTC_8x6_SRGB_BLOCK,   VK_FORMAT_ASTC_8x8_SRGB_BLOCK,
                                     VK_FORMAT_ASTC_10x5_SRGB_BLOCK,  VK_FORMAT_ASTC_10x6_SRGB_BLOCK,
                                     VK_FORMAT_ASTC_10x8_SRGB_BLOCK,  VK_FORMAT_ASTC_10x10_SRGB_BLOCK,
                                     VK_FORMAT_ASTC_12x10_SRGB_BLOCK, VK_FORMAT_ASTC_12x12_SRGB_BLOCK };
    for (const VkFormat astcSrgb: allAstcSrgb)
    {
        EXPECT_TRUE(isASTCSRGBFormat(astcSrgb));
        EXPECT_FALSE(isASTCSRGBFormat(vkFormatToLinear(astcSrgb)));
    }

    EXPECT_FALSE(isASTCHDRFormat(static_cast<VkFormat>(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT - 1)));
    EXPECT_TRUE(isASTCHDRFormat(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT));
    EXPECT_TRUE(isASTCHDRFormat(VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT));
    EXPECT_FALSE(isASTCHDRFormat(static_cast<VkFormat>(VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT + 1)));
    EXPECT_FALSE(isASTCHDRFormat(VK_FORMAT_ASTC_12x12_UNORM_BLOCK));

    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_BC1_RGBA_UNORM_BLOCK), 4u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_BC4_UNORM_BLOCK), 4u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_R8_UNORM), 8u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_BC7_SRGB_BLOCK), 8u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_ASTC_4x4_UNORM_BLOCK), 8u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT), 8u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_ASTC_6x6_UNORM_BLOCK), 0u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_ASTC_8x8_UNORM_BLOCK), 0u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_R16_SFLOAT), 16u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_R8G8B8_UNORM), 24u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_R8G8B8A8_UNORM), 32u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_D32_SFLOAT_S8_UINT), 40u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_R16G16B16_SFLOAT), 48u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_R16G16B16A16_SFLOAT), 64u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_R32G32B32_SFLOAT), 96u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_R32G32B32A32_SFLOAT), 128u);
    EXPECT_EQ(getNumBitsPerPixel(VK_FORMAT_UNDEFINED), 0u);
}

TEST(VulkanPixelFormatTest, BufferAndImageCapabilityTranslation)
{
    EXPECT_EQ(translateBufferFormatCapabilities(VK_FORMAT_R32_UINT, 0u), PixelFormatCapabilities::None);

    // Unmapped non-zero buffer feature still advertises generic Buffer capability.
    EXPECT_EQ(
        translateBufferFormatCapabilities(VK_FORMAT_R32_UINT, VK_FORMAT_FEATURE_TRANSFER_SRC_BIT),
        PixelFormatCapabilities::Buffer
    );

    // IndexBuffer requires both BufferLoad and BufferStore on R16_UINT / R32_UINT.
    EXPECT_FALSE(hasPixelFormatCapabilities(
        translateBufferFormatCapabilities(VK_FORMAT_R32_UINT, VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT),
        PixelFormatCapabilities::IndexBuffer
    ));
    EXPECT_FALSE(hasPixelFormatCapabilities(
        translateBufferFormatCapabilities(VK_FORMAT_R16_UINT, VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT),
        PixelFormatCapabilities::IndexBuffer
    ));

    const VkFormatFeatureFlags fullBufferFlags =
        VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT | VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT |
        VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT | VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT;

    const PixelFormatCapabilities r32UintCaps = translateBufferFormatCapabilities(VK_FORMAT_R32_UINT, fullBufferFlags);
    EXPECT_TRUE(hasPixelFormatCapabilities(
        r32UintCaps,
        PixelFormatCapabilities::Buffer | PixelFormatCapabilities::VertexBuffer | PixelFormatCapabilities::BufferLoad |
            PixelFormatCapabilities::BufferStore | PixelFormatCapabilities::BufferAtomics |
            PixelFormatCapabilities::IndexBuffer
    ));

    const PixelFormatCapabilities r16UintCaps = translateBufferFormatCapabilities(
        VK_FORMAT_R16_UINT, VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT | VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT
    );
    EXPECT_TRUE(hasPixelFormatCapabilities(r16UintCaps, PixelFormatCapabilities::IndexBuffer));

    const PixelFormatCapabilities r32FloatCaps =
        translateBufferFormatCapabilities(VK_FORMAT_R32_SFLOAT, fullBufferFlags);
    EXPECT_FALSE(hasPixelFormatCapabilities(r32FloatCaps, PixelFormatCapabilities::IndexBuffer));

    // Empty and buffer-only VkFormatProperties yield no image capabilities.
    const VkFormatProperties emptyProps{};
    EXPECT_EQ(translateImageFormatCapabilities(emptyProps), PixelFormatCapabilities::None);

    VkFormatProperties bufferOnlyProps{};
    bufferOnlyProps.bufferFeatures = fullBufferFlags;
    EXPECT_EQ(translateImageFormatCapabilities(bufferOnlyProps), PixelFormatCapabilities::None);

    // Optimal-only attachment bits must ignore linearTilingFeatures.
    VkFormatProperties linearOnlyProps{};
    linearOnlyProps.linearTilingFeatures =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

    const PixelFormatCapabilities linearCaps = translateImageFormatCapabilities(linearOnlyProps);
    EXPECT_TRUE(hasPixelFormatCapabilities(
        linearCaps,
        PixelFormatCapabilities::AnyTexture | PixelFormatCapabilities::TextureSample |
            PixelFormatCapabilities::TextureLoad | PixelFormatCapabilities::AllUAVFlags |
            PixelFormatCapabilities::TextureStore | PixelFormatCapabilities::TextureAtomics |
            PixelFormatCapabilities::TextureFilterable
    ));
    EXPECT_FALSE(hasPixelFormatCapabilities(linearCaps, PixelFormatCapabilities::RenderTarget));
    EXPECT_FALSE(hasPixelFormatCapabilities(linearCaps, PixelFormatCapabilities::TextureBlendable));
    EXPECT_FALSE(hasPixelFormatCapabilities(linearCaps, PixelFormatCapabilities::DepthStencil));

    // Optimal tiling enables both attachment bits and general image sampling/storage/filtering bits.
    VkFormatProperties optimalProps{};
    optimalProps.optimalTilingFeatures =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    const PixelFormatCapabilities optimalCaps = translateImageFormatCapabilities(optimalProps);
    EXPECT_TRUE(hasPixelFormatCapabilities(
        optimalCaps,
        PixelFormatCapabilities::AnyTexture | PixelFormatCapabilities::TextureSample |
            PixelFormatCapabilities::TextureLoad | PixelFormatCapabilities::RenderTarget |
            PixelFormatCapabilities::TextureBlendable | PixelFormatCapabilities::DepthStencil |
            PixelFormatCapabilities::AllUAVFlags | PixelFormatCapabilities::TextureStore |
            PixelFormatCapabilities::TextureAtomics | PixelFormatCapabilities::TextureFilterable
    ));
}

}   // namespace gp::rhi::vulkan::tests
