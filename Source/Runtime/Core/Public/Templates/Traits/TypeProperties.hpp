// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "Templates/Core/TemplateTypes.hpp"

namespace GP
{

// clang-format off

/// @brief Checks whether T is const-qualified.
template <typename T> struct TIsConst              : FFalseType {};
template <typename T> struct TIsConst<const T>     : FTrueType  {};
template <typename T> inline constexpr bool TIsConst_V = TIsConst<T>::Value;

/// @brief Checks whether T is volatile-qualified.
template <typename T> struct TIsVolatile             : FFalseType {};
template <typename T> struct TIsVolatile<volatile T> : FTrueType  {};
template <typename T> inline constexpr bool TIsVolatile_V = TIsVolatile<T>::Value;

/// @brief Checks whether T is an abstract class.
template <typename T>
struct TIsAbstract : TBoolConstant<__is_abstract(T)> {};
template <typename T> inline constexpr bool TIsAbstract_V = TIsAbstract<T>::Value;

/// @brief Checks whether T is declared final.
template <typename T>
struct TIsFinal : TBoolConstant<__is_final(T)> {};
template <typename T> inline constexpr bool TIsFinal_V = TIsFinal<T>::Value;

/// @brief Checks whether T is a polymorphic class (has a virtual function).
template <typename T>
struct TIsPolymorphic : TBoolConstant<__is_polymorphic(T)> {};
template <typename T> inline constexpr bool TIsPolymorphic_V = TIsPolymorphic<T>::Value;

/// @brief Checks whether T is an empty class (no data members and no virtual table).
template <typename T>
struct TIsEmpty : TBoolConstant<__is_empty(T)> {};
template <typename T> inline constexpr bool TIsEmpty_V = TIsEmpty<T>::Value;

/// @brief Checks whether T is an aggregate type.
template <typename T>
struct TIsAggregate : TBoolConstant<__is_aggregate(T)> {};
template <typename T> inline constexpr bool TIsAggregate_V = TIsAggregate<T>::Value;

/// @brief Checks whether T is a standard-layout type.
template <typename T>
struct TIsStandardLayout : TBoolConstant<__is_standard_layout(T)> {};
template <typename T> inline constexpr bool TIsStandardLayout_V = TIsStandardLayout<T>::Value;

/// @brief Checks whether T is a trivially-copyable type.
template <typename T>
struct TIsTriviallyCopyable : TBoolConstant<__is_trivially_copyable(T)> {};
template <typename T> inline constexpr bool TIsTriviallyCopyable_V = TIsTriviallyCopyable<T>::Value;

/// @brief Checks whether T is a trivial type (trivially copyable + trivial default ctor).
template <typename T>
struct TIsTrivial : TBoolConstant<__is_trivial(T)> {};
template <typename T> inline constexpr bool TIsTrivial_V = TIsTrivial<T>::Value;

/// @brief Checks whether T has a trivial destructor.
template <typename T>
struct TIsTriviallyDestructible : TBoolConstant<__is_trivially_destructible(T)> {};
template <typename T> inline constexpr bool TIsTriviallyDestructible_V = TIsTriviallyDestructible<T>::Value;

/// @brief Checks whether T is trivially default constructible.
template <typename T>
struct TIsTriviallyDefaultConstructible : TBoolConstant<__is_trivially_constructible(T)> {};
template <typename T> inline constexpr bool TIsTriviallyDefaultConstructible_V = TIsTriviallyDefaultConstructible<T>::Value;

/// @brief Checks whether T is trivially copy-constructible.
template <typename T>
struct TIsTriviallyCopyConstructible : TBoolConstant<__is_trivially_constructible(T, const T&)> {};
template <typename T> inline constexpr bool TIsTriviallyCopyConstructible_V = TIsTriviallyCopyConstructible<T>::Value;

/// @brief Checks whether T is trivially move-constructible.
template <typename T>
struct TIsTriviallyMoveConstructible : TBoolConstant<__is_trivially_constructible(T, T&&)> {};
template <typename T> inline constexpr bool TIsTriviallyMoveConstructible_V = TIsTriviallyMoveConstructible<T>::Value;

/// @brief Checks whether T is trivially copy-assignable.
template <typename T>
struct TIsTriviallyCopyAssignable : TBoolConstant<__is_trivially_assignable(T&, const T&)> {};
template <typename T> inline constexpr bool TIsTriviallyCopyAssignable_V = TIsTriviallyCopyAssignable<T>::Value;

/// @brief Checks whether T is trivially move-assignable.
template <typename T>
struct TIsTriviallyMoveAssignable : TBoolConstant<__is_trivially_assignable(T&, T&&)> {};
template <typename T> inline constexpr bool TIsTriviallyMoveAssignable_V = TIsTriviallyMoveAssignable<T>::Value;

/// @brief Returns the alignment (in bytes) of type T as a compile-time constant.
template <typename T>
struct TAlignmentOf : TIntegralConstant<SizeT, alignof(T)> {};
template <typename T> inline constexpr SizeT TAlignmentOf_V = TAlignmentOf<T>::Value;

/// @brief Returns the size (in bytes) of type T as a compile-time constant.
template <typename T>
struct TSizeOf : TIntegralConstant<SizeT, sizeof(T)> {};
template <typename T> inline constexpr SizeT TSizeOf_V = TSizeOf<T>::Value;

/// @brief Returns the number of dimensions of an array type.
template <typename T>          struct TRank       : TIntegralConstant<SizeT, 0>                   {};
template <typename T>          struct TRank<T[]>  : TIntegralConstant<SizeT, TRank<T>::Value + 1> {};
template <typename T, SizeT N> struct TRank<T[N]> : TIntegralConstant<SizeT, TRank<T>::Value + 1> {};
template <typename T> inline constexpr SizeT TRank_V = TRank<T>::Value;

/// @brief Returns the extent (size) of the Nth dimension of an array type.
template <typename T, unsigned N = 0>      struct TExtent          : TIntegralConstant<SizeT, 0> {};
template <typename T>                      struct TExtent<T[], 0>  : TIntegralConstant<SizeT, 0> {};
template <typename T, unsigned N>          struct TExtent<T[], N>  : TExtent<T, N - 1>           {};
template <typename T, SizeT I>             struct TExtent<T[I], 0> : TIntegralConstant<SizeT, I> {};
template <typename T, SizeT I, unsigned N> struct TExtent<T[I], N> : TExtent<T, N - 1>           {};
template <typename T, unsigned N = 0> inline constexpr SizeT TExtent_V = TExtent<T, N>::Value;

// clang-format on

}   // namespace GP
