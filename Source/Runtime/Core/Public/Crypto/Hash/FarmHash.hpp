// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief FarmHash - Google's successor to CityHash with improved avalanche properties.
/// Based on FarmHash by Geoff Pike. Provides 32-bit, 64-bit, and 128-bit fingerprint variants.
struct FarmHash final : public Template::NonInstantiable
{
public:
#if GP_ARCHITECTURE_64BIT
    using HashType = UInt64;   //<! 64-bit hash type on 64-bit architectures.
#else
    using HashType = UInt32;   //<! 32-bit hash type on 32-bit architectures.
#endif

private:
    static constexpr UInt64 K0 = 0xC3A5C85C97CB3127ULL;
    static constexpr UInt64 K1 = 0xB492B66FBE98F273ULL;
    static constexpr UInt64 K2 = 0x9AE16A3B2F90404FULL;
    static constexpr UInt32 C1 = 0xCC9E2D51U;
    static constexpr UInt32 C2 = 0x1B873593U;

    GP_NODISCARD static constexpr UInt64 ShiftMix(UInt64 val) noexcept { return val ^ (val >> 47); }

    GP_NODISCARD static constexpr UInt32 FMix(UInt32 h) noexcept
    {
        h ^= h >> 16;
        h *= 0x85EBCA6BU;
        h ^= h >> 13;
        h *= 0xC2B2AE35U;
        h ^= h >> 16;
        return h;
    }

    GP_NODISCARD static constexpr UInt32 Mur(UInt32 a, UInt32 h) noexcept
    {
        a *= C1;
        a = Detail::HashUtilities::Rotr32(a, 17);
        a *= C2;
        h ^= a;
        h = Detail::HashUtilities::Rotr32(h, 19);
        return h * 5 + 0xE6546B64U;
    }

    GP_NODISCARD static constexpr UInt64 HashLen16(UInt64 u, UInt64 v, UInt64 mul) noexcept
    {
        UInt64 a = (u ^ v) * mul;
        a ^= (a >> 47);
        UInt64 b = (v ^ a) * mul;
        b ^= (b >> 47);
        b *= mul;
        return b;
    }

    GP_NODISCARD static constexpr UInt32 Hash32Len0To4(const char* data, SizeT length) noexcept
    {
        UInt32 b = 0;
        UInt32 c = 9;
        for (SizeT i = 0; i < length; ++i)
        {
            UInt8 v = static_cast<UInt8>(data[i]);
            b = b * C1 + v;
            c ^= b;
        }
        return FMix(Mur(b, Mur(static_cast<UInt32>(length), c)));
    }

    GP_NODISCARD static constexpr UInt32 Hash32Len5To12(const char* data, SizeT length) noexcept
    {
        UInt32 a = static_cast<UInt32>(length);
        UInt32 b = static_cast<UInt32>(length) * 5;
        UInt32 c = 9;
        UInt32 d = b;
        a += Detail::HashUtilities::ReadU32LE(data, 0);
        b += Detail::HashUtilities::ReadU32LE(data, length - 4);
        c += Detail::HashUtilities::ReadU32LE(data, (length >> 1) & 4);
        return FMix(Mur(c, Mur(b, Mur(a, d))));
    }

