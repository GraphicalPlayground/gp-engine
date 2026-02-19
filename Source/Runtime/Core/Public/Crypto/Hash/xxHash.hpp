// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief xxHash-inspired fast non-cryptographic hashing utilities (32/64-bit variants).
struct xxHash final : public Template::NonInstantiable
{
public:
#if GP_ARCHITECTURE_64BIT
    using HashType = UInt64;   //<! 64-bit hash type.
#else
    using HashType = UInt32;   //<! 32-bit hash type.
#endif

    static constexpr UInt32 Prime32_1 = 0x9E3779B1U;             //<! 32-bit prime 1 for xxHash.
    static constexpr UInt32 Prime32_2 = 0x85EBCA77U;             //<! 32-bit prime 2 for xxHash.
    static constexpr UInt32 Prime32_3 = 0xC2B2AE3DU;             //<! 32-bit prime 3 for xxHash.
    static constexpr UInt32 Prime32_4 = 0x27D4EB2FU;             //<! 32-bit prime 4 for xxHash.
    static constexpr UInt32 Prime32_5 = 0x165667B1U;             //<! 32-bit prime 5 for xxHash.

    static constexpr UInt64 Prime64_1 = 0x9E3779B185EBCA87ULL;   //<! 64-bit prime 1 for xxHash.
    static constexpr UInt64 Prime64_2 = 0xC2B2AE3D27D4EB4FULL;   //<! 64-bit prime 2 for xxHash.
    static constexpr UInt64 Prime64_3 = 0x165667B19E3779F9ULL;   //<! 64-bit prime 3 for xxHash.
    static constexpr UInt64 Prime64_4 = 0x85EBCA77C2B2AE63ULL;   //<! 64-bit prime 4 for xxHash.
    static constexpr UInt64 Prime64_5 = 0x27D4EB2F165667C5ULL;   //<! 64-bit prime 5 for xxHash.

    static constexpr UInt32 DefaultSeed32 = 0U;                  //<! Default seed for 32-bit xxHash.
    static constexpr UInt64 DefaultSeed64 = 0ULL;                //<! Default seed for 64-bit xxHash.

private:
    /// \brief Round function for 32-bit xxHash.
    /// \param acc Accumulator value.
    /// \param input Input value to mix.
    /// \return Updated accumulator.
    GP_NODISCARD static constexpr UInt32 Round32(UInt32 acc, UInt32 input) noexcept
    {
        acc += input * Prime32_2;
        acc = Detail::HashUtilities::Rotl32(acc, 13);
        acc *= Prime32_1;
        return acc;
    }

    /// \brief Avalanche function for 32-bit xxHash to finalize the hash value.
    /// \param h Hash value to avalanche.
    /// \return Avalanched hash value.
    GP_NODISCARD static constexpr UInt32 Avalanche32(UInt32 h) noexcept
    {
        h ^= h >> 15;
        h *= Prime32_2;
        h ^= h >> 13;
        h *= Prime32_3;
        h ^= h >> 16;
        return h;
    }

    /// \brief Round function for 64-bit xxHash.
    /// \param acc Accumulator value.
    /// \param input Input value to mix.
    /// \return Updated accumulator.
    GP_NODISCARD static constexpr UInt64 Round64(UInt64 acc, UInt64 input) noexcept
    {
        acc += input * Prime64_2;
        acc = Detail::HashUtilities::Rotl64(acc, 31);
        acc *= Prime64_1;
        return acc;
    }

    /// \brief Merge round function for 64-bit xxHash to combine accumulator values.
    /// \param acc Accumulator value.
    /// \param val Value to merge.
    /// \return Updated accumulator.
    GP_NODISCARD static constexpr UInt64 MergeRound64(UInt64 acc, UInt64 val) noexcept
    {
        val = Round64(0, val);
        acc ^= val;
        acc = acc * Prime64_1 + Prime64_4;
        return acc;
    }

