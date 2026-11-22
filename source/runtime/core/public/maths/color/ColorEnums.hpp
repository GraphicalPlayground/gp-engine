// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/color/ColorForward.hpp"      // IWYU pragma: keep

namespace gp::math
{

/// @brief Enumerates the gamma transfer curves used for 8-bit `Color` to `LinearColor` conversion.
enum class GammaSpace : gp::UInt8
{
    /// @brief No gamma correction is applied; values are already in linear space.
    Linear = 0u,

    /// @brief Simplified power-law gamma curve `pow(x, 1 / 2.2)` / `pow(x, 2.2)`.
    Pow22 = 1u,

    /// @brief IEC 61966-2-1 standard piecewise sRGB transfer curve.
    sRGB = 2u,

    /// @brief Unspecified or invalid gamma space sentinel.
    Invalid = 3u
};

/// @brief Standard RGB color space definitions (primaries and white point).
/// @details Mirrors Unreal Engine's `UE::Color::EColorSpace` catalog for working-color-space and ACES/HDR pipelines.
enum class ColorSpaceType : gp::UInt8
{
    /// @brief Unspecified or custom user-defined primaries.
    None = 0u,

    /// @brief sRGB / Rec.709 color space (ITU-R BT.709 primaries, D65 white point).
    sRGB = 1u,

    /// @brief ITU-R BT.2020 Ultra-HD television color space (D65 white point).
    Rec2020 = 2u,

    /// @brief Academy Color Encoding System AP0 (2065-1 archival color space, ACES ~D60 white point).
    ACESAP0 = 3u,

    /// @brief Academy Color Encoding System AP1 (ACEScg working rendering color space, ACES ~D60 white point).
    ACESAP1 = 4u,

    /// @brief DCI-P3 theatrical projection color space (DCI ~6300K white point).
    P3DCI = 5u,

    /// @brief Display P3 / P3-D65 wide-gamut consumer display color space (D65 white point).
    P3D65 = 6u,

    /// @brief RED Wide Gamut RGB camera color space (D65 white point).
    REDWideGamut = 7u,

    /// @brief Sony S-Gamut3 camera color space (D65 white point).
    SonySGamut3 = 8u,

    /// @brief Sony S-Gamut3.Cine camera color space (D65 white point).
    SonySGamut3Cine = 9u,

    /// @brief Alexa Wide Gamut (ARRI LogC3) camera color space (D65 white point).
    AlexaWideGamut = 10u,

    /// @brief Canon Cinema Gamut camera color space (D65 white point).
    CanonCinemaGamut = 11u,

    /// @brief GoPro Protune Native camera color space (D65 white point).
    GoProProtuneNative = 12u,

    /// @brief Panasonic V-Gamut camera color space (D65 white point).
    PanasonicVGamut = 13u,

    /// @brief PLASA ANSI E1.54 entertainment lighting color space.
    PLASA_E1_54 = 14u,

    /// @brief Total number of defined `ColorSpaceType` enumerators.
    Count = 15u
};

/// @brief Total number of `ColorSpaceType` enumerators.
inline constexpr gp::USize kColorSpaceTypeCount = static_cast<gp::USize>(ColorSpaceType::Count);

/// @brief Standard CIE 1931 2-degree standard observer reference illuminant white points.
enum class WhitePoint : gp::UInt8
{
    /// @brief CIE Standard Illuminant D50 (horizon daylight, ~5003K).
    CIE1931_D50 = 0u,

    /// @brief CIE Illuminant D55 (mid-morning daylight, ~5503K).
    CIE1931_D55 = 1u,

    /// @brief ACES reference white point (~6000K, `(0.32168, 0.33767)`).
    ACES_D60 = 2u,

    /// @brief DCI-P3 theatrical projection white point (~6300K, `(0.314, 0.351)`).
    DCI_CalibrationWhite = 3u,

    /// @brief CIE Standard Illuminant D65 (noon daylight, ~6504K, `(0.3127, 0.3290)`).
    CIE1931_D65 = 4u,

    /// @brief CIE Illuminant D75 (north sky daylight, ~7504K).
    CIE1931_D75 = 5u,

