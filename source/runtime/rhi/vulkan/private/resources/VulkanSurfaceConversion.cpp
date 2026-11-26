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

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Bit pattern of a positive single-precision infinity, also the exponent mask.
constexpr gp::UInt32 kFloatExponentMask = 0x7F800000u;

/// @brief Value of the smallest normal number of a format with a 5-bit exponent (`2^-14`).
constexpr gp::Float32 kSmallFloatMinNormal = 6.103515625e-05f;

/// @brief Reads one texel field without assuming the staging memory is aligned for `InValueType`.
/// @param[in] texel First byte of the field.
/// @return Loaded value.
template <typename InValueType>
[[nodiscard]] InValueType loadTexelValue(const gp::UInt8* const texel) noexcept
{
    InValueType value{};
    gp::memory::copyMemory(&value, texel, sizeof(InValueType));
    return value;
}

/// @brief Decodes an unsigned float with a 5-bit exponent (bias 15) and `mantissaBits` mantissa bits.
/// @details Shared by half floats (10-bit mantissa, sign handled by the caller) and the 11/10-bit channels of
///          `R11G11B10_Float` (6 and 5-bit mantissas).
/// @param[in] bits Exponent and mantissa, right aligned.
/// @param[in] mantissaBits Mantissa width.
/// @return Decoded value.
[[nodiscard]] gp::Float32 decodeUnsignedSmallFloat(const gp::UInt32 bits, const gp::UInt32 mantissaBits) noexcept
{
    const gp::UInt32 exponent = (bits >> mantissaBits) & 0x1Fu;
    const gp::UInt32 mantissa = bits & ((1u << mantissaBits) - 1u);
    if (exponent == 0u)
    {
        // Subnormal: 0.mantissa * 2^-14.
        return static_cast<gp::Float32>(mantissa) * kSmallFloatMinNormal / static_cast<gp::Float32>(1u << mantissaBits);
    }

    const gp::UInt32 widenedMantissa = mantissa << (23u - mantissaBits);
    if (exponent == 0x1Fu)
    {
        // Infinity (zero mantissa) or NaN (payload kept).
        return std::bit_cast<gp::Float32>(kFloatExponentMask | widenedMantissa);
    }

    // Rebias the exponent from 15 to 127.
    return std::bit_cast<gp::Float32>(((exponent + 112u) << 23u) | widenedMantissa);
}

/// @brief Reports whether a single-precision value is neither infinite nor NaN.
/// @param[in] value Value to test.
/// @return True when finite.
[[nodiscard]] bool isFiniteFloat(const gp::Float32 value) noexcept
{
    return (std::bit_cast<gp::UInt32>(value) & kFloatExponentMask) != kFloatExponentMask;
}

/// @brief Expands an `n`-bit UNorm value to 8 bits with exact rounding.
/// @param[in] value Source value in `[0, maxValue]`.
/// @param[in] maxValue Largest source value (`2^n - 1`).
/// @return Requantized value in `[0, 255]`.
[[nodiscard]] gp::UInt8 requantizeUNormTo8(const gp::UInt32 value, const gp::UInt32 maxValue) noexcept
{
    const gp::UInt64 scaled = static_cast<gp::UInt64>(value) * 255u + (maxValue / 2u);
    return static_cast<gp::UInt8>(scaled / maxValue);
}

/// @brief Converts an `n`-bit UNorm value to a float in `[0, 1]`.
/// @param[in] value Source value in `[0, maxValue]`.
/// @param[in] maxValue Largest source value (`2^n - 1`).
/// @return Normalized value.
[[nodiscard]] gp::Float32 unormToFloat(const gp::UInt32 value, const gp::UInt32 maxValue) noexcept
{
    return static_cast<gp::Float32>(static_cast<gp::Float64>(value) / static_cast<gp::Float64>(maxValue));
}

/// @brief Builds a gray 8-bit color.
/// @param[in] value Gray level.
/// @return Opaque `(value, value, value)`.
[[nodiscard]] gp::math::Color makeGray(const gp::UInt8 value) noexcept
{
    return gp::math::Color(value, value, value, 255u);
}

