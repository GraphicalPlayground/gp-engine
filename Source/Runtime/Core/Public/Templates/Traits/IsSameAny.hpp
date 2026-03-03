// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/TemplateTypes.hpp"
#include "Templates/Traits/TypeRelationships.hpp"

namespace GP
{

// clang-format off

/// @brief Checks whether T is the same type as any type in the pack Ts.
///
///   TIsSameAny_V<int, float, int, double>   →  true
///   TIsSameAny_V<bool, float, int, double>  →  false
///
/// @tparam T The type to test.
/// @tparam Ts The candidate types.
template <typename T, typename... Ts>
struct TIsSameAny : TBoolConstant<(TIsSame_V<T, Ts> || ...)> {};

/// @brief Variable template for TIsSameAny.
template <typename T, typename... Ts>
inline constexpr bool TIsSameAny_V = TIsSameAny<T, Ts...>::Value;

/// @brief Checks whether T is the same type as ALL types in the pack Ts.
///
///   TIsSameAll_V<int, int, int>        →  true
///   TIsSameAll_V<int, int, float>      →  false
///
/// @tparam T The reference type.
/// @tparam Ts The types that must all match T.
template <typename T, typename... Ts>
struct TIsSameAll : TBoolConstant<(TIsSame_V<T, Ts> && ...)> {};

/// @brief Variable template for TIsSameAll.
template <typename T, typename... Ts>
inline constexpr bool TIsSameAll_V = TIsSameAll<T, Ts...>::Value;

// clang-format on

}   // namespace GP
