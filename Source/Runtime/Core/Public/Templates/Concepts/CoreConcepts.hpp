// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/Utility.hpp"
#include "Templates/Traits/CompositeTraits.hpp"
#include "Templates/Traits/TypeCategories.hpp"
#include "Templates/Traits/TypeRelationships.hpp"

namespace GP
{

// clang-format off

/// @brief Satisfied when T and U are the same type (including cv-qualifiers).
template <typename T, typename U>
concept CSameAs = TIsSame_V<T, U> && TIsSame_V<U, T>;

/// @brief Satisfied when TDerived is publicly derived from TBase, or they are the same type.
template <typename TDerived, typename TBase>
concept CDerivedFrom =
    TIsBaseOf_V<TBase, TDerived> &&
    TIsConvertible_V<const volatile TDerived*, const volatile TBase*>;

/// @brief Satisfied when TFrom is implicitly convertible to TTo.
template <typename TFrom, typename TTo>
concept CConvertibleTo =
    TIsConvertible_V<TFrom, TTo> &&
    requires { static_cast<TTo>(GP::Declval<TFrom>()); };

/// @brief Satisfied when T is an integral type.
template <typename T>
concept CIntegral = TIsIntegral_V<T>;

/// @brief Satisfied when T is a signed integral type.
template <typename T>
concept CSignedIntegral = CIntegral<T> && TIsSigned_V<T>;

/// @brief Satisfied when T is an unsigned integral type.
template <typename T>
concept CUnsignedIntegral = CIntegral<T> && TIsUnsigned_V<T>;

/// @brief Satisfied when T is a floating-point type.
template <typename T>
concept CFloatingPoint = TIsFloating_V<T>;

/// @brief Satisfied when T is any arithmetic type (integral or floating-point).
template <typename T>
concept CArithmetic = TIsArithmetic_V<T>;

/// @brief Satisfied when T is an enumeration type.
template <typename T>
concept CEnum = TIsEnum_V<T>;

/// @brief Satisfied when T is a class type (union or non-union).
template <typename T>
concept CClass = TIsClass_V<T>;

/// @brief Satisfied when T is a pointer type.
template <typename T>
concept CPointer = TIsPointer_V<T>;

/// @brief Satisfied when T is void.
template <typename T>
concept CVoid = TIsVoid_V<T>;

/// @brief Satisfied when T is not void.
template <typename T>
concept CNonVoid = !TIsVoid_V<T>;

/// @brief Satisfied when T is a reference type.
template <typename T>
concept CReference = TIsReference_V<T>;

/// @brief Satisfied when T is a constant type.
template <typename T>
concept CConst = TIsConst_V<T>;

/// @brief Satisfied when T is a non-constant type.
template <typename T>
concept CNonConst = !TIsConst_V<T>;

/// @brief Satisfied when T is a volatile type.
template <typename T>
concept CVolatile = TIsVolatile_V<T>;

/// @brief Satisfied when T is a non-volatile type.
template <typename T>
concept CNonVolatile = !TIsVolatile_V<T>;

/// @brief Satisfied when T can be constructed from Args.
template <typename T, typename... TArgs>
concept CConstructibleFrom = TIsConstructible_V<T, TArgs...>;

/// @brief Satisfied when T can be default-initialised.
template <typename T>
concept CDefaultInitializable = CConstructibleFrom<T> &&
    requires { T{}; ::new T; };

/// @brief Satisfied when T can be move-constructed.
template <typename T>
concept CMoveConstructible = CConstructibleFrom<T, T> && CConvertibleTo<T, T>;

/// @brief Satisfied when T can be copy-constructed.
template <typename T>
concept CCopyConstructible =
    CMoveConstructible<T>                                          &&
    CConstructibleFrom<T,       T&> && CConvertibleTo<      T&, T> &&
    CConstructibleFrom<T, const T&> && CConvertibleTo<const T&, T> &&
    CConstructibleFrom<T, const T > && CConvertibleTo<const T,  T>;

/// @brief Satisfied when TFn can be invoked with TArgs.
template <typename TFn, typename... TArgs>
concept CInvocable = TIsInvocable_V<TFn, TArgs...>;

/// @brief Same as CInvocable but additionally requires semantic equality preservation.
template <typename TFn, typename... TArgs>
concept CRegularInvocable = CInvocable<TFn, TArgs...>;

/// @brief Satisfied when TFn invoked with TArgs returns bool.
template <typename TFn, typename... TArgs>
concept CPredicate = CRegularInvocable<TFn, TArgs...> &&
    TIsConvertible_V<TInvokeResult_T<TFn, TArgs...>, bool>;

/// @brief Satisfied when TFn is a binary relation over T and U.
template <typename TFn, typename T, typename U>
concept CRelation =
    CPredicate<TFn, T, T> && CPredicate<TFn, U, U> &&
    CPredicate<TFn, T, U> && CPredicate<TFn, U, T>;

/// @brief Satisfied when TFn is an equivalence relation.
template <typename TFn, typename T, typename U>
concept CEquivalenceRelation = CRelation<TFn, T, U>;

/// @brief Satisfied when TFn is a strict weak ordering relation.
template <typename TFn, typename T, typename U>
concept CStrictWeakOrder = CRelation<TFn, T, U>;

// clang-format on

}   // namespace GP
