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

/// @brief Small Fast Chaotic 64-bit generator (Chris Doty-Humphrey, PractRand `sfc64`).
/// @details Uses a non-linear 256-bit chaotic state (`a`, `b`, `c`, `counter`) requiring zero integer multiplications
///          per step. The monotonic Weyl `counter` guarantees a minimum cycle length of `2^64` for every seed.
class Sfc64 final : public RandomEngineBase<Sfc64, gp::UInt64, Sfc64State, RandomAlgorithm::Sfc64>
{
    friend class RandomEngineBase<Sfc64, gp::UInt64, Sfc64State, RandomAlgorithm::Sfc64>;

public:
    /// @brief Constructs an `Sfc64` generator seeded with @p seedValue and optional @p streamIdValue.
    /// @param[in] seedValue Initial 64-bit seed (defaults to `kDefaultRandomSeed`).
    /// @param[in] streamIdValue Optional stream identifier (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit Sfc64(
        const gp::UInt64 seedValue = kDefaultRandomSeed, const gp::UInt64 streamIdValue = 0ULL
    ) noexcept
    {
        seed(seedValue, streamIdValue);
    }

private:
    constexpr void seedImpl(const gp::UInt64 seedValue, const gp::UInt64 streamIdValue) noexcept
    {
        if (streamIdValue == 0ULL)
        {
            m_state.a = seedValue;
            m_state.b = seedValue;
            m_state.c = seedValue;
        }
        else
        {
            m_state.a = seedValue;
            m_state.b = streamIdValue;
            m_state.c = seedValue ^ (streamIdValue * 0x9E3779B97F4A7C15ULL);
        }
        m_state.counter = 1ULL;

        for (gp::USize i = 0u; i < 18u; ++i)
        {
            (void)nextWordImpl();
        }
    }

    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 nextWordImpl() noexcept
    {
        const gp::UInt64 result = m_state.a + m_state.b + m_state.counter++;
        m_state.a = m_state.b ^ (m_state.b >> 11u);
        m_state.b = m_state.c + (m_state.c << 3u);
        m_state.c = std::rotl(m_state.c, 24) + result;
        return result;
    }

    constexpr void discardImpl(const gp::UInt64 steps) noexcept
    {
        for (gp::UInt64 i = 0ULL; i < steps; ++i)
        {
            (void)nextWordImpl();
        }
    }
};

/// @brief `RandomAlgorithmTraits` specialization mapping `RandomAlgorithm::Sfc64` to `Sfc64`.
template <>
struct RandomAlgorithmTraits<RandomAlgorithm::Sfc64>
{
    using Type = Sfc64;
};

}   // namespace gp::math
