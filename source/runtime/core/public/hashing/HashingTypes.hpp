// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include <compare>

namespace gp::hashing
{

namespace detail
{

/// @brief Converts a 4-bit nibble (`0..15`) into an ASCII hexadecimal character without pulling in string headers.
/// @param[in] nibble Low 4-bit value masked with `0x0Fu`.
/// @param[in] uppercase True for `'A'..'F'`, false for `'a'..'f'`.
/// @return ASCII hex character.
[[nodiscard]] constexpr char nibbleToHex(const gp::UInt8 nibble, const bool uppercase) noexcept
{
    const gp::UInt8 v = static_cast<gp::UInt8>(nibble & 0x0Fu);
    if (v < 10u)
    {
        return static_cast<char>('0' + v);
    }
    return static_cast<char>((uppercase ? 'A' : 'a') + (v - 10u));
}

/// @brief Parses a single ASCII hexadecimal character into its numeric value (`0..15`).
/// @param[in] ch Candidate hex character.
/// @return Integer value in `[0, 15]`, or `-1` if @p ch is not valid hex.
[[nodiscard]] constexpr gp::Int32 hexCharToNibble(const char ch) noexcept
{
    if (ch >= '0' && ch <= '9')
    {
        return static_cast<gp::Int32>(ch - '0');
    }
    if (ch >= 'a' && ch <= 'f')
    {
        return static_cast<gp::Int32>(10 + (ch - 'a'));
    }
    if (ch >= 'A' && ch <= 'F')
    {
        return static_cast<gp::Int32>(10 + (ch - 'A'));
    }
    return -1;
}

/// @brief Writes a 64-bit integer as 16 big-endian hex characters into @p outBuffer.
/// @param[in] value 64-bit word to format.
/// @param[out] outBuffer Destination array of at least 16 chars.
/// @param[in] uppercase True for uppercase hex digits.
constexpr void writeHex64(const gp::UInt64 value, char* const outBuffer, const bool uppercase) noexcept
{
    for (gp::USize i = 0u; i < 16u; ++i)
    {
        const gp::UInt32 shift = static_cast<gp::UInt32>((15u - i) * 4u);
        const gp::UInt8 nibble = static_cast<gp::UInt8>((value >> shift) & 0x0Full);
        outBuffer[i] = nibbleToHex(nibble, uppercase);
    }
}

/// @brief Parses 16 ASCII hex characters into a 64-bit unsigned integer.
/// @param[in] hex Source pointer containing at least 16 hex characters.
/// @param[out] outValue Receives the parsed 64-bit word on success.
/// @return True if all 16 characters are valid hex digits, false otherwise.
[[nodiscard]] constexpr bool parseHex64(const char* const hex, gp::UInt64& outValue) noexcept
{
    gp::UInt64 acc = 0ULL;
    for (gp::USize i = 0u; i < 16u; ++i)
    {
        const gp::Int32 nib = hexCharToNibble(hex[i]);
        if (nib < 0)
        {
            return false;
        }
        acc = (acc << 4u) | static_cast<gp::UInt64>(nib);
    }
    outValue = acc;
    return true;
}

}   // namespace detail

/// @brief 16-byte trivially copyable 128-bit digest used for asset GUIDs, shader keys, and Murmur3_128 outputs.
/// @details Ordering compares `high` first, then `low`, matching standard 128-bit unsigned integer ordering.
struct Hash128
{
public:
    /// @brief Required buffer capacity (32 hex digits plus trailing null terminator) for `toHexString()`.
    static constexpr gp::USize kHexBufferSize = 33u;

    /// @brief Lower 64 bits of the 128-bit digest.
    gp::UInt64 low{ 0ULL };

    /// @brief Upper 64 bits of the 128-bit digest.
    gp::UInt64 high{ 0ULL };

public:
    /// @brief Default-constructs a zeroed 128-bit digest (`{ 0, 0 }`).
    [[nodiscard]] constexpr Hash128() noexcept = default;

    /// @brief Constructs a 128-bit digest from explicit low and high 64-bit words.
    /// @param[in] inLow Lower 64 bits.
    /// @param[in] inHigh Upper 64 bits.
    [[nodiscard]] constexpr Hash128(const gp::UInt64 inLow, const gp::UInt64 inHigh) noexcept
        : low(inLow)
        , high(inHigh)
    {}

    /// @brief Checks exact 128-bit equality.
    /// @param[in] other Digest to compare against.
    /// @return True if both `low` and `high` match.
    [[nodiscard]] constexpr bool operator==(const Hash128& other) const noexcept = default;

