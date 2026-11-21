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

/// @brief Xoshiro128++ 32-bit generator with 128-bit state (Blackman & Vigna 2018).
/// @details Uses a 16-byte state footprint (`4 x UInt32`) with period `2^128 - 1`, passing BigCrush and PractRand.
///          Supports non-overlapping `jump()` (`2^64` steps) and `longJump()` (`2^96` steps) for lightweight
///          per-entity streams or GPU-mirrored procedural systems.
class Xoshiro128PlusPlus final
    : public RandomEngineBase<Xoshiro128PlusPlus, gp::UInt32, Xoshiro128State, RandomAlgorithm::Xoshiro128PlusPlus>
{
    friend class RandomEngineBase<Xoshiro128PlusPlus, gp::UInt32, Xoshiro128State, RandomAlgorithm::Xoshiro128PlusPlus>;

public:
    /// @brief Constructs a `Xoshiro128PlusPlus` generator seeded via `SplitMix64` expansion.
    /// @param[in] seedValue Initial 64-bit seed (defaults to `kDefaultRandomSeed`).
    /// @param[in] streamIdValue Optional stream identifier (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit Xoshiro128PlusPlus(
        const gp::UInt64 seedValue = kDefaultRandomSeed, const gp::UInt64 streamIdValue = 0ULL
    ) noexcept
    {
        seed(seedValue, streamIdValue);
    }

    /// @brief Advances the state by `2^64` steps using Blackman & Vigna's jump polynomial.
    /// @details Supports up to `2^64` non-overlapping sub-streams of length `2^64`.
    constexpr void jump() noexcept
    {
        static constexpr gp::UInt32 kJump[4] = { 0x8764000Bu, 0xF542D2D3u, 0x6FA035C3u, 0x77F2DB5Bu };
        applyJumpPolynomial(kJump);
    }

    /// @brief Advances the state by `2^96` steps using Blackman & Vigna's long-jump polynomial.
    /// @details Supports `2^32` worker streams, each capable of `2^32` regular `jump()` calls.
    constexpr void longJump() noexcept
    {
        static constexpr gp::UInt32 kLongJump[4] = { 0xB523952Eu, 0x0B6F099Fu, 0xCCF5A0EFu, 0x1C580662u };
        applyJumpPolynomial(kLongJump);
    }

private:
    constexpr void seedImpl(const gp::UInt64 seedValue, const gp::UInt64 streamIdValue) noexcept
    {
        SplitMix64 expander(mixSeedAndStream(seedValue, streamIdValue));
        const gp::UInt64 low64 = expander.nextWord();
        const gp::UInt64 high64 = expander.nextWord();

        m_state.words[0] = static_cast<gp::UInt32>(low64);
        m_state.words[1] = static_cast<gp::UInt32>(low64 >> 32u);
        m_state.words[2] = static_cast<gp::UInt32>(high64);
        m_state.words[3] = static_cast<gp::UInt32>(high64 >> 32u);

        if ((m_state.words[0] | m_state.words[1] | m_state.words[2] | m_state.words[3]) == 0u)
        {
            m_state.words[0] = 0x9E3779B9u;
            m_state.words[1] = 0x85EBCA77u;
        }
    }

    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 nextWordImpl() noexcept
    {
        const gp::UInt32 result = std::rotl(m_state.words[0] + m_state.words[3], 7) + m_state.words[0];
        const gp::UInt32 t = m_state.words[1] << 9u;

        m_state.words[2] ^= m_state.words[0];
        m_state.words[3] ^= m_state.words[1];
        m_state.words[1] ^= m_state.words[2];
        m_state.words[0] ^= m_state.words[3];
        m_state.words[2] ^= t;
        m_state.words[3] = std::rotl(m_state.words[3], 11);

        return result;
    }

    constexpr void discardImpl(const gp::UInt64 steps) noexcept
    {
        for (gp::UInt64 i = 0ULL; i < steps; ++i)
        {
            (void)nextWordImpl();
        }
    }

    constexpr void applyJumpPolynomial(const gp::UInt32 (&jumpTable)[4]) noexcept
    {
        gp::UInt32 s0 = 0u;
        gp::UInt32 s1 = 0u;
        gp::UInt32 s2 = 0u;
        gp::UInt32 s3 = 0u;

        for (gp::USize i = 0u; i < 4u; ++i)
        {
            for (gp::UInt32 b = 0u; b < 32u; ++b)
            {
                if ((jumpTable[i] & (1u << b)) != 0u)
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

/// @brief `RandomAlgorithmTraits` specialization mapping `RandomAlgorithm::Xoshiro128PlusPlus` to `Xoshiro128PlusPlus`.
template <>
struct RandomAlgorithmTraits<RandomAlgorithm::Xoshiro128PlusPlus>
{
    using Type = Xoshiro128PlusPlus;
};

}   // namespace gp::math
