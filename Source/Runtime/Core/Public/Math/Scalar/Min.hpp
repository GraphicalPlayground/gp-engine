// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Concepts/CoreConcepts.hpp"

namespace GP
{

/// @brief Returns the minimum of two values.
/// @tparam T Arithmetic type.
/// @param a First value.
/// @param b Second value.
/// @return The smaller of a and b.
template <CArithmetic T>
GP_NODISCARD constexpr T Min(T a, T b) noexcept
{
    return a < b ? a : b;
}

}   // namespace GP
