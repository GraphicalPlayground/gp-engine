// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "hashing/HashingConcepts.hpp"
#include "hashing/HashingTypes.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Constants.hpp"
#include "maths/base/FloatingPoint.hpp"
#include "maths/base/Interpolation.hpp"
#include "maths/random/RandomConcepts.hpp"
#include "maths/random/RandomForward.hpp"   // IWYU pragma: keep
#include "maths/random/RandomTypes.hpp"
#include "maths/random/StatelessRandom.hpp"
#include "maths/random/Xoshiro256.hpp"
#include "maths/vector/Vector2.hpp"
#include "maths/vector/Vector3.hpp"
#include "maths/vector/Vector4.hpp"
#include "templates/Interval.hpp"
#include "templates/Pair.hpp"
#include <cmath>
#include <ranges>
#include <type_traits>
#include <utility>

namespace gp::math
{

/// @brief Shared CRTP mixin providing one unified distribution, geometric, and container RNG API across streams.
/// @details Both `BasicRandomStream<InEngine>` and `DynamicRandomStream` inherit from this class so every uniform
///          range, statistical distribution, spatial sampler, and container utility behaves identically regardless of
///          whether the backing PRNG is chosen at compile time or runtime.
/// @tparam InDerived Concrete stream subclass exposing `engineNextUInt32()` and `engineNextUInt64()`.
template <typename InDerived>
class RandomStreamOperations
{
public:
    /// @section Raw & Boolean Generation

    /// @brief Returns a uniform 32-bit unsigned integer from the backing stream.
    /// @return Uniform 32-bit integer in `[0, UINT32_MAX]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 nextUInt32() noexcept
    {
        return streamDerived().engineNextUInt32();
    }

    /// @brief Returns a uniform 64-bit unsigned integer from the backing stream.
    /// @return Uniform 64-bit integer in `[0, UINT64_MAX]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 nextUInt64() noexcept
    {
        return streamDerived().engineNextUInt64();
    }

    /// @brief Returns a uniform unbiased boolean value (`50%` true, `50%` false).
    /// @details Tests the most significant bit of a 32-bit draw so 32-bit LCG/PCG low-bit artifacts cannot bias it.
    /// @return `true` or `false` with equal probability.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr bool nextBool() noexcept
    {
        return (nextUInt32() >> 31u) != 0u;
    }

    /// @brief Evaluates a Bernoulli trial returning `true` with probability @p probability.
    /// @param[in] probability Threshold in `[0.0f, 1.0f]` (`<= 0` always false, `>= 1` always true).
    /// @return `true` with probability @p probability.
    [[nodiscard]] constexpr bool nextBool(const gp::Float32 probability) noexcept
    {
        if (probability <= 0.0f)
        {
            return false;
        }
        if (probability >= 1.0f)
        {
            return true;
        }
        return nextFloat() < probability;
    }

    /// @brief Evaluates a double-precision Bernoulli trial returning `true` with probability @p probability.
    /// @param[in] probability Threshold in `[0.0, 1.0]` (`<= 0` always false, `>= 1` always true).
    /// @return `true` with probability @p probability.
    [[nodiscard]] constexpr bool nextBool(const gp::Float64 probability) noexcept
    {
        if (probability <= 0.0)
        {
            return false;
        }
        if (probability >= 1.0)
        {
            return true;
        }
        return nextDouble() < probability;
    }

public:
    /// @section Uniform Floating-Point Generation

    /// @brief Returns a uniform `Float32` in the half-open interval `[0.0f, 1.0f)`.
    /// @return Uniform `Float32` in `[0.0f, 1.0f)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 nextFloat() noexcept
    {
        return bitsToFloat01(nextUInt32());
    }

    /// @brief Returns a uniform `Float32` in the closed interval `[0.0f, 1.0f]`.
    /// @return Uniform `Float32` in `[0.0f, 1.0f]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 nextFloatInclusive() noexcept
    {
        return bitsToFloat01Inclusive(nextUInt32());
    }

    /// @brief Returns a uniform bipolar `Float32` in `[-1.0f, 1.0f]`.
    /// @return Uniform `Float32` in `[-1.0f, 1.0f]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 nextSignedFloat() noexcept
    {
        return bitsToSignedFloat(nextUInt32());
    }

    /// @brief Returns a uniform bipolar `Float32` in `[-1.0f, 1.0f)`.
    /// @return Uniform `Float32` in `[-1.0f, 1.0f)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float32 nextSignedFloatHalfOpen() noexcept
    {
        return bitsToSignedFloatHalfOpen(nextUInt32());
    }

    /// @brief Returns a uniform `Float64` in the half-open interval `[0.0, 1.0)`.
    /// @return Uniform `Float64` in `[0.0, 1.0)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float64 nextDouble() noexcept
    {
        return bitsToDouble01(nextUInt64());
    }

    /// @brief Returns a uniform `Float64` in the closed interval `[0.0, 1.0]`.
    /// @return Uniform `Float64` in `[0.0, 1.0]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float64 nextDoubleInclusive() noexcept
    {
        return bitsToDouble01Inclusive(nextUInt64());
    }

    /// @brief Returns a uniform bipolar `Float64` in `[-1.0, 1.0]`.
    /// @return Uniform `Float64` in `[-1.0, 1.0]`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float64 nextSignedDouble() noexcept
    {
        return bitsToSignedDouble(nextUInt64());
    }

    /// @brief Returns a uniform bipolar `Float64` in `[-1.0, 1.0)`.
    /// @return Uniform `Float64` in `[-1.0, 1.0)`.
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::Float64 nextSignedDoubleHalfOpen() noexcept
    {
        return bitsToSignedDoubleHalfOpen(nextUInt64());
    }

    /// @brief Returns a uniform floating-point scalar of type @p InFloatT in `[0, 1)`.
    /// @tparam InFloatT Floating-point type (`gp::Float32` or `gp::Float64`).
    /// @return Uniform scalar in `[0, 1)`.
    template <gp::concepts::IsFloatingPoint InFloatT>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr InFloatT nextUnitScalar() noexcept
    {
        if constexpr (sizeof(InFloatT) == 4u)
        {
            return nextFloat();
        }
        else
        {
            return static_cast<InFloatT>(nextDouble());
        }
    }

public:
    /// @section Unbiased Bounded Integer & Float Ranges

    /// @brief Returns an unbiased uniform 32-bit integer in `[0, upperExclusive)`.
    /// @details Uses Lemire's rejection method instead of `%` to avoid modulo bias on non-power-of-two bounds.
    /// @param[in] upperExclusive Exclusive upper bound (`<= 1` returns `0`).
    /// @return Uniform 32-bit integer in `[0, upperExclusive)`.
    [[nodiscard]] constexpr gp::UInt32 nextBoundedUInt32(const gp::UInt32 upperExclusive) noexcept
    {
        return random_detail::boundedUInt32(
            upperExclusive,
            [this]() noexcept -> gp::UInt32
        {
            return nextUInt32();
        }
        );
    }

