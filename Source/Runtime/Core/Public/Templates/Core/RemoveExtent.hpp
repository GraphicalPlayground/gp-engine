// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"

namespace GP
{

// clang-format off

/// @brief Type trait to remove the outermost array extent from a type.
template <typename T>          struct TRemoveExtent       { using Type = T; };
template <typename T>          struct TRemoveExtent<T[]>  { using Type = T; };
template <typename T, SizeT N> struct TRemoveExtent<T[N]> { using Type = T; };

/// @brief Helper alias to remove the outermost array extent from a type.
template <typename T>
using TRemoveExtent_T = typename TRemoveExtent<T>::Type;

/// @brief Type trait to remove all array extents from a type recursively.
template <typename T>          struct TRemoveAllExtents       { using Type = T; };
template <typename T>          struct TRemoveAllExtents<T[]>  { using Type = typename TRemoveAllExtents<T>::Type; };
template <typename T, SizeT N> struct TRemoveAllExtents<T[N]> { using Type = typename TRemoveAllExtents<T>::Type; };

/// @brief Helper alias to remove all array extents from a type recursively.
template <typename T>
using TRemoveAllExtents_T = typename TRemoveAllExtents<T>::Type;

// clang-format on

}   // namespace GP
