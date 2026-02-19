// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief WyHash - one of the fastest non-cryptographic hash functions for general-purpose use.
/// Based on wyhash v4.2 by Wang Yi. Excellent distribution and speed on modern CPUs.
struct WyHash final : public Template::NonInstantiable
{
public:
    using HashType = UInt64;   //<! WyHash produces 64-bit output.

public:
    static constexpr UInt64 DefaultSeed = 0ULL;   //<! Default seed for WyHash.

private:
    static constexpr UInt64 Secret0 = 0xA0761D6478BD642FULL;   //<! WyHash secret constant 0.
    static constexpr UInt64 Secret1 = 0xE7037ED1A0B428DBULL;   //<! WyHash secret constant 1.
    static constexpr UInt64 Secret2 = 0x8EBC6AF09C88C6E3ULL;   //<! WyHash secret constant 2.
    static constexpr UInt64 Secret3 = 0x589965CC75374CC3ULL;   //<! WyHash secret constant 3.

    /// \brief WyHash mixing function using 128-bit multiplication.
    /// \param a First operand.
    /// \param b Second operand.
    /// \return Mixed 64-bit result.
    GP_NODISCARD static constexpr UInt64 WyMix(UInt64 a, UInt64 b) noexcept
    {
        UInt64 lo, hi;
        Detail::HashUtilities::Multiply128(a, b, lo, hi);
        return lo ^ hi;
    }

    /// \brief Read exactly 8 bytes from a buffer.
    /// \param data Pointer to the byte buffer.
    /// \return 64-bit value read in little-endian order.
    GP_NODISCARD static constexpr UInt64 Read8(const char* data) noexcept
    {
        return Detail::HashUtilities::ReadU64LE(data, 0);
    }

    /// \brief Read exactly 4 bytes from a buffer.
    /// \param data Pointer to the byte buffer.
    /// \return 32-bit value read in little-endian order, widened to 64-bit.
    GP_NODISCARD static constexpr UInt64 Read4(const char* data) noexcept
    {
        return static_cast<UInt64>(Detail::HashUtilities::ReadU32LE(data, 0));
    }

    /// \brief Read 1-3 bytes from a buffer into a 64-bit value.
    /// \param data Pointer to the byte buffer.
    /// \param length Number of bytes (1, 2, or 3).
    /// \return 64-bit value packed from the bytes.
    GP_NODISCARD static constexpr UInt64 Read3(const char* data, SizeT length) noexcept
    {
        return (static_cast<UInt64>(static_cast<UInt8>(data[0])) << 16) |
               (static_cast<UInt64>(static_cast<UInt8>(data[length >> 1])) << 8) |
               static_cast<UInt64>(static_cast<UInt8>(data[length - 1]));
    }

public:
    /// \brief Compute 64-bit WyHash for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional seed value.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* data, SizeT length, UInt64 seed = DefaultSeed) noexcept
    {
        seed ^= WyMix(seed ^ Secret0, Secret1);
        UInt64 a, b;

        if (length <= 16)
        {
            if (length >= 4)
            {
                a = (Read4(data) << 32) | Read4(data + ((length >> 3) << 2));
                b = (Read4(data + length - 4) << 32) | Read4(data + length - 4 - ((length >> 3) << 2));
            }
            else if (length > 0)
            {
                a = Read3(data, length);
                b = 0;
            }
            else
            {
                a = 0;
                b = 0;
            }
        }
        else if (length <= 48)
        {
            seed = WyMix(Read8(data) ^ Secret1, Read8(data + 8) ^ seed);
            if (length > 32) { seed = WyMix(Read8(data + 16) ^ Secret2, Read8(data + 24) ^ seed); }
            a = Read8(data + length - 16);
            b = Read8(data + length - 8);
        }
        else
        {
            SizeT remaining = length;
            SizeT offset = 0;
            UInt64 see1 = seed;
            UInt64 see2 = seed;

            // Consume all 48 bytes of each block via three independent mixing lanes.
            // Using see1/see2 alongside seed mirrors the reference wyhash v4 algorithm and
            // ensures bytes at block offsets +32/+40 (Secret3 lane) are not silently skipped.
            do {
                seed = WyMix(Read8(data + offset) ^ Secret1, Read8(data + offset + 8) ^ seed);
                see1 = WyMix(Read8(data + offset + 16) ^ Secret2, Read8(data + offset + 24) ^ see1);
                see2 = WyMix(Read8(data + offset + 32) ^ Secret3, Read8(data + offset + 40) ^ see2);
                offset += 48;
                remaining -= 48;
            }
            while (remaining > 48);

            seed ^= see1 ^ see2;

            const char* tail = data + length - 48;
            seed = WyMix(Read8(tail) ^ Secret1, Read8(tail + 8) ^ seed);
            seed = WyMix(Read8(tail + 16) ^ Secret2, Read8(tail + 24) ^ seed);
            a = Read8(tail + 32);
            b = Read8(tail + 40);
        }

        a ^= Secret1;
        b ^= seed;
        UInt64 lo, hi;
        Detail::HashUtilities::Multiply128(a, b, lo, hi);
        return WyMix(lo ^ Secret0 ^ static_cast<UInt64>(length), hi ^ Secret1);
    }

    /// \brief Compute 64-bit WyHash from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional seed value.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const void* data, SizeT length, UInt64 seed = DefaultSeed) noexcept
    {
        return Hash64(static_cast<const char*>(data), length, seed);
    }

    /// \brief Compute 64-bit WyHash for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* str) noexcept
    {
        return Hash64(str, std::char_traits<char>::length(str), DefaultSeed);
    }

    /// \brief Compute 64-bit WyHash from a string view.
    /// \param view String view to hash.
    /// \param seed Optional seed value.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(std::string_view view, UInt64 seed = DefaultSeed) noexcept
    {
        return Hash64(view.data(), view.size(), seed);
    }

    /// \brief Convenience architecture-sized hash entry point.
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return Architecture-sized hash (always 64-bit for WyHash).
    GP_NODISCARD static constexpr HashType Hash(const char* data, SizeT length) noexcept
    {
        return Hash64(data, length, DefaultSeed);
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
};

}   // namespace GP::Crypto

/// \brief User-defined literal for computing WyHash at compile time from string literals.
/// \param str String literal to hash.
/// \param len Length of the string literal (automatically provided).
/// \return Compile-time computed WyHash value.
inline constexpr GP::UInt64 operator""_wyhash(const char* str, GP::SizeT len) noexcept
{
    return GP::Crypto::WyHash::Hash64(str, len);
}
