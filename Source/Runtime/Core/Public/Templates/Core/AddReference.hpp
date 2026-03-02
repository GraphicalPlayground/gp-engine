// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/EnableIf.hpp"

namespace GP
{

namespace Detail
{

// clang-format off

/// @brief Minimal holder used to smuggle a type through a decltype expression.
///        Unlike TVoid, preserves the wrapped type via its ::Type member.
template <typename T> struct TTypeHolder { using Type = T; };

/// @brief Adds an `lvalue` reference to the type if possible, otherwise returns the type itself.
template <typename T> auto TryAddLValueRef(int) -> TTypeHolder<T&>;
template <typename T> auto TryAddLValueRef(...) -> TTypeHolder<T>;

/// @brief Adds an `rvalue` reference to the type if possible, otherwise returns the type itself.
template <typename T> auto TryAddRValueRef(int) -> TTypeHolder<T&&>;
template <typename T> auto TryAddRValueRef(...) -> TTypeHolder<T>;

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
