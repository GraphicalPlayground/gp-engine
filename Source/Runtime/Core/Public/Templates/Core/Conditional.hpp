// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

namespace GP
{

// clang-format off

/// @brief Selects one of two types at compile time based on a boolean condition.
/// @tparam Condition The compile-time boolean selector.
/// @tparam TTrue The type chosen when Condition is true.
/// @tparam TFalse The type chosen when Condition is false.
template <bool Condition, typename TTrue, typename TFalse> struct TConditional                       { using Type = TTrue;  };
template <typename TTrue, typename TFalse>                 struct TConditional<false, TTrue, TFalse> { using Type = TFalse; };

/// @brief Helper alias for compile-time type selection.
template <bool Condition, typename TTrue, typename TFalse>
using TConditional_T = typename TConditional<Condition, TTrue, TFalse>::Type;

// clang-format on

}   // namespace GP
