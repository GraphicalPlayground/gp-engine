// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

namespace GP
{

// clang-format off

/// @brief Type trait to add a `const` qualifier to a type.
template <typename T> struct TAddConst { using Type = const T; };

/// @brief Helper alias to add a `const` qualifier to a type.
template <typename T>
using TAddConst_T = typename TAddConst<T>::Type;

/// @brief Type trait to add a `volatile` qualifier to a type.
template <typename T> struct TAddVolatile { using Type = volatile T; };

/// @brief Helper alias to add a `volatile` qualifier to a type.
template <typename T>
using TAddVolatile_T = typename TAddVolatile<T>::Type;

/// @brief Type trait to add both `const` and `volatile` qualifiers to a type.
template <typename T> struct TAddCV { using Type = const volatile T; };

/// @brief Helper alias to add both `const` and `volatile` qualifiers to a type.
template <typename T>
using TAddCV_T = typename TAddCV<T>::Type;

// clang-format on

}   // namespace GP
