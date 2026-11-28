// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include "RHIPixelFormat.hpp"
#include "RHIVulkan.hpp"

namespace gp::rhi::vulkan
{

/// @brief Half-precision encoding of `1.0f`.
inline constexpr gp::UInt16 kHalfOne = 0x3C00u;

/// @brief Decodes an IEEE-754 binary16 value.
/// @details Handles zeros, subnormals, infinities and NaNs.
/// @param[in] half Encoded value.
/// @return Decoded single-precision value.
[[nodiscard]] GP_RHI_VULKAN_API gp::Float32 halfToFloat(gp::UInt16 half) noexcept;

/// @brief Encodes a value as IEEE-754 binary16 with round-to-nearest-even.
/// @details Values beyond the half range become infinities; NaNs stay NaNs; tiny values become subnormals or zero.
/// @param[in] value Single-precision value.
/// @return Encoded value.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt16 floatToHalf(gp::Float32 value) noexcept;

/// @brief Returns the size of one texel as the conversion functions read it.
/// @details Matches the layout `vkCmdCopyImageToBuffer` writes. For depth/stencil formats only the depth aspect is
///          copied, so `D24_UNorm_S8_UInt` reads 4 bytes (depth in the low 24 bits) and `D32_Float_S8_UInt` reads 4
///          bytes (the float depth).
/// @param[in] format Pixel format of the raw texels.
/// @return Texel byte size, or `0` when the format is not supported.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32 getVulkanSurfaceTexelBytes(PixelFormat format) noexcept;

/// @brief Reports whether surface readback can convert texels of `format`.
/// @details Supported formats: UNorm formats with 5, 8, 10 or 16-bit channels, half and single float formats, the
///          packed `R11G11B10_Float` format, and depth formats (depth aspect only). Signed, integer, shared-exponent
///          and block-compressed formats are not.
/// @param[in] format Pixel format of the raw texels.
/// @return True when `convertVulkanSurfaceToColor()` and `convertVulkanSurfaceToLinearColor()` accept `format`.
[[nodiscard]] GP_RHI_VULKAN_API bool isVulkanSurfaceConversionSupported(PixelFormat format) noexcept;

/// @brief Converts raw texels into 8-bit colors.
/// @details
///          - 5/8/10/16-bit normalized formats are requantized to 8 bits exactly, with no gamma change.
///          - Half, `R32G32_Float` and packed float formats are converted like a linear color, sRGB encoded when
///            `linearToGamma` is set.
///          - `R32_Float` and `R32G32B32A32_Float` are normalized to the min/max of the surface so the result is
///            visible whatever the value range (alpha is clamped instead).
///          - Single-channel formats (red or depth) become gray; `A8_UNorm` becomes black with that alpha.
/// @param[in] format Pixel format of the raw texels.
/// @param[in] source First texel of the surface.
/// @param[in] sourcePitch Byte pitch between rows.
/// @param[in] width Texels per row.
/// @param[in] height Row count.
/// @param[in] linearToGamma True to sRGB encode float formats.
/// @param[out] outData Receives `width * height` colors, row-major.
/// @return False when the format is not supported (nothing written).
[[nodiscard]] GP_RHI_VULKAN_API bool convertVulkanSurfaceToColor(
    PixelFormat format,
    const gp::UInt8* source,
    gp::UInt32 sourcePitch,
    gp::UInt32 width,
    gp::UInt32 height,
    bool linearToGamma,
    gp::math::Color* outData
) noexcept;

/// @brief Converts raw texels into linear colors.
/// @details 8-bit formats are sRGB decoded when `isSRGB` is set and reinterpreted as linear otherwise. Other
///          normalized formats are divided by their maximum value. Float formats are copied. Missing channels read
///          as zero, and a missing alpha reads as one.
/// @param[in] format Pixel format of the raw texels.
/// @param[in] source First texel of the surface.
/// @param[in] sourcePitch Byte pitch between rows.
/// @param[in] width Texels per row.
/// @param[in] height Row count.
/// @param[in] isSRGB True when the texels are sRGB encoded.
/// @param[out] outData Receives `width * height` colors, row-major.
/// @return False when the format is not supported (nothing written).
[[nodiscard]] GP_RHI_VULKAN_API bool convertVulkanSurfaceToLinearColor(
    PixelFormat format,
    const gp::UInt8* source,
    gp::UInt32 sourcePitch,
    gp::UInt32 width,
    gp::UInt32 height,
    bool isSRGB,
    gp::math::LinearColor* outData
) noexcept;

/// @brief Converts raw float texels into half-precision colors.
/// @details Half formats are copied bit for bit; `R32*_Float` and `R11G11B10_Float` are re-encoded. Missing channels
///          read as zero, and a missing alpha reads as one.
/// @param[in] format Pixel format of the raw texels; a float format.
/// @param[in] source First texel of the surface.
/// @param[in] sourcePitch Byte pitch between rows.
/// @param[in] width Texels per row.
/// @param[in] height Row count.
/// @param[out] outData Receives `width * height` colors, row-major.
/// @return False when the format is not a supported float format (nothing written).
[[nodiscard]] GP_RHI_VULKAN_API bool convertVulkanSurfaceToFloat16Color(
    PixelFormat format,
    const gp::UInt8* source,
    gp::UInt32 sourcePitch,
    gp::UInt32 width,
    gp::UInt32 height,
    resource::Float16Color* outData
) noexcept;

}   // namespace gp::rhi::vulkan
