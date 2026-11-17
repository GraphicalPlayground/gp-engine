// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashingTypes.hpp"
#include "hashing/StreamingHasher.hpp"
#include <bit>

namespace gp::hashing
{

/// @brief Streaming and `constexpr` SipHash-2-4 keyed 64-bit pseudorandom function (Aumasson & Bernstein).
/// @details Designed for hash tables exposed to untrusted input (network packets, user-generated content, HTTP headers)
///          where attackers could otherwise craft colliding keys for FNV-1a or MurmurHash3 (HashDoS). Accepts a 128-bit
///          secret key (`k0`, `k1`) or a single 64-bit seed and streams 8-byte blocks cleanly.
class SipHash24 final : public StreamingHasherBase<SipHash24, gp::UInt64, 0ULL, HashAlgorithm::SipHash24>
{
    friend class StreamingHasherBase<SipHash24, gp::UInt64, 0ULL, HashAlgorithm::SipHash24>;

public:
    using StreamingHasherBase::hash;
    using StreamingHasherBase::hashIgnoreCase;

    /// @brief Constructs a SipHash-2-4 accumulator from a single 64-bit seed (`k0 = seed`, `k1 = 0`).
    /// @param[in] seed Initial 64-bit key low lane (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit SipHash24(const gp::UInt64 seed = 0ULL) noexcept
    {
        resetKey(seed, 0ULL);
    }

    /// @brief Constructs a SipHash-2-4 accumulator with an explicit 128-bit key (`k0`, `k1`).
    /// @param[in] k0 Lower 64 bits of the 128-bit secret key.
    /// @param[in] k1 Upper 64 bits of the 128-bit secret key.
    [[nodiscard]] constexpr SipHash24(const gp::UInt64 k0, const gp::UInt64 k1) noexcept
    {
        resetKey(k0, k1);
    }

    /// @brief Constructs a SipHash-2-4 accumulator from a `Hash128` secret key.
    /// @param[in] key 128-bit secret key (`key.low` -> `k0`, `key.high` -> `k1`).
    [[nodiscard]] constexpr explicit SipHash24(const Hash128& key) noexcept
    {
        resetKey(key.low, key.high);
    }

    /// @brief Resets the accumulator with an explicit 128-bit secret key (`k0`, `k1`).
    /// @param[in] k0 Lower 64 bits of the key.
    /// @param[in] k1 Upper 64 bits of the key.
    constexpr void resetKey(const gp::UInt64 k0, const gp::UInt64 k1) noexcept
    {
        m_v0 = 0x736F6D6570736575ULL ^ k0;
        m_v1 = 0x646F72616E646F6DULL ^ k1;
        m_v2 = 0x6C7967656E657261ULL ^ k0;
        m_v3 = 0x7465646279746573ULL ^ k1;
        m_totalLength = 0ULL;
        m_tailLength = 0u;
    }

    /// @brief Resets the accumulator with a `Hash128` secret key.
    /// @param[in] key 128-bit secret key.
    constexpr void resetKey(const Hash128& key) noexcept
    {
        resetKey(key.low, key.high);
    }

    /// @brief Computes a one-shot keyed SipHash-2-4 digest over a `gp::UInt8` buffer (`constexpr`-capable).
    /// @param[in] k0 Lower 64 bits of the secret key.
    /// @param[in] k1 Upper 64 bits of the secret key.
    /// @param[in] data Source byte pointer.
    /// @param[in] sizeInBytes Number of bytes to hash.
    /// @return 64-bit keyed SipHash-2-4 digest.
    [[nodiscard]] static constexpr gp::UInt64 hashKeyed(
        const gp::UInt64 k0, const gp::UInt64 k1, const gp::UInt8* const data, const gp::USize sizeInBytes
    ) noexcept
    {
        SipHash24 hasher(k0, k1);
        hasher.update(data, sizeInBytes);
        return hasher.finalize();
    }

    /// @brief Computes a one-shot keyed SipHash-2-4 digest over a `Hash128`-keyed `gp::UInt8` buffer.
    /// @param[in] key 128-bit secret key.
    /// @param[in] data Source byte pointer.
    /// @param[in] sizeInBytes Number of bytes to hash.
    /// @return 64-bit keyed SipHash-2-4 digest.
    [[nodiscard]] static constexpr gp::UInt64
        hashKeyed(const Hash128& key, const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        return hashKeyed(key.low, key.high, data, sizeInBytes);
    }