    /// @brief Returns an unbiased uniform 64-bit integer in `[0, upperExclusive)`.
    /// @param[in] upperExclusive Exclusive upper bound (`<= 1` returns `0`).
    /// @return Uniform 64-bit integer in `[0, upperExclusive)`.
    [[nodiscard]] constexpr gp::UInt64 nextBoundedUInt64(const gp::UInt64 upperExclusive) noexcept
    {
        return random_detail::boundedUInt64(
            upperExclusive,
            [this]() noexcept -> gp::UInt64
        {
            return nextUInt64();
        }
        );
    }

    /// @brief Returns an unbiased uniform array or container index in `[0, count)`.
    /// @param[in] count Number of elements (`<= 1` safely returns `0`).
    /// @return Uniform index in `[0, count)`.
    [[nodiscard]] constexpr gp::USize nextIndex(const gp::USize count) noexcept
    {
        if constexpr (sizeof(gp::USize) == 8u)
        {
            return static_cast<gp::USize>(nextBoundedUInt64(static_cast<gp::UInt64>(count)));
        }
        else
        {
            return static_cast<gp::USize>(nextBoundedUInt32(static_cast<gp::UInt32>(count)));
        }
    }

    /// @brief Returns an unbiased uniform integer in the closed interval `[minInclusive, maxInclusive]`.
    /// @details Handles full signed and unsigned ranges (such as `[INT32_MIN, INT32_MAX]`) without signed overflow.
    /// @tparam InIntT Integral type (`gp::Int32`, `gp::UInt32`, `gp::Int64`, `gp::UInt64`, etc.).
    /// @param[in] minInclusive Inclusive lower bound.
    /// @param[in] maxInclusive Inclusive upper bound (must be `>= minInclusive`).
    /// @return Uniform integer in `[minInclusive, maxInclusive]`.
    template <gp::concepts::IsIntegral InIntT>
    requires(!gp::concepts::IsBool<InIntT>)
    [[nodiscard]] constexpr InIntT nextRange(const InIntT minInclusive, const InIntT maxInclusive) noexcept
    {
        GP_ASSERT(minInclusive <= maxInclusive && "RandomStream::nextRange() requires minInclusive <= maxInclusive.");
        if (minInclusive >= maxInclusive)
        {
            return minInclusive;
        }

        using UnsignedT = std::make_unsigned_t<InIntT>;
        const UnsignedT span = static_cast<UnsignedT>(maxInclusive) - static_cast<UnsignedT>(minInclusive);

        if constexpr (sizeof(InIntT) <= 4u)
        {
            const gp::UInt32 uSpan = static_cast<gp::UInt32>(span);
            if (uSpan == 0xFFFFFFFFu)
            {
                return static_cast<InIntT>(nextUInt32());
            }
            const gp::UInt32 offset = nextBoundedUInt32(uSpan + 1u);
            return static_cast<InIntT>(static_cast<UnsignedT>(minInclusive) + static_cast<UnsignedT>(offset));
        }
        else
        {
            const gp::UInt64 uSpan = static_cast<gp::UInt64>(span);
            if (uSpan == 0xFFFFFFFFFFFFFFFFULL)
            {
                return static_cast<InIntT>(nextUInt64());
            }
            const gp::UInt64 offset = nextBoundedUInt64(uSpan + 1ULL);
            return static_cast<InIntT>(static_cast<UnsignedT>(minInclusive) + static_cast<UnsignedT>(offset));
        }
    }

    /// @brief Returns a uniform floating-point value in the half-open interval `[minInclusive, maxExclusive)`.
    /// @tparam InFloatT Floating-point type (`gp::Float32` or `gp::Float64`).
    /// @param[in] minInclusive Inclusive lower bound.
    /// @param[in] maxExclusive Exclusive upper bound (must be `>= minInclusive`).
    /// @return Uniform value in `[minInclusive, maxExclusive)`.
    template <gp::concepts::IsFloatingPoint InFloatT>
    [[nodiscard]] constexpr InFloatT nextRange(const InFloatT minInclusive, const InFloatT maxExclusive) noexcept
    {
        GP_ASSERT(minInclusive <= maxExclusive && "RandomStream::nextRange() requires minInclusive <= maxExclusive.");
        return random_detail::scaleUnitToRange(nextUnitScalar<InFloatT>(), minInclusive, maxExclusive);
    }

    /// @brief Samples a uniform value inside a valid `gp::Interval<InValueT>`.
    /// @details Closed `[min, max]` for integral intervals and half-open `[min, max)` for floating-point intervals.
    /// @tparam InValueT Numeric element type of @p interval.
    /// @param[in] interval Source interval to sample within.
    /// @return Sampled value within @p interval, or `InValueT{}` if @p interval is invalid.
    template <gp::concepts::IsNumeric InValueT>
    [[nodiscard]] constexpr InValueT nextInInterval(const gp::Interval<InValueT>& interval) noexcept
    {
        GP_ASSERT(interval.isValid() && "RandomStream::nextInInterval() requires a valid interval.");
        if (!interval.isValid())
        {
            return InValueT{};
        }
        return nextRange(interval.min, interval.max);
    }

public:
    /// @section Statistical & Procedural Distributions

    /// @brief Returns `-1` or `+1` with equal probability (`50%` each).
    /// @tparam InNumericT Signed numeric type (`gp::Int32`, `gp::Float32`, `gp::Float64`, etc.).
    /// @return `InNumericT{ 1 }` or `InNumericT{ -1 }`.
    template <gp::concepts::IsSigned InNumericT = gp::Int32>
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr InNumericT nextSign() noexcept
    {
        return nextBool() ? InNumericT{ 1 } : InNumericT{ -1 };
    }

    /// @brief Generates a normally distributed sample `N(mean, stdDev^2)` via the Marsaglia polar method.
    /// @details Uses only square roots and logarithms, avoiding trigonometric calls for portable determinism.
    /// @note Does not cache the second polar variate inside the stream so `state()` captures the full generator state
    ///       for replays and rollback. Call `nextGaussianPair()` when you want both independent variates.
    /// @tparam InFloatT Floating-point scalar type (`gp::Float32` or `gp::Float64`).
    /// @param[in] mean Distribution center (defaults to `0`).
    /// @param[in] stdDev Non-negative standard deviation (defaults to `1`).
    /// @return Normally distributed sample.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline InFloatT
        nextGaussian(const InFloatT mean = InFloatT{ 0 }, const InFloatT stdDev = InFloatT{ 1 }) noexcept
    {
        return nextGaussianPair<InFloatT>(mean, stdDev).first;
    }

