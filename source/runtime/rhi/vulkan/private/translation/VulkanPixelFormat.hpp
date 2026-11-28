// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIPixelFormat.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Standard RGBA identity component swizzle (`R, G, B, A`).
inline constexpr VkComponentMapping kComponentMappingRGBA{
    VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A
};

/// @brief RGB color swizzle with constant alpha of 1 (`R, G, B, 1`).
inline constexpr VkComponentMapping kComponentMappingRGB1{
    VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_ONE
};

/// @brief Two-channel RG swizzle with blue zero and alpha 1 (`R, G, 0, 1`).
inline constexpr VkComponentMapping kComponentMappingRG01{
    VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE
};

/// @brief Single-channel red swizzle with green/blue zero and alpha 1 (`R, 0, 0, 1`).
inline constexpr VkComponentMapping kComponentMappingR001{
    VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE
};

/// @brief Depth-buffer swizzle reading red and preserving identity on remaining channels (`R, I, I, I`).
inline constexpr VkComponentMapping kComponentMappingRIII{
    VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
};

/// @brief Alpha-only format swizzle routing the stored red byte into alpha (`0, 0, 0, R`).
inline constexpr VkComponentMapping kComponentMapping000R{
    VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_R
};

/// @brief Single-channel red swizzle with remaining channels zeroed (`R, 0, 0, 0`).
inline constexpr VkComponentMapping kComponentMappingR000{
    VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ZERO
};

/// @brief Stencil-view swizzle replicating the sampled channel into red and green (`R, R, 0, 1`).
inline constexpr VkComponentMapping kComponentMappingRR01{
    VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_ZERO, VK_COMPONENT_SWIZZLE_ONE
};

/// @brief Immutable Vulkan format descriptor entry paired with an engine `PixelFormat`.
struct VulkanPixelFormatEntry final
{
    /// @brief Engine pixel format described by this entry.
    PixelFormat format{ PixelFormat::Unknown };

    /// @brief Canonical linear `VkFormat` for `format`.
    VkFormat linearFormat{ VK_FORMAT_UNDEFINED };

    /// @brief Hardware sRGB `VkFormat` when supported, or `linearFormat` when no sRGB variant exists.
    VkFormat srgbFormat{ VK_FORMAT_UNDEFINED };

    /// @brief Default `VkComponentMapping` applied when creating sampled image views for `format`.
    VkComponentMapping componentMapping{ kComponentMappingRGBA };
};

