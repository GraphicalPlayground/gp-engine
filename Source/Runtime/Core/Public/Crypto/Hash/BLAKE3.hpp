// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Crypto/CryptoForward.hpp"
#include "Crypto/Hash/HashUtilities.hpp"
#include "Templates/Generic.hpp"
#include <string_view>

namespace GP::Crypto
{

/// \brief BLAKE3 cryptographic hash function.
/// Extremely fast, secure, and parallelizable. Default 256-bit output.
/// Based on the BLAKE3 specification by Jack O'Connor, Jean-Philippe Aumasson,
/// Samuel Neves, and Zooko Wilcox-O'Hearn.
struct BLAKE3 final : public Template::NonInstantiable
{
public:
    static constexpr SizeT BLOCK_SIZE = 64;     //<! BLAKE3 processes 64-byte blocks.
    static constexpr SizeT CHUNK_SIZE = 1024;   //<! BLAKE3 chunk size (16 blocks).
    static constexpr SizeT DIGEST_SIZE = 32;    //<! Default output size.
    static constexpr SizeT KEY_SIZE = 32;       //<! Key size for keyed hashing.

private:
    static constexpr UInt32 IV[8] = { 0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
                                      0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U };

    static constexpr UInt32 MSG_PERMUTATION[16] = { 2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8 };

    static constexpr UInt32 CHUNK_START = 1 << 0;
    static constexpr UInt32 CHUNK_END = 1 << 1;
    static constexpr UInt32 PARENT = 1 << 2;
    static constexpr UInt32 ROOT = 1 << 3;
    static constexpr UInt32 KEYED_HASH = 1 << 4;
    static constexpr UInt32 DERIVE_KEY_CONTEXT = 1 << 5;
    static constexpr UInt32 DERIVE_KEY_MATERIAL = 1 << 6;

    static constexpr void G(UInt32* state, SizeT a, SizeT b, SizeT c, SizeT d, UInt32 mx, UInt32 my) noexcept
    {
        state[a] = state[a] + state[b] + mx;
        state[d] = Detail::HashUtilities::Rotr32(state[d] ^ state[a], 16);
        state[c] = state[c] + state[d];
        state[b] = Detail::HashUtilities::Rotr32(state[b] ^ state[c], 12);
        state[a] = state[a] + state[b] + my;
        state[d] = Detail::HashUtilities::Rotr32(state[d] ^ state[a], 8);
        state[c] = state[c] + state[d];
        state[b] = Detail::HashUtilities::Rotr32(state[b] ^ state[c], 7);
    }

    static constexpr void Round(UInt32* state, const UInt32* msg) noexcept
    {
        G(state, 0, 4, 8, 12, msg[0], msg[1]);
        G(state, 1, 5, 9, 13, msg[2], msg[3]);
        G(state, 2, 6, 10, 14, msg[4], msg[5]);
        G(state, 3, 7, 11, 15, msg[6], msg[7]);
        G(state, 0, 5, 10, 15, msg[8], msg[9]);
        G(state, 1, 6, 11, 12, msg[10], msg[11]);
        G(state, 2, 7, 8, 13, msg[12], msg[13]);
        G(state, 3, 4, 9, 14, msg[14], msg[15]);
    }

    static constexpr void Permute(UInt32* msg) noexcept
    {
        UInt32 permuted[16];
        for (SizeT i = 0; i < 16; ++i) { permuted[i] = msg[MSG_PERMUTATION[i]]; }
        for (SizeT i = 0; i < 16; ++i) { msg[i] = permuted[i]; }
    }

    static constexpr void Compress(
        const UInt32* chainingValue,
        const UInt32* blockWords,
        UInt64 counter,
        UInt32 blockLen,
        UInt32 flags,
        UInt32* out
    ) noexcept
    {
        UInt32 state[16] = { chainingValue[0],
                             chainingValue[1],
                             chainingValue[2],
                             chainingValue[3],
                             chainingValue[4],
                             chainingValue[5],
                             chainingValue[6],
                             chainingValue[7],
                             IV[0],
                             IV[1],
                             IV[2],
                             IV[3],
                             static_cast<UInt32>(counter),
                             static_cast<UInt32>(counter >> 32),
                             blockLen,
                             flags };

        UInt32 msg[16];
        for (SizeT i = 0; i < 16; ++i) { msg[i] = blockWords[i]; }

        Round(state, msg);
        Permute(msg);
        Round(state, msg);
        Permute(msg);
        Round(state, msg);
        Permute(msg);
        Round(state, msg);
        Permute(msg);
        Round(state, msg);
        Permute(msg);
        Round(state, msg);
        Permute(msg);
        Round(state, msg);

        for (SizeT i = 0; i < 8; ++i)
        {
            state[i] ^= state[i + 8];
            state[i + 8] ^= chainingValue[i];
        }

        for (SizeT i = 0; i < 16; ++i) { out[i] = state[i]; }
    }