    /// @brief Generates two independent normally distributed samples `N(mean, stdDev^2)` in one polar evaluation.
    /// @tparam InFloatT Floating-point scalar type (`gp::Float32` or `gp::Float64`).
    /// @param[in] mean Distribution center (defaults to `0`).
    /// @param[in] stdDev Non-negative standard deviation (defaults to `1`).
    /// @return `gp::Pair` holding two independent Gaussian variates.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline gp::Pair<InFloatT, InFloatT>
        nextGaussianPair(const InFloatT mean = InFloatT{ 0 }, const InFloatT stdDev = InFloatT{ 1 }) noexcept
    {
        GP_ASSERT(stdDev >= InFloatT{ 0 } && "RandomStream::nextGaussianPair() requires stdDev >= 0.");
        InFloatT u = InFloatT{ 0 };
        InFloatT v = InFloatT{ 0 };
        InFloatT s = InFloatT{ 0 };

        do
        {
            if constexpr (sizeof(InFloatT) == 4u)
            {
                u = nextSignedFloat();
                v = nextSignedFloat();
            }
            else
            {
                u = static_cast<InFloatT>(nextSignedDouble());
                v = static_cast<InFloatT>(nextSignedDouble());
            }
            s = u * u + v * v;
        }
        while (s >= InFloatT{ 1 } || s <= InFloatT{ 0 });

        const InFloatT scale = gp::math::sqrt((InFloatT{ -2 } * std::log(s)) / s) * stdDev;
        return gp::Pair<InFloatT, InFloatT>(mean + u * scale, mean + v * scale);
    }

    /// @brief Generates an exponentially distributed variate `-ln(1 - u) / lambda` for arrival/cooldown timing.
    /// @tparam InFloatT Floating-point scalar type (`gp::Float32` or `gp::Float64`).
    /// @param[in] lambda Positive rate parameter (defaults to `1`).
    /// @return Non-negative exponentially distributed variate.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline InFloatT nextExponential(const InFloatT lambda = InFloatT{ 1 }) noexcept
    {
        GP_ASSERT(lambda > InFloatT{ 0 } && "RandomStream::nextExponential() requires lambda > 0.");
        if (lambda <= InFloatT{ 0 })
        {
            return InFloatT{ 0 };
        }
        const InFloatT oneMinusU = InFloatT{ 1 } - nextUnitScalar<InFloatT>();
        return -std::log(oneMinusU) / lambda;
    }

    /// @brief Generates a log-normally distributed variate `exp(N(mean, stdDev^2))` for scale and drop sizing.
    /// @tparam InFloatT Floating-point scalar type (`gp::Float32` or `gp::Float64`).
    /// @param[in] mean Mean of the underlying normal distribution (defaults to `0`).
    /// @param[in] stdDev Standard deviation of the underlying normal distribution (defaults to `1`).
    /// @return Positive log-normally distributed variate.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline InFloatT
        nextLogNormal(const InFloatT mean = InFloatT{ 0 }, const InFloatT stdDev = InFloatT{ 1 }) noexcept
    {
        return std::exp(nextGaussian<InFloatT>(mean, stdDev));
    }

    /// @brief Generates a sample from a triangular distribution over `[min, max]` peaking at @p mode.
    /// @tparam InFloatT Floating-point scalar type (`gp::Float32` or `gp::Float64`).
    /// @param[in] min Lower bound.
    /// @param[in] max Upper bound (`>= min`).
    /// @param[in] mode Peak mode inside `[min, max]`.
    /// @return Sampled value in `[min, max]`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline InFloatT nextTriangular(const InFloatT min, const InFloatT max, const InFloatT mode) noexcept
    {
        GP_ASSERT(min <= max && mode >= min && mode <= max && "Invalid triangular distribution bounds.");
        if (min >= max)
        {
            return min;
        }

        const InFloatT clampedMode = clamp(mode, min, max);
        const InFloatT span = max - min;
        const InFloatT fc = (clampedMode - min) / span;
        const InFloatT u = nextUnitScalar<InFloatT>();

        if (u < fc)
        {
            return min + gp::math::sqrt(u * span * (clampedMode - min));
        }
        return max - gp::math::sqrt((InFloatT{ 1 } - u) * span * (max - clampedMode));
    }

    /// @brief Generates a power-biased sample in `[min, max)` shaped by `u^exponent`.
    /// @details `exponent > 1` clusters samples near @p min; `0 < exponent < 1` clusters samples near @p max.
    /// @tparam InFloatT Floating-point scalar type (`gp::Float32` or `gp::Float64`).
    /// @param[in] min Lower bound.
    /// @param[in] max Upper bound (`>= min`).
    /// @param[in] exponent Positive shaping exponent (`1` produces a uniform range).
    /// @return Biased sample in `[min, max)`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline InFloatT nextBiased(const InFloatT min, const InFloatT max, const InFloatT exponent) noexcept
    {
        GP_ASSERT(exponent > InFloatT{ 0 } && "RandomStream::nextBiased() requires exponent > 0.");
        if (min >= max)
        {
            return min;
        }
        const InFloatT shaped =
            std::pow(nextUnitScalar<InFloatT>(), gp::math::max(exponent, Constants<InFloatT>::smallNumber));
        return random_detail::scaleUnitToRange(shaped, min, max);
    }

    /// @brief Returns a uniform angle in radians over `[0, 2 * pi)`.
    /// @tparam InFloatT Floating-point scalar type (defaults to `gp::Float32`).
    /// @return Uniform angle in `[0, 2 * pi)`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] constexpr InFloatT nextAngleRadians() noexcept
    {
        return nextUnitScalar<InFloatT>() * Constants<InFloatT>::twoPi;
    }

    /// @brief Returns a uniform angle in degrees over `[0, 360)`.
    /// @tparam InFloatT Floating-point scalar type (defaults to `gp::Float32`).
    /// @return Uniform angle in `[0, 360)`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] constexpr InFloatT nextAngleDegrees() noexcept
    {
        return nextUnitScalar<InFloatT>() * static_cast<InFloatT>(360.0);
    }

public:
    /// @section Geometric & Spatial Sampling

    /// @brief Samples a uniform 2D direction on the circumference of the unit circle.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @return Unit-length `Vector2<InFloatT>`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline Vector2<InFloatT> nextUnitCircle() noexcept
    {
        const InFloatT angle = nextAngleRadians<InFloatT>();
        return Vector2<InFloatT>(std::cos(angle), std::sin(angle));
    }

    /// @brief Samples a uniformly area-distributed 2D point inside the unit disk (`x^2 + y^2 <= 1`).
    /// @details Uses rejection sampling in `[-1, 1]^2` to avoid trigonometric and square-root calls.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @return Point inside the unit circle.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] constexpr Vector2<InFloatT> nextPointInUnitCircle() noexcept
    {
        InFloatT x = InFloatT{ 0 };
        InFloatT y = InFloatT{ 0 };
        do
        {
            if constexpr (sizeof(InFloatT) == 4u)
            {
                x = nextSignedFloat();
                y = nextSignedFloat();
            }
            else
            {
                x = static_cast<InFloatT>(nextSignedDouble());
                y = static_cast<InFloatT>(nextSignedDouble());
            }
        }
        while ((x * x + y * y) > InFloatT{ 1 });

        return Vector2<InFloatT>(x, y);
    }

