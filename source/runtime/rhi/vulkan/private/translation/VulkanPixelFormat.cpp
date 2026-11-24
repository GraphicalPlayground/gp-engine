// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "translation/VulkanPixelFormat.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIPixelFormat.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

inline constexpr gp::Array<VkFormat, 2u> kFallbackR16UNorm{ VK_FORMAT_R16_UNORM, VK_FORMAT_R16_SFLOAT };
inline constexpr gp::Array<VkFormat, 2u> kFallbackR16G16UNorm{ VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16_SFLOAT };
inline constexpr gp::Array<VkFormat, 2u> kFallbackR16G16B16A16UNorm{ VK_FORMAT_R16G16B16A16_UNORM,
                                                                     VK_FORMAT_R16G16B16A16_SFLOAT };
inline constexpr gp::Array<VkFormat, 2u> kFallbackR16G16B16A16SNorm{ VK_FORMAT_R16G16B16A16_SNORM,
                                                                     VK_FORMAT_R16G16B16A16_SFLOAT };
inline constexpr gp::Array<VkFormat, 2u> kFallbackR64UInt{ VK_FORMAT_R64_UINT, VK_FORMAT_R32G32_UINT };
inline constexpr gp::Array<VkFormat, 3u> kFallbackB5G5R5A1UNorm{ VK_FORMAT_A1R5G5B5_UNORM_PACK16,
                                                                 VK_FORMAT_R5G5B5A1_UNORM_PACK16,
                                                                 VK_FORMAT_B8G8R8A8_UNORM };
inline constexpr gp::Array<VkFormat, 3u> kFallbackR11G11B10Float{ VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                                                                  VK_FORMAT_R16G16B16_SFLOAT,
                                                                  VK_FORMAT_R16G16B16A16_SFLOAT };
inline constexpr gp::Array<VkFormat, 3u> kFallbackD24S8{ VK_FORMAT_D24_UNORM_S8_UINT,
                                                         VK_FORMAT_D32_SFLOAT_S8_UINT,
                                                         VK_FORMAT_D16_UNORM_S8_UINT };
inline constexpr gp::Array<VkFormat, 3u> kFallbackD32S8{ VK_FORMAT_D32_SFLOAT_S8_UINT,
                                                         VK_FORMAT_D24_UNORM_S8_UINT,
                                                         VK_FORMAT_D16_UNORM_S8_UINT };

}   // namespace