// clang-format off
/// @brief Compile-time lookup table mapping every `PixelFormat` enumerator to its Vulkan linear format, sRGB variant,
///        and default view component swizzle.
inline constexpr gp::Array<VulkanPixelFormatEntry, kPixelFormatCount> kVulkanPixelFormatTable{
    VulkanPixelFormatEntry{ PixelFormat::Unknown,            VK_FORMAT_UNDEFINED,                    VK_FORMAT_UNDEFINED,                  kComponentMappingRGBA },

    // 8-bit uncompressed formats
    VulkanPixelFormatEntry{ PixelFormat::R8_UNorm,           VK_FORMAT_R8_UNORM,                     VK_FORMAT_R8_SRGB,                    kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::R8_SNorm,           VK_FORMAT_R8_SNORM,                     VK_FORMAT_R8_SNORM,                   kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::R8_UInt,            VK_FORMAT_R8_UINT,                      VK_FORMAT_R8_UINT,                    kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::R8_SInt,            VK_FORMAT_R8_SINT,                      VK_FORMAT_R8_SINT,                    kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::A8_UNorm,           VK_FORMAT_R8_UNORM,                     VK_FORMAT_R8_UNORM,                   kComponentMapping000R },

    VulkanPixelFormatEntry{ PixelFormat::R8G8_UNorm,         VK_FORMAT_R8G8_UNORM,                   VK_FORMAT_R8G8_SRGB,                  kComponentMappingRG01 },
    VulkanPixelFormatEntry{ PixelFormat::R8G8_SNorm,         VK_FORMAT_R8G8_SNORM,                   VK_FORMAT_R8G8_SNORM,                 kComponentMappingRG01 },
    VulkanPixelFormatEntry{ PixelFormat::R8G8_UInt,          VK_FORMAT_R8G8_UINT,                    VK_FORMAT_R8G8_UINT,                  kComponentMappingRG01 },
    VulkanPixelFormatEntry{ PixelFormat::R8G8_SInt,          VK_FORMAT_R8G8_SINT,                    VK_FORMAT_R8G8_SINT,                  kComponentMappingRG01 },

    VulkanPixelFormatEntry{ PixelFormat::R8G8B8_UNorm,       VK_FORMAT_R8G8B8_UNORM,                 VK_FORMAT_R8G8B8_SRGB,                kComponentMappingRGB1 },

    VulkanPixelFormatEntry{ PixelFormat::R8G8B8A8_UNorm,     VK_FORMAT_R8G8B8A8_UNORM,               VK_FORMAT_R8G8B8A8_SRGB,              kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R8G8B8A8_SNorm,     VK_FORMAT_R8G8B8A8_SNORM,               VK_FORMAT_R8G8B8A8_SNORM,             kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R8G8B8A8_UInt,      VK_FORMAT_R8G8B8A8_UINT,                VK_FORMAT_R8G8B8A8_UINT,              kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R8G8B8A8_SInt,      VK_FORMAT_R8G8B8A8_SINT,                VK_FORMAT_R8G8B8A8_SINT,              kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::B8G8R8A8_UNorm,     VK_FORMAT_B8G8R8A8_UNORM,               VK_FORMAT_B8G8R8A8_SRGB,              kComponentMappingRGBA },

    // 16-bit uncompressed formats
    VulkanPixelFormatEntry{ PixelFormat::R16_UNorm,          VK_FORMAT_R16_UNORM,                    VK_FORMAT_R16_UNORM,                  kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::R16_SNorm,          VK_FORMAT_R16_SNORM,                    VK_FORMAT_R16_SNORM,                  kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::R16_UInt,           VK_FORMAT_R16_UINT,                     VK_FORMAT_R16_UINT,                   kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::R16_SInt,           VK_FORMAT_R16_SINT,                     VK_FORMAT_R16_SINT,                   kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::R16_Float,          VK_FORMAT_R16_SFLOAT,                   VK_FORMAT_R16_SFLOAT,                 kComponentMappingR001 },

    VulkanPixelFormatEntry{ PixelFormat::R16G16_UNorm,       VK_FORMAT_R16G16_UNORM,                 VK_FORMAT_R16G16_UNORM,               kComponentMappingRG01 },
    VulkanPixelFormatEntry{ PixelFormat::R16G16_SNorm,       VK_FORMAT_R16G16_SNORM,                 VK_FORMAT_R16G16_SNORM,               kComponentMappingRG01 },
    VulkanPixelFormatEntry{ PixelFormat::R16G16_UInt,        VK_FORMAT_R16G16_UINT,                  VK_FORMAT_R16G16_UINT,                kComponentMappingRG01 },
    VulkanPixelFormatEntry{ PixelFormat::R16G16_SInt,        VK_FORMAT_R16G16_SINT,                  VK_FORMAT_R16G16_SINT,                kComponentMappingRG01 },
    VulkanPixelFormatEntry{ PixelFormat::R16G16_Float,       VK_FORMAT_R16G16_SFLOAT,                VK_FORMAT_R16G16_SFLOAT,              kComponentMappingRG01 },

    VulkanPixelFormatEntry{ PixelFormat::R16G16B16A16_UNorm, VK_FORMAT_R16G16B16A16_UNORM,           VK_FORMAT_R16G16B16A16_UNORM,         kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R16G16B16A16_SNorm, VK_FORMAT_R16G16B16A16_SNORM,           VK_FORMAT_R16G16B16A16_SNORM,         kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R16G16B16A16_UInt,  VK_FORMAT_R16G16B16A16_UINT,            VK_FORMAT_R16G16B16A16_UINT,          kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R16G16B16A16_SInt,  VK_FORMAT_R16G16B16A16_SINT,            VK_FORMAT_R16G16B16A16_SINT,          kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R16G16B16A16_Float, VK_FORMAT_R16G16B16A16_SFLOAT,          VK_FORMAT_R16G16B16A16_SFLOAT,        kComponentMappingRGBA },

    // 32-bit and 64-bit uncompressed formats
    VulkanPixelFormatEntry{ PixelFormat::R32_UInt,           VK_FORMAT_R32_UINT,                     VK_FORMAT_R32_UINT,                   kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::R32_SInt,           VK_FORMAT_R32_SINT,                     VK_FORMAT_R32_SINT,                   kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::R32_Float,          VK_FORMAT_R32_SFLOAT,                   VK_FORMAT_R32_SFLOAT,                 kComponentMappingR001 },

    VulkanPixelFormatEntry{ PixelFormat::R32G32_UInt,        VK_FORMAT_R32G32_UINT,                  VK_FORMAT_R32G32_UINT,                kComponentMappingRG01 },
    VulkanPixelFormatEntry{ PixelFormat::R32G32_SInt,        VK_FORMAT_R32G32_SINT,                  VK_FORMAT_R32G32_SINT,                kComponentMappingRG01 },
    VulkanPixelFormatEntry{ PixelFormat::R32G32_Float,       VK_FORMAT_R32G32_SFLOAT,                VK_FORMAT_R32G32_SFLOAT,              kComponentMappingRG01 },

    VulkanPixelFormatEntry{ PixelFormat::R32G32B32_UInt,     VK_FORMAT_R32G32B32_UINT,               VK_FORMAT_R32G32B32_UINT,             kComponentMappingRGB1 },
    VulkanPixelFormatEntry{ PixelFormat::R32G32B32_SInt,     VK_FORMAT_R32G32B32_SINT,               VK_FORMAT_R32G32B32_SINT,             kComponentMappingRGB1 },
    VulkanPixelFormatEntry{ PixelFormat::R32G32B32_Float,    VK_FORMAT_R32G32B32_SFLOAT,             VK_FORMAT_R32G32B32_SFLOAT,           kComponentMappingRGB1 },

    VulkanPixelFormatEntry{ PixelFormat::R32G32B32A32_UInt,  VK_FORMAT_R32G32B32A32_UINT,            VK_FORMAT_R32G32B32A32_UINT,          kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R32G32B32A32_SInt,  VK_FORMAT_R32G32B32A32_SINT,            VK_FORMAT_R32G32B32A32_SINT,          kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R32G32B32A32_Float, VK_FORMAT_R32G32B32A32_SFLOAT,          VK_FORMAT_R32G32B32A32_SFLOAT,        kComponentMappingRGBA },

    VulkanPixelFormatEntry{ PixelFormat::R64_UInt,           VK_FORMAT_R64_UINT,                     VK_FORMAT_R64_UINT,                   kComponentMappingR001 },

    // Packed and HDR special color formats
    VulkanPixelFormatEntry{ PixelFormat::B5G6R5_UNorm,       VK_FORMAT_R5G6B5_UNORM_PACK16,          VK_FORMAT_R5G6B5_UNORM_PACK16,        kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::B5G5R5A1_UNorm,     VK_FORMAT_A1R5G5B5_UNORM_PACK16,        VK_FORMAT_A1R5G5B5_UNORM_PACK16,      kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R10G10B10A2_UNorm,  VK_FORMAT_A2B10G10R10_UNORM_PACK32,     VK_FORMAT_A2B10G10R10_UNORM_PACK32,   kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::B10G10R10A2_UNorm,  VK_FORMAT_A2R10G10B10_UNORM_PACK32,     VK_FORMAT_A2R10G10B10_UNORM_PACK32,   kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::R11G11B10_Float,    VK_FORMAT_B10G11R11_UFLOAT_PACK32,      VK_FORMAT_B10G11R11_UFLOAT_PACK32,    kComponentMappingRGB1 },
    VulkanPixelFormatEntry{ PixelFormat::R9G9B9E5_SharedExp, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,       VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,     kComponentMappingRGB1 },

    // Depth and stencil formats
    VulkanPixelFormatEntry{ PixelFormat::D16_UNorm,          VK_FORMAT_D16_UNORM,                    VK_FORMAT_D16_UNORM,                  kComponentMappingRIII },
    VulkanPixelFormatEntry{ PixelFormat::D24_UNorm_S8_UInt,  VK_FORMAT_D24_UNORM_S8_UINT,            VK_FORMAT_D24_UNORM_S8_UINT,          kComponentMappingRIII },
    VulkanPixelFormatEntry{ PixelFormat::D32_Float,          VK_FORMAT_D32_SFLOAT,                   VK_FORMAT_D32_SFLOAT,                 kComponentMappingRIII },
    VulkanPixelFormatEntry{ PixelFormat::D32_Float_S8_UInt,  VK_FORMAT_D32_SFLOAT_S8_UINT,           VK_FORMAT_D32_SFLOAT_S8_UINT,         kComponentMappingRIII },
    VulkanPixelFormatEntry{ PixelFormat::X24_G8_UInt,        VK_FORMAT_D24_UNORM_S8_UINT,            VK_FORMAT_D24_UNORM_S8_UINT,          kComponentMappingRR01 },
    VulkanPixelFormatEntry{ PixelFormat::X32_G8_UInt,        VK_FORMAT_D32_SFLOAT_S8_UINT,           VK_FORMAT_D32_SFLOAT_S8_UINT,         kComponentMappingRR01 },

    // BCn / DXTC block-compressed formats
    VulkanPixelFormatEntry{ PixelFormat::BC1_UNorm,          VK_FORMAT_BC1_RGBA_UNORM_BLOCK,         VK_FORMAT_BC1_RGBA_SRGB_BLOCK,        kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::BC2_UNorm,          VK_FORMAT_BC2_UNORM_BLOCK,              VK_FORMAT_BC2_SRGB_BLOCK,             kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::BC3_UNorm,          VK_FORMAT_BC3_UNORM_BLOCK,              VK_FORMAT_BC3_SRGB_BLOCK,             kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::BC4_UNorm,          VK_FORMAT_BC4_UNORM_BLOCK,              VK_FORMAT_BC4_UNORM_BLOCK,            kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::BC5_UNorm,          VK_FORMAT_BC5_UNORM_BLOCK,              VK_FORMAT_BC5_UNORM_BLOCK,            kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::BC6H_UFloat,        VK_FORMAT_BC6H_UFLOAT_BLOCK,            VK_FORMAT_BC6H_UFLOAT_BLOCK,          kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::BC6H_SFloat,        VK_FORMAT_BC6H_SFLOAT_BLOCK,            VK_FORMAT_BC6H_SFLOAT_BLOCK,          kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::BC7_UNorm,          VK_FORMAT_BC7_UNORM_BLOCK,              VK_FORMAT_BC7_SRGB_BLOCK,             kComponentMappingRGBA },

    // ETC2 / EAC block-compressed formats
    VulkanPixelFormatEntry{ PixelFormat::ETC2_RGB_UNorm,     VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK,      VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK,     kComponentMappingRGB1 },
    VulkanPixelFormatEntry{ PixelFormat::ETC2_RGBA_UNorm,    VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK,    VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK,   kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::ETC2_R11_EAC,       VK_FORMAT_EAC_R11_UNORM_BLOCK,          VK_FORMAT_EAC_R11_UNORM_BLOCK,        kComponentMappingR001 },
    VulkanPixelFormatEntry{ PixelFormat::ETC2_RG11_EAC,      VK_FORMAT_EAC_R11G11_UNORM_BLOCK,       VK_FORMAT_EAC_R11G11_UNORM_BLOCK,     kComponentMappingRG01 },

    // ASTC block-compressed formats (LDR + HDR)
    VulkanPixelFormatEntry{ PixelFormat::ASTC_4x4_UNorm,     VK_FORMAT_ASTC_4x4_UNORM_BLOCK,         VK_FORMAT_ASTC_4x4_SRGB_BLOCK,        kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::ASTC_6x6_UNorm,     VK_FORMAT_ASTC_6x6_UNORM_BLOCK,         VK_FORMAT_ASTC_6x6_SRGB_BLOCK,        kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::ASTC_8x8_UNorm,     VK_FORMAT_ASTC_8x8_UNORM_BLOCK,         VK_FORMAT_ASTC_8x8_SRGB_BLOCK,        kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::ASTC_10x10_UNorm,   VK_FORMAT_ASTC_10x10_UNORM_BLOCK,       VK_FORMAT_ASTC_10x10_SRGB_BLOCK,      kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::ASTC_12x12_UNorm,   VK_FORMAT_ASTC_12x12_UNORM_BLOCK,       VK_FORMAT_ASTC_12x12_SRGB_BLOCK,      kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::ASTC_4x4_Float,     VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT,    VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT,  kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::ASTC_6x6_Float,     VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT,    VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT,  kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::ASTC_8x8_Float,     VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT,    VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT,  kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::ASTC_10x10_Float,   VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT,  VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT, kComponentMappingRGBA },
    VulkanPixelFormatEntry{ PixelFormat::ASTC_12x12_Float,   VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT,  VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT, kComponentMappingRGBA }
};
// clang-format on

