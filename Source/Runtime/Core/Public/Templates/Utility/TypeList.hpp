// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/Conditional.hpp"
#include "Templates/Core/IntegerSequence.hpp"
#include "Templates/Core/TemplateTypes.hpp"
#include "Templates/Traits/TypeRelationships.hpp"

namespace GP
{

// clang-format off

/// @brief An ordered compile-time list of types.
/// @tparam ...Ts   The types in the list.
template <typename... Ts>
struct TTypeList
{
    /// @brief Number of types in the list.
    static constexpr SizeT Size = sizeof...(Ts);
};

namespace Detail
{

/// @brief Helper struct for retrieving the type at a specific index in a TTypeList.
template <SizeT I, typename TList> struct TTypeListGetImpl;
template <SizeT I, typename T, typename... Ts>
struct TTypeListGetImpl<I, TTypeList<T, Ts...>>
{
    using Type = typename TTypeListGetImpl<I - 1, TTypeList<Ts...>>::Type;
};

/// @brief Base case for TTypeListGetImpl when I is 0, returning the first type in the list.
template <typename T, typename... Ts>
struct TTypeListGetImpl<0, TTypeList<T, Ts...>>
{
    using Type = T;
};

}   // namespace Detail

/// @brief Retrieves the type at index I in a TTypeList.
template <SizeT I, typename TList>
struct TTypeListGet
{
    static_assert(I < TList::Size, "[GP] TTypeListGet: Index out of bounds.");
    using Type = typename Detail::TTypeListGetImpl<I, TList>::Type;
};

/// @brief Alias for TTypeListGet to simplify syntax when retrieving a type from a TTypeList.
template <SizeT I, typename TList>
using TTypeListGet_T = typename TTypeListGet<I, TList>::Type;

/// @brief Checks whether type T appears in TList.
template <typename T, typename TList>     struct TTypeListContains;
template <typename T>
struct TTypeListContains<T, TTypeList<>> : FFalseType {};
template <typename T, typename U, typename... Ts>
struct TTypeListContains<T, TTypeList<U, Ts...>>
    : TConditional_T<TIsSame_V<T, U>, FTrueType, TTypeListContains<T, TTypeList<Ts...>>> {};

/// @brief Alias for TTypeListContains to simplify syntax when checking for type containment in a TTypeList.
template <typename T, typename TList>
inline constexpr bool TTypeListContains_V = TTypeListContains<T, TList>::Value;

namespace Detail
{

/// @brief Helper struct for retrieving the index of a type in a TTypeList.
template <typename T, SizeT I, typename TList> struct TTypeListIndexOfImpl;
template <typename T, SizeT I>
struct TTypeListIndexOfImpl<T, I, TTypeList<>> : TIntegralConstant<SizeT, ~SizeT(0)> {};
template <typename T, SizeT I, typename U, typename... Ts>
struct TTypeListIndexOfImpl<T, I, TTypeList<U, Ts...>>
    : TConditional_T<TIsSame_V<T, U>,
          TIntegralConstant<SizeT, I>,
          TTypeListIndexOfImpl<T, I + 1, TTypeList<Ts...>>> {};

}   // namespace Detail

/// @brief Returns the first index of T in TList, or ~0ull if not found.
template <typename T, typename TList>
struct TTypeListIndexOf : Detail::TTypeListIndexOfImpl<T, 0, TList> {};

/// @brief Alias for TTypeListIndexOf to simplify syntax when retrieving the index of a type in a TTypeList.
template <typename T, typename TList>
inline constexpr SizeT TTypeListIndexOf_V = TTypeListIndexOf<T, TList>::Value;

/// @brief Prepends type T to a TTypeList.
template <typename T, typename TList> struct TTypeListPrepend;
template <typename T, typename... Ts>
struct TTypeListPrepend<T, TTypeList<Ts...>>
{
    using Type = TTypeList<T, Ts...>;
};
template <typename T, typename TList>
using TTypeListPrepend_T = typename TTypeListPrepend<T, TList>::Type;

/// @brief Appends type T to a TTypeList.
template <typename T, typename TList> struct TTypeListAppend;
template <typename T, typename... Ts>
struct TTypeListAppend<T, TTypeList<Ts...>>
{
    using Type = TTypeList<Ts..., T>;
};
template <typename T, typename TList>
using TTypeListAppend_T = typename TTypeListAppend<T, TList>::Type;

/// @brief Concatenates two TTypeList types.
template <typename TListA, typename TListB> struct TTypeListConcat;
template <typename... Ts, typename... Us>
struct TTypeListConcat<TTypeList<Ts...>, TTypeList<Us...>>
{
    using Type = TTypeList<Ts..., Us...>;
};
template <typename TListA, typename TListB>
using TTypeListConcat_T = typename TTypeListConcat<TListA, TListB>::Type;

/// @brief Applies a template trait T to every type in TList, producing a new TTypeList.
/// @tparam TTrait A template that accepts one type and provides a ::Type member.
template <template <typename> class TTrait, typename TList> struct TTypeListTransform;
template <template <typename> class TTrait, typename... Ts>
struct TTypeListTransform<TTrait, TTypeList<Ts...>>
{
    using Type = TTypeList<typename TTrait<Ts>::Type...>;
};
template <template <typename> class TTrait, typename TList>
using TTypeListTransform_T = typename TTypeListTransform<TTrait, TList>::Type;

namespace Detail
{
/// @brief Helper struct for filtering types in a TTypeList based on a predicate template.
template <template <typename> class TPred, typename TIn, typename TOut>
struct TTypeListFilterImpl { using Type = TOut; };

/// @brief Recursive case for TTypeListFilterImpl when the predicate is satisfied, adding T to the output list.
template <template <typename> class TPred, typename T, typename... Ts, typename... Out>
struct TTypeListFilterImpl<TPred, TTypeList<T, Ts...>, TTypeList<Out...>>
{
    using Type = typename TConditional_T<
        TPred<T>::Value,
        TTypeListFilterImpl<TPred, TTypeList<Ts...>, TTypeList<Out..., T>>,
        TTypeListFilterImpl<TPred, TTypeList<Ts...>, TTypeList<Out...>>
    >::Type;
};

}   // namespace Detail

/// @brief Filters a TTypeList, keeping only types for which TPred<T>::Value is true.
template <template <typename> class TPred, typename TList>
struct TTypeListFilter
{
    using Type = typename Detail::TTypeListFilterImpl<TPred, TList, TTypeList<>>::Type;
};
template <template <typename> class TPred, typename TList>
using TTypeListFilter_T = typename TTypeListFilter<TPred, TList>::Type;

// clang-format on

}   // namespace GP
