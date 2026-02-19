// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief BLAKE2b cryptographic hash function (RFC 7693).
/// Fast, secure, and modern. Supports variable output length (1-64 bytes) and optional keyed hashing.
/// Recommended as the primary cryptographic hash for new applications.
struct BLAKE2b final : public Template::NonInstantiable
{
public:
    static constexpr SizeT BLOCK_SIZE = 128;       //<! BLAKE2b processes 128-byte blocks.
    static constexpr SizeT MAX_DIGEST_SIZE = 64;   //<! Maximum output length in bytes.
    static constexpr SizeT MAX_KEY_SIZE = 64;      //<! Maximum key length in bytes.
    static constexpr SizeT DEFAULT_DIGEST_SIZE = 32;

private:
    static constexpr UInt64 IV[8] = { 0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL, 0x3C6EF372FE94F82BULL,
                                      0xA54FF53A5F1D36F1ULL, 0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL,
                                      0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL };

    static constexpr UInt8 SIGMA[12][16] = {
        {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
        { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 },
        { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 },
        {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 },
        {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 },
        {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 },
        { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 },
        { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 },
        {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 },
        { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13,  0 },
        {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 },
        { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 }
    };

    static constexpr void G(UInt64* v, Int32 a, Int32 b, Int32 c, Int32 d, UInt64 x, UInt64 y) noexcept
    {
        v[a] = v[a] + v[b] + x;
        v[d] = Detail::HashUtilities::Rotr64(v[d] ^ v[a], 32);
        v[c] = v[c] + v[d];
        v[b] = Detail::HashUtilities::Rotr64(v[b] ^ v[c], 24);
        v[a] = v[a] + v[b] + y;
        v[d] = Detail::HashUtilities::Rotr64(v[d] ^ v[a], 16);
        v[c] = v[c] + v[d];
        v[b] = Detail::HashUtilities::Rotr64(v[b] ^ v[c], 63);
    }

    static constexpr void
        Compress(UInt64* state, const char* block, UInt64 countLo, UInt64 countHi, bool isLast) noexcept
    {
        UInt64 m[16];
        for (SizeT i = 0; i < 16; ++i) { m[i] = Detail::HashUtilities::ReadU64LE(block, i * 8); }

        UInt64 v[16];
        for (SizeT i = 0; i < 8; ++i) { v[i] = state[i]; }
        v[8] = IV[0];
        v[9] = IV[1];
        v[10] = IV[2];
        v[11] = IV[3];
        v[12] = IV[4] ^ countLo;
        v[13] = IV[5] ^ countHi;
        v[14] = isLast ? ~IV[6] : IV[6];
        v[15] = IV[7];

        for (SizeT round = 0; round < 12; ++round)
        {
            const auto* s = SIGMA[round];
            G(v, 0, 4, 8, 12, m[s[0]], m[s[1]]);
            G(v, 1, 5, 9, 13, m[s[2]], m[s[3]]);
            G(v, 2, 6, 10, 14, m[s[4]], m[s[5]]);
            G(v, 3, 7, 11, 15, m[s[6]], m[s[7]]);
            G(v, 0, 5, 10, 15, m[s[8]], m[s[9]]);
            G(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
            G(v, 2, 7, 8, 13, m[s[12]], m[s[13]]);
            G(v, 3, 4, 9, 14, m[s[14]], m[s[15]]);
        }

        for (SizeT i = 0; i < 8; ++i) { state[i] ^= v[i] ^ v[i + 8]; }
    }

public:
    /// \brief Compute BLAKE2b digest for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param outLen Desired output length in bytes (1-64, default 32).
    /// \return BLAKE2b digest with the specified output length.
    GP_NODISCARD static constexpr BLAKE2bDigest
        Hash(const char* data, SizeT length, SizeT outLen = DEFAULT_DIGEST_SIZE) noexcept
    {
        return HashKeyed(data, length, nullptr, 0, outLen);
    }

    /// \brief Compute BLAKE2b digest from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param outLen Desired output length in bytes.
    /// \return BLAKE2b digest.
    GP_NODISCARD static constexpr BLAKE2bDigest
        Hash(const void* data, SizeT length, SizeT outLen = DEFAULT_DIGEST_SIZE) noexcept
    {
        return Hash(static_cast<const char*>(data), length, outLen);
    }

    /// \brief Compute BLAKE2b digest for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return BLAKE2b digest (32 bytes default).
    GP_NODISCARD static constexpr BLAKE2bDigest Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str), DEFAULT_DIGEST_SIZE);
    }

    /// \brief Compute BLAKE2b digest from a string view.
    /// \param view String view to hash.
    /// \param outLen Desired output length in bytes.
    /// \return BLAKE2b digest.
    GP_NODISCARD static constexpr BLAKE2bDigest Hash(std::string_view view, SizeT outLen = DEFAULT_DIGEST_SIZE) noexcept
    {
        return Hash(view.data(), view.size(), outLen);
    }

    /// \brief Compute keyed BLAKE2b digest (MAC).
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \param key Pointer to the key bytes (may be nullptr for unkeyed).
    /// \param keyLen Key length in bytes (0-64).
    /// \param outLen Desired output length in bytes (1-64).
    /// \return BLAKE2b digest.
    GP_NODISCARD static constexpr BLAKE2bDigest HashKeyed(
        const char* data, SizeT length, const char* key, SizeT keyLen, SizeT outLen = DEFAULT_DIGEST_SIZE
    ) noexcept
    {
        UInt64 state[8];
        for (SizeT i = 0; i < 8; ++i) { state[i] = IV[i]; }

        state[0] ^= 0x01010000ULL ^ (static_cast<UInt64>(keyLen) << 8) ^ static_cast<UInt64>(outLen);

        UInt64 countLo = 0;
        UInt64 countHi = 0;

        if (keyLen > 0 && key != nullptr)
        {
            char keyBlock[BLOCK_SIZE]{};
            for (SizeT i = 0; i < keyLen; ++i) { keyBlock[i] = key[i]; }

            if (length == 0)
            {
                countLo = BLOCK_SIZE;
                Compress(state, keyBlock, countLo, countHi, true);

                BLAKE2bDigest result{};
                result.length = outLen;
                for (SizeT i = 0; i < outLen; ++i)
                {
                    result.bytes[i] = static_cast<Byte>((state[i / 8] >> (8 * (i % 8))) & 0xFF);
                }
                return result;
            }

            countLo = BLOCK_SIZE;
            Compress(state, keyBlock, countLo, countHi, false);
        }

        SizeT offset = 0;
        while (offset + BLOCK_SIZE < length)
        {
            countLo += BLOCK_SIZE;
            if (countLo < BLOCK_SIZE) { ++countHi; }
            Compress(state, data + offset, countLo, countHi, false);
            offset += BLOCK_SIZE;
        }

        SizeT remaining = length - offset;
        char lastBlock[BLOCK_SIZE]{};
        for (SizeT i = 0; i < remaining; ++i) { lastBlock[i] = data[offset + i]; }
        countLo += remaining;
        if (countLo < remaining) { ++countHi; }
        Compress(state, lastBlock, countLo, countHi, true);

        BLAKE2bDigest result{};
        result.length = outLen;
        for (SizeT i = 0; i < outLen; ++i)
        {
            result.bytes[i] = static_cast<Byte>((state[i / 8] >> (8 * (i % 8))) & 0xFF);
        }
        return result;
    }

    /// \brief Verify data against an expected BLAKE2b digest.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected digest value.
    /// \return True if computed digest matches expected.
    GP_NODISCARD static constexpr bool Verify(const char* data, SizeT length, const BLAKE2bDigest& expected) noexcept
    {
        return Hash(data, length, expected.length) == expected;
    }

    /// \brief Streaming BLAKE2b context for incremental hashing.
    struct Context
    {
    public:
        UInt64 state[8]{};
        char buffer[BLOCK_SIZE]{};
        SizeT bufferLen = 0;
        UInt64 countLo = 0;
        UInt64 countHi = 0;
        SizeT outLen = DEFAULT_DIGEST_SIZE;

    public:
        /// \brief Initialize the context.
        /// \param digestLen Desired output length in bytes (1-64, default 32).
        /// \param key Optional key for keyed hashing.
        /// \param keyLen Key length in bytes.
        constexpr void Init(SizeT digestLen = DEFAULT_DIGEST_SIZE, const char* key = nullptr, SizeT keyLen = 0) noexcept
        {
            outLen = digestLen;
            for (SizeT i = 0; i < 8; ++i) { state[i] = IV[i]; }
            state[0] ^= 0x01010000ULL ^ (static_cast<UInt64>(keyLen) << 8) ^ static_cast<UInt64>(outLen);
            bufferLen = 0;
            countLo = 0;
            countHi = 0;

            if (keyLen > 0 && key != nullptr)
            {
                char keyBlock[BLOCK_SIZE]{};
                for (SizeT i = 0; i < keyLen; ++i) { keyBlock[i] = key[i]; }
                for (SizeT i = 0; i < BLOCK_SIZE; ++i) { buffer[i] = keyBlock[i]; }
                bufferLen = BLOCK_SIZE;
            }
        }

        /// \brief Feed data into the hash context.
        /// \param data Pointer to the input bytes.
        /// \param length Number of bytes to process.
        constexpr void Update(const char* data, SizeT length) noexcept
        {
            SizeT offset = 0;

            if (bufferLen > 0)
            {
                SizeT fill = BLOCK_SIZE - bufferLen;
                if (length <= fill)
                {
                    for (SizeT i = 0; i < length; ++i) { buffer[bufferLen + i] = data[i]; }
                    bufferLen += length;
                    return;
                }
                for (SizeT i = 0; i < fill; ++i) { buffer[bufferLen + i] = data[i]; }
                countLo += BLOCK_SIZE;
                if (countLo < BLOCK_SIZE) { ++countHi; }
                Compress(state, buffer, countLo, countHi, false);
                bufferLen = 0;
                offset = fill;
            }

            while (offset + BLOCK_SIZE < length)
            {
                countLo += BLOCK_SIZE;
                if (countLo < BLOCK_SIZE) { ++countHi; }
                Compress(state, data + offset, countLo, countHi, false);
                offset += BLOCK_SIZE;
            }

            SizeT remaining = length - offset;
            for (SizeT i = 0; i < remaining; ++i) { buffer[i] = data[offset + i]; }
            bufferLen = remaining;
        }

        /// \brief Finalize the hash and return the digest.
        /// \return BLAKE2b digest.
        GP_NODISCARD constexpr BLAKE2bDigest Finalize() noexcept
        {
            countLo += bufferLen;
            if (countLo < bufferLen) { ++countHi; }

            for (SizeT i = bufferLen; i < BLOCK_SIZE; ++i) { buffer[i] = 0; }
            Compress(state, buffer, countLo, countHi, true);

            BLAKE2bDigest result{};
            result.length = outLen;
            for (SizeT i = 0; i < outLen; ++i)
            {
                result.bytes[i] = static_cast<Byte>((state[i / 8] >> (8 * (i % 8))) & 0xFF);
            }
            return result;
        }
    };
};

}   // namespace GP::Crypto
