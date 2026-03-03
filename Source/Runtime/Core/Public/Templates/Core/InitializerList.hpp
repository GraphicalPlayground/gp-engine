// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "CoreTypes.hpp"
#include "Templates/Core/TemplateTypes.hpp"
#include <initializer_list>

namespace GP
{

/// @brief A lightweight, read-only view over a compiler-generated const array.
/// The compiler automatically constructs a TInitializerList when a braced-init-list { ... } is used where a
/// TInitializerList parameter is expected.
/// ABI note: The backing array's lifetime is the enclosing full-expression. This type must never be stored as a member.
/// Usage in a container constructor:
/// @code
///     TArray(TInitializerList<T> InitList)
///     {
///         Reserve(InitList.size());
///         for (const T& Elem : InitList)
///             EmplaceBack(Elem);
///     }
/// @endcode
/// @tparam T The element type. Always exposed as const T.
template <typename T>
using TInitializerList = std::initializer_list<T>;

}   // namespace GP
