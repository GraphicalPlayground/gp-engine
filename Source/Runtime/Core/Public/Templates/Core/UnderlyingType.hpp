// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/TemplateTypes.hpp"

namespace GP
{

/// @brief Obtains the underlying integer type of an enumeration.
///        Uses the __underlying_type compiler intrinsic (GCC/Clang/MSVC).
/// @tparam T An enumeration type.
template <typename T>
struct TUnderlyingType
{
    static_assert(__is_enum(T), "[GP] TUnderlyingType requires an enum type.");
    using Type = __underlying_type(T);
};

/// @brief Helper alias to obtain the underlying integer type of an enumeration.
template <typename T>
using TUnderlyingType_T = typename TUnderlyingType<T>::Type;

}   // namespace GP
