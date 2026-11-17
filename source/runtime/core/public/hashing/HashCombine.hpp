// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"
#include "hashing/HashingTypes.hpp"
#include "hashing/HashMix.hpp"
#include <bit>
#include <type_traits>

namespace gp::hashing
{

/// @brief Combines two 32-bit hashes using a fast golden-ratio shift-add mix (`0x9E3779B9`).
/// @details Matches Boost/Unreal `HashCombineFast`. Very cheap (zero integer multiplies), suited for hot lookups when
///          inputs already have decent bit spread. Use `hashCombine()` when sequential small keys need full avalanche.
/// @param[in] seed Accumulated 32-bit hash state.
/// @param[in] valueHash Incoming 32-bit hash to fold in.
/// @return Combined 32-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
    hashCombineFast(const gp::UInt32 seed, const gp::UInt32 valueHash) noexcept
{
    return seed ^ (valueHash + 0x9E3779B9u + (seed << 6u) + (seed >> 2u));
}

/// @brief Combines two 64-bit hashes using a fast 64-bit golden-ratio shift-add mix (`0x9E3779B97F4A7C15`).
/// @details Suited for hot-path hash map keys where inputs are already hashed. Use `hashCombine()` for small integers
///          or collision-sensitive keys.
/// @param[in] seed Accumulated 64-bit hash state.
/// @param[in] valueHash Incoming 64-bit hash to fold in.
/// @return Combined 64-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64
    hashCombineFast(const gp::UInt64 seed, const gp::UInt64 valueHash) noexcept
{
    return seed ^ (valueHash + 0x9E3779B97F4A7C15ULL + (seed << 6u) + (seed >> 2u));
}

/// @brief Combines a variadic pack of raw 32-bit hashes sequentially via `hashCombineFast`.
/// @tparam InRestT Remaining 32-bit digest types.
/// @param[in] seed Initial 32-bit hash state.
/// @param[in] first First 32-bit hash to mix.
/// @param[in] rest Additional 32-bit hashes folded left-to-right.
/// @return Combined 32-bit hash.
template <typename... InRestT>
requires(sizeof...(InRestT) > 0u && (gp::concepts::IsSameAs<std::remove_cvref_t<InRestT>, gp::UInt32> && ...))
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
    hashCombineFast(gp::UInt32 seed, const gp::UInt32 first, const InRestT... rest) noexcept
{
    seed = hashCombineFast(seed, first);
    ((seed = hashCombineFast(seed, rest)), ...);
    return seed;
}

/// @brief Combines a variadic pack of raw 64-bit hashes sequentially via `hashCombineFast`.
/// @tparam InRestT Remaining 64-bit digest types.
/// @param[in] seed Initial 64-bit hash state.
/// @param[in] first First 64-bit hash to mix.
/// @param[in] rest Additional 64-bit hashes folded left-to-right.
/// @return Combined 64-bit hash.
template <typename... InRestT>
requires(sizeof...(InRestT) > 0u && (gp::concepts::IsSameAs<std::remove_cvref_t<InRestT>, gp::UInt64> && ...))
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64
    hashCombineFast(gp::UInt64 seed, const gp::UInt64 first, const InRestT... rest) noexcept
{
    seed = hashCombineFast(seed, first);
    ((seed = hashCombineFast(seed, rest)), ...);
    return seed;
}

/// @brief Combines two 32-bit hashes with strong avalanche using a Murmur3 block step and `fmix32`.
/// @details Order-sensitive (`hashCombine(a, b) != hashCombine(b, a)` in general) and resists cancellation when both
///          inputs are equal or zero.
/// @param[in] seed Accumulated 32-bit hash state.
/// @param[in] valueHash Incoming 32-bit hash or small integer value.
/// @return High-dispersion combined 32-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
    hashCombine(const gp::UInt32 seed, const gp::UInt32 valueHash) noexcept
{
    gp::UInt32 k = valueHash * 0xCC9E2D51u;
    k = std::rotl(k, 15);
    k *= 0x1B873593u;

    gp::UInt32 h = seed ^ k;
    h = std::rotl(h, 13);
    h = (h * 5u) + 0xE6546B64u;
    return avalancheMix32(h);
}

/// @brief Combines two 64-bit hashes with strong avalanche using CityHash `Hash128to64`.
/// @details Order-sensitive (`hashCombine(a, b) != hashCombine(b, a)` in general). Perturbing `valueHash` with the
///          64-bit golden-ratio constant ensures `(0, 0)` does not collapse to `0` while keeping full avalanche.
/// @param[in] seed Accumulated 64-bit hash state.
/// @param[in] valueHash Incoming 64-bit hash or integer value.
/// @return High-dispersion combined 64-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64
    hashCombine(const gp::UInt64 seed, const gp::UInt64 valueHash) noexcept
{
    return cityHash128To64(valueHash ^ 0x9E3779B97F4A7C15ULL, seed);
}

/// @brief Combines two 128-bit digests with order sensitivity.
/// @param[in] seed Accumulated 128-bit digest.
/// @param[in] valueHash Incoming 128-bit digest.
/// @return Combined 128-bit digest.
[[nodiscard]] constexpr Hash128 hashCombine(const Hash128& seed, const Hash128& valueHash) noexcept
{
    const gp::UInt64 newLow = cityHash128To64(seed.low ^ valueHash.high, valueHash.low ^ 0x9E3779B97F4A7C15ULL);
    const gp::UInt64 newHigh = cityHash128To64(seed.high ^ valueHash.low, valueHash.high ^ newLow);
    return Hash128(newLow, newHigh);
}

/// @brief Combines a variadic pack of raw 32-bit hashes sequentially via `hashCombine`.
/// @tparam InRestT Remaining 32-bit digest types.
/// @param[in] seed Initial 32-bit hash state.
/// @param[in] first First 32-bit hash to mix.
/// @param[in] rest Additional 32-bit hashes folded left-to-right.
/// @return Combined 32-bit hash.
template <typename... InRestT>
requires(sizeof...(InRestT) > 0u && (gp::concepts::IsSameAs<std::remove_cvref_t<InRestT>, gp::UInt32> && ...))
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
    hashCombine(gp::UInt32 seed, const gp::UInt32 first, const InRestT... rest) noexcept
{
    seed = hashCombine(seed, first);
    ((seed = hashCombine(seed, rest)), ...);
    return seed;
}

/// @brief Combines a variadic pack of raw 64-bit hashes sequentially via `hashCombine`.
/// @tparam InRestT Remaining 64-bit digest types.
/// @param[in] seed Initial 64-bit hash state.
/// @param[in] first First 64-bit hash to mix.
/// @param[in] rest Additional 64-bit hashes folded left-to-right.
/// @return Combined 64-bit hash.
template <typename... InRestT>
requires(sizeof...(InRestT) > 0u && (gp::concepts::IsSameAs<std::remove_cvref_t<InRestT>, gp::UInt64> && ...))
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64
    hashCombine(gp::UInt64 seed, const gp::UInt64 first, const InRestT... rest) noexcept
{
    seed = hashCombine(seed, first);
    ((seed = hashCombine(seed, rest)), ...);
    return seed;
}

/// @brief Combines two 32-bit hashes symmetrically so `hashCombineSymmetric(a, b) == hashCombineSymmetric(b, a)`.
/// @details Naive `a ^ b` collapses equal inputs `(x, x)` to `0`. Sorting `(min, max)` first and feeding `hashCombine`
///          keeps order independence for undirected mesh edges and pairs while avoiding zero-cancellation.
/// @param[in] a First 32-bit hash.
/// @param[in] b Second 32-bit hash.
/// @return Order-independent combined 32-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32
    hashCombineSymmetric(const gp::UInt32 a, const gp::UInt32 b) noexcept
{
    return (a < b) ? hashCombine(a, b) : hashCombine(b, a);
}

/// @brief Combines two 64-bit hashes symmetrically so `hashCombineSymmetric(a, b) == hashCombineSymmetric(b, a)`.
/// @param[in] a First 64-bit hash.
/// @param[in] b Second 64-bit hash.
/// @return Order-independent combined 64-bit hash.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64
    hashCombineSymmetric(const gp::UInt64 a, const gp::UInt64 b) noexcept
{
    return (a < b) ? hashCombine(a, b) : hashCombine(b, a);
}

/// @brief Mutates @p inOutSeed in place by folding @p valueHash with `hashCombine`.
/// @param[in,out] inOutSeed Running 32-bit hash seed updated in place.
/// @param[in] valueHash Incoming 32-bit hash.
GP_FORCEINLINE_HINT constexpr void appendHash(gp::UInt32& inOutSeed, const gp::UInt32 valueHash) noexcept
{
    inOutSeed = hashCombine(inOutSeed, valueHash);
}

/// @brief Mutates @p inOutSeed in place by folding @p valueHash with `hashCombine`.
/// @param[in,out] inOutSeed Running 64-bit hash seed updated in place.
/// @param[in] valueHash Incoming 64-bit hash.
GP_FORCEINLINE_HINT constexpr void appendHash(gp::UInt64& inOutSeed, const gp::UInt64 valueHash) noexcept
{
    inOutSeed = hashCombine(inOutSeed, valueHash);
}

/// @brief Mutates @p inOutSeed in place by folding @p valueHash with `hashCombineFast`.
/// @param[in,out] inOutSeed Running 32-bit hash seed updated in place.
/// @param[in] valueHash Incoming 32-bit hash.
GP_FORCEINLINE_HINT constexpr void appendHashFast(gp::UInt32& inOutSeed, const gp::UInt32 valueHash) noexcept
{
    inOutSeed = hashCombineFast(inOutSeed, valueHash);
}

/// @brief Mutates @p inOutSeed in place by folding @p valueHash with `hashCombineFast`.
/// @param[in,out] inOutSeed Running 64-bit hash seed updated in place.
/// @param[in] valueHash Incoming 64-bit hash.
GP_FORCEINLINE_HINT constexpr void appendHashFast(gp::UInt64& inOutSeed, const gp::UInt64 valueHash) noexcept
{
    inOutSeed = hashCombineFast(inOutSeed, valueHash);
}

}   // namespace gp::hashing
