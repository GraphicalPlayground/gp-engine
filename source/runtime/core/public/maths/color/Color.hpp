// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/color/ColorForward.hpp"   // IWYU pragma: keep
#include <type_traits>

namespace gp::math
{

/// @brief An 8-bit-per-channel RGBA color (`32-bit` packed BGRA layout on little-endian architectures).
/// @details Mirrors Unreal Engine's `FColor`. Stores `{ b, g, r, a }` in byte order on little-endian targets so a
///          32-bit load yields `0xAARRGGBB` (`B8G8R8A8_UNorm` GPU layout).
/// @note 8-bit integer precision is insufficient for linear-light calculations; store display-referred sRGB values in
///       `Color` via `LinearColor::toColor(true)` and use `LinearColor` for linear arithmetic and shading.
struct Color final
{
public:
    /// @brief Blue channel (`[0, 255]`, byte offset 0 on little-endian).
    gp::UInt8 b{ 0u };

    /// @brief Green channel (`[0, 255]`, byte offset 1 on little-endian).
    gp::UInt8 g{ 0u };

    /// @brief Red channel (`[0, 255]`, byte offset 2 on little-endian).
    gp::UInt8 r{ 0u };

    /// @brief Alpha channel (`[0, 255]`, byte offset 3 on little-endian).
    gp::UInt8 a{ 0u };

public:
    /// @section Constructors

    /// @brief Default constructor. Initializes all channels to zero (`(0, 0, 0, 0)`).
    [[nodiscard]] constexpr Color() noexcept = default;

    /// @brief Constructs a color from explicit 8-bit Red, Green, Blue, and optional Alpha values.
    /// @param[in] inR Red component in `[0, 255]`.
    /// @param[in] inG Green component in `[0, 255]`.
    /// @param[in] inB Blue component in `[0, 255]`.
    /// @param[in] inA Alpha component in `[0, 255]` (defaults to `255` opaque).
    [[nodiscard]] constexpr Color(
        const gp::UInt8 inR, const gp::UInt8 inG, const gp::UInt8 inB, const gp::UInt8 inA = 255u
    ) noexcept
        : b(inB)
        , g(inG)
        , r(inR)
        , a(inA)
    {}

    /// @brief Constructs a color from a raw 32-bit packed `DWColor` integer (`0xAARRGGBB` on little-endian).
    /// @param[in] inColor Packed 32-bit integer value (`(a << 24) | (r << 16) | (g << 8) | b`).
    [[nodiscard]] explicit constexpr Color(const gp::UInt32 inColor) noexcept
        : b(static_cast<gp::UInt8>(inColor & 0xFFu))
        , g(static_cast<gp::UInt8>((inColor >> 8u) & 0xFFu))
        , r(static_cast<gp::UInt8>((inColor >> 16u) & 0xFFu))
        , a(static_cast<gp::UInt8>((inColor >> 24u) & 0xFFu))
    {}

public:
    /// @section Raw 32-Bit & Packed Integer Representations

    /// @brief Returns the raw 32-bit integer representation matching memory layout (`0xAARRGGBB` on little-endian).
    /// @return Packed 32-bit integer (`toPackedARGB()`).
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 dwColor() const noexcept
    {
        return toPackedARGB();
    }

    /// @brief Assigns all four channels from a raw 32-bit `DWColor` integer (`0xAARRGGBB` on little-endian).
    /// @param[in] inColor Packed 32-bit integer value.
    GP_FORCEINLINE_HINT constexpr void setDwColor(const gp::UInt32 inColor) noexcept
    {
        b = static_cast<gp::UInt8>(inColor & 0xFFu);
        g = static_cast<gp::UInt8>((inColor >> 8u) & 0xFFu);
        r = static_cast<gp::UInt8>((inColor >> 16u) & 0xFFu);
        a = static_cast<gp::UInt8>((inColor >> 24u) & 0xFFu);
    }

