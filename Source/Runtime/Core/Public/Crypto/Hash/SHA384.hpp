// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Crypto/Hash/SHA512.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief SHA-384 (Secure Hash Algorithm 384) implementation per FIPS 180-4.
/// Produces a 384-bit (48-byte) message digest. Uses the same block processing as SHA-512
/// with different initial hash values and a truncated output.
struct SHA384 final : public Template::NonInstantiable
{
public:
    static constexpr SizeT BLOCK_SIZE = 128;   //<! SHA-384 processes 128-byte blocks (same as SHA-512).
    static constexpr SizeT DIGEST_SIZE = 48;   //<! SHA-384 produces a 48-byte digest.

private:
    /// \brief Initial hash values for SHA-384 (FIPS 180-4 Section 5.3.4).
    static constexpr UInt64 H0 = 0xCBBB9D5DC1059ED8ULL;
    static constexpr UInt64 H1 = 0x629A292A367CD507ULL;
    static constexpr UInt64 H2 = 0x9159015A3070DD17ULL;
    static constexpr UInt64 H3 = 0x152FECD8F70E5939ULL;
    static constexpr UInt64 H4 = 0x67332667FFC00B31ULL;
    static constexpr UInt64 H5 = 0x8EB44A8768581511ULL;
    static constexpr UInt64 H6 = 0xDB0C2E0D64F98FA7ULL;
    static constexpr UInt64 H7 = 0x47B5481DBEFA4FA4ULL;

    /// \brief Round constants K (identical to SHA-512, FIPS 180-4 Section 4.2.3).
    static constexpr UInt64 K[80] = {
        0x428A2F98D728AE22ULL, 0x7137449123EF65CDULL, 0xB5C0FBCFEC4D3B2FULL, 0xE9B5DBA58189DBBCULL,
        0x3956C25BF348B538ULL, 0x59F111F1B605D019ULL, 0x923F82A4AF194F9BULL, 0xAB1C5ED5DA6D8118ULL,
        0xD807AA98A3030242ULL, 0x12835B0145706FBEULL, 0x243185BE4EE4B28CULL, 0x550C7DC3D5FFB4E2ULL,
        0x72BE5D74F27B896FULL, 0x80DEB1FE3B1696B1ULL, 0x9BDC06A725C71235ULL, 0xC19BF174CF692694ULL,
        0xE49B69C19EF14AD2ULL, 0xEFBE4786384F25E3ULL, 0x0FC19DC68B8CD5B5ULL, 0x240CA1CC77AC9C65ULL,
        0x2DE92C6F592B0275ULL, 0x4A7484AA6EA6E483ULL, 0x5CB0A9DCBD41FBD4ULL, 0x76F988DA831153B5ULL,
        0x983E5152EE66DFABULL, 0xA831C66D2DB43210ULL, 0xB00327C898FB213FULL, 0xBF597FC7BEEF0EE4ULL,
        0xC6E00BF33DA88FC2ULL, 0xD5A79147930AA725ULL, 0x06CA6351E003826FULL, 0x142929670A0E6E70ULL,
        0x27B70A8546D22FFCULL, 0x2E1B21385C26C926ULL, 0x4D2C6DFC5AC42AEDULL, 0x53380D139D95B3DFULL,
        0x650A73548BAF63DEULL, 0x766A0ABB3C77B2A8ULL, 0x81C2C92E47EDAEE6ULL, 0x92722C851482353BULL,
        0xA2BFE8A14CF10364ULL, 0xA81A664BBC423001ULL, 0xC24B8B70D0F89791ULL, 0xC76C51A30654BE30ULL,
        0xD192E819D6EF5218ULL, 0xD69906245565A910ULL, 0xF40E35855771202AULL, 0x106AA07032BBD1B8ULL,
        0x19A4C116B8D2D0C8ULL, 0x1E376C085141AB53ULL, 0x2748774CDF8EEB99ULL, 0x34B0BCB5E19B48A8ULL,
        0x391C0CB3C5C95A63ULL, 0x4ED8AA4AE3418ACBULL, 0x5B9CCA4F7763E373ULL, 0x682E6FF3D6B2B8A3ULL,
        0x748F82EE5DEFB2FCULL, 0x78A5636F43172F60ULL, 0x84C87814A1F0AB72ULL, 0x8CC702081A6439ECULL,
        0x90BEFFFA23631E28ULL, 0xA4506CEBDE82BDE9ULL, 0xBEF9A3F7B2C67915ULL, 0xC67178F2E372532BULL,
        0xCA273ECEEA26619CULL, 0xD186B8C721C0C207ULL, 0xEADA7DD6CDE0EB1EULL, 0xF57D4F7FEE6ED178ULL,
        0x06F067AA72176FBAULL, 0x0A637DC5A2C898A6ULL, 0x113F9804BEF90DAEULL, 0x1B710B35131C471BULL,
        0x28DB77F523047D84ULL, 0x32CAAB7B40C72493ULL, 0x3C9EBE0A15C9BEBCULL, 0x431D67C49C100D4CULL,
        0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL, 0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL
    };

