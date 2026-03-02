// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

namespace GP
{

// clang-format off

/// @brief Type trait to remove reference qualifiers from a type.
template<typename T> struct TRemoveReference      { using Type = T; };
template<typename T> struct TRemoveReference<T&>  { using Type = T; };
template<typename T> struct TRemoveReference<T&&> { using Type = T; };

/// @brief Helper alias template to remove reference qualifiers from a type.
template<typename T>
using TRemoveReference_T = typename TRemoveReference<T>::Type;

// clang-format on

}   // namespace GP