    /// @brief Packs the color into a 32-bit integer in `ARGB` bit order (`(a << 24) | (r << 16) | (g << 8) | b`).
    /// @return 32-bit packed `ARGB` value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 toPackedARGB() const noexcept
    {
        return (static_cast<gp::UInt32>(a) << 24u) | (static_cast<gp::UInt32>(r) << 16u) |
               (static_cast<gp::UInt32>(g) << 8u) | (static_cast<gp::UInt32>(b) << 0u);
    }

    /// @brief Packs the color into a 32-bit integer in `ABGR` bit order (`(a << 24) | (b << 16) | (g << 8) | r`).
    /// @return 32-bit packed `ABGR` value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 toPackedABGR() const noexcept
    {
        return (static_cast<gp::UInt32>(a) << 24u) | (static_cast<gp::UInt32>(b) << 16u) |
               (static_cast<gp::UInt32>(g) << 8u) | (static_cast<gp::UInt32>(r) << 0u);
    }

    /// @brief Packs the color into a 32-bit integer in `RGBA` bit order (`(r << 24) | (g << 16) | (b << 8) | a`).
    /// @return 32-bit packed `RGBA` value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 toPackedRGBA() const noexcept
    {
        return (static_cast<gp::UInt32>(r) << 24u) | (static_cast<gp::UInt32>(g) << 16u) |
               (static_cast<gp::UInt32>(b) << 8u) | (static_cast<gp::UInt32>(a) << 0u);
    }

    /// @brief Packs the color into a 32-bit integer in `BGRA` bit order (`(b << 24) | (g << 16) | (r << 8) | a`).
    /// @return 32-bit packed `BGRA` value.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 toPackedBGRA() const noexcept
    {
        return (static_cast<gp::UInt32>(b) << 24u) | (static_cast<gp::UInt32>(g) << 16u) |
               (static_cast<gp::UInt32>(r) << 8u) | (static_cast<gp::UInt32>(a) << 0u);
    }

public:
    /// @section Operators & Modifiers

    /// @brief Compares two colors for exact 32-bit channel equality.
    /// @param[in] other Color to compare against.
    /// @return True if all four `R, G, B, A` channels match exactly.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool operator==(const Color& other) const noexcept
    {
        return dwColor() == other.dwColor();
    }

    /// @brief Compares two colors for inequality.
    /// @param[in] other Color to compare against.
    /// @return True if any channel differs.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool operator!=(const Color& other) const noexcept
    {
        return dwColor() != other.dwColor();
    }

    /// @brief Component-wise saturating addition in `[0, 255]`.
    /// @param[in] other Color to add.
    /// @return Reference to `*this`.
    constexpr Color& operator+=(const Color& other) noexcept
    {
        r = static_cast<gp::UInt8>(gp::math::min(static_cast<gp::Int32>(r) + static_cast<gp::Int32>(other.r), 255));
        g = static_cast<gp::UInt8>(gp::math::min(static_cast<gp::Int32>(g) + static_cast<gp::Int32>(other.g), 255));
        b = static_cast<gp::UInt8>(gp::math::min(static_cast<gp::Int32>(b) + static_cast<gp::Int32>(other.b), 255));
        a = static_cast<gp::UInt8>(gp::math::min(static_cast<gp::Int32>(a) + static_cast<gp::Int32>(other.a), 255));
        return *this;
    }

    /// @brief Returns a copy of this color with its alpha channel replaced by @p newAlpha.
    /// @param[in] newAlpha Replacement alpha value in `[0, 255]`.
    /// @return New `Color(r, g, b, newAlpha)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr Color withAlpha(const gp::UInt8 newAlpha) const noexcept
    {
        return Color(r, g, b, newAlpha);
    }

public:
    /// @section GPU UNorm Quantization & Bit-Depth Requantization

    /// @brief Clamps a float to `[0.0f, 1.0f]`, mapping NaN and negative values to `0.0f` per GPU spec.
    /// @param[in] value Input floating-point value.
    /// @return Value clamped to `[0.0f, 1.0f]` (`0.0f` when `value` is `NaN`).
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::Float32 clamp01NansTo0(const gp::Float32 value) noexcept
    {
        const gp::Float32 clampedLo = (value > 0.0f) ? value : 0.0f;
        return (clampedLo < 1.0f) ? clampedLo : 1.0f;
    }

