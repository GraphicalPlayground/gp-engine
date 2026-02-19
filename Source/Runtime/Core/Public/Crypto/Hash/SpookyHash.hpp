// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief SpookyHash V2 - Bob Jenkins' fast non-cryptographic hash function.
/// Produces 64-bit and 128-bit hashes. Optimized for large inputs with a short-message fast path.
struct SpookyHash final : public Template::NonInstantiable
{
public:
    using HashType = UInt64;   //<! SpookyHash primary output is 64-bit.

public:
    static constexpr UInt64 DefaultSeed = 0ULL;   //<! Default seed for SpookyHash.

private:
    static constexpr SizeT SC_NUMVARS = 12;                     //<! Number of state variables.
    static constexpr SizeT SC_BLOCKSIZE = SC_NUMVARS * 8;       //<! Block size in bytes (96).
    static constexpr SizeT SC_BUFSIZE = 2 * SC_BLOCKSIZE;       //<! Buffer size (192).
    static constexpr UInt64 SC_CONST = 0xDEADBEEFDEADBEEFULL;   //<! SpookyHash magic constant.

    /// \brief SpookyHash mixing function for a single state variable.
    /// \param data Input data word.
    /// \param s0-s11 State variables passed by reference.
    static constexpr void
        Mix(const UInt64* data,
            UInt64& s0,
            UInt64& s1,
            UInt64& s2,
            UInt64& s3,
            UInt64& s4,
            UInt64& s5,
            UInt64& s6,
            UInt64& s7,
            UInt64& s8,
            UInt64& s9,
            UInt64& s10,
            UInt64& s11) noexcept
    {
        s0 += data[0];
        s2 ^= s10;
        s11 ^= s0;
        s0 = Detail::HashUtilities::Rotl64(s0, 11);
        s11 += s1;
        s1 += data[1];
        s3 ^= s11;
        s0 ^= s1;
        s1 = Detail::HashUtilities::Rotl64(s1, 32);
        s0 += s2;
        s2 += data[2];
        s4 ^= s0;
        s1 ^= s2;
        s2 = Detail::HashUtilities::Rotl64(s2, 43);
        s1 += s3;
        s3 += data[3];
        s5 ^= s1;
        s2 ^= s3;
        s3 = Detail::HashUtilities::Rotl64(s3, 31);
        s2 += s4;
        s4 += data[4];
        s6 ^= s2;
        s3 ^= s4;
        s4 = Detail::HashUtilities::Rotl64(s4, 17);
        s3 += s5;
        s5 += data[5];
        s7 ^= s3;
        s4 ^= s5;
        s5 = Detail::HashUtilities::Rotl64(s5, 28);
        s4 += s6;
        s6 += data[6];
        s8 ^= s4;
        s5 ^= s6;
        s6 = Detail::HashUtilities::Rotl64(s6, 39);
        s5 += s7;
        s7 += data[7];
        s9 ^= s5;
        s6 ^= s7;
        s7 = Detail::HashUtilities::Rotl64(s7, 57);
        s6 += s8;
        s8 += data[8];
        s10 ^= s6;
        s7 ^= s8;
        s8 = Detail::HashUtilities::Rotl64(s8, 55);
        s7 += s9;
        s9 += data[9];
        s11 ^= s7;
        s8 ^= s9;
        s9 = Detail::HashUtilities::Rotl64(s9, 54);
        s8 += s10;
        s10 += data[10];
        s0 ^= s8;
        s9 ^= s10;
        s10 = Detail::HashUtilities::Rotl64(s10, 22);
        s9 += s11;
        s11 += data[11];
        s1 ^= s9;
        s10 ^= s11;
        s11 = Detail::HashUtilities::Rotl64(s11, 46);
        s10 += s0;
    }

