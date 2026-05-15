// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Fundamental.hpp"

namespace gp::concepts
{

/// @brief Concept to check if an arithmetic type is signed.
template <typename T>
concept IsSigned = IsArithmetic<T> && std::is_signed_v<T>;

/// @brief Concept to check if an arithmetic type is unsigned.
template <typename T>
concept IsUnsigned = IsArithmetic<T> && std::is_unsigned_v<T>;

}   // namespace gp::concepts
