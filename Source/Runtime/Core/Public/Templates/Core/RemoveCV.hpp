// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

namespace GP
{

// clang-format off

/// @brief Type trait to remove `const` qualifiers from a type.
template <typename T> struct TRemoveConst          { using Type = T; };
template <typename T> struct TRemoveConst<const T> { using Type = T; };

/// @brief Helper alias to remove the `const` qualifier from a type.
template <typename T>
using TRemoveConst_T = typename TRemoveConst<T>::Type;

/// @brief Type trait to remove `volatile` qualifiers from a type.
template <typename T> struct TRemoveVolatile             { using Type = T; };
template <typename T> struct TRemoveVolatile<volatile T> { using Type = T; };

/// @brief Helper alias to remove the `volatile` qualifier from a type.
template <typename T>
using TRemoveVolatile_T = typename TRemoveVolatile<T>::Type;

/// @brief Type trait to remove both `const` and `volatile` qualifiers from a type.
template <typename T>
struct TRemoveCV
{
    using Type = typename TRemoveConst<typename TRemoveVolatile<T>::Type>::Type;
};

/// @brief Helper alias template to remove both `const` and `volatile` qualifiers from a type.
template <typename T>
using TRemoveCV_T = typename TRemoveCV<T>::Type;

// clang-format on

}   // namespace GP