    /// \brief SHA-384/512 Ch(x, y, z) = (x & y) ^ (~x & z).
    GP_NODISCARD static constexpr UInt64 Ch(UInt64 x, UInt64 y, UInt64 z) noexcept { return (x & y) ^ (~x & z); }

    /// \brief SHA-384/512 Maj(x, y, z) = (x & y) ^ (x & z) ^ (y & z).
    GP_NODISCARD static constexpr UInt64 Maj(UInt64 x, UInt64 y, UInt64 z) noexcept
    {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    /// \brief SHA-384/512 big Sigma0: ROTR(28) ^ ROTR(34) ^ ROTR(39).
    GP_NODISCARD static constexpr UInt64 Sigma0(UInt64 x) noexcept
    {
        return Detail::HashUtilities::Rotr64(x, 28) ^ Detail::HashUtilities::Rotr64(x, 34) ^
               Detail::HashUtilities::Rotr64(x, 39);
    }

    /// \brief SHA-384/512 big Sigma1: ROTR(14) ^ ROTR(18) ^ ROTR(41).
    GP_NODISCARD static constexpr UInt64 Sigma1(UInt64 x) noexcept
    {
        return Detail::HashUtilities::Rotr64(x, 14) ^ Detail::HashUtilities::Rotr64(x, 18) ^
               Detail::HashUtilities::Rotr64(x, 41);
    }

    /// \brief SHA-384/512 small sigma0: ROTR(1) ^ ROTR(8) ^ SHR(7).
    GP_NODISCARD static constexpr UInt64 sigma0(UInt64 x) noexcept
    {
        return Detail::HashUtilities::Rotr64(x, 1) ^ Detail::HashUtilities::Rotr64(x, 8) ^ (x >> 7);
    }

    /// \brief SHA-384/512 small sigma1: ROTR(19) ^ ROTR(61) ^ SHR(6).
    GP_NODISCARD static constexpr UInt64 sigma1(UInt64 x) noexcept
    {
        return Detail::HashUtilities::Rotr64(x, 19) ^ Detail::HashUtilities::Rotr64(x, 61) ^ (x >> 6);
    }

    /// \brief Process a single 128-byte block and update state.
    /// Identical algorithm to SHA-512 (FIPS 180-4 Section 6.4.2).
    /// \param block Pointer to the 128-byte block.
    /// \param state Pointer to the 8-element state array.
    static constexpr void ProcessBlock(const char* block, UInt64* state) noexcept
    {
        UInt64 w[80]{};
        for (SizeT i = 0; i < 16; ++i) { w[i] = Detail::HashUtilities::ReadU64BE(block, i * 8); }
        for (SizeT i = 16; i < 80; ++i) { w[i] = sigma1(w[i - 2]) + w[i - 7] + sigma0(w[i - 15]) + w[i - 16]; }

        UInt64 a = state[0];
        UInt64 b = state[1];
        UInt64 c = state[2];
        UInt64 d = state[3];
        UInt64 e = state[4];
        UInt64 f = state[5];
        UInt64 g = state[6];
        UInt64 h = state[7];

        for (SizeT i = 0; i < 80; ++i)
        {
            UInt64 t1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + w[i];
            UInt64 t2 = Sigma0(a) + Maj(a, b, c);
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
    /// \brief Compute SHA-384 digest for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return SHA-384 digest (48 bytes).
    GP_NODISCARD static constexpr SHA384Digest Hash(const char* data, SizeT length) noexcept
    {
        UInt64 state[8] = { H0, H1, H2, H3, H4, H5, H6, H7 };

        SizeT offset = 0;
        while (offset + BLOCK_SIZE <= length)
        {
            ProcessBlock(data + offset, state);
            offset += BLOCK_SIZE;
        }

        char padded[256]{};
        SizeT remaining = length - offset;
        for (SizeT i = 0; i < remaining; ++i) { padded[i] = data[offset + i]; }
        padded[remaining] = static_cast<char>(0x80);

        SizeT padLen = (remaining < 112) ? 128 : 256;
        UInt64 bitLength = static_cast<UInt64>(length) * 8;
        // SHA-384 appends a 128-bit length. Upper 64 bits are zero for messages < 2^64 bits.
        Detail::HashUtilities::WriteU64BE(padded, padLen - 16, 0ULL);
        Detail::HashUtilities::WriteU64BE(padded, padLen - 8, bitLength);

        ProcessBlock(padded, state);
        if (padLen == 256) { ProcessBlock(padded + 128, state); }

        // SHA-384 output is the first 48 bytes (first 6 of 8 state words).
        SHA384Digest result{};
        for (SizeT i = 0; i < 6; ++i)
        {
            result.bytes[i * 8] = static_cast<Byte>((state[i] >> 56) & 0xFF);
            result.bytes[i * 8 + 1] = static_cast<Byte>((state[i] >> 48) & 0xFF);
            result.bytes[i * 8 + 2] = static_cast<Byte>((state[i] >> 40) & 0xFF);
            result.bytes[i * 8 + 3] = static_cast<Byte>((state[i] >> 32) & 0xFF);
            result.bytes[i * 8 + 4] = static_cast<Byte>((state[i] >> 24) & 0xFF);
            result.bytes[i * 8 + 5] = static_cast<Byte>((state[i] >> 16) & 0xFF);
            result.bytes[i * 8 + 6] = static_cast<Byte>((state[i] >> 8) & 0xFF);
            result.bytes[i * 8 + 7] = static_cast<Byte>(state[i] & 0xFF);
        }
        return result;
    }

    /// \brief Compute SHA-384 digest from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return SHA-384 digest.
    GP_NODISCARD static constexpr SHA384Digest Hash(const void* data, SizeT length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Compute SHA-384 digest for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return SHA-384 digest.
    GP_NODISCARD static constexpr SHA384Digest Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute SHA-384 digest from a string view.
    /// \param view String view to hash.
    /// \return SHA-384 digest.
    GP_NODISCARD static constexpr SHA384Digest Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }

    /// \brief Verify data against an expected SHA-384 digest.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected digest value.
    /// \return True if computed digest matches expected.
    GP_NODISCARD static constexpr bool Verify(const char* data, SizeT length, const SHA384Digest& expected) noexcept
    {
        return Hash(data, length) == expected;
    }

    /// \brief Verify data from a void pointer against an expected digest.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected digest value.
    /// \return True if computed digest matches expected.
    GP_NODISCARD static constexpr bool Verify(const void* data, SizeT length, const SHA384Digest& expected) noexcept
    {
        return Hash(data, length) == expected;
    }

    /// \brief Streaming SHA-384 context for incremental hashing.
    struct Context
    {
    public:
        UInt64 state[8] = { H0, H1, H2, H3, H4, H5, H6, H7 };
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
        /// \return SHA-384 digest.
        GP_NODISCARD constexpr SHA384Digest Finalize() noexcept
        {
            char padded[256]{};
            for (SizeT i = 0; i < bufferLen; ++i) { padded[i] = buffer[i]; }
            padded[bufferLen] = static_cast<char>(0x80);

            SizeT padLen = (bufferLen < 112) ? 128 : 256;
            UInt64 bitLength = totalLen * 8;
            Detail::HashUtilities::WriteU64BE(padded, padLen - 16, 0ULL);
            Detail::HashUtilities::WriteU64BE(padded, padLen - 8, bitLength);

            ProcessBlock(padded, state);
            if (padLen == 256) { ProcessBlock(padded + 128, state); }

            // SHA-384 output is the first 48 bytes (first 6 of 8 state words).
            SHA384Digest result{};
            for (SizeT i = 0; i < 6; ++i)
            {
                result.bytes[i * 8] = static_cast<Byte>((state[i] >> 56) & 0xFF);
                result.bytes[i * 8 + 1] = static_cast<Byte>((state[i] >> 48) & 0xFF);
                result.bytes[i * 8 + 2] = static_cast<Byte>((state[i] >> 40) & 0xFF);
                result.bytes[i * 8 + 3] = static_cast<Byte>((state[i] >> 32) & 0xFF);
                result.bytes[i * 8 + 4] = static_cast<Byte>((state[i] >> 24) & 0xFF);
                result.bytes[i * 8 + 5] = static_cast<Byte>((state[i] >> 16) & 0xFF);
                result.bytes[i * 8 + 6] = static_cast<Byte>((state[i] >> 8) & 0xFF);
                result.bytes[i * 8 + 7] = static_cast<Byte>(state[i] & 0xFF);
            }
            return result;
        }
    };
};

}   // namespace GP::Crypto