    /// @brief Computes a one-shot keyed SipHash-2-4 digest over a string-like container or view.
    /// @tparam InStrT String type satisfying `concepts::IsCharacterSequence`.
    /// @param[in] key 128-bit secret key.
    /// @param[in] str Character sequence to hash.
    /// @return 64-bit keyed SipHash-2-4 digest.
    template <concepts::IsCharacterSequence InStrT>
    [[nodiscard]] static constexpr gp::UInt64 hashKeyed(const Hash128& key, const InStrT& str) noexcept
    {
        SipHash24 hasher(key);
        hasher.update(str);
        return hasher.finalize();
    }

private:
    static GP_FORCEINLINE_HINT constexpr void
        sipRound(gp::UInt64& v0, gp::UInt64& v1, gp::UInt64& v2, gp::UInt64& v3) noexcept
    {
        v0 += v1;
        v1 = std::rotl(v1, 13);
        v1 ^= v0;
        v0 = std::rotl(v0, 32);
        v2 += v3;
        v3 = std::rotl(v3, 16);
        v3 ^= v2;
        v0 += v3;
        v3 = std::rotl(v3, 21);
        v3 ^= v0;
        v2 += v1;
        v1 = std::rotl(v1, 17);
        v1 ^= v2;
        v2 = std::rotl(v2, 32);
    }

    constexpr void processBlock(const gp::UInt8* const block) noexcept
    {
        const gp::UInt64 m = detail::readLittleEndian64(block);
        m_v3 ^= m;
        sipRound(m_v0, m_v1, m_v2, m_v3);
        sipRound(m_v0, m_v1, m_v2, m_v3);
        m_v0 ^= m;
    }

    constexpr void resetImpl(const gp::UInt64 seed) noexcept
    {
        resetKey(seed, 0ULL);
    }

    constexpr void updateBytesImpl(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        m_totalLength += static_cast<gp::UInt64>(sizeInBytes);
        gp::USize offset = 0u;

        if (m_tailLength > 0u)
        {
            const gp::USize needed = 8u - m_tailLength;
            const gp::USize take = (sizeInBytes < needed) ? sizeInBytes : needed;
            detail::copyBytes(m_tail + m_tailLength, data, take);
            m_tailLength += take;
            offset += take;

            if (m_tailLength == 8u)
            {
                processBlock(m_tail);
                m_tailLength = 0u;
            }
        }

        while (offset + 8u <= sizeInBytes)
        {
            processBlock(data + offset);
            offset += 8u;
        }

        if (offset < sizeInBytes)
        {
            m_tailLength = sizeInBytes - offset;
            detail::copyBytes(m_tail, data + offset, m_tailLength);
        }
    }

    [[nodiscard]] constexpr gp::UInt64 finalizeImpl() const noexcept
    {
        gp::UInt64 v0 = m_v0;
        gp::UInt64 v1 = m_v1;
        gp::UInt64 v2 = m_v2;
        gp::UInt64 v3 = m_v3;

        gp::UInt64 b = (m_totalLength & 0xFFULL) << 56u;
        for (gp::USize i = 0u; i < m_tailLength; ++i)
        {
            b |= static_cast<gp::UInt64>(m_tail[i]) << (i * 8u);
        }

        v3 ^= b;
        sipRound(v0, v1, v2, v3);
        sipRound(v0, v1, v2, v3);
        v0 ^= b;

        v2 ^= 0xFFULL;
        sipRound(v0, v1, v2, v3);
        sipRound(v0, v1, v2, v3);
        sipRound(v0, v1, v2, v3);
        sipRound(v0, v1, v2, v3);

        return v0 ^ v1 ^ v2 ^ v3;
    }

private:
    gp::UInt64 m_v0{ 0ULL };
    gp::UInt64 m_v1{ 0ULL };
    gp::UInt64 m_v2{ 0ULL };
    gp::UInt64 m_v3{ 0ULL };
    gp::UInt64 m_totalLength{ 0ULL };
    gp::UInt8 m_tail[8]{};
    gp::USize m_tailLength{ 0u };
};

/// @brief Algorithm traits specialization mapping `HashAlgorithm::SipHash24` to `SipHash24`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::SipHash24>
{
    using Type = SipHash24;
};

}   // namespace gp::hashing