/// @brief Reads the 8-bit channels of a texel; missing channels are zero and a missing alpha is opaque.
/// @param[in] format An 8-bit UNorm format.
/// @param[in] texel First byte of the texel.
/// @return Texel channels.
[[nodiscard]] gp::math::Color loadColor8(const PixelFormat format, const gp::UInt8* const texel) noexcept
{
    switch (format)
    {
    case PixelFormat::R8_UNorm:
        return gp::math::Color(texel[0], 0u, 0u, 255u);
    case PixelFormat::A8_UNorm:
        return gp::math::Color(0u, 0u, 0u, texel[0]);
    case PixelFormat::R8G8_UNorm:
        return gp::math::Color(texel[0], texel[1], 0u, 255u);
    case PixelFormat::R8G8B8A8_UNorm:
        return gp::math::Color(texel[0], texel[1], texel[2], texel[3]);
    case PixelFormat::B8G8R8A8_UNorm:
        return gp::math::Color(texel[2], texel[1], texel[0], texel[3]);
    default:
        break;
    }
    return gp::math::Color(0u, 0u, 0u, 255u);
}

/// @brief Reports whether a format stores 8-bit UNorm channels.
/// @param[in] format Format to test.
/// @return True for the formats `loadColor8()` reads.
[[nodiscard]] bool isUNorm8Format(const PixelFormat format) noexcept
{
    return format == PixelFormat::R8_UNorm || format == PixelFormat::A8_UNorm || format == PixelFormat::R8G8_UNorm ||
           format == PixelFormat::R8G8B8A8_UNorm || format == PixelFormat::B8G8R8A8_UNorm;
}

