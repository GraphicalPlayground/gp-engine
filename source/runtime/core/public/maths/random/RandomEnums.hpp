// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep

namespace gp::math
{

/// @brief Identifies a built-in deterministic pseudorandom number generator algorithm.
enum class RandomAlgorithm : gp::UInt8
{
    /// @brief 64-bit Weyl-sequence mixer (Steele, Lea, Flood 2014). Ideal for seed expansion and tiny streams.
    SplitMix64 = 0u,

    /// @brief PCG-XSH-RR 64-bit state / 32-bit output (O'Neill 2014). Supports independent stream IDs and O(log n)
    /// jumps.
    Pcg32 = 1u,

    /// @brief Xoshiro128++ 32-bit generator (Blackman & Vigna 2018). Compact 16-byte state with 2^64 and 2^96 jumps.
    Xoshiro128PlusPlus = 2u,

    /// @brief Xoshiro256++ 64-bit generator (Blackman & Vigna 2018). Default engine for `gp::math::RandomStream`.
    Xoshiro256PlusPlus = 3u,

    /// @brief Small Fast Chaotic 64-bit generator (Chris Doty-Humphrey, PractRand). High speed chaotic mixing.
    Sfc64 = 4u,

    /// @brief Sentinel count of built-in stateful PRNG engines.
    Count = 5u
};

/// @brief Returns the canonical ASCII name of a `RandomAlgorithm` enumerator.
/// @param[in] algorithm Algorithm identifier to query.
/// @return Null-terminated ASCII string literal naming @p algorithm.
[[nodiscard]] constexpr const char* algorithmToString(const RandomAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
    case RandomAlgorithm::SplitMix64:
        return "SplitMix64";
    case RandomAlgorithm::Pcg32:
        return "Pcg32";
    case RandomAlgorithm::Xoshiro128PlusPlus:
        return "Xoshiro128PlusPlus";
    case RandomAlgorithm::Xoshiro256PlusPlus:
        return "Xoshiro256PlusPlus";
    case RandomAlgorithm::Sfc64:
        return "Sfc64";
    case RandomAlgorithm::Count:
        // falls through
    default:
        return "Unknown";
    }
}

/// @brief Returns the native raw output word width in bits (`32` or `64`) for @p algorithm.
/// @param[in] algorithm Algorithm identifier to query.
/// @return Output bit width (`32` or `64`), or `0` for invalid values.
[[nodiscard]] constexpr gp::UInt32 algorithmOutputBits(const RandomAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
    case RandomAlgorithm::Pcg32:
    case RandomAlgorithm::Xoshiro128PlusPlus:
        return 32u;
    case RandomAlgorithm::SplitMix64:
    case RandomAlgorithm::Xoshiro256PlusPlus:
    case RandomAlgorithm::Sfc64:
        return 64u;
    case RandomAlgorithm::Count:
        // falls through
    default:
        return 0u;
    }
}

/// @brief Returns the internal state size in bytes for @p algorithm.
/// @param[in] algorithm Algorithm identifier to query.
/// @return State footprint in bytes (`8`, `16`, or `32`), or `0` for invalid values.
[[nodiscard]] constexpr gp::USize algorithmStateBytes(const RandomAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
    case RandomAlgorithm::SplitMix64:
        return 8u;
    case RandomAlgorithm::Pcg32:
    case RandomAlgorithm::Xoshiro128PlusPlus:
        return 16u;
    case RandomAlgorithm::Xoshiro256PlusPlus:
    case RandomAlgorithm::Sfc64:
        return 32u;
    case RandomAlgorithm::Count:
        // falls through
    default:
        return 0u;
    }
}

/// @brief Reports whether @p algorithm provides O(1) or polynomial fast jump-ahead (`jump()` / `longJump()`).
/// @param[in] algorithm Algorithm identifier to query.
/// @return True for `SplitMix64`, `Pcg32`, `Xoshiro128PlusPlus`, and `Xoshiro256PlusPlus`; false for `Sfc64`.
[[nodiscard]] constexpr bool supportsJumpAhead(const RandomAlgorithm algorithm) noexcept
{
    switch (algorithm)
    {
    case RandomAlgorithm::SplitMix64:
    case RandomAlgorithm::Pcg32:
    case RandomAlgorithm::Xoshiro128PlusPlus:
    case RandomAlgorithm::Xoshiro256PlusPlus:
        return true;
    case RandomAlgorithm::Sfc64:
    case RandomAlgorithm::Count:
        // falls through
    default:
        return false;
    }
}

/// @brief Reports whether @p algorithm has a mathematically independent stream selector (`streamId`).
/// @details `Pcg32` uses an odd additive LCG constant `(streamId << 1) | 1`, giving 2^63 distinct sequences that never
///          overlap for the same state transition structure. Other engines incorporate `streamId` during seeding.
/// @param[in] algorithm Algorithm identifier to query.
/// @return True if @p algorithm selects distinct sequences natively via `streamId`.
[[nodiscard]] constexpr bool supportsNativeStreamId(const RandomAlgorithm algorithm) noexcept
{
    return algorithm == RandomAlgorithm::Pcg32;
}

}   // namespace gp::math
