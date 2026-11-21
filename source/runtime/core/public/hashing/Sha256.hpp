// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashingTypes.hpp"
#include "hashing/StreamingHasher.hpp"
#include <bit>

namespace gp::hashing
{

/// @brief Streaming and `constexpr` FIPS 180-4 SHA-256 accumulator producing a 256-bit `Hash256`.
/// @details Used for content-addressable asset caches, Derived Data Cache (DDC) keys, cooked archive integrity, and
///          patch manifests (matching Unreal IoStore / Decima content hashing). Passing the default zero `Hash256{}`
///          seed yields standard FIPS 180-4 SHA-256; passing a non-zero `Hash256` feeds its 32 bytes as an initial
///          domain-separation prefix.
class Sha256 final : public StreamingHasherBase<Sha256, Hash256, Hash256{}, HashAlgorithm::Sha256>
{
    friend class StreamingHasherBase<Sha256, Hash256, Hash256{}, HashAlgorithm::Sha256>;

public:
    /// @brief Constructs a SHA-256 accumulator with optional domain-separation seed.
    /// @param[in] seed Zero `Hash256{}` for standard FIPS 180-4 SHA-256, or non-zero prefix seed.
    [[nodiscard]] constexpr explicit Sha256(const Hash256& seed = Hash256{}) noexcept
    {
        resetImpl(seed);
    }

private:
    static constexpr gp::UInt32 kInitialState[8] = { 0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u, 0xA54FF53Au,
                                                     0x510E527Fu, 0x9B05688Cu, 0x1F83D9ABu, 0x5BE0CD19u };

    static constexpr gp::UInt32 kRoundConstants[64] = {
        0x428A2F98u, 0x71374491u, 0xB5C0FBCFu, 0xE9B5DBA5u, 0x3956C25Bu, 0x59F111F1u, 0x923F82A4u, 0xAB1C5ED5u,
        0xD807AA98u, 0x12835B01u, 0x243185BEu, 0x550C7DC3u, 0x72BE5D74u, 0x80DEB1FEu, 0x9BDC06A7u, 0xC19BF174u,
        0xE49B69C1u, 0xEFBE4786u, 0x0FC19DC6u, 0x240CA1CCu, 0x2DE92C6Fu, 0x4A7484AAu, 0x5CB0A9DCu, 0x76F988DAu,
        0x983E5152u, 0xA831C66Du, 0xB00327C8u, 0xBF597FC7u, 0xC6E00BF3u, 0xD5A79147u, 0x06CA6351u, 0x14292967u,
        0x27B70A85u, 0x2E1B2138u, 0x4D2C6DFCu, 0x53380D13u, 0x650A7354u, 0x766A0ABBu, 0x81C2C92Eu, 0x92722C85u,
        0xA2BFE8A1u, 0xA81A664Bu, 0xC24B8B70u, 0xC76C51A3u, 0xD192E819u, 0xD6990624u, 0xF40E3585u, 0x106AA070u,
        0x19A4C116u, 0x1E376C08u, 0x2748774Cu, 0x34B0BCB5u, 0x391C0CB3u, 0x4ED8AA4Au, 0x5B9CCA4Fu, 0x682E6FF3u,
        0x748F82EEu, 0x78A5636Fu, 0x84C87814u, 0x8CC70208u, 0x90BEFFFAu, 0xA4506CEBu, 0xBEF9A3F7u, 0xC67178F2u
    };

    [[nodiscard]] static constexpr gp::UInt32 readBigEndian32(const gp::UInt8* const ptr) noexcept
    {
        return (static_cast<gp::UInt32>(ptr[0]) << 24u) | (static_cast<gp::UInt32>(ptr[1]) << 16u) |
               (static_cast<gp::UInt32>(ptr[2]) << 8u) | static_cast<gp::UInt32>(ptr[3]);
    }

