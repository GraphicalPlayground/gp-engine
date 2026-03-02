// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Templates/Core/RemoveCV.hpp"
#include "Templates/Core/RemoveCVRef.hpp"
#include "Templates/Core/TemplateTypes.hpp"
#include "Templates/Traits/TypeProperties.hpp"

namespace GP
{

// clang-format off

/// @brief Checks whether T is void.
template <typename T> struct TIsVoid                      : FFalseType {};
template <>           struct TIsVoid<void>                : FTrueType  {};
template <>           struct TIsVoid<const void>          : FTrueType  {};
template <>           struct TIsVoid<volatile void>       : FTrueType  {};
template <>           struct TIsVoid<const volatile void> : FTrueType  {};

/// @brief Helper variable template to check if T is void.
template <typename T> inline constexpr bool TIsVoid_V = TIsVoid<T>::Value;

/// @brief Checks whether T is std::nullptr_t.
template <typename T> struct TIsNullPointer                                    : FFalseType {};
template <>           struct TIsNullPointer<decltype(nullptr)>                 : FTrueType  {};
template <>           struct TIsNullPointer<const decltype(nullptr)>           : FTrueType  {};
template <>           struct TIsNullPointer<volatile decltype(nullptr)>        : FTrueType  {};
template <>           struct TIsNullPointer<const volatile decltype(nullptr)>  : FTrueType  {};

/// @brief Helper variable template to check if T is std::nullptr_t.
template <typename T> inline constexpr bool TIsNullPointer_V = TIsNullPointer<T>::Value;

namespace Detail
{

template <typename T> struct TIsIntegralHelper                     : FFalseType {};
template <>           struct TIsIntegralHelper<bool>               : FTrueType  {};
template <>           struct TIsIntegralHelper<char>               : FTrueType  {};
template <>           struct TIsIntegralHelper<signed char>        : FTrueType  {};
template <>           struct TIsIntegralHelper<unsigned char>      : FTrueType  {};
template <>           struct TIsIntegralHelper<wchar_t>            : FTrueType  {};
template <>           struct TIsIntegralHelper<char8_t>            : FTrueType  {};
template <>           struct TIsIntegralHelper<char16_t>           : FTrueType  {};
template <>           struct TIsIntegralHelper<char32_t>           : FTrueType  {};
template <>           struct TIsIntegralHelper<short>              : FTrueType  {};
template <>           struct TIsIntegralHelper<unsigned short>     : FTrueType  {};
template <>           struct TIsIntegralHelper<int>                : FTrueType  {};
template <>           struct TIsIntegralHelper<unsigned int>       : FTrueType  {};
template <>           struct TIsIntegralHelper<long>               : FTrueType  {};
template <>           struct TIsIntegralHelper<unsigned long>      : FTrueType  {};
template <>           struct TIsIntegralHelper<long long>          : FTrueType  {};
template <>           struct TIsIntegralHelper<unsigned long long> : FTrueType  {};

}   // namespace Detail

/// @brief Checks whether T is an integral type (including cv-qualified variants).
template <typename T>
struct TIsIntegral : Detail::TIsIntegralHelper<TRemoveCV_T<T>> {};

/// @brief Helper variable template to check if T is an integral type.
template <typename T> inline constexpr bool TIsIntegral_V = TIsIntegral<T>::Value;

namespace Detail
{

template <typename T> struct TIsFloatingHelper              : FFalseType {};
template <>           struct TIsFloatingHelper<float>       : FTrueType  {};
template <>           struct TIsFloatingHelper<double>      : FTrueType  {};
template <>           struct TIsFloatingHelper<long double> : FTrueType  {};

}   // namespace Detail

/// @brief Checks whether T is a floating-point type.
template <typename T>
struct TIsFloating : Detail::TIsFloatingHelper<TRemoveCV_T<T>> {};

/// @brief Helper variable template to check if T is a floating-point type.
template <typename T> inline constexpr bool TIsFloating_V = TIsFloating<T>::Value;

/// @brief Checks whether T is an array type.
template <typename T>          struct TIsArray       : FFalseType {};
template <typename T>          struct TIsArray<T[]>  : FTrueType  {};
template <typename T, SizeT N> struct TIsArray<T[N]> : FTrueType  {};

/// @brief Helper variable template to check if T is an array type.
template <typename T> inline constexpr bool TIsArray_V = TIsArray<T>::Value;

/// @brief Checks whether T is a bounded array type T[N].
template <typename T>          struct TIsBoundedArray       : FFalseType {};
template <typename T, SizeT N> struct TIsBoundedArray<T[N]> : FTrueType  {};

/// @brief Helper variable template to check if T is a bounded array type T[N].
template <typename T> inline constexpr bool TIsBoundedArray_V = TIsBoundedArray<T>::Value;

/// @brief Checks whether T is an unbounded array type T[].
template <typename T> struct TIsUnboundedArray      : FFalseType {};
template <typename T> struct TIsUnboundedArray<T[]> : FTrueType  {};

/// @brief Helper variable template to check if T is an unbounded array type T[].
template <typename T> inline constexpr bool TIsUnboundedArray_V = TIsUnboundedArray<T>::Value;

/// @brief Checks whether T is an enumeration type.
template <typename T> struct TIsEnum : TBoolConstant<__is_enum(T)> {};
template <typename T> inline constexpr bool TIsEnum_V = TIsEnum<T>::Value;

/// @brief Checks whether T is a union type.
template <typename T> struct TIsUnion : TBoolConstant<__is_union(T)> {};
template <typename T> inline constexpr bool TIsUnion_V = TIsUnion<T>::Value;

/// @brief Checks whether T is a non-union class type.
template <typename T> struct TIsClass : TBoolConstant<__is_class(T)> {};
template <typename T> inline constexpr bool TIsClass_V = TIsClass<T>::Value;

namespace Detail
{

template <typename T> struct TIsPointerHelper     : FFalseType {};
template <typename T> struct TIsPointerHelper<T*> : FTrueType  {};

}   // namespace Detail

/// @brief Checks whether T is a pointer type (excluding pointer-to-member).
template <typename T>
struct TIsPointer : Detail::TIsPointerHelper<TRemoveCV_T<T>> {};

/// @brief Helper variable template to check if T is a pointer type.
template <typename T> inline constexpr bool TIsPointer_V = TIsPointer<T>::Value;

/// @brief Checks whether T is an lvalue reference type.
template <typename T> struct TIsLValueRef     : FFalseType {};
template <typename T> struct TIsLValueRef<T&> : FTrueType  {};

/// @brief Helper variable template to check if T is an lvalue reference type.
template <typename T> inline constexpr bool TIsLValueRef_V = TIsLValueRef<T>::Value;

/// @brief Checks whether T is an rvalue reference type.
template <typename T> struct TIsRValueRef      : FFalseType {};
template <typename T> struct TIsRValueRef<T&&> : FTrueType  {};

/// @brief Helper variable template to check if T is an rvalue reference type.
template <typename T> inline constexpr bool TIsRValueRef_V = TIsRValueRef<T>::Value;

/// @brief Checks whether T is any reference type (lvalue or rvalue).
template <typename T>
struct TIsReference : TBoolConstant<TIsLValueRef_V<T> || TIsRValueRef_V<T>> {};

/// @brief Helper variable template to check if T is any reference type.
template <typename T> inline constexpr bool TIsReference_V = TIsReference<T>::Value;

namespace Detail
{

template <typename T>             struct TIsMemberPointerHelper         : FFalseType {};
template <typename T, typename C> struct TIsMemberPointerHelper<T C::*> : FTrueType  {};

}   // namespace Detail

/// @brief Checks whether T is a pointer-to-member type.
template <typename T>
struct TIsMemberPointer : Detail::TIsMemberPointerHelper<TRemoveCV_T<T>> {};

/// @brief Helper variable template to check if T is a pointer-to-member type.
template <typename T> inline constexpr bool TIsMemberPointer_V = TIsMemberPointer<T>::Value;

namespace Detail
{

template <typename T>             struct TIsMemberFuncPtrHelper         : FFalseType                         {};
template <typename T, typename C> struct TIsMemberFuncPtrHelper<T C::*> : TBoolConstant<!TIsConst_V<const T>> {};

}

/// @brief Checks whether T is a pointer-to-member-function type.
template <typename T>
struct TIsMemberFunctionPointer : Detail::TIsMemberFuncPtrHelper<TRemoveCV_T<T>> {};

/// @brief Helper variable template to check if T is a pointer-to-member-function type.
template <typename T> inline constexpr bool TIsMemberFunctionPointer_V = TIsMemberFunctionPointer<T>::Value;

/// @brief Checks whether T is a pointer-to-member-object type.
template <typename T>
struct TIsMemberObjectPointer : TBoolConstant<TIsMemberPointer_V<T> && !TIsMemberFunctionPointer_V<T>> {};

/// @brief Helper variable template to check if T is a pointer-to-member-object type.
template <typename T> inline constexpr bool TIsMemberObjectPointer_V = TIsMemberObjectPointer<T>::Value;

/// @brief Checks whether T is a function type (not a function pointer).
///        A function type cannot be const-qualified or a reference.
template <typename T>
struct TIsFunction : TBoolConstant<!TIsReference_V<T> && !TIsConst_V<const T>> {};

/// @brief Helper variable template to check if T is a function type.
template <typename T> inline constexpr bool TIsFunction_V = TIsFunction<T>::Value;

/// @brief Checks whether T is a signed integral type.
template <typename T>
struct TIsSigned : TBoolConstant<TIsIntegral_V<T> && (T(-1) < T(0))> {};

template <> struct TIsSigned<float>       : FTrueType {};
template <> struct TIsSigned<double>      : FTrueType {};
template <> struct TIsSigned<long double> : FTrueType {};

/// @brief Helper variable template to check if T is a signed integral type.
template <typename T> inline constexpr bool TIsSigned_V = TIsSigned<T>::Value;

/// @brief Checks whether T is an unsigned integral type.
template <typename T>
struct TIsUnsigned : TBoolConstant<TIsIntegral_V<T> && !TIsSigned_V<T>> {};

/// @brief Helper variable template to check if T is an unsigned integral type.
template <typename T> inline constexpr bool TIsUnsigned_V = TIsUnsigned<T>::Value;

// clang-format on

}   // namespace GP