namespace detail
{

/// @brief Compile-time structural verifier ensuring `kVulkanPixelFormatTable` stays aligned with `PixelFormat`.
/// @return True when every slot matches its `PixelFormat` enumerator and every non-unknown format maps cleanly.
[[nodiscard]] consteval bool validateVulkanPixelFormatTable() noexcept
{
    for (gp::USize index = 0u; index < kPixelFormatCount; ++index)
    {
        const VulkanPixelFormatEntry& entry = kVulkanPixelFormatTable[index];
        if (entry.format != static_cast<PixelFormat>(index))
        {
            return false;
        }

        if (index == 0u)
        {
            if (entry.linearFormat != VK_FORMAT_UNDEFINED || entry.srgbFormat != VK_FORMAT_UNDEFINED)
            {
                return false;
            }
        }
        else
        {
            if (entry.linearFormat == VK_FORMAT_UNDEFINED || entry.srgbFormat == VK_FORMAT_UNDEFINED)
            {
                return false;
            }
        }
    }

    return true;
}

static_assert(
    validateVulkanPixelFormatTable(),
    "kVulkanPixelFormatTable indices or Vulkan formats are out of sync with gp::math::PixelFormat."
);

}   // namespace detail

/// @brief Returns the `VulkanPixelFormatEntry` metadata for `format`.
/// @param[in] format Engine pixel format enumerator.
/// @return Const reference to the table entry, or the `Unknown` entry when `format` is out of range.
[[nodiscard]] inline constexpr const VulkanPixelFormatEntry&
    getVulkanPixelFormatEntry(const PixelFormat format) noexcept
{
    const gp::USize index = static_cast<gp::USize>(format);
    if (index >= kPixelFormatCount)
    {
        return kVulkanPixelFormatTable[0u];
    }

    return kVulkanPixelFormatTable[index];
}