    static constexpr void WordsFromBlock(const char* block, SizeT blockLen, UInt32* out) noexcept
    {
        char padded[BLOCK_SIZE]{};
        for (SizeT i = 0; i < blockLen; ++i) { padded[i] = block[i]; }
        for (SizeT i = 0; i < 16; ++i) { out[i] = Detail::HashUtilities::ReadU32LE(padded, i * 4); }
    }

    struct Output
    {
        UInt32 chainingValue[8]{};
        UInt32 blockWords[16]{};
        UInt64 counter = 0;
        UInt32 blockLen = 0;
        UInt32 flags = 0;

        constexpr void ChainingValue(UInt32* out) const noexcept
        {
            UInt32 full[16];
            Compress(chainingValue, blockWords, counter, blockLen, flags, full);
            for (SizeT i = 0; i < 8; ++i) { out[i] = full[i]; }
        }

        constexpr void RootBytes(Byte* out, SizeT outLen) const noexcept
        {
            UInt64 outputBlockCounter = 0;
            SizeT offset = 0;
            while (offset < outLen)
            {
                UInt32 full[16];
                Compress(chainingValue, blockWords, outputBlockCounter, blockLen, flags | ROOT, full);
                SizeT take = outLen - offset;
                if (take > 64) { take = 64; }
                for (SizeT i = 0; i < take; ++i)
                {
                    out[offset + i] = static_cast<Byte>((full[i / 4] >> (8 * (i % 4))) & 0xFF);
                }
                offset += take;
                ++outputBlockCounter;
            }
        }
    };

    struct ChunkState
    {
        UInt32 chainingValue[8]{};
        UInt64 chunkCounter = 0;
        char block[BLOCK_SIZE]{};
        UInt32 blockLen = 0;
        UInt32 blocksCompressed = 0;
        UInt32 flags = 0;

        constexpr void Init(const UInt32* key, UInt64 counter, UInt32 f) noexcept
        {
            for (SizeT i = 0; i < 8; ++i) { chainingValue[i] = key[i]; }
            chunkCounter = counter;
            blockLen = 0;
            blocksCompressed = 0;
            flags = f;
            for (SizeT i = 0; i < BLOCK_SIZE; ++i) { block[i] = 0; }
        }

        constexpr SizeT Len() const noexcept
        {
            return static_cast<SizeT>(blocksCompressed) * BLOCK_SIZE + static_cast<SizeT>(blockLen);
        }

        constexpr UInt32 StartFlag() const noexcept { return blocksCompressed == 0 ? CHUNK_START : 0; }

        constexpr void Update(const char* data, SizeT length) noexcept
        {
            SizeT offset = 0;
            while (offset < length)
            {
                if (blockLen == BLOCK_SIZE)
                {
                    UInt32 blockWords[16];
                    WordsFromBlock(block, BLOCK_SIZE, blockWords);
                    UInt32 full[16];
                    Compress(chainingValue, blockWords, chunkCounter, BLOCK_SIZE, flags | StartFlag(), full);
                    for (SizeT i = 0; i < 8; ++i) { chainingValue[i] = full[i]; }
                    ++blocksCompressed;
                    blockLen = 0;
                    for (SizeT i = 0; i < BLOCK_SIZE; ++i) { block[i] = 0; }
                }
                SizeT want = BLOCK_SIZE - blockLen;
                SizeT take = length - offset;
                if (take > want) { take = want; }
                for (SizeT i = 0; i < take; ++i) { block[blockLen + i] = data[offset + i]; }
                blockLen += static_cast<UInt32>(take);
                offset += take;
            }
        }

        constexpr Output CreateOutput() const noexcept
        {
            Output out{};
            for (SizeT i = 0; i < 8; ++i) { out.chainingValue[i] = chainingValue[i]; }
            WordsFromBlock(block, blockLen, out.blockWords);
            out.counter = chunkCounter;
            out.blockLen = blockLen;
            out.flags = flags | StartFlag() | CHUNK_END;
            return out;
        }
    };

