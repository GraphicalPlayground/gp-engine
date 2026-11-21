// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Fundamental.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "maths/base/Arithmetic.hpp"
#include "templates/NumericLimits.hpp"
#include <numeric>

namespace gp
{

/// @brief Compact scalar interval storing lower (`min`) and upper (`max`) bounds for resource lifetimes and spans.
/// @details Default construction sets `min` to the type maximum and `max` to its lowest value so `include()` starts
///          cleanly from an empty sentinel without an extra validity boolean (`sizeof(Interval<T>) == 2 * sizeof(T)`).
///          Provides closed `[min, max]` helpers for inclusive pass spans (`contains`, `overlaps`, `intersect`) and
///          half-open `[min, max)` helpers (`containsHalfOpen`, `overlapsHalfOpen`, `intersectHalfOpen`) for byte
///          ranges and adjacent non-overlapping allocations.
/// @tparam T Numeric scalar type (`gp::UInt32`, `gp::Int32`, `gp::Float32`, etc.).
template <concepts::IsNumeric T>
struct Interval final
{
public:
    using ElementType = T;

public:
    /// @brief Lower endpoint (inclusive in both closed and half-open queries).
    T min = NumericLimits<T>::max();

    /// @brief Upper endpoint (inclusive for closed queries, exclusive for half-open queries).
    T max = NumericLimits<T>::lowest();

public:
    /// @brief Constructs an empty sentinel interval where `isValid()` is `false`.
    constexpr Interval() noexcept = default;

    /// @brief Constructs an interval from explicit lower and upper endpoints.
    /// @param[in] inMin Lower bound.
    /// @param[in] inMax Upper bound.
    constexpr Interval(const T inMin, const T inMax) noexcept
        : min(inMin)
        , max(inMax)
    {}

    /// @brief Compares two intervals by exact endpoint equality.
    [[nodiscard]] constexpr bool operator==(const Interval&) const noexcept = default;

public:
    /// @brief Returns the empty sentinel interval (`min > max`).
    /// @return Invalid empty interval ready for incremental expansion via `include()`.
    [[nodiscard]] static constexpr Interval empty() noexcept
    {
        return {};
    }

    /// @brief Constructs a closed interval `[inMin, inMax]`.
    /// @param[in] inMin Inclusive start bound.
    /// @param[in] inMax Inclusive end bound.
    /// @return Constructed interval.
    [[nodiscard]] static constexpr Interval closed(const T inMin, const T inMax) noexcept
    {
        return Interval(inMin, inMax);
    }

    /// @brief Constructs a half-open interval `[inBegin, inEnd)`.
    /// @param[in] inBegin Inclusive start offset.
    /// @param[in] inEnd Exclusive end offset.
    /// @return Constructed interval.
    [[nodiscard]] static constexpr Interval halfOpen(const T inBegin, const T inEnd) noexcept
    {
        return Interval(inBegin, inEnd);
    }

    /// @brief Computes the bounding union containing both @p a and @p b, skipping any invalid input.
    /// @param[in] a First interval.
    /// @param[in] b Second interval.
    /// @return Merged closed hull, or `empty()` if neither input is valid.
    [[nodiscard]] static constexpr Interval merge(const Interval& a, const Interval& b) noexcept
    {
        Interval result = a;
        result.include(b);
        return result;
    }

    /// @brief Computes the closed intersection `[max(a.min, b.min), min(a.max, b.max)]`.
    /// @param[in] a First closed interval.
    /// @param[in] b Second closed interval.
    /// @return Overlapping closed region, or `empty()` if the intervals do not overlap.
    [[nodiscard]] static constexpr Interval intersect(const Interval& a, const Interval& b) noexcept
    {
        if (!a.isValid() || !b.isValid())
        {
            return empty();
        }
        const T lo = math::max(a.min, b.min);
        const T hi = math::min(a.max, b.max);
        return (lo <= hi) ? Interval(lo, hi) : empty();
    }

    /// @brief Computes the half-open intersection `[max(a.min, b.min), min(a.max, b.max))`.
    /// @details Adjacent intervals such as `[0, 64)` and `[64, 128)` return `empty()`.
    /// @param[in] a First half-open interval.
    /// @param[in] b Second half-open interval.
    /// @return Non-empty overlapping half-open region, or `empty()` when disjoint or adjacent.
    [[nodiscard]] static constexpr Interval intersectHalfOpen(const Interval& a, const Interval& b) noexcept
    {
        if (!a.isValidHalfOpen() || !b.isValidHalfOpen())
        {
            return empty();
        }
        const T lo = math::max(a.min, b.min);
        const T hi = math::min(a.max, b.max);
        return (lo < hi) ? Interval(lo, hi) : empty();
    }

public:
    /// @brief Reports whether the interval holds at least one valid closed endpoint (`min <= max`).
    /// @return `true` if `min <= max`, `false` for empty or inverted bounds.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return min <= max;
    }