/// @brief Translates an engine `PixelFormat` into its linear or sRGB `VkFormat`.
/// @param[in] format Engine pixel format enumerator.
/// @param[in] isSRGB When true, returns the hardware sRGB `VkFormat` if available, or the linear format otherwise.
/// @return Corresponding `VkFormat`, or `VK_FORMAT_UNDEFINED` when `format` is `Unknown` or out of range.
[[nodiscard]] inline constexpr VkFormat
    pixelFormatToVkFormat(const PixelFormat format, const bool isSRGB = false) noexcept
{
    const VulkanPixelFormatEntry& entry = getVulkanPixelFormatEntry(format);
    return isSRGB ? entry.srgbFormat : entry.linearFormat;
}

/// @brief Returns the default `VkComponentMapping` view swizzle for `format`.
/// @param[in] format Engine pixel format enumerator.
/// @return Default `VkComponentMapping` applied to sampled views of `format`.
[[nodiscard]] inline constexpr VkComponentMapping getVulkanFormatComponentMapping(const PixelFormat format) noexcept
{
    return getVulkanPixelFormatEntry(format).componentMapping;
}

/// @brief Maps a linear `VkFormat` to its hardware sRGB counterpart.
/// @details Covers uncompressed 8-bit formats, BC1/BC2/BC3/BC7, ETC2, and all 14 ASTC block dimensions.
///          Returns `format` unchanged when no sRGB equivalent exists.
/// @param[in] format Linear Vulkan format.
/// @param[in] allowR8SRGB When false, keeps `VK_FORMAT_R8_UNORM` linear (matching ES 3.1 feature-level behavior).
/// @return Hardware sRGB `VkFormat`, or `format` unchanged if no sRGB variant applies.
[[nodiscard]] inline constexpr VkFormat vkFormatToSRGB(const VkFormat format, const bool allowR8SRGB = true) noexcept
{
    switch (format)
    {
    case VK_FORMAT_B8G8R8A8_UNORM:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        return VK_FORMAT_A8B8G8R8_SRGB_PACK32;
    case VK_FORMAT_R8_UNORM:
        return allowR8SRGB ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
    case VK_FORMAT_R8G8_UNORM:
        return VK_FORMAT_R8G8_SRGB;
    case VK_FORMAT_R8G8B8_UNORM:
        return VK_FORMAT_R8G8B8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case VK_FORMAT_BC2_UNORM_BLOCK:
        return VK_FORMAT_BC2_SRGB_BLOCK;
    case VK_FORMAT_BC3_UNORM_BLOCK:
        return VK_FORMAT_BC3_SRGB_BLOCK;
    case VK_FORMAT_BC7_UNORM_BLOCK:
        return VK_FORMAT_BC7_SRGB_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
    default:
        return format;
    }
}

