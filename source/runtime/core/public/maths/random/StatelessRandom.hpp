// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashMix.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Constants.hpp"
#include "maths/base/FloatingPoint.hpp"
#include "maths/base/Interpolation.hpp"     // IWYU pragma: keep
#include "maths/random/Philox4x32.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep
#include "maths/random/RandomTypes.hpp"
#include "maths/vector/Vector2.hpp"
#include "maths/vector/Vector3.hpp"
#include "memory/MemoryOrder.hpp"
#include "platforms/base/system/Time.hpp"
#include "templates/Interval.hpp"
#include "threading/atomics/Atomic.hpp"
#include <cmath>
#include <type_traits>

namespace gp::math
{

namespace random_detail
{

/// @brief Unbiased Lemire bounded 32-bit range sampler taking a uniform 32-bit generator callable.
/// @tparam InNextU32 Callable returning uniform `gp::UInt32`.
/// @param[in] range Exclusive upper bound (`[0, range)`).
/// @param[in,out] nextU32 Source of uniform 32-bit unsigned integers.
/// @return Uniform integer in `[0, range)`, or `0` when `range <= 1`.
template <typename InNextU32>
[[nodiscard]] constexpr gp::UInt32 boundedUInt32(const gp::UInt32 range, InNextU32&& nextU32) noexcept
{
    if (range <= 1u)
    {
        return 0u;
    }

    gp::UInt64 product = static_cast<gp::UInt64>(nextU32()) * static_cast<gp::UInt64>(range);
    gp::UInt32 low = static_cast<gp::UInt32>(product);
    if (low < range)
    {
        const gp::UInt32 threshold = (0u - range) % range;
        while (low < threshold)
        {
            product = static_cast<gp::UInt64>(nextU32()) * static_cast<gp::UInt64>(range);
            low = static_cast<gp::UInt32>(product);
        }
    }
    return static_cast<gp::UInt32>(product >> 32u);
}

/// @brief Unbiased Lemire bounded 64-bit range sampler taking a uniform 64-bit generator callable.
/// @tparam InNextU64 Callable returning uniform `gp::UInt64`.
/// @param[in] range Exclusive upper bound (`[0, range)`).
/// @param[in,out] nextU64 Source of uniform 64-bit unsigned integers.
/// @return Uniform integer in `[0, range)`, or `0` when `range <= 1`.
template <typename InNextU64>
[[nodiscard]] constexpr gp::UInt64 boundedUInt64(const gp::UInt64 range, InNextU64&& nextU64) noexcept
{
    if (range <= 1ULL)
    {
        return 0ULL;
    }

    gp::UInt64 low = 0ULL;
    gp::UInt64 high = 0ULL;
    multiplyUInt64To128(nextU64(), range, low, high);
    if (low < range)
    {
        const gp::UInt64 threshold = (0ULL - range) % range;
        while (low < threshold)
        {
            multiplyUInt64To128(nextU64(), range, low, high);
        }
    }
    return high;
}

/// @brief Builds an orthonormal tangent frame around a unit normal using Duff et al. (2017).
/// @tparam InFloatT Floating-point scalar type.
/// @param[in] unitNormal Normalized input vector.
/// @param[out] outTangent First unit tangent vector orthogonal to @p unitNormal.
/// @param[out] outBitangent Second unit tangent vector orthogonal to @p unitNormal and @p outTangent.
template <gp::concepts::IsFloatingPoint InFloatT>
inline void buildOrthonormalBasis(
    const Vector3<InFloatT>& unitNormal, Vector3<InFloatT>& outTangent, Vector3<InFloatT>& outBitangent
) noexcept
{
    const InFloatT sign = (unitNormal.z >= InFloatT{ 0 }) ? InFloatT{ 1 } : InFloatT{ -1 };
    const InFloatT a = -InFloatT{ 1 } / (sign + unitNormal.z);
    const InFloatT b = unitNormal.x * unitNormal.y * a;

    outTangent =
        Vector3<InFloatT>(InFloatT{ 1 } + sign * unitNormal.x * unitNormal.x * a, sign * b, -sign * unitNormal.x);
    outBitangent = Vector3<InFloatT>(b, sign + unitNormal.y * unitNormal.y * a, -unitNormal.y);
}

/// @brief Maps a uniform `[0, 1)` scalar onto `[minInclusive, maxExclusive)` while guarding IEEE-754 rounding.
/// @tparam InFloatT Floating-point scalar type.
/// @param[in] unit01 Uniform scalar in `[0, 1)`.
/// @param[in] minInclusive Inclusive lower bound.
/// @param[in] maxExclusive Exclusive upper bound.
/// @return Scaled floating-point value strictly less than @p maxExclusive when `minInclusive < maxExclusive`.
template <gp::concepts::IsFloatingPoint InFloatT>
[[nodiscard]] constexpr InFloatT
    scaleUnitToRange(const InFloatT unit01, const InFloatT minInclusive, const InFloatT maxExclusive) noexcept
{
    if (!isFinite(minInclusive) || !isFinite(maxExclusive) || minInclusive >= maxExclusive)
    {
        return minInclusive;
    }

    const InFloatT span = maxExclusive - minInclusive;
    InFloatT result = InFloatT{ 0 };
    if (isFinite(span))
    {
        result = minInclusive + span * unit01;
    }
    else
    {
        const InFloatT halfMin = minInclusive * InFloatT{ 0.5 };
        const InFloatT halfMax = maxExclusive * InFloatT{ 0.5 };
        result = (halfMin + (halfMax - halfMin) * unit01) * InFloatT{ 2 };
    }

    if (result >= maxExclusive)
    {
        if (!std::is_constant_evaluated())
        {
            return std::nextafter(maxExclusive, minInclusive);
        }
        return minInclusive;
    }
    return result;
}

}   // namespace random_detail

/// @brief Generates a non-deterministic 64-bit seed from monotonic time, CPU cycles, stack address, and an atomic
///        counter.
/// @details Uses only lock-free engine primitives (`gp::platform::Time` and `gp::thread::Atomic`) so it never throws
///          or allocates heap memory. Intended for client sessions or tools that want a fresh starting seed.
/// @return Mixed 64-bit entropy seed.
[[nodiscard]] inline gp::UInt64 makeEntropySeed() noexcept
{
    static gp::thread::Atomic<gp::UInt64> s_entropyCounter{ 0ULL };
    const gp::UInt64 seq = s_entropyCounter.fetchAdd(1ULL, gp::MemoryOrder::Relaxed) + 1ULL;
    const gp::UInt64 ns = gp::platform::Time::nowNanoseconds();
    const gp::UInt64 cycles = gp::platform::Time::cpuCycles();
    const gp::UInt64 stackAddr = gp::hashing::hashPointer(&seq);

    const gp::UInt64 low = gp::hashing::cityHash128To64(ns ^ stackAddr, cycles);
    const gp::UInt64 high = gp::hashing::splitMix64(seq * 0x9E3779B97F4A7C15ULL);
    return gp::hashing::cityHash128To64(low, high);
}

/// @brief Stateless pure-function random sampling utilities inspired by Decima, Anvil, and Frostbite world systems.
/// @details Open-world streaming and parallel jobs cannot rely on mutable global state or draw-order assumptions.
///          Every function here evaluates a deterministic mapping from `(seed, coordinate/index)` to random outputs
///          with zero mutable state and zero thread synchronization.
struct StatelessRandom final
{
public:
    StatelessRandom() = delete;

public:
    /// @brief Computes a deterministic 64-bit seed from an ASCII/UTF-8 string label via `gp::hashing::Fnv1a64`.
    /// @tparam InStrT String-like type satisfying `gp::hashing::concepts::IsCharacterSequence`.
    /// @param[in] label Descriptive seed label (such as a region or generator name).
    /// @return Mixed 64-bit seed.
    template <gp::hashing::concepts::IsCharacterSequence InStrT>
    [[nodiscard]] static constexpr gp::UInt64 hashSeed(const InStrT& label) noexcept
    {
        return gp::hashing::splitMix64(gp::hashing::Fnv1a64::hash(label) ^ 0x9E3779B97F4A7C15ULL);
    }

