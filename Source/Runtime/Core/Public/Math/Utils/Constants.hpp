// Copyright (c) - Mallory SCOTTON. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Concepts.hpp"
#include <cmath>
#include <limits>
#include <numbers>

namespace GP::Math
{

/// \brief Mathematical constants.
/// \tparam T The type of the constant, must be a floating-point type.
/// \note This struct provides a collection of commonly used mathematical constants and thresholds for floating-point
/// computations, ensuring precision and consistency across the codebase.
template <Concepts::IsFloatingPoint T>
struct Constants final
{
    static constexpr T Pi = std::numbers::pi_v<T>;
    static constexpr T TwoPi = static_cast<T>(2.0) * std::numbers::pi_v<T>;
    static constexpr T HalfPi = std::numbers::pi_v<T> / static_cast<T>(2.0);
    static constexpr T QuarterPi = std::numbers::pi_v<T> / static_cast<T>(4.0);
    static constexpr T InvPi = std::numbers::inv_pi_v<T>;
    static constexpr T InvTwoPi = static_cast<T>(1.0) / (static_cast<T>(2.0) * std::numbers::pi_v<T>);

    static constexpr T DegToRad = std::numbers::pi_v<T> / static_cast<T>(180.0);
    static constexpr T RadToDeg = static_cast<T>(180.0) / std::numbers::pi_v<T>;

    static constexpr T E = std::numbers::e_v<T>;
    static constexpr T Sqrt2 = std::numbers::sqrt2_v<T>;
    static constexpr T Sqrt3 = static_cast<T>(1.73205080756887729352);
    static constexpr T InvSqrt2 = static_cast<T>(1.0) / std::numbers::sqrt2_v<T>;
    static constexpr T Ln2 = std::numbers::ln2_v<T>;
    static constexpr T Ln10 = std::numbers::ln10_v<T>;
    static constexpr T Log2E = std::numbers::log2e_v<T>;
    static constexpr T Log10E = std::numbers::log10e_v<T>;
    static constexpr T Phi = std::numbers::phi_v<T>;

    static constexpr T Epsilon = std::numeric_limits<T>::epsilon();
    static constexpr T Infinity = std::numeric_limits<T>::infinity();

    static constexpr T SmallNumber = static_cast<T>(1.e-8);
    static constexpr T KindaSmallNumber = static_cast<T>(1.e-4);
    static constexpr T BigNumber = static_cast<T>(3.4e+38);
    static constexpr T Delta = static_cast<T>(0.00001);

    /// \brief Threshold values for various mathematical comparisons and operations.
    /// \note These thresholds help manage floating-point precision issues in geometric computations.
    struct Thresholds final
    {
        /// \brief Lengths of normalized vectors (These are half their maximum values to assure that dot products with
        /// normalized vectors don't overflow).
        static constexpr T Normal = static_cast<T>(0.0001);

        /// \brief Thickness of plane for front/back/inside test
        static constexpr T PointOnPlane = static_cast<T>(0.10);

        /// \brief Thickness of polygon side's side-plane for point-inside/outside/on side test
        static constexpr T PointOnSide = static_cast<T>(0.20);

        /// \brief Two points are same if within this distance
        static constexpr T PointsAreSame = static_cast<T>(0.00002);

        /// \brief Two points are near if within this distance and can be combined if imprecise math is ok
        static constexpr T PointsAreNear = static_cast<T>(0.015);

        /// \brief Two normal points are same if within this distance
        static constexpr T NormalsAreSame = static_cast<T>(0.00002);

        /// \brief Two UV are same if within this threshold (1.0f/1024f) Making this too large results in incorrect CSG
        /// classification and disaster
        static constexpr T UVsAreSame = static_cast<T>(0.0009765625);

        /// \brief Two vectors are near if within this distance and can be combined if imprecise math is ok. Making this
        /// too large results in lighting problems due to inaccurate texture coordinates
        static constexpr T VectorsAreNear = static_cast<T>(0.0004);

        /// \brief A plane splits a polygon in half
        static constexpr T SplitPolygonWithPlane = static_cast<T>(0.25);

        /// \brief A plane exactly splits a polygon
        static constexpr T SplitPolygonPrecisely = static_cast<T>(0.01);

        /// \brief Size of a unit normal that is considered "zero", squared
        static constexpr T ZeroNormSquared = static_cast<T>(0.0001);

        /// \brief Two unit vectors are parallel if abs(A dot B) is greater than or equal to this. This is roughly
        /// cosine(1.0 degrees).
        static constexpr T NormalsAreParallel = static_cast<T>(0.999845);

        /// \brief Two unit vectors are orthogonal (perpendicular) if abs(A dot B) is less than or equal this. This is
        /// roughly cosine(89.0 degrees).
        static constexpr T NormalsAreOrthogonal = static_cast<T>(0.017455);

        /// \brief Allowed error for a normalized vector (against squared magnitude)
        static constexpr T VectorNormalized = static_cast<T>(0.01);

        /// \brief  Allowed error for a normalized quaternion (against squared magnitude)
        static constexpr T QuaternionNormalized = static_cast<T>(0.01);
    };
};

}   // namespace GP::Math