    /// @brief Quantizes a unit floating-point value `[0, 1]` to an 8-bit unsigned integer with `+0.5f` rounding.
    /// @details Matches the GPU `FLOAT -> UNORM8` specification and pairs with `dequantizeUNorm8ToFloat`.
    /// @param[in] unitFloat Input float (clamped to `[0, 1]`, NaNs mapped to `0`).
    /// @return Quantized 8-bit integer in `[0, 255]`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::UInt8
        quantizeUNormFloatTo8(const gp::Float32 unitFloat) noexcept
    {
        return static_cast<gp::UInt8>(0.5f + clamp01NansTo0(unitFloat) * 255.0f);
    }

    /// @brief Quantizes a unit floating-point value `[0, 1]` to a 16-bit unsigned integer with `+0.5f` rounding.
    /// @details Matches the GPU `FLOAT -> UNORM16` specification and pairs with `dequantizeUNorm16ToFloat`.
    /// @param[in] unitFloat Input float (clamped to `[0, 1]`, NaNs mapped to `0`).
    /// @return Quantized 16-bit integer in `[0, 65535]`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::UInt16
        quantizeUNormFloatTo16(const gp::Float32 unitFloat) noexcept
    {
        return static_cast<gp::UInt16>(0.5f + clamp01NansTo0(unitFloat) * 65535.0f);
    }

    /// @brief Dequantizes an 8-bit UNorm integer in `[0, 255]` to a floating-point value in `[0.0f, 1.0f]`.
    /// @param[in] value8 8-bit integer in `[0, 255]`.
    /// @return `value8 / 255.0f`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::Float32
        dequantizeUNorm8ToFloat(const gp::Int32 value8) noexcept
    {
        GP_ASSERT(value8 >= 0 && value8 <= 255, "Color::dequantizeUNorm8ToFloat value8 out of [0, 255] range.");
        return static_cast<gp::Float32>(value8) * (1.0f / 255.0f);
    }

    /// @brief Dequantizes a 16-bit UNorm integer in `[0, 65535]` to a floating-point value in `[0.0f, 1.0f]`.
    /// @param[in] value16 16-bit integer in `[0, 65535]`.
    /// @return `value16 / 65535.0f`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::Float32
        dequantizeUNorm16ToFloat(const gp::Int32 value16) noexcept
    {
        GP_ASSERT(value16 >= 0 && value16 <= 65535, "Color::dequantizeUNorm16ToFloat value16 out of [0, 65535] range.");
        return static_cast<gp::Float32>(value16) * (1.0f / 65535.0f);
    }

    /// @brief Requantizes a 10-bit UNorm integer `[0, 1023]` to an 8-bit UNorm integer `[0, 255]` with exact rounding.
    /// @details Exact integer arithmetic equivalent of `(int)((value10 / 1023.0f) * 255.0f + 0.5f)`.
    /// @param[in] value10 10-bit integer in `[0, 1023]`.
    /// @return Requantized 8-bit integer in `[0, 255]`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::UInt8 requantize10to8(const gp::Int32 value10) noexcept
    {
        GP_ASSERT(value10 >= 0 && value10 <= 1023, "Color::requantize10to8 value10 out of [0, 1023] range.");
        const gp::Int32 temp = value10 * 255 + (1 << 9);
        const gp::Int32 value8 = (temp + (temp >> 10)) >> 10;
        return static_cast<gp::UInt8>(value8);
    }

