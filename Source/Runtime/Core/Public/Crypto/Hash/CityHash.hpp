// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief CityHash - Google's fast non-cryptographic hash function for strings.
/// Based on CityHash64/CityHash128 by Geoff Pike and Jyrki Alakuijala.
/// Excellent quality and speed for medium-to-large inputs on modern CPUs.
struct CityHash final : public Template::NonInstantiable
{
public:
    using HashType = UInt64;   //<! CityHash primary output is 64-bit.

private:
    static constexpr UInt64 K0 = 0xC3A5C85C97CB3127ULL;   //<! Internal mixing constant.
    static constexpr UInt64 K1 = 0xB492B66FBE98F273ULL;   //<! Internal mixing constant.
    static constexpr UInt64 K2 = 0x9AE16A3B2F90404FULL;   //<! Internal mixing constant.

    GP_NODISCARD static constexpr UInt64 ShiftMix(UInt64 val) noexcept { return val ^ (val >> 47); }

    GP_NODISCARD static constexpr UInt64 HashLen16(UInt64 u, UInt64 v, UInt64 mul) noexcept
    {
        UInt64 a = (u ^ v) * mul;
        a ^= (a >> 47);
        UInt64 b = (v ^ a) * mul;
        b ^= (b >> 47);
        b *= mul;
        return b;
    }

    GP_NODISCARD static constexpr UInt64 HashLen0To16(const char* data, SizeT length) noexcept
    {
        if (length >= 8)
        {
            UInt64 mul = K2 + length * 2;
            UInt64 a = Detail::HashUtilities::ReadU64LE(data, 0) + K2;
            UInt64 b = Detail::HashUtilities::ReadU64LE(data, length - 8);
            UInt64 c = Detail::HashUtilities::Rotl64(b, 37) * mul + a;
            UInt64 d = (Detail::HashUtilities::Rotl64(a, 25) + b) * mul;
            return HashLen16(c, d, mul);
        }
        if (length >= 4)
        {
            UInt64 mul = K2 + length * 2;
            UInt64 a = static_cast<UInt64>(Detail::HashUtilities::ReadU32LE(data, 0));
            return HashLen16(
                length + (a << 3), static_cast<UInt64>(Detail::HashUtilities::ReadU32LE(data, length - 4)), mul
            );
        }
        if (length > 0)
        {
            UInt8 a = static_cast<UInt8>(data[0]);
            UInt8 b = static_cast<UInt8>(data[length >> 1]);
            UInt8 c = static_cast<UInt8>(data[length - 1]);
            UInt32 y = static_cast<UInt32>(a) + (static_cast<UInt32>(b) << 8);
            UInt32 z = static_cast<UInt32>(length) + (static_cast<UInt32>(c) << 2);
            return ShiftMix(y * K2 ^ z * K0) * K2;
        }
        return K2;
    }

    GP_NODISCARD static constexpr UInt64 HashLen17To32(const char* data, SizeT length) noexcept
    {
        UInt64 mul = K2 + length * 2;
        UInt64 a = Detail::HashUtilities::ReadU64LE(data, 0) * K1;
        UInt64 b = Detail::HashUtilities::ReadU64LE(data, 8);
        UInt64 c = Detail::HashUtilities::ReadU64LE(data, length - 8) * mul;
        UInt64 d = Detail::HashUtilities::ReadU64LE(data, length - 16) * K2;
        return HashLen16(
            Detail::HashUtilities::Rotl64(a + b, 43) + Detail::HashUtilities::Rotl64(c, 30) + d,
            a + Detail::HashUtilities::Rotl64(b + K2, 18) + c,
            mul
        );
    }