/// @brief Decodes one texel of a supported format into a linear color.
/// @param[in] format Supported format.
/// @param[in] texel First byte of the texel.
/// @param[in] isSRGB True to sRGB decode 8-bit formats.
/// @return Decoded color; missing channels are zero and a missing alpha is one.
[[nodiscard]] gp::math::LinearColor
    decodeLinearTexel(const PixelFormat format, const gp::UInt8* const texel, const bool isSRGB) noexcept
{
    if (isUNorm8Format(format))
    {
        const gp::math::Color color = loadColor8(format, texel);
        return isSRGB ? gp::math::LinearColor(color) : color.reinterpretAsLinear();
    }

    switch (format)
    {
    case PixelFormat::B5G6R5_UNorm:
    {
        const gp::UInt32 bits = loadTexelValue<gp::UInt16>(texel);
        return gp::math::LinearColor(
            unormToFloat(bits >> 11u, 31u),
            unormToFloat((bits >> 5u) & 0x3Fu, 63u),
            unormToFloat(bits & 0x1Fu, 31u),
            1.0f
        );
    }
    case PixelFormat::R16_UNorm:
    case PixelFormat::D16_UNorm:
        return gp::math::LinearColor(unormToFloat(loadTexelValue<gp::UInt16>(texel), 0xFFFFu), 0.0f, 0.0f, 1.0f);
    case PixelFormat::R16G16_UNorm:
        return gp::math::LinearColor(
            unormToFloat(loadTexelValue<gp::UInt16>(texel), 0xFFFFu),
            unormToFloat(loadTexelValue<gp::UInt16>(texel + 2u), 0xFFFFu),
            0.0f,
            1.0f
        );
    case PixelFormat::R16G16B16A16_UNorm:
        return gp::math::LinearColor(
            unormToFloat(loadTexelValue<gp::UInt16>(texel), 0xFFFFu),
            unormToFloat(loadTexelValue<gp::UInt16>(texel + 2u), 0xFFFFu),
            unormToFloat(loadTexelValue<gp::UInt16>(texel + 4u), 0xFFFFu),
            unormToFloat(loadTexelValue<gp::UInt16>(texel + 6u), 0xFFFFu)
        );
    case PixelFormat::R10G10B10A2_UNorm:
    case PixelFormat::B10G10R10A2_UNorm:
    {
        const gp::UInt32 bits = loadTexelValue<gp::UInt32>(texel);
        const gp::Float32 low = unormToFloat(bits & 0x3FFu, 0x3FFu);
        const gp::Float32 green = unormToFloat((bits >> 10u) & 0x3FFu, 0x3FFu);
        const gp::Float32 high = unormToFloat((bits >> 20u) & 0x3FFu, 0x3FFu);
        const gp::Float32 alpha = unormToFloat(bits >> 30u, 3u);
        return format == PixelFormat::R10G10B10A2_UNorm ? gp::math::LinearColor(low, green, high, alpha)
                                                        : gp::math::LinearColor(high, green, low, alpha);
    }
    case PixelFormat::R16_Float:
        return gp::math::LinearColor(halfToFloat(loadTexelValue<gp::UInt16>(texel)), 0.0f, 0.0f, 1.0f);
    case PixelFormat::R16G16_Float:
        return gp::math::LinearColor(
            halfToFloat(loadTexelValue<gp::UInt16>(texel)),
            halfToFloat(loadTexelValue<gp::UInt16>(texel + 2u)),
            0.0f,
            1.0f
        );
    case PixelFormat::R16G16B16A16_Float:
        return gp::math::LinearColor(
            halfToFloat(loadTexelValue<gp::UInt16>(texel)),
            halfToFloat(loadTexelValue<gp::UInt16>(texel + 2u)),
            halfToFloat(loadTexelValue<gp::UInt16>(texel + 4u)),
            halfToFloat(loadTexelValue<gp::UInt16>(texel + 6u))
        );
    case PixelFormat::R32_Float:
    case PixelFormat::D32_Float:
    case PixelFormat::D32_Float_S8_UInt:
        return gp::math::LinearColor(loadTexelValue<gp::Float32>(texel), 0.0f, 0.0f, 1.0f);
    case PixelFormat::R32G32_Float:
        return gp::math::LinearColor(
            loadTexelValue<gp::Float32>(texel), loadTexelValue<gp::Float32>(texel + 4u), 0.0f, 1.0f
        );
    case PixelFormat::R32G32B32A32_Float:
        return gp::math::LinearColor(
            loadTexelValue<gp::Float32>(texel),
            loadTexelValue<gp::Float32>(texel + 4u),
            loadTexelValue<gp::Float32>(texel + 8u),
            loadTexelValue<gp::Float32>(texel + 12u)
        );
    case PixelFormat::R11G11B10_Float:
    {
        const gp::UInt32 bits = loadTexelValue<gp::UInt32>(texel);
        return gp::math::LinearColor(
            decodeUnsignedSmallFloat(bits & 0x7FFu, 6u),
            decodeUnsignedSmallFloat((bits >> 11u) & 0x7FFu, 6u),
            decodeUnsignedSmallFloat(bits >> 22u, 5u),
            1.0f
        );
    }
    case PixelFormat::D24_UNorm_S8_UInt:
        return gp::math::LinearColor(
            unormToFloat(loadTexelValue<gp::UInt32>(texel) & 0xFFFFFFu, 0xFFFFFFu), 0.0f, 0.0f, 1.0f
        );
    default:
        break;
    }
    return gp::math::LinearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

/// @brief Value range of the finite color channels of a float surface.
struct FloatSurfaceRange final
{
    gp::Float32 minValue{ 0.0f };
    gp::Float32 scale{ 1.0f };
};

/// @brief Scans the color channels (alpha excluded) of an `R32_Float` or `R32G32B32A32_Float` surface.
/// @param[in] source First texel of the surface.
/// @param[in] sourcePitch Byte pitch between rows.
/// @param[in] width Texels per row.
/// @param[in] height Row count.
/// @param[in] texelBytes Texel size.
/// @param[in] colorChannels Channels scanned per texel (1 or 3).
/// @return Minimum and `1 / (max - min)`, or the identity `(0, 1)` when the range is empty.
[[nodiscard]] FloatSurfaceRange scanFloatSurfaceRange(
    const gp::UInt8* const source,
    const gp::UInt32 sourcePitch,
    const gp::UInt32 width,
    const gp::UInt32 height,
    const gp::UInt32 texelBytes,
    const gp::UInt32 colorChannels
) noexcept
{
    bool hasValue = false;
    gp::Float32 minValue = 0.0f;
    gp::Float32 maxValue = 0.0f;
    for (gp::UInt32 y = 0u; y < height; ++y)
    {
        const gp::UInt8* const row = source + static_cast<gp::USize>(y) * sourcePitch;
        for (gp::UInt32 x = 0u; x < width; ++x)
        {
            const gp::UInt8* const texel = row + static_cast<gp::USize>(x) * texelBytes;
            for (gp::UInt32 channel = 0u; channel < colorChannels; ++channel)
            {
                const gp::Float32 value = loadTexelValue<gp::Float32>(texel + channel * 4u);
                if (!isFiniteFloat(value))
                {
                    continue;
                }
                minValue = hasValue ? (value < minValue ? value : minValue) : value;
                maxValue = hasValue ? (value > maxValue ? value : maxValue) : value;
                hasValue = true;
            }
        }
    }

    // An empty range (constant surface) keeps the values as they are, clamped by the quantization.
    FloatSurfaceRange range{};
    if (maxValue > minValue)
    {
        range.minValue = minValue;
        range.scale = 1.0f / (maxValue - minValue);
    }
    return range;
}

/// @brief Maps a float through a surface range and quantizes it to 8 bits.
/// @param[in] value Source value.
/// @param[in] range Surface range.
/// @return Quantized value; NaNs give `0`.
[[nodiscard]] gp::UInt8 quantizeInRange(const gp::Float32 value, const FloatSurfaceRange& range) noexcept
{
    return gp::math::Color::quantizeUNormFloatTo8((value - range.minValue) * range.scale);
}

/// @brief Converts one texel of a format with an exact 8-bit mapping.
/// @param[in] format Supported format other than `R32_Float` and `R32G32B32A32_Float`.
/// @param[in] texel First byte of the texel.
/// @param[in] linearToGamma True to sRGB encode float formats.
/// @return Converted color.
[[nodiscard]] gp::math::Color
    convertTexelToColor(const PixelFormat format, const gp::UInt8* const texel, const bool linearToGamma) noexcept
{
    switch (format)
    {
    case PixelFormat::R8_UNorm:
        return makeGray(texel[0]);
    case PixelFormat::A8_UNorm:
    case PixelFormat::R8G8_UNorm:
    case PixelFormat::R8G8B8A8_UNorm:
    case PixelFormat::B8G8R8A8_UNorm:
        return loadColor8(format, texel);
    case PixelFormat::B5G6R5_UNorm:
    {
        const gp::UInt32 bits = loadTexelValue<gp::UInt16>(texel);
        return gp::math::Color(
            requantizeUNormTo8(bits >> 11u, 31u),
            requantizeUNormTo8((bits >> 5u) & 0x3Fu, 63u),
            requantizeUNormTo8(bits & 0x1Fu, 31u),
            255u
        );
    }
    case PixelFormat::R16_UNorm:
    case PixelFormat::D16_UNorm:
        return makeGray(gp::math::Color::requantize16to8(loadTexelValue<gp::UInt16>(texel)));
    case PixelFormat::R16G16_UNorm:
        return gp::math::Color(
            gp::math::Color::requantize16to8(loadTexelValue<gp::UInt16>(texel)),
            gp::math::Color::requantize16to8(loadTexelValue<gp::UInt16>(texel + 2u)),
            0u,
            255u
        );
    case PixelFormat::R16G16B16A16_UNorm:
        return gp::math::Color(
            gp::math::Color::requantize16to8(loadTexelValue<gp::UInt16>(texel)),
            gp::math::Color::requantize16to8(loadTexelValue<gp::UInt16>(texel + 2u)),
            gp::math::Color::requantize16to8(loadTexelValue<gp::UInt16>(texel + 4u)),
            gp::math::Color::requantize16to8(loadTexelValue<gp::UInt16>(texel + 6u))
        );
    case PixelFormat::R10G10B10A2_UNorm:
    case PixelFormat::B10G10R10A2_UNorm:
    {
        const gp::UInt32 bits = loadTexelValue<gp::UInt32>(texel);
        const gp::Int32 low = static_cast<gp::Int32>(bits & 0x3FFu);
        const gp::Int32 green = static_cast<gp::Int32>((bits >> 10u) & 0x3FFu);
        const gp::Int32 high = static_cast<gp::Int32>((bits >> 20u) & 0x3FFu);
        const gp::Int32 alpha = static_cast<gp::Int32>(bits >> 30u);
        return format == PixelFormat::R10G10B10A2_UNorm
                   ? gp::math::Color::makeRequantizeFrom1010102(low, green, high, alpha)
                   : gp::math::Color::makeRequantizeFrom1010102(high, green, low, alpha);
    }
    case PixelFormat::D24_UNorm_S8_UInt:
        return makeGray(requantizeUNormTo8(loadTexelValue<gp::UInt32>(texel) & 0xFFFFFFu, 0xFFFFFFu));
    case PixelFormat::D32_Float:
    case PixelFormat::D32_Float_S8_UInt:
        return makeGray(gp::math::Color::quantizeUNormFloatTo8(loadTexelValue<gp::Float32>(texel)));
    default:
        break;
    }

    // Half, R32G32 and packed float formats behave like an HDR color.
    return decodeLinearTexel(format, texel, false).toColor(linearToGamma);
}

}   // namespace

