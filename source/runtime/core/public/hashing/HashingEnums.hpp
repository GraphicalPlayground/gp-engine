// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep

namespace gp::hashing
{

/// @brief Identifies a built-in non-cryptographic hashing or checksum algorithm.
enum class HashAlgorithm : gp::UInt8
{
    /// @brief 32-bit FNV-1a byte-wise hash (`0x811C9DC5` basis, `0x01000193` prime).
    Fnv1a32 = 0u,

    /// @brief 64-bit FNV-1a byte-wise hash (`0xCBF29CE484222325` basis, `0x100000001B3` prime).
    Fnv1a64 = 1u,

    /// @brief Standard IEEE 802.3 32-bit CRC checksum (`0xEDB88320` reflected polynomial).
    Crc32 = 2u,

    /// @brief Castagnoli 32-bit CRC checksum (`0x82F63B78` reflected polynomial, hardware-accelerable on SSE4.2/ARMv8).
    Crc32C = 3u,

    /// @brief MurmurHash3 x86 32-bit block-mixed hash.
    Murmur3_32 = 4u,

    /// @brief MurmurHash3 x64 128-bit digest (`Hash128`), modeled after Decima asset and path digests.
    Murmur3_128 = 5u,

    /// @brief XXH32 32-bit streaming hash for bulk asset and buffer checks.
    XxHash32 = 6u,

    /// @brief XXH64 64-bit streaming hash for bulk asset, shader, and archive payloads.
    XxHash64 = 7u,

    /// @brief Wyhash final4 64-bit hash using 128-bit multiply-fold mixing for fast general-purpose keys.
    Wyhash64 = 8u,

    /// @brief SipHash-2-4 keyed 64-bit PRF resistant to hash-flooding (HashDoS) attacks on network tables.
    SipHash24 = 9u,

    /// @brief FIPS 180-4 SHA-256 cryptographic 256-bit digest (`Hash256`) for content-addressable assets and DDC keys.
    Sha256 = 10u,

    /// @brief Sentinel count of supported built-in algorithms.
    Count = 11u
};

/// @brief Controls whether string hashing distinguishes uppercase and lowercase ASCII letters.
enum class HashCase : gp::UInt8
{
    /// @brief Hashes exact character code units (`'A' != 'a'`).
    CaseSensitive = 0u,

    /// @brief Folds ASCII `'A'..'Z'` to `'a'..'z'` before hashing without consulting OS locales.
    IgnoreCase = 1u
};

/// @brief Bit width of a hash digest returned by an algorithm or `HashDigest`.
enum class HashWidth : gp::UInt16
{
    /// @brief 32-bit unsigned integer digest (`gp::UInt32`).
    Bits32 = 32u,

    /// @brief 64-bit unsigned integer digest (`gp::UInt64`).
    Bits64 = 64u,

    /// @brief 128-bit digest (`gp::hashing::Hash128`).
    Bits128 = 128u,

    /// @brief 256-bit digest (`gp::hashing::Hash256`).
    Bits256 = 256u
};

/// @brief Returns the canonical short name of a `HashAlgorithm` enumerator.
/// @param[in] algorithm Algorithm identifier to query.
/// @return Null-terminated ASCII literal naming the algorithm.
[[nodiscard]] constexpr const char* algorithmToString(const HashAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
    case HashAlgorithm::Fnv1a32:
        return "Fnv1a32";
    case HashAlgorithm::Fnv1a64:
        return "Fnv1a64";
    case HashAlgorithm::Crc32:
        return "Crc32";
    case HashAlgorithm::Crc32C:
        return "Crc32C";
    case HashAlgorithm::Murmur3_32:
        return "Murmur3_32";
    case HashAlgorithm::Murmur3_128:
        return "Murmur3_128";
    case HashAlgorithm::XxHash32:
        return "XxHash32";
    case HashAlgorithm::XxHash64:
        return "XxHash64";
    case HashAlgorithm::Wyhash64:
        return "Wyhash64";
    case HashAlgorithm::SipHash24:
        return "SipHash24";
    case HashAlgorithm::Sha256:
        return "Sha256";
    case HashAlgorithm::Count:
        // falls through
    default:
        return "Unknown";
    }
}

/// @brief Returns the output digest width in bits for @p algorithm.
/// @param[in] algorithm Algorithm identifier to query.
/// @return `32`, `64`, `128`, or `256` for valid enumerators; `0` otherwise.
[[nodiscard]] constexpr gp::UInt32 algorithmDigestBits(const HashAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
    case HashAlgorithm::Fnv1a32:
    case HashAlgorithm::Crc32:
    case HashAlgorithm::Crc32C:
    case HashAlgorithm::Murmur3_32:
    case HashAlgorithm::XxHash32:
        return 32u;
    case HashAlgorithm::Fnv1a64:
    case HashAlgorithm::XxHash64:
    case HashAlgorithm::Wyhash64:
    case HashAlgorithm::SipHash24:
        return 64u;
    case HashAlgorithm::Murmur3_128:
        return 128u;
    case HashAlgorithm::Sha256:
        return 256u;
    case HashAlgorithm::Count:
        // falls through
    default:
        return 0u;
    }
}

/// @brief Returns the `HashWidth` category corresponding to @p algorithm.
/// @param[in] algorithm Algorithm identifier to query.
/// @return `HashWidth::Bits32`, `HashWidth::Bits64`, `HashWidth::Bits128`, or `HashWidth::Bits256`.
[[nodiscard]] constexpr HashWidth algorithmWidth(const HashAlgorithm algorithm) noexcept
{
    const gp::UInt32 bits = algorithmDigestBits(algorithm);
    if (bits == 256u)
    {
        return HashWidth::Bits256;
    }
    if (bits == 128u)
    {
        return HashWidth::Bits128;
    }
    if (bits == 64u)
    {
        return HashWidth::Bits64;
    }
    return HashWidth::Bits32;
}

/// @brief Reports whether @p algorithm is an error-detecting CRC checksum.
/// @param[in] algorithm Algorithm identifier to inspect.
/// @return True for `Crc32` and `Crc32C`, false for non-CRC hash functions.
[[nodiscard]] constexpr bool isChecksumAlgorithm(const HashAlgorithm algorithm) noexcept
{
    return algorithm == HashAlgorithm::Crc32 || algorithm == HashAlgorithm::Crc32C;
}

/// @brief Reports whether @p algorithm produces a cryptographic collision-resistant digest.
/// @param[in] algorithm Algorithm identifier to inspect.
/// @return True for `Sha256`, false otherwise.
[[nodiscard]] constexpr bool isCryptographicAlgorithm(const HashAlgorithm algorithm) noexcept
{
    return algorithm == HashAlgorithm::Sha256;
}

/// @brief Reports whether @p algorithm requires or supports a secret 128-bit key against HashDoS attacks.
/// @param[in] algorithm Algorithm identifier to inspect.
/// @return True for `SipHash24`, false otherwise.
[[nodiscard]] constexpr bool isKeyedAlgorithm(const HashAlgorithm algorithm) noexcept
{
    return algorithm == HashAlgorithm::SipHash24;
}

}   // namespace gp::hashing