    /// @brief Samples a uniformly area-distributed 2D point inside a disk centered at @p center with @p radius.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @param[in] center Disk center position.
    /// @param[in] radius Non-negative disk radius.
    /// @return Point inside the circle.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] constexpr Vector2<InFloatT>
        nextPointInCircle(const Vector2<InFloatT>& center, const InFloatT radius) noexcept
    {
        GP_ASSERT(radius >= InFloatT{ 0 } && "RandomStream::nextPointInCircle() requires radius >= 0.");
        return center + nextPointInUnitCircle<InFloatT>() * radius;
    }

    /// @brief Samples a uniformly area-distributed 2D point inside an annulus `[innerRadius, outerRadius]`.
    /// @details Uses inverse-transform sampling on squared radii (`sqrt(lerp(r0^2, r1^2, u))`) for exact area
    ///          uniformity without rejection loops on thin rings.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @param[in] innerRadius Inner radius (`>= 0`).
    /// @param[in] outerRadius Outer radius (`>= innerRadius`).
    /// @return Point inside the annulus centered at `(0, 0)`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline Vector2<InFloatT>
        nextPointInAnnulus(const InFloatT innerRadius, const InFloatT outerRadius) noexcept
    {
        GP_ASSERT(
            innerRadius >= InFloatT{ 0 } && outerRadius >= innerRadius &&
            "RandomStream::nextPointInAnnulus() requires 0 <= innerRadius <= outerRadius."
        );
        if (outerRadius <= InFloatT{ 0 })
        {
            return Vector2<InFloatT>::zero();
        }
        const InFloatT r0 = gp::math::max(InFloatT{ 0 }, innerRadius);
        const InFloatT r1 = gp::math::max(r0, outerRadius);
        const InFloatT rSq = lerp(r0 * r0, r1 * r1, nextUnitScalar<InFloatT>());
        const InFloatT r = gp::math::sqrt(rSq);
        const InFloatT angle = nextAngleRadians<InFloatT>();
        return Vector2<InFloatT>(r * std::cos(angle), r * std::sin(angle));
    }

    /// @brief Samples a uniform 3D unit direction on the surface of the unit sphere.
    /// @details Uses Archimedes' area-preserving cylinder projection (`z` uniform in `[-1, 1]`, `phi` in `[0, 2*pi)`).
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @return Unit-length `Vector3<InFloatT>`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline Vector3<InFloatT> nextUnitSphere() noexcept
    {
        InFloatT z = InFloatT{ 0 };
        if constexpr (sizeof(InFloatT) == 4u)
        {
            z = nextSignedFloat();
        }
        else
        {
            z = static_cast<InFloatT>(nextSignedDouble());
        }
        const InFloatT phi = nextAngleRadians<InFloatT>();
        const InFloatT r = gp::math::sqrt(gp::math::max(InFloatT{ 0 }, InFloatT{ 1 } - z * z));
        return Vector3<InFloatT>(r * std::cos(phi), r * std::sin(phi), z);
    }

    /// @brief Samples a uniformly volume-distributed 3D point inside the unit sphere (`x^2 + y^2 + z^2 <= 1`).
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @return Point inside the unit ball.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] constexpr Vector3<InFloatT> nextPointInUnitSphere() noexcept
    {
        InFloatT x = InFloatT{ 0 };
        InFloatT y = InFloatT{ 0 };
        InFloatT z = InFloatT{ 0 };
        do
        {
            if constexpr (sizeof(InFloatT) == 4u)
            {
                x = nextSignedFloat();
                y = nextSignedFloat();
                z = nextSignedFloat();
            }
            else
            {
                x = static_cast<InFloatT>(nextSignedDouble());
                y = static_cast<InFloatT>(nextSignedDouble());
                z = static_cast<InFloatT>(nextSignedDouble());
            }
        }
        while ((x * x + y * y + z * z) > InFloatT{ 1 });

        return Vector3<InFloatT>(x, y, z);
    }

    /// @brief Samples a uniformly volume-distributed 3D point inside a sphere centered at @p center with @p radius.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @param[in] center Sphere center position.
    /// @param[in] radius Non-negative sphere radius.
    /// @return Point inside the sphere.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] constexpr Vector3<InFloatT>
        nextPointInSphere(const Vector3<InFloatT>& center, const InFloatT radius) noexcept
    {
        GP_ASSERT(radius >= InFloatT{ 0 } && "RandomStream::nextPointInSphere() requires radius >= 0.");
        return center + nextPointInUnitSphere<InFloatT>() * radius;
    }

    /// @brief Samples a uniform 3D unit direction in the hemisphere oriented around @p normal.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @param[in] normal Hemisphere axis (falls back to `+Z` if near zero).
    /// @return Unit-length `Vector3<InFloatT>` with `dot(result, safeNormal) >= 0`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline Vector3<InFloatT> nextUnitHemisphere(const Vector3<InFloatT>& normal) noexcept
    {
        Vector3<InFloatT> n = normal.getSafeNormal();
        if (n.lengthSquared() <= Constants<InFloatT>::smallNumber)
        {
            n = Vector3<InFloatT>::unitZ();
        }
        const Vector3<InFloatT> dir = nextUnitSphere<InFloatT>();
        return ((dir | n) < InFloatT{ 0 }) ? -dir : dir;
    }

    /// @brief Samples a cosine-weighted 3D unit direction over the hemisphere oriented around @p normal.
    /// @details Uses Malley's method: projects a uniform unit-disk sample vertically onto the hemisphere and transforms
    ///          it into the tangent basis around @p normal. Standard importance sampling kernel for diffuse Monte Carlo
    ///          global illumination (`pdf = cos(theta) / pi`).
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @param[in] normal Surface normal (falls back to `+Z` if near zero).
    /// @return Cosine-weighted unit direction in the hemisphere around @p normal.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline Vector3<InFloatT> nextCosineHemisphere(const Vector3<InFloatT>& normal) noexcept
    {
        Vector3<InFloatT> n = normal.getSafeNormal();
        if (n.lengthSquared() <= Constants<InFloatT>::smallNumber)
        {
            n = Vector3<InFloatT>::unitZ();
        }

        const Vector2<InFloatT> disk = nextPointInUnitCircle<InFloatT>();
        const InFloatT z =
            gp::math::sqrt(gp::math::max(InFloatT{ 0 }, InFloatT{ 1 } - disk.x * disk.x - disk.y * disk.y));

        Vector3<InFloatT> tangent;
        Vector3<InFloatT> bitangent;
        random_detail::buildOrthonormalBasis(n, tangent, bitangent);

        return (tangent * disk.x + bitangent * disk.y + n * z).getSafeNormal();
    }

