// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

namespace GP
{

// clang-format off

/// @brief Type trait to remove one level of pointer indirection from a type.
template <typename T> struct TRemovePointer                    { using Type = T; };
template <typename T> struct TRemovePointer<T*>                { using Type = T; };
template <typename T> struct TRemovePointer<T* const>          { using Type = T; };
template <typename T> struct TRemovePointer<T* volatile>       { using Type = T; };
template <typename T> struct TRemovePointer<T* const volatile> { using Type = T; };

/// @brief Helper alias to remove one level of pointer indirection from a type.
template <typename T>
using TRemovePointer_T = typename TRemovePointer<T>::Type;

// clang-format on

}   // namespace GP