gp::Float32 halfToFloat(const gp::UInt16 half) noexcept
{
    const gp::UInt32 sign = (static_cast<gp::UInt32>(half) & 0x8000u) << 16u;
    const gp::Float32 magnitude = decodeUnsignedSmallFloat(static_cast<gp::UInt32>(half) & 0x7FFFu, 10u);
    return std::bit_cast<gp::Float32>(std::bit_cast<gp::UInt32>(magnitude) | sign);
}

gp::UInt16 floatToHalf(const gp::Float32 value) noexcept
{
    const gp::UInt32 bits = std::bit_cast<gp::UInt32>(value);
    const gp::UInt32 sign = (bits >> 16u) & 0x8000u;
    const gp::UInt32 magnitude = bits & 0x7FFFFFFFu;

    if (magnitude >= kFloatExponentMask)
    {
        // Infinity stays infinity; NaN keeps its top payload bits and stays quiet.
        const gp::UInt32 nanBits = magnitude > kFloatExponentMask ? (0x200u | ((magnitude >> 13u) & 0x3FFu)) : 0u;
        return static_cast<gp::UInt16>(sign | 0x7C00u | nanBits);
    }

    if (magnitude >= 0x477FF000u)
    {
        // At or past the midpoint between 65504 (largest half) and 65536: rounds to infinity.
        return static_cast<gp::UInt16>(sign | 0x7C00u);
    }

    if (magnitude < 0x38800000u)
    {
        // Below 2^-14: subnormal half, or zero at and below 2^-25 (the 2^-25 tie rounds to the even zero).
        if (magnitude <= 0x33000000u)
        {
            return static_cast<gp::UInt16>(sign);
        }

        const gp::UInt32 exponent = magnitude >> 23u;
        const gp::UInt32 mantissa = (magnitude & 0x7FFFFFu) | 0x800000u;
        const gp::UInt32 shift = 126u - exponent;
        gp::UInt32 result = mantissa >> shift;
        const gp::UInt32 remainder = mantissa & ((1u << shift) - 1u);
        const gp::UInt32 halfway = 1u << (shift - 1u);
        if (remainder > halfway || (remainder == halfway && (result & 1u) != 0u))
        {
            ++result;
        }
        return static_cast<gp::UInt16>(sign | result);
    }

    // Normal: rebias the exponent from 127 to 15 and round the 13 dropped mantissa bits to nearest even. A carry out
    // of the mantissa correctly bumps the exponent.
    gp::UInt32 result = (magnitude - 0x38000000u) >> 13u;
    const gp::UInt32 remainder = magnitude & 0x1FFFu;
    if (remainder > 0x1000u || (remainder == 0x1000u && (result & 1u) != 0u))
    {
        ++result;
    }
    return static_cast<gp::UInt16>(sign | result);
}