    /// @brief Samples a uniform solid-angle 3D unit direction inside a cone around @p direction.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @param[in] direction Cone central axis (falls back to `+Z` if near zero).
    /// @param[in] halfAngleRadians Cone half-angle in radians (`[0, pi]`).
    /// @return Unit-length `Vector3<InFloatT>` within @p halfAngleRadians of @p direction.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] inline Vector3<InFloatT>
        nextConeDirection(const Vector3<InFloatT>& direction, const InFloatT halfAngleRadians) noexcept
    {
        Vector3<InFloatT> axis = direction.getSafeNormal();
        if (axis.lengthSquared() <= Constants<InFloatT>::smallNumber)
        {
            axis = Vector3<InFloatT>::unitZ();
        }

        const InFloatT clampedAngle = clamp(halfAngleRadians, InFloatT{ 0 }, Constants<InFloatT>::pi);
        const InFloatT cosThetaMax = std::cos(clampedAngle);
        const InFloatT cosTheta = lerp(cosThetaMax, InFloatT{ 1 }, nextUnitScalar<InFloatT>());
        const InFloatT sinTheta = gp::math::sqrt(gp::math::max(InFloatT{ 0 }, InFloatT{ 1 } - cosTheta * cosTheta));
        const InFloatT phi = nextAngleRadians<InFloatT>();

        Vector3<InFloatT> tangent;
        Vector3<InFloatT> bitangent;
        random_detail::buildOrthonormalBasis(axis, tangent, bitangent);

        return (tangent * (sinTheta * std::cos(phi)) + bitangent * (sinTheta * std::sin(phi)) + axis * cosTheta)
            .getSafeNormal();
    }

    /// @brief Samples a uniform 2D point inside the axis-aligned box `[minBounds, maxBounds)`.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @param[in] minBounds Lower corner of the box.
    /// @param[in] maxBounds Upper corner of the box.
    /// @return Sampled `Vector2<InFloatT>` inside the box.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] constexpr Vector2<InFloatT>
        nextPointInBox(const Vector2<InFloatT>& minBounds, const Vector2<InFloatT>& maxBounds) noexcept
    {
        return Vector2<InFloatT>(nextRange(minBounds.x, maxBounds.x), nextRange(minBounds.y, maxBounds.y));
    }

    /// @brief Samples a uniform 3D point inside the axis-aligned box `[minBounds, maxBounds)`.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @param[in] minBounds Lower corner of the box.
    /// @param[in] maxBounds Upper corner of the box.
    /// @return Sampled `Vector3<InFloatT>` inside the box.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] constexpr Vector3<InFloatT>
        nextPointInBox(const Vector3<InFloatT>& minBounds, const Vector3<InFloatT>& maxBounds) noexcept
    {
        return Vector3<InFloatT>(
            nextRange(minBounds.x, maxBounds.x),
            nextRange(minBounds.y, maxBounds.y),
            nextRange(minBounds.z, maxBounds.z)
        );
    }

    /// @brief Samples uniform barycentric coordinates `(u, v, w)` for uniform surface sampling on any triangle.
    /// @details Uses Turk's reflection trick (`if u + v > 1`, reflect `(u, v)` to `(1 - u, 1 - v)`) without
    ///          requiring square roots.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @return `Vector3<InFloatT>` satisfying `u >= 0`, `v >= 0`, `w >= 0`, and `u + v + w == 1`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] constexpr Vector3<InFloatT> nextBarycentric() noexcept
    {
        InFloatT u = nextUnitScalar<InFloatT>();
        InFloatT v = nextUnitScalar<InFloatT>();
        if (u + v > InFloatT{ 1 })
        {
            u = InFloatT{ 1 } - u;
            v = InFloatT{ 1 } - v;
        }
        const InFloatT w = InFloatT{ 1 } - u - v;
        return Vector3<InFloatT>(u, v, w);
    }

    /// @brief Generates a normalized linear RGBA color vector in `[0, 1]^4`.
    /// @tparam InFloatT Floating-point component type (defaults to `gp::Float32`).
    /// @param[in] randomizeAlpha When true, samples alpha in `[0, 1]`; when false, sets alpha to `1`.
    /// @return `Vector4<InFloatT>` representing `(r, g, b, a)`.
    template <gp::concepts::IsFloatingPoint InFloatT = gp::Float32>
    [[nodiscard]] constexpr Vector4<InFloatT> nextColorRgba(const bool randomizeAlpha = false) noexcept
    {
        const InFloatT r = nextUnitScalar<InFloatT>();
        const InFloatT g = nextUnitScalar<InFloatT>();
        const InFloatT b = nextUnitScalar<InFloatT>();
        const InFloatT a = randomizeAlpha ? nextUnitScalar<InFloatT>() : InFloatT{ 1 };
        return Vector4<InFloatT>(r, g, b, a);
    }

public:
    /// @section Byte Buffers, Shuffling & Weighted Selection

    /// @brief Fills a `gp::UInt8` buffer with deterministic pseudorandom bytes (`constexpr`-capable).
    /// @param[out] outBuffer Destination byte array (safe when `nullptr` or zero length).
    /// @param[in] sizeInBytes Number of bytes to write.
    constexpr void fillBytes(gp::UInt8* const outBuffer, const gp::USize sizeInBytes) noexcept
    {
        fillBytesImpl(outBuffer, sizeInBytes);
    }

    /// @brief Fills a `gp::Byte` (`std::byte`) buffer with deterministic pseudorandom bytes (`constexpr`-capable).
    /// @param[out] outBuffer Destination `gp::Byte` array (safe when `nullptr` or zero length).
    /// @param[in] sizeInBytes Number of bytes to write.
    constexpr void fillBytes(gp::Byte* const outBuffer, const gp::USize sizeInBytes) noexcept
    {
        fillBytesImpl(outBuffer, sizeInBytes);
    }

    /// @brief Fills an untyped runtime memory buffer with deterministic pseudorandom bytes.
    /// @param[out] outBuffer Destination memory address (safe when `nullptr` or zero length).
    /// @param[in] sizeInBytes Number of bytes to write.
    void fillBytes(void* const outBuffer, const gp::USize sizeInBytes) noexcept
    {
        fillBytes(static_cast<gp::UInt8*>(outBuffer), sizeInBytes);
    }

    /// @brief Performs an unbiased in-place Fisher-Yates shuffle over a raw contiguous array.
    /// @tparam InElemT Swappable element type.
    /// @param[in,out] data Pointer to the first element (safe when `nullptr` or `count <= 1`).
    /// @param[in] count Number of elements in @p data.
    template <typename InElemT>
    constexpr void shuffle(InElemT* const data, const gp::USize count) noexcept
    {
        if (data == nullptr || count <= 1u)
        {
            return;
        }

        for (gp::USize i = count - 1u; i > 0u; --i)
        {
            const gp::USize j = nextIndex(i + 1u);
            if (i != j)
            {
                std::swap(data[i], data[j]);
            }
        }
    }

