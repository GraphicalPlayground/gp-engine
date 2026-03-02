// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/TemplateTypes.hpp"
#include "Templates/Traits/TypeCategories.hpp"

namespace GP
{

// clang-format off

/// @brief Checks whether T is an arithmetic type (integral or floating-point).
template <typename T>
struct TIsArithmetic : TBoolConstant<TIsIntegral_V<T> || TIsFloating_V<T>> {};
template <typename T> inline constexpr bool TIsArithmetic_V = TIsArithmetic<T>::Value;

/// @brief Checks whether T is a fundamental type (arithmetic, void, or nullptr_t).
template <typename T>
struct TIsFundamental : TBoolConstant<TIsArithmetic_V<T> || TIsVoid_V<T> || TIsNullPointer_V<T>> {};
template <typename T> inline constexpr bool TIsFundamental_V = TIsFundamental<T>::Value;

/// @brief Checks whether T is a scalar type.
///        Scalars: arithmetic, enum, pointer, member pointer, nullptr_t.
template <typename T>
struct TIsScalar : TBoolConstant<
    TIsArithmetic_V<T>    ||
    TIsEnum_V<T>          ||
    TIsPointer_V<T>       ||
    TIsMemberPointer_V<T> ||
    TIsNullPointer_V<T>> {};
template <typename T> inline constexpr bool TIsScalar_V = TIsScalar<T>::Value;

/// @brief Checks whether T is an object type (anything that is not a function, reference, or void).
template <typename T>
struct TIsObject : TBoolConstant<
    !TIsFunction_V<T>   &&
    !TIsReference_V<T>  &&
    !TIsVoid_V<T>> {};
template <typename T> inline constexpr bool TIsObject_V = TIsObject<T>::Value;

/// @brief Checks whether T is a compound type (anything that is not fundamental).
template <typename T>
struct TIsCompound : TBoolConstant<!TIsFundamental_V<T>> {};
template <typename T> inline constexpr bool TIsCompound_V = TIsCompound<T>::Value;

// clang-format on

}   // namespace GP
