// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "hashing/HashingEnums.hpp"
#include "hashing/HashingForward.hpp"   // IWYU pragma: keep
#include "hashing/HashingTypes.hpp"
#include "hashing/StreamingHasher.hpp"

namespace gp::hashing
{

/// @brief Streaming and `constexpr` FNV-1a (Fowler-Noll-Vo alternate) byte-wise hash family.
/// @details FNV-1a XORs each byte before multiplying by the prime, which gives better short-string dispersion than
///          FNV-1. Because there is no block buffer, every byte mixes immediately in one cycle of XOR + multiply,
///          making it compact for compile-time literals, `gp::NamePool`, and `switch` dispatch.
/// @tparam InDigestType Unsigned digest width (`gp::UInt32` or `gp::UInt64`).
/// @tparam InOffsetBasis Canonical IETF FNV-1a offset basis constant.
/// @tparam InPrime Canonical IETF FNV-1a prime multiplier constant.
template <typename InDigestType, InDigestType InOffsetBasis, InDigestType InPrime>
class BasicFnv1a final
    : public StreamingHasherBase<
          BasicFnv1a<InDigestType, InOffsetBasis, InPrime>,
          InDigestType,
          InOffsetBasis,
          (sizeof(InDigestType) == 4u) ? HashAlgorithm::Fnv1a32 : HashAlgorithm::Fnv1a64>
{
    friend class StreamingHasherBase<
        BasicFnv1a<InDigestType, InOffsetBasis, InPrime>,
        InDigestType,
        InOffsetBasis,
        (sizeof(InDigestType) == 4u) ? HashAlgorithm::Fnv1a32 : HashAlgorithm::Fnv1a64>;

public:
    /// @brief Canonical IETF FNV-1a initial offset basis constant.
    static constexpr InDigestType kOffsetBasis = InOffsetBasis;

    /// @brief Canonical IETF FNV-1a prime multiplier constant.
    static constexpr InDigestType kPrime = InPrime;

public:
    /// @brief Constructs an FNV-1a accumulator seeded with @p seed.
    /// @param[in] seed Initial accumulator state (defaults to `kOffsetBasis`).
    [[nodiscard]] constexpr explicit BasicFnv1a(const InDigestType seed = kOffsetBasis) noexcept
        : m_state(seed)
    {}

private:
    constexpr void resetImpl(const InDigestType seed) noexcept
    {
        m_state = seed;
    }

    constexpr void updateBytesImpl(const gp::UInt8* const data, const gp::USize sizeInBytes) noexcept
    {
        InDigestType h = m_state;
        for (gp::USize i = 0u; i < sizeInBytes; ++i)
        {
            h ^= static_cast<InDigestType>(data[i]);
            h *= kPrime;
        }
        m_state = h;
    }

    [[nodiscard]] constexpr InDigestType finalizeImpl() const noexcept
    {
        return m_state;
    }

private:
    InDigestType m_state{ kOffsetBasis };
};

/// @brief 32-bit FNV-1a streaming and `constexpr` hasher (`0x811C9DC5` basis, `0x01000193` prime).
using Fnv1a32 = BasicFnv1a<gp::UInt32, 0x811C9DC5u, 0x01000193u>;

/// @brief 64-bit FNV-1a streaming and `constexpr` hasher (`0xCBF29CE484222325` basis, `0x100000001B3` prime).
using Fnv1a64 = BasicFnv1a<gp::UInt64, 0xCBF29CE484222325ULL, 0x00000100000001B3ULL>;

/// @brief Uppercase alias for `Fnv1a32`.
using FNV1a32 = Fnv1a32;

/// @brief Uppercase alias for `Fnv1a64`.
using FNV1a64 = Fnv1a64;

/// @brief Algorithm traits specialization mapping `HashAlgorithm::Fnv1a32` to `Fnv1a32`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::Fnv1a32>
{
    using Type = Fnv1a32;
};

/// @brief Algorithm traits specialization mapping `HashAlgorithm::Fnv1a64` to `Fnv1a64`.
template <>
struct HashAlgorithmTraits<HashAlgorithm::Fnv1a64>
{
    using Type = Fnv1a64;
};

}   // namespace gp::hashing
