// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/random/RandomEngine.hpp"
#include "maths/random/RandomEnums.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep
#include "maths/random/RandomTypes.hpp"
#include "maths/random/SplitMix64.hpp"
#include <bit>

namespace gp::math
{

/// @brief Xoshiro256++ 64-bit generator with 256-bit state (Blackman & Vigna 2018).
/// @details Default PRNG engine backing `gp::math::RandomStream`. Maintains four 64-bit words (`32` bytes total)
///          with period `2^256 - 1` and native `jump()` (`2^128` steps) and `longJump()` (`2^192` steps) operations
///          for partitioning deterministic sub-streams across `JobSystem` worker threads.
class Xoshiro256PlusPlus final
    : public RandomEngineBase<Xoshiro256PlusPlus, gp::UInt64, Xoshiro256State, RandomAlgorithm::Xoshiro256PlusPlus>
{
    friend class RandomEngineBase<Xoshiro256PlusPlus, gp::UInt64, Xoshiro256State, RandomAlgorithm::Xoshiro256PlusPlus>;

public:
    /// @brief Constructs a `Xoshiro256PlusPlus` generator seeded via `SplitMix64` expansion.
    /// @param[in] seedValue Initial 64-bit seed (defaults to `kDefaultRandomSeed`).
    /// @param[in] streamIdValue Optional stream identifier (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit Xoshiro256PlusPlus(
        const gp::UInt64 seedValue = kDefaultRandomSeed, const gp::UInt64 streamIdValue = 0ULL
    ) noexcept
    {
        seed(seedValue, streamIdValue);
    }

    /// @brief Advances the state by `2^128` steps using Blackman & Vigna's jump polynomial.
    /// @details Supports up to `2^128` non-overlapping sub-streams of length `2^128`.
    constexpr void jump() noexcept
    {
        static constexpr gp::UInt64 kJump[4] = {
            0x180EC6D33CFD0ABAULL, 0xD5A61266F0C9392CULL, 0xA9582618E03FC9AAULL, 0x39ABDC4529B1661CULL
        };
        applyJumpPolynomial(kJump);
    }

    /// @brief Advances the state by `2^192` steps using Blackman & Vigna's long-jump polynomial.
    /// @details Supports `2^64` independent job workers, each able to perform `2^64` regular `jump()` calls.
    constexpr void longJump() noexcept
    {
        static constexpr gp::UInt64 kLongJump[4] = {
            0x76E15D3EFDFBFBBFULL, 0xC5004E441C522FB3ULL, 0x77710069854EE241ULL, 0x39109BB02ACBE635ULL
        };
        applyJumpPolynomial(kLongJump);
    }

private:
    constexpr void seedImpl(const gp::UInt64 seedValue, const gp::UInt64 streamIdValue) noexcept
    {
        SplitMix64 expander(mixSeedAndStream(seedValue, streamIdValue));
        m_state.words[0] = expander.nextWord();
        m_state.words[1] = expander.nextWord();
        m_state.words[2] = expander.nextWord();
        m_state.words[3] = expander.nextWord();

        if ((m_state.words[0] | m_state.words[1] | m_state.words[2] | m_state.words[3]) == 0ULL)
        {
            m_state.words[0] = 0x9E3779B97F4A7C15ULL;
            m_state.words[1] = 0xBF58476D1CE4E5B9ULL;
        }
    }

    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 nextWordImpl() noexcept
    {
        const gp::UInt64 result = std::rotl(m_state.words[0] + m_state.words[3], 23) + m_state.words[0];
        const gp::UInt64 t = m_state.words[1] << 17u;

        m_state.words[2] ^= m_state.words[0];
        m_state.words[3] ^= m_state.words[1];
        m_state.words[1] ^= m_state.words[2];
        m_state.words[0] ^= m_state.words[3];
        m_state.words[2] ^= t;
        m_state.words[3] = std::rotl(m_state.words[3], 45);

        return result;
    }

    constexpr void discardImpl(const gp::UInt64 steps) noexcept
    {
        for (gp::UInt64 i = 0ULL; i < steps; ++i)
        {
            (void)nextWordImpl();
        }
    }

    constexpr void applyJumpPolynomial(const gp::UInt64 (&jumpTable)[4]) noexcept
    {
        gp::UInt64 s0 = 0ULL;
        gp::UInt64 s1 = 0ULL;
        gp::UInt64 s2 = 0ULL;
        gp::UInt64 s3 = 0ULL;

        for (gp::USize i = 0u; i < 4u; ++i)
        {
            for (gp::UInt32 b = 0u; b < 64u; ++b)
            {
                if ((jumpTable[i] & (1ULL << b)) != 0ULL)
                {
                    s0 ^= m_state.words[0];
                    s1 ^= m_state.words[1];
                    s2 ^= m_state.words[2];
                    s3 ^= m_state.words[3];
                }
                (void)nextWordImpl();
            }
        }

        m_state.words[0] = s0;
        m_state.words[1] = s1;
        m_state.words[2] = s2;
        m_state.words[3] = s3;
    }
};

/// @brief `RandomAlgorithmTraits` specialization mapping `RandomAlgorithm::Xoshiro256PlusPlus` to `Xoshiro256PlusPlus`.
template <>
struct RandomAlgorithmTraits<RandomAlgorithm::Xoshiro256PlusPlus>
{
    using Type = Xoshiro256PlusPlus;
};

}   // namespace gp::math
