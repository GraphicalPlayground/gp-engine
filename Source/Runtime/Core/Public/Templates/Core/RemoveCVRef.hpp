// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/RemoveCV.hpp"
#include "Templates/Core/RemoveReference.hpp"

namespace GP
{

/// @brief Type trait to remove both cv-qualifiers and reference from a type. Equivalent to std::remove_cvref.
template <typename T>
struct TRemoveCVRef
{
    using Type = TRemoveCV_T<TRemoveReference_T<T>>;
};

/// @brief Helper alias to remove cv-qualifiers and reference from a type.
template <typename T>
using TRemoveCVRef_T = typename TRemoveCVRef<T>::Type;

}   // namespace GP