    /// @brief CIE Equal-Energy radiator illuminant E (`(1/3, 1/3)`).
    CIE1931_E = 6u,

    /// @brief Total number of defined `WhitePoint` enumerators.
    Count = 7u
};

/// @brief Total number of `WhitePoint` enumerators.
inline constexpr gp::USize kWhitePointCount = static_cast<gp::USize>(WhitePoint::Count);

/// @brief Cone-response chromatic adaptation transform (CAT) methods for mapping between reference white points.
enum class ChromaticAdaptationMethod : gp::UInt8
{
    /// @brief Direct XYZ scaling without cone-response transformation (diagonal XYZ ratio).
    None = 0u,

    /// @brief Bradford cone-response matrix (ICC profile and Unreal Engine default).
    Bradford = 1u,

    /// @brief CAT02 CIECAM02 chromatic adaptation matrix.
    CAT02 = 2u,

    /// @brief Total number of defined `ChromaticAdaptationMethod` enumerators.
    Count = 3u
};

/// @brief Opto-Electronic / Electro-Optical Transfer Function (OETF / EOTF) encodings for SDR, HDR, and camera log.
enum class ColorEncoding : gp::UInt8
{
    /// @brief Linear scene-referred or display-referred light (`f(x) = x`).
    Linear = 0u,

    /// @brief IEC 61966-2-1 piecewise sRGB transfer function.
    sRGB = 1u,

    /// @brief Pure power-law gamma 2.2 transfer function.
    Gamma22 = 2u,

    /// @brief ITU-R BT.601 / BT.709 / BT.2020 broadcast camera OETF.
    BT1886 = 3u,

    /// @brief SMPTE ST 2084 Perceptual Quantizer (PQ) HDR transfer function (10,000 nits peak).
    ST2084 = 4u,

    /// @brief Cineon / Kodak printing density 10-bit film log curve.
    Cineon = 5u,

    /// @brief RED Log film curve.
    REDLog = 6u,

    /// @brief RED Log3G10 wide dynamic range camera log curve.
    REDLog3G10 = 7u,

    /// @brief Sony S-Log1 camera log curve.
    SLog1 = 8u,

    /// @brief Sony S-Log2 camera log curve.
    SLog2 = 9u,

    /// @brief Sony S-Log3 camera log curve.
    SLog3 = 10u,

    /// @brief ARRI Alexa V3 LogC (EI 800) camera log curve.
    AlexaV3LogC = 11u,

    /// @brief Canon Log camera curve.
    CanonLog = 12u,

    /// @brief GoPro ProTune log curve.
    ProTune = 13u,

    /// @brief Panasonic V-Log camera curve.
    VLog = 14u,