    /// \brief Avalanche function for 64-bit xxHash to finalize the hash value.
    /// \param h Hash value to avalanche.
    /// \return Avalanched hash value.
    GP_NODISCARD static constexpr UInt64 Avalanche64(UInt64 h) noexcept
    {
        h ^= h >> 33;
        h *= Prime64_2;
        h ^= h >> 29;
        h *= Prime64_3;
        h ^= h >> 32;
        return h;
    }

public:
    /// \brief Compute 32-bit xxHash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional seed value (defaults to zero).
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const char* data, SizeT length, UInt32 seed = DefaultSeed32) noexcept
    {
        UInt32 h;
        SizeT offset = 0;

        if (length >= 16)
        {
            UInt32 v1 = seed + Prime32_1 + Prime32_2;
            UInt32 v2 = seed + Prime32_2;
            UInt32 v3 = seed;
            UInt32 v4 = seed - Prime32_1;

            const SizeT limit = length - 16;
            do {
                v1 = Round32(v1, Detail::HashUtilities::ReadU32LE(data, offset));
                offset += 4;
                v2 = Round32(v2, Detail::HashUtilities::ReadU32LE(data, offset));
                offset += 4;
                v3 = Round32(v3, Detail::HashUtilities::ReadU32LE(data, offset));
                offset += 4;
                v4 = Round32(v4, Detail::HashUtilities::ReadU32LE(data, offset));
                offset += 4;
            }
            while (offset <= limit);

            h = Detail::HashUtilities::Rotl32(v1, 1) + Detail::HashUtilities::Rotl32(v2, 7) +
                Detail::HashUtilities::Rotl32(v3, 12) + Detail::HashUtilities::Rotl32(v4, 18);
        }
        else { h = seed + Prime32_5; }

        h += static_cast<UInt32>(length);

        while (offset + 4 <= length)
        {
            h += Detail::HashUtilities::ReadU32LE(data, offset) * Prime32_3;
            h = Detail::HashUtilities::Rotl32(h, 17) * Prime32_4;
            offset += 4;
        }

        while (offset < length)
        {
            h += static_cast<UInt32>(static_cast<UInt8>(data[offset])) * Prime32_5;
            h = Detail::HashUtilities::Rotl32(h, 11) * Prime32_1;
            ++offset;
        }

        return Avalanche32(h);
    }

    /// \brief Compute 32-bit xxHash from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional seed value.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const void* data, SizeT length, UInt32 seed = DefaultSeed32) noexcept
    {
        return Hash32(static_cast<const char*>(data), length, seed);
    }

    /// \brief Compute 32-bit xxHash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const char* str) noexcept
    {
        return Hash32(str, std::char_traits<char>::length(str), DefaultSeed32);
    }

    /// \brief Compute 32-bit xxHash from a string view.
    /// \param view String view to hash.
    /// \param seed Optional seed value.
    /// \return 32-bit hash value.
    /// \note Accepts FStringView implicitly via its conversion to std::string_view.
    GP_NODISCARD static constexpr UInt32 Hash32(std::string_view view, UInt32 seed = DefaultSeed32) noexcept
    {
        return Hash32(view.data(), view.size(), seed);
    }

    /// \brief Compute 64-bit xxHash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional seed value (defaults to zero).
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* data, SizeT length, UInt64 seed = DefaultSeed64) noexcept
    {
        UInt64 h;
        SizeT offset = 0;

        if (length >= 32)
        {
            UInt64 v1 = seed + Prime64_1 + Prime64_2;
            UInt64 v2 = seed + Prime64_2;
            UInt64 v3 = seed;
            UInt64 v4 = seed - Prime64_1;

            const SizeT limit = length - 32;
            do {
                v1 = Round64(v1, Detail::HashUtilities::ReadU64LE(data, offset));
                offset += 8;
                v2 = Round64(v2, Detail::HashUtilities::ReadU64LE(data, offset));
                offset += 8;
                v3 = Round64(v3, Detail::HashUtilities::ReadU64LE(data, offset));
                offset += 8;
                v4 = Round64(v4, Detail::HashUtilities::ReadU64LE(data, offset));
                offset += 8;
            }
            while (offset <= limit);

            h = Detail::HashUtilities::Rotl64(v1, 1) + Detail::HashUtilities::Rotl64(v2, 7) +
                Detail::HashUtilities::Rotl64(v3, 12) + Detail::HashUtilities::Rotl64(v4, 18);
            h = MergeRound64(h, v1);
            h = MergeRound64(h, v2);
            h = MergeRound64(h, v3);
            h = MergeRound64(h, v4);
        }
        else { h = seed + Prime64_5; }

        h += static_cast<UInt64>(length);

        while (offset + 8 <= length)
        {
            h ^= Round64(0, Detail::HashUtilities::ReadU64LE(data, offset));
            h = Detail::HashUtilities::Rotl64(h, 27) * Prime64_1 + Prime64_4;
            offset += 8;
        }

        while (offset + 4 <= length)
        {
            h ^= static_cast<UInt64>(Detail::HashUtilities::ReadU32LE(data, offset)) * Prime64_1;
            h = Detail::HashUtilities::Rotl64(h, 23) * Prime64_2 + Prime64_3;
            offset += 4;
        }

        while (offset < length)
        {
            h ^= static_cast<UInt64>(static_cast<UInt8>(data[offset])) * Prime64_5;
            h = Detail::HashUtilities::Rotl64(h, 11) * Prime64_1;
            ++offset;
        }

        return Avalanche64(h);
    }

    /// \brief Compute 64-bit xxHash from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional seed value.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const void* data, SizeT length, UInt64 seed = DefaultSeed64) noexcept
    {
        return Hash64(static_cast<const char*>(data), length, seed);
    }

    /// \brief Compute 64-bit xxHash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* str) noexcept
    {
        return Hash64(str, std::char_traits<char>::length(str), DefaultSeed64);
    }

    /// \brief Compute 64-bit xxHash from a string view.
    /// \param view String view to hash.
    /// \param seed Optional seed value.
    /// \return 64-bit hash value.
    /// \note Accepts FStringView implicitly via its conversion to std::string_view.
    GP_NODISCARD static constexpr UInt64 Hash64(std::string_view view, UInt64 seed = DefaultSeed64) noexcept
    {
        return Hash64(view.data(), view.size(), seed);
    }

    /// \brief Convenience architecture-sized hash entry point.
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return Architecture-sized hash.
    GP_NODISCARD static constexpr HashType Hash(const char* data, SizeT length) noexcept
    {
#if GP_ARCHITECTURE_64BIT
        return Hash64(data, length, DefaultSeed64);
#else
        return Hash32(data, length, DefaultSeed32);
#endif
    }

    /// \brief Convenience overload for void pointer input.
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return Architecture-sized hash.
    GP_NODISCARD static constexpr HashType Hash(const void* data, SizeT length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Convenience overload for null-terminated strings.
    /// \param str Null-terminated input string.
    /// \return Architecture-sized hash.
    GP_NODISCARD static constexpr HashType Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Convenience overload for string views.
    /// \param view String view to hash.
    /// \return Architecture-sized hash.
    /// \note Accepts FStringView implicitly via its conversion to std::string_view.
    GP_NODISCARD static constexpr HashType Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }
};

}   // namespace GP::Crypto

/// \brief User-defined literal for computing xxHash at compile time from string literals.
/// \param str String literal to hash.
/// \param len Length of the string literal (automatically provided).
/// \return Compile-time computed xxHash value.
inline constexpr GP::Crypto::xxHash::HashType operator""_xxhash(const char* str, GP::SizeT len) noexcept
{
    return GP::Crypto::xxHash::Hash(str, len);
}