/// @brief Maps an sRGB `VkFormat` back to its corresponding linear UNORM `VkFormat`.
/// @param[in] format Vulkan format to normalize.
/// @return Linear UNORM `VkFormat`, or `format` unchanged if `format` is already linear.
[[nodiscard]] inline constexpr VkFormat vkFormatToLinear(const VkFormat format) noexcept
{
    switch (format)
    {
    case VK_FORMAT_B8G8R8A8_SRGB:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        return VK_FORMAT_A8B8G8R8_UNORM_PACK32;
    case VK_FORMAT_R8_SRGB:
        return VK_FORMAT_R8_UNORM;
    case VK_FORMAT_R8G8_SRGB:
        return VK_FORMAT_R8G8_UNORM;
    case VK_FORMAT_R8G8B8_SRGB:
        return VK_FORMAT_R8G8B8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case VK_FORMAT_BC2_SRGB_BLOCK:
        return VK_FORMAT_BC2_UNORM_BLOCK;
    case VK_FORMAT_BC3_SRGB_BLOCK:
        return VK_FORMAT_BC3_UNORM_BLOCK;
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
        return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
        return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
        return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
        return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
        return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
    default:
        return format;
    }
}

/// @brief Reports whether `format` uses hardware sRGB gamma decoding and encoding.
/// @param[in] format Vulkan format to inspect.
/// @return True when `format` is an sRGB `VkFormat`.
[[nodiscard]] inline constexpr bool isSRGBVkFormat(const VkFormat format) noexcept
{
    return vkFormatToLinear(format) != format;
}