    /// \brief End-of-message mixing.
    static constexpr void EndPartial(
        UInt64& h0,
        UInt64& h1,
        UInt64& h2,
        UInt64& h3,
        UInt64& h4,
        UInt64& h5,
        UInt64& h6,
        UInt64& h7,
        UInt64& h8,
        UInt64& h9,
        UInt64& h10,
        UInt64& h11
    ) noexcept
    {
        h11 += h1;
        h2 ^= h11;
        h1 = Detail::HashUtilities::Rotl64(h1, 44);
        h0 += h2;
        h3 ^= h0;
        h2 = Detail::HashUtilities::Rotl64(h2, 15);
        h1 += h3;
        h4 ^= h1;
        h3 = Detail::HashUtilities::Rotl64(h3, 34);
        h2 += h4;
        h5 ^= h2;
        h4 = Detail::HashUtilities::Rotl64(h4, 21);
        h3 += h5;
        h6 ^= h3;
        h5 = Detail::HashUtilities::Rotl64(h5, 38);
        h4 += h6;
        h7 ^= h4;
        h6 = Detail::HashUtilities::Rotl64(h6, 33);
        h5 += h7;
        h8 ^= h5;
        h7 = Detail::HashUtilities::Rotl64(h7, 10);
        h6 += h8;
        h9 ^= h6;
        h8 = Detail::HashUtilities::Rotl64(h8, 13);
        h7 += h9;
        h10 ^= h7;
        h9 = Detail::HashUtilities::Rotl64(h9, 38);
        h8 += h10;
        h11 ^= h8;
        h10 = Detail::HashUtilities::Rotl64(h10, 53);
        h9 += h11;
        h0 ^= h9;
        h11 = Detail::HashUtilities::Rotl64(h11, 42);
        h10 += h0;
        h1 ^= h10;
        h0 = Detail::HashUtilities::Rotl64(h0, 54);
    }

    static constexpr void
        End(UInt64& h0,
            UInt64& h1,
            UInt64& h2,
            UInt64& h3,
            UInt64& h4,
            UInt64& h5,
            UInt64& h6,
            UInt64& h7,
            UInt64& h8,
            UInt64& h9,
            UInt64& h10,
            UInt64& h11) noexcept
    {
        EndPartial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
        EndPartial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
        EndPartial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
    }

    /// \brief Short message hash (< SC_BUFSIZE bytes).
    GP_NODISCARD static constexpr Hash128Result
        ShortHash(const char* data, SizeT length, UInt64 seedLow, UInt64 seedHigh) noexcept
    {
        UInt64 a = seedLow;
        UInt64 b = seedHigh;
        UInt64 c = SC_CONST;
        UInt64 d = SC_CONST;

        if (length > 15)
        {
            const SizeT nRounds = length / 32;
            for (SizeT i = 0; i < nRounds; ++i)
            {
                SizeT off = i * 32;
                c += Detail::HashUtilities::ReadU64LE(data, off);
                d += Detail::HashUtilities::ReadU64LE(data, off + 8);
                c = Detail::HashUtilities::Rotl64(c, 50);
                c += d;
                a ^= c;
                d = Detail::HashUtilities::Rotl64(d, 52);
                d += a;
                b ^= d;
                a = Detail::HashUtilities::Rotl64(a, 30);
                a += b;
                c ^= a;
                b = Detail::HashUtilities::Rotl64(b, 41);
                b += c;
                d ^= b;
                c = Detail::HashUtilities::Rotl64(c, 54);
                c += d;
                a ^= c;
                d = Detail::HashUtilities::Rotl64(d, 48);
                d += a;
                b ^= d;

                UInt64 e = Detail::HashUtilities::ReadU64LE(data, off + 16) + c;
                UInt64 f = Detail::HashUtilities::ReadU64LE(data, off + 24) + d;
                a += e;
                b += f;
                a = Detail::HashUtilities::Rotl64(a, 30) + b;
                b = Detail::HashUtilities::Rotl64(b, 41) + a;
            }

            const SizeT tailStart = nRounds * 32;
            d += static_cast<UInt64>(length) << 56;

            for (SizeT i = tailStart; i < length; ++i)
            {
                if ((i - tailStart) < 8)
                {
                    c += static_cast<UInt64>(static_cast<UInt8>(data[i])) << ((i - tailStart) * 8);
                }
                else { d += static_cast<UInt64>(static_cast<UInt8>(data[i])) << ((i - tailStart - 8) * 8); }
            }
        }
        else
        {
            d += static_cast<UInt64>(length) << 56;
            for (SizeT i = 0; i < length; ++i)
            {
                if (i < 8) { c += static_cast<UInt64>(static_cast<UInt8>(data[i])) << (i * 8); }
                else { d += static_cast<UInt64>(static_cast<UInt8>(data[i])) << ((i - 8) * 8); }
            }
        }

        c = Detail::HashUtilities::Rotl64(c, 50);
        c += d;
        a ^= c;
        d = Detail::HashUtilities::Rotl64(d, 52);
        d += a;
        b ^= d;
        a = Detail::HashUtilities::Rotl64(a, 30);
        a += b;
        c ^= a;
        b = Detail::HashUtilities::Rotl64(b, 41);
        b += c;
        d ^= b;
        c = Detail::HashUtilities::Rotl64(c, 54);
        c += d;
        a ^= c;
        d = Detail::HashUtilities::Rotl64(d, 48);
        d += a;
        b ^= d;
        a = Detail::HashUtilities::Rotl64(a, 38);
        a += b;
        c ^= a;
        b = Detail::HashUtilities::Rotl64(b, 37);
        b += c;
        d ^= b;
        c = Detail::HashUtilities::Rotl64(c, 62);
        c += d;
        a ^= c;
        d = Detail::HashUtilities::Rotl64(d, 34);
        d += a;
        b ^= d;
        a = Detail::HashUtilities::Rotl64(a, 5);
        a += b;
        c ^= a;
        b = Detail::HashUtilities::Rotl64(b, 36);
        b += c;
        d ^= b;

        return { a, b };
    }

public:
    /// \brief Compute 128-bit SpookyHash V2 for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed1 First 64-bit seed.
    /// \param seed2 Second 64-bit seed.
    /// \return 128-bit hash result (low/high).
    GP_NODISCARD static constexpr Hash128Result
        Hash128(const char* data, SizeT length, UInt64 seed1 = DefaultSeed, UInt64 seed2 = DefaultSeed) noexcept
    {
        return ShortHash(data, length, seed1, seed2);
    }