    /// @brief Computes a deterministic 64-bit seed from a null-terminated C-string label.
    /// @param[in] label Null-terminated ASCII/UTF-8 string (safe when `nullptr`).
    /// @return Mixed 64-bit seed.
    [[nodiscard]] static constexpr gp::UInt64 hashSeed(const char* const label) noexcept
    {
        return hashSeed(gp::StringView(label));
    }

    /// @brief Derives an independent child 64-bit seed from @p baseSeed and a numeric @p salt.
    /// @param[in] baseSeed Parent 64-bit seed.
    /// @param[in] salt Domain or pass salt.
    /// @return Derived 64-bit seed.
    [[nodiscard]] static constexpr gp::UInt64 deriveSeed(const gp::UInt64 baseSeed, const gp::UInt64 salt) noexcept
    {
        return gp::hashing::splitMix64(
            gp::hashing::cityHash128To64(baseSeed ^ 0x9E3779B97F4A7C15ULL, salt ^ 0xBF58476D1CE4E5B9ULL)
        );
    }

    /// @brief Derives an independent child 64-bit seed from @p baseSeed and a descriptive string @p label.
    /// @tparam InStrT String-like type satisfying `gp::hashing::concepts::IsCharacterSequence`.
    /// @param[in] baseSeed Parent 64-bit seed.
    /// @param[in] label Child domain label.
    /// @return Derived 64-bit seed.
    template <gp::hashing::concepts::IsCharacterSequence InStrT>
    [[nodiscard]] static constexpr gp::UInt64 deriveSeed(const gp::UInt64 baseSeed, const InStrT& label) noexcept
    {
        return deriveSeed(baseSeed, gp::hashing::Fnv1a64::hash(label));
    }