/// @brief Translates a `VkFormat` back to its canonical engine `PixelFormat`.
/// @details Resolves both linear and sRGB Vulkan formats as well as packed aliases (such as
///          `VK_FORMAT_A8B8G8R8_UNORM_PACK32`, `VK_FORMAT_BC1_RGB_UNORM_BLOCK`, and `VK_FORMAT_R5G5B5A1_UNORM_PACK16`).
///          Aliased engine formats that share a single `VkFormat` (such as `A8_UNorm` vs `R8_UNorm`, or
///          `X24_G8_UInt` vs `D24_UNorm_S8_UInt`) resolve to the primary owning `PixelFormat`.
///          Device-fallback formats without a dedicated `PixelFormat` enumerator (such as
///          `VK_FORMAT_D16_UNORM_S8_UINT` and `VK_FORMAT_R16G16B16_SFLOAT`) return `PixelFormat::Unknown`.
/// @param[in] format Vulkan format to translate.
/// @return Matching `PixelFormat`, or `PixelFormat::Unknown` if unmapped.
[[nodiscard]] GP_RHI_VULKAN_API PixelFormat vkFormatToPixelFormat(VkFormat format) noexcept;

/// @brief Returns the prioritized candidate `VkFormat` list probed during device format setup for `format`.
/// @details Formats with hardware fallback chains (such as `R11G11B10_Float`, `B5G5R5A1_UNorm`, `R16_UNorm`,
///          `R64_UInt`, and depth-stencil formats) return multiple candidates in preference order. All other valid
///          formats return a single-element view containing their canonical linear `VkFormat`.
/// @param[in] format Engine pixel format to query.
/// @return Read-only view of candidate `VkFormat` values (empty for `PixelFormat::Unknown` or out-of-range values).
[[nodiscard]] GP_RHI_VULKAN_API gp::ConstVectorView<VkFormat> getVulkanFormatFallbackList(PixelFormat format) noexcept;