PixelFormat vkFormatToPixelFormat(const VkFormat format) noexcept
{
    switch (format)
    {
    case VK_FORMAT_UNDEFINED:
        return PixelFormat::Unknown;

    // 8-bit uncompressed
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SRGB:
        return PixelFormat::R8_UNorm;
    case VK_FORMAT_R8_SNORM:
        return PixelFormat::R8_SNorm;
    case VK_FORMAT_R8_UINT:
        return PixelFormat::R8_UInt;
    case VK_FORMAT_R8_SINT:
        return PixelFormat::R8_SInt;

    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SRGB:
        return PixelFormat::R8G8_UNorm;
    case VK_FORMAT_R8G8_SNORM:
        return PixelFormat::R8G8_SNorm;
    case VK_FORMAT_R8G8_UINT:
        return PixelFormat::R8G8_UInt;
    case VK_FORMAT_R8G8_SINT:
        return PixelFormat::R8G8_SInt;

    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
        return PixelFormat::R8G8B8_UNorm;

    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        return PixelFormat::R8G8B8A8_UNorm;
    case VK_FORMAT_R8G8B8A8_SNORM:
        return PixelFormat::R8G8B8A8_SNorm;
    case VK_FORMAT_R8G8B8A8_UINT:
        return PixelFormat::R8G8B8A8_UInt;
    case VK_FORMAT_R8G8B8A8_SINT:
        return PixelFormat::R8G8B8A8_SInt;

    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        return PixelFormat::B8G8R8A8_UNorm;

    // 16-bit uncompressed
    case VK_FORMAT_R16_UNORM:
        return PixelFormat::R16_UNorm;
    case VK_FORMAT_R16_SNORM:
        return PixelFormat::R16_SNorm;
    case VK_FORMAT_R16_UINT:
        return PixelFormat::R16_UInt;
    case VK_FORMAT_R16_SINT:
        return PixelFormat::R16_SInt;
    case VK_FORMAT_R16_SFLOAT:
        return PixelFormat::R16_Float;

    case VK_FORMAT_R16G16_UNORM:
        return PixelFormat::R16G16_UNorm;
    case VK_FORMAT_R16G16_SNORM:
        return PixelFormat::R16G16_SNorm;
    case VK_FORMAT_R16G16_UINT:
        return PixelFormat::R16G16_UInt;
    case VK_FORMAT_R16G16_SINT:
        return PixelFormat::R16G16_SInt;
    case VK_FORMAT_R16G16_SFLOAT:
        return PixelFormat::R16G16_Float;

    case VK_FORMAT_R16G16B16A16_UNORM:
        return PixelFormat::R16G16B16A16_UNorm;
    case VK_FORMAT_R16G16B16A16_SNORM:
        return PixelFormat::R16G16B16A16_SNorm;
    case VK_FORMAT_R16G16B16A16_UINT:
        return PixelFormat::R16G16B16A16_UInt;
    case VK_FORMAT_R16G16B16A16_SINT:
        return PixelFormat::R16G16B16A16_SInt;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return PixelFormat::R16G16B16A16_Float;

    // 32-bit and 64-bit uncompressed
    case VK_FORMAT_R32_UINT:
        return PixelFormat::R32_UInt;
    case VK_FORMAT_R32_SINT:
        return PixelFormat::R32_SInt;
    case VK_FORMAT_R32_SFLOAT:
        return PixelFormat::R32_Float;

    case VK_FORMAT_R32G32_UINT:
        return PixelFormat::R32G32_UInt;
    case VK_FORMAT_R32G32_SINT:
        return PixelFormat::R32G32_SInt;
    case VK_FORMAT_R32G32_SFLOAT:
        return PixelFormat::R32G32_Float;

    case VK_FORMAT_R32G32B32_UINT:
        return PixelFormat::R32G32B32_UInt;
    case VK_FORMAT_R32G32B32_SINT:
        return PixelFormat::R32G32B32_SInt;
    case VK_FORMAT_R32G32B32_SFLOAT:
        return PixelFormat::R32G32B32_Float;

    case VK_FORMAT_R32G32B32A32_UINT:
        return PixelFormat::R32G32B32A32_UInt;
    case VK_FORMAT_R32G32B32A32_SINT:
        return PixelFormat::R32G32B32A32_SInt;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return PixelFormat::R32G32B32A32_Float;

    case VK_FORMAT_R64_UINT:
        return PixelFormat::R64_UInt;

    // Packed and HDR special color
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
        return PixelFormat::B5G6R5_UNorm;
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
        return PixelFormat::B5G5R5A1_UNorm;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        return PixelFormat::R10G10B10A2_UNorm;
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        return PixelFormat::B10G10R10A2_UNorm;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        return PixelFormat::R11G11B10_Float;
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
        return PixelFormat::R9G9B9E5_SharedExp;

    // Depth and stencil
    case VK_FORMAT_D16_UNORM:
        return PixelFormat::D16_UNorm;
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return PixelFormat::D24_UNorm_S8_UInt;
    case VK_FORMAT_D32_SFLOAT:
        return PixelFormat::D32_Float;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return PixelFormat::D32_Float_S8_UInt;

    // BCn / DXTC block-compressed
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        return PixelFormat::BC1_UNorm;
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
        return PixelFormat::BC2_UNorm;
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
        return PixelFormat::BC3_UNorm;
    case VK_FORMAT_BC4_UNORM_BLOCK:
        return PixelFormat::BC4_UNorm;
    case VK_FORMAT_BC5_UNORM_BLOCK:
        return PixelFormat::BC5_UNorm;
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        return PixelFormat::BC6H_UFloat;
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        return PixelFormat::BC6H_SFloat;
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return PixelFormat::BC7_UNorm;

    // ETC2 / EAC block-compressed
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        return PixelFormat::ETC2_RGB_UNorm;
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        return PixelFormat::ETC2_RGBA_UNorm;
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
        return PixelFormat::ETC2_R11_EAC;
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
        return PixelFormat::ETC2_RG11_EAC;

    // ASTC block-compressed (LDR + HDR)
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        return PixelFormat::ASTC_4x4_UNorm;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        return PixelFormat::ASTC_6x6_UNorm;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        return PixelFormat::ASTC_8x8_UNorm;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
        return PixelFormat::ASTC_10x10_UNorm;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
        return PixelFormat::ASTC_12x12_UNorm;
    case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT:
        return PixelFormat::ASTC_4x4_Float;
    case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT:
        return PixelFormat::ASTC_6x6_Float;
    case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT:
        return PixelFormat::ASTC_8x8_Float;
    case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT:
        return PixelFormat::ASTC_10x10_Float;
    case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT:
        return PixelFormat::ASTC_12x12_Float;

    default:
        return PixelFormat::Unknown;
    }
}

