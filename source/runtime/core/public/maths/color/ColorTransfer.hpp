// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Constants.hpp"       // IWYU pragma: keep
#include "maths/color/ColorEnums.hpp"
#include "maths/color/ColorForward.hpp"   // IWYU pragma: keep
#include "maths/color/LinearColor.hpp"
#include <cmath>

namespace gp::math
{

namespace ColorTransfer
{

/// @section sRGB (IEC 61966-2-1)

/// @brief Applies the IEC 61966-2-1 sRGB OETF (linear `[0, 1]` -> sRGB encoded `[0, 1]`).
/// @param[in] value Linear scalar component.
/// @return sRGB-encoded scalar component.
[[nodiscard]] inline gp::Float32 encodeSRGB(const gp::Float32 value) noexcept
{
    if (value <= 0.0031308f)
    {
        return value * 12.92f;
    }
    return 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
}

/// @brief Applies the IEC 61966-2-1 sRGB EOTF (sRGB encoded `[0, 1]` -> linear `[0, 1]`).
/// @param[in] value sRGB-encoded scalar component.
/// @return Decoded linear scalar component.
[[nodiscard]] inline gp::Float32 decodeSRGB(const gp::Float32 value) noexcept
{
    if (value <= 0.04045f)
    {
        return value / 12.92f;
    }
    return std::pow((value + 0.055f) / 1.055f, 2.4f);
}

/// @section Pure Power-Law Gamma 2.2

/// @brief Encodes a linear value with a pure `1 / 2.2` power-law gamma curve.
/// @param[in] value Linear scalar component.
/// @return Gamma-2.2 encoded scalar.
[[nodiscard]] inline gp::Float32 encodeGamma22(const gp::Float32 value) noexcept
{
    if (value <= 0.0f)
    {
        return 0.0f;
    }
    return std::pow(value, 1.0f / 2.2f);
}

/// @brief Decodes a pure `2.2` power-law gamma value back to linear space.
/// @param[in] value Gamma-2.2 encoded scalar.
/// @return Linear scalar.
[[nodiscard]] inline gp::Float32 decodeGamma22(const gp::Float32 value) noexcept
{
    if (value <= 0.0f)
    {
        return 0.0f;
    }
    return std::pow(value, 2.2f);
}

/// @section ITU-R BT.1886 / Rec.709 / Rec.2020 OETF

/// @brief Encodes a linear value using the ITU-R BT.709 / BT.2020 / BT.1886 reference OETF.
/// @param[in] value Linear scalar component.
/// @return Encoded BT.709/BT.1886 scalar.
[[nodiscard]] inline gp::Float32 encodeBT1886(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kAlpha = 1.09929682680944f;
    constexpr gp::Float32 kBeta = 0.018053968510807f;
    if (value < kBeta)
    {
        return 4.5f * value;
    }
    return kAlpha * std::pow(value, 0.45f) - (kAlpha - 1.0f);
}

/// @brief Decodes an ITU-R BT.709 / BT.2020 / BT.1886 value back to linear space.
/// @param[in] value Encoded BT.709/BT.1886 scalar.
/// @return Linear scalar.
[[nodiscard]] inline gp::Float32 decodeBT1886(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kAlpha = 1.09929682680944f;
    constexpr gp::Float32 kBeta = 0.018053968510807f;
    if (value < 4.5f * kBeta)
    {
        return value / 4.5f;
    }
    return std::pow((value + (kAlpha - 1.0f)) / kAlpha, 1.0f / 0.45f);
}

/// @section SMPTE ST 2084 Perceptual Quantizer (PQ)

/// @brief Encodes normalized linear luminance (`1.0 == 10,000 cd/m^2`) using the SMPTE ST 2084 PQ OETF.
/// @param[in] value Linear value normalized to `[0, 1]` for `[0, 10000]` nits.
/// @return PQ-encoded value in `[0, 1]`.
[[nodiscard]] inline gp::Float32 encodeST2084(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kM1 = 2610.0f / 16384.0f;             // 0.1593017578125f
    constexpr gp::Float32 kM2 = (2523.0f / 4096.0f) * 128.0f;   // 78.84375f
    constexpr gp::Float32 kC1 = 3424.0f / 4096.0f;              // 0.8359375f
    constexpr gp::Float32 kC2 = (2413.0f / 4096.0f) * 32.0f;    // 18.8515625f
    constexpr gp::Float32 kC3 = (2392.0f / 4096.0f) * 32.0f;    // 18.6875f

    const gp::Float32 clamped = gp::math::max(0.0f, value);
    const gp::Float32 ym1 = std::pow(clamped, kM1);
    return std::pow((kC1 + kC2 * ym1) / (1.0f + kC3 * ym1), kM2);
}

/// @brief Decodes a SMPTE ST 2084 PQ signal in `[0, 1]` back to normalized linear luminance (`1.0 == 10,000 cd/m^2`).
/// @param[in] value PQ-encoded value in `[0, 1]`.
/// @return Normalized linear luminance.
[[nodiscard]] inline gp::Float32 decodeST2084(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kM1 = 2610.0f / 16384.0f;
    constexpr gp::Float32 kM2 = (2523.0f / 4096.0f) * 128.0f;
    constexpr gp::Float32 kC1 = 3424.0f / 4096.0f;
    constexpr gp::Float32 kC2 = (2413.0f / 4096.0f) * 32.0f;
    constexpr gp::Float32 kC3 = (2392.0f / 4096.0f) * 32.0f;

    const gp::Float32 clamped = gp::math::max(0.0f, value);
    const gp::Float32 np = std::pow(clamped, 1.0f / kM2);
    const gp::Float32 num = gp::math::max(0.0f, np - kC1);
    const gp::Float32 den = kC2 - kC3 * np;
    if (den <= 0.0f)
    {
        return 0.0f;
    }
    return std::pow(num / den, 1.0f / kM1);
}

/// @section Cineon Film Log

/// @brief Encodes linear light into Cineon printing density log code value `[0, 1]`.
/// @param[in] value Linear scene value.
/// @return Cineon log value.
[[nodiscard]] inline gp::Float32 encodeCineon(const gp::Float32 value) noexcept
{
    const gp::Float32 blackOffset = std::pow(10.0f, (95.0f - 685.0f) / 300.0f);
    const gp::Float32 safeVal = gp::math::max(0.0f, value);
    return (685.0f + 300.0f * std::log10(safeVal * (1.0f - blackOffset) + blackOffset)) / 1023.0f;
}

/// @brief Decodes a Cineon printing density log value back to linear light.
/// @param[in] value Cineon log value.
/// @return Linear scene value.
[[nodiscard]] inline gp::Float32 decodeCineon(const gp::Float32 value) noexcept
{
    const gp::Float32 blackOffset = std::pow(10.0f, (95.0f - 685.0f) / 300.0f);
    return (std::pow(10.0f, (1023.0f * value - 685.0f) / 300.0f) - blackOffset) / (1.0f - blackOffset);
}

/// @section RED Log & RED Log3G10

/// @brief Encodes linear light into RED Log.
/// @param[in] value Linear scene value.
/// @return RED Log encoded value.
[[nodiscard]] inline gp::Float32 encodeREDLog(const gp::Float32 value) noexcept
{
    const gp::Float32 blackOffset = std::pow(10.0f, (0.0f - 1023.0f) / 511.0f);
    const gp::Float32 safeVal = gp::math::max(0.0f, value);
    return (1023.0f + 511.0f * std::log10(safeVal * (1.0f - blackOffset) + blackOffset)) / 1023.0f;
}

/// @brief Decodes a RED Log value back to linear light.
/// @param[in] value RED Log encoded value.
/// @return Linear scene value.
[[nodiscard]] inline gp::Float32 decodeREDLog(const gp::Float32 value) noexcept
{
    const gp::Float32 blackOffset = std::pow(10.0f, (0.0f - 1023.0f) / 511.0f);
    return (std::pow(10.0f, (1023.0f * value - 1023.0f) / 511.0f) - blackOffset) / (1.0f - blackOffset);
}

/// @brief Encodes linear light into RED Log3G10.
/// @param[in] value Linear scene value.
/// @return RED Log3G10 encoded value.
[[nodiscard]] inline gp::Float32 encodeREDLog3G10(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kA = 0.224282f;
    constexpr gp::Float32 kB = 155.975327f;
    constexpr gp::Float32 kC = 0.01f;
    constexpr gp::Float32 kG = 15.1927f;

    const gp::Float32 shifted = value + kC;
    if (shifted < 0.0f)
    {
        return shifted * kG;
    }
    return kA * std::log10(shifted * kB + 1.0f);
}

/// @brief Decodes a RED Log3G10 value back to linear light.
/// @param[in] value RED Log3G10 encoded value.
/// @return Linear scene value.
[[nodiscard]] inline gp::Float32 decodeREDLog3G10(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kA = 0.224282f;
    constexpr gp::Float32 kB = 155.975327f;
    constexpr gp::Float32 kC = 0.01f;
    constexpr gp::Float32 kG = 15.1927f;

    if (value < 0.0f)
    {
        return (value / kG) - kC;
    }
    return ((std::pow(10.0f, value / kA) - 1.0f) / kB) - kC;
}

/// @section Sony S-Log1, S-Log2, and S-Log3

/// @brief Encodes linear light into Sony S-Log1.
/// @param[in] value Linear scene value.
/// @return S-Log1 encoded value.
[[nodiscard]] inline gp::Float32 encodeSLog1(const gp::Float32 value) noexcept
{
    const gp::Float32 safeVal = gp::math::max(0.0f, value) / 0.9f;
    return (43.299108f + 220.574824f * std::log10(safeVal + 0.037584f) + 61.6f) / 1023.0f;
}

/// @brief Decodes a Sony S-Log1 value back to linear light.
/// @param[in] value S-Log1 encoded value.
/// @return Linear scene value.
[[nodiscard]] inline gp::Float32 decodeSLog1(const gp::Float32 value) noexcept
{
    return (std::pow(10.0f, ((value * 1023.0f - 61.6f) - 43.299108f) / 220.574824f) - 0.037584f) * 0.9f;
}

/// @brief Encodes linear light into Sony S-Log2.
/// @param[in] value Linear scene value.
/// @return S-Log2 encoded value.
[[nodiscard]] inline gp::Float32 encodeSLog2(const gp::Float32 value) noexcept
{
    if (value < 0.0f)
    {
        return (64.0f + value * 3.53881278538813f * 876.0f / 0.9f) / 1023.0f;
    }
    return (64.0f + 876.0f * (0.432699f * std::log10(155.0f * value / 197.1f + 0.037584f) + 0.616596f + 0.03f)) /
           1023.0f;
}

/// @brief Decodes a Sony S-Log2 value back to linear light.
/// @param[in] value S-Log2 encoded value.
/// @return Linear scene value.
[[nodiscard]] inline gp::Float32 decodeSLog2(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kCut = (64.0f + 876.0f * (0.432699f * (-1.425f) + 0.616596f + 0.03f)) / 1023.0f;
    if (value < kCut)
    {
        return ((value * 1023.0f - 64.0f) * 0.9f) / (876.0f * 3.53881278538813f);
    }
    return 197.1f *
           (std::pow(10.0f, (((value * 1023.0f - 64.0f) / 876.0f) - 0.616596f - 0.03f) / 0.432699f) - 0.037584f) /
           155.0f;
}

/// @brief Encodes linear light into Sony S-Log3.
/// @param[in] value Linear scene value.
/// @return S-Log3 encoded value.
[[nodiscard]] inline gp::Float32 encodeSLog3(const gp::Float32 value) noexcept
{
    if (value >= 0.01125000f)
    {
        return (420.0f + std::log10((value + 0.01f) / (0.18f + 0.01f)) * 261.5f) / 1023.0f;
    }
    return (value * (171.2102946929f - 95.0f) / 0.01125000f + 95.0f) / 1023.0f;
}

/// @brief Decodes a Sony S-Log3 value back to linear light.
/// @param[in] value S-Log3 encoded value.
/// @return Linear scene value.
[[nodiscard]] inline gp::Float32 decodeSLog3(const gp::Float32 value) noexcept
{
    if (value >= (171.2102946929f / 1023.0f))
    {
        return std::pow(10.0f, (value * 1023.0f - 420.0f) / 261.5f) * (0.18f + 0.01f) - 0.01f;
    }
    return (value * 1023.0f - 95.0f) * 0.01125000f / (171.2102946929f - 95.0f);
}

/// @section ARRI Alexa V3 LogC (EI 800)

/// @brief Encodes linear light into ARRI Alexa V3 LogC (EI 800).
/// @param[in] value Linear scene value.
/// @return Alexa V3 LogC encoded value.
[[nodiscard]] inline gp::Float32 encodeAlexaV3LogC(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kCut = 0.010591f;
    constexpr gp::Float32 kA = 5.555556f;
    constexpr gp::Float32 kB = 0.052272f;
    constexpr gp::Float32 kC = 0.247190f;
    constexpr gp::Float32 kD = 0.385537f;
    constexpr gp::Float32 kE = 5.367655f;
    constexpr gp::Float32 kF = 0.092809f;

    if (value > kCut)
    {
        return kC * std::log10(kA * value + kB) + kD;
    }
    return kE * value + kF;
}

/// @brief Decodes an ARRI Alexa V3 LogC (EI 800) value back to linear light.
/// @param[in] value Alexa V3 LogC encoded value.
/// @return Linear scene value.
[[nodiscard]] inline gp::Float32 decodeAlexaV3LogC(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kCut = 0.010591f;
    constexpr gp::Float32 kA = 5.555556f;
    constexpr gp::Float32 kB = 0.052272f;
    constexpr gp::Float32 kC = 0.247190f;
    constexpr gp::Float32 kD = 0.385537f;
    constexpr gp::Float32 kE = 5.367655f;
    constexpr gp::Float32 kF = 0.092809f;

    if (value > (kE * kCut + kF))
    {
        return (std::pow(10.0f, (value - kD) / kC) - kB) / kA;
    }
    return (value - kF) / kE;
}

/// @section Canon Log

/// @brief Encodes linear light into Canon Log.
/// @param[in] value Linear scene value.
/// @return Canon Log encoded value.
[[nodiscard]] inline gp::Float32 encodeCanonLog(const gp::Float32 value) noexcept
{
    if (value < 0.0f)
    {
        return -(0.529136f * (std::log10(1.0f - value * 10.1596f)) - 0.0730597f);
    }
    return 0.529136f * std::log10(10.1596f * value + 1.0f) + 0.0730597f;
}

/// @brief Decodes a Canon Log value back to linear light.
/// @param[in] value Canon Log encoded value.
/// @return Linear scene value.
[[nodiscard]] inline gp::Float32 decodeCanonLog(const gp::Float32 value) noexcept
{
    if (value < 0.0730597f)
    {
        return -(std::pow(10.0f, (0.0730597f - value) / 0.529136f) - 1.0f) / 10.1596f;
    }
    return (std::pow(10.0f, (value - 0.0730597f) / 0.529136f) - 1.0f) / 10.1596f;
}

/// @section GoPro ProTune

/// @brief Encodes linear light into GoPro ProTune log.
/// @param[in] value Linear scene value.
/// @return ProTune encoded value.
[[nodiscard]] inline gp::Float32 encodeProTune(const gp::Float32 value) noexcept
{
    const gp::Float32 safeVal = gp::math::max(0.0f, value);
    return std::log(safeVal * 112.0f + 1.0f) / std::log(113.0f);
}

/// @brief Decodes a GoPro ProTune log value back to linear light.
/// @param[in] value ProTune encoded value.
/// @return Linear scene value.
[[nodiscard]] inline gp::Float32 decodeProTune(const gp::Float32 value) noexcept
{
    return (std::pow(113.0f, value) - 1.0f) / 112.0f;
}

/// @section Panasonic V-Log

/// @brief Encodes linear light into Panasonic V-Log.
/// @param[in] value Linear scene value.
/// @return V-Log encoded value.
[[nodiscard]] inline gp::Float32 encodeVLog(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kCut1 = 0.01f;
    constexpr gp::Float32 kB = 0.00873f;
    constexpr gp::Float32 kC = 0.241514f;
    constexpr gp::Float32 kD = 0.598206f;

    if (value < kCut1)
    {
        return 5.6f * value + 0.125f;
    }
    return kC * std::log10(value + kB) + kD;
}

/// @brief Decodes a Panasonic V-Log value back to linear light.
/// @param[in] value V-Log encoded value.
/// @return Linear scene value.
[[nodiscard]] inline gp::Float32 decodeVLog(const gp::Float32 value) noexcept
{
    constexpr gp::Float32 kCut2 = 0.181f;
    constexpr gp::Float32 kB = 0.00873f;
    constexpr gp::Float32 kC = 0.241514f;
    constexpr gp::Float32 kD = 0.598206f;

    if (value < kCut2)
    {
        return (value - 0.125f) / 5.6f;
    }
    return std::pow(10.0f, (value - kD) / kC) - kB;
}

/// @section Runtime Dispatch by ColorEncoding

/// @brief Applies the transfer-function encoding (OETF: linear -> encoded) specified by @p encoding to a scalar.
/// @param[in] encoding Target transfer curve.
/// @param[in] value Input linear scalar.
/// @return Encoded scalar.
[[nodiscard]] inline gp::Float32 encode(const ColorEncoding encoding, const gp::Float32 value) noexcept
{
    switch (encoding)
    {
    case ColorEncoding::Linear:
        return value;
    case ColorEncoding::sRGB:
        return encodeSRGB(value);
    case ColorEncoding::Gamma22:
        return encodeGamma22(value);
    case ColorEncoding::BT1886:
        return encodeBT1886(value);
    case ColorEncoding::ST2084:
        return encodeST2084(value);
    case ColorEncoding::Cineon:
        return encodeCineon(value);
    case ColorEncoding::REDLog:
        return encodeREDLog(value);
    case ColorEncoding::REDLog3G10:
        return encodeREDLog3G10(value);
    case ColorEncoding::SLog1:
        return encodeSLog1(value);
    case ColorEncoding::SLog2:
        return encodeSLog2(value);
    case ColorEncoding::SLog3:
        return encodeSLog3(value);
    case ColorEncoding::AlexaV3LogC:
        return encodeAlexaV3LogC(value);
    case ColorEncoding::CanonLog:
        return encodeCanonLog(value);
    case ColorEncoding::ProTune:
        return encodeProTune(value);
    case ColorEncoding::VLog:
        return encodeVLog(value);
    case ColorEncoding::Count:
        // falls through
    default:
        return value;
    }
}

/// @brief Applies the inverse transfer-function decoding (EOTF: encoded -> linear) specified by @p encoding to a
///        scalar.
/// @param[in] encoding Source transfer curve.
/// @param[in] value Input encoded scalar.
/// @return Decoded linear scalar.
[[nodiscard]] inline gp::Float32 decode(const ColorEncoding encoding, const gp::Float32 value) noexcept
{
    switch (encoding)
    {
    case ColorEncoding::Linear:
        return value;
    case ColorEncoding::sRGB:
        return decodeSRGB(value);
    case ColorEncoding::Gamma22:
        return decodeGamma22(value);
    case ColorEncoding::BT1886:
        return decodeBT1886(value);
    case ColorEncoding::ST2084:
        return decodeST2084(value);
    case ColorEncoding::Cineon:
        return decodeCineon(value);
    case ColorEncoding::REDLog:
        return decodeREDLog(value);
    case ColorEncoding::REDLog3G10:
        return decodeREDLog3G10(value);
    case ColorEncoding::SLog1:
        return decodeSLog1(value);
    case ColorEncoding::SLog2:
        return decodeSLog2(value);
    case ColorEncoding::SLog3:
        return decodeSLog3(value);
    case ColorEncoding::AlexaV3LogC:
        return decodeAlexaV3LogC(value);
    case ColorEncoding::CanonLog:
        return decodeCanonLog(value);
    case ColorEncoding::ProTune:
        return decodeProTune(value);
    case ColorEncoding::VLog:
        return decodeVLog(value);
    case ColorEncoding::Count:
        // falls through
    default:
        return value;
    }
}

/// @brief Applies the transfer-function encoding (OETF: linear -> encoded) to the RGB channels of @p color,
///        preserving alpha (`a`).
/// @param[in] encoding Target transfer curve.
/// @param[in] color Linear input color.
/// @return Encoded `LinearColor` with `a == color.a`.
[[nodiscard]] inline LinearColor encode(const ColorEncoding encoding, const LinearColor& color) noexcept
{
    return LinearColor(encode(encoding, color.r), encode(encoding, color.g), encode(encoding, color.b), color.a);
}

/// @brief Applies the inverse transfer-function decoding (EOTF: encoded -> linear) to the RGB channels of @p color,
///        preserving alpha (`a`).
/// @param[in] encoding Source transfer curve.
/// @param[in] color Encoded input color.
/// @return Linear `LinearColor` with `a == color.a`.
[[nodiscard]] inline LinearColor decode(const ColorEncoding encoding, const LinearColor& color) noexcept
{
    return LinearColor(decode(encoding, color.r), decode(encoding, color.g), decode(encoding, color.b), color.a);
}

}   // namespace ColorTransfer

}   // namespace gp::math