gp::UInt32 getVulkanSurfaceTexelBytes(const PixelFormat format) noexcept
{
    switch (format)
    {
    case PixelFormat::R8_UNorm:
    case PixelFormat::A8_UNorm:
        return 1u;
    case PixelFormat::R8G8_UNorm:
    case PixelFormat::B5G6R5_UNorm:
    case PixelFormat::R16_UNorm:
    case PixelFormat::R16_Float:
    case PixelFormat::D16_UNorm:
        return 2u;
    case PixelFormat::R8G8B8A8_UNorm:
    case PixelFormat::B8G8R8A8_UNorm:
    case PixelFormat::R16G16_UNorm:
    case PixelFormat::R16G16_Float:
    case PixelFormat::R32_Float:
    case PixelFormat::R10G10B10A2_UNorm:
    case PixelFormat::B10G10R10A2_UNorm:
    case PixelFormat::R11G11B10_Float:
    case PixelFormat::D24_UNorm_S8_UInt:
    case PixelFormat::D32_Float:
    case PixelFormat::D32_Float_S8_UInt:
        return 4u;
    case PixelFormat::R16G16B16A16_UNorm:
    case PixelFormat::R16G16B16A16_Float:
    case PixelFormat::R32G32_Float:
        return 8u;
    case PixelFormat::R32G32B32A32_Float:
        return 16u;
    default:
        break;
    }
    return 0u;
}