    GP_NODISCARD static constexpr UInt64 HashLen33To64(const char* data, SizeT length) noexcept
    {
        UInt64 mul = K2 + length * 2;
        UInt64 a = Detail::HashUtilities::ReadU64LE(data, 0) * K2;
        UInt64 b = Detail::HashUtilities::ReadU64LE(data, 8);
        UInt64 c = Detail::HashUtilities::ReadU64LE(data, length - 24);
        UInt64 d = Detail::HashUtilities::ReadU64LE(data, length - 32);
        UInt64 e = Detail::HashUtilities::ReadU64LE(data, 16) * K2;
        UInt64 f = Detail::HashUtilities::ReadU64LE(data, 24) * 9;
        UInt64 g = Detail::HashUtilities::ReadU64LE(data, length - 8);
        UInt64 h = Detail::HashUtilities::ReadU64LE(data, length - 16) * mul;
        UInt64 u = Detail::HashUtilities::Rotl64(a + g, 43) + (Detail::HashUtilities::Rotl64(b, 30) + c) * 9;
        UInt64 v = ((a + g) ^ d) + f + 1;
        UInt64 w = Detail::HashUtilities::ByteSwap64((u + v) * mul) + h;
        UInt64 x = Detail::HashUtilities::Rotl64(e + f, 42) + c;
        UInt64 y = (Detail::HashUtilities::ByteSwap64((v + w) * mul) + g) * mul;
        UInt64 z = e + f + c;
        a = Detail::HashUtilities::ByteSwap64((x + z) * mul + y) + b;
        b = ShiftMix((z + a) * mul + d + h) * mul;
        return b + x;
    }

    struct WeakPair
    {
        UInt64 first;
        UInt64 second;
    };

    GP_NODISCARD static constexpr WeakPair
        WeakHashLen32WithSeeds(UInt64 w, UInt64 x, UInt64 y, UInt64 z, UInt64 a, UInt64 b) noexcept
    {
        a += w;
        b = Detail::HashUtilities::Rotl64(b + a + z, 21);
        UInt64 c = a;
        a += x;
        a += y;
        b += Detail::HashUtilities::Rotl64(a, 44);
        return { a + z, b + c };
    }

    GP_NODISCARD static constexpr WeakPair
        WeakHashLen32WithSeeds(const char* data, SizeT offset, UInt64 a, UInt64 b) noexcept
    {
        return WeakHashLen32WithSeeds(
            Detail::HashUtilities::ReadU64LE(data, offset),
            Detail::HashUtilities::ReadU64LE(data, offset + 8),
            Detail::HashUtilities::ReadU64LE(data, offset + 16),
            Detail::HashUtilities::ReadU64LE(data, offset + 24),
            a,
            b
        );
    }

public:
    /// \brief Compute 64-bit CityHash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* data, SizeT length) noexcept
    {
        if (length <= 32)
        {
            if (length <= 16) { return HashLen0To16(data, length); }
            return HashLen17To32(data, length);
        }
        if (length <= 64) { return HashLen33To64(data, length); }

        UInt64 x = Detail::HashUtilities::ReadU64LE(data, length - 40);
        UInt64 y =
            Detail::HashUtilities::ReadU64LE(data, length - 16) + Detail::HashUtilities::ReadU64LE(data, length - 56);
        UInt64 z = HashLen16(
            Detail::HashUtilities::ReadU64LE(data, length - 48) + length,
            Detail::HashUtilities::ReadU64LE(data, length - 24),
            K2
        );

        auto v = WeakHashLen32WithSeeds(data, length - 64, length, z);
        auto w = WeakHashLen32WithSeeds(data, length - 32, y + K1, x);
        x = x * K1 + Detail::HashUtilities::ReadU64LE(data, 0);

        SizeT offset = 0;
        const SizeT endOffset = ((length - 1) / 64) * 64;

        do {
            x = Detail::HashUtilities::Rotl64(
                    x + y + v.first + Detail::HashUtilities::ReadU64LE(data, offset + 8), 37
                ) *
                K1;
            y = Detail::HashUtilities::Rotl64(y + v.second + Detail::HashUtilities::ReadU64LE(data, offset + 48), 42) *
                K1;
            x ^= w.second;
            y += v.first + Detail::HashUtilities::ReadU64LE(data, offset + 40);
            z = Detail::HashUtilities::Rotl64(z + w.first, 33) * K1;
            v = WeakHashLen32WithSeeds(data, offset, v.second * K1, x + w.first);
            w = WeakHashLen32WithSeeds(
                data, offset + 32, z + w.second, y + Detail::HashUtilities::ReadU64LE(data, offset + 16)
            );
            UInt64 tmp = z;
            z = x;
            x = tmp;
            offset += 64;
        }
        while (offset < endOffset);

        return HashLen16(
            HashLen16(v.first, w.first, K2) + ShiftMix(y) * K0 + z, HashLen16(v.second, w.second, K2) + x, K2
        );
    }

