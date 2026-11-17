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

/// @brief Streaming and `constexpr` XXH32 32-bit hash accumulator (Cyan4973 xxHash32 specification).
/// @details Maintains four independent 32-bit accumulators over 16-byte stripes for high throughput on asset chunks,
///          network packets, and bulk payloads while producing identical output to single-pass `xxHash32`.
class XxHash32 final : public StreamingHasherBase<XxHash32, gp::UInt32, 0u, HashAlgorithm::XxHash32>
{
    friend class StreamingHasherBase<XxHash32, gp::UInt32, 0u, HashAlgorithm::XxHash32>;

public:
    /// @brief Constructs an XXH32 accumulator seeded with @p seed.
    /// @param[in] seed Initial 32-bit seed (defaults to `0u`).
    [[nodiscard]] constexpr explicit XxHash32(const gp::UInt32 seed = 0u) noexcept
        : m_seed(seed)
        , m_v1(seed + kPrime1 + kPrime2)
        , m_v2(seed + kPrime2)
        , m_v3(seed)
        , m_v4(seed - kPrime1)
    {}

private:
    static constexpr gp::UInt32 kPrime1 = 0x9E3779B1u;
    static constexpr gp::UInt32 kPrime2 = 0x85EBCA77u;
    static constexpr gp::UInt32 kPrime3 = 0xC2B2AE3Du;
    static constexpr gp::UInt32 kPrime4 = 0x27D4EB2Fu;
    static constexpr gp::UInt32 kPrime5 = 0x165667B1u;

    [[nodiscard]] static constexpr gp::UInt32 round(gp::UInt32 acc, const gp::UInt32 input) noexcept
    {
        acc += input * kPrime2;
        acc = std::rotl(acc, 13);
        acc *= kPrime1;
        return acc;
    }

    constexpr void processStripe(const gp::UInt8* const stripe) noexcept
    {
        m_v1 = round(m_v1, detail::readLittleEndian32(stripe));
        m_v2 = round(m_v2, detail::readLittleEndian32(stripe + 4u));
        m_v3 = round(m_v3, detail::readLittleEndian32(stripe + 8u));
        m_v4 = round(m_v4, detail::readLittleEndian32(stripe + 12u));
    }

    constexpr void resetImpl(const gp::UInt32 seed) noexcept
    {
        m_seed = seed;
        m_v1 = seed + kPrime1 + kPrime2;
        m_v2 = seed + kPrime2;
        m_v3 = seed;
        m_v4 = seed - kPrime1;
        m_totalLength = 0ULL;
        m_bufferSize = 0u;
    }

    constexpr void updateBytesImpl(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        m_totalLength += static_cast<gp::UInt64>(sizeInBytes);
        gp::USize offset = 0u;

        if (m_bufferSize + sizeInBytes < 16u)
        {
            detail::copyBytes(m_buffer + m_bufferSize, data, sizeInBytes);
            m_bufferSize += sizeInBytes;
            return;
        }

        if (m_bufferSize > 0u)
        {
            const gp::USize needed = 16u - m_bufferSize;
            detail::copyBytes(m_buffer + m_bufferSize, data, needed);
            processStripe(m_buffer);
            offset += needed;
            m_bufferSize = 0u;
        }

        while (offset + 16u <= sizeInBytes)
        {
            processStripe(data + offset);
            offset += 16u;
        }

        if (offset < sizeInBytes)
        {
            m_bufferSize = sizeInBytes - offset;
            detail::copyBytes(m_buffer, data + offset, m_bufferSize);
        }
    }

    [[nodiscard]] constexpr gp::UInt32 finalizeImpl() const noexcept
    {
        gp::UInt32 h = 0u;
        if (m_totalLength >= 16ULL)
        {
            h = std::rotl(m_v1, 1) + std::rotl(m_v2, 7) + std::rotl(m_v3, 12) + std::rotl(m_v4, 18);
        }
        else
        {
            h = m_seed + kPrime5;
        }

        h += static_cast<gp::UInt32>(m_totalLength);

        gp::USize idx = 0u;
        while (idx + 4u <= m_bufferSize)
        {
            h += detail::readLittleEndian32(m_buffer + idx) * kPrime3;
            h = std::rotl(h, 17) * kPrime4;
            idx += 4u;
        }

        while (idx < m_bufferSize)
        {
            h += static_cast<gp::UInt32>(m_buffer[idx]) * kPrime5;
            h = std::rotl(h, 11) * kPrime1;
            ++idx;
        }

        h ^= h >> 15u;
        h *= kPrime2;
        h ^= h >> 13u;
        h *= kPrime3;
        h ^= h >> 16u;
        return h;
    }

private:
    gp::UInt32 m_seed{ 0u };
    gp::UInt32 m_v1{ 0u };
    gp::UInt32 m_v2{ 0u };
    gp::UInt32 m_v3{ 0u };
    gp::UInt32 m_v4{ 0u };
    gp::UInt64 m_totalLength{ 0ULL };
    gp::UInt8 m_buffer[16]{};
    gp::USize m_bufferSize{ 0u };
};