bool isVulkanSurfaceConversionSupported(const PixelFormat format) noexcept
{
    return getVulkanSurfaceTexelBytes(format) != 0u;
}

bool convertVulkanSurfaceToColor(
    const PixelFormat format,
    const gp::UInt8* const source,
    const gp::UInt32 sourcePitch,
    const gp::UInt32 width,
    const gp::UInt32 height,
    const bool linearToGamma,
    gp::math::Color* const outData
) noexcept
{
    const gp::UInt32 texelBytes = getVulkanSurfaceTexelBytes(format);
    if (texelBytes == 0u || source == nullptr || outData == nullptr)
    {
        return false;
    }

    const bool isNormalized = format == PixelFormat::R32_Float || format == PixelFormat::R32G32B32A32_Float;
    const gp::UInt32 colorChannels = format == PixelFormat::R32_Float ? 1u : 3u;
    const FloatSurfaceRange range =
        isNormalized ? scanFloatSurfaceRange(source, sourcePitch, width, height, texelBytes, colorChannels)
                     : FloatSurfaceRange{};

    for (gp::UInt32 y = 0u; y < height; ++y)
    {
        const gp::UInt8* const row = source + static_cast<gp::USize>(y) * sourcePitch;
        gp::math::Color* const outRow = outData + static_cast<gp::USize>(y) * width;
        for (gp::UInt32 x = 0u; x < width; ++x)
        {
            const gp::UInt8* const texel = row + static_cast<gp::USize>(x) * texelBytes;
            if (format == PixelFormat::R32_Float)
            {
                outRow[x] = makeGray(quantizeInRange(loadTexelValue<gp::Float32>(texel), range));
            }
            else if (format == PixelFormat::R32G32B32A32_Float)
            {
                outRow[x] = gp::math::Color(
                    quantizeInRange(loadTexelValue<gp::Float32>(texel), range),
                    quantizeInRange(loadTexelValue<gp::Float32>(texel + 4u), range),
                    quantizeInRange(loadTexelValue<gp::Float32>(texel + 8u), range),
                    gp::math::Color::quantizeUNormFloatTo8(loadTexelValue<gp::Float32>(texel + 12u))
                );
            }
            else
            {
                outRow[x] = convertTexelToColor(format, texel, linearToGamma);
            }
        }
    }
    return true;
}