    /// @brief Derives an independent child 64-bit seed from @p baseSeed and a null-terminated C-string @p label.
    /// @param[in] baseSeed Parent 64-bit seed.
    /// @param[in] label Null-terminated child domain label.
    /// @return Derived 64-bit seed.
    [[nodiscard]] static constexpr gp::UInt64 deriveSeed(const gp::UInt64 baseSeed, const char* const label) noexcept
    {
        return deriveSeed(baseSeed, gp::StringView(label));
    }

    /// @brief Samples a deterministic 64-bit unsigned integer at 1D coordinate @p index.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element, entity, or step index.
    /// @return Uniform 64-bit unsigned integer.
    [[nodiscard]] static constexpr gp::UInt64 sampleUInt64(const gp::UInt64 seed, const gp::UInt64 index) noexcept
    {
        return gp::hashing::splitMix64(
            gp::hashing::cityHash128To64(seed ^ 0x9E3779B97F4A7C15ULL, index ^ 0x94D049BB133111EBULL)
        );
    }

    /// @brief Samples a deterministic 32-bit unsigned integer at 1D coordinate @p index.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element, entity, or step index.
    /// @return Uniform 32-bit unsigned integer.
    [[nodiscard]] static constexpr gp::UInt32 sampleUInt32(const gp::UInt64 seed, const gp::UInt64 index) noexcept
    {
        return static_cast<gp::UInt32>(sampleUInt64(seed, index) >> 32u);
    }

    /// @brief Samples a deterministic 64-bit unsigned integer at 2D grid coordinate `(x, y)`.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] x Integer X coordinate.
    /// @param[in] y Integer Y coordinate.
    /// @return Uniform 64-bit unsigned integer.
    [[nodiscard]] static constexpr gp::UInt64
        sampleUInt64(const gp::UInt64 seed, const gp::Int32 x, const gp::Int32 y) noexcept
    {
        const gp::UInt64 cellHash = gp::hashing::hashGrid2D64(x, y);
        return gp::hashing::splitMix64(
            gp::hashing::cityHash128To64(seed ^ 0x9E3779B97F4A7C15ULL, cellHash ^ 0xBF58476D1CE4E5B9ULL)
        );
    }

    /// @brief Samples a deterministic 32-bit unsigned integer at 2D grid coordinate `(x, y)`.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] x Integer X coordinate.
    /// @param[in] y Integer Y coordinate.
    /// @return Uniform 32-bit unsigned integer.
    [[nodiscard]] static constexpr gp::UInt32
        sampleUInt32(const gp::UInt64 seed, const gp::Int32 x, const gp::Int32 y) noexcept
    {
        return static_cast<gp::UInt32>(sampleUInt64(seed, x, y) >> 32u);
    }

    /// @brief Samples a deterministic 64-bit unsigned integer at 3D voxel coordinate `(x, y, z)`.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] x Integer X coordinate.
    /// @param[in] y Integer Y coordinate.
    /// @param[in] z Integer Z coordinate.
    /// @return Uniform 64-bit unsigned integer.
    [[nodiscard]] static constexpr gp::UInt64
        sampleUInt64(const gp::UInt64 seed, const gp::Int32 x, const gp::Int32 y, const gp::Int32 z) noexcept
    {
        const gp::UInt64 cellHash = gp::hashing::hashGrid3D64(x, y, z);
        return gp::hashing::splitMix64(
            gp::hashing::cityHash128To64(seed ^ 0x9E3779B97F4A7C15ULL, cellHash ^ 0xBF58476D1CE4E5B9ULL)
        );
    }

