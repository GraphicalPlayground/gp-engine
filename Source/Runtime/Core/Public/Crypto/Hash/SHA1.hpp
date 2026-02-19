// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief SHA-1 (Secure Hash Algorithm 1) implementation.
/// Produces a 160-bit (20-byte) message digest. Provided for legacy compatibility only.
/// \warning SHA-1 is cryptographically broken. Use SHA-256 or BLAKE2b for new applications.
struct GP_DEPRECATED("SHA-1 is cryptographically broken. Use SHA256 or BLAKE2b.") SHA1 final
    : public Template::NonInstantiable
{
public:
    static constexpr SizeT BLOCK_SIZE = 64;    //<! SHA-1 processes 64-byte blocks.
    static constexpr SizeT DIGEST_SIZE = 20;   //<! SHA-1 produces a 20-byte digest.

private:
    static constexpr UInt32 H0 = 0x67452301U;
    static constexpr UInt32 H1 = 0xEFCDAB89U;
    static constexpr UInt32 H2 = 0x98BADCFEU;
    static constexpr UInt32 H3 = 0x10325476U;
    static constexpr UInt32 H4 = 0xC3D2E1F0U;

    GP_NODISCARD static constexpr UInt32 Ch(UInt32 x, UInt32 y, UInt32 z) noexcept { return (x & y) ^ (~x & z); }

    GP_NODISCARD static constexpr UInt32 Parity(UInt32 x, UInt32 y, UInt32 z) noexcept { return x ^ y ^ z; }

    GP_NODISCARD static constexpr UInt32 Maj(UInt32 x, UInt32 y, UInt32 z) noexcept
    {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    static constexpr void ProcessBlock(const char* block, UInt32* state) noexcept
    {
        UInt32 w[80]{};
        for (SizeT i = 0; i < 16; ++i) { w[i] = Detail::HashUtilities::ReadU32BE(block, i * 4); }
        for (SizeT i = 16; i < 80; ++i)
        {
            w[i] = Detail::HashUtilities::Rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        UInt32 a = state[0];
        UInt32 b = state[1];
        UInt32 c = state[2];
        UInt32 d = state[3];
        UInt32 e = state[4];

        for (SizeT i = 0; i < 80; ++i)
        {
            UInt32 f, k;
            if (i < 20)
            {
                f = Ch(b, c, d);
                k = 0x5A827999U;
            }
            else if (i < 40)
            {
                f = Parity(b, c, d);
                k = 0x6ED9EBA1U;
            }
            else if (i < 60)
            {
                f = Maj(b, c, d);
                k = 0x8F1BBCDCU;
            }
            else
            {
                f = Parity(b, c, d);
                k = 0xCA62C1D6U;
            }

            UInt32 temp = Detail::HashUtilities::Rotl32(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = Detail::HashUtilities::Rotl32(b, 30);
            b = a;
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }

public:
    /// \brief Compute SHA-1 digest for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return SHA-1 digest (20 bytes).
    GP_NODISCARD static constexpr SHA1Digest Hash(const char* data, SizeT length) noexcept
    {
        UInt32 state[5] = { H0, H1, H2, H3, H4 };

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
        Detail::HashUtilities::WriteU64BE(padded, padLen - 8, bitLength);

        ProcessBlock(padded, state);
        if (padLen == 128) { ProcessBlock(padded + 64, state); }

        SHA1Digest result{};
        for (SizeT i = 0; i < 5; ++i)
        {
            result.bytes[i * 4] = static_cast<Byte>((state[i] >> 24) & 0xFF);
            result.bytes[i * 4 + 1] = static_cast<Byte>((state[i] >> 16) & 0xFF);
            result.bytes[i * 4 + 2] = static_cast<Byte>((state[i] >> 8) & 0xFF);
            result.bytes[i * 4 + 3] = static_cast<Byte>(state[i] & 0xFF);
        }
        return result;
    }

    /// \brief Compute SHA-1 digest from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return SHA-1 digest.
    GP_NODISCARD static constexpr SHA1Digest Hash(const void* data, SizeT length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Compute SHA-1 digest for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return SHA-1 digest.
    GP_NODISCARD static constexpr SHA1Digest Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute SHA-1 digest from a string view.
    /// \param view String view to hash.
    /// \return SHA-1 digest.
    GP_NODISCARD static constexpr SHA1Digest Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }

    /// \brief Verify data against an expected SHA-1 digest.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected digest value.
    /// \return True if computed digest matches expected.
    GP_NODISCARD static constexpr bool Verify(const char* data, SizeT length, const SHA1Digest& expected) noexcept
    {
        return Hash(data, length) == expected;
    }

    /// \brief Verify data from a void pointer against an expected digest.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected digest value.
    /// \return True if computed digest matches expected.
    GP_NODISCARD static constexpr bool Verify(const void* data, SizeT length, const SHA1Digest& expected) noexcept
    {
        return Hash(data, length) == expected;
    }

    /// \brief Streaming SHA-1 context for incremental hashing.
    struct Context
    {
    public:
        UInt32 state[5] = { H0, H1, H2, H3, H4 };
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
        /// \return SHA-1 digest.
        GP_NODISCARD constexpr SHA1Digest Finalize() noexcept
        {
            char padded[128]{};
            for (SizeT i = 0; i < bufferLen; ++i) { padded[i] = buffer[i]; }
            padded[bufferLen] = static_cast<char>(0x80);

            SizeT padLen = (bufferLen < 56) ? 64 : 128;
            UInt64 bitLength = totalLen * 8;
            Detail::HashUtilities::WriteU64BE(padded, padLen - 8, bitLength);

            ProcessBlock(padded, state);
            if (padLen == 128) { ProcessBlock(padded + 64, state); }

            SHA1Digest result{};
            for (SizeT i = 0; i < 5; ++i)
            {
                result.bytes[i * 4] = static_cast<Byte>((state[i] >> 24) & 0xFF);
                result.bytes[i * 4 + 1] = static_cast<Byte>((state[i] >> 16) & 0xFF);
                result.bytes[i * 4 + 2] = static_cast<Byte>((state[i] >> 8) & 0xFF);
                result.bytes[i * 4 + 3] = static_cast<Byte>(state[i] & 0xFF);
            }
            return result;
        }
    };
};

}   // namespace GP::Crypto
