// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/random/RandomEnums.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep
#include <type_traits>

#if GP_COMPILER_MSVC && GP_ARCHITECTURE_64BIT && !GP_COMPILER_CLANG
    #include <intrin.h>
#endif

namespace gp::math
{

namespace random_detail
{

/// @brief Multiplies two 64-bit unsigned integers into a 128-bit product `(outHigh, outLow)`.
/// @details Uses compiler 128-bit intrinsics at runtime and 32-bit limb multiplication during `constexpr` evaluation.
///          Lemire's unbiased 64-bit bounded range reduction relies on the upper 64 bits of this product.
/// @param[in] lhs Left 64-bit operand.
/// @param[in] rhs Right 64-bit operand.
/// @param[out] outLow Lower 64 bits of the product.
/// @param[out] outHigh Upper 64 bits of the product.
GP_FORCEINLINE_HINT constexpr void
    multiplyUInt64To128(const gp::UInt64 lhs, const gp::UInt64 rhs, gp::UInt64& outLow, gp::UInt64& outHigh) noexcept
{
    if (!std::is_constant_evaluated())
    {
#if defined(__SIZEOF_INT128__)
        const unsigned __int128 product = static_cast<unsigned __int128>(lhs) * static_cast<unsigned __int128>(rhs);
        outLow = static_cast<gp::UInt64>(product);
        outHigh = static_cast<gp::UInt64>(product >> 64u);
        return;
#elif GP_COMPILER_MSVC && GP_ARCHITECTURE_64BIT
        outLow = _umul128(lhs, rhs, &outHigh);
        return;
#endif
    }

    const gp::UInt64 aLo = lhs & 0xFFFFFFFFULL;
    const gp::UInt64 aHi = lhs >> 32u;
    const gp::UInt64 bLo = rhs & 0xFFFFFFFFULL;
    const gp::UInt64 bHi = rhs >> 32u;

    const gp::UInt64 loLo = aLo * bLo;
    const gp::UInt64 loHi = aLo * bHi;
    const gp::UInt64 hiLo = aHi * bLo;
    const gp::UInt64 hiHi = aHi * bHi;

    const gp::UInt64 cross = (loLo >> 32u) + (loHi & 0xFFFFFFFFULL) + (hiLo & 0xFFFFFFFFULL);
    outLow = (loLo & 0xFFFFFFFFULL) | (cross << 32u);
    outHigh = hiHi + (loHi >> 32u) + (hiLo >> 32u) + (cross >> 32u);
}

}   // namespace random_detail

/// @brief Canonical default seed used when constructing deterministic `gp::math` generators without an explicit seed.
inline constexpr gp::UInt64 kDefaultRandomSeed = 0x853C49E6748FEA9BULL;

/// @brief Serializable 8-byte state snapshot for `SplitMix64`.
struct SplitMix64State
{
public:
    /// @brief Internal 64-bit Weyl sequence state.
    gp::UInt64 state{ 0ULL };

public:
    /// @brief Compares two `SplitMix64State` snapshots for exact equality.
    /// @param[in] other Snapshot to compare against.
    /// @return True if both snapshots hold the same state.
    [[nodiscard]] constexpr bool operator==(const SplitMix64State& other) const noexcept = default;

    /// @brief Streams the internal state into any `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(state);
    }
};

static_assert(sizeof(SplitMix64State) == 8u, "SplitMix64State must be 8 bytes.");
static_assert(std::is_trivially_copyable_v<SplitMix64State>, "SplitMix64State must be trivially copyable.");

/// @brief Serializable 16-byte state snapshot for `Pcg32`.
struct Pcg32State
{
public:
    /// @brief Current 64-bit LCG state word.
    gp::UInt64 state{ 0ULL };

    /// @brief Odd 64-bit LCG additive increment (`(streamId << 1) | 1`).
    gp::UInt64 increment{ 0ULL };

public:
    /// @brief Compares two `Pcg32State` snapshots for exact equality.
    /// @param[in] other Snapshot to compare against.
    /// @return True if both `state` and `increment` match.
    [[nodiscard]] constexpr bool operator==(const Pcg32State& other) const noexcept = default;

