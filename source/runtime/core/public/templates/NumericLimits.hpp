// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <limits>
#include <type_traits>

namespace gp
{

/// @brief Compile-time numeric bounds and IEEE-754 representation properties for scalar types.
/// @details Wraps `std::numeric_limits` under a uniform, allocator-free `gp::` contract while stripping cv-qualifiers
///          and highlighting critical engine distinctions such as `min()` vs `lowest()`:
///          - For integral types, `min()` == `lowest()` (the most negative representable value, or `0` if unsigned).
///          - For floating-point types, `min()` is the smallest *positive* normalized value (`~1.175e-38f` for
///            `Float32`), whereas `lowest()` is the most negative finite value (`-max()`).
///          Always use `lowest()` when initializing bounding-box or min-accumulation sentinels!
/// @tparam T Numeric or scalar type whose limits are queried (cv-qualifiers are ignored).
template <typename T>
struct NumericLimits final
{
private:
    using PlainType = std::remove_cv_t<T>;
    using StdLimits = std::numeric_limits<PlainType>;

public:
    static constexpr bool kIsSpecialized = StdLimits::is_specialized;
    static constexpr bool kIsSigned = StdLimits::is_signed;
    static constexpr bool kIsInteger = StdLimits::is_integer;
    static constexpr bool kIsExact = StdLimits::is_exact;
    static constexpr bool kHasInfinity = StdLimits::has_infinity;
    static constexpr bool kHasQuietNaN = StdLimits::has_quiet_NaN;
    static constexpr bool kHasSignalingNaN = StdLimits::has_signaling_NaN;
    static constexpr bool kIsIEC559 = StdLimits::is_iec559;

    static constexpr Int32 kDigits = StdLimits::digits;
    static constexpr Int32 kDigits10 = StdLimits::digits10;
    static constexpr Int32 kMaxDigits10 = StdLimits::max_digits10;
    static constexpr Int32 kMinExponent = StdLimits::min_exponent;
    static constexpr Int32 kMaxExponent = StdLimits::max_exponent;

public:
    /// @brief Returns the minimum finite positive normal value (floats) or minimum representable value (integers).
    /// @return Minimum positive normal value for floating-point types, or minimum value for integral types.
    [[nodiscard]] static constexpr PlainType min() noexcept
    {
        return StdLimits::min();
    }

    /// @brief Returns the largest finite representable value of @c T.
    /// @return Maximum finite value.
    [[nodiscard]] static constexpr PlainType max() noexcept
    {
        return StdLimits::max();
    }

    /// @brief Returns the most negative finite representable value (equal to `min()` for integers and `-max()` for
    ///        IEEE-754 floats).
    /// @return Lowest finite value of @c T.
    [[nodiscard]] static constexpr PlainType lowest() noexcept
    {
        return StdLimits::lowest();
    }

    /// @brief Returns machine epsilon (`1.0` step above `1.0`) for floating-point types, or `0` for integers.
    /// @return Difference between `1.0` and the next representable value.
    [[nodiscard]] static constexpr PlainType epsilon() noexcept
    {
        return StdLimits::epsilon();
    }

    /// @brief Returns the maximum rounding error in ULP (`0.5` for IEEE-754 round-to-nearest).
    /// @return Maximum rounding error.
    [[nodiscard]] static constexpr PlainType roundError() noexcept
    {
        return StdLimits::round_error();
    }

    /// @brief Returns positive infinity for floating-point types that support it (`0` otherwise).
    /// @return Positive infinity representation.
    [[nodiscard]] static constexpr PlainType infinity() noexcept
    {
        return StdLimits::infinity();
    }

    /// @brief Returns a quiet NaN representation (`0` for integral types).
    /// @return Quiet NaN value.
    [[nodiscard]] static constexpr PlainType quietNaN() noexcept
    {
        return StdLimits::quiet_NaN();
    }

    /// @brief Returns a signaling NaN representation (`0` for integral types).
    /// @return Signaling NaN value.
    [[nodiscard]] static constexpr PlainType signalingNaN() noexcept
    {
        return StdLimits::signaling_NaN();
    }

    /// @brief Returns the smallest positive subnormal value for floats supporting denormals, or `min()` otherwise.
    /// @return Smallest positive subnormal or minimum representable value.
    [[nodiscard]] static constexpr PlainType denormMin() noexcept
    {
        return StdLimits::denorm_min();
    }
};

}   // namespace gp
