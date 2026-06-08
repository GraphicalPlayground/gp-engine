// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "concepts/Fundamental.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <cmath>             // IWYU pragma: keep
#include <limits>
#include <numbers>

namespace gp::math
{

/// @brief A collection of mathematical constants for a given floating-point type.
/// @tparam T The floating-point type for the constants (e.g., float, double, long double).
template <concepts::IsFloatingPoint T>
struct constants final
{
    // Pi-related
    static constexpr T pi = std::numbers::pi_v<T>;
    static constexpr T twoPi = static_cast<T>(2.0) * std::numbers::pi_v<T>;
    static constexpr T halfPi = std::numbers::pi_v<T> / static_cast<T>(2.0);
    static constexpr T quarterPi = std::numbers::pi_v<T> / static_cast<T>(4.0);
    static constexpr T invPi = std::numbers::inv_pi_v<T>;
    static constexpr T invTwoPi = static_cast<T>(1.0) / (static_cast<T>(2.0) * std::numbers::pi_v<T>);

    // Angular conversions
    static constexpr T degToRad = std::numbers::pi_v<T> / static_cast<T>(180.0);
    static constexpr T radToDeg = static_cast<T>(180.0) / std::numbers::pi_v<T>;

    // Common irrational numbers
    static constexpr T e = std::numbers::e_v<T>;
    static constexpr T sqrt2 = std::numbers::sqrt2_v<T>;
    static constexpr T sqrt3 = static_cast<T>(1.73205080756887729352);
    static constexpr T invSqrt2 = static_cast<T>(1.0) / std::numbers::sqrt2_v<T>;
    static constexpr T ln2 = std::numbers::ln2_v<T>;
    static constexpr T ln10 = std::numbers::ln10_v<T>;
    static constexpr T log2E = std::numbers::log2e_v<T>;
    static constexpr T log10E = std::numbers::log10e_v<T>;
    static constexpr T phi = std::numbers::phi_v<T>;

    // Floating point thresholds
    static constexpr T epsilon = std::numeric_limits<T>::epsilon();
    static constexpr T infinity = std::numeric_limits<T>::infinity();

    // Engine specific sizing & tolerances
    static constexpr T smallNumber = static_cast<T>(1.e-8);
    static constexpr T kindaSmallNumber = static_cast<T>(1.e-4);
    static constexpr T bigNumber = static_cast<T>(3.4e+38);
    static constexpr T delta = static_cast<T>(0.00001);
};

}   // namespace gp::math