bool convertVulkanSurfaceToLinearColor(
    const PixelFormat format,
    const gp::UInt8* const source,
    const gp::UInt32 sourcePitch,
    const gp::UInt32 width,
    const gp::UInt32 height,
    const bool isSRGB,
    gp::math::LinearColor* const outData
) noexcept
{
    const gp::UInt32 texelBytes = getVulkanSurfaceTexelBytes(format);
    if (texelBytes == 0u || source == nullptr || outData == nullptr)
    {
        return false;
    }

    for (gp::UInt32 y = 0u; y < height; ++y)
    {
        const gp::UInt8* const row = source + static_cast<gp::USize>(y) * sourcePitch;
        gp::math::LinearColor* const outRow = outData + static_cast<gp::USize>(y) * width;
        for (gp::UInt32 x = 0u; x < width; ++x)
        {
            outRow[x] = decodeLinearTexel(format, row + static_cast<gp::USize>(x) * texelBytes, isSRGB);
        }
    }
    return true;
}

bool convertVulkanSurfaceToFloat16Color(
    const PixelFormat format,
    const gp::UInt8* const source,
    const gp::UInt32 sourcePitch,
    const gp::UInt32 width,
    const gp::UInt32 height,
    resource::Float16Color* const outData
) noexcept
{
    const bool isHalfFormat = format == PixelFormat::R16_Float || format == PixelFormat::R16G16_Float ||
                              format == PixelFormat::R16G16B16A16_Float;
    const bool isReencodedFormat = format == PixelFormat::R32_Float || format == PixelFormat::R32G32_Float ||
                                   format == PixelFormat::R32G32B32A32_Float || format == PixelFormat::R11G11B10_Float;
    if ((!isHalfFormat && !isReencodedFormat) || source == nullptr || outData == nullptr)
    {
        return false;
    }

    const gp::UInt32 texelBytes = getVulkanSurfaceTexelBytes(format);
    for (gp::UInt32 y = 0u; y < height; ++y)
    {
        const gp::UInt8* const row = source + static_cast<gp::USize>(y) * sourcePitch;
        resource::Float16Color* const outRow = outData + static_cast<gp::USize>(y) * width;
        for (gp::UInt32 x = 0u; x < width; ++x)
        {
            const gp::UInt8* const texel = row + static_cast<gp::USize>(x) * texelBytes;
            if (format == PixelFormat::R16G16B16A16_Float)
            {
                outRow[x] = resource::Float16Color(
                    loadTexelValue<gp::UInt16>(texel),
                    loadTexelValue<gp::UInt16>(texel + 2u),
                    loadTexelValue<gp::UInt16>(texel + 4u),
                    loadTexelValue<gp::UInt16>(texel + 6u)
                );
            }
            else if (format == PixelFormat::R16G16_Float)
            {
                outRow[x] = resource::Float16Color(
                    loadTexelValue<gp::UInt16>(texel), loadTexelValue<gp::UInt16>(texel + 2u), 0u, kHalfOne
                );
            }
            else if (format == PixelFormat::R16_Float)
            {
                outRow[x] = resource::Float16Color(loadTexelValue<gp::UInt16>(texel), 0u, 0u, kHalfOne);
            }
            else
            {
                const gp::math::LinearColor color = decodeLinearTexel(format, texel, false);
                outRow[x] = resource::Float16Color(
                    floatToHalf(color.r), floatToHalf(color.g), floatToHalf(color.b), floatToHalf(color.a)
                );
            }
        }
    }
    return true;
}

}   // namespace gp::rhi::vulkan
