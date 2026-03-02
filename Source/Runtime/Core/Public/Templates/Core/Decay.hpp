// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/AddPointer.hpp"
#include "Templates/Core/Conditional.hpp"
#include "Templates/Core/RemoveCV.hpp"
#include "Templates/Core/RemoveExtent.hpp"
#include "Templates/Core/RemoveReference.hpp"
#include "Templates/Traits/TypeCategories.hpp"

namespace GP
{

// clang-format off

/// @brief Applies the same type decay rules as passing an argument by value.
///
/// Array types T[N] decay to T*.
/// Function types decay to function pointers.
/// All other types have cv-qualifiers and references stripped.
///
/// @tparam T The type to decay.
template <typename T>
struct TDecay
{
private:
    using TNoRef = TRemoveReference_T<T>;   //<! The type with references removed.

public:
    using Type = TConditional_T<
        TIsArray_V<TNoRef>,
        TAddPointer_T<TRemoveExtent_T<TNoRef>>,
        TConditional_T<
            TIsFunction_V<TNoRef>,
            TAddPointer_T<TNoRef>,
            TRemoveCV_T<TNoRef>
        >
    >;
};

/// @brief Helper alias for type decay.
template <typename T>
using TDecay_T = typename TDecay<T>::Type;

// clang-format on

}   // namespace GP