    static constexpr Output
        ParentOutput(const UInt32* leftCV, const UInt32* rightCV, const UInt32* key, UInt32 flags) noexcept
    {
        Output out{};
        for (SizeT i = 0; i < 8; ++i) { out.chainingValue[i] = key[i]; }
        UInt32 blockWords[16];
        for (SizeT i = 0; i < 8; ++i) { blockWords[i] = leftCV[i]; }
        for (SizeT i = 0; i < 8; ++i) { blockWords[8 + i] = rightCV[i]; }
        for (SizeT i = 0; i < 16; ++i) { out.blockWords[i] = blockWords[i]; }
        out.counter = 0;
        out.blockLen = BLOCK_SIZE;
        out.flags = PARENT | flags;
        return out;
    }

public:
    /// \brief Compute BLAKE3 digest for a data buffer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return BLAKE3 digest (32 bytes).
    GP_NODISCARD static constexpr BLAKE3Digest Hash(const char* data, SizeT length) noexcept
    {
        UInt32 key[8];
        for (SizeT i = 0; i < 8; ++i) { key[i] = IV[i]; }

        UInt32 cvStack[54 * 8]{};
        UInt32 cvStackLen = 0;
        UInt64 chunkCounter = 0;

        ChunkState chunk{};
        chunk.Init(key, 0, 0);

        SizeT offset = 0;
        while (offset < length)
        {
            if (chunk.Len() == CHUNK_SIZE)
            {
                UInt32 chunkCV[8];
                chunk.CreateOutput().ChainingValue(chunkCV);

                UInt64 totalChunks = chunkCounter + 1;
                while ((totalChunks & 1) == 0)
                {
                    UInt32 rightCV[8];
                    for (SizeT i = 0; i < 8; ++i) { rightCV[i] = chunkCV[i]; }
                    --cvStackLen;
                    UInt32 leftCV[8];
                    for (SizeT i = 0; i < 8; ++i) { leftCV[i] = cvStack[cvStackLen * 8 + i]; }
                    ParentOutput(leftCV, rightCV, key, 0).ChainingValue(chunkCV);
                    totalChunks >>= 1;
                }
                for (SizeT i = 0; i < 8; ++i) { cvStack[cvStackLen * 8 + i] = chunkCV[i]; }
                ++cvStackLen;
                chunkCounter = totalChunks;

                chunk.Init(key, chunkCounter, 0);
            }

            SizeT want = CHUNK_SIZE - chunk.Len();
            SizeT take = length - offset;
            if (take > want) { take = want; }
            chunk.Update(data + offset, take);
            offset += take;
        }

        Output output = chunk.CreateOutput();

        while (cvStackLen > 0)
        {
            --cvStackLen;
            UInt32 chunkCV[8];
            output.ChainingValue(chunkCV);
            UInt32 leftCV[8];
            for (SizeT i = 0; i < 8; ++i) { leftCV[i] = cvStack[cvStackLen * 8 + i]; }
            output = ParentOutput(leftCV, chunkCV, key, 0);
        }

        BLAKE3Digest result{};
        output.RootBytes(result.bytes, DIGEST_SIZE);
        return result;
    }

    /// \brief Compute BLAKE3 digest from a void pointer.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to hash.
    /// \return BLAKE3 digest.
    GP_NODISCARD static constexpr BLAKE3Digest Hash(const void* data, SizeT length) noexcept
    {
        return Hash(static_cast<const char*>(data), length);
    }

    /// \brief Compute BLAKE3 digest for a null-terminated C-string.
    /// \param str Null-terminated input string.
    /// \return BLAKE3 digest.
    GP_NODISCARD static constexpr BLAKE3Digest Hash(const char* str) noexcept
    {
        return Hash(str, std::char_traits<char>::length(str));
    }

    /// \brief Compute BLAKE3 digest from a string view.
    /// \param view String view to hash.
    /// \return BLAKE3 digest.
    GP_NODISCARD static constexpr BLAKE3Digest Hash(std::string_view view) noexcept
    {
        return Hash(view.data(), view.size());
    }

    /// \brief Verify data against an expected BLAKE3 digest.
    /// \param data Pointer to the input bytes.
    /// \param length Number of bytes to verify.
    /// \param expected Expected digest value.
    /// \return True if computed digest matches expected.
    GP_NODISCARD static constexpr bool Verify(const char* data, SizeT length, const BLAKE3Digest& expected) noexcept
    {
        return Hash(data, length) == expected;
    }
};

}   // namespace GP::Crypto
