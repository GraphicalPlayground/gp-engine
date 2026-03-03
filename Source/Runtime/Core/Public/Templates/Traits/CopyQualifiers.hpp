// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/AddCV.hpp"
#include "Templates/Core/Conditional.hpp"
#include "Templates/Core/RemoveCV.hpp"

namespace GP
{

// clang-format off

/// @brief Copies the cv-qualifiers of TFrom onto TTo.
///
///   TCopyConst_T<const int,   float>  →  const float
///   TCopyConst_T<      int,   float>  →        float
///
/// @tparam TFrom The type whose const-qualifier is mirrored.
/// @tparam TTo The type receiving the qualifier.
template <typename TFrom, typename TTo>
struct TCopyConst
{
    using Type = TConditional_T<TIsConst_V<TFrom>, TAddConst_T<TTo>, TTo>;
};
template <typename TFrom, typename TTo>
using TCopyConst_T = typename TCopyConst<TFrom, TTo>::Type;

/// @brief Copies the volatile-qualifier of TFrom onto TTo.
template <typename TFrom, typename TTo>
struct TCopyVolatile
{
    using Type = TConditional_T<TIsVolatile_V<TFrom>, TAddVolatile_T<TTo>, TTo>;
};
template <typename TFrom, typename TTo>
using TCopyVolatile_T = typename TCopyVolatile<TFrom, TTo>::Type;

/// @brief Copies all cv-qualifiers of TFrom onto TTo.
///
///   TCopyCV_T<const volatile int, float>  →  const volatile float
///   TCopyCV_T<const          int, float>  →  const          float
///   TCopyCV_T<               int, float>  →                 float
///
/// @tparam TFrom   Source of the cv-qualifiers.
/// @tparam TTo     Destination type.
template <typename TFrom, typename TTo>
struct TCopyCV
{
    using Type = TCopyConst_T<TFrom, TCopyVolatile_T<TFrom, TTo>>;
};
template <typename TFrom, typename TTo>
using TCopyCV_T = typename TCopyCV<TFrom, TTo>::Type;

/// @brief Copies the reference category (none / & / &&) of TFrom onto TTo.
///
///   TCopyReference_T<int&,  float>  →  float&
///   TCopyReference_T<int&&, float>  →  float&&
///   TCopyReference_T<int,   float>  →  float
///
/// @tparam TFrom Source of the reference category.
/// @tparam TTo Destination type.
template <typename TFrom, typename TTo>
struct TCopyReference
{
    using Type = TConditional_T<TIsLValueRef_V<TFrom>,
                    TAddLValueRef_T<TTo>,
                    TConditional_T<TIsRValueRef_V<TFrom>,
                        TAddRValueRef_T<TTo>,
                        TTo>>;
};
template <typename TFrom, typename TTo>
using TCopyReference_T = typename TCopyReference<TFrom, TTo>::Type;

/// @brief Copies all cv-qualifiers AND the reference category of TFrom onto TTo.
///
///   TCopyCVRef_T<const int&, float>  →  const float&
///
/// @tparam TFrom Source of the cv + reference qualifiers.
/// @tparam TTo Destination type.
template <typename TFrom, typename TTo>
struct TCopyCVRef
{
    using TNoRef = TRemoveReference_T<TFrom>;
    using Type   = TCopyReference_T<TFrom, TCopyCV_T<TNoRef, TTo>>;
};
template <typename TFrom, typename TTo>
using TCopyCVRef_T = typename TCopyCVRef<TFrom, TTo>::Type;

// clang-format on

}   // namespace GP