gp::ConstVectorView<VkFormat> getVulkanFormatFallbackList(const PixelFormat format) noexcept
{
    const gp::USize index = static_cast<gp::USize>(format);
    if (index == 0u || index >= kPixelFormatCount)
    {
        return {};
    }

    switch (format)
    {
    case PixelFormat::R16_UNorm:
        return gp::ConstVectorView<VkFormat>(kFallbackR16UNorm.data(), kFallbackR16UNorm.size());
    case PixelFormat::R16G16_UNorm:
        return gp::ConstVectorView<VkFormat>(kFallbackR16G16UNorm.data(), kFallbackR16G16UNorm.size());
    case PixelFormat::R16G16B16A16_UNorm:
        return gp::ConstVectorView<VkFormat>(kFallbackR16G16B16A16UNorm.data(), kFallbackR16G16B16A16UNorm.size());
    case PixelFormat::R16G16B16A16_SNorm:
        return gp::ConstVectorView<VkFormat>(kFallbackR16G16B16A16SNorm.data(), kFallbackR16G16B16A16SNorm.size());
    case PixelFormat::R64_UInt:
        return gp::ConstVectorView<VkFormat>(kFallbackR64UInt.data(), kFallbackR64UInt.size());
    case PixelFormat::B5G5R5A1_UNorm:
        return gp::ConstVectorView<VkFormat>(kFallbackB5G5R5A1UNorm.data(), kFallbackB5G5R5A1UNorm.size());
    case PixelFormat::R11G11B10_Float:
        return gp::ConstVectorView<VkFormat>(kFallbackR11G11B10Float.data(), kFallbackR11G11B10Float.size());
    case PixelFormat::D24_UNorm_S8_UInt:
    case PixelFormat::X24_G8_UInt:
        return gp::ConstVectorView<VkFormat>(kFallbackD24S8.data(), kFallbackD24S8.size());
    case PixelFormat::D32_Float_S8_UInt:
    case PixelFormat::X32_G8_UInt:
        return gp::ConstVectorView<VkFormat>(kFallbackD32S8.data(), kFallbackD32S8.size());
    default:
        return gp::ConstVectorView<VkFormat>(&kVulkanPixelFormatTable[index].linearFormat, 1u);
    }
}

