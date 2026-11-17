// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/random/RandomEngine.hpp"
#include "maths/random/RandomEnums.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep
#include "maths/random/RandomTypes.hpp"
#include <bit>

namespace gp::math
{

/// @brief PCG-XSH-RR 64-bit state / 32-bit output generator (Melissa O'Neill 2014).
/// @details Combines a 64-bit LCG transition with an xorshift-high, random-rotate output permutation. Supports 2^63
///          mathematically disjoint streams via `streamId` and exact O(log n) arbitrary `advance()` and `rewind()`
///          jump-ahead for deterministic replays and lockstep rollback.
class Pcg32 final : public RandomEngineBase<Pcg32, gp::UInt32, Pcg32State, RandomAlgorithm::Pcg32>
{
    friend class RandomEngineBase<Pcg32, gp::UInt32, Pcg32State, RandomAlgorithm::Pcg32>;

public:
    /// @brief Canonical 64-bit PCG linear congruential multiplier.
    static constexpr gp::UInt64 kMultiplier = 6364136223846793005ULL;

    /// @brief Default stream selector matching the reference `0xDA3E39CB94B95BDB` odd increment.
    static constexpr gp::UInt64 kDefaultStreamId = 0xDA3E39CB94B95BDBULL >> 1u;

public:
    /// @brief Constructs a `Pcg32` generator seeded with @p seedValue and @p streamIdValue.
    /// @param[in] seedValue Initial 64-bit seed (defaults to `kDefaultRandomSeed`).
    /// @param[in] streamIdValue Stream selector (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit Pcg32(
        const gp::UInt64 seedValue = kDefaultRandomSeed, const gp::UInt64 streamIdValue = 0ULL
    ) noexcept
    {
        seed(seedValue, streamIdValue);
    }

    /// @brief Advances the internal LCG state by @p delta steps in O(log delta) time without generating outputs.
    /// @details Uses Brown's binary exponentiation of `(mult, inc)` modulo `2^64`.
    /// @param[in] delta Number of 32-bit `nextWord()` transitions to skip forward.
    constexpr void advance(const gp::UInt64 delta) noexcept
    {
        gp::UInt64 curMult = kMultiplier;
        gp::UInt64 curPlus = m_state.increment;
        gp::UInt64 accMult = 1ULL;
        gp::UInt64 accPlus = 0ULL;
        gp::UInt64 remaining = delta;

        while (remaining > 0ULL)
        {
            if ((remaining & 1ULL) != 0ULL)
            {
                accMult *= curMult;
                accPlus = accPlus * curMult + curPlus;
            }
            curPlus = (curMult + 1ULL) * curPlus;
            curMult *= curMult;
            remaining >>= 1u;
        }

        m_state.state = accMult * m_state.state + accPlus;
    }

    /// @brief Rewinds the internal LCG state backward by @p delta steps in O(log delta) time.
    /// @param[in] delta Number of 32-bit `nextWord()` transitions to step backward.
    constexpr void rewind(const gp::UInt64 delta) noexcept
    {
        advance(0ULL - delta);
    }

    /// @brief Jumps forward by `2^32` steps in O(log n) time for parallel stream partitioning.
    constexpr void jump() noexcept
    {
        advance(1ULL << 32u);
    }

    /// @brief Jumps forward by `2^48` steps in O(log n) time for coarse parallel stream partitioning.
    constexpr void longJump() noexcept
    {
        advance(1ULL << 48u);
    }

private:
    constexpr void seedImpl(const gp::UInt64 seedValue, const gp::UInt64 streamIdValue) noexcept
    {
        m_state.state = 0ULL;
        m_state.increment = (streamIdValue << 1u) | 1ULL;
        (void)nextWordImpl();
        m_state.state += seedValue;
        (void)nextWordImpl();
    }

    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 nextWordImpl() noexcept
    {
        const gp::UInt64 oldState = m_state.state;
        m_state.state = oldState * kMultiplier + m_state.increment;
        const gp::UInt32 xorShifted = static_cast<gp::UInt32>(((oldState >> 18u) ^ oldState) >> 27u);
        const gp::Int32 rot = static_cast<gp::Int32>(oldState >> 59u);
        return std::rotr(xorShifted, rot);
    }

    constexpr void discardImpl(const gp::UInt64 steps) noexcept
    {
        advance(steps);
    }
};

/// @brief `RandomAlgorithmTraits` specialization mapping `RandomAlgorithm::Pcg32` to `Pcg32`.
template <>
struct RandomAlgorithmTraits<RandomAlgorithm::Pcg32>
{
    using Type = Pcg32;
};

}   // namespace gp::math