    /// @brief Requantizes a 16-bit UNorm integer `[0, 65535]` to an 8-bit UNorm integer `[0, 255]` with exact rounding.
    /// @details Exact integer arithmetic equivalent of `(int)((value16 / 65535.0f) * 255.0f + 0.5f)`.
    /// @param[in] value16 16-bit integer in `[0, 65535]`.
    /// @return Requantized 8-bit integer in `[0, 255]`.
    [[nodiscard]] GP_FORCEINLINE_HINT static constexpr gp::UInt8 requantize16to8(const gp::Int32 value16) noexcept
    {
        GP_ASSERT(value16 >= 0 && value16 <= 65535, "Color::requantize16to8 value16 out of [0, 65535] range.");
        const gp::Int32 value8 = (value16 * 255 + 32895) >> 16;
        return static_cast<gp::UInt8>(value8);
    }

    /// @brief Constructs an 8-bit `Color` by requantizing a 10-bit RGB, 2-bit Alpha (`R10G10B10A2_UNorm`) texel.
    /// @param[in] inR 10-bit red value in `[0, 1023]`.
    /// @param[in] inG 10-bit green value in `[0, 1023]`.
    /// @param[in] inB 10-bit blue value in `[0, 1023]`.
    /// @param[in] inA 2-bit alpha value in `[0, 3]`.
    /// @return Requantized 8-bit `Color`.
    [[nodiscard]] static constexpr Color makeRequantizeFrom1010102(
        const gp::Int32 inR, const gp::Int32 inG, const gp::Int32 inB, const gp::Int32 inA
    ) noexcept
    {
        GP_ASSERT(inA >= 0 && inA <= 3, "Color::makeRequantizeFrom1010102 alpha out of [0, 3] range.");
        constexpr gp::UInt8 kRequantize2to8[4] = { 0u, 0x55u, 0xAAu, 0xFFu };
        return Color(requantize10to8(inR), requantize10to8(inG), requantize10to8(inB), kRequantize2to8[inA & 3]);
    }

public:
    /// @section Conversions & Factories

    /// @brief Reinterprets the 8-bit channels directly as linear `[0, 1]` floats (`channel / 255.0f`) without sRGB
    ///        decoding.
    /// @details Exact inverse dequantizer for `LinearColor::quantizeRound()`.
    /// @return Linear color `(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f)`.
    [[nodiscard]] constexpr LinearColor reinterpretAsLinear() const noexcept;

    /// @brief Decodes a Radiance RGBE (`8:8:8:8` shared-exponent HDR) packet into a linear floating-point color.
    /// @details Follows Gregory Ward's *Real Pixels* (Graphics Gems II) with centered `+0.5f` bucket restoration.
    /// @return Decoded `LinearColor` with `a = 1.0f` (or `LinearColor::kBlack` when `a == 0`).
    [[nodiscard]] GP_CORE_API LinearColor fromRGBE() const noexcept;

    /// @brief Parses a hexadecimal color string into an 8-bit `Color`.
    /// @details Supports `RGB`, `RRGGBB`, `RRGGBBAA`, `#RGB`, `#RRGGBB`, and `#RRGGBBAA` (case-insensitive).
    ///          Invalid strings or unsupported lengths return `Color(0, 0, 0, 0)`.
    /// @param[in] hexString Input hexadecimal string view.
    /// @return Parsed `Color`.
    [[nodiscard]] GP_CORE_API static Color fromHex(gp::StringView hexString) noexcept;

    /// @brief Generates a visually distinct pseudorandom color by stepping around the HSV hue wheel.
    /// @return Random opaque sRGB `Color`.
    [[nodiscard]] GP_CORE_API static Color makeRandomColor() noexcept;

    /// @brief Generates a deterministic pseudorandom color from an explicit 32-bit integer seed.
    /// @param[in] seed Deterministic seed value.
    /// @return Seeded sRGB `Color`.
    [[nodiscard]] GP_CORE_API static Color makeRandomSeededColor(gp::Int32 seed) noexcept;

    /// @brief Maps a scalar in `[0.0f, 1.0f]` to a Red-Yellow-Green traffic-light ramp (`0` = Red, `0.5` = Yellow,
    ///        `1` = Green).
    /// @param[in] scalar Normalized value in `[0.0f, 1.0f]` (clamped if outside `[0, 1]`).
    /// @return Interpolated `Color` with `b = 0` and `a = 255`.
    [[nodiscard]] GP_CORE_API static Color makeRedToGreenColorFromScalar(gp::Float32 scalar) noexcept;