    /// @brief Performs an unbiased in-place Fisher-Yates shuffle over any contiguous sized range.
    /// @tparam InRangeT Contiguous sized range type (e.g., `gp::Array`, `gp::Vector`).
    /// @param[in,out] range Mutable range to shuffle.
    template <gp::concepts::IsContiguousRange InRangeT>
    requires gp::concepts::IsSizedRange<InRangeT> constexpr void shuffle(InRangeT& range) noexcept
    {
        shuffle(std::ranges::data(range), static_cast<gp::USize>(std::ranges::size(range)));
    }

    /// @brief Generates a uniform random permutation of indices `[0, count)` into @p outIndices.
    /// @param[out] outIndices Caller-owned array of at least @p count elements.
    /// @param[in] count Number of indices to populate and shuffle.
    constexpr void nextPermutation(gp::USize* const outIndices, const gp::USize count) noexcept
    {
        if (outIndices == nullptr || count == 0u)
        {
            return;
        }
        for (gp::USize i = 0u; i < count; ++i)
        {
            outIndices[i] = i;
        }
        shuffle(outIndices, count);
    }

    /// @brief Selects an index from @p weights with probability proportional to its weight (roulette-wheel selection).
    /// @details Ignores non-positive weights and tracks the last positive-weight index so floating-point rounding near
    ///          the upper boundary never selects a zero-weight entry. Returns `0` if empty or all weights are `<= 0`.
    /// @tparam InWeightT Arithmetic weight type (`gp::Float32`, `gp::Float64`, `gp::UInt32`, `gp::Int32`, etc.).
    /// @param[in] weights Array of element weights.
    /// @param[in] count Number of weights in @p weights.
    /// @return Selected zero-based index in `[0, count)`.
    template <gp::concepts::IsNumeric InWeightT>
    [[nodiscard]] constexpr gp::USize pickIndex(const InWeightT* const weights, const gp::USize count) noexcept
    {
        if (weights == nullptr || count == 0u)
        {
            return 0u;
        }

        gp::Float64 totalWeight = 0.0;
        gp::USize lastPositiveIndex = 0u;
        bool hasPositiveWeight = false;

        for (gp::USize i = 0u; i < count; ++i)
        {
            const gp::Float64 w = static_cast<gp::Float64>(weights[i]);
            if (isFinite(w) && w > 0.0)
            {
                totalWeight += w;
                lastPositiveIndex = i;
                hasPositiveWeight = true;
            }
        }

        if (!hasPositiveWeight || !isFinite(totalWeight) || totalWeight <= 0.0)
        {
            return 0u;
        }

        const gp::Float64 target = nextDouble() * totalWeight;
        gp::Float64 cumulative = 0.0;

        for (gp::USize i = 0u; i < count; ++i)
        {
            const gp::Float64 w = static_cast<gp::Float64>(weights[i]);
            if (!isFinite(w) || w <= 0.0)
            {
                continue;
            }
            cumulative += w;
            if (target < cumulative)
            {
                return i;
            }
        }

        return lastPositiveIndex;
    }

    /// @brief Selects an index from a contiguous sized range of weights (`gp::Array`, `gp::Vector`, etc.).
    /// @tparam InRangeT Contiguous sized range of numeric weights.
    /// @param[in] weights Range of weights.
    /// @return Selected zero-based index.
    template <gp::concepts::IsContiguousRange InRangeT>
    requires gp::concepts::IsSizedRange<InRangeT>
    [[nodiscard]] constexpr gp::USize pickIndex(const InRangeT& weights) noexcept
    {
        return pickIndex(std::ranges::data(weights), static_cast<gp::USize>(std::ranges::size(weights)));
    }

    /// @brief Uniformly selects and returns a const reference to one element of a non-empty contiguous array.
    /// @tparam InElemT Element type.
    /// @param[in] data Pointer to a non-empty array of elements.
    /// @param[in] count Number of elements (`> 0`).
    /// @return Const reference to the chosen element.
    template <typename InElemT>
    [[nodiscard]] constexpr const InElemT& pickElement(const InElemT* const data, const gp::USize count) noexcept
    {
        GP_ASSERT(data != nullptr && count > 0u && "RandomStream::pickElement() requires a non-empty array.");
        return data[nextIndex(count)];
    }

    /// @brief Uniformly selects and returns a mutable reference to one element of a non-empty contiguous array.
    /// @tparam InElemT Element type.
    /// @param[in,out] data Pointer to a non-empty array of elements.
    /// @param[in] count Number of elements (`> 0`).
    /// @return Mutable reference to the chosen element.
    template <typename InElemT>
    [[nodiscard]] constexpr InElemT& pickElement(InElemT* const data, const gp::USize count) noexcept
    {
        GP_ASSERT(data != nullptr && count > 0u && "RandomStream::pickElement() requires a non-empty array.");
        return data[nextIndex(count)];
    }

    /// @brief Uniformly selects a const element from any non-empty contiguous sized range.
    /// @tparam InRangeT Contiguous sized range type.
    /// @param[in] range Non-empty range to sample from.
    /// @return Const reference to the selected element.
    template <gp::concepts::IsContiguousRange InRangeT>
    requires gp::concepts::IsSizedRange<InRangeT>
    [[nodiscard]] constexpr const std::ranges::range_value_t<InRangeT>& pickElement(const InRangeT& range) noexcept
    {
        return pickElement(std::ranges::data(range), static_cast<gp::USize>(std::ranges::size(range)));
    }

    /// @brief Uniformly selects a mutable element from any non-empty contiguous sized range.
    /// @tparam InRangeT Contiguous sized range type.
    /// @param[in,out] range Non-empty mutable range to sample from.
    /// @return Mutable reference to the selected element.
    template <gp::concepts::IsContiguousRange InRangeT>
    requires gp::concepts::IsSizedRange<InRangeT>
    [[nodiscard]] constexpr std::ranges::range_value_t<InRangeT>& pickElement(InRangeT& range) noexcept
    {
        return pickElement(std::ranges::data(range), static_cast<gp::USize>(std::ranges::size(range)));
    }

