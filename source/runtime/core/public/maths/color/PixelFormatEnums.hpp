// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::math
{

/// @brief Canonical engine pixel format describing encoded memory layout and block dimensions.
/// @details Defines API-agnostic image and texture storage layouts shared across `gp::core`, asset parsers,
///          HAL, and `gp::rhi`. Hardware support and native API format mappings (`VkFormat`, `DXGI_FORMAT`,
///          `MTLPixelFormat`) live strictly inside the RHI layer rather than in `gp::core`.
/// @note sRGB gamma transfer is orthogonal to this enum and controlled via texture/view creation flags
///       (such as `gp::rhi::resource::TextureCreateFlags::SRGB`).
enum class PixelFormat : gp::UInt8
{
    /// @brief Unspecified or invalid pixel format.
    Unknown = 0,

    // 8-bit per-channel uncompressed formats
    R8_UNorm,
    R8_SNorm,
    R8_UInt,
    R8_SInt,
    A8_UNorm,

    R8G8_UNorm,
    R8G8_SNorm,
    R8G8_UInt,
    R8G8_SInt,

    /// @brief 24-bit tightly packed RGB interchange format (primarily for CPU image decoding/encoding).
    R8G8B8_UNorm,

    R8G8B8A8_UNorm,
    R8G8B8A8_SNorm,
    R8G8B8A8_UInt,
    R8G8B8A8_SInt,
    B8G8R8A8_UNorm,

    // 16-bit per-channel uncompressed formats
    R16_UNorm,
    R16_SNorm,
    R16_UInt,
    R16_SInt,
    R16_Float,

    R16G16_UNorm,
    R16G16_SNorm,
    R16G16_UInt,
    R16G16_SInt,
    R16G16_Float,

    R16G16B16A16_UNorm,
    R16G16B16A16_SNorm,
    R16G16B16A16_UInt,
    R16G16B16A16_SInt,
    R16G16B16A16_Float,

    // 32-bit and 64-bit per-channel uncompressed formats
    R32_UInt,
    R32_SInt,
    R32_Float,

    R32G32_UInt,
    R32G32_SInt,
    R32G32_Float,

    R32G32B32_UInt,
    R32G32B32_SInt,
    R32G32B32_Float,

    R32G32B32A32_UInt,
    R32G32B32A32_SInt,
    R32G32B32A32_Float,

    R64_UInt,

    // Packed and HDR special color formats
    B5G6R5_UNorm,
    B5G5R5A1_UNorm,
    R10G10B10A2_UNorm,
    B10G10R10A2_UNorm,
    R11G11B10_Float,
    R9G9B9E5_SharedExp,

    // Depth and stencil formats
    D16_UNorm,
    D24_UNorm_S8_UInt,
    D32_Float,
    D32_Float_S8_UInt,
    /// @brief Stencil-only SRV alias format over a `D24_UNorm_S8_UInt` surface.
    X24_G8_UInt,
    /// @brief Stencil-only SRV alias format over a `D32_Float_S8_UInt` surface.
    X32_G8_UInt,

    // BCn / DXTC block-compressed formats (4x4 texel blocks)
    BC1_UNorm,
    BC2_UNorm,
    BC3_UNorm,
    BC4_UNorm,
    BC5_UNorm,
    BC6H_UFloat,
    BC6H_SFloat,
    BC7_UNorm,

    // ETC2 / EAC block-compressed formats (4x4 texel blocks)
    ETC2_RGB_UNorm,
    ETC2_RGBA_UNorm,
    ETC2_R11_EAC,
    ETC2_RG11_EAC,

    // ASTC block-compressed formats (16-byte blocks, LDR and HDR)
    ASTC_4x4_UNorm,
    ASTC_6x6_UNorm,
    ASTC_8x8_UNorm,
    ASTC_10x10_UNorm,
    ASTC_12x12_UNorm,
    ASTC_4x4_Float,
    ASTC_6x6_Float,
    ASTC_8x8_Float,
    ASTC_10x10_Float,
    ASTC_12x12_Float,

    /// @brief Total number of valid enumerators (sentinel; not a usable format).
    Count
};

/// @brief Total number of entries in `PixelFormat` (including `PixelFormat::Unknown`).
inline constexpr gp::USize kPixelFormatCount = static_cast<gp::USize>(PixelFormat::Count);

/// @brief Bitmask flags describing which logical channels exist in a `PixelFormat`.
enum class PixelFormatChannelFlags : gp::UInt8
{
    None = 0x00u,
    R = 1u << 0,
    G = 1u << 1,
    B = 1u << 2,
    A = 1u << 3,
    Depth = 1u << 4,
    Stencil = 1u << 5,

    RG = R | G,
    RGB = R | G | B,
    RGBA = R | G | B | A,
    DepthStencil = Depth | Stencil
};

/// @brief Numeric encoding representation of component values in a `PixelFormat`.
enum class PixelFormatComponentType : gp::UInt8
{
    /// @brief Invalid or unspecified format.
    None = 0,
    /// @brief Unsigned normalized fixed-point mapped to `[0, 1]`.
    UNorm,
    /// @brief Signed normalized fixed-point mapped to `[-1, 1]`.
    SNorm,
    /// @brief Unsigned integer.
    UInt,
    /// @brief Signed integer.
    SInt,
    /// @brief Signed IEEE-style floating-point.
    Float,
    /// @brief Unsigned floating-point (e.g., `R11G11B10_Float`, `BC6H_UFloat`).
    UFloat,
    /// @brief Unsigned mantissas with shared exponent (e.g., `R9G9B9E5_SharedExp`).
    SharedExp,
    /// @brief Mixed depth + unsigned integer stencil layout (e.g., `D24_UNorm_S8_UInt`, `D32_Float_S8_UInt`).
    DepthStencil
};

/// @brief Hardware block-compression family of a `PixelFormat`.
enum class PixelFormatCompression : gp::UInt8
{
    /// @brief Uncompressed texels (`1x1x1` block dimensions).
    None = 0,
    /// @brief Desktop/console BCn (DXTC / BPTC / RGTC) 4x4 block compression.
    BC,
    /// @brief Mobile Ericsson Texture Compression 2 / EAC 4x4 block compression.
    ETC2,
    /// @brief Adaptive Scalable Texture Compression (variable block dimensions, 16 bytes per block).
    ASTC
};

}   // namespace gp::math

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::math::PixelFormatChannelFlags);