    /// @brief Orders two 128-bit digests lexicographically by `(high, low)`.
    /// @param[in] other Digest to compare against.
    /// @return `std::strong_ordering` result.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const Hash128& other) const noexcept
    {
        if (const std::strong_ordering highCmp = high <=> other.high; highCmp != 0)
        {
            return highCmp;
        }
        return low <=> other.low;
    }

public:
    /// @brief Checks whether both 64-bit halves are zero (`low == 0 && high == 0`).
    /// @return True if zeroed, false otherwise.
    [[nodiscard]] constexpr bool isZero() const noexcept
    {
        return low == 0ULL && high == 0ULL;
    }

    /// @brief Checks whether the digest holds any non-zero bit (`!isZero()`).
    /// @return True if non-zero, false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return !isZero();
    }

    /// @brief Contextual boolean conversion returning `isValid()`.
    /// @return True if non-zero, false if zero.
    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return isValid();
    }

    /// @brief Folds all 128 bits down to a 64-bit hash using CityHash `Hash128to64` mixing.
    /// @return Mixed 64-bit hash suitable for 64-bit hash table buckets.
    [[nodiscard]] constexpr gp::UInt64 foldTo64() const noexcept
    {
        constexpr gp::UInt64 kMul = 0x9DDFEA08EB382D69ULL;
        gp::UInt64 a = (low ^ high) * kMul;
        a ^= (a >> 47u);
        gp::UInt64 b = (high ^ a) * kMul;
        b ^= (b >> 47u);
        b *= kMul;
        return b;
    }

    /// @brief Folds all 128 bits down to a 32-bit hash via `foldTo64()` and Murmur3 finalization.
    /// @return Mixed 32-bit hash.
    [[nodiscard]] constexpr gp::UInt32 foldTo32() const noexcept
    {
        const gp::UInt64 folded = foldTo64();
        gp::UInt32 h = static_cast<gp::UInt32>(folded ^ (folded >> 32u));
        h ^= h >> 16u;
        h *= 0x85EBCA6Bu;
        h ^= h >> 13u;
        h *= 0xC2B2AE35u;
        h ^= h >> 16u;
        return h;
    }

    /// @brief Formats the digest as 32 big-endian hexadecimal digits (`high` followed by `low`) plus null terminator.
    /// @param[out] outBuffer Caller-owned destination buffer.
    /// @param[in] bufferSize Byte capacity of @p outBuffer; must be at least `kHexBufferSize` (`33`).
    /// @param[in] uppercase True for uppercase `'A'..'F'`, false for lowercase `'a'..'f'`.
    /// @return True if formatting succeeded, false if @p outBuffer is null or too small.
    constexpr bool
        toHexString(char* const outBuffer, const gp::USize bufferSize, const bool uppercase = false) const noexcept
    {
        if (outBuffer == nullptr || bufferSize < kHexBufferSize)
        {
            return false;
        }
        detail::writeHex64(high, outBuffer, uppercase);
        detail::writeHex64(low, outBuffer + 16u, uppercase);
        outBuffer[32] = '\0';
        return true;
    }

    /// @brief Parses a 32-character hexadecimal digest string (`high` followed by `low`) into @p outDigest.
    /// @param[in] hexStr Pointer to 32 ASCII hexadecimal digits (no `0x` prefix or hyphens).
    /// @param[in] length Number of characters available at @p hexStr (must equal `32`).
    /// @param[out] outDigest Receives the parsed `Hash128` on success.
    /// @return True if parsing succeeded, false if input is null, wrong length, or contains non-hex characters.
    [[nodiscard]] static constexpr bool
        tryParseHex(const char* const hexStr, const gp::USize length, Hash128& outDigest) noexcept
    {
        if (hexStr == nullptr || length != 32u)
        {
            return false;
        }
        gp::UInt64 parsedHigh = 0ULL;
        gp::UInt64 parsedLow = 0ULL;
        if (!detail::parseHex64(hexStr, parsedHigh) || !detail::parseHex64(hexStr + 16u, parsedLow))
        {
            return false;
        }
        outDigest = Hash128(parsedLow, parsedHigh);
        return true;
    }
};

static_assert(sizeof(Hash128) == 16u, "Hash128 must be 16 bytes.");
static_assert(alignof(Hash128) == 8u, "Hash128 must be 8-byte aligned.");

/// @brief Tagged uniform result container returned by runtime-dispatched `DynamicHasher` and `computeHash()`.
/// @details Always occupies 24 bytes without heap allocations. Use `toUInt32()`, `toUInt64()`, or `toHash128()`
///          to convert any algorithm width, or read `bits`, `width`, and `algorithm` directly.
struct HashDigest
{
public:
    /// @brief Underlying storage holding up to 128 digest bits.
    Hash128 bits{};