    /// @brief Selects @p sampleCount elements uniformly without replacement using Knuth's single-pass Algorithm S.
    /// @details Preserves the relative order of selected elements from @p source without heap allocation.
    /// @tparam InElemT Copy-assignable element type.
    /// @param[in] source Input array of candidate elements.
    /// @param[in] sourceCount Number of elements in @p source.
    /// @param[out] outSamples Caller-owned destination array receiving up to @p sampleCount elements.
    /// @param[in] sampleCount Desired number of unique samples.
    /// @return Actual number of elements written (`min(sourceCount, sampleCount)`).
    template <typename InElemT>
    constexpr gp::USize sampleWithoutReplacement(
        const InElemT* const source, const gp::USize sourceCount, InElemT* const outSamples, const gp::USize sampleCount
    ) noexcept
    {
        if (source == nullptr || outSamples == nullptr || sourceCount == 0u || sampleCount == 0u)
        {
            return 0u;
        }

        const gp::USize actualCount = (sampleCount < sourceCount) ? sampleCount : sourceCount;
        gp::USize selected = 0u;

        for (gp::USize i = 0u; i < sourceCount && selected < actualCount; ++i)
        {
            const gp::USize remainingNeeded = actualCount - selected;
            const gp::USize remainingAvailable = sourceCount - i;
            if (nextIndex(remainingAvailable) < remainingNeeded)
            {
                outSamples[selected++] = source[i];
            }
        }

        return selected;
    }

protected:
    /// @brief Protected default constructor restricted to stream subclasses.
    constexpr RandomStreamOperations() noexcept = default;

private:
    template <typename InByteT>
    constexpr void fillBytesImpl(InByteT* const outBuffer, const gp::USize sizeInBytes) noexcept
    {
        if (outBuffer == nullptr || sizeInBytes == 0u)
        {
            return;
        }

        gp::USize offset = 0u;
        while (offset + 8u <= sizeInBytes)
        {
            const gp::UInt64 word = nextUInt64();
            for (gp::USize b = 0u; b < 8u; ++b)
            {
                outBuffer[offset + b] = static_cast<InByteT>((word >> (b * 8u)) & 0xFFULL);
            }
            offset += 8u;
        }

        if (offset < sizeInBytes)
        {
            const gp::UInt64 tailWord = nextUInt64();
            gp::USize shift = 0u;
            while (offset < sizeInBytes)
            {
                outBuffer[offset++] = static_cast<InByteT>((tailWord >> shift) & 0xFFULL);
                shift += 8u;
            }
        }
    }

    [[nodiscard]] constexpr InDerived& streamDerived() noexcept
    {
        return static_cast<InDerived&>(*this);
    }

    [[nodiscard]] constexpr const InDerived& streamDerived() const noexcept
    {
        return static_cast<const InDerived&>(*this);
    }
};

/// @brief Ergonomic deterministic seeded PRNG stream parameterized by any `gp::math::IsRandomEngine` generator.
/// @details Serves as the primary engine-wide random stream (`gp::math::RandomStream` aliases
///          `BasicRandomStream<Xoshiro256PlusPlus>`). Combines zero-allocation state snapshots, deterministic child
///          `fork()`, parallel `split()` via jump polynomials, and the full `RandomStreamOperations` utility API.
/// @tparam InEngine Concrete PRNG engine satisfying `gp::math::IsRandomEngine`.
template <typename InEngine>
class BasicRandomStream final : public RandomStreamOperations<BasicRandomStream<InEngine>>
{
    static_assert(IsRandomEngine<InEngine>, "InEngine must satisfy gp::math::IsRandomEngine.");
    friend class RandomStreamOperations<BasicRandomStream<InEngine>>;

public:
    /// @brief Underlying PRNG engine type.
    using EngineType = InEngine;

    /// @brief Native raw word type emitted by the underlying engine.
    using WordType = typename InEngine::WordType;

    /// @brief Serializable POD state type of the underlying engine.
    using StateType = typename InEngine::StateType;

    /// @brief Algorithm identifier enumerator for the underlying engine.
    static constexpr RandomAlgorithm kAlgorithm = InEngine::kAlgorithm;

    /// @brief Canonical default 64-bit seed.
    static constexpr gp::UInt64 kDefaultSeed = InEngine::kDefaultSeed;

public:
    /// @brief Constructs a stream initialized with the canonical deterministic default seed.
    [[nodiscard]] constexpr BasicRandomStream() noexcept = default;

    /// @brief Constructs a stream seeded with an explicit 64-bit seed and optional stream identifier.
    /// @param[in] seedValue Deterministic 64-bit seed.
    /// @param[in] streamIdValue Optional stream selector (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit BasicRandomStream(
        const gp::UInt64 seedValue, const gp::UInt64 streamIdValue = 0ULL
    ) noexcept
        : m_engine(seedValue, streamIdValue)
    {}

    /// @brief Constructs a stream seeded deterministically from an ASCII/UTF-8 label string.
    /// @tparam InStrT String-like type satisfying `gp::hashing::concepts::IsCharacterSequence`.
    /// @param[in] seedLabel Descriptive seed string.
    /// @param[in] salt Optional 64-bit numeric salt (defaults to `0ULL`).
    template <gp::hashing::concepts::IsCharacterSequence InStrT>
    [[nodiscard]] constexpr explicit BasicRandomStream(const InStrT& seedLabel, const gp::UInt64 salt = 0ULL) noexcept
    {
        m_engine.seedFromString(seedLabel, salt);
    }

    /// @brief Constructs a stream seeded deterministically from a null-terminated C-string label.
    /// @param[in] seedLabel Null-terminated ASCII/UTF-8 string.
    /// @param[in] salt Optional 64-bit numeric salt (defaults to `0ULL`).
    [[nodiscard]] constexpr explicit BasicRandomStream(
        const char* const seedLabel, const gp::UInt64 salt = 0ULL
    ) noexcept
    {
        m_engine.seedFromString(seedLabel, salt);
    }

    /// @brief Constructs a stream seeded from a 128-bit digest (`Hash128`).
    /// @param[in] seedHash 128-bit digest seed.
    [[nodiscard]] constexpr explicit BasicRandomStream(const gp::hashing::Hash128& seedHash) noexcept
    {
        m_engine.seedFromHash(seedHash);
    }

    /// @brief Constructs a stream seeded from a 256-bit digest (`Hash256`).
    /// @param[in] seedHash 256-bit digest seed.
    [[nodiscard]] constexpr explicit BasicRandomStream(const gp::hashing::Hash256& seedHash) noexcept
    {
        m_engine.seedFromHash(seedHash);
    }

    /// @brief Constructs a stream wrapping an existing engine state snapshot.
    /// @param[in] engineInstance Pre-configured engine instance.
    [[nodiscard]] constexpr explicit BasicRandomStream(const InEngine& engineInstance) noexcept
        : m_engine(engineInstance)
    {}

    /// @brief Creates a stream seeded with non-deterministic runtime hardware entropy.
    /// @return Stream seeded from monotonic time, CPU cycle counter, stack address, and an atomic counter.
    [[nodiscard]] static inline BasicRandomStream fromEntropy() noexcept
    {
        return BasicRandomStream(makeEntropySeed());
    }

