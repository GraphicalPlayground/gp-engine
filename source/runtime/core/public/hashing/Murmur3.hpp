// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashingTypes.hpp"
#include "hashing/HashMix.hpp"
#include "hashing/StreamingHasher.hpp"
#include <bit>

namespace gp::hashing
{

/// @brief Streaming and `constexpr` MurmurHash3 x86 32-bit hash accumulator.
/// @details Buffers partial 4-byte blocks across `update()` calls so streaming arbitrary chunk boundaries matches a
///          single contiguous `hash()` call bit-for-bit.
class Murmur3_32 final : public StreamingHasherBase<Murmur3_32, gp::UInt32, 0u, HashAlgorithm::Murmur3_32>
{
    friend class StreamingHasherBase<Murmur3_32, gp::UInt32, 0u, HashAlgorithm::Murmur3_32>;

public:
    /// @brief Constructs a 32-bit MurmurHash3 accumulator seeded with @p seed.
    /// @param[in] seed Initial 32-bit seed (defaults to `0u`).
    [[nodiscard]] constexpr explicit Murmur3_32(const gp::UInt32 seed = 0u) noexcept
        : m_hash(seed)
    {}

private:
    static constexpr gp::UInt32 kC1 = 0xCC9E2D51u;
    static constexpr gp::UInt32 kC2 = 0x1B873593u;
    static constexpr gp::UInt32 kAdd = 0xE6546B64u;

    static constexpr gp::UInt32 mixBlock(gp::UInt32 hash, gp::UInt32 k1) noexcept
    {
        k1 *= kC1;
        k1 = std::rotl(k1, 15);
        k1 *= kC2;

        hash ^= k1;
        hash = std::rotl(hash, 13);
        return (hash * 5u) + kAdd;
    }

    constexpr void resetImpl(const gp::UInt32 seed) noexcept
    {
        m_hash = seed;
        m_totalLength = 0u;
        m_tailLength = 0u;
    }

    constexpr void updateBytesImpl(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        m_totalLength += sizeInBytes;
        gp::USize offset = 0u;

        if (m_tailLength > 0u)
        {
            const gp::USize needed = 4u - m_tailLength;
            const gp::USize take = (sizeInBytes < needed) ? sizeInBytes : needed;
            detail::copyBytes(m_tail + m_tailLength, data, take);
            m_tailLength += take;
            offset += take;

            if (m_tailLength == 4u)
            {
                m_hash = mixBlock(m_hash, detail::readLittleEndian32(m_tail));
                m_tailLength = 0u;
            }
        }

        while (offset + 4u <= sizeInBytes)
        {
            m_hash = mixBlock(m_hash, detail::readLittleEndian32(data + offset));
            offset += 4u;
        }

        if (offset < sizeInBytes)
        {
            m_tailLength = sizeInBytes - offset;
            detail::copyBytes(m_tail, data + offset, m_tailLength);
        }
    }

    [[nodiscard]] constexpr gp::UInt32 finalizeImpl() const noexcept
    {
        gp::UInt32 h = m_hash;
        if (m_tailLength > 0u)
        {
            gp::UInt32 k1 = 0u;
            for (gp::USize i = m_tailLength; i > 0u; --i)
            {
                k1 = (k1 << 8u) | static_cast<gp::UInt32>(m_tail[i - 1u]);
            }
            k1 *= kC1;
            k1 = std::rotl(k1, 15);
            k1 *= kC2;
            h ^= k1;
        }
        h ^= static_cast<gp::UInt32>(m_totalLength);
        return avalancheMix32(h);
    }

private:
    gp::UInt32 m_hash{ 0u };
    gp::USize m_totalLength{ 0u };
    gp::UInt8 m_tail[4]{};
    gp::USize m_tailLength{ 0u };
};

/// @brief Streaming and `constexpr` MurmurHash3 x64 128-bit digest accumulator producing `Hash128`.
/// @details Modeled after Decima-style 128-bit content and path digests. Buffers partial 16-byte blocks across
///          `update()` calls so incremental streaming produces the exact same `Hash128` as a single contiguous pass.
class Murmur3_128 final : public StreamingHasherBase<Murmur3_128, Hash128, Hash128{}, HashAlgorithm::Murmur3_128>
{
    friend class StreamingHasherBase<Murmur3_128, Hash128, Hash128{}, HashAlgorithm::Murmur3_128>;

public:
    using StreamingHasherBase::hash;
    using StreamingHasherBase::hashIgnoreCase;
    using StreamingHasherBase::reset;

    /// @brief Constructs a 128-bit MurmurHash3 accumulator from a 128-bit seed `{ low, high }`.
    /// @param[in] seed Initial 128-bit seed (defaults to `{ 0, 0 }`).
    [[nodiscard]] constexpr explicit Murmur3_128(const Hash128& seed = Hash128{}) noexcept
        : m_h1(seed.low)
        , m_h2(seed.high)
    {}