    GP_NODISCARD static constexpr UInt32 Hash32Len13To24(const char* data, SizeT length) noexcept
    {
        UInt32 a = Detail::HashUtilities::ReadU32LE(data, (length >> 1) - 4);
        UInt32 b = Detail::HashUtilities::ReadU32LE(data, 4);
        UInt32 c = Detail::HashUtilities::ReadU32LE(data, length - 8);
        UInt32 d = Detail::HashUtilities::ReadU32LE(data, length >> 1);
        UInt32 e = Detail::HashUtilities::ReadU32LE(data, 0);
        UInt32 f = Detail::HashUtilities::ReadU32LE(data, length - 4);
        UInt32 h = static_cast<UInt32>(length);
        return FMix(Mur(f, Mur(e, Mur(d, Mur(c, Mur(b, Mur(a, h)))))));
    }

public:
    /// \brief Compute 32-bit FarmHash fingerprint for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const char* data, SizeT length) noexcept
    {
        if (length <= 4) { return Hash32Len0To4(data, length); }
        if (length <= 12) { return Hash32Len5To12(data, length); }
        if (length <= 24) { return Hash32Len13To24(data, length); }

        UInt32 h = static_cast<UInt32>(length);
        UInt32 g = C1 * h;
        UInt32 f = g;
        UInt32 a0 = Detail::HashUtilities::Rotr32(Detail::HashUtilities::ReadU32LE(data, length - 4) * C1, 17) * C2;
        UInt32 a1 = Detail::HashUtilities::Rotr32(Detail::HashUtilities::ReadU32LE(data, length - 8) * C1, 17) * C2;
        UInt32 a2 = Detail::HashUtilities::Rotr32(Detail::HashUtilities::ReadU32LE(data, length - 16) * C1, 17) * C2;
        UInt32 a3 = Detail::HashUtilities::Rotr32(Detail::HashUtilities::ReadU32LE(data, length - 12) * C1, 17) * C2;
        UInt32 a4 = Detail::HashUtilities::Rotr32(Detail::HashUtilities::ReadU32LE(data, length - 20) * C1, 17) * C2;
        h ^= a0;
        h = Detail::HashUtilities::Rotr32(h, 19);
        h = h * 5 + 0xE6546B64U;
        h ^= a2;
        h = Detail::HashUtilities::Rotr32(h, 19);
        h = h * 5 + 0xE6546B64U;
        g ^= a1;
        g = Detail::HashUtilities::Rotr32(g, 19);
        g = g * 5 + 0xE6546B64U;
        g ^= a3;
        g = Detail::HashUtilities::Rotr32(g, 19);
        g = g * 5 + 0xE6546B64U;
        f += a4;
        f = Detail::HashUtilities::Rotr32(f, 19);
        f = f * 5 + 0xE6546B64U;

        SizeT offset = 0;
        const SizeT iters = ((length - 1) / 20);
        for (SizeT i = 0; i < iters; ++i)
        {
            UInt32 a = Detail::HashUtilities::ReadU32LE(data, offset);
            UInt32 b = Detail::HashUtilities::ReadU32LE(data, offset + 4);
            UInt32 c = Detail::HashUtilities::ReadU32LE(data, offset + 8);
            UInt32 d = Detail::HashUtilities::ReadU32LE(data, offset + 12);
            UInt32 e = Detail::HashUtilities::ReadU32LE(data, offset + 16);
            h += a;
            g += b;
            f += c;
            h = Mur(d, h) + e;
            g = Mur(c, g) + a;
            f = Mur(b + e * C1, f) + d;
            f += g;
            g += f;
            offset += 20;
        }

        g = Detail::HashUtilities::Rotr32(g, 11) * C1;
        g = Detail::HashUtilities::Rotr32(g, 17) * C1;
        f = Detail::HashUtilities::Rotr32(f, 11) * C1;
        f = Detail::HashUtilities::Rotr32(f, 17) * C1;
        h = Detail::HashUtilities::Rotr32(h + g, 19);
        h = h * 5 + 0xE6546B64U;
        h = Detail::HashUtilities::Rotr32(h, 17) * C1;
        h = Detail::HashUtilities::Rotr32(h + f, 19);
        h = h * 5 + 0xE6546B64U;
        h = Detail::HashUtilities::Rotr32(h, 17) * C1;
        return h;
    }

    /// \brief Compute 32-bit FarmHash from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const void* data, SizeT length) noexcept
    {
        return Hash32(static_cast<const char*>(data), length);
    }