public:
    /// @brief Seeds the underlying engine with a 64-bit seed and optional stream identifier.
    /// @param[in] seedValue Deterministic 64-bit seed.
    /// @param[in] streamIdValue Optional stream selector (defaults to `0ULL`).
    constexpr void seed(const gp::UInt64 seedValue, const gp::UInt64 streamIdValue = 0ULL) noexcept
    {
        m_engine.seed(seedValue, streamIdValue);
    }

    /// @brief Seeds the underlying engine from a string label and optional numeric salt.
    /// @tparam InStrT String-like type satisfying `gp::hashing::concepts::IsCharacterSequence`.
    /// @param[in] seedLabel Descriptive label string.
    /// @param[in] salt Optional 64-bit numeric salt (defaults to `0ULL`).
    template <gp::hashing::concepts::IsCharacterSequence InStrT>
    constexpr void seedFromString(const InStrT& seedLabel, const gp::UInt64 salt = 0ULL) noexcept
    {
        m_engine.seedFromString(seedLabel, salt);
    }

    /// @brief Seeds the underlying engine from a null-terminated C-string label.
    /// @param[in] seedLabel Null-terminated ASCII/UTF-8 string.
    /// @param[in] salt Optional 64-bit numeric salt (defaults to `0ULL`).
    constexpr void seedFromString(const char* const seedLabel, const gp::UInt64 salt = 0ULL) noexcept
    {
        m_engine.seedFromString(seedLabel, salt);
    }

    /// @brief Seeds the underlying engine from a 128-bit digest (`Hash128`).
    /// @param[in] seedHash 128-bit digest seed.
    constexpr void seedFromHash(const gp::hashing::Hash128& seedHash) noexcept
    {
        m_engine.seedFromHash(seedHash);
    }

    /// @brief Seeds the underlying engine from a 256-bit digest (`Hash256`).
    /// @param[in] seedHash 256-bit digest seed.
    constexpr void seedFromHash(const gp::hashing::Hash256& seedHash) noexcept
    {
        m_engine.seedFromHash(seedHash);
    }

    /// @brief Resets the stream back to its initial seed and stream identifier.
    constexpr void reset() noexcept
    {
        m_engine.reset();
    }

    /// @brief Skips forward by @p steps raw engine word transitions.
    /// @param[in] steps Number of raw generator outputs to discard.
    constexpr void discard(const gp::UInt64 steps) noexcept
    {
        m_engine.discard(steps);
    }

    /// @brief Advances the stream using the underlying engine's fast jump polynomial (`jump()`).
    constexpr void jump() noexcept requires IsJumpableRandomEngine<InEngine>
    {
        m_engine.jump();
    }

    /// @brief Advances the stream using the underlying engine's coarse jump polynomial (`longJump()`).
    constexpr void longJump() noexcept requires IsJumpableRandomEngine<InEngine>
    {
        m_engine.longJump();
    }

    /// @brief Partitions a non-overlapping sibling stream for parallel worker threads.
    /// @details Captures the current stream state for the returned worker stream, then advances `*this` by one
    ///          `jump()` interval so neither the caller nor previously split siblings overlap in sequence.
    /// @return Independent worker stream positioned at the pre-jump state.
    [[nodiscard]] constexpr BasicRandomStream split() noexcept requires IsJumpableRandomEngine<InEngine>
    {
        BasicRandomStream worker(m_engine);
        m_engine.jump();
        return worker;
    }

    /// @brief Derives a deterministic independent child stream without mutating `*this`.
    /// @param[in] subStreamId Numeric child stream identifier.
    /// @return Newly seeded child stream.
    [[nodiscard]] constexpr BasicRandomStream fork(const gp::UInt64 subStreamId) const noexcept
    {
        return BasicRandomStream(m_engine.fork(subStreamId));
    }

    /// @brief Derives a deterministic independent child stream keyed by @p label without mutating `*this`.
    /// @tparam InStrT String-like type satisfying `gp::hashing::concepts::IsCharacterSequence`.
    /// @param[in] label Descriptive child stream name.
    /// @return Newly seeded child stream.
    template <gp::hashing::concepts::IsCharacterSequence InStrT>
    [[nodiscard]] constexpr BasicRandomStream fork(const InStrT& label) const noexcept
    {
        return BasicRandomStream(m_engine.fork(label));
    }

    /// @brief Derives a deterministic child stream keyed by a null-terminated C-string label.
    /// @param[in] label Null-terminated ASCII/UTF-8 label.
    /// @return Newly seeded child stream.
    [[nodiscard]] constexpr BasicRandomStream fork(const char* const label) const noexcept
    {
        return BasicRandomStream(m_engine.fork(label));
    }

    /// @brief Returns a read-only reference to the underlying PRNG engine.
    /// @return Const reference to `InEngine`.
    [[nodiscard]] constexpr const InEngine& engine() const noexcept
    {
        return m_engine;
    }

    /// @brief Returns a mutable reference to the underlying PRNG engine.
    /// @return Mutable reference to `InEngine`.
    [[nodiscard]] constexpr InEngine& engine() noexcept
    {
        return m_engine;
    }

    /// @brief Returns the serializable state snapshot of the underlying engine.
    /// @return Const reference to `StateType`.
    [[nodiscard]] constexpr const StateType& state() const noexcept
    {
        return m_engine.state();
    }

    /// @brief Restores a previously saved engine state snapshot.
    /// @param[in] snapshot State snapshot obtained from `state()`.
    constexpr void setState(const StateType& snapshot) noexcept
    {
        m_engine.setState(snapshot);
    }

    /// @brief Returns the initial 64-bit seed of the underlying engine.
    /// @return Initial 64-bit seed.
    [[nodiscard]] constexpr gp::UInt64 initialSeed() const noexcept
    {
        return m_engine.initialSeed();
    }

    /// @brief Returns the 64-bit stream identifier of the underlying engine.
    /// @return Initial 64-bit stream ID.
    [[nodiscard]] constexpr gp::UInt64 streamId() const noexcept
    {
        return m_engine.streamId();
    }

    /// @brief Streams the underlying engine's algorithm, seed, and state into a `gp::hashing` accumulator.
    /// @tparam InHasher Streaming hasher type.
    /// @param[in,out] hasher Target accumulator.
    template <typename InHasher>
    constexpr void hashAppend(InHasher& hasher) const noexcept
    {
        m_engine.hashAppend(hasher);
    }

    /// @brief Compares two streams by their internal engine state.
    /// @param[in] other Stream to compare against.
    /// @return True if both streams hold identical internal engine state.
    [[nodiscard]] constexpr bool operator==(const BasicRandomStream& other) const noexcept
    {
        return m_engine == other.m_engine;
    }

private:
    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt32 engineNextUInt32() noexcept
    {
        return m_engine.nextUInt32();
    }

    [[nodiscard]] GP_FORCEINLINE_HINT constexpr gp::UInt64 engineNextUInt64() noexcept
    {
        return m_engine.nextUInt64();
    }

private:
    InEngine m_engine{};
};

/// @brief Default engine-wide deterministic PRNG stream backed by `Xoshiro256PlusPlus`.
using RandomStream = BasicRandomStream<Xoshiro256PlusPlus>;

/// @brief Compact 16-byte deterministic PRNG stream backed by `Pcg32` (supports 2^63 streams and O(log n) rewind).
using RandomStream32 = BasicRandomStream<Pcg32>;

/// @brief Compact 16-byte jumpable deterministic PRNG stream backed by `Xoshiro128PlusPlus`.
using CompactRandomStream = BasicRandomStream<Xoshiro128PlusPlus>;

/// @brief High-speed chaotic 64-bit deterministic PRNG stream backed by `Sfc64`.
using FastRandomStream = BasicRandomStream<Sfc64>;

}   // namespace gp::math