    static constexpr void compressBlock(gp::UInt32 (&state)[8], const gp::UInt8* const block) noexcept
    {
        gp::UInt32 w[64]{};
        for (gp::USize i = 0u; i < 16u; ++i)
        {
            w[i] = readBigEndian32(block + (i * 4u));
        }
        for (gp::USize i = 16u; i < 64u; ++i)
        {
            const gp::UInt32 s0 = std::rotr(w[i - 15u], 7) ^ std::rotr(w[i - 15u], 18) ^ (w[i - 15u] >> 3u);
            const gp::UInt32 s1 = std::rotr(w[i - 2u], 17) ^ std::rotr(w[i - 2u], 19) ^ (w[i - 2u] >> 10u);
            w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
        }

        gp::UInt32 a = state[0];
        gp::UInt32 b = state[1];
        gp::UInt32 c = state[2];
        gp::UInt32 d = state[3];
        gp::UInt32 e = state[4];
        gp::UInt32 f = state[5];
        gp::UInt32 g = state[6];
        gp::UInt32 h = state[7];

        for (gp::USize i = 0u; i < 64u; ++i)
        {
            const gp::UInt32 s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const gp::UInt32 ch = (e & f) ^ ((~e) & g);
            const gp::UInt32 temp1 = h + s1 + ch + kRoundConstants[i] + w[i];
            const gp::UInt32 s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const gp::UInt32 maj = (a & b) ^ (a & c) ^ (b & c);
            const gp::UInt32 temp2 = s0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
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

    constexpr void resetImpl(const Hash256& seed) noexcept
    {
        for (gp::USize i = 0u; i < 8u; ++i)
        {
            m_state[i] = kInitialState[i];
        }
        m_totalBytes = 0ULL;
        m_bufferLength = 0u;

        if (!seed.isZero())
        {
            gp::UInt8 seedBytes[32]{};
            for (gp::USize w = 0u; w < 4u; ++w)
            {
                for (gp::USize b = 0u; b < 8u; ++b)
                {
                    seedBytes[(w * 8u) + b] = static_cast<gp::UInt8>((seed.words[w] >> ((7u - b) * 8u)) & 0xFFULL);
                }
            }
            updateBytesImpl(seedBytes, 32u);
        }
    }

    constexpr void updateBytesImpl(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        m_totalBytes += static_cast<gp::UInt64>(sizeInBytes);
        gp::USize offset = 0u;

        if (m_bufferLength > 0u)
        {
            const gp::USize needed = 64u - m_bufferLength;
            const gp::USize take = (sizeInBytes < needed) ? sizeInBytes : needed;
            detail::copyBytes(m_buffer + m_bufferLength, data, take);
            m_bufferLength += take;
            offset += take;

            if (m_bufferLength == 64u)
            {
                compressBlock(m_state, m_buffer);
                m_bufferLength = 0u;
            }
        }

        while (offset + 64u <= sizeInBytes)
        {
            compressBlock(m_state, data + offset);
            offset += 64u;
        }

        if (offset < sizeInBytes)
        {
            m_bufferLength = sizeInBytes - offset;
            detail::copyBytes(m_buffer, data + offset, m_bufferLength);
        }
    }

    [[nodiscard]] constexpr Hash256 finalizeImpl() const noexcept
    {
        gp::UInt32 finalState[8]{};
        for (gp::USize i = 0u; i < 8u; ++i)
        {
            finalState[i] = m_state[i];
        }

        gp::UInt8 padBlock[64]{};
        detail::copyBytes(padBlock, m_buffer, m_bufferLength);
        padBlock[m_bufferLength] = 0x80u;

        if (m_bufferLength >= 56u)
        {
            compressBlock(finalState, padBlock);
            for (gp::USize i = 0u; i < 56u; ++i)
            {
                padBlock[i] = 0u;
            }
        }

        const gp::UInt64 totalBits = m_totalBytes * 8ULL;
        for (gp::USize i = 0u; i < 8u; ++i)
        {
            padBlock[56u + i] = static_cast<gp::UInt8>((totalBits >> ((7u - i) * 8u)) & 0xFFULL);
        }
        compressBlock(finalState, padBlock);

        return Hash256(
            (static_cast<gp::UInt64>(finalState[0]) << 32u) | static_cast<gp::UInt64>(finalState[1]),
            (static_cast<gp::UInt64>(finalState[2]) << 32u) | static_cast<gp::UInt64>(finalState[3]),
            (static_cast<gp::UInt64>(finalState[4]) << 32u) | static_cast<gp::UInt64>(finalState[5]),
            (static_cast<gp::UInt64>(finalState[6]) << 32u) | static_cast<gp::UInt64>(finalState[7])
        );
    }

private:
    gp::UInt32 m_state[8]{};
    gp::UInt64 m_totalBytes{ 0ULL };
    gp::UInt8 m_buffer[64]{};
    gp::USize m_bufferLength{ 0u };
};

/// @brief Algorithm traits specialization mapping `HashAlgorithm::Sha256` to `Sha256`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::Sha256>
{
    using Type = Sha256;
};

}   // namespace gp::hashing
