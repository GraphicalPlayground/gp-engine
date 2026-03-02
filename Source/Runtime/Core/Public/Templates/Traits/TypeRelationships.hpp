// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Templates/Core/EnableIf.hpp"
#include "Templates/Core/TemplateTypes.hpp"

namespace GP
{

// clang-format off

/// @brief Checks whether two types are exactly the same (including cv-qualifiers).
template <typename T, typename U> struct TIsSame       : FFalseType {};
template <typename T>             struct TIsSame<T, T> : FTrueType  {};

/// @brief Helper variable template for TIsSame.
template <typename T, typename U>
inline constexpr bool TIsSame_V = TIsSame<T, U>::Value;

/// @brief Checks whether Base is a base class of Derived. Returns false for the same type.
template <typename TBase, typename TDerived>
struct TIsBaseOf : TBoolConstant<__is_base_of(TBase, TDerived)> {};

/// @brief Helper variable template for TIsBaseOf.
template <typename TBase, typename TDerived>
inline constexpr bool TIsBaseOf_V = TIsBaseOf<TBase, TDerived>::Value;

/// @brief Checks whether an expression of type TFrom is implicitly convertible to TTo.
template <typename TFrom, typename TTo>
struct TIsConvertible : TBoolConstant<__is_convertible_to(TFrom, TTo)> {};

/// @brief Helper variable template for TIsConvertible.
template <typename TFrom, typename TTo>
inline constexpr bool TIsConvertible_V = TIsConvertible<TFrom, TTo>::Value;

namespace Detail
{

/// @brief Helper struct to determine the result type of invoking a callable with given argument types.
template <typename TFn, typename... TArgs>
struct TInvokeResultHelper
{
private:
    /// @brief Tries to invoke TFn with TArgs and deduces the return type if well-formed.
    template <typename F, typename... A>
    static auto Test(int) -> decltype(static_cast<F&&>(*(F*)nullptr)(static_cast<A&&>(*(A*)nullptr)...));

    /// @brief Fallback overload if the above is not well-formed, resulting in a substitution failure.
    template <typename, typename...>
    static auto Test(...) -> void;

public:
    using Type = decltype(Test<TFn, TArgs...>(0));
};

}   // namespace Detail

/// @brief Computes the return type of invoking TFn with TArgs.
template <typename TFn, typename... TArgs>
struct TInvokeResult
{
    using Type = typename Detail::TInvokeResultHelper<TFn, TArgs...>::Type;
};

/// @brief Helper type alias for TInvokeResult.
template <typename TFn, typename... TArgs>
using TInvokeResult_T = typename TInvokeResult<TFn, TArgs...>::Type;

/// @brief Checks whether TFn can be invoked with TArgs.
template <typename TFn, typename... TArgs>
struct TIsInvocable : TBoolConstant<!TIsSame_V<TInvokeResult_T<TFn, TArgs...>, void> ||
                                     TIsVoid_V<TInvokeResult_T<TFn, TArgs...>>> {};

/// @brief Helper variable template for TIsInvocable.
template <typename TFn, typename... TArgs>
inline constexpr bool TIsInvocable_V = TIsInvocable<TFn, TArgs...>::Value;

/// @brief Checks whether TFn can be invoked with TArgs and returns a type convertible to TRet.
template <typename TRet, typename TFn, typename... TArgs>
struct TIsInvocableR : TBoolConstant<TIsConvertible_V<TInvokeResult_T<TFn, TArgs...>, TRet>> {};

/// @brief Helper variable template for TIsInvocableR.
template <typename TRet, typename TFn, typename... TArgs>
inline constexpr bool TIsInvocableR_V = TIsInvocableR<TRet, TFn, TArgs...>::Value;

/// @brief Checks whether T is constructible from Args.
template <typename T, typename... TArgs>
struct TIsConstructible : TBoolConstant<__is_constructible(T, TArgs...)> {};
template <typename T, typename... TArgs>
inline constexpr bool TIsConstructible_V = TIsConstructible<T, TArgs...>::Value;

/// @brief Checks whether T is nothrow constructible from Args.
template <typename T, typename... TArgs>
struct TIsNothrowConstructible : TBoolConstant<__is_nothrow_constructible(T, TArgs...)> {};
template <typename T, typename... TArgs>
inline constexpr bool TIsNothrowConstructible_V = TIsNothrowConstructible<T, TArgs...>::Value;

/// @brief Checks whether T is default constructible.
template <typename T>
struct TIsDefaultConstructible : TIsConstructible<T> {};
template <typename T>
inline constexpr bool TIsDefaultConstructible_V = TIsDefaultConstructible<T>::Value;

/// @brief Checks whether T is copy constructible.
template <typename T>
struct TIsCopyConstructible : TIsConstructible<T, const T&> {};
template <typename T>
inline constexpr bool TIsCopyConstructible_V = TIsCopyConstructible<T>::Value;

/// @brief Checks whether T is move constructible.
template <typename T>
struct TIsMoveConstructible : TIsConstructible<T, T&&> {};
template <typename T>
inline constexpr bool TIsMoveConstructible_V = TIsMoveConstructible<T>::Value;

/// @brief Checks whether T is assignable from U.
template <typename T, typename U>
struct TIsAssignable : TBoolConstant<__is_assignable(T, U)> {};
template <typename T, typename U>
inline constexpr bool TIsAssignable_V = TIsAssignable<T, U>::Value;

/// @brief Checks whether T is nothrow assignable from U.
template <typename T, typename U>
struct TIsNothrowAssignable : TBoolConstant<__is_nothrow_assignable(T, U)> {};
template <typename T, typename U>
inline constexpr bool TIsNothrowAssignable_V = TIsNothrowAssignable<T, U>::Value;

/// @brief Checks whether T is copy assignable.
template <typename T>
struct TIsCopyAssignable : TIsAssignable<T&, const T&> {};
template <typename T>
inline constexpr bool TIsCopyAssignable_V = TIsCopyAssignable<T>::Value;

/// @brief Checks whether T is move assignable.
template <typename T>
struct TIsMoveAssignable : TIsAssignable<T&, T&&> {};
template <typename T>
inline constexpr bool TIsMoveAssignable_V = TIsMoveAssignable<T>::Value;

/// @brief Checks whether T is destructible (has an accessible non-deleted destructor).
template <typename T>
struct TIsDestructible : TBoolConstant<__is_destructible(T)> {};
template <typename T>
inline constexpr bool TIsDestructible_V = TIsDestructible<T>::Value;

/// @brief Checks whether T's destructor is noexcept.
template <typename T>
struct TIsNothrowDestructible : TBoolConstant<__is_nothrow_destructible(T)> {};
template <typename T>
inline constexpr bool TIsNothrowDestructible_V = TIsNothrowDestructible<T>::Value;

// clang-format on

}   // namespace GP