    /// @brief Constructs a 128-bit MurmurHash3 accumulator from a single 64-bit seed applied to both lanes.
    /// @details Matches Appleby's canonical `MurmurHash3_x64_128(key, len, seed)` where `h1 = h2 = seed`.
    /// @param[in] seed 64-bit seed replicated into both 64-bit lanes.
    [[nodiscard]] constexpr explicit Murmur3_128(const gp::UInt64 seed) noexcept
        : m_h1(seed)
        , m_h2(seed)
    {}

    /// @brief Resets the accumulator using a single 64-bit seed replicated into both 64-bit lanes.
    /// @param[in] seed 64-bit seed value.
    constexpr void reset(const gp::UInt64 seed) noexcept
    {
        resetImpl(Hash128(seed, seed));
    }

private:
    static constexpr gp::UInt64 kC1 = 0x87C37B91114253D5ULL;
    static constexpr gp::UInt64 kC2 = 0x4CF5AD432745937FULL;

    constexpr void processBlock(const gp::UInt8* const block) noexcept
    {
        gp::UInt64 k1 = detail::readLittleEndian64(block);
        gp::UInt64 k2 = detail::readLittleEndian64(block + 8u);

        k1 *= kC1;
        k1 = std::rotl(k1, 31);
        k1 *= kC2;
        m_h1 ^= k1;

        m_h1 = std::rotl(m_h1, 27);
        m_h1 += m_h2;
        m_h1 = (m_h1 * 5ULL) + 0x52DCE729ULL;

        k2 *= kC2;
        k2 = std::rotl(k2, 33);
        k2 *= kC1;
        m_h2 ^= k2;

        m_h2 = std::rotl(m_h2, 31);
        m_h2 += m_h1;
        m_h2 = (m_h2 * 5ULL) + 0x38495AB5ULL;
    }

    constexpr void resetImpl(const Hash128& seed) noexcept
    {
        m_h1 = seed.low;
        m_h2 = seed.high;
        m_totalLength = 0u;
        m_tailLength = 0u;
    }

    constexpr void updateBytesImpl(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        m_totalLength += sizeInBytes;
        gp::USize offset = 0u;

        if (m_tailLength > 0u)
        {
            const gp::USize needed = 16u - m_tailLength;
            const gp::USize take = (sizeInBytes < needed) ? sizeInBytes : needed;
            detail::copyBytes(m_tail + m_tailLength, data, take);
            m_tailLength += take;
            offset += take;

            if (m_tailLength == 16u)
            {
                processBlock(m_tail);
                m_tailLength = 0u;
            }
        }

        while (offset + 16u <= sizeInBytes)
        {
            processBlock(data + offset);
            offset += 16u;
        }

        if (offset < sizeInBytes)
        {
            m_tailLength = sizeInBytes - offset;
            detail::copyBytes(m_tail, data + offset, m_tailLength);
        }
    }

    [[nodiscard]] constexpr Hash128 finalizeImpl() const noexcept
    {
        gp::UInt64 h1 = m_h1;
        gp::UInt64 h2 = m_h2;

        if (m_tailLength > 0u)
        {
            gp::UInt64 k1 = 0ULL;
            gp::UInt64 k2 = 0ULL;

            for (gp::USize i = m_tailLength; i > 0u; --i)
            {
                const gp::USize idx = i - 1u;
                const gp::UInt64 byteVal = static_cast<gp::UInt64>(m_tail[idx]);
                if (idx >= 8u)
                {
                    k2 ^= byteVal << ((idx - 8u) * 8u);
                }
                else
                {
                    k1 ^= byteVal << (idx * 8u);
                }
            }

            if (m_tailLength > 8u)
            {
                k2 *= kC2;
                k2 = std::rotl(k2, 33);
                k2 *= kC1;
                h2 ^= k2;
            }

            k1 *= kC1;
            k1 = std::rotl(k1, 31);
            k1 *= kC2;
            h1 ^= k1;
        }

        h1 ^= static_cast<gp::UInt64>(m_totalLength);
        h2 ^= static_cast<gp::UInt64>(m_totalLength);

        h1 += h2;
        h2 += h1;

        h1 = avalancheMix64(h1);
        h2 = avalancheMix64(h2);

        h1 += h2;
        h2 += h1;

        return Hash128(h1, h2);
    }

private:
    gp::UInt64 m_h1{ 0ULL };
    gp::UInt64 m_h2{ 0ULL };
    gp::USize m_totalLength{ 0u };
    gp::UInt8 m_tail[16]{};
    gp::USize m_tailLength{ 0u };
};

/// @brief Algorithm traits specialization mapping `HashAlgorithm::Murmur3_32` to `Murmur3_32`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::Murmur3_32>
{
    using Type = Murmur3_32;
};

/// @brief Algorithm traits specialization mapping `HashAlgorithm::Murmur3_128` to `Murmur3_128`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::Murmur3_128>
{
    using Type = Murmur3_128;
};

}   // namespace gp::hashing