/// @brief Streaming and `constexpr` XXH64 64-bit hash accumulator (Cyan4973 xxHash64 specification).
/// @details Processes 32-byte stripes across four independent 64-bit accumulators. Well suited for large binary
///          blobs, shader bytecode, cooked archives, and multi-chunk asset verification.
class XxHash64 final : public StreamingHasherBase<XxHash64, gp::UInt64, 0ULL, HashAlgorithm::XxHash64>
{
    friend class StreamingHasherBase<XxHash64, gp::UInt64, 0ULL, HashAlgorithm::XxHash64>;

public:
    /// @brief Constructs an XXH64 accumulator seeded with @p seed.
    /// @param[in] seed Initial 64-bit seed (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit XxHash64(const gp::UInt64 seed = 0ULL) noexcept
        : m_seed(seed)
        , m_v1(seed + kPrime1 + kPrime2)
        , m_v2(seed + kPrime2)
        , m_v3(seed)
        , m_v4(seed - kPrime1)
    {}

private:
    static constexpr gp::UInt64 kPrime1 = 0x9E3779B185EBCA87ULL;
    static constexpr gp::UInt64 kPrime2 = 0xC2B2AE3D27D4EB4FULL;
    static constexpr gp::UInt64 kPrime3 = 0x165667B19E3779F9ULL;
    static constexpr gp::UInt64 kPrime4 = 0x85EBCA77C2B2AE63ULL;
    static constexpr gp::UInt64 kPrime5 = 0x27D4EB2F165667C5ULL;

    [[nodiscard]] static constexpr gp::UInt64 round(gp::UInt64 acc, const gp::UInt64 input) noexcept
    {
        acc += input * kPrime2;
        acc = std::rotl(acc, 31);
        acc *= kPrime1;
        return acc;
    }

    [[nodiscard]] static constexpr gp::UInt64 mergeRound(gp::UInt64 acc, const gp::UInt64 val) noexcept
    {
        acc ^= round(0ULL, val);
        acc = (acc * kPrime1) + kPrime4;
        return acc;
    }

    constexpr void processStripe(const gp::UInt8* const stripe) noexcept
    {
        m_v1 = round(m_v1, detail::readLittleEndian64(stripe));
        m_v2 = round(m_v2, detail::readLittleEndian64(stripe + 8u));
        m_v3 = round(m_v3, detail::readLittleEndian64(stripe + 16u));
        m_v4 = round(m_v4, detail::readLittleEndian64(stripe + 24u));
    }

    constexpr void resetImpl(const gp::UInt64 seed) noexcept
    {
        m_seed = seed;
        m_v1 = seed + kPrime1 + kPrime2;
        m_v2 = seed + kPrime2;
        m_v3 = seed;
        m_v4 = seed - kPrime1;
        m_totalLength = 0ULL;
        m_bufferSize = 0u;
    }

    constexpr void updateBytesImpl(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        m_totalLength += static_cast<gp::UInt64>(sizeInBytes);
        gp::USize offset = 0u;

        if (m_bufferSize + sizeInBytes < 32u)
        {
            detail::copyBytes(m_buffer + m_bufferSize, data, sizeInBytes);
            m_bufferSize += sizeInBytes;
            return;
        }

        if (m_bufferSize > 0u)
        {
            const gp::USize needed = 32u - m_bufferSize;
            detail::copyBytes(m_buffer + m_bufferSize, data, needed);
            processStripe(m_buffer);
            offset += needed;
            m_bufferSize = 0u;
        }

        while (offset + 32u <= sizeInBytes)
        {
            processStripe(data + offset);
            offset += 32u;
        }

        if (offset < sizeInBytes)
        {
            m_bufferSize = sizeInBytes - offset;
            detail::copyBytes(m_buffer, data + offset, m_bufferSize);
        }
    }

    [[nodiscard]] constexpr gp::UInt64 finalizeImpl() const noexcept
    {
        gp::UInt64 h = 0ULL;
        if (m_totalLength >= 32ULL)
        {
            h = std::rotl(m_v1, 1) + std::rotl(m_v2, 7) + std::rotl(m_v3, 12) + std::rotl(m_v4, 18);
            h = mergeRound(h, m_v1);
            h = mergeRound(h, m_v2);
            h = mergeRound(h, m_v3);
            h = mergeRound(h, m_v4);
        }
        else
        {
            h = m_seed + kPrime5;
        }

        h += m_totalLength;

        gp::USize idx = 0u;
        while (idx + 8u <= m_bufferSize)
        {
            h ^= round(0ULL, detail::readLittleEndian64(m_buffer + idx));
            h = (std::rotl(h, 27) * kPrime1) + kPrime4;
            idx += 8u;
        }

        if (idx + 4u <= m_bufferSize)
        {
            h ^= static_cast<gp::UInt64>(detail::readLittleEndian32(m_buffer + idx)) * kPrime1;
            h = (std::rotl(h, 23) * kPrime2) + kPrime3;
            idx += 4u;
        }

        while (idx < m_bufferSize)
        {
            h ^= static_cast<gp::UInt64>(m_buffer[idx]) * kPrime5;
            h = std::rotl(h, 11) * kPrime1;
            ++idx;
        }

        h ^= h >> 33u;
        h *= kPrime2;
        h ^= h >> 29u;
        h *= kPrime3;
        h ^= h >> 32u;
        return h;
    }

private:
    gp::UInt64 m_seed{ 0ULL };
    gp::UInt64 m_v1{ 0ULL };
    gp::UInt64 m_v2{ 0ULL };
    gp::UInt64 m_v3{ 0ULL };
    gp::UInt64 m_v4{ 0ULL };
    gp::UInt64 m_totalLength{ 0ULL };
    gp::UInt8 m_buffer[32]{};
    gp::USize m_bufferSize{ 0u };
};

/// @brief Algorithm traits specialization mapping `HashAlgorithm::XxHash32` to `XxHash32`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::XxHash32>
{
    using Type = XxHash32;
};

/// @brief Algorithm traits specialization mapping `HashAlgorithm::XxHash64` to `XxHash64`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::XxHash64>
{
    using Type = XxHash64;
};

}   // namespace gp::hashing
