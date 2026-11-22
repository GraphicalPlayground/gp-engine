// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/color/Color.hpp"
#include "console/ConsoleManager.hpp"   // IWYU pragma: keep
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"    // IWYU pragma: keep
#include "containers/strings/CharTraits.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "containers/strings/StringEnums.hpp"
#include "containers/views/StringView.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Constants.hpp"
#include "maths/base/FloatingPoint.hpp"
#include "maths/base/Interpolation.hpp"
#include "maths/color/ColorSpace.hpp"
#include "maths/color/LinearColor.hpp"
#include "maths/random/RandomStream.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"
#include <bit>
#include <charconv>
#include <cmath>

#if GP_ARCHITECTURE_X86_FAMILY
    #include <emmintrin.h>
#endif

namespace gp::math
{

namespace
{

// clang-format off
/// @brief Rygorous (Fabian Giesen) fast IEEE-754 binary32 -> 8-bit sRGB piecewise linear interpolation table.
/// @details Guarantees exact round-trip identity across all 256 sRGB code values (`LinearColor(c).toColorSRGB() == c`).
constexpr gp::UInt32 kFp32ToSrgb8Tab4[104] =
{
    0x0073000du, 0x007a000du, 0x0080000du, 0x0087000du, 0x008d000du, 0x0094000du, 0x009a000du, 0x00a1000du,
    0x00a7001au, 0x00b4001au, 0x00c1001au, 0x00ce001au, 0x00da001au, 0x00e7001au, 0x00f4001au, 0x0101001au,
    0x010e0033u, 0x01280033u, 0x01410033u, 0x015b0033u, 0x01750033u, 0x018f0033u, 0x01a80033u, 0x01c20033u,
    0x01dc0067u, 0x020f0067u, 0x02430067u, 0x02760067u, 0x02aa0067u, 0x02dd0067u, 0x03110067u, 0x03440067u,
    0x037800ceu, 0x03df00ceu, 0x044600ceu, 0x04ad00ceu, 0x051400ceu, 0x057b00c5u, 0x05dd00bcu, 0x063b00b5u,
    0x06970158u, 0x07420142u, 0x07e30130u, 0x087b0120u, 0x090b0112u, 0x09940106u, 0x0a1700fcu, 0x0a9500f2u,
    0x0b0f01cbu, 0x0bf401aeu, 0x0ccb0195u, 0x0d950180u, 0x0e56016eu, 0x0f0d015eu, 0x0fbc0150u, 0x10630143u,
    0x11070264u, 0x1238023eu, 0x1357021du, 0x14660201u, 0x156601e9u, 0x165a01d3u, 0x174401c0u, 0x182401afu,
    0x18fe0331u, 0x1a9602feu, 0x1c1502d2u, 0x1d7e02adu, 0x1ed4028du, 0x201a0270u, 0x21520256u, 0x227d0240u,
    0x239f0443u, 0x25c003feu, 0x27bf03c4u, 0x29a10392u, 0x2b6a0367u, 0x2d1d0341u, 0x2ebe031fu, 0x304d0300u,
    0x31d105b0u, 0x34a80555u, 0x37520507u, 0x39d504c5u, 0x3c37048bu, 0x3e7c0458u, 0x40a8042au, 0x42bd0401u,
    0x44c20798u, 0x488e071eu, 0x4c1c06b6u, 0x4f76065du, 0x52a50610u, 0x55ac05ccu, 0x5892058fu, 0x5b590559u,
    0x5e0c0a23u, 0x631c0980u, 0x67db08f6u, 0x6c55087fu, 0x70940818u, 0x74a007bdu, 0x787d076cu, 0x7c330723u
};
// clang-format on

/// @brief Scalar fast conversion of a single linear float channel to an 8-bit sRGB integer.
/// @param[in] inVal Input linear float channel.
/// @return 8-bit sRGB encoded integer (`[0, 255]`).
[[maybe_unused]] [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt8
    linearToSrgbUCharFast(const gp::Float32 inVal) noexcept
{
    constexpr gp::UInt32 kAlmostOneBits = 0x3f7fffffu;        // 1.0f - eps
    constexpr gp::UInt32 kMinValBits = (127u - 13u) << 23u;   // 2^-13
    const gp::Float32 almostOne = std::bit_cast<gp::Float32>(kAlmostOneBits);
    const gp::Float32 minVal = std::bit_cast<gp::Float32>(kMinValBits);

    // Clamp to [2^-13, 1 - eps]; NaNs fail `!(inVal > minVal)` and map to `minVal` (which quantizes to 0).
    gp::Float32 clamped = (inVal > minVal) ? inVal : minVal;
    if (clamped > almostOne)
    {
        clamped = almostOne;
    }

    const gp::UInt32 fBits = std::bit_cast<gp::UInt32>(clamped);
    const gp::UInt32 tab = kFp32ToSrgb8Tab4[(fBits - kMinValBits) >> 20u];
    const gp::UInt32 bias = (tab >> 16u) << 9u;
    const gp::UInt32 scale = tab & 0xFFFFu;
    const gp::UInt32 t = (fBits >> 12u) & 0xFFu;

    return static_cast<gp::UInt8>((bias + scale * t) >> 16u);
}

#if GP_ARCHITECTURE_X86_FAMILY
/// @brief SSE2-accelerated conversion of a `LinearColor` to an sRGB-encoded 8-bit `Color`.
/// @param[in] inColor Source linear color.
/// @return Packed 8-bit sRGB `Color`.
[[nodiscard]] GP_FORCEINLINE_HINT Color convertLinearToSRGBSSE2(const LinearColor& inColor) noexcept
{
    const __m128 inRGBA = _mm_loadu_ps(&inColor.r);

    const __m128i almostOneInt = _mm_set1_epi32(0x3f7fffff);
    const __m128 almostOne = _mm_castsi128_ps(almostOneInt);
    const __m128i minValInt = _mm_set1_epi32((127 - 13) << 23);
    const __m128 minValFlt = _mm_castsi128_ps(minValInt);

    // NOTE: In SSE2 `_mm_max_ps(a, b)` returns the second operand `b` whenever `a` is NaN!
    // Therefore `inRGBA` MUST be the first argument and `minValFlt` the second so NaNs clamp to `minValFlt` (0).
    const __m128 clampedLo = _mm_max_ps(inRGBA, minValFlt);
    const __m128 inClamped = _mm_min_ps(clampedLo, almostOne);

    const __m128i tabIndexVec = _mm_srli_epi32(_mm_sub_epi32(_mm_castps_si128(inClamped), minValInt), 20);

    const __m128i tabValR =
        _mm_cvtsi32_si128(static_cast<int>(kFp32ToSrgb8Tab4[static_cast<gp::UInt32>(_mm_cvtsi128_si32(tabIndexVec))]));
    const __m128i tabValG = _mm_cvtsi32_si128(
        static_cast<int>(kFp32ToSrgb8Tab4[static_cast<gp::UInt32>(_mm_extract_epi16(tabIndexVec, 2))])
    );
    const __m128i tabValB = _mm_cvtsi32_si128(
        static_cast<int>(kFp32ToSrgb8Tab4[static_cast<gp::UInt32>(_mm_extract_epi16(tabIndexVec, 4))])
    );

    const __m128i tabValRG = _mm_unpacklo_epi32(tabValR, tabValG);
    const __m128i tabValsRGB = _mm_unpacklo_epi64(tabValRG, tabValB);

    const __m128i mantissaLerpFactor =
        _mm_and_si128(_mm_srli_epi32(_mm_castps_si128(inClamped), 12), _mm_set1_epi32(0xff));
    const __m128i finalMultiplier = _mm_or_si128(mantissaLerpFactor, _mm_set1_epi32(512 << 16));

    const __m128i interpolatedRGB = _mm_srli_epi32(_mm_madd_epi16(tabValsRGB, finalMultiplier), 16);

    const __m128 scaledBiasedA =
        _mm_add_ps(_mm_mul_ps(inClamped, _mm_setr_ps(0.0f, 0.0f, 0.0f, 255.0f)), _mm_set1_ps(0.5f));
    const __m128i finalA = _mm_cvttps_epi32(scaledBiasedA);

    const __m128i finalRGBA = _mm_or_si128(interpolatedRGB, finalA);
    const __m128i finalBGRA = _mm_shuffle_epi32(finalRGBA, _MM_SHUFFLE(3, 0, 1, 2));

    const __m128i packed16 = _mm_packs_epi32(finalBGRA, finalBGRA);
    const __m128i packed8 = _mm_packus_epi16(packed16, packed16);

    return Color(static_cast<gp::UInt32>(_mm_cvtsi128_si32(packed8)));
}
#endif

/// @brief Converts a single hex character (`'0'-'9'`, `'a'-'f'`, `'A'-'F'`) to its integer value `[0, 15]`, or `-1`.
/// @param[in] ch Input character.
/// @return Integer nibble in `[0, 15]`, or `-1` if not a valid hex digit.
[[nodiscard]] constexpr gp::Int32 parseHexDigit(const char ch) noexcept
{
    return gp::container::CharTraits<char>::hexDigitToValue(ch);
}

/// @brief Extracts a token value following a case-insensitive key prefix (such as `"R="`) inside `source`.
/// @param[in] source Full input string view (e.g. `"(R=255,G=128,B=64,A=255)"`).
/// @param[in] keyPrefix Key prefix to locate (e.g. `"R="`).
/// @param[out] outTokenView Receives the trimmed value token view up to the next `,`, `)`, or whitespace delimiter.
/// @return True if `keyPrefix` was found and a non-empty token follows it.
[[nodiscard]] constexpr bool extractKeyValueToken(
    const gp::StringView source, const gp::StringView keyPrefix, gp::StringView& outTokenView
) noexcept
{
    const gp::USize keyPos = source.find(keyPrefix, 0u, gp::container::SearchCase::IgnoreCase);
    if (keyPos == gp::StringView::npos)
    {
        return false;
    }

    gp::USize valueStart = keyPos + keyPrefix.size();
    while (valueStart < source.size() && gp::container::CharTraits<char>::isWhitespace(source[valueStart]))
    {
        ++valueStart;
    }

    gp::USize valueEnd = valueStart;
    while (valueEnd < source.size())
    {
        const char ch = source[valueEnd];
        if (ch == ',' || ch == ')' || ch == ';' || gp::container::CharTraits<char>::isWhitespace(ch))
        {
            break;
        }
        ++valueEnd;
    }

    if (valueEnd <= valueStart)
    {
        return false;
    }

    outTokenView = source.substr(valueStart, valueEnd - valueStart);
    return true;
}

/// @brief Parses an 8-bit integer value `[0, 255]` for `keyPrefix` inside `source`.
/// @param[in] source Full input string view.
/// @param[in] keyPrefix Key prefix (e.g. `"R="`).
/// @param[out] outChannel Receives the clamped 8-bit value on success.
/// @return True if found and parsed as a valid integer.
[[nodiscard]] bool
    parseUInt8Channel(const gp::StringView source, const gp::StringView keyPrefix, gp::UInt8& outChannel) noexcept
{
    gp::StringView token{};
    if (!extractKeyValueToken(source, keyPrefix, token))
    {
        return false;
    }

    gp::Int32 parsedInt = 0;
    if (!token.tryParseInt32(parsedInt, 10))
    {
        return false;
    }

    outChannel = static_cast<gp::UInt8>(gp::math::clamp(parsedInt, 0, 255));
    return true;
}

/// @brief Parses a 32-bit float value for `keyPrefix` inside `source`.
/// @param[in] source Full input string view.
/// @param[in] keyPrefix Key prefix (e.g. `"R="`).
/// @param[out] outChannel Receives the parsed float value on success.
/// @return True if found and parsed as a valid float.
[[nodiscard]] bool
    parseFloat32Channel(const gp::StringView source, const gp::StringView keyPrefix, gp::Float32& outChannel) noexcept
{
    gp::StringView token{};
    if (!extractKeyValueToken(source, keyPrefix, token))
    {
        return false;
    }

    gp::Float32 parsedFloat = 0.0f;
    if (!token.tryParseFloat32(parsedFloat))
    {
        return false;
    }

    outChannel = parsedFloat;
    return true;
}

}   // namespace

/// @section Color Out-of-Line Implementations

LinearColor Color::fromRGBE() const noexcept
{
    if (a == 0u)
    {
        return LinearColor::kBlack;
    }

    // The extra -8 divides by 256; +0.5f centers the [0, 255] mantissa bucket into [0.5, 255.5].
    const gp::Float32 scale = std::ldexp(1.0f, static_cast<int>(a) - (128 + 8));
    return LinearColor(
        (static_cast<gp::Float32>(r) + 0.5f) * scale,
        (static_cast<gp::Float32>(g) + 0.5f) * scale,
        (static_cast<gp::Float32>(b) + 0.5f) * scale,
        1.0f
    );
}

Color Color::fromHex(const gp::StringView hexString) noexcept
{
    if (hexString.isEmpty())
    {
        return Color(0u, 0u, 0u, 0u);
    }

    const gp::USize startIndex = (hexString[0u] == '#') ? 1u : 0u;
    const gp::USize hexLen = hexString.size() - startIndex;

    if (hexLen == 3u)
    {
        const gp::Int32 rNib = parseHexDigit(hexString[startIndex + 0u]);
        const gp::Int32 gNib = parseHexDigit(hexString[startIndex + 1u]);
        const gp::Int32 bNib = parseHexDigit(hexString[startIndex + 2u]);
        if (rNib < 0 || gNib < 0 || bNib < 0)
        {
            return Color(0u, 0u, 0u, 0u);
        }

        return Color(
            static_cast<gp::UInt8>((rNib << 4) + rNib),
            static_cast<gp::UInt8>((gNib << 4) + gNib),
            static_cast<gp::UInt8>((bNib << 4) + bNib),
            255u
        );
    }

    if (hexLen == 6u)
    {
        const gp::Int32 rHi = parseHexDigit(hexString[startIndex + 0u]);
        const gp::Int32 rLo = parseHexDigit(hexString[startIndex + 1u]);
        const gp::Int32 gHi = parseHexDigit(hexString[startIndex + 2u]);
        const gp::Int32 gLo = parseHexDigit(hexString[startIndex + 3u]);
        const gp::Int32 bHi = parseHexDigit(hexString[startIndex + 4u]);
        const gp::Int32 bLo = parseHexDigit(hexString[startIndex + 5u]);

        if (rHi < 0 || rLo < 0 || gHi < 0 || gLo < 0 || bHi < 0 || bLo < 0)
        {
            return Color(0u, 0u, 0u, 0u);
        }

        return Color(
            static_cast<gp::UInt8>((rHi << 4) + rLo),
            static_cast<gp::UInt8>((gHi << 4) + gLo),
            static_cast<gp::UInt8>((bHi << 4) + bLo),
            255u
        );
    }

    if (hexLen == 8u)
    {
        const gp::Int32 rHi = parseHexDigit(hexString[startIndex + 0u]);
        const gp::Int32 rLo = parseHexDigit(hexString[startIndex + 1u]);
        const gp::Int32 gHi = parseHexDigit(hexString[startIndex + 2u]);
        const gp::Int32 gLo = parseHexDigit(hexString[startIndex + 3u]);
        const gp::Int32 bHi = parseHexDigit(hexString[startIndex + 4u]);
        const gp::Int32 bLo = parseHexDigit(hexString[startIndex + 5u]);
        const gp::Int32 aHi = parseHexDigit(hexString[startIndex + 6u]);
        const gp::Int32 aLo = parseHexDigit(hexString[startIndex + 7u]);

        if (rHi < 0 || rLo < 0 || gHi < 0 || gLo < 0 || bHi < 0 || bLo < 0 || aHi < 0 || aLo < 0)
        {
            return Color(0u, 0u, 0u, 0u);
        }

        return Color(
            static_cast<gp::UInt8>((rHi << 4) + rLo),
            static_cast<gp::UInt8>((gHi << 4) + gLo),
            static_cast<gp::UInt8>((bHi << 4) + bLo),
            static_cast<gp::UInt8>((aHi << 4) + aLo)
        );
    }

    return Color(0u, 0u, 0u, 0u);
}

Color Color::makeRandomColor() noexcept
{
    return LinearColor::makeRandomColor().toColor(true);
}

Color Color::makeRandomSeededColor(const gp::Int32 seed) noexcept
{
    return LinearColor::makeRandomSeededColor(seed).toColor(true);
}

Color Color::makeFromColorTemperature(const gp::Float32 temp) noexcept
{
    return LinearColor::makeFromColorTemperature(temp).toColor(true);
}

Color Color::makeRedToGreenColorFromScalar(const gp::Float32 scalar) noexcept
{
    const gp::Float32 redSclr = gp::math::clamp((1.0f - scalar) / 0.5f, 0.0f, 1.0f);
    const gp::Float32 greenSclr = gp::math::clamp(scalar / 0.5f, 0.0f, 1.0f);
    const gp::UInt8 outR = static_cast<gp::UInt8>(gp::math::truncToInt(255.0f * redSclr));
    const gp::UInt8 outG = static_cast<gp::UInt8>(gp::math::truncToInt(255.0f * greenSclr));
    return Color(outR, outG, 0u, 255u);
}

gp::String Color::toHex() const noexcept
{
    using Traits = gp::container::CharTraits<char>;
    const char hexChars[8] = { Traits::valueToHexDigitUpper(static_cast<gp::UInt8>(r >> 4u)),
                               Traits::valueToHexDigitUpper(static_cast<gp::UInt8>(r & 0x0Fu)),
                               Traits::valueToHexDigitUpper(static_cast<gp::UInt8>(g >> 4u)),
                               Traits::valueToHexDigitUpper(static_cast<gp::UInt8>(g & 0x0Fu)),
                               Traits::valueToHexDigitUpper(static_cast<gp::UInt8>(b >> 4u)),
                               Traits::valueToHexDigitUpper(static_cast<gp::UInt8>(b & 0x0Fu)),
                               Traits::valueToHexDigitUpper(static_cast<gp::UInt8>(a >> 4u)),
                               Traits::valueToHexDigitUpper(static_cast<gp::UInt8>(a & 0x0Fu)) };
    return gp::String(hexChars, 8u);
}

gp::String Color::toString() const noexcept
{
    char buffer[32]{};
    char* ptr = buffer;
    char* const end = buffer + sizeof(buffer);

    for (const char ch: gp::StringView("(R="))
    {
        *ptr++ = ch;
    }
    const std::to_chars_result resR = std::to_chars(ptr, end, static_cast<gp::UInt32>(r), 10);
    ptr = (resR.ec == std::errc{}) ? resR.ptr : ptr;

    for (const char ch: gp::StringView(",G="))
    {
        *ptr++ = ch;
    }
    const std::to_chars_result resG = std::to_chars(ptr, end, static_cast<gp::UInt32>(g), 10);
    ptr = (resG.ec == std::errc{}) ? resG.ptr : ptr;

    for (const char ch: gp::StringView(",B="))
    {
        *ptr++ = ch;
    }
    const std::to_chars_result resB = std::to_chars(ptr, end, static_cast<gp::UInt32>(b), 10);
    ptr = (resB.ec == std::errc{}) ? resB.ptr : ptr;

    for (const char ch: gp::StringView(",A="))
    {
        *ptr++ = ch;
    }
    const std::to_chars_result resA = std::to_chars(ptr, end, static_cast<gp::UInt32>(a), 10);
    ptr = (resA.ec == std::errc{}) ? resA.ptr : ptr;

    *ptr++ = ')';

    return gp::String(buffer, static_cast<gp::USize>(ptr - buffer));
}

bool Color::initFromString(const gp::StringView sourceString) noexcept
{
    r = 0u;
    g = 0u;
    b = 0u;
    a = 255u;

    const bool success = parseUInt8Channel(sourceString, "R=", r) && parseUInt8Channel(sourceString, "G=", g) &&
                         parseUInt8Channel(sourceString, "B=", b);

    // Alpha is optional; preserve default 255 if absent or invalid.
    (void)parseUInt8Channel(sourceString, "A=", a);

    return success;
}

/// @section LinearColor Out-of-Line Implementations

Color LinearColor::toRGBE() const noexcept
{
    if (!gp::math::isFinite(r) || !gp::math::isFinite(g) || !gp::math::isFinite(b))
    {
        return Color(0u, 0u, 0u, 0u);
    }

    const gp::Float32 primary = gp::math::max(r, g, b);
    if (primary < 1.0e-32f)
    {
        return Color(0u, 0u, 0u, 0u);
    }

    const gp::Float32 nonNegativeR = gp::math::max(r, 0.0f);
    const gp::Float32 nonNegativeG = gp::math::max(g, 0.0f);
    const gp::Float32 nonNegativeB = gp::math::max(b, 0.0f);

    const gp::Int32 exponent = 1 + static_cast<gp::Int32>(std::logbf(primary));
    const gp::Float32 scale = std::ldexp(1.0f, -exponent + 8);

    const gp::Int32 scaledR = gp::math::clamp(static_cast<gp::Int32>(nonNegativeR * scale), 0, 255);
    const gp::Int32 scaledG = gp::math::clamp(static_cast<gp::Int32>(nonNegativeG * scale), 0, 255);
    const gp::Int32 scaledB = gp::math::clamp(static_cast<gp::Int32>(nonNegativeB * scale), 0, 255);
    const gp::Int32 biasedExp = gp::math::clamp(exponent + 128, 0, 255);

    return Color(
        static_cast<gp::UInt8>(scaledR),
        static_cast<gp::UInt8>(scaledG),
        static_cast<gp::UInt8>(scaledB),
        static_cast<gp::UInt8>(biasedExp)
    );
}

Color LinearColor::toColorSRGB() const noexcept
{
#if GP_ARCHITECTURE_X86_FAMILY
    return convertLinearToSRGBSSE2(*this);
#else
    return Color(
        linearToSrgbUCharFast(r),
        linearToSrgbUCharFast(g),
        linearToSrgbUCharFast(b),
        static_cast<gp::UInt8>(0.5f + clamp01NansTo0(a) * 255.0f)
    );
#endif
}

LinearColor LinearColor::makeFromHSV8(const gp::UInt8 h, const gp::UInt8 s, const gp::UInt8 v) noexcept
{
    const LinearColor hsv(
        static_cast<gp::Float32>(h) * (360.0f / 256.0f),
        static_cast<gp::Float32>(s) * (1.0f / 255.0f),
        static_cast<gp::Float32>(v) * (1.0f / 255.0f),
        1.0f
    );
    return hsv.hsvToLinearRGB();
}

LinearColor LinearColor::linearRGBToHSV() const noexcept
{
    const gp::Float32 rgbMin = gp::math::min(r, g, b);
    const gp::Float32 rgbMax = gp::math::max(r, g, b);
    const gp::Float32 rgbRange = rgbMax - rgbMin;

    if (rgbMax <= 0.0f)
    {
        return LinearColor(0.0f, 0.0f, 0.0f, a);
    }

    gp::Float32 hue = 0.0f;
    if (rgbMax != rgbMin)
    {
        if (rgbMax == r)
        {
            hue = std::fmod((((g - b) / rgbRange) * 60.0f) + 360.0f, 360.0f);
        }
        else if (rgbMax == g)
        {
            hue = (((b - r) / rgbRange) * 60.0f) + 120.0f;
        }
        else
        {
            hue = (((r - g) / rgbRange) * 60.0f) + 240.0f;
        }
    }

    const gp::Float32 saturation = (rgbMax == 0.0f) ? 0.0f : (rgbRange / rgbMax);
    const gp::Float32 value = rgbMax;

    return LinearColor(hue, saturation, value, a);
}

LinearColor LinearColor::hsvToLinearRGB() const noexcept
{
    gp::Float32 hue = std::fmod(r, 360.0f);
    if (hue < 0.0f)
    {
        hue += 360.0f;
    }

    const gp::Float32 saturation = gp::math::clamp(g, 0.0f, 1.0f);
    const gp::Float32 value = b;

    const gp::Float32 hDiv60 = hue / 60.0f;
    const gp::UInt32 hDiv60Floor = static_cast<gp::UInt32>(hDiv60);
    const gp::Float32 hDiv60Fraction = hDiv60 - static_cast<gp::Float32>(hDiv60Floor);

    const gp::Float32 rgbValues[4] = { value,
                                       value * (1.0f - saturation),
                                       value * (1.0f - (hDiv60Fraction * saturation)),
                                       value * (1.0f - ((1.0f - hDiv60Fraction) * saturation)) };

    constexpr gp::UInt32 kRgbSwizzle[6][3] = {
        { 0u, 3u, 1u },
        { 2u, 0u, 1u },
        { 1u, 0u, 3u },
        { 1u, 2u, 0u },
        { 3u, 1u, 0u },
        { 0u, 1u, 2u }
    };

    const gp::UInt32 swizzleIndex = hDiv60Floor % 6u;
    return LinearColor(
        rgbValues[kRgbSwizzle[swizzleIndex][0]],
        rgbValues[kRgbSwizzle[swizzleIndex][1]],
        rgbValues[kRgbSwizzle[swizzleIndex][2]],
        a
    );
}

LinearColor
    LinearColor::lerpUsingHSV(const LinearColor& from, const LinearColor& to, const gp::Float32 progress) noexcept
{
    const LinearColor fromHSV = from.linearRGBToHSV();
    const LinearColor toHSV = to.linearRGBToHSV();

    gp::Float32 fromHue = fromHSV.r;
    gp::Float32 toHue = toHSV.r;

    if (gp::math::abs(fromHue - toHue) > 180.0f)
    {
        if (toHue > fromHue)
        {
            fromHue += 360.0f;
        }
        else
        {
            toHue += 360.0f;
        }
    }

    gp::Float32 newHue = gp::math::lerp(fromHue, toHue, progress);
    newHue = std::fmod(newHue, 360.0f);
    if (newHue < 0.0f)
    {
        newHue += 360.0f;
    }

    const gp::Float32 newSaturation = gp::math::lerp(fromHSV.g, toHSV.g, progress);
    const gp::Float32 newValue = gp::math::lerp(fromHSV.b, toHSV.b, progress);

    LinearColor interpolated = LinearColor(newHue, newSaturation, newValue, 1.0f).hsvToLinearRGB();
    interpolated.a = gp::math::lerp(from.a, to.a, progress);
    return interpolated;
}

gp::Float32 LinearColor::getLuminance() const noexcept
{
    if (gp::console::IConsoleManager::isGlobalInitialized())
    {
        const gp::console::IConsoleVariable* const cvar =
            gp::console::IConsoleManager::get().findConsoleVariable("r.LegacyLuminanceFactors");
        if (cvar != nullptr && cvar->getInt() != 0)
        {
            return r * 0.3f + g * 0.59f + b * 0.11f;
        }
    }

    const LinearColor factors = ColorSpace::getWorking().getLuminanceFactors();
    return r * factors.r + g * factors.g + b * factors.b;
}

LinearColor LinearColor::desaturate(const gp::Float32 desaturation) const noexcept
{
    const gp::Float32 lum = getLuminance();
    return gp::math::lerp(*this, LinearColor(lum, lum, lum, 0.0f), desaturation);
}

LinearColor LinearColor::makeRandomColor() noexcept
{
    static gp::thread::Atomic<gp::UInt32> s_hue(0u);
    const gp::UInt32 hue = s_hue.fetchAdd(157u, gp::MemoryOrder::Relaxed);
    return makeFromHSV8(static_cast<gp::UInt8>(hue), 255u, 255u);
}

LinearColor LinearColor::makeRandomSeededColor(const gp::Int32 seed) noexcept
{
    RandomStream stream(static_cast<gp::UInt64>(static_cast<gp::UInt32>(seed)));
    const gp::Float32 outR = stream.nextFloat();
    const gp::Float32 outG = stream.nextFloat();
    const gp::Float32 outB = stream.nextFloat();
    return LinearColor(outR, outG, outB, 1.0f);
}

LinearColor LinearColor::makeFromColorTemperature(const gp::Float32 temp) noexcept
{
    const gp::Float32 clampedTemp = gp::math::clamp(temp, 1000.0f, 15000.0f);

    // Approximate Planckian locus in CIE 1960 UCS
    const gp::Float32 u = (0.860117757f + 1.54118254e-4f * clampedTemp + 1.28641212e-7f * clampedTemp * clampedTemp) /
                          (1.0f + 8.42420235e-4f * clampedTemp + 7.08145163e-7f * clampedTemp * clampedTemp);
    const gp::Float32 v = (0.317398726f + 4.22806245e-5f * clampedTemp + 4.20481691e-8f * clampedTemp * clampedTemp) /
                          (1.0f - 2.89741816e-5f * clampedTemp + 1.61456053e-7f * clampedTemp * clampedTemp);

    const gp::Float32 denom = 2.0f * u - 8.0f * v + 4.0f;
    const gp::Float32 x = (3.0f * u) / denom;
    const gp::Float32 y = (2.0f * v) / denom;
    const gp::Float32 z = 1.0f - x - y;

    constexpr gp::Float32 kY = 1.0f;
    const gp::Float32 bigX = (kY / y) * x;
    const gp::Float32 bigZ = (kY / y) * z;

    // XYZ to RGB with ITU-R BT.709 / sRGB primaries
    const gp::Float32 outR = 3.2404542f * bigX + -1.5371385f * kY + -0.4985314f * bigZ;
    const gp::Float32 outG = -0.9692660f * bigX + 1.8760108f * kY + 0.0415560f * bigZ;
    const gp::Float32 outB = 0.0556434f * bigX + -0.2040259f * kY + 1.0572252f * bigZ;

    return LinearColor(gp::math::max(0.0f, outR), gp::math::max(0.0f, outG), gp::math::max(0.0f, outB), 1.0f);
}

gp::Float32 LinearColor::evaluateBezier(
    const LinearColor* const controlPoints, const gp::Int32 numPoints, gp::Vector<LinearColor>& outPoints
) noexcept
{
    if (controlPoints == nullptr || numPoints < 2)
    {
        return 0.0f;
    }

    const gp::Float32 q = 1.0f / static_cast<gp::Float32>(numPoints - 1);

    const LinearColor& p0 = controlPoints[0];
    const LinearColor& p1 = controlPoints[1];
    const LinearColor& p2 = controlPoints[2];
    const LinearColor& p3 = controlPoints[3];

    // Forward-difference coefficients for cubic Bezier curve
    const LinearColor a = p0;
    const LinearColor b = (p1 - p0) * 3.0f;
    const LinearColor c = (p2 - p1 * 2.0f + p0) * 3.0f;
    const LinearColor d = p3 - p2 * 3.0f + p1 * 3.0f - p0;

    LinearColor s = a;
    LinearColor u = b * q + c * (q * q) + d * (q * q * q);
    LinearColor v = c * (2.0f * q * q) + d * (6.0f * q * q * q);
    const LinearColor w = d * (6.0f * q * q * q);

    outPoints.reserve(outPoints.size() + static_cast<gp::USize>(numPoints));
    outPoints.pushBack(p0);

    LinearColor oldPos = p0;
    gp::Float32 totalLength = 0.0f;

    for (gp::Int32 i = 1; i < numPoints; ++i)
    {
        s += u;
        u += v;
        v += w;

        totalLength += dist(s, oldPos);
        oldPos = s;
        outPoints.pushBack(s);
    }

    return totalLength;
}

gp::String LinearColor::toString() const noexcept
{
    char buffer[128]{};
    char* ptr = buffer;
    char* const end = buffer + sizeof(buffer);

    const gp::StringView prefixes[4] = {
        gp::StringView("(R="), gp::StringView(",G="), gp::StringView(",B="), gp::StringView(",A=")
    };
    const gp::Float32 channels[4] = { r, g, b, a };

    for (gp::USize idx = 0u; idx < 4u; ++idx)
    {
        for (const char ch: prefixes[idx])
        {
            *ptr++ = ch;
        }

        const std::to_chars_result res = std::to_chars(ptr, end, channels[idx], std::chars_format::fixed, 6);
        if (res.ec == std::errc{})
        {
            ptr = res.ptr;
        }
        else
        {
            for (const char ch: gp::StringView("0.000000"))
            {
                *ptr++ = ch;
            }
        }
    }

    *ptr++ = ')';

    return gp::String(buffer, static_cast<gp::USize>(ptr - buffer));
}

bool LinearColor::initFromString(const gp::StringView sourceString) noexcept
{
    r = 0.0f;
    g = 0.0f;
    b = 0.0f;
    a = 1.0f;

    const bool success = parseFloat32Channel(sourceString, "R=", r) && parseFloat32Channel(sourceString, "G=", g) &&
                         parseFloat32Channel(sourceString, "B=", b);

    // Alpha is optional; preserve default 1.0f if absent or invalid.
    (void)parseFloat32Channel(sourceString, "A=", a);

    return success;
}

void computeAndFixedColorAndIntensity(
    const LinearColor& inLinearColor, Color& outColor, gp::Float32& outIntensity
) noexcept
{
    const gp::Float32 maxComponent = gp::math::max(
        Constants<gp::Float32>::delta, gp::math::max(inLinearColor.r, gp::math::max(inLinearColor.g, inLinearColor.b))
    );
    outColor = (inLinearColor / maxComponent).toColor(true);
    outIntensity = maxComponent;
}

void convertLinearColorsToColorSRGB(
    const LinearColor* const inLinearColors, Color* const outColorsSRGB, const gp::Int64 inCount
) noexcept
{
    if (inLinearColors == nullptr || outColorsSRGB == nullptr || inCount <= 0)
    {
        return;
    }

    for (gp::Int64 index = 0; index < inCount; ++index)
    {
        outColorsSRGB[index] = inLinearColors[index].toColorSRGB();
    }
}

}   // namespace gp::math