    /// \brief Compute 32-bit FarmHash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(const char* str) noexcept
    {
        return Hash32(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute 32-bit FarmHash from a string view.
    /// \param view String view to hash.
    /// \return 32-bit hash value.
    GP_NODISCARD static constexpr UInt32 Hash32(std::string_view view) noexcept
    {
        return Hash32(view.data(), view.size());
    }

    /// \brief Compute 64-bit FarmHash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* data, SizeT length) noexcept
    {
        if (length <= 16)
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
        if (length <= 32)
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
        if (length <= 64)
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

        UInt64 x = Detail::HashUtilities::ReadU64LE(data, length - 40);
        UInt64 y =
            Detail::HashUtilities::ReadU64LE(data, length - 16) + Detail::HashUtilities::ReadU64LE(data, length - 56);
        UInt64 z = HashLen16(
            Detail::HashUtilities::ReadU64LE(data, length - 48) + length,
            Detail::HashUtilities::ReadU64LE(data, length - 24),
            K2
        );

        UInt64 vFirst, vSecond, wFirst, wSecond;
        {
            UInt64 a = length;
            UInt64 b = z;
            auto p = WeakHash32(data, length - 64, a, b);
            vFirst = p.first;
            vSecond = p.second;
        }
        {
            UInt64 a = y + K1;
            UInt64 b = x;
            auto p = WeakHash32(data, length - 32, a, b);
            wFirst = p.first;
            wSecond = p.second;
        }
        x = x * K1 + Detail::HashUtilities::ReadU64LE(data, 0);

        SizeT offset = 0;
        const SizeT endOffset = ((length - 1) / 64) * 64;

        do {
            x = Detail::HashUtilities::Rotl64(x + y + vFirst + Detail::HashUtilities::ReadU64LE(data, offset + 8), 37) *
                K1;
            y = Detail::HashUtilities::Rotl64(y + vSecond + Detail::HashUtilities::ReadU64LE(data, offset + 48), 42) *
                K1;
            x ^= wSecond;
            y += vFirst + Detail::HashUtilities::ReadU64LE(data, offset + 40);
            z = Detail::HashUtilities::Rotl64(z + wFirst, 33) * K1;
            {
                auto p = WeakHash32(data, offset, vSecond * K1, x + wFirst);
                vFirst = p.first;
                vSecond = p.second;
            }
            {
                auto p =
                    WeakHash32(data, offset + 32, z + wSecond, y + Detail::HashUtilities::ReadU64LE(data, offset + 16));
                wFirst = p.first;
                wSecond = p.second;
            }
            UInt64 tmp = z;
            z = x;
            x = tmp;
            offset += 64;
        }
        while (offset < endOffset);

        UInt64 mul = K1 + ((z & 0xFF) << 1);
        offset = endOffset;
        wFirst += (static_cast<UInt64>(length) - 1) & 63;
        vFirst += wFirst;
        wFirst += vFirst;

        x = Detail::HashUtilities::Rotl64(x + y + vFirst + Detail::HashUtilities::ReadU64LE(data, offset + 8), 37) *
            mul;
        y = Detail::HashUtilities::Rotl64(y + vSecond + Detail::HashUtilities::ReadU64LE(data, offset + 48), 42) * mul;
        x ^= wSecond * 9;
        y += vFirst * 9 + Detail::HashUtilities::ReadU64LE(data, offset + 40);
        z = Detail::HashUtilities::Rotl64(z + wFirst, 33) * mul;
        {
            auto p = WeakHash32(data, offset, vSecond * mul, x + wFirst);
            vFirst = p.first;
            vSecond = p.second;
        }
        {
            auto p =
                WeakHash32(data, offset + 32, z + wSecond, y + Detail::HashUtilities::ReadU64LE(data, offset + 16));
            wFirst = p.first;
            wSecond = p.second;
        }
        {
            UInt64 tmp = z;
            z = x;
            x = tmp;
        }
        return HashLen16(
            HashLen16(vFirst, wFirst, mul) + ShiftMix(y) * K0 + z, HashLen16(vSecond, wSecond, mul) + x, mul
        );
    }

    /// \brief Compute 64-bit FarmHash from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const void* data, SizeT length) noexcept
    {
        return Hash64(static_cast<const char*>(data), length);
    }

    /// \brief Compute 64-bit FarmHash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* str) noexcept
    {
        return Hash64(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute 64-bit FarmHash from a string view.
    /// \param view String view to hash.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(std::string_view view) noexcept
    {
        return Hash64(view.data(), view.size());
    }

    /// \brief Convenience architecture-sized hash entry point.
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return Architecture-sized hash value.
    GP_NODISCARD static constexpr HashType Hash(const char* data, SizeT length) noexcept
    {
#if GP_ARCHITECTURE_64BIT
        return Hash64(data, length);
#else
        return Hash32(data, length);
#endif
    }

    /// \brief Convenience overload for void pointer input.
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return Architecture-sized hash value.
    GP_NODISCARD static constexpr HashType Hash(const void* data, SizeT length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Convenience overload for null-terminated strings.
    /// \param str Null-terminated input string.
    /// \return Architecture-sized hash value.
    GP_NODISCARD static constexpr HashType Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Convenience overload for string views.
    /// \param view String view to hash.
    /// \return Architecture-sized hash value.
    GP_NODISCARD static constexpr HashType Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }

private:
    struct Pair128
    {
        UInt64 first;
        UInt64 second;
    };

    GP_NODISCARD static constexpr Pair128 WeakHash32(const char* data, SizeT offset, UInt64 a, UInt64 b) noexcept
    {
        UInt64 w = Detail::HashUtilities::ReadU64LE(data, offset);
        UInt64 x = Detail::HashUtilities::ReadU64LE(data, offset + 8);
        UInt64 y = Detail::HashUtilities::ReadU64LE(data, offset + 16);
        UInt64 z = Detail::HashUtilities::ReadU64LE(data, offset + 24);
        a += w;
        b = Detail::HashUtilities::Rotl64(b + a + z, 21);
        UInt64 c = a;
        a += x;
        a += y;
        b += Detail::HashUtilities::Rotl64(a, 44);
        return { a + z, b + c };
    }
};

}   // namespace GP::Crypto

/// \brief User-defined literal for computing FarmHash at compile time from string literals.
/// \param str String literal to hash.
/// \param len Length of the string literal (automatically provided).
/// \return Compile-time computed FarmHash value (architecture-sized).
inline constexpr GP::Crypto::FarmHash::HashType operator""_farmhash(const char* str, GP::SizeT len) noexcept
{
    return GP::Crypto::FarmHash::Hash(str, len);
}