    /// @brief Reports whether the interval is invalid or empty (`min > max`).
    /// @return `true` if `min > max`, `false` otherwise.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return min > max;
    }

    /// @brief Reports whether the interval holds a non-empty half-open span (`min < max`).
    /// @return `true` if `min < max`, `false` when zero-length (`min == max`) or invalid.
    [[nodiscard]] constexpr bool isValidHalfOpen() const noexcept
    {
        return min < max;
    }

    /// @brief Returns the span distance `max - min`, or `0` when the interval is empty (`min > max`).
    /// @return Distance between `min` and `max`.
    [[nodiscard]] constexpr T size() const noexcept
    {
        if (!isValid())
        {
            return static_cast<T>(0);
        }
        return static_cast<T>(max - min);
    }

    /// @brief Returns the overflow-safe midpoint between `min` and `max`, or `0` when the interval is invalid.
    /// @return Midpoint value in `[min, max]`.
    [[nodiscard]] constexpr T center() const noexcept
    {
        if (!isValid())
        {
            return static_cast<T>(0);
        }
        return std::midpoint(min, max);
    }

    /// @brief Tests closed membership (`min <= value && value <= max`).
    /// @param[in] value Element to test.
    /// @return `true` if @p value lies inside the closed interval.
    [[nodiscard]] constexpr bool contains(const T value) const noexcept
    {
        return isValid() && (value >= min) && (value <= max);
    }

    /// @brief Tests whether @p other fits completely inside this closed interval.
    /// @param[in] other Sub-interval to test.
    /// @return `true` if both intervals are valid and @p other lies within `[min, max]`.
    [[nodiscard]] constexpr bool contains(const Interval& other) const noexcept
    {
        return isValid() && other.isValid() && (other.min >= min) && (other.max <= max);
    }

    /// @brief Tests half-open membership (`min <= value && value < max`).
    /// @param[in] value Element to test.
    /// @return `true` if @p value lies inside `[min, max)`.
    [[nodiscard]] constexpr bool containsHalfOpen(const T value) const noexcept
    {
        return (value >= min) && (value < max);
    }

    /// @brief Tests whether @p other lies completely inside this half-open interval `[min, max)`.
    /// @param[in] other Sub-interval to test.
    /// @return `true` if both intervals are valid half-open spans and @p other lies within `[min, max)`.
    [[nodiscard]] constexpr bool containsHalfOpen(const Interval& other) const noexcept
    {
        return isValidHalfOpen() && other.isValidHalfOpen() && (other.min >= min) && (other.max <= max);
    }

    /// @brief Tests whether two closed intervals share at least one endpoint or interior value.
    /// @param[in] other Interval to compare against.
    /// @return `true` if both intervals are valid and intersect inclusively.
    [[nodiscard]] constexpr bool overlaps(const Interval& other) const noexcept
    {
        return isValid() && other.isValid() && (min <= other.max) && (max >= other.min);
    }

    /// @brief Tests whether two half-open intervals share a non-zero overlap (`min < other.max && max > other.min`).
    /// @param[in] other Half-open interval to compare against.
    /// @return `true` if both half-open spans overlap strictly.
    [[nodiscard]] constexpr bool overlapsHalfOpen(const Interval& other) const noexcept
    {
        return isValidHalfOpen() && other.isValidHalfOpen() && (min < other.max) && (max > other.min);
    }

    /// @brief Returns the closed intersection of `*this` and @p other.
    /// @param[in] other Interval to intersect with.
    /// @return Overlapping closed region, or `empty()`.
    [[nodiscard]] constexpr Interval intersect(const Interval& other) const noexcept
    {
        return intersect(*this, other);
    }

    /// @brief Returns the half-open intersection of `*this` and @p other.
    /// @param[in] other Half-open interval to intersect with.
    /// @return Overlapping half-open region, or `empty()`.
    [[nodiscard]] constexpr Interval intersectHalfOpen(const Interval& other) const noexcept
    {
        return intersectHalfOpen(*this, other);
    }

    /// @brief Expands the interval bounds if needed so @p value is included.
    /// @param[in] value Point to absorb into `[min, max]`.
    constexpr void include(const T value) noexcept
    {
        if (!isValid())
        {
            min = value;
            max = value;
            return;
        }
        min = math::min(min, value);
        max = math::max(max, value);
    }

    /// @brief Expands the interval bounds to absorb a valid @p other interval.
    /// @param[in] other Interval to merge into `*this`.
    constexpr void include(const Interval& other) noexcept
    {
        if (!other.isValid())
        {
            return;
        }
        if (!isValid())
        {
            *this = other;
            return;
        }
        min = math::min(min, other.min);
        max = math::max(max, other.max);
    }

    /// @brief Widens a valid interval by subtracting @p amount from `min` and adding it to `max`.
    /// @param[in] amount Symmetrical expansion amount on each side.
    constexpr void expand(const T amount) noexcept
    {
        if (isValid())
        {
            min = static_cast<T>(min - amount);
            max = static_cast<T>(max + amount);
        }
    }
};

template <concepts::IsNumeric T>
Interval(T, T) -> Interval<T>;

}   // namespace gp
