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

namespace gp::hashing
{

/// @brief Streaming and `constexpr` Wyhash final4 64-bit hash accumulator.
/// @details Uses 128-bit multiply-XOR-fold mixing (`multiplyUInt64To128`) over 48-byte three-lane stripes. A 16-byte
///          overlap window preserves the bytes preceding the final stripe so arbitrary chunked `update()` calls match
///          one-shot `wyhash` bit-for-bit across all input lengths.
class Wyhash64 final : public StreamingHasherBase<Wyhash64, gp::UInt64, 0ULL, HashAlgorithm::Wyhash64>
{
    friend class StreamingHasherBase<Wyhash64, gp::UInt64, 0ULL, HashAlgorithm::Wyhash64>;

public:
    /// @brief Constructs a Wyhash64 accumulator seeded with @p seed.
    /// @param[in] seed Initial 64-bit seed (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit Wyhash64(const gp::UInt64 seed = 0ULL) noexcept
    {
        resetImpl(seed);
    }

private:
    static constexpr gp::UInt64 kWyp0 = 0x2D358DCCAA6C78A5ULL;
    static constexpr gp::UInt64 kWyp1 = 0x8BB84B93962EACC9ULL;
    static constexpr gp::UInt64 kWyp2 = 0x4B33A62ED433D4A3ULL;
    static constexpr gp::UInt64 kWyp3 = 0x4D5A2DA51DE1AA47ULL;

    static GP_FORCEINLINE_HINT constexpr void wymum(gp::UInt64& a, gp::UInt64& b) noexcept
    {
        gp::UInt64 low = 0ULL;
        gp::UInt64 high = 0ULL;
        detail::multiplyUInt64To128(a, b, low, high);
        a = low;
        b = high;
    }

    [[nodiscard]] static GP_FORCEINLINE_HINT constexpr gp::UInt64 wymix(gp::UInt64 a, gp::UInt64 b) noexcept
    {
        wymum(a, b);
        return a ^ b;
    }

    [[nodiscard]] static GP_FORCEINLINE_HINT constexpr gp::UInt64
        readShort3(const gp::UInt8* const ptr, const gp::USize len) noexcept
    {
        return (static_cast<gp::UInt64>(ptr[0]) << 16u) | (static_cast<gp::UInt64>(ptr[len >> 1u]) << 8u) |
               static_cast<gp::UInt64>(ptr[len - 1u]);
    }

    constexpr void processStripe(const gp::UInt8* const stripe) noexcept
    {
        m_seed = wymix(detail::readLittleEndian64(stripe) ^ kWyp1, detail::readLittleEndian64(stripe + 8u) ^ m_seed);
        m_see1 =
            wymix(detail::readLittleEndian64(stripe + 16u) ^ kWyp2, detail::readLittleEndian64(stripe + 24u) ^ m_see1);
        m_see2 =
            wymix(detail::readLittleEndian64(stripe + 32u) ^ kWyp3, detail::readLittleEndian64(stripe + 40u) ^ m_see2);
        detail::copyBytes(m_prevTail, stripe + 32u, 16u);
    }

    constexpr void resetImpl(const gp::UInt64 seed) noexcept
    {
        m_seed = seed ^ wymix(seed ^ kWyp0, kWyp1);
        m_see1 = m_seed;
        m_see2 = m_seed;
        m_totalLength = 0ULL;
        m_bufferSize = 0u;
        m_hasStripe = false;
    }

    constexpr void updateBytesImpl(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        m_totalLength += static_cast<gp::UInt64>(sizeInBytes);

        // Hold up to 48 bytes in the staging buffer; a 48-byte stripe is flushed only when at least 1 byte follows.
        if (m_bufferSize + sizeInBytes <= 48u)
        {
            detail::copyBytes(m_buffer + m_bufferSize, data, sizeInBytes);
            m_bufferSize += sizeInBytes;
            return;
        }

        gp::USize offset = 0u;
        if (m_bufferSize > 0u)
        {
            const gp::USize needed = 48u - m_bufferSize;
            detail::copyBytes(m_buffer + m_bufferSize, data, needed);
            processStripe(m_buffer);
            m_hasStripe = true;
            offset += needed;
            m_bufferSize = 0u;
        }

        while (offset + 48u < sizeInBytes)
        {
            processStripe(data + offset);
            m_hasStripe = true;
            offset += 48u;
        }

        m_bufferSize = sizeInBytes - offset;
        detail::copyBytes(m_buffer, data + offset, m_bufferSize);
    }

    [[nodiscard]] constexpr gp::UInt64 finalizeImpl() const noexcept
    {
        gp::UInt64 seed = m_seed;
        gp::UInt64 a = 0ULL;
        gp::UInt64 b = 0ULL;

        if (!m_hasStripe)
        {
            const gp::USize len = static_cast<gp::USize>(m_totalLength);
            if (len <= 16u)
            {
                if (len >= 4u)
                {
                    const gp::USize midShift = (len >> 3u) << 2u;
                    a = (static_cast<gp::UInt64>(detail::readLittleEndian32(m_buffer)) << 32u) |
                        static_cast<gp::UInt64>(detail::readLittleEndian32(m_buffer + midShift));
                    b = (static_cast<gp::UInt64>(detail::readLittleEndian32(m_buffer + len - 4u)) << 32u) |
                        static_cast<gp::UInt64>(detail::readLittleEndian32(m_buffer + len - 4u - midShift));
                }
                else if (len > 0u)
                {
                    a = readShort3(m_buffer, len);
                    b = 0ULL;
                }
            }
            else
            {
                gp::USize pos = 0u;
                gp::USize remaining = len;
                while (remaining > 16u)
                {
                    seed = wymix(
                        detail::readLittleEndian64(m_buffer + pos) ^ kWyp1,
                        detail::readLittleEndian64(m_buffer + pos + 8u) ^ seed
                    );
                    pos += 16u;
                    remaining -= 16u;
                }
                a = detail::readLittleEndian64(m_buffer + pos + remaining - 16u);
                b = detail::readLittleEndian64(m_buffer + pos + remaining - 8u);
            }
        }
        else
        {
            seed = m_seed ^ m_see1 ^ m_see2;
            gp::UInt8 window[64]{};
            detail::copyBytes(window, m_prevTail, 16u);
            detail::copyBytes(window + 16u, m_buffer, m_bufferSize);

            gp::USize pos = 16u;
            gp::USize remaining = m_bufferSize;
            while (remaining > 16u)
            {
                seed = wymix(
                    detail::readLittleEndian64(window + pos) ^ kWyp1,
                    detail::readLittleEndian64(window + pos + 8u) ^ seed
                );
                pos += 16u;
                remaining -= 16u;
            }
            a = detail::readLittleEndian64(window + pos + remaining - 16u);
            b = detail::readLittleEndian64(window + pos + remaining - 8u);
        }

        a ^= kWyp1;
        b ^= seed;
        wymum(a, b);
        return wymix(a ^ kWyp0 ^ m_totalLength, b ^ kWyp1);
    }

private:
    gp::UInt64 m_seed{ 0ULL };
    gp::UInt64 m_see1{ 0ULL };
    gp::UInt64 m_see2{ 0ULL };
    gp::UInt64 m_totalLength{ 0ULL };
    gp::UInt8 m_buffer[48]{};
    gp::UInt8 m_prevTail[16]{};
    gp::USize m_bufferSize{ 0u };
    bool m_hasStripe{ false };
};

/// @brief Algorithm traits specialization mapping `HashAlgorithm::Wyhash64` to `Wyhash64`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::Wyhash64>
{
    using Type = Wyhash64;
};

}   // namespace gp::hashing