    /// @brief Samples a deterministic 32-bit unsigned integer at 3D voxel coordinate `(x, y, z)`.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] x Integer X coordinate.
    /// @param[in] y Integer Y coordinate.
    /// @param[in] z Integer Z coordinate.
    /// @return Uniform 32-bit unsigned integer.
    [[nodiscard]] static constexpr gp::UInt32
        sampleUInt32(const gp::UInt64 seed, const gp::Int32 x, const gp::Int32 y, const gp::Int32 z) noexcept
    {
        return static_cast<gp::UInt32>(sampleUInt64(seed, x, y, z) >> 32u);
    }

    /// @brief Samples a uniform `Float32` in `[0.0f, 1.0f)` at 1D coordinate @p index.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element coordinate.
    /// @return Uniform `Float32` in `[0.0f, 1.0f)`.
    [[nodiscard]] static constexpr gp::Float32 sampleFloat(const gp::UInt64 seed, const gp::UInt64 index) noexcept
    {
        return bitsToFloat01(sampleUInt32(seed, index));
    }

    /// @brief Samples a uniform `Float32` in `[0.0f, 1.0f)` at 2D grid coordinate `(x, y)`.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] x Integer X coordinate.
    /// @param[in] y Integer Y coordinate.
    /// @return Uniform `Float32` in `[0.0f, 1.0f)`.
    [[nodiscard]] static constexpr gp::Float32
        sampleFloat(const gp::UInt64 seed, const gp::Int32 x, const gp::Int32 y) noexcept
    {
        return bitsToFloat01(sampleUInt32(seed, x, y));
    }

    /// @brief Samples a uniform `Float32` in `[0.0f, 1.0f)` at 3D voxel coordinate `(x, y, z)`.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] x Integer X coordinate.
    /// @param[in] y Integer Y coordinate.
    /// @param[in] z Integer Z coordinate.
    /// @return Uniform `Float32` in `[0.0f, 1.0f)`.
    [[nodiscard]] static constexpr gp::Float32
        sampleFloat(const gp::UInt64 seed, const gp::Int32 x, const gp::Int32 y, const gp::Int32 z) noexcept
    {
        return bitsToFloat01(sampleUInt32(seed, x, y, z));
    }

    /// @brief Samples a uniform `Float64` in `[0.0, 1.0)` at 1D coordinate @p index.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element coordinate.
    /// @return Uniform `Float64` in `[0.0, 1.0)`.
    [[nodiscard]] static constexpr gp::Float64 sampleDouble(const gp::UInt64 seed, const gp::UInt64 index) noexcept
    {
        return bitsToDouble01(sampleUInt64(seed, index));
    }

    /// @brief Samples a uniform `Float64` in `[0.0, 1.0)` at 2D grid coordinate `(x, y)`.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] x Integer X coordinate.
    /// @param[in] y Integer Y coordinate.
    /// @return Uniform `Float64` in `[0.0, 1.0)`.
    [[nodiscard]] static constexpr gp::Float64
        sampleDouble(const gp::UInt64 seed, const gp::Int32 x, const gp::Int32 y) noexcept
    {
        return bitsToDouble01(sampleUInt64(seed, x, y));
    }

    /// @brief Samples a uniform `Float64` in `[0.0, 1.0)` at 3D voxel coordinate `(x, y, z)`.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] x Integer X coordinate.
    /// @param[in] y Integer Y coordinate.
    /// @param[in] z Integer Z coordinate.
    /// @return Uniform `Float64` in `[0.0, 1.0)`.
    [[nodiscard]] static constexpr gp::Float64
        sampleDouble(const gp::UInt64 seed, const gp::Int32 x, const gp::Int32 y, const gp::Int32 z) noexcept
    {
        return bitsToDouble01(sampleUInt64(seed, x, y, z));
    }

    /// @brief Evaluates a deterministic Bernoulli trial at @p index with probability @p probability.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element coordinate.
    /// @param[in] probability Success probability in `[0.0f, 1.0f]` (defaults to `0.5f`).
    /// @return True with probability @p probability, false otherwise.
    [[nodiscard]] static constexpr bool
        sampleBool(const gp::UInt64 seed, const gp::UInt64 index, const gp::Float32 probability = 0.5f) noexcept
    {
        if (probability <= 0.0f)
        {
            return false;
        }
        if (probability >= 1.0f)
        {
            return true;
        }
        return sampleFloat(seed, index) < probability;
    }