    /// @brief Streams both `state` and `increment` into any `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(state);
        hasher.append(increment);
    }
};

static_assert(sizeof(Pcg32State) == 16u, "Pcg32State must be 16 bytes.");
static_assert(std::is_trivially_copyable_v<Pcg32State>, "Pcg32State must be trivially copyable.");

/// @brief Serializable 16-byte state snapshot for `Xoshiro128PlusPlus`.
struct Xoshiro128State
{
public:
    /// @brief Four 32-bit LFSR state words (`words[0..3]`).
    gp::UInt32 words[4]{};

public:
    /// @brief Compares two `Xoshiro128State` snapshots for exact equality.
    /// @param[in] other Snapshot to compare against.
    /// @return True if all four words match.
    [[nodiscard]] constexpr bool operator==(const Xoshiro128State& other) const noexcept = default;

    /// @brief Streams all four 32-bit state words into any `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        for (gp::USize i = 0u; i < 4u; ++i)
        {
            hasher.append(words[i]);
        }
    }
};

static_assert(sizeof(Xoshiro128State) == 16u, "Xoshiro128State must be 16 bytes.");
static_assert(std::is_trivially_copyable_v<Xoshiro128State>, "Xoshiro128State must be trivially copyable.");

/// @brief Serializable 32-byte state snapshot for `Xoshiro256PlusPlus`.
struct Xoshiro256State
{
public:
    /// @brief Four 64-bit LFSR state words (`words[0..3]`).
    gp::UInt64 words[4]{};

public:
    /// @brief Compares two `Xoshiro256State` snapshots for exact equality.
    /// @param[in] other Snapshot to compare against.
    /// @return True if all four words match.
    [[nodiscard]] constexpr bool operator==(const Xoshiro256State& other) const noexcept = default;

    /// @brief Streams all four 64-bit state words into any `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        for (gp::USize i = 0u; i < 4u; ++i)
        {
            hasher.append(words[i]);
        }
    }
};

static_assert(sizeof(Xoshiro256State) == 32u, "Xoshiro256State must be 32 bytes.");
static_assert(std::is_trivially_copyable_v<Xoshiro256State>, "Xoshiro256State must be trivially copyable.");

/// @brief Serializable 32-byte state snapshot for `Sfc64`.
struct Sfc64State
{
public:
    /// @brief First chaotic mixing word.
    gp::UInt64 a{ 0ULL };

    /// @brief Second chaotic mixing word.
    gp::UInt64 b{ 0ULL };

    /// @brief Third chaotic mixing word.
    gp::UInt64 c{ 0ULL };

    /// @brief Monotonic Weyl counter guaranteeing minimum period 2^64.
    gp::UInt64 counter{ 0ULL };

public:
    /// @brief Compares two `Sfc64State` snapshots for exact equality.
    /// @param[in] other Snapshot to compare against.
    /// @return True if all four state words match.
    [[nodiscard]] constexpr bool operator==(const Sfc64State& other) const noexcept = default;

    /// @brief Streams all four 64-bit words into any `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(a);
        hasher.append(b);
        hasher.append(c);
        hasher.append(counter);
    }
};

static_assert(sizeof(Sfc64State) == 32u, "Sfc64State must be 32 bytes.");
static_assert(std::is_trivially_copyable_v<Sfc64State>, "Sfc64State must be trivially copyable.");

/// @brief 64-bit key descriptor (`2 x UInt32`) for stateless `Philox4x32` evaluations.
struct Philox4x32Key
{
public:
    /// @brief Two 32-bit round key words.
    gp::UInt32 words[2]{};

public:
    /// @brief Default-constructs a zero key.
    [[nodiscard]] constexpr Philox4x32Key() noexcept = default;