    /// @brief Total number of defined `ColorEncoding` enumerators.
    Count = 15u
};

/// @brief Total number of `ColorEncoding` enumerators.
inline constexpr gp::USize kColorEncodingCount = static_cast<gp::USize>(ColorEncoding::Count);

/// @brief Returns the canonical ASCII name of a `GammaSpace` enumerator.
/// @param[in] space Gamma space enumerator to stringify.
/// @return String view naming @p space.
[[nodiscard]] constexpr gp::StringView toString(const GammaSpace space) noexcept
{
    switch (space)
    {
    case GammaSpace::Linear:
        return "Linear";
    case GammaSpace::Pow22:
        return "Pow22";
    case GammaSpace::sRGB:
        return "sRGB";
    case GammaSpace::Invalid:
        // falls through
    default:
        return "Invalid";
    }
}

/// @brief Returns the canonical ASCII name of a `ColorSpaceType` enumerator.
/// @param[in] type Color space type to stringify.
/// @return String view naming @p type.
[[nodiscard]] constexpr gp::StringView toString(const ColorSpaceType type) noexcept
{
    switch (type)
    {
    case ColorSpaceType::None:
        return "None";
    case ColorSpaceType::sRGB:
        return "sRGB";
    case ColorSpaceType::Rec2020:
        return "Rec2020";
    case ColorSpaceType::ACESAP0:
        return "ACESAP0";
    case ColorSpaceType::ACESAP1:
        return "ACESAP1";
    case ColorSpaceType::P3DCI:
        return "P3DCI";
    case ColorSpaceType::P3D65:
        return "P3D65";
    case ColorSpaceType::REDWideGamut:
        return "REDWideGamut";
    case ColorSpaceType::SonySGamut3:
        return "SonySGamut3";
    case ColorSpaceType::SonySGamut3Cine:
        return "SonySGamut3Cine";
    case ColorSpaceType::AlexaWideGamut:
        return "AlexaWideGamut";
    case ColorSpaceType::CanonCinemaGamut:
        return "CanonCinemaGamut";
    case ColorSpaceType::GoProProtuneNative:
        return "GoProProtuneNative";
    case ColorSpaceType::PanasonicVGamut:
        return "PanasonicVGamut";
    case ColorSpaceType::PLASA_E1_54:
        return "PLASA_E1_54";
    case ColorSpaceType::Count:
        // falls through
    default:
        return "Unknown";
    }
}

/// @brief Returns the canonical ASCII name of a `WhitePoint` enumerator.
/// @param[in] wp White point enumerator to stringify.
/// @return String view naming @p wp.
[[nodiscard]] constexpr gp::StringView toString(const WhitePoint wp) noexcept
{
    switch (wp)
    {
    case WhitePoint::CIE1931_D50:
        return "CIE1931_D50";
    case WhitePoint::CIE1931_D55:
        return "CIE1931_D55";
    case WhitePoint::ACES_D60:
        return "ACES_D60";
    case WhitePoint::DCI_CalibrationWhite:
        return "DCI_CalibrationWhite";
    case WhitePoint::CIE1931_D65:
        return "CIE1931_D65";
    case WhitePoint::CIE1931_D75:
        return "CIE1931_D75";
    case WhitePoint::CIE1931_E:
        return "CIE1931_E";
    case WhitePoint::Count:
        // falls through
    default:
        return "Unknown";
    }
}

/// @brief Returns the canonical ASCII name of a `ChromaticAdaptationMethod` enumerator.
/// @param[in] method Chromatic adaptation method to stringify.
/// @return String view naming @p method.
[[nodiscard]] constexpr gp::StringView toString(const ChromaticAdaptationMethod method) noexcept
{
    switch (method)
    {
    case ChromaticAdaptationMethod::None:
        return "None";
    case ChromaticAdaptationMethod::Bradford:
        return "Bradford";
    case ChromaticAdaptationMethod::CAT02:
        return "CAT02";
    case ChromaticAdaptationMethod::Count:
        // falls through
    default:
        return "Unknown";
    }
}

/// @brief Returns the canonical ASCII name of a `ColorEncoding` enumerator.
/// @param[in] encoding Transfer function encoding to stringify.
/// @return String view naming @p encoding.
[[nodiscard]] constexpr gp::StringView toString(const ColorEncoding encoding) noexcept
{
    switch (encoding)
    {
    case ColorEncoding::Linear:
        return "Linear";
    case ColorEncoding::sRGB:
        return "sRGB";
    case ColorEncoding::Gamma22:
        return "Gamma22";
    case ColorEncoding::BT1886:
        return "BT1886";
    case ColorEncoding::ST2084:
        return "ST2084";
    case ColorEncoding::Cineon:
        return "Cineon";
    case ColorEncoding::REDLog:
        return "REDLog";
    case ColorEncoding::REDLog3G10:
        return "REDLog3G10";
    case ColorEncoding::SLog1:
        return "SLog1";
    case ColorEncoding::SLog2:
        return "SLog2";
    case ColorEncoding::SLog3:
        return "SLog3";
    case ColorEncoding::AlexaV3LogC:
        return "AlexaV3LogC";
    case ColorEncoding::CanonLog:
        return "CanonLog";
    case ColorEncoding::ProTune:
        return "ProTune";
    case ColorEncoding::VLog:
        return "VLog";
    case ColorEncoding::Count:
        // falls through
    default:
        return "Unknown";
    }
}

}   // namespace gp::math
