// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "hashing/HashMix.hpp"
#include "maths/random/RandomEngine.hpp"
#include "maths/random/RandomEnums.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep
#include "maths/random/RandomTypes.hpp"

namespace gp::math
{

/// @brief 64-bit SplitMix64 generator (Steele, Lea, Flood 2014) and canonical seed expander.
/// @details Advances a 64-bit Weyl sequence by `0x9E3779B97F4A7C15` and applies Stafford's Mix13 bijective mixer.
///          Occupies 8 bytes and supports O(1) arbitrary forward and backward jumps, making it suitable for tiny
///          per-entity streams and expanding a single 64-bit user seed into 128-bit or 256-bit Xoshiro states.
class SplitMix64 final : public RandomEngineBase<SplitMix64, gp::UInt64, SplitMix64State, RandomAlgorithm::SplitMix64>
{
    friend class RandomEngineBase<SplitMix64, gp::UInt64, SplitMix64State, RandomAlgorithm::SplitMix64>;

public:
    /// @brief Golden-ratio Weyl increment constant used on every step.
    static constexpr gp::UInt64 kWeylGamma = 0x9E3779B97F4A7C15ULL;

public:
    /// @brief Constructs a `SplitMix64` generator seeded with @p seedValue and optional @p streamIdValue.
    /// @param[in] seedValue Initial 64-bit seed (defaults to `kDefaultRandomSeed`).
    /// @param[in] streamIdValue Optional stream identifier (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit SplitMix64(
        const gp::UInt64 seedValue = kDefaultRandomSeed, const gp::UInt64 streamIdValue = 0ULL
    ) noexcept
    {
        seed(seedValue, streamIdValue);
    }

    /// @brief Advances or rewinds the generator by @p delta steps in O(1) time.
    /// @param[in] delta Signed step count (positive advances forward, negative rewinds backward).
    constexpr void advance(const gp::Int64 delta) noexcept
    {
        m_state.state += static_cast<gp::UInt64>(delta) * kWeylGamma;
    }

    /// @brief Jumps forward by `2^63` steps in O(1) time for parallel worker stream partitioning.
    constexpr void jump() noexcept
    {
        m_state.state += 0x8000000000000000ULL * kWeylGamma;
    }

    /// @brief Jumps forward by `2^62` steps in O(1) time.
    constexpr void longJump() noexcept
    {
        m_state.state += 0x4000000000000000ULL * kWeylGamma;
    }

private:
    constexpr void seedImpl(const gp::UInt64 seedValue, const gp::UInt64 streamIdValue) noexcept
    {
        if (streamIdValue == 0ULL)
        {
            m_state.state = seedValue;
        }
        else
        {
            m_state.state = seedValue ^ gp::hashing::splitMix64(streamIdValue + kWeylGamma);
        }
    }

    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 nextWordImpl() noexcept
    {
        m_state.state += kWeylGamma;
        return gp::hashing::splitMix64(m_state.state);
    }

    constexpr void discardImpl(const gp::UInt64 steps) noexcept
    {
        m_state.state += steps * kWeylGamma;
    }
};

/// @brief `RandomAlgorithmTraits` specialization mapping `RandomAlgorithm::SplitMix64` to `SplitMix64`.
template <>
struct RandomAlgorithmTraits<RandomAlgorithm::SplitMix64>
{
    using Type = SplitMix64;
};

}   // namespace gp::math
