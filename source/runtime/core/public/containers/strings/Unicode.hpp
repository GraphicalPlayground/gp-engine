// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "platforms/base/Platform.hpp"
#include <iterator>

namespace gp::unicode
{

/// @brief Unicode replacement character (U+FFFD) substituted for ill-formed code unit sequences.
inline constexpr char32_t kReplacementChar = 0x0000FFFDu;

/// @brief Maximum valid Unicode scalar value (U+10FFFF).
inline constexpr char32_t kMaxCodepoint = 0x0010FFFFu;

/// @brief Inclusive start of the UTF-16 high-surrogate range (U+D800).
inline constexpr char32_t kSurrogateHighStart = 0x0000D800u;

/// @brief Inclusive end of the UTF-16 high-surrogate range (U+DBFF).
inline constexpr char32_t kSurrogateHighEnd = 0x0000DBFFu;

/// @brief Inclusive start of the UTF-16 low-surrogate range (U+DC00).
inline constexpr char32_t kSurrogateLowStart = 0x0000DC00u;

/// @brief Inclusive end of the UTF-16 low-surrogate range (U+DFFF).
inline constexpr char32_t kSurrogateLowEnd = 0x0000DFFFu;

/// @brief Checks whether a 32-bit value falls within the UTF-16 high-surrogate range [U+D800, U+DBFF].
/// @param[in] codepoint The value to inspect.
/// @return True if the value is a high surrogate, false otherwise.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isHighSurrogate(const char32_t codepoint) noexcept
{
    return (codepoint >= kSurrogateHighStart) && (codepoint <= kSurrogateHighEnd);
}

/// @brief Checks whether a 32-bit value falls within the UTF-16 low-surrogate range [U+DC00, U+DFFF].
/// @param[in] codepoint The value to inspect.
/// @return True if the value is a low surrogate, false otherwise.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isLowSurrogate(const char32_t codepoint) noexcept
{
    return (codepoint >= kSurrogateLowStart) && (codepoint <= kSurrogateLowEnd);
}

/// @brief Checks whether a 32-bit value falls within the UTF-16 surrogate range [U+D800, U+DFFF].
/// @param[in] codepoint The value to inspect.
/// @return True if the value is any UTF-16 surrogate, false otherwise.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isSurrogate(const char32_t codepoint) noexcept
{
    return (codepoint >= kSurrogateHighStart) && (codepoint <= kSurrogateLowEnd);
}

/// @brief Checks whether a 32-bit value is a valid Unicode scalar value.
/// @details Valid Unicode scalar values are in the range [U+0000, U+D7FF] or [U+E000, U+10FFFF].
/// @param[in] codepoint The value to validate.
/// @return True if the codepoint is a valid Unicode scalar value, false otherwise.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isValidCodepoint(const char32_t codepoint) noexcept
{
    return (codepoint <= kMaxCodepoint) && !isSurrogate(codepoint);
}

/// @brief Checks whether a UTF-8 byte is a trailing continuation byte (bit pattern 10xxxxxx).
/// @param[in] byte The unsigned byte to inspect.
/// @return True if the byte is in [0x80, 0xBF], false otherwise.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr bool isUtf8ContinuationByte(const gp::UInt8 byte) noexcept
{
    return (byte & 0xC0u) == 0x80u;
}

/// @brief Computes the number of UTF-8 code units required to encode a Unicode codepoint.
/// @note Ill-formed codepoints are replaced by U+FFFD, which requires 3 UTF-8 bytes.
/// @param[in] codepoint The Unicode codepoint to measure.
/// @return Number of UTF-8 bytes required (1 to 4).
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize utf8EncodedLength(const char32_t codepoint) noexcept
{
    if (!isValidCodepoint(codepoint))
    {
        return 3u;
    }
    if (codepoint <= 0x0000007Fu)
    {
        return 1u;
    }
    if (codepoint <= 0x000007FFu)
    {
        return 2u;
    }
    if (codepoint <= 0x0000FFFFu)
    {
        return 3u;
    }
    return 4u;
}

/// @brief Computes the number of UTF-16 code units required to encode a Unicode codepoint.
/// @note Ill-formed codepoints are replaced by U+FFFD, which requires 1 UTF-16 code unit.
/// @param[in] codepoint The Unicode codepoint to measure.
/// @return Number of UTF-16 code units required (1 or 2).
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize utf16EncodedLength(const char32_t codepoint) noexcept
{
    if (!isValidCodepoint(codepoint))
    {
        return 1u;
    }
    return (codepoint <= 0x0000FFFFu) ? 1u : 2u;
}

/// @brief Computes the number of UTF-32 code units required to encode a Unicode codepoint.
/// @param[in] codepoint The Unicode codepoint to measure.
/// @return Always 1.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize
    utf32EncodedLength([[maybe_unused]] const char32_t codepoint) noexcept
{
    return 1u;
}

/// @brief Computes the number of `InCharT` code units required to encode a Unicode codepoint.
/// @tparam InCharT Target character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
/// @param[in] codepoint The Unicode codepoint to measure.
/// @return Number of `InCharT` code units required.
template <concepts::IsCharacter InCharT>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize encodedLength(const char32_t codepoint) noexcept
{
    if constexpr (sizeof(InCharT) == 1u)
    {
        return utf8EncodedLength(codepoint);
    }
    else if constexpr (sizeof(InCharT) == 2u)
    {
        return utf16EncodedLength(codepoint);
    }
    else
    {
        return utf32EncodedLength(codepoint);
    }
}

/// @brief Decodes a single Unicode codepoint from a UTF-8 buffer, advancing `outNext`.
/// @details Implements Unicode Standard maximal-subpart replacement: any ill-formed, overlong, surrogate, or
///          truncated sequence yields `kReplacementChar` (U+FFFD) and advances `outNext` past the maximal valid
///          prefix so decoding is guaranteed to make forward progress without throwing or asserting.
/// @tparam InCharT 1-byte character type (`char` or `char8_t`).
/// @param[in] ptr Pointer to the start of the UTF-8 sequence (must be `< end`).
/// @param[in] end Pointer one past the last available code unit.
/// @param[out] outNext Receives the pointer to the next unread code unit.
/// @return Decoded Unicode scalar value, or `kReplacementChar` on ill-formed input.
template <concepts::IsCharacter InCharT>
requires(sizeof(InCharT) == 1u) [[nodiscard]] constexpr char32_t
    decodeUtf8(const InCharT* const ptr, const InCharT* const end, const InCharT*& outNext) noexcept
{
    if (ptr == nullptr || end == nullptr || ptr >= end) [[unlikely]]
    {
        outNext = ptr;
        return kReplacementChar;
    }

    const gp::UInt8 b0 = static_cast<gp::UInt8>(ptr[0]);

    // 1-byte ASCII sequence: [0x00, 0x7F]
    if (b0 < 0x80u) [[likely]]
    {
        outNext = ptr + 1;
        return static_cast<char32_t>(b0);
    }

    // 2-byte sequence: leading byte in [0xC2, 0xDF] (0xC0 and 0xC1 are always overlong)
    if (b0 >= 0xC2u && b0 <= 0xDFu)
    {
        if ((end - ptr) < 2)
        {
            outNext = ptr + 1;
            return kReplacementChar;
        }

        const gp::UInt8 b1 = static_cast<gp::UInt8>(ptr[1]);
        if (!isUtf8ContinuationByte(b1))
        {
            outNext = ptr + 1;
            return kReplacementChar;
        }

        outNext = ptr + 2;
        return (static_cast<char32_t>(b0 & 0x1Fu) << 6u) | static_cast<char32_t>(b1 & 0x3Fu);
    }

    // 3-byte sequence: leading byte in [0xE0, 0xEF]
    if (b0 >= 0xE0u && b0 <= 0xEFu)
    {
        if ((end - ptr) < 2)
        {
            outNext = ptr + 1;
            return kReplacementChar;
        }

        const gp::UInt8 b1 = static_cast<gp::UInt8>(ptr[1]);
        // Validate second byte:
        // - If b0 == 0xE0, b1 must be in [0xA0, 0xBF] to prevent overlong encodings (< U+0800).
        // - If b0 == 0xED, b1 must be in [0x80, 0x9F] to reject UTF-16 surrogates [U+D800, U+DFFF].
        // - Otherwise, b1 must be in [0x80, 0xBF].
        const gp::UInt8 b1Min = (b0 == 0xE0u) ? 0xA0u : 0x80u;
        const gp::UInt8 b1Max = (b0 == 0xEDu) ? 0x9Fu : 0xBFu;
        if (b1 < b1Min || b1 > b1Max)
        {
            outNext = ptr + 1;
            return kReplacementChar;
        }

        if ((end - ptr) < 3)
        {
            outNext = ptr + 2;
            return kReplacementChar;
        }

        const gp::UInt8 b2 = static_cast<gp::UInt8>(ptr[2]);
        if (!isUtf8ContinuationByte(b2))
        {
            outNext = ptr + 2;
            return kReplacementChar;
        }

        outNext = ptr + 3;
        return (static_cast<char32_t>(b0 & 0x0Fu) << 12u) | (static_cast<char32_t>(b1 & 0x3Fu) << 6u) |
               static_cast<char32_t>(b2 & 0x3Fu);
    }

    // 4-byte sequence: leading byte in [0xF0, 0xF4]
    if (b0 >= 0xF0u && b0 <= 0xF4u)
    {
        if ((end - ptr) < 2)
        {
            outNext = ptr + 1;
            return kReplacementChar;
        }

        const gp::UInt8 b1 = static_cast<gp::UInt8>(ptr[1]);
        // Validate second byte:
        // - If b0 == 0xF0, b1 must be in [0x90, 0xBF] to prevent overlong encodings (< U+10000).
        // - If b0 == 0xF4, b1 must be in [0x80, 0x8F] to prevent codepoints > U+10FFFF.
        // - Otherwise, b1 must be in [0x80, 0xBF].
        const gp::UInt8 b1Min = (b0 == 0xF0u) ? 0x90u : 0x80u;
        const gp::UInt8 b1Max = (b0 == 0xF4u) ? 0x8Fu : 0xBFu;
        if (b1 < b1Min || b1 > b1Max)
        {
            outNext = ptr + 1;
            return kReplacementChar;
        }

        if ((end - ptr) < 3)
        {
            outNext = ptr + 2;
            return kReplacementChar;
        }

        const gp::UInt8 b2 = static_cast<gp::UInt8>(ptr[2]);
        if (!isUtf8ContinuationByte(b2))
        {
            outNext = ptr + 2;
            return kReplacementChar;
        }

        if ((end - ptr) < 4)
        {
            outNext = ptr + 3;
            return kReplacementChar;
        }

        const gp::UInt8 b3 = static_cast<gp::UInt8>(ptr[3]);
        if (!isUtf8ContinuationByte(b3))
        {
            outNext = ptr + 3;
            return kReplacementChar;
        }

        outNext = ptr + 4;
        return (static_cast<char32_t>(b0 & 0x07u) << 18u) | (static_cast<char32_t>(b1 & 0x3Fu) << 12u) |
               (static_cast<char32_t>(b2 & 0x3Fu) << 6u) | static_cast<char32_t>(b3 & 0x3Fu);
    }

    // Stray continuation byte [0x80, 0xBF], overlong starter [0xC0, 0xC1], or invalid starter [0xF5, 0xFF]
    outNext = ptr + 1;
    return kReplacementChar;
}

/// @brief Decodes a single Unicode codepoint from a UTF-16 buffer, advancing `outNext`.
/// @tparam InCharT 2-byte character type (`char16_t` or 2-byte `wchar_t`).
/// @param[in] ptr Pointer to the start of the UTF-16 sequence (must be `< end`).
/// @param[in] end Pointer one past the last available code unit.
/// @param[out] outNext Receives the pointer to the next unread code unit.
/// @return Decoded Unicode scalar value, or `kReplacementChar` on unpaired surrogates.
template <concepts::IsCharacter InCharT>
requires(sizeof(InCharT) == 2u) [[nodiscard]] constexpr char32_t
    decodeUtf16(const InCharT* const ptr, const InCharT* const end, const InCharT*& outNext) noexcept
{
    if (ptr == nullptr || end == nullptr || ptr >= end) [[unlikely]]
    {
        outNext = ptr;
        return kReplacementChar;
    }

    const char32_t u0 = static_cast<char32_t>(static_cast<gp::UInt16>(ptr[0]));
    if (!isSurrogate(u0)) [[likely]]
    {
        outNext = ptr + 1;
        return u0;
    }

    if (isHighSurrogate(u0) && ((end - ptr) >= 2))
    {
        const char32_t u1 = static_cast<char32_t>(static_cast<gp::UInt16>(ptr[1]));
        if (isLowSurrogate(u1))
        {
            outNext = ptr + 2;
            return 0x00010000u + ((u0 - kSurrogateHighStart) << 10u) + (u1 - kSurrogateLowStart);
        }
    }

    // Unpaired high or low surrogate
    outNext = ptr + 1;
    return kReplacementChar;
}

/// @brief Decodes a single Unicode codepoint from a UTF-32 buffer, advancing `outNext`.
/// @tparam InCharT 4-byte character type (`char32_t` or 4-byte `wchar_t`).
/// @param[in] ptr Pointer to the start of the UTF-32 sequence (must be `< end`).
/// @param[in] end Pointer one past the last available code unit.
/// @param[out] outNext Receives the pointer to the next unread code unit.
/// @return Decoded Unicode scalar value, or `kReplacementChar` if the value is not a valid Unicode scalar.
template <concepts::IsCharacter InCharT>
requires(sizeof(InCharT) == 4u) [[nodiscard]] constexpr char32_t
    decodeUtf32(const InCharT* const ptr, const InCharT* const end, const InCharT*& outNext) noexcept
{
    if (ptr == nullptr || end == nullptr || ptr >= end) [[unlikely]]
    {
        outNext = ptr;
        return kReplacementChar;
    }

    outNext = ptr + 1;
    const char32_t cp = static_cast<char32_t>(static_cast<gp::UInt32>(ptr[0]));
    return isValidCodepoint(cp) ? cp : kReplacementChar;
}

/// @brief Decodes a single Unicode codepoint from a buffer of any character width, advancing `outNext`.
/// @tparam InCharT Character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
/// @param[in] ptr Pointer to the start of the sequence (must be `< end`).
/// @param[in] end Pointer one past the last available code unit.
/// @param[out] outNext Receives the pointer to the next unread code unit.
/// @return Decoded Unicode scalar value, or `kReplacementChar` on ill-formed input.
template <concepts::IsCharacter InCharT>
[[nodiscard]] GP_FORCEINLINE_HINT constexpr char32_t
    decodeNext(const InCharT* const ptr, const InCharT* const end, const InCharT*& outNext) noexcept
{
    if constexpr (sizeof(InCharT) == 1u)
    {
        return decodeUtf8(ptr, end, outNext);
    }
    else if constexpr (sizeof(InCharT) == 2u)
    {
        return decodeUtf16(ptr, end, outNext);
    }
    else
    {
        return decodeUtf32(ptr, end, outNext);
    }
}

/// @brief Encodes a Unicode codepoint into 1 to 4 UTF-8 code units.
/// @note If `codepoint` is not a valid Unicode scalar value, encodes `kReplacementChar` (U+FFFD).
/// @tparam InCharT 1-byte character type (`char` or `char8_t`).
/// @param[in] codepoint The Unicode scalar value to encode.
/// @param[out] outBuf Destination buffer with space for at least 4 code units.
/// @return Number of UTF-8 code units written (1 to 4).
template <concepts::IsCharacter InCharT>
requires(sizeof(InCharT) == 1u) constexpr gp::USize encodeUtf8(const char32_t codepoint, InCharT* const outBuf) noexcept
{
    const char32_t cp = isValidCodepoint(codepoint) ? codepoint : kReplacementChar;

    if (cp <= 0x0000007Fu)
    {
        outBuf[0] = static_cast<InCharT>(static_cast<gp::UInt8>(cp));
        return 1u;
    }

    if (cp <= 0x000007FFu)
    {
        outBuf[0] = static_cast<InCharT>(static_cast<gp::UInt8>(0xC0u | ((cp >> 6u) & 0x1Fu)));
        outBuf[1] = static_cast<InCharT>(static_cast<gp::UInt8>(0x80u | (cp & 0x3Fu)));
        return 2u;
    }

    if (cp <= 0x0000FFFFu)
    {
        outBuf[0] = static_cast<InCharT>(static_cast<gp::UInt8>(0xE0u | ((cp >> 12u) & 0x0Fu)));
        outBuf[1] = static_cast<InCharT>(static_cast<gp::UInt8>(0x80u | ((cp >> 6u) & 0x3Fu)));
        outBuf[2] = static_cast<InCharT>(static_cast<gp::UInt8>(0x80u | (cp & 0x3Fu)));
        return 3u;
    }

    outBuf[0] = static_cast<InCharT>(static_cast<gp::UInt8>(0xF0u | ((cp >> 18u) & 0x07u)));
    outBuf[1] = static_cast<InCharT>(static_cast<gp::UInt8>(0x80u | ((cp >> 12u) & 0x3Fu)));
    outBuf[2] = static_cast<InCharT>(static_cast<gp::UInt8>(0x80u | ((cp >> 6u) & 0x3Fu)));
    outBuf[3] = static_cast<InCharT>(static_cast<gp::UInt8>(0x80u | (cp & 0x3Fu)));
    return 4u;
}

/// @brief Encodes a Unicode codepoint into 1 or 2 UTF-16 code units.
/// @note If `codepoint` is not a valid Unicode scalar value, encodes `kReplacementChar` (U+FFFD).
/// @tparam InCharT 2-byte character type (`char16_t` or 2-byte `wchar_t`).
/// @param[in] codepoint The Unicode scalar value to encode.
/// @param[out] outBuf Destination buffer with space for at least 2 code units.
/// @return Number of UTF-16 code units written (1 or 2).
template <concepts::IsCharacter InCharT>
requires(sizeof(InCharT) == 2u)
constexpr gp::USize encodeUtf16(const char32_t codepoint, InCharT* const outBuf) noexcept
{
    const char32_t cp = isValidCodepoint(codepoint) ? codepoint : kReplacementChar;

    if (cp <= 0x0000FFFFu)
    {
        outBuf[0] = static_cast<InCharT>(static_cast<gp::UInt16>(cp));
        return 1u;
    }

    const char32_t offset = cp - 0x00010000u;
    outBuf[0] = static_cast<InCharT>(static_cast<gp::UInt16>(kSurrogateHighStart + (offset >> 10u)));
    outBuf[1] = static_cast<InCharT>(static_cast<gp::UInt16>(kSurrogateLowStart + (offset & 0x3FFu)));
    return 2u;
}

/// @brief Encodes a Unicode codepoint into 1 UTF-32 code unit.
/// @note If `codepoint` is not a valid Unicode scalar value, encodes `kReplacementChar` (U+FFFD).
/// @tparam InCharT 4-byte character type (`char32_t` or 4-byte `wchar_t`).
/// @param[in] codepoint The Unicode scalar value to encode.
/// @param[out] outBuf Destination buffer with space for at least 1 code unit.
/// @return Always 1.
template <concepts::IsCharacter InCharT>
requires(sizeof(InCharT) == 4u)
constexpr gp::USize encodeUtf32(const char32_t codepoint, InCharT* const outBuf) noexcept
{
    const char32_t cp = isValidCodepoint(codepoint) ? codepoint : kReplacementChar;
    outBuf[0] = static_cast<InCharT>(static_cast<gp::UInt32>(cp));
    return 1u;
}

/// @brief Encodes a Unicode codepoint into `InCharT` code units.
/// @tparam InCharT Destination character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
/// @param[in] codepoint The Unicode scalar value to encode.
/// @param[out] outBuf Destination buffer with space for at least 4 (UTF-8), 2 (UTF-16), or 1 (UTF-32) code units.
/// @return Number of `InCharT` code units written.
template <concepts::IsCharacter InCharT>
constexpr gp::USize encodeCodepoint(const char32_t codepoint, InCharT* const outBuf) noexcept
{
    if constexpr (sizeof(InCharT) == 1u)
    {
        return encodeUtf8(codepoint, outBuf);
    }
    else if constexpr (sizeof(InCharT) == 2u)
    {
        return encodeUtf16(codepoint, outBuf);
    }
    else
    {
        return encodeUtf32(codepoint, outBuf);
    }
}

/// @brief Validates whether a 1-byte string view contains strictly well-formed UTF-8.
/// @tparam InCharT 1-byte character type (`char` or `char8_t`).
/// @param[in] view The UTF-8 sequence to validate.
/// @return True if every byte sequence in `view` is valid UTF-8, false otherwise.
template <concepts::IsCharacter InCharT>
requires(sizeof(InCharT) == 1u)
[[nodiscard]] constexpr bool isValidUtf8(const container::BasicStringView<InCharT> view) noexcept
{
    const InCharT* ptr = view.data();
    const InCharT* const end = ptr + view.size();

    while (ptr < end)
    {
        const gp::UInt8 b0 = static_cast<gp::UInt8>(ptr[0]);
        if (b0 < 0x80u) [[likely]]
        {
            ++ptr;
            continue;
        }

        if (b0 >= 0xC2u && b0 <= 0xDFu)
        {
            if ((end - ptr) < 2 || !isUtf8ContinuationByte(static_cast<gp::UInt8>(ptr[1])))
            {
                return false;
            }
            ptr += 2;
            continue;
        }

        if (b0 >= 0xE0u && b0 <= 0xEFu)
        {
            if ((end - ptr) < 3)
            {
                return false;
            }
            const gp::UInt8 b1 = static_cast<gp::UInt8>(ptr[1]);
            const gp::UInt8 b1Min = (b0 == 0xE0u) ? 0xA0u : 0x80u;
            const gp::UInt8 b1Max = (b0 == 0xEDu) ? 0x9Fu : 0xBFu;
            if (b1 < b1Min || b1 > b1Max || !isUtf8ContinuationByte(static_cast<gp::UInt8>(ptr[2])))
            {
                return false;
            }
            ptr += 3;
            continue;
        }

        if (b0 >= 0xF0u && b0 <= 0xF4u)
        {
            if ((end - ptr) < 4)
            {
                return false;
            }
            const gp::UInt8 b1 = static_cast<gp::UInt8>(ptr[1]);
            const gp::UInt8 b1Min = (b0 == 0xF0u) ? 0x90u : 0x80u;
            const gp::UInt8 b1Max = (b0 == 0xF4u) ? 0x8Fu : 0xBFu;
            if (b1 < b1Min || b1 > b1Max || !isUtf8ContinuationByte(static_cast<gp::UInt8>(ptr[2])) ||
                !isUtf8ContinuationByte(static_cast<gp::UInt8>(ptr[3])))
            {
                return false;
            }
            ptr += 4;
            continue;
        }

        return false;
    }

    return true;
}

/// @brief Validates whether a 2-byte string view contains strictly well-formed UTF-16.
/// @tparam InCharT 2-byte character type (`char16_t` or 2-byte `wchar_t`).
/// @param[in] view The UTF-16 sequence to validate.
/// @return True if every code unit sequence in `view` is valid UTF-16, false otherwise.
template <concepts::IsCharacter InCharT>
requires(sizeof(InCharT) == 2u)
[[nodiscard]] constexpr bool isValidUtf16(const container::BasicStringView<InCharT> view) noexcept
{
    const InCharT* ptr = view.data();
    const InCharT* const end = ptr + view.size();

    while (ptr < end)
    {
        const char32_t u0 = static_cast<char32_t>(static_cast<gp::UInt16>(ptr[0]));
        if (!isSurrogate(u0)) [[likely]]
        {
            ++ptr;
            continue;
        }

        if (isHighSurrogate(u0) && ((end - ptr) >= 2))
        {
            const char32_t u1 = static_cast<char32_t>(static_cast<gp::UInt16>(ptr[1]));
            if (isLowSurrogate(u1))
            {
                ptr += 2;
                continue;
            }
        }

        return false;
    }

    return true;
}

/// @brief Validates whether a 4-byte string view contains strictly well-formed UTF-32.
/// @tparam InCharT 4-byte character type (`char32_t` or 4-byte `wchar_t`).
/// @param[in] view The UTF-32 sequence to validate.
/// @return True if every code unit in `view` is a valid Unicode scalar value, false otherwise.
template <concepts::IsCharacter InCharT>
requires(sizeof(InCharT) == 4u)
[[nodiscard]] constexpr bool isValidUtf32(const container::BasicStringView<InCharT> view) noexcept
{
    for (gp::USize i = 0u; i < view.size(); ++i)
    {
        const char32_t cp = static_cast<char32_t>(static_cast<gp::UInt32>(view[i]));
        if (!isValidCodepoint(cp))
        {
            return false;
        }
    }
    return true;
}

/// @brief Validates whether a string view contains strictly well-formed Unicode for its character width.
/// @tparam InCharT Character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
/// @param[in] view The sequence to validate.
/// @return True if `view` contains well-formed UTF-8/16/32, false otherwise.
template <concepts::IsCharacter InCharT>
[[nodiscard]] constexpr bool isValidUtf(const container::BasicStringView<InCharT> view) noexcept
{
    if constexpr (sizeof(InCharT) == 1u)
    {
        return isValidUtf8(view);
    }
    else if constexpr (sizeof(InCharT) == 2u)
    {
        return isValidUtf16(view);
    }
    else
    {
        return isValidUtf32(view);
    }
}

/// @brief Counts the number of decoded Unicode codepoints in a string view.
/// @note Ill-formed sequences are decoded as `kReplacementChar` using maximal-subpart rules and counted accordingly.
/// @tparam InCharT Character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
/// @param[in] view The string view to inspect.
/// @return Total number of decoded Unicode codepoints.
template <concepts::IsCharacter InCharT>
[[nodiscard]] constexpr gp::USize countCodepoints(const container::BasicStringView<InCharT> view) noexcept
{
    if constexpr (sizeof(InCharT) == 4u)
    {
        return view.size();
    }
    else
    {
        const InCharT* ptr = view.data();
        const InCharT* const end = ptr + view.size();
        gp::USize count = 0u;
        while (ptr < end)
        {
            const InCharT* next = nullptr;
            (void)decodeNext(ptr, end, next);
            ptr = next;
            ++count;
        }
        return count;
    }
}

/// @brief Computes the exact number of `InDstCharT` code units required to transcode `src`.
/// @tparam InDstCharT Target character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
/// @tparam InSrcCharT Source character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
/// @param[in] src The source string view to measure.
/// @return Number of `InDstCharT` code units (excluding null terminator) produced when transcoding `src`.
template <concepts::IsCharacter InDstCharT, concepts::IsCharacter InSrcCharT>
[[nodiscard]] constexpr gp::USize transcodeLength(const container::BasicStringView<InSrcCharT> src) noexcept
{
    const InSrcCharT* ptr = src.data();
    const InSrcCharT* const end = ptr + src.size();
    gp::USize totalUnits = 0u;

    while (ptr < end)
    {
        const InSrcCharT* next = nullptr;
        const char32_t cp = decodeNext(ptr, end, next);
        ptr = next;
        totalUnits += encodedLength<InDstCharT>(cp);
    }

    return totalUnits;
}

/// @brief Computes the number of UTF-16 code units required to transcode a UTF-8 view.
/// @tparam InSrcCharT 1-byte source character type (`char` or `char8_t`).
/// @param[in] src Source UTF-8 string view.
/// @return Number of UTF-16 code units required.
template <concepts::IsCharacter InSrcCharT>
requires(sizeof(InSrcCharT) == 1u) [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize
    utf8ToUtf16Length(const container::BasicStringView<InSrcCharT> src) noexcept
{
    return transcodeLength<char16_t>(src);
}

/// @brief Computes the number of UTF-32 code units required to transcode a UTF-8 view.
/// @tparam InSrcCharT 1-byte source character type (`char` or `char8_t`).
/// @param[in] src Source UTF-8 string view.
/// @return Number of UTF-32 code units required.
template <concepts::IsCharacter InSrcCharT>
requires(sizeof(InSrcCharT) == 1u) [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize
    utf8ToUtf32Length(const container::BasicStringView<InSrcCharT> src) noexcept
{
    return transcodeLength<char32_t>(src);
}

/// @brief Computes the number of UTF-8 code units required to transcode a UTF-16 view.
/// @tparam InSrcCharT 2-byte source character type (`char16_t` or 2-byte `wchar_t`).
/// @param[in] src Source UTF-16 string view.
/// @return Number of UTF-8 code units required.
template <concepts::IsCharacter InSrcCharT>
requires(sizeof(InSrcCharT) == 2u) [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize
    utf16ToUtf8Length(const container::BasicStringView<InSrcCharT> src) noexcept
{
    return transcodeLength<char>(src);
}

/// @brief Computes the number of UTF-32 code units required to transcode a UTF-16 view.
/// @tparam InSrcCharT 2-byte source character type (`char16_t` or 2-byte `wchar_t`).
/// @param[in] src Source UTF-16 string view.
/// @return Number of UTF-32 code units required.
template <concepts::IsCharacter InSrcCharT>
requires(sizeof(InSrcCharT) == 2u) [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize
    utf16ToUtf32Length(const container::BasicStringView<InSrcCharT> src) noexcept
{
    return transcodeLength<char32_t>(src);
}

/// @brief Computes the number of UTF-8 code units required to transcode a UTF-32 view.
/// @tparam InSrcCharT 4-byte source character type (`char32_t` or 4-byte `wchar_t`).
/// @param[in] src Source UTF-32 string view.
/// @return Number of UTF-8 code units required.
template <concepts::IsCharacter InSrcCharT>
requires(sizeof(InSrcCharT) == 4u) [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize
    utf32ToUtf8Length(const container::BasicStringView<InSrcCharT> src) noexcept
{
    return transcodeLength<char>(src);
}

/// @brief Computes the number of UTF-16 code units required to transcode a UTF-32 view.
/// @tparam InSrcCharT 4-byte source character type (`char32_t` or 4-byte `wchar_t`).
/// @param[in] src Source UTF-32 string view.
/// @return Number of UTF-16 code units required.
template <concepts::IsCharacter InSrcCharT>
requires(sizeof(InSrcCharT) == 4u) [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::USize
    utf32ToUtf16Length(const container::BasicStringView<InSrcCharT> src) noexcept
{
    return transcodeLength<char16_t>(src);
}

/// @brief Transcodes a Unicode view into a caller-supplied destination buffer without splitting multi-unit sequences.
/// @tparam InDstCharT Destination character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
/// @tparam InSrcCharT Source character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
/// @param[in] src Source string view to transcode.
/// @param[out] dstBuf Destination buffer to write encoded code units into.
/// @param[in] dstCapacity Maximum number of `InDstCharT` code units that can be written (excluding any null
/// terminator).
/// @return Total number of `InDstCharT` code units written to `dstBuf`.
template <concepts::IsCharacter InDstCharT, concepts::IsCharacter InSrcCharT>
constexpr gp::USize transcodeToBuffer(
    const container::BasicStringView<InSrcCharT> src, InDstCharT* const dstBuf, const gp::USize dstCapacity
) noexcept
{
    if (dstBuf == nullptr || dstCapacity == 0u || src.isEmpty())
    {
        return 0u;
    }

    const InSrcCharT* ptr = src.data();
    const InSrcCharT* const end = ptr + src.size();
    gp::USize written = 0u;

    while (ptr < end)
    {
        const InSrcCharT* next = nullptr;
        const char32_t cp = decodeNext(ptr, end, next);
        const gp::USize needed = encodedLength<InDstCharT>(cp);
        if (written + needed > dstCapacity)
        {
            break;
        }

        written += encodeCodepoint<InDstCharT>(cp, dstBuf + written);
        ptr = next;
    }

    return written;
}

/// @brief Transcodes a Unicode string view into an owning engine string container (`BasicString` or
/// `BasicFixedString`).
/// @tparam InSrcCharT Source character type.
/// @tparam InDstStringT Destination string container type exposing `ValueType`, `clear()`, and `append()` or
/// `pushBack()`.
/// @param[in] src Source string view to transcode.
/// @param[out] outDst Destination string container that receives the transcoded characters.
template <concepts::IsCharacter InSrcCharT, typename InDstStringT>
constexpr void convert(const container::BasicStringView<InSrcCharT> src, InDstStringT& outDst) noexcept
{
    using DstCharT = typename InDstStringT::ValueType;
    outDst.clear();
    if (src.isEmpty())
    {
        return;
    }

    const gp::USize requiredLen = transcodeLength<DstCharT>(src);
    if constexpr (requires(InDstStringT& s, gp::USize n) { s.resizeUninitialized(n); })
    {
        outDst.resizeUninitialized(requiredLen);
        (void)transcodeToBuffer<DstCharT>(src, outDst.data(), requiredLen);
    }
    else
    {
        if constexpr (requires(InDstStringT& s, gp::USize n) { s.reserve(n); })
        {
            outDst.reserve(requiredLen);
        }

        const InSrcCharT* ptr = src.data();
        const InSrcCharT* const end = ptr + src.size();
        DstCharT tempBuf[4]{};
        while (ptr < end)
        {
            const InSrcCharT* next = nullptr;
            const char32_t cp = decodeNext(ptr, end, next);
            ptr = next;
            const gp::USize count = encodeCodepoint<DstCharT>(cp, tempBuf);
            for (gp::USize i = 0u; i < count; ++i)
            {
                outDst.pushBack(tempBuf[i]);
            }
        }
    }
}

/// @brief Forward iterator that decodes Unicode codepoints (`char32_t`) on the fly from a buffer of `InCharT`.
/// @tparam InCharT Underlying code unit character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
template <concepts::IsCharacter InCharT>
class CodepointIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = char32_t;
    using difference_type = gp::ISize;
    using pointer = const char32_t*;
    using reference = char32_t;

public:
    /// @brief Default-constructs a singular end iterator.
    constexpr CodepointIterator() noexcept = default;

    /// @brief Constructs a codepoint iterator positioned at `current` within `[current, end)`.
    /// @param[in] current Pointer to the current code unit.
    /// @param[in] end Pointer one past the last code unit in the view.
    constexpr CodepointIterator(const InCharT* const current, const InCharT* const end) noexcept
        : m_current(current)
        , m_next(current)
        , m_end(end)
    {
        if (m_current != nullptr && m_end != nullptr && m_current < m_end)
        {
            m_codepoint = decodeNext(m_current, m_end, m_next);
        }
    }

    /// @brief Returns the currently decoded Unicode codepoint.
    /// @return Decoded `char32_t` scalar value (or `kReplacementChar` for ill-formed sequences).
    [[nodiscard]] constexpr char32_t operator*() const noexcept
    {
        return m_codepoint;
    }

    /// @brief Advances the iterator to the next Unicode codepoint (pre-increment).
    /// @return Reference to this iterator after advancing.
    constexpr CodepointIterator& operator++() noexcept
    {
        m_current = m_next;
        if (m_current != nullptr && m_end != nullptr && m_current < m_end)
        {
            m_codepoint = decodeNext(m_current, m_end, m_next);
        }
        else
        {
            m_codepoint = kReplacementChar;
        }
        return *this;
    }

    /// @brief Advances the iterator to the next Unicode codepoint (post-increment).
    /// @return Copy of the iterator prior to advancing.
    constexpr CodepointIterator operator++(int) noexcept
    {
        const CodepointIterator copy = *this;
        ++(*this);
        return copy;
    }

    /// @brief Returns a pointer to the underlying code unit at the start of the current codepoint.
    /// @return Pointer to the current code unit.
    [[nodiscard]] constexpr const InCharT* base() const noexcept
    {
        return m_current;
    }

    /// @brief Compares two codepoint iterators for equality.
    /// @param[in] other The iterator to compare against.
    /// @return True if both iterators point to the same underlying code unit position.
    [[nodiscard]] constexpr bool operator==(const CodepointIterator& other) const noexcept
    {
        return m_current == other.m_current;
    }

    /// @brief Compares two codepoint iterators for inequality.
    /// @param[in] other The iterator to compare against.
    /// @return True if the iterators point to different code unit positions.
    [[nodiscard]] constexpr bool operator!=(const CodepointIterator& other) const noexcept
    {
        return m_current != other.m_current;
    }

private:
    const InCharT* m_current{ nullptr };
    const InCharT* m_next{ nullptr };
    const InCharT* m_end{ nullptr };
    char32_t m_codepoint{ kReplacementChar };
};

/// @brief Non-owning range adapter that iterates over decoded `char32_t` Unicode codepoints of a string view.
/// @tparam InCharT Underlying code unit character type (`char`, `char8_t`, `char16_t`, `char32_t`, or `wchar_t`).
template <concepts::IsCharacter InCharT>
class CodepointView
{
public:
    using ValueType = char32_t;
    using Iterator = CodepointIterator<InCharT>;
    using ConstIterator = CodepointIterator<InCharT>;

public:
    /// @brief Default-constructs an empty codepoint view.
    constexpr CodepointView() noexcept = default;

    /// @brief Constructs a codepoint view over the given string view.
    /// @param[in] view Underlying code unit string view.
    constexpr explicit CodepointView(const container::BasicStringView<InCharT> view) noexcept
        : m_view(view)
    {}

    /// @brief Returns an iterator to the first decoded codepoint.
    /// @return `CodepointIterator` positioned at the beginning of the view.
    [[nodiscard]] constexpr ConstIterator begin() const noexcept
    {
        return ConstIterator(m_view.data(), m_view.data() + m_view.size());
    }

    /// @brief Returns an iterator one past the last codepoint.
    /// @return `CodepointIterator` positioned at the end of the view.
    [[nodiscard]] constexpr ConstIterator end() const noexcept
    {
        const InCharT* const endPtr = m_view.data() + m_view.size();
        return ConstIterator(endPtr, endPtr);
    }

    /// @brief Returns a const iterator to the first decoded codepoint.
    /// @return `CodepointIterator` positioned at the beginning of the view.
    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept
    {
        return begin();
    }

    /// @brief Returns a const iterator one past the last codepoint.
    /// @return `CodepointIterator` positioned at the end of the view.
    [[nodiscard]] constexpr ConstIterator cend() const noexcept
    {
        return end();
    }

    /// @brief Checks whether the underlying view is empty.
    /// @return True if the view contains zero code units, false otherwise.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return m_view.isEmpty();
    }

    /// @brief Returns the underlying code unit string view.
    /// @return The wrapped `BasicStringView<InCharT>`.
    [[nodiscard]] constexpr container::BasicStringView<InCharT> baseView() const noexcept
    {
        return m_view;
    }

private:
    container::BasicStringView<InCharT> m_view{};
};

/// @brief Type alias for a UTF-8 `char` codepoint view.
using Utf8CodepointView = CodepointView<char>;

/// @brief Type alias for a UTF-8 `char8_t` codepoint view.
using U8CodepointView = CodepointView<char8_t>;

/// @brief Type alias for a UTF-16 `char16_t` codepoint view.
using Utf16CodepointView = CodepointView<char16_t>;

/// @brief Type alias for a UTF-32 `char32_t` codepoint view.
using Utf32CodepointView = CodepointView<char32_t>;

/// @brief Type alias for a `wchar_t` codepoint view.
using WideCodepointView = CodepointView<wchar_t>;

}   // namespace gp::unicode
