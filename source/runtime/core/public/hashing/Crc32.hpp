// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashingTypes.hpp"
#include "hashing/StreamingHasher.hpp"
#include <type_traits>   // IWYU pragma: keep

#if GP_ARCHITECTURE_X64 && defined(__SSE4_2__)
    #include <nmmintrin.h>
#endif

namespace gp::hashing
{

namespace detail
{

/// @brief Builds the 8x256 Slicing-by-8 CRC-32 lookup table at compile time.
/// @tparam InReflectedPolynomial Reflected 32-bit CRC generator polynomial.
template <gp::UInt32 InReflectedPolynomial>
struct Crc32Tables
{
    gp::UInt32 slices[8][256]{};

    /// @brief Populates slice 0 (standard byte table) and derives slices 1..7 for 8-byte parallel folding.
    consteval Crc32Tables() noexcept
    {
        for (gp::UInt32 i = 0u; i < 256u; ++i)
        {
            gp::UInt32 crc = i;
            for (gp::UInt32 bit = 0u; bit < 8u; ++bit)
            {
                if ((crc & 1u) != 0u)
                {
                    crc = (crc >> 1u) ^ InReflectedPolynomial;
                }
                else
                {
                    crc >>= 1u;
                }
            }
            slices[0][i] = crc;
        }

        for (gp::UInt32 slice = 1u; slice < 8u; ++slice)
        {
            for (gp::UInt32 i = 0u; i < 256u; ++i)
            {
                const gp::UInt32 prev = slices[slice - 1u][i];
                slices[slice][i] = (prev >> 8u) ^ slices[0][prev & 0xFFu];
            }
        }
    }
};

}   // namespace detail

/// @brief Streaming and `constexpr` 32-bit Cyclic Redundancy Check using `consteval` Slicing-by-8 tables.
/// @details Parameterized by the reflected generator polynomial so IEEE 802.3 (`Crc32`, `0xEDB88320`) and Castagnoli
///          (`Crc32C`, `0x82F63B78`) share one implementation. Passing a previous digest to `reset(prevCrc)` or the
///          constructor continues a multi-chunk checksum across chunks without re-initialization, matching zlib
///          `crc32` and Unreal `FCrc::MemCrc32`.
/// @tparam InReflectedPolynomial Reflected 32-bit CRC polynomial.
template <gp::UInt32 InReflectedPolynomial>
class BasicCrc32 final
    : public StreamingHasherBase<
          BasicCrc32<InReflectedPolynomial>,
          gp::UInt32,
          0u,
          (InReflectedPolynomial == 0x82F63B78u) ? HashAlgorithm::Crc32C : HashAlgorithm::Crc32>
{
    friend class StreamingHasherBase<
        BasicCrc32<InReflectedPolynomial>,
        gp::UInt32,
        0u,
        (InReflectedPolynomial == 0x82F63B78u) ? HashAlgorithm::Crc32C : HashAlgorithm::Crc32>;

public:
    /// @brief Reflected 32-bit generator polynomial.
    static constexpr gp::UInt32 kPolynomial = InReflectedPolynomial;

public:
    /// @brief Constructs a CRC-32 accumulator initialized from a prior CRC value (`0` for a fresh stream).
    /// @param[in] initialCrc Prior CRC digest to continue from, or `0u` to start fresh.
    [[nodiscard]] constexpr explicit BasicCrc32(const gp::UInt32 initialCrc = 0u) noexcept
        : m_state(~initialCrc)
    {}

private:
    static constexpr detail::Crc32Tables<InReflectedPolynomial> kTables{};

    constexpr void resetImpl(const gp::UInt32 initialCrc) noexcept
    {
        m_state = ~initialCrc;
    }

    constexpr void updateBytesImpl(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        gp::UInt32 crc = m_state;
        gp::USize offset = 0u;

#if GP_ARCHITECTURE_X64 && defined(__SSE4_2__)
        if constexpr (InReflectedPolynomial == 0x82F63B78u)
        {
            if (!std::is_constant_evaluated())
            {
                gp::UInt64 crc64 = static_cast<gp::UInt64>(crc);
                while (offset + 8u <= sizeInBytes)
                {
                    const gp::UInt64 word = detail::readLittleEndian64(data + offset);
                    crc64 = _mm_crc32_u64(crc64, word);
                    offset += 8u;
                }
                crc = static_cast<gp::UInt32>(crc64);
                while (offset < sizeInBytes)
                {
                    crc = _mm_crc32_u8(crc, data[offset]);
                    ++offset;
                }
                m_state = crc;
                return;
            }
        }
#endif

        // Slicing-by-8 processes 8 bytes per iteration with independent table lookups to break serial dependencies.
        while (offset + 8u <= sizeInBytes)
        {
            const gp::UInt32 one = detail::readLittleEndian32(data + offset) ^ crc;
            const gp::UInt32 two = detail::readLittleEndian32(data + offset + 4u);
            crc = kTables.slices[7][one & 0xFFu] ^ kTables.slices[6][(one >> 8u) & 0xFFu] ^
                  kTables.slices[5][(one >> 16u) & 0xFFu] ^ kTables.slices[4][(one >> 24u) & 0xFFu] ^
                  kTables.slices[3][two & 0xFFu] ^ kTables.slices[2][(two >> 8u) & 0xFFu] ^
                  kTables.slices[1][(two >> 16u) & 0xFFu] ^ kTables.slices[0][(two >> 24u) & 0xFFu];
            offset += 8u;
        }

        while (offset < sizeInBytes)
        {
            crc = (crc >> 8u) ^ kTables.slices[0][(crc ^ static_cast<gp::UInt32>(data[offset])) & 0xFFu];
            ++offset;
        }

        m_state = crc;
    }

    [[nodiscard]] constexpr gp::UInt32 finalizeImpl() const noexcept
    {
        return ~m_state;
    }

private:
    gp::UInt32 m_state{ 0xFFFFFFFFu };
};

/// @brief Standard IEEE 802.3 CRC-32 checksum (`0xEDB88320`, matches zlib/PNG/ZIP).
using Crc32 = BasicCrc32<0xEDB88320u>;

/// @brief Castagnoli CRC-32C checksum (`0x82F63B78`, hardware-accelerated when SSE4.2 is enabled).
using Crc32C = BasicCrc32<0x82F63B78u>;

/// @brief Uppercase alias for `Crc32`.
using CRC32 = Crc32;

/// @brief Uppercase alias for `Crc32C`.
using CRC32C = Crc32C;

/// @brief Algorithm traits specialization mapping `HashAlgorithm::Crc32` to `Crc32`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::Crc32>
{
    using Type = Crc32;
};

/// @brief Algorithm traits specialization mapping `HashAlgorithm::Crc32C` to `Crc32C`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::Crc32C>
{
    using Type = Crc32C;
};

}   // namespace gp::hashing