    /// @brief Constructs a key from two explicit 32-bit words.
    /// @param[in] k0 Low 32-bit key word.
    /// @param[in] k1 High 32-bit key word.
    [[nodiscard]] constexpr Philox4x32Key(const gp::UInt32 k0, const gp::UInt32 k1) noexcept
        : words{ k0, k1 }
    {}

    /// @brief Constructs a key from a single 64-bit seed.
    /// @param[in] seed 64-bit seed split into low and high 32-bit key words.
    [[nodiscard]] constexpr explicit Philox4x32Key(const gp::UInt64 seed) noexcept
        : words{ static_cast<gp::UInt32>(seed), static_cast<gp::UInt32>(seed >> 32u) }
    {}

    /// @brief Compares two `Philox4x32Key` values for equality.
    /// @param[in] other Key to compare against.
    /// @return True if both key words match.
    [[nodiscard]] constexpr bool operator==(const Philox4x32Key& other) const noexcept = default;

    /// @brief Streams the two 32-bit key words into any `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        hasher.append(words[0]);
        hasher.append(words[1]);
    }
};

static_assert(sizeof(Philox4x32Key) == 8u, "Philox4x32Key must be 8 bytes.");
static_assert(std::is_trivially_copyable_v<Philox4x32Key>, "Philox4x32Key must be trivially copyable.");

/// @brief 128-bit counter coordinate (`4 x UInt32`) for stateless `Philox4x32` evaluations.
struct Philox4x32Counter
{
public:
    /// @brief Four 32-bit counter words (`words[0..3]`).
    gp::UInt32 words[4]{};

public:
    /// @brief Default-constructs a zero counter.
    [[nodiscard]] constexpr Philox4x32Counter() noexcept = default;

    /// @brief Constructs a 4D counter from four explicit 32-bit coordinates.
    /// @param[in] c0 First coordinate word.
    /// @param[in] c1 Second coordinate word.
    /// @param[in] c2 Third coordinate word.
    /// @param[in] c3 Fourth coordinate word.
    [[nodiscard]] constexpr Philox4x32Counter(
        const gp::UInt32 c0, const gp::UInt32 c1, const gp::UInt32 c2, const gp::UInt32 c3
    ) noexcept
        : words{ c0, c1, c2, c3 }
    {}

    /// @brief Constructs a counter from two 64-bit lanes (e.g., `(index, passId)`).
    /// @param[in] low64 Lower 64-bit coordinate packed into `words[0..1]`.
    /// @param[in] high64 Upper 64-bit coordinate packed into `words[2..3]`.
    [[nodiscard]] constexpr explicit Philox4x32Counter(const gp::UInt64 low64, const gp::UInt64 high64 = 0ULL) noexcept
        : words{ static_cast<gp::UInt32>(low64),
                 static_cast<gp::UInt32>(low64 >> 32u),
                 static_cast<gp::UInt32>(high64),
                 static_cast<gp::UInt32>(high64 >> 32u) }
    {}

    /// @brief Compares two `Philox4x32Counter` values for equality.
    /// @param[in] other Counter to compare against.
    /// @return True if all four counter words match.
    [[nodiscard]] constexpr bool operator==(const Philox4x32Counter& other) const noexcept = default;

    /// @brief Streams all four counter words into any `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        for (gp::USize i = 0u; i < 4u; ++i)
        {
            hasher.append(words[i]);
        }
    }
};

static_assert(sizeof(Philox4x32Counter) == 16u, "Philox4x32Counter must be 16 bytes.");
static_assert(std::is_trivially_copyable_v<Philox4x32Counter>, "Philox4x32Counter must be trivially copyable.");

/// @brief Compile-time traits mapping a `RandomAlgorithm` enumerator to its concrete engine type.
/// @tparam InAlgorithm Target `RandomAlgorithm` enumerator.
template <RandomAlgorithm InAlgorithm>
struct RandomAlgorithmTraits;

