// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief MD5 (Message-Digest Algorithm 5) implementation per RFC 1321.
/// Produces a 128-bit (16-byte) message digest.
/// \warning MD5 is cryptographically broken. Use SHA-256 or BLAKE2b for new applications.
struct MD5 final : public Template::NonInstantiable
{
public:
    static constexpr SizeT BLOCK_SIZE = 64;    //<! MD5 processes 64-byte blocks.
    static constexpr SizeT DIGEST_SIZE = 16;   //<! MD5 produces a 16-byte digest.

private:
    /// \brief Initial hash values (RFC 1321 Section 3.3).
    static constexpr UInt32 H0 = 0x67452301U;
    static constexpr UInt32 H1 = 0xEFCDAB89U;
    static constexpr UInt32 H2 = 0x98BADCFEU;
    static constexpr UInt32 H3 = 0x10325476U;

    /// \brief Auxiliary function F(X, Y, Z) = (X & Y) | (~X & Z).
    GP_NODISCARD static constexpr UInt32 F(UInt32 x, UInt32 y, UInt32 z) noexcept { return (x & y) | (~x & z); }

    /// \brief Auxiliary function G(X, Y, Z) = (X & Z) | (Y & ~Z).
    GP_NODISCARD static constexpr UInt32 G(UInt32 x, UInt32 y, UInt32 z) noexcept { return (x & z) | (y & ~z); }

    /// \brief Auxiliary function H(X, Y, Z) = X ^ Y ^ Z.
    GP_NODISCARD static constexpr UInt32 H(UInt32 x, UInt32 y, UInt32 z) noexcept { return x ^ y ^ z; }

    /// \brief Auxiliary function I(X, Y, Z) = Y ^ (X | ~Z).
    GP_NODISCARD static constexpr UInt32 I(UInt32 x, UInt32 y, UInt32 z) noexcept { return y ^ (x | ~z); }

