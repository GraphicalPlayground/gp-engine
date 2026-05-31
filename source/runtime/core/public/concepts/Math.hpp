// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Fundamental.hpp"

namespace gp::concepts
{

/// @brief Concept to check if an arithmetic type is signed.
/// @note Pedagogical Note: Signed types are preferred for general arithmetic (like positions or offsets)
/// as they handle negative results naturally. Be aware that signed overflow is "Undefined Behavior" (UB) in C++.
template <typename T>
concept IsSigned = IsArithmetic<T> && std::is_signed_v<T>;

/// @brief Concept to check if an arithmetic type is unsigned.
/// @note Unsigned types are ideal for bitmasks and representing sizes or counts. Unlike signed types,
/// unsigned overflow (wrapping) is well-defined in C++, which is useful for hashing algorithms.
template <typename T>
concept IsUnsigned = IsArithmetic<T> && std::is_unsigned_v<T>;

}   // namespace gp::concepts