    /// @brief Samples a uniform integer in the closed interval `[minInclusive, maxInclusive]` via Philox4x32.
    /// @tparam InIntT Integral type (`gp::Int32`, `gp::UInt32`, `gp::Int64`, `gp::UInt64`).
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element coordinate.
    /// @param[in] minInclusive Inclusive lower bound.
    /// @param[in] maxInclusive Inclusive upper bound.
    /// @return Uniform integer in `[minInclusive, maxInclusive]`.
    template <gp::concepts::IsIntegral InIntT>
    requires(!gp::concepts::IsBool<InIntT>) [[nodiscard]] static constexpr InIntT sampleRange(
        const gp::UInt64 seed, const gp::UInt64 index, const InIntT minInclusive, const InIntT maxInclusive
    ) noexcept
    {
        GP_ASSERT(
            minInclusive <= maxInclusive && "StatelessRandom::sampleRange() requires minInclusive <= maxInclusive."
        );
        GP_ASSERT(
            minInclusive <= maxInclusive && "StatelessRandom::sampleRange() requires minInclusive <= maxInclusive."
        );
        if (minInclusive >= maxInclusive)
        {
            return minInclusive;
        }

        using UnsignedT = std::make_unsigned_t<InIntT>;
        const UnsignedT span = static_cast<UnsignedT>(maxInclusive) - static_cast<UnsignedT>(minInclusive);

        gp::UInt32 lane = 0u;
        const Philox4x32::BlockType block = Philox4x32::evaluate(seed, index, 0ULL);

        if constexpr (sizeof(InIntT) <= 4u)
        {
            const gp::UInt32 uSpan = static_cast<gp::UInt32>(span);
            if (uSpan == 0xFFFFFFFFu)
            {
                return static_cast<InIntT>(block[0]);
            }
            const gp::UInt32 offset = random_detail::boundedUInt32(
                uSpan + 1u,
                [&block, &lane, seed, index]() noexcept -> gp::UInt32
            {
                if (lane < 4u)
                {
                    return block[lane++];
                }
                const Philox4x32::BlockType extra = Philox4x32::evaluate(seed, index, static_cast<gp::UInt64>(lane));
                ++lane;
                return extra[0];
            }
            );
            return static_cast<InIntT>(static_cast<UnsignedT>(minInclusive) + static_cast<UnsignedT>(offset));
        }
        else
        {
            const gp::UInt64 uSpan = static_cast<gp::UInt64>(span);
            if (uSpan == 0xFFFFFFFFFFFFFFFFULL)
            {
                const gp::UInt64 full = (static_cast<gp::UInt64>(block[0]) << 32u) | static_cast<gp::UInt64>(block[1]);
                return static_cast<InIntT>(full);
            }
            const gp::UInt64 offset = random_detail::boundedUInt64(
                uSpan + 1ULL,
                [&block, &lane, seed, index]() noexcept -> gp::UInt64
            {
                if (lane + 1u < 4u)
                {
                    const gp::UInt64 hi = static_cast<gp::UInt64>(block[lane]);
                    const gp::UInt64 lo = static_cast<gp::UInt64>(block[lane + 1u]);
                    lane += 2u;
                    return (hi << 32u) | lo;
                }
                const Philox4x32::BlockType extra = Philox4x32::evaluate(seed, index, static_cast<gp::UInt64>(lane));
                lane += 2u;
                return (static_cast<gp::UInt64>(extra[0]) << 32u) | static_cast<gp::UInt64>(extra[1]);
            }
            );
            return static_cast<InIntT>(static_cast<UnsignedT>(minInclusive) + static_cast<UnsignedT>(offset));
        }
    }

    /// @brief Samples a uniform floating-point scalar in `[minInclusive, maxExclusive)` at @p index.
    /// @tparam InFloatT Floating-point scalar type (`gp::Float32` or `gp::Float64`).
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element coordinate.
    /// @param[in] minInclusive Inclusive lower bound.
    /// @param[in] maxExclusive Exclusive upper bound.
    /// @return Uniform scalar in `[minInclusive, maxExclusive)`.
    template <gp::concepts::IsFloatingPoint InFloatT>
    [[nodiscard]] static constexpr InFloatT sampleRange(
        const gp::UInt64 seed, const gp::UInt64 index, const InFloatT minInclusive, const InFloatT maxExclusive
    ) noexcept
    {
        if constexpr (sizeof(InFloatT) == 4u)
        {
            return random_detail::scaleUnitToRange(sampleFloat(seed, index), minInclusive, maxExclusive);
        }
        else
        {
            return random_detail::scaleUnitToRange(
                static_cast<InFloatT>(sampleDouble(seed, index)), minInclusive, maxExclusive
            );
        }
    }