/// @brief Reports whether `format` contains a Vulkan depth plane.
/// @param[in] format Vulkan format to inspect.
/// @return True for `D16`, `X8_D24`, `D32_SFLOAT`, and combined depth-stencil formats.
[[nodiscard]] inline constexpr bool vkFormatHasDepth(const VkFormat format) noexcept
{
    switch (format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

/// @brief Reports whether `format` contains a Vulkan stencil plane.
/// @param[in] format Vulkan format to inspect.
/// @return True for `S8_UINT` and combined depth-stencil formats.
[[nodiscard]] inline constexpr bool vkFormatHasStencil(const VkFormat format) noexcept
{
    switch (format)
    {
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

/// @brief Computes the `VkImageAspectFlags` mask for an engine `PixelFormat`.
/// @param[in] format Engine pixel format.
/// @param[in] includeStencil True to include `VK_IMAGE_ASPECT_STENCIL_BIT` on combined depth-stencil formats.
/// @param[in] includeDepth True to include `VK_IMAGE_ASPECT_DEPTH_BIT` on depth-bearing formats.
/// @return `VkImageAspectFlags` bitmask for image views and subresource ranges (`0u` for `PixelFormat::Unknown` or
///         out-of-range values).
[[nodiscard]] inline constexpr VkImageAspectFlags
    getVkImageAspectFlags(const PixelFormat format, const bool includeStencil, const bool includeDepth = true) noexcept
{
    if (format == PixelFormat::Unknown || static_cast<gp::USize>(format) >= kPixelFormatCount)
    {
        return 0u;
    }

    switch (format)
    {
    case PixelFormat::X24_G8_UInt:
    case PixelFormat::X32_G8_UInt:
        return VK_IMAGE_ASPECT_STENCIL_BIT;

    case PixelFormat::D24_UNorm_S8_UInt:
    case PixelFormat::D32_Float_S8_UInt:
        return (includeDepth ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT) : 0u) |
               (includeStencil ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_STENCIL_BIT) : 0u);

    case PixelFormat::D16_UNorm:
    case PixelFormat::D32_Float:
        return includeDepth ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT) : 0u;

    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

/// @brief Computes the `VkImageAspectFlags` mask directly from a native `VkFormat`.
/// @param[in] format Vulkan format.
/// @param[in] includeStencil True to include `VK_IMAGE_ASPECT_STENCIL_BIT` when the format has stencil.
/// @param[in] includeDepth True to include `VK_IMAGE_ASPECT_DEPTH_BIT` when the format has depth.
/// @return `VkImageAspectFlags` bitmask (`VK_IMAGE_ASPECT_COLOR_BIT` for color formats, `0` for `VK_FORMAT_UNDEFINED`).
[[nodiscard]] inline constexpr VkImageAspectFlags
    getVkImageAspectFlags(const VkFormat format, const bool includeStencil, const bool includeDepth = true) noexcept
{
    if (format == VK_FORMAT_UNDEFINED)
    {
        return 0u;
    }

    const bool hasDepth = vkFormatHasDepth(format);
    const bool hasStencil = vkFormatHasStencil(format);
    if (!hasDepth && !hasStencil)
    {
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkImageAspectFlags flags = 0u;
    if (hasDepth && includeDepth)
    {
        flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (hasStencil && includeStencil)
    {
        flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    return flags;
}

/// @brief Reports whether `format` is an LDR ASTC block-compressed Vulkan format (UNORM or sRGB).
/// @param[in] format Vulkan format to test.
/// @return True for `VK_FORMAT_ASTC_4x4_UNORM_BLOCK` through `VK_FORMAT_ASTC_12x12_SRGB_BLOCK`.
[[nodiscard]] inline constexpr bool isASTCLDRFormat(const VkFormat format) noexcept
{
    return format >= VK_FORMAT_ASTC_4x4_UNORM_BLOCK && format <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
}

/// @brief Reports whether `format` is an sRGB ASTC block-compressed Vulkan format.
/// @param[in] format Vulkan format to test.
/// @return True for any `VK_FORMAT_ASTC_*_SRGB_BLOCK` enumerator.
[[nodiscard]] inline constexpr bool isASTCSRGBFormat(const VkFormat format) noexcept
{
    switch (format)
    {
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
        return true;
    default:
        return false;
    }
}

/// @brief Reports whether `format` is an HDR floating-point ASTC block-compressed Vulkan format.
/// @param[in] format Vulkan format to test.
/// @return True for `VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT` through `VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT`.
[[nodiscard]] inline constexpr bool isASTCHDRFormat(const VkFormat format) noexcept
{
    return format >= VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT && format <= VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT;
}

/// @brief Returns the storage bit count per texel for uncompressed and 4x4 block-compressed Vulkan formats.
/// @details For 4x4 compressed formats (`BC1`, `BC4`, `ETC2_RGB`, `EAC_R11` = 4 bpp; `BC2`, `BC3`, `BC5`, `BC6H`,
///          `BC7`, `ETC2_RGBA`, `EAC_RG11`, `ASTC_4x4` = 8 bpp), returns the effective bits per texel.
///          Returns `0u` for non-4x4 ASTC formats or unrecognized `VkFormat` values.
/// @param[in] format Vulkan format to inspect.
/// @return Bits per pixel, or `0u` when `format` is unrecognized or uses a non-4x4 ASTC block footprint.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32 getNumBitsPerPixel(VkFormat format) noexcept;

/// @brief Translates Vulkan buffer format feature flags into engine `PixelFormatCapabilities`.
/// @param[in] vulkanFormat Native `VkFormat` being evaluated (used to identify 16/32-bit index buffer eligibility).
/// @param[in] bufferFeatures `VkFormatProperties::bufferFeatures` bitmask reported by the physical device.
/// @return Equivalent `PixelFormatCapabilities` mask for buffer usage.
[[nodiscard]] GP_RHI_VULKAN_API PixelFormatCapabilities
    translateBufferFormatCapabilities(VkFormat vulkanFormat, VkFormatFeatureFlags bufferFeatures) noexcept;

/// @brief Translates Vulkan linear and optimal image format feature flags into engine `PixelFormatCapabilities`.
/// @details Maps sampled, depth-stencil, color attachment, blending, storage image, atomic, and linear filtering bits
///          using optimal tiling for attachments and either tiling for general image usage.
/// @param[in] properties Physical device format properties queried via `vkGetPhysicalDeviceFormatProperties`.
/// @return Equivalent `PixelFormatCapabilities` mask for image usage.
[[nodiscard]] GP_RHI_VULKAN_API PixelFormatCapabilities
    translateImageFormatCapabilities(const VkFormatProperties& properties) noexcept;

}   // namespace gp::rhi::vulkan