    /// @brief Converts a black-body radiator temperature in Kelvins (`[1000K, 15000K]`) to an sRGB `Color`.
    /// @param[in] temp Temperature in Kelvins.
    /// @return Chromaticity color encoded in 8-bit sRGB.
    [[nodiscard]] GP_CORE_API static Color makeFromColorTemperature(gp::Float32 temp) noexcept;

    /// @brief Formats this color as an 8-character uppercase hexadecimal string `"RRGGBBAA"`.
    /// @return Hexadecimal `gp::String`.
    [[nodiscard]] GP_CORE_API gp::String toHex() const noexcept;

    /// @brief Formats this color as a parenthesized key-value string `"(R=r,G=g,B=b,A=a)"`.
    /// @return Formatted `gp::String`.
    [[nodiscard]] GP_CORE_API gp::String toString() const noexcept;

    /// @brief Initializes this color from a string containing `"R=..."`, `"G=..."`, `"B=..."`, and optional `"A=..."`.
    /// @details Defaults `a` to `255` when `"A="` is omitted. Returns `false` if any of `R=`, `G=`, or `B=` is missing
    ///          or unparseable.
    /// @param[in] sourceString Input string view to parse.
    /// @return True if `R`, `G`, and `B` were successfully parsed.
    GP_CORE_API bool initFromString(gp::StringView sourceString) noexcept;

public:
    /// @section Hashing Support

    /// @brief Returns the 32-bit integer representation as a fast hash code.
    /// @return `dwColor()`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 getHash() const noexcept
    {
        return dwColor();
    }

    /// @brief Streams the packed 32-bit color word into any `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(dwColor());
    }

public:
    /// @section Named Color Constants

    static const Color kWhite;
    static const Color kBlack;
    static const Color kTransparent;
    static const Color kRed;
    static const Color kGreen;
    static const Color kBlue;
    static const Color kYellow;
    static const Color kCyan;
    static const Color kMagenta;
    static const Color kOrange;
    static const Color kPurple;
    static const Color kTurquoise;
    static const Color kSilver;
    static const Color kEmerald;
};

static_assert(sizeof(Color) == 4u, "gp::math::Color must be exactly 4 bytes.");
static_assert(alignof(Color) == 1u, "gp::math::Color must have 1-byte alignment.");
static_assert(std::is_trivially_copyable_v<Color>, "gp::math::Color must be trivially copyable.");
static_assert(std::is_standard_layout_v<Color>, "gp::math::Color must have standard layout.");
static_assert(
    std::has_unique_object_representations_v<Color>,
    "gp::math::Color must have unique object representations (zero padding bits)."
);

inline constexpr Color Color::kWhite(255u, 255u, 255u, 255u);
inline constexpr Color Color::kBlack(0u, 0u, 0u, 255u);
inline constexpr Color Color::kTransparent(0u, 0u, 0u, 0u);
inline constexpr Color Color::kRed(255u, 0u, 0u, 255u);
inline constexpr Color Color::kGreen(0u, 255u, 0u, 255u);
inline constexpr Color Color::kBlue(0u, 0u, 255u, 255u);
inline constexpr Color Color::kYellow(255u, 255u, 0u, 255u);
inline constexpr Color Color::kCyan(0u, 255u, 255u, 255u);
inline constexpr Color Color::kMagenta(255u, 0u, 255u, 255u);
inline constexpr Color Color::kOrange(243u, 156u, 18u, 255u);
inline constexpr Color Color::kPurple(169u, 7u, 228u, 255u);
inline constexpr Color Color::kTurquoise(26u, 188u, 156u, 255u);
inline constexpr Color Color::kSilver(189u, 195u, 199u, 255u);
inline constexpr Color Color::kEmerald(46u, 204u, 113u, 255u);

}   // namespace gp::math
