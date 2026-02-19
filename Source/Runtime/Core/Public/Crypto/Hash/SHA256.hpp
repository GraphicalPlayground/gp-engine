// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief SHA-256 (Secure Hash Algorithm 256) implementation per FIPS 180-4.
/// Produces a 256-bit (32-byte) message digest. All operations are constexpr-capable.
struct SHA256 final : public Template::NonInstantiable
{
public:
    static constexpr SizeT BLOCK_SIZE = 64;    //<! SHA-256 processes 64-byte blocks.
    static constexpr SizeT DIGEST_SIZE = 32;   //<! SHA-256 produces a 32-byte digest.

private:
    /// \brief Initial hash values (FIPS 180-4 Section 5.3.3).
    static constexpr UInt32 H0 = 0x6A09E667U;
    static constexpr UInt32 H1 = 0xBB67AE85U;
    static constexpr UInt32 H2 = 0x3C6EF372U;
    static constexpr UInt32 H3 = 0xA54FF53AU;
    static constexpr UInt32 H4 = 0x510E527FU;
    static constexpr UInt32 H5 = 0x9B05688CU;
    static constexpr UInt32 H6 = 0x1F83D9ABU;
    static constexpr UInt32 H7 = 0x5BE0CD19U;

    /// \brief Round constants K (FIPS 180-4 Section 4.2.2).
    static constexpr UInt32 K[64] = {
        0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
        0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U, 0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
        0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
        0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U, 0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
        0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U, 0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
        0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
        0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
        0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U, 0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U
    };

    /// \brief SHA-256 Ch(x, y, z) = (x & y) ^ (~x & z).
    GP_NODISCARD static constexpr UInt32 Ch(UInt32 x, UInt32 y, UInt32 z) noexcept { return (x & y) ^ (~x & z); }

    /// \brief SHA-256 Maj(x, y, z) = (x & y) ^ (x & z) ^ (y & z).
    GP_NODISCARD static constexpr UInt32 Maj(UInt32 x, UInt32 y, UInt32 z) noexcept
    {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    /// \brief SHA-256 big Sigma0: ROTR(2) ^ ROTR(13) ^ ROTR(22).
    GP_NODISCARD static constexpr UInt32 Sigma0(UInt32 x) noexcept
    {
        return Detail::HashUtilities::Rotr32(x, 2) ^ Detail::HashUtilities::Rotr32(x, 13) ^
               Detail::HashUtilities::Rotr32(x, 22);
    }

    /// \brief SHA-256 big Sigma1: ROTR(6) ^ ROTR(11) ^ ROTR(25).
    GP_NODISCARD static constexpr UInt32 Sigma1(UInt32 x) noexcept
    {
        return Detail::HashUtilities::Rotr32(x, 6) ^ Detail::HashUtilities::Rotr32(x, 11) ^
               Detail::HashUtilities::Rotr32(x, 25);
    }

    /// \brief SHA-256 small sigma0: ROTR(7) ^ ROTR(18) ^ SHR(3).
    GP_NODISCARD static constexpr UInt32 sigma0(UInt32 x) noexcept
    {
        return Detail::HashUtilities::Rotr32(x, 7) ^ Detail::HashUtilities::Rotr32(x, 18) ^ (x >> 3);
    }

    /// \brief SHA-256 small sigma1: ROTR(17) ^ ROTR(19) ^ SHR(10).
    GP_NODISCARD static constexpr UInt32 sigma1(UInt32 x) noexcept
    {
        return Detail::HashUtilities::Rotr32(x, 17) ^ Detail::HashUtilities::Rotr32(x, 19) ^ (x >> 10);
    }

    /// \brief Process a single 64-byte block and update state.
    /// \param block Pointer to the 64-byte block.
    /// \param state Pointer to the 8-element state array.
    static constexpr void ProcessBlock(const char* block, UInt32* state) noexcept
    {
        UInt32 w[64]{};
        for (SizeT i = 0; i < 16; ++i) { w[i] = Detail::HashUtilities::ReadU32BE(block, i * 4); }
        for (SizeT i = 16; i < 64; ++i) { w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16]; }

        UInt32 a = state[0];
        UInt32 b = state[1];
        UInt32 c = state[2];
        UInt32 d = state[3];
        UInt32 e = state[4];
        UInt32 f = state[5];
        UInt32 g = state[6];
        UInt32 h = state[7];

        for (SizeT i = 0; i < 64; ++i)
        {
            UInt32 t1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + w[i];
            UInt32 t2 = Sigma0(a) + Maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

public:
    /// \brief Compute SHA-256 digest for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return SHA-256 digest (32 bytes).
    GP_NODISCARD static constexpr SHA256Digest Hash(const char* data, SizeT length) noexcept
    {
        UInt32 state[8] = { H0, H1, H2, H3, H4, H5, H6, H7 };

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

        SHA256Digest result{};
        for (SizeT i = 0; i < 8; ++i)
        {
            result.bytes[i * 4] = static_cast<Byte>((state[i] >> 24) & 0xFF);
            result.bytes[i * 4 + 1] = static_cast<Byte>((state[i] >> 16) & 0xFF);
            result.bytes[i * 4 + 2] = static_cast<Byte>((state[i] >> 8) & 0xFF);
            result.bytes[i * 4 + 3] = static_cast<Byte>(state[i] & 0xFF);
        }
        return result;
    }

    /// \brief Compute SHA-256 digest from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return SHA-256 digest.
    GP_NODISCARD static constexpr SHA256Digest Hash(const void* data, SizeT length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Compute SHA-256 digest for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return SHA-256 digest.
    GP_NODISCARD static constexpr SHA256Digest Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute SHA-256 digest from a string view.
    /// \param view String view to hash.
    /// \return SHA-256 digest.
    GP_NODISCARD static constexpr SHA256Digest Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }

    /// \brief Verify data against an expected SHA-256 digest.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected digest value.
    /// \return True if computed digest matches expected.
    GP_NODISCARD static constexpr bool Verify(const char* data, SizeT length, const SHA256Digest& expected) noexcept
    {
        return Hash(data, length) == expected;
    }

    /// \brief Verify data from a void pointer against an expected digest.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected digest value.
    /// \return True if computed digest matches expected.
    GP_NODISCARD static constexpr bool Verify(const void* data, SizeT length, const SHA256Digest& expected) noexcept
    {
        return Hash(data, length) == expected;
    }

    /// \brief Streaming SHA-256 context for incremental hashing.
    struct Context
    {
    public:
        UInt32 state[8] = { H0, H1, H2, H3, H4, H5, H6, H7 };
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
        /// \return SHA-256 digest.
        GP_NODISCARD constexpr SHA256Digest Finalize() noexcept
        {
            char padded[128]{};
            for (SizeT i = 0; i < bufferLen; ++i) { padded[i] = buffer[i]; }
            padded[bufferLen] = static_cast<char>(0x80);

            SizeT padLen = (bufferLen < 56) ? 64 : 128;
            UInt64 bitLength = totalLen * 8;
            Detail::HashUtilities::WriteU64BE(padded, padLen - 8, bitLength);

            ProcessBlock(padded, state);
            if (padLen == 128) { ProcessBlock(padded + 64, state); }

            SHA256Digest result{};
            for (SizeT i = 0; i < 8; ++i)
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