    /// \brief Compute 64-bit CityHash from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const void* data, SizeT length) noexcept
    {
        return Hash64(static_cast<const char*>(data), length);
    }

    /// \brief Compute 64-bit CityHash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* str) noexcept
    {
        return Hash64(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute 64-bit CityHash from a string view.
    /// \param view String view to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(std::string_view view) noexcept
    {
        return Hash64(view.data(), view.size());
    }

    /// \brief Compute 64-bit CityHash with a seed.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Seed value.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 HashWithSeed64(const char* data, SizeT length, UInt64 seed) noexcept
    {
        return HashLen16(Hash64(data, length) - seed, K2, K2);
    }

    /// \brief Compute 128-bit CityHash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 128-bit hash result.
    GP_NODISCARD static constexpr Hash128Result Hash128(const char* data, SizeT length) noexcept
    {
        if (length >= 16)
        {
            UInt64 seedLow = Detail::HashUtilities::ReadU64LE(data, 0);
            UInt64 seedHigh = Detail::HashUtilities::ReadU64LE(data, 8) + K0;
            return Hash128WithSeed(data + 16, length - 16, seedLow, seedHigh);
        }
        return Hash128WithSeed(data, length, K0, K1);
    }

    /// \brief Compute 128-bit CityHash from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 128-bit hash result.
    GP_NODISCARD static constexpr Hash128Result Hash128(const void* data, SizeT length) noexcept
    {
        return Hash128(static_cast<const char*>(data), length);
    }

    /// \brief Compute 128-bit CityHash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 128-bit hash result.
    GP_NODISCARD static constexpr Hash128Result Hash128(const char* str) noexcept
    {
        return Hash128(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute 128-bit CityHash from a string view.
    /// \param view String view to hash.
    /// \return 128-bit hash result.
    GP_NODISCARD static constexpr Hash128Result Hash128(std::string_view view) noexcept
    {
        return Hash128(view.data(), view.size());
    }

    /// \brief Convenience architecture-sized hash entry point.
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr HashType Hash(const char* data, SizeT length) noexcept
    {
        return Hash64(data, length);
    }

    /// \brief Convenience overload for void pointer input.
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr HashType Hash(const void* data, SizeT length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Convenience overload for null-terminated strings.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr HashType Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Convenience overload for string views.
    /// \param view String view to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr HashType Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }

private:
    GP_NODISCARD static constexpr Hash128Result
        Hash128WithSeed(const char* data, SizeT length, UInt64 seedLow, UInt64 seedHigh) noexcept
    {
        if (length < 128) { return CityMurmur(data, length, seedLow, seedHigh); }

        UInt64 x = seedLow;
        UInt64 y = seedHigh;
        UInt64 z = static_cast<UInt64>(length) * K1;
        UInt64 vFirst = Detail::HashUtilities::Rotl64(y ^ K1, 49) * K1 + Detail::HashUtilities::ReadU64LE(data, 0);
        auto v = WeakPair{ vFirst,
                           Detail::HashUtilities::Rotl64(vFirst, 42) * K1 + Detail::HashUtilities::ReadU64LE(data, 8) };
        auto w = WeakPair{ Detail::HashUtilities::Rotl64(y + z, 35) * K1 + x,
                           Detail::HashUtilities::Rotl64(x + Detail::HashUtilities::ReadU64LE(data, 88), 53) * K1 };

        SizeT offset = 0;
        do {
            x = Detail::HashUtilities::Rotl64(
                    x + y + v.first + Detail::HashUtilities::ReadU64LE(data, offset + 8), 37
                ) *
                K1;
            y = Detail::HashUtilities::Rotl64(y + v.second + Detail::HashUtilities::ReadU64LE(data, offset + 48), 42) *
                K1;
            x ^= w.second;
            y += v.first + Detail::HashUtilities::ReadU64LE(data, offset + 40);
            z = Detail::HashUtilities::Rotl64(z + w.first, 33) * K1;
            v = WeakHashLen32WithSeeds(data, offset, v.second * K1, x + w.first);
            w = WeakHashLen32WithSeeds(
                data, offset + 32, z + w.second, y + Detail::HashUtilities::ReadU64LE(data, offset + 16)
            );
            UInt64 tmp = z;
            z = x;
            x = tmp;
            offset += 64;

            x = Detail::HashUtilities::Rotl64(
                    x + y + v.first + Detail::HashUtilities::ReadU64LE(data, offset + 8), 37
                ) *
                K1;
            y = Detail::HashUtilities::Rotl64(y + v.second + Detail::HashUtilities::ReadU64LE(data, offset + 48), 42) *
                K1;
            x ^= w.second;
            y += v.first + Detail::HashUtilities::ReadU64LE(data, offset + 40);
            z = Detail::HashUtilities::Rotl64(z + w.first, 33) * K1;
            v = WeakHashLen32WithSeeds(data, offset, v.second * K1, x + w.first);
            w = WeakHashLen32WithSeeds(
                data, offset + 32, z + w.second, y + Detail::HashUtilities::ReadU64LE(data, offset + 16)
            );
            tmp = z;
            z = x;
            x = tmp;
            offset += 64;
        }
        while (offset < length - 128);

        x += Detail::HashUtilities::Rotl64(v.first + z, 49) * K0;
        y = y * K0 + Detail::HashUtilities::Rotl64(w.second, 37);
        z = z * K0 + Detail::HashUtilities::Rotl64(w.first, 27);
        w.first *= 9;
        v.first *= K0;

        for (SizeT tailLen = 0; tailLen < length - offset; tailLen += 32)
        {
            y = Detail::HashUtilities::Rotl64(x + y, 42) * K0 + v.second;
            w.first += Detail::HashUtilities::ReadU64LE(data, offset + tailLen + 16);
            x = x * K0 + w.first;
            z += w.second + Detail::HashUtilities::ReadU64LE(data, offset + tailLen);
            w.second += v.first;
            v = WeakHashLen32WithSeeds(data, offset + tailLen, v.first + z, v.second);
            v.first *= K0;
        }

        x = HashLen16(x, v.first, K2);
        y = HashLen16(y + z, w.first, K2);
        return { HashLen16(x + v.second, w.second, K2) + y, HashLen16(x + w.second, y + v.second, K2) };
    }

    GP_NODISCARD static constexpr Hash128Result
        CityMurmur(const char* data, SizeT length, UInt64 seedLow, UInt64 seedHigh) noexcept
    {
        UInt64 a = seedLow;
        UInt64 b = seedHigh;
        UInt64 c = 0;
        UInt64 d = 0;

        if (length <= 16)
        {
            a = ShiftMix(a * K1) * K1;
            c = b * K1 + HashLen0To16(data, length);
            d = ShiftMix(a + (length >= 8 ? Detail::HashUtilities::ReadU64LE(data, 0) : c));
        }
        else
        {
            c = HashLen16(Detail::HashUtilities::ReadU64LE(data, length - 8) + K1, a, K2);
            d = HashLen16(b + length, c + Detail::HashUtilities::ReadU64LE(data, length - 16), K2);
            a += d;
            SizeT offset = 0;
            do {
                a ^= ShiftMix(Detail::HashUtilities::ReadU64LE(data, offset) * K1) * K1;
                a *= K1;
                b ^= a;
                c ^= ShiftMix(Detail::HashUtilities::ReadU64LE(data, offset + 8) * K1) * K1;
                c *= K1;
                d ^= c;
                offset += 16;
            }
            while (offset < length - 16);
        }

        a = HashLen16(a, c, K2);
        b = HashLen16(d, b, K2);
        return { a ^ b, HashLen16(b, a, K2) };
    }
};

}   // namespace GP::Crypto

/// \brief User-defined literal for computing CityHash64 at compile time from string literals.
/// \param str String literal to hash.
/// \param len Length of the string literal (automatically provided).
/// \return Compile-time computed CityHash64 value.
inline constexpr GP::UInt64 operator""_cityhash(const char* str, GP::SizeT len) noexcept
{
    return GP::Crypto::CityHash::Hash64(str, len);
}
