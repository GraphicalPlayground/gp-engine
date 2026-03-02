// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/EnableIf.hpp"

namespace GP
{

namespace Detail
{

// clang-format off

/// @brief Adds an `lvalue` reference to the type if possible, otherwise returns the type itself.
template <typename T> auto TryAddLValueRef(int) -> TVoid<T&>;
template <typename T> auto TryAddLValueRef(...) -> TVoid<T>;

/// @brief Adds an `lvalue` reference to the type if possible, otherwise returns the type itself.
template <typename T> auto TryAddRValueRef(int) -> TVoid<T&&>;
template <typename T> auto TryAddRValueRef(...) -> TVoid<T>;

// clang-format on

}   // namespace Detail

// clang-format off

/// @brief Type trait to add an lvalue reference qualifier to a type.
///        Types that cannot be referenced (e.g. void) are returned unchanged.
template <typename T> struct TAddLValueRef { using Type = typename decltype(Detail::TryAddLValueRef<T>(0))::Type; };

/// @brief Helper alias for adding an lvalue reference.
template <typename T>
using TAddLValueRef_T = typename TAddLValueRef<T>::Type;

/// @brief Type trait to add an rvalue reference qualifier to a type.
///        Types that cannot be referenced (e.g. void) are returned unchanged.
template <typename T> struct TAddRValueRef { using Type = typename decltype(Detail::TryAddRValueRef<T>(0))::Type; };

/// @brief Helper alias for adding an rvalue reference.
template <typename T>
using TAddRValueRef_T = typename TAddRValueRef<T>::Type;

// clang-format on

}   // namespace GP