/// @brief Convenience alias resolving a `RandomAlgorithm` enumerator to its concrete engine type.
/// @tparam InAlgorithm Target `RandomAlgorithm` enumerator.
template <RandomAlgorithm InAlgorithm>
using RandomEngine = typename RandomAlgorithmTraits<InAlgorithm>::Type;

/// @brief Converts 32 uniform random bits into a `Float32` in the half-open interval `[0.0f, 1.0f)`.
/// @details Shifts out the low 8 bits and multiplies the 24-bit integer by `2^-24` so every step maps directly onto
///          dyadic IEEE-754 binary32 mantissa values without division bias.
/// @param[in] bits Uniform 32-bit unsigned integer.
/// @return Uniform `Float32` in `[0.0f, 1.0f)`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 bitsToFloat01(const gp::UInt32 bits) noexcept
{
    return static_cast<gp::Float32>(bits >> 8u) * 0x1.0p-24f;
}

/// @brief Converts 32 uniform random bits into a `Float32` in the closed interval `[0.0f, 1.0f]`.
/// @param[in] bits Uniform 32-bit unsigned integer.
/// @return Uniform `Float32` in `[0.0f, 1.0f]`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 bitsToFloat01Inclusive(const gp::UInt32 bits) noexcept
{
    return static_cast<gp::Float32>(bits >> 8u) * (1.0f / 16777215.0f);
}

/// @brief Converts 64 uniform random bits into a `Float64` in the half-open interval `[0.0, 1.0)`.
/// @details Uses the top 53 bits multiplied by `2^-53`, matching the exact precision of an IEEE-754 binary64 mantissa.
/// @param[in] bits Uniform 64-bit unsigned integer.
/// @return Uniform `Float64` in `[0.0, 1.0)`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float64 bitsToDouble01(const gp::UInt64 bits) noexcept
{
    return static_cast<gp::Float64>(bits >> 11u) * 0x1.0p-53;
}

/// @brief Converts 64 uniform random bits into a `Float64` in the closed interval `[0.0, 1.0]`.
/// @param[in] bits Uniform 64-bit unsigned integer.
/// @return Uniform `Float64` in `[0.0, 1.0]`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float64 bitsToDouble01Inclusive(const gp::UInt64 bits) noexcept
{
    return static_cast<gp::Float64>(bits >> 11u) * (1.0 / 9007199254740991.0);
}

/// @brief Converts 32 uniform random bits into a bipolar `Float32` in `[-1.0f, 1.0f]`.
/// @param[in] bits Uniform 32-bit unsigned integer.
/// @return Uniform `Float32` in `[-1.0f, 1.0f]`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 bitsToSignedFloat(const gp::UInt32 bits) noexcept
{
    return (bitsToFloat01Inclusive(bits) * 2.0f) - 1.0f;
}

/// @brief Converts 32 uniform random bits into a bipolar `Float32` in `[-1.0f, 1.0f)`.
/// @param[in] bits Uniform 32-bit unsigned integer.
/// @return Uniform `Float32` in `[-1.0f, 1.0f)`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 bitsToSignedFloatHalfOpen(const gp::UInt32 bits) noexcept
{
    return (bitsToFloat01(bits) * 2.0f) - 1.0f;
}

/// @brief Converts 64 uniform random bits into a bipolar `Float64` in `[-1.0, 1.0]`.
/// @param[in] bits Uniform 64-bit unsigned integer.
/// @return Uniform `Float64` in `[-1.0, 1.0]`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float64 bitsToSignedDouble(const gp::UInt64 bits) noexcept
{
    return (bitsToDouble01Inclusive(bits) * 2.0) - 1.0;
}

/// @brief Converts 64 uniform random bits into a bipolar `Float64` in `[-1.0, 1.0)`.
/// @param[in] bits Uniform 64-bit unsigned integer.
/// @return Uniform `Float64` in `[-1.0, 1.0)`.
[[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float64 bitsToSignedDoubleHalfOpen(const gp::UInt64 bits) noexcept
{
    return (bitsToDouble01(bits) * 2.0) - 1.0;
}

}   // namespace gp::math
