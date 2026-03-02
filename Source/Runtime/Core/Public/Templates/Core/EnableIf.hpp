// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

namespace GP
{

// clang-format off

/// @brief SFINAE helper, provides a member type `Type` only when Condition is true.
/// @tparam Condition The compile-time condition.
/// @tparam T The type to expose as `Type` (defaults to void).
template <bool Condition, typename T = void> struct TEnableIf          {};
template <typename T>                        struct TEnableIf<true, T> { using Type = T; };

/// @brief Helper alias for TEnableIf; substitution failure when Condition is false.
template <bool Condition, typename T = void>
using TEnableIf_T = typename TEnableIf<Condition, T>::Type;

/// @brief Maps any set of types to void. Useful for detecting ill-formed expressions (void_t).
/// @tparam ...Ts Any types; all are discarded.
template <typename... Ts> struct TVoidHelper { using Type = void; };

/// @brief Alias that maps any well-formed type list to void.
template <typename... Ts>
using TVoid = typename TVoidHelper<Ts...>::Type;

// clang-format on

}   // namespace GP