    /// \brief Per-round left-rotate amounts (RFC 1321 Section 3.4).
    static constexpr Int32 S[64] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                                     5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                                     4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                                     6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };

    /// \brief Precomputed sine constants T[i] = floor(2^32 * abs(sin(i + 1))) (RFC 1321 Section 3.4).
    static constexpr UInt32 T[64] = {
        0xD76AA478U, 0xE8C7B756U, 0x242070DBU, 0xC1BDCEEEU, 0xF57C0FAFU, 0x4787C62AU, 0xA8304613U, 0xFD469501U,
        0x698098D8U, 0x8B44F7AFU, 0xFFFF5BB1U, 0x895CD7BEU, 0x6B901122U, 0xFD987193U, 0xA679438EU, 0x49B40821U,
        0xF61E2562U, 0xC040B340U, 0x265E5A51U, 0xE9B6C7AAU, 0xD62F105DU, 0x02441453U, 0xD8A1E681U, 0xE7D3FBC8U,
        0x21E1CDE6U, 0xC33707D6U, 0xF4D50D87U, 0x455A14EDU, 0xA9E3E905U, 0xFCEFA3F8U, 0x676F02D9U, 0x8D2A4C8AU,
        0xFFFA3942U, 0x8771F681U, 0x6D9D6122U, 0xFDE5380CU, 0xA4BEEA44U, 0x4BDECFA9U, 0xF6BB4B60U, 0xBEBFBC70U,
        0x289B7EC6U, 0xEAA127FAU, 0xD4EF3085U, 0x04881D05U, 0xD9D4D039U, 0xE6DB99E5U, 0x1FA27CF8U, 0xC4AC5665U,
        0xF4292244U, 0x432AFF97U, 0xAB9423A7U, 0xFC93A039U, 0x655B59C3U, 0x8F0CCC92U, 0xFFEFF47DU, 0x85845DD1U,
        0x6FA87E4FU, 0xFE2CE6E0U, 0xA3014314U, 0x4E0811A1U, 0xF7537E82U, 0xBD3AF235U, 0x2AD7D2BBU, 0xEB86D391U
    };

    /// \brief Process a single 64-byte block and update state.
    /// \param block Pointer to the 64-byte block.
    /// \param state Pointer to the 4-element state array.
    static constexpr void ProcessBlock(const char* block, UInt32* state) noexcept
    {
        UInt32 m[16]{};
        for (SizeT i = 0; i < 16; ++i) { m[i] = Detail::HashUtilities::ReadU32LE(block, i * 4); }

        UInt32 a = state[0];
        UInt32 b = state[1];
        UInt32 c = state[2];
        UInt32 d = state[3];

        for (SizeT i = 0; i < 64; ++i)
        {
            UInt32 f;
            SizeT g;

            if (i < 16)
            {
                f = F(b, c, d);
                g = i;
            }
            else if (i < 32)
            {
                f = G(b, c, d);
                g = (5 * i + 1) % 16;
            }
            else if (i < 48)
            {
                f = H(b, c, d);
                g = (3 * i + 5) % 16;
            }
            else
            {
                f = I(b, c, d);
                g = (7 * i) % 16;
            }

            UInt32 temp = d;
            d = c;
            c = b;
            b = b + Detail::HashUtilities::Rotl32(a + f + T[i] + m[g], S[i]);
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }

public:
    /// \brief Compute MD5 digest for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return MD5 digest (16 bytes).
    GP_NODISCARD static constexpr MD5Digest Hash(const char* data, SizeT length) noexcept
    {
        UInt32 state[4] = { H0, H1, H2, H3 };

        SizeT offset = 0;
        while (offset + BLOCK_SIZE <= length)
        {
            ProcessBlock(data + offset, state);
            offset += BLOCK_SIZE;
        }

        char padded[128]{};
        SizeT remaining = length - offset;
        for (SizeT i = 0; i < remaining; ++i) { padded[i] = data[offset + i]; }
        padded[remaining] = static_cast<char>(0x80);

        SizeT padLen = (remaining < 56) ? 64 : 128;
        UInt64 bitLength = static_cast<UInt64>(length) * 8;
        Detail::HashUtilities::WriteU64LE(padded, padLen - 8, bitLength);

        ProcessBlock(padded, state);
        if (padLen == 128) { ProcessBlock(padded + 64, state); }

        MD5Digest result{};
        for (SizeT i = 0; i < 4; ++i)
        {
            result.bytes[i * 4] = static_cast<Byte>(state[i] & 0xFF);
            result.bytes[i * 4 + 1] = static_cast<Byte>((state[i] >> 8) & 0xFF);
            result.bytes[i * 4 + 2] = static_cast<Byte>((state[i] >> 16) & 0xFF);
            result.bytes[i * 4 + 3] = static_cast<Byte>((state[i] >> 24) & 0xFF);
        }
        return result;
    }

    /// \brief Compute MD5 digest from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return MD5 digest.
    GP_NODISCARD static constexpr MD5Digest Hash(const void* data, SizeT length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Compute MD5 digest for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return MD5 digest.
    GP_NODISCARD static constexpr MD5Digest Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute MD5 digest from a string view.
    /// \param view String view to hash.
    /// \return MD5 digest.
    GP_NODISCARD static constexpr MD5Digest Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }

    /// \brief Verify data against an expected MD5 digest.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected digest value.
    /// \return True if computed digest matches expected.
    GP_NODISCARD static constexpr bool Verify(const char* data, SizeT length, const MD5Digest& expected) noexcept
    {
        return Hash(data, length) == expected;
    }

    /// \brief Verify data from a void pointer against an expected digest.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected digest value.
    /// \return True if computed digest matches expected.
    GP_NODISCARD static constexpr bool Verify(const void* data, SizeT length, const MD5Digest& expected) noexcept
    {
        return Hash(data, length) == expected;
    }

    /// \brief Streaming MD5 context for incremental hashing.
    struct Context
    {
    public:
        UInt32 state[4] = { H0, H1, H2, H3 };
        char buffer[BLOCK_SIZE]{};
        SizeT bufferLen = 0;
        UInt64 totalLen = 0;

    public:
        /// \brief Feed data into the hash context.
        /// \param data Pointer to the input bytes.
        /// \param length Number of bytes to process.
        constexpr void Update(const char* data, SizeT length) noexcept
        {
            totalLen += length;
            SizeT offset = 0;

            if (bufferLen > 0)
            {
                SizeT fill = BLOCK_SIZE - bufferLen;
                if (length < fill)
                {
                    for (SizeT i = 0; i < length; ++i) { buffer[bufferLen + i] = data[i]; }
                    bufferLen += length;
                    return;
                }
                for (SizeT i = 0; i < fill; ++i) { buffer[bufferLen + i] = data[i]; }
                ProcessBlock(buffer, state);
                bufferLen = 0;
                offset = fill;
            }

            while (offset + BLOCK_SIZE <= length)
            {
                ProcessBlock(data + offset, state);
                offset += BLOCK_SIZE;
            }

            SizeT remaining = length - offset;
            for (SizeT i = 0; i < remaining; ++i) { buffer[i] = data[offset + i]; }
            bufferLen = remaining;
        }

        /// \brief Finalize the hash and return the digest.
        /// \return MD5 digest.
        GP_NODISCARD constexpr MD5Digest Finalize() noexcept
        {
            char padded[128]{};
            for (SizeT i = 0; i < bufferLen; ++i) { padded[i] = buffer[i]; }
            padded[bufferLen] = static_cast<char>(0x80);

            SizeT padLen = (bufferLen < 56) ? 64 : 128;
            UInt64 bitLength = totalLen * 8;
            Detail::HashUtilities::WriteU64LE(padded, padLen - 8, bitLength);

            ProcessBlock(padded, state);
            if (padLen == 128) { ProcessBlock(padded + 64, state); }

            MD5Digest result{};
            for (SizeT i = 0; i < 4; ++i)
            {
                Detail::HashUtilities::WriteU32LE(reinterpret_cast<char*>(result.bytes), i * 4, state[i]);
            }
            return result;
        }
    };
};

}   // namespace GP::Crypto
