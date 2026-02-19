// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief MurmurHash3 implementation (32-bit, 128-bit variants).
/// Provides constexpr Murmur3 hashing helpers for strings and binary data.
struct Murmur3 final : public Template::NonInstantiable
{
public:
#if GP_ARCHITECTURE_64BIT
    using HashType = UInt64;   //<! 64-bit hash type for 64-bit architectures.
#else
    using HashType = UInt32;   //<! 32-bit hash type for 32-bit architectures.
#endif

    using Hash128Result = GP::Crypto::Hash128Result;

public:
    static constexpr UInt32 DefaultSeed32 = 0U;     //<! Default seed for 32-bit Murmur3.
    static constexpr UInt64 DefaultSeed64 = 0ULL;   //<! Default seed for 64-bit Murmur3.

private:
    /// \brief Finalization mix function for 32-bit Murmur3 to avalanche the bits.
    /// \param h Hash value to mix.
    /// \return Mixed hash value.
    GP_NODISCARD static constexpr UInt32 FMix32(UInt32 h) noexcept
    {
        h ^= h >> 16;
        h *= 0x85EBCA6BU;
        h ^= h >> 13;
        h *= 0xC2B2AE35U;
        h ^= h >> 16;
        return h;
    }

    /// \brief Finalization mix function for 64-bit Murmur3 to avalanche the bits.
    /// \param k Hash value to mix.
    /// \return Mixed hash value.
    GP_NODISCARD static constexpr UInt64 FMix64(UInt64 k) noexcept
    {
        k ^= k >> 33;
        k *= 0xFF51AFD7ED558CCDULL;
        k ^= k >> 33;
        k *= 0xC4CEB9FE1A85EC53ULL;
        k ^= k >> 33;
        return k;
    }

public:
    /// \brief Compute 32-bit Murmur3 hash for a buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional seed value (defaults to zero).
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const char* data, SizeT length, UInt32 seed = DefaultSeed32) noexcept
    {
        const SizeT nblocks = length / 4;
        UInt32 h1 = seed;

        constexpr UInt32 c1 = 0xCC9E2D51U;
        constexpr UInt32 c2 = 0x1B873593U;

        for (SizeT i = 0; i < nblocks; ++i)
        {
            UInt32 k1 = Detail::HashUtilities::ReadU32LE(data, i * 4);
            k1 *= c1;
            k1 = Detail::HashUtilities::Rotl32(k1, 15);
            k1 *= c2;

            h1 ^= k1;
            h1 = Detail::HashUtilities::Rotl32(h1, 13);
            h1 = h1 * 5 + 0xE6546B64U;
        }

        const SizeT tail = nblocks * 4;
        UInt32 k1 = 0;

        switch (length & 3)
        {
        case 3: k1 ^= static_cast<UInt32>(static_cast<UInt8>(data[tail + 2])) << 16; [[fallthrough]];
        case 2: k1 ^= static_cast<UInt32>(static_cast<UInt8>(data[tail + 1])) << 8; [[fallthrough]];
        case 1:
            k1 ^= static_cast<UInt32>(static_cast<UInt8>(data[tail]));
            k1 *= c1;
            k1 = Detail::HashUtilities::Rotl32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
        }

        h1 ^= static_cast<UInt32>(length);
        h1 = FMix32(h1);
        return h1;
    }

    /// \brief Compute 32-bit Murmur3 from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional seed value (defaults to zero).
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const void* data, SizeT length, UInt32 seed = DefaultSeed32) noexcept
    {
        return Hash32(static_cast<const char*>(data), length, seed);
    }

    /// \brief Compute 32-bit Murmur3 for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const char* str) noexcept
    {
        return Hash32(str, std::char_traits<char>::length(str), DefaultSeed32);
    }

    /// \brief Compute 32-bit Murmur3 from a string view.
    /// \param view String view to hash.
    /// \param seed Optional seed value.
    /// \return 32-bit hash value.
    /// \note Accepts FStringView implicitly via its conversion to std::string_view.
    GP_NODISCARD static constexpr UInt32 Hash32(std::string_view view, UInt32 seed = DefaultSeed32) noexcept
    {
        return Hash32(view.data(), view.size(), seed);
    }

    /// \brief Compute 128-bit Murmur3 hash for a buffer.
    /// \param data Pointer to input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional 64-bit seed.
    /// \return 128-bit hash result (low/high).
    GP_NODISCARD static constexpr Hash128Result
        Hash128(const char* data, SizeT length, UInt64 seed = DefaultSeed64) noexcept
    {
        const SizeT nblocks = length / 16;
        UInt64 h1 = seed;
        UInt64 h2 = seed;

        constexpr UInt64 c1 = 0x87C37B91114253D5ULL;
        constexpr UInt64 c2 = 0x4CF5AD432745937FULL;

        for (SizeT i = 0; i < nblocks; ++i)
        {
            UInt64 k1 = Detail::HashUtilities::ReadU64LE(data, i * 16);
            UInt64 k2 = Detail::HashUtilities::ReadU64LE(data, i * 16 + 8);

            k1 *= c1;
            k1 = Detail::HashUtilities::Rotl64(k1, 31);
            k1 *= c2;
            h1 ^= k1;
            h1 = Detail::HashUtilities::Rotl64(h1, 27);
            h1 += h2;
            h1 = h1 * 5 + 0x52DCE729;

            k2 *= c2;
            k2 = Detail::HashUtilities::Rotl64(k2, 33);
            k2 *= c1;
            h2 ^= k2;
            h2 = Detail::HashUtilities::Rotl64(h2, 31);
            h2 += h1;
            h2 = h2 * 5 + 0x38495AB5;
        }

        const SizeT tail = nblocks * 16;
        UInt64 k1 = 0;
        UInt64 k2 = 0;

        switch (length & 15)
        {
        case 15: k2 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 14])) << 48; [[fallthrough]];
        case 14: k2 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 13])) << 40; [[fallthrough]];
        case 13: k2 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 12])) << 32; [[fallthrough]];
        case 12: k2 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 11])) << 24; [[fallthrough]];
        case 11: k2 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 10])) << 16; [[fallthrough]];
        case 10: k2 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 9])) << 8; [[fallthrough]];
        case 9:
            k2 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 8]));
            k2 *= c2;
            k2 = Detail::HashUtilities::Rotl64(k2, 33);
            k2 *= c1;
            h2 ^= k2;
            [[fallthrough]];
        case 8: k1 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 7])) << 56; [[fallthrough]];
        case 7: k1 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 6])) << 48; [[fallthrough]];
        case 6: k1 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 5])) << 40; [[fallthrough]];
        case 5: k1 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 4])) << 32; [[fallthrough]];
        case 4: k1 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 3])) << 24; [[fallthrough]];
        case 3: k1 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 2])) << 16; [[fallthrough]];
        case 2: k1 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail + 1])) << 8; [[fallthrough]];
        case 1:
            k1 ^= static_cast<UInt64>(static_cast<UInt8>(data[tail]));
            k1 *= c1;
            k1 = Detail::HashUtilities::Rotl64(k1, 31);
            k1 *= c2;
            h1 ^= k1;
        }

        h1 ^= static_cast<UInt64>(length);
        h2 ^= static_cast<UInt64>(length);

        h1 += h2;
        h2 += h1;

        h1 = FMix64(h1);
        h2 = FMix64(h2);

        h1 += h2;
        h2 += h1;

        return { h1, h2 };
    }

    /// \brief Compute 128-bit Murmur3 from a void pointer.
    /// \param data Pointer to input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional 64-bit seed.
    /// \return 128-bit hash result (low/high).
    GP_NODISCARD static constexpr Hash128Result
        Hash128(const void* data, SizeT length, UInt64 seed = DefaultSeed64) noexcept
    {
        return Hash128(static_cast<const char*>(data), length, seed);
    }

    /// \brief Compute 128-bit Murmur3 for a null-terminated string.
    /// \param str Null-terminated input string.
    /// \return 128-bit hash result (low/high).
    GP_NODISCARD static constexpr Hash128Result Hash128(const char* str) noexcept
    {
        return Hash128(str, std::char_traits<char>::length(str), DefaultSeed64);
    }

    /// \brief Compute 128-bit Murmur3 from a string view.
    /// \param view String view to hash.
    /// \param seed Optional 64-bit seed.
    /// \return 128-bit hash result (low/high).
    /// \note Accepts FStringView implicitly via its conversion to std::string_view.
    GP_NODISCARD static constexpr Hash128Result Hash128(std::string_view view, UInt64 seed = DefaultSeed64) noexcept
    {
        return Hash128(view.data(), view.size(), seed);
    }

    /// \brief Convenience: architecture-sized hash (64-bit returns low 64-bits of 128-bit result).
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return Architecture-sized hash.
    GP_NODISCARD static constexpr HashType Hash(const char* data, SizeT length) noexcept
    {
#if GP_ARCHITECTURE_64BIT
        return Hash128(data, length).low;
#else
        return Hash32(data, length);
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

/// \brief User-defined literal for computing Murmur3 hash at compile time from string literals.
/// \param str String literal to hash.
/// \param len Length of the string literal (automatically provided).
/// \return Compile-time computed Murmur3 hash value.
inline constexpr GP::UInt32 operator""_murmur3(const char* str, GP::SizeT len) noexcept
{
    return GP::Crypto::Murmur3::Hash32(str, len);
}