    /// @brief Samples a uniform value inside a valid `gp::Interval<InValueT>` at coordinate @p index.
    /// @tparam InValueT Numeric element type of @p interval.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element coordinate.
    /// @param[in] interval Source interval (`[min, max]` for integers, `[min, max)` for floats).
    /// @return Sampled value within @p interval, or `InValueT{}` if @p interval is invalid.
    template <gp::concepts::IsNumeric InValueT>
    [[nodiscard]] static constexpr InValueT
        sampleInInterval(const gp::UInt64 seed, const gp::UInt64 index, const gp::Interval<InValueT>& interval) noexcept
    {
        GP_ASSERT(interval.isValid() && "StatelessRandom::sampleInInterval() requires a valid interval.");
        if (!interval.isValid())
        {
            return InValueT{};
        }
        return sampleRange(seed, index, interval.min, interval.max);
    }

    /// @brief Samples four independent uniform `Float32` values in `[0.0f, 1.0f)` in one Philox4x32 call.
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element coordinate.
    /// @param[in] passId Optional pass or dimension salt (defaults to `0ULL`).
    /// @return Array of four uniform `Float32` values in `[0.0f, 1.0f)`.
    [[nodiscard]] static constexpr gp::Array<gp::Float32, 4u>
        samplePhiloxFloat4(const gp::UInt64 seed, const gp::UInt64 index, const gp::UInt64 passId = 0ULL) noexcept
    {
        return Philox4x32::evaluateFloat4(seed, index, passId);
    }

    /// @brief Samples a uniform 2D unit direction on the unit circle at `(seed, index)`.
    /// @tparam InFloatT Floating-point scalar type (defaults to `gp::Float32`).
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element coordinate.
    /// @return Unit-length `Vector2<InFloatT>`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] static inline Vector2<InFloatT>
        sampleUnitCircle(const gp::UInt64 seed, const gp::UInt64 index) noexcept
    {
        const gp::UInt64 bits = sampleUInt64(seed, index);
        const InFloatT angle = static_cast<InFloatT>(bitsToDouble01(bits)) * Constants<InFloatT>::twoPi;
        return Vector2<InFloatT>(std::cos(angle), std::sin(angle));
    }

    /// @brief Samples a uniform 3D unit direction on the unit sphere at `(seed, index)`.
    /// @tparam InFloatT Floating-point scalar type (defaults to `gp::Float32`).
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element coordinate.
    /// @return Unit-length `Vector3<InFloatT>`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] static inline Vector3<InFloatT>
        sampleUnitSphere(const gp::UInt64 seed, const gp::UInt64 index) noexcept
    {
        const Philox4x32::BlockType block = Philox4x32::evaluate(seed, index, 0ULL);
        const gp::UInt64 u0Bits = (static_cast<gp::UInt64>(block[0]) << 32u) | static_cast<gp::UInt64>(block[1]);
        const gp::UInt64 u1Bits = (static_cast<gp::UInt64>(block[2]) << 32u) | static_cast<gp::UInt64>(block[3]);

        const InFloatT z = static_cast<InFloatT>(bitsToSignedDouble(u0Bits));
        const InFloatT phi = static_cast<InFloatT>(bitsToDouble01(u1Bits)) * Constants<InFloatT>::twoPi;
        const InFloatT r = gp::math::sqrt(gp::math::max(InFloatT{ 0 }, InFloatT{ 1 } - z * z));
        return Vector3<InFloatT>(r * std::cos(phi), r * std::sin(phi), z);
    }

    /// @brief Samples a uniform 3D unit direction in the hemisphere oriented around @p normal.
    /// @tparam InFloatT Floating-point scalar type (defaults to `gp::Float32`).
    /// @param[in] seed Base 64-bit seed.
    /// @param[in] index Element coordinate.
    /// @param[in] normal Hemisphere orientation axis.
    /// @return Unit-length `Vector3<InFloatT>` satisfying `dot(result, normal) >= 0`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] static inline Vector3<InFloatT>
        sampleUnitHemisphere(const gp::UInt64 seed, const gp::UInt64 index, const Vector3<InFloatT>& normal) noexcept
    {
        const Vector3<InFloatT> dir = sampleUnitSphere<InFloatT>(seed, index);
        return ((dir | normal) < InFloatT{ 0 }) ? -dir : dir;
    }
};

}   // namespace gp::math
