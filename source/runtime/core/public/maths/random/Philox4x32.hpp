// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep
#include "maths/random/RandomTypes.hpp"

namespace gp::math
{

/// @brief Stateless counter-based Philox4x32-10 pseudorandom generator (Salmon et al., SC11 / Random123).
/// @details Evaluates a pure bijective function from `(key, counter)` to four independent 32-bit random words in 10
///          Feistel-like multiply-XOR rounds. Because no mutable state is carried between calls, parallel worker jobs
///          and GPU shaders can sample identical values in any execution order given `(seed, entityId, passId)`.
class Philox4x32 final
{
public:
    /// @brief Key type passed to `evaluate()`.
    using KeyType = Philox4x32Key;

    /// @brief 128-bit counter coordinate passed to `evaluate()`.
    using CounterType = Philox4x32Counter;

    /// @brief Output block containing four independent 32-bit random words.
    using BlockType = gp::Array<gp::UInt32, 4u>;

    /// @brief Number of Philox mixing rounds (`10`, matching the Random123 `philox4x32_10` specification).
    static constexpr gp::USize kRounds = 10u;

private:
    static constexpr gp::UInt32 kM0 = 0xD2511F53u;
    static constexpr gp::UInt32 kM1 = 0xCD9E8D57u;
    static constexpr gp::UInt32 kW0 = 0x9E3779B9u;
    static constexpr gp::UInt32 kW1 = 0xBB67AE85u;

public:
    Philox4x32() = delete;

    /// @brief Evaluates 10 rounds of Philox4x32 for @p key and @p counter (`constexpr`-capable).
    /// @param[in] key 64-bit seed key.
    /// @param[in] counter 128-bit counter coordinate.
    /// @return Four independent uniform 32-bit random words.
    [[nodiscard]] static constexpr BlockType evaluate(const KeyType& key, const CounterType& counter) noexcept
    {
        BlockType ctr{ counter.words[0], counter.words[1], counter.words[2], counter.words[3] };
        gp::UInt32 k0 = key.words[0];
        gp::UInt32 k1 = key.words[1];

        for (gp::USize roundIdx = 0u; roundIdx < kRounds; ++roundIdx)
        {
            const gp::UInt64 p0 = static_cast<gp::UInt64>(kM0) * static_cast<gp::UInt64>(ctr[0]);
            const gp::UInt64 p1 = static_cast<gp::UInt64>(kM1) * static_cast<gp::UInt64>(ctr[2]);

            const gp::UInt32 hi0 = static_cast<gp::UInt32>(p0 >> 32u);
            const gp::UInt32 lo0 = static_cast<gp::UInt32>(p0);
            const gp::UInt32 hi1 = static_cast<gp::UInt32>(p1 >> 32u);
            const gp::UInt32 lo1 = static_cast<gp::UInt32>(p1);

            const gp::UInt32 next0 = hi1 ^ ctr[1] ^ k0;
            const gp::UInt32 next1 = lo1;
            const gp::UInt32 next2 = hi0 ^ ctr[3] ^ k1;
            const gp::UInt32 next3 = lo0;

            ctr[0] = next0;
            ctr[1] = next1;
            ctr[2] = next2;
            ctr[3] = next3;

            k0 += kW0;
            k1 += kW1;
        }

        return ctr;
    }

    /// @brief Convenience overload evaluating Philox4x32 from a 64-bit seed and two 64-bit counter lanes.
    /// @param[in] seed 64-bit seed key.
    /// @param[in] counterLow Lower 64-bit counter coordinate (e.g., element or entity index).
    /// @param[in] counterHigh Upper 64-bit counter coordinate (e.g., frame or pass index).
    /// @return Four independent uniform 32-bit random words.
    [[nodiscard]] static constexpr BlockType
        evaluate(const gp::UInt64 seed, const gp::UInt64 counterLow, const gp::UInt64 counterHigh = 0ULL) noexcept
    {
        return evaluate(KeyType(seed), CounterType(counterLow, counterHigh));
    }

    /// @brief Evaluates Philox4x32 and packs the first two output words into a uniform 64-bit integer.
    /// @param[in] seed 64-bit seed key.
    /// @param[in] counterLow Lower 64-bit counter coordinate.
    /// @param[in] counterHigh Upper 64-bit counter coordinate.
    /// @return Uniform 64-bit unsigned integer.
    [[nodiscard]] static constexpr gp::UInt64
        evaluate64(const gp::UInt64 seed, const gp::UInt64 counterLow, const gp::UInt64 counterHigh = 0ULL) noexcept
    {
        const BlockType block = evaluate(seed, counterLow, counterHigh);
        return (static_cast<gp::UInt64>(block[0]) << 32u) | static_cast<gp::UInt64>(block[1]);
    }

    /// @brief Evaluates Philox4x32 and returns four uniform `Float32` values in `[0.0f, 1.0f)`.
    /// @param[in] seed 64-bit seed key.
    /// @param[in] counterLow Lower 64-bit counter coordinate.
    /// @param[in] counterHigh Upper 64-bit counter coordinate.
    /// @return Array of four independent `Float32` values in `[0.0f, 1.0f)`.
    [[nodiscard]] static constexpr gp::Array<gp::Float32, 4u>
        evaluateFloat4(const gp::UInt64 seed, const gp::UInt64 counterLow, const gp::UInt64 counterHigh = 0ULL) noexcept
    {
        const BlockType block = evaluate(seed, counterLow, counterHigh);
        return gp::Array<gp::Float32, 4u>{
            bitsToFloat01(block[0]), bitsToFloat01(block[1]), bitsToFloat01(block[2]), bitsToFloat01(block[3])
        };
    }
};

}   // namespace gp::math