    /// @brief Native output width of the algorithm that produced this digest.
    HashWidth width{ HashWidth::Bits64 };

    /// @brief Algorithm that produced this digest.
    HashAlgorithm algorithm{ HashAlgorithm::Fnv1a64 };

public:
    /// @brief Default-constructs a zeroed 64-bit digest.
    [[nodiscard]] constexpr HashDigest() noexcept = default;

    /// @brief Constructs a 32-bit digest result.
    /// @tparam InDigestT 32-bit unsigned integral digest type.
    /// @param[in] value 32-bit digest value.
    /// @param[in] inAlgorithm Originating algorithm.
    template <gp::concepts::IsUnsignedIntegral InDigestT>
    requires(sizeof(InDigestT) == 4u)
    [[nodiscard]] constexpr HashDigest(const InDigestT value, const HashAlgorithm inAlgorithm) noexcept
        : bits(static_cast<gp::UInt64>(value), 0ULL)
        , width(HashWidth::Bits32)
        , algorithm(inAlgorithm)
    {}

    /// @brief Constructs a 64-bit digest result (`gp::UInt64`, `unsigned long`, or `unsigned long long`).
    /// @tparam InDigestT 64-bit unsigned integral digest type.
    /// @param[in] value 64-bit digest value.
    /// @param[in] inAlgorithm Originating algorithm.
    template <gp::concepts::IsUnsignedIntegral InDigestT>
    requires(sizeof(InDigestT) == 8u)
    [[nodiscard]] constexpr HashDigest(const InDigestT value, const HashAlgorithm inAlgorithm) noexcept
        : bits(static_cast<gp::UInt64>(value), 0ULL)
        , width(HashWidth::Bits64)
        , algorithm(inAlgorithm)
    {}

    /// @brief Constructs a 128-bit digest result.
    /// @param[in] value 128-bit digest value.
    /// @param[in] inAlgorithm Originating algorithm.
    [[nodiscard]] constexpr HashDigest(const Hash128& value, const HashAlgorithm inAlgorithm) noexcept
        : bits(value)
        , width(HashWidth::Bits128)
        , algorithm(inAlgorithm)
    {}

    /// @brief Compares two `HashDigest` instances by algorithm, width, and digest bits.
    [[nodiscard]] constexpr bool operator==(const HashDigest& other) const noexcept = default;

public:
    /// @brief Returns the raw 32-bit value for a 32-bit digest, or folds wider digests down to 32 bits.
    /// @return 32-bit digest representation.
    [[nodiscard]] constexpr gp::UInt32 toUInt32() const noexcept
    {
        if (width == HashWidth::Bits32)
        {
            return static_cast<gp::UInt32>(bits.low);
        }
        return bits.foldTo32();
    }

    /// @brief Returns the raw 64-bit value for a 64-bit digest, zero-extends 32-bit digests, or folds 128-bit digests.
    /// @return 64-bit digest representation.
    [[nodiscard]] constexpr gp::UInt64 toUInt64() const noexcept
    {
        if (width == HashWidth::Bits128)
        {
            return bits.foldTo64();
        }
        return bits.low;
    }

    /// @brief Returns the full 128-bit storage (`{ value, 0 }` for 32-bit and 64-bit digests).
    /// @return 128-bit digest representation.
    [[nodiscard]] constexpr Hash128 toHash128() const noexcept
    {
        return bits;
    }

    /// @brief Checks whether all stored digest bits are zero.
    /// @return True if `bits.isZero()` is true, false otherwise.
    [[nodiscard]] constexpr bool isZero() const noexcept
    {
        return bits.isZero();
    }

    /// @brief Checks whether any stored digest bit is non-zero.
    /// @return True if `bits.isValid()` is true, false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return bits.isValid();
    }
};

static_assert(sizeof(HashDigest) == 24u, "HashDigest must be 24 bytes.");

/// @brief Compile-time mapping from a `HashAlgorithm` enumerator to its concrete streaming hasher class.
/// @tparam InAlgorithm Target `HashAlgorithm` enumerator.
template <HashAlgorithm InAlgorithm>
struct HashAlgorithmTraits;

/// @brief Convenience alias resolving a `HashAlgorithm` enumerator to its concrete streaming hasher class.
/// @tparam InAlgorithm Target `HashAlgorithm` enumerator.
template <HashAlgorithm InAlgorithm>
using Hasher = typename HashAlgorithmTraits<InAlgorithm>::Type;

}   // namespace gp::hashing