    /// \brief Compute 128-bit SpookyHash V2 from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed1 First 64-bit seed.
    /// \param seed2 Second 64-bit seed.
    /// \return 128-bit hash result.
    GP_NODISCARD static constexpr Hash128Result
        Hash128(const void* data, SizeT length, UInt64 seed1 = DefaultSeed, UInt64 seed2 = DefaultSeed) noexcept
    {
        return Hash128(static_cast<const char*>(data), length, seed1, seed2);
    }

    /// \brief Compute 128-bit SpookyHash V2 for a null-terminated string.
    /// \param str Null-terminated input string.
    /// \return 128-bit hash result.
    GP_NODISCARD static constexpr Hash128Result Hash128(const char* str) noexcept
    {
        return Hash128(str, std::char_traits<char>::length(str), DefaultSeed, DefaultSeed);
    }

    /// \brief Compute 128-bit SpookyHash V2 from a string view.
    /// \param view String view to hash.
    /// \param seed1 First 64-bit seed.
    /// \param seed2 Second 64-bit seed.
    /// \return 128-bit hash result.
    GP_NODISCARD static constexpr Hash128Result
        Hash128(std::string_view view, UInt64 seed1 = DefaultSeed, UInt64 seed2 = DefaultSeed) noexcept
    {
        return Hash128(view.data(), view.size(), seed1, seed2);
    }

    /// \brief Compute 64-bit SpookyHash V2 for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional 64-bit seed.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* data, SizeT length, UInt64 seed = DefaultSeed) noexcept
    {
        return Hash128(data, length, seed, seed).low;
    }

    /// \brief Compute 64-bit SpookyHash V2 from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param seed Optional 64-bit seed.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const void* data, SizeT length, UInt64 seed = DefaultSeed) noexcept
    {
        return Hash64(static_cast<const char*>(data), length, seed);
    }

    /// \brief Compute 64-bit SpookyHash V2 for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(const char* str) noexcept
    {
        return Hash64(str, std::char_traits<char>::length(str), DefaultSeed);
    }

    /// \brief Compute 64-bit SpookyHash V2 from a string view.
    /// \param view String view to hash.
    /// \param seed Optional 64-bit seed.
    /// \return 64-bit hash value.
    GP_NODISCARD static constexpr UInt64 Hash64(std::string_view view, UInt64 seed = DefaultSeed) noexcept
    {
        return Hash64(view.data(), view.size(), seed);
    }

    /// \brief Convenience entry point.
    /// \param data Pointer to bytes.
    /// \param length Number of bytes.
    /// \return 64-bit hash value.
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

/// \brief User-defined literal for computing SpookyHash at compile time from string literals.
/// \param str String literal to hash.
/// \param len Length of the string literal (automatically provided).
/// \return Compile-time computed SpookyHash value.
inline constexpr GP::UInt64 operator""_spooky(const char* str, GP::SizeT len) noexcept
{
    return GP::Crypto::SpookyHash::Hash64(str, len);
}