gp::UInt32 getNumBitsPerPixel(const VkFormat format) noexcept
{
    switch (format)
    {
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
    case VK_FORMAT_BC4_UNORM_BLOCK:
    case VK_FORMAT_BC4_SNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
        return 4u;

    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R8_SRGB:
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_BC2_UNORM_BLOCK:
    case VK_FORMAT_BC2_SRGB_BLOCK:
    case VK_FORMAT_BC3_UNORM_BLOCK:
    case VK_FORMAT_BC3_SRGB_BLOCK:
    case VK_FORMAT_BC5_UNORM_BLOCK:
    case VK_FORMAT_BC5_SNORM_BLOCK:
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
    case VK_FORMAT_BC7_UNORM_BLOCK:
    case VK_FORMAT_BC7_SRGB_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT:
        return 8u;

    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8G8_SRGB:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
        return 16u;

    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_D16_UNORM_S8_UINT:
        return 24u;

    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
        return 32u;

    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return 40u;

    case VK_FORMAT_R16G16B16_UNORM:
    case VK_FORMAT_R16G16B16_SNORM:
    case VK_FORMAT_R16G16B16_UINT:
    case VK_FORMAT_R16G16B16_SINT:
    case VK_FORMAT_R16G16B16_SFLOAT:
        return 48u;

    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R64_UINT:
    case VK_FORMAT_R64_SINT:
        return 64u;

    case VK_FORMAT_R32G32B32_UINT:
    case VK_FORMAT_R32G32B32_SINT:
    case VK_FORMAT_R32G32B32_SFLOAT:
        return 96u;

    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return 128u;

    default:
        return 0u;
    }
}

PixelFormatCapabilities
    translateBufferFormatCapabilities(const VkFormat vulkanFormat, const VkFormatFeatureFlags bufferFeatures) noexcept
{
    if (bufferFeatures == 0u)
    {
        return PixelFormatCapabilities::None;
    }

    PixelFormatCapabilities capabilities = PixelFormatCapabilities::Buffer;

    if ((bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) != 0u)
    {
        capabilities |= PixelFormatCapabilities::VertexBuffer;
    }
    if ((bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT) != 0u)
    {
        capabilities |= PixelFormatCapabilities::BufferLoad;
    }
    if ((bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT) != 0u)
    {
        capabilities |= PixelFormatCapabilities::BufferStore;
    }
    if ((bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT) != 0u)
    {
        capabilities |= PixelFormatCapabilities::BufferAtomics;
    }

    if (hasPixelFormatCapabilities(
            capabilities, PixelFormatCapabilities::BufferLoad | PixelFormatCapabilities::BufferStore
        ) &&
        (vulkanFormat == VK_FORMAT_R16_UINT || vulkanFormat == VK_FORMAT_R32_UINT))
    {
        capabilities |= PixelFormatCapabilities::IndexBuffer;
    }

    return capabilities;
}

PixelFormatCapabilities translateImageFormatCapabilities(const VkFormatProperties& properties) noexcept
{
    PixelFormatCapabilities capabilities = PixelFormatCapabilities::None;

    const auto mapFeature =
        [&capabilities, &properties](
            const PixelFormatCapabilities capability, const VkFormatFeatureFlags flag, const bool optimalTilingOnly
        ) noexcept -> void
    {
        bool supported = (properties.optimalTilingFeatures & flag) == flag;
        if (!optimalTilingOnly)
        {
            supported = supported || ((properties.linearTilingFeatures & flag) == flag);
        }

        if (supported)
        {
            capabilities |= capability;
        }
    };

    mapFeature(
        PixelFormatCapabilities::AnyTexture | PixelFormatCapabilities::TextureSample |
            PixelFormatCapabilities::TextureLoad,
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,
        false
    );
    mapFeature(PixelFormatCapabilities::DepthStencil, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, true);
    mapFeature(PixelFormatCapabilities::RenderTarget, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT, true);
    mapFeature(PixelFormatCapabilities::TextureBlendable, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT, true);
    mapFeature(
        PixelFormatCapabilities::AllUAVFlags | PixelFormatCapabilities::TextureStore,
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
        false
    );
    mapFeature(PixelFormatCapabilities::TextureAtomics, VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT, false);
    mapFeature(PixelFormatCapabilities::TextureFilterable, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, false);

    return capabilities;
}

}   // namespace gp::rhi::vulkan
