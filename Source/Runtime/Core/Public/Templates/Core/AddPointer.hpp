// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Core/EnableIf.hpp"
#include "Templates/Core/RemoveReference.hpp"

namespace GP
{

namespace Detail
{

// clang-format off

/// @brief Helper function to add a pointer to a type, correctly handling reference types.
template <typename T> auto TryAddPointer(int) -> TVoid<TRemoveReference_T<T>*>;
template <typename T> auto TryAddPointer(...) -> TVoid<T>;

// clang-format on

}   // namespace Detail

// clang-format off

/// @brief Type trait to add a pointer to a type.
///        Correctly handles reference types by first removing the reference.
template <typename T> struct TAddPointer { using Type = typename decltype(Detail::TryAddPointer<T>(0))::Type; };

/// @brief Helper alias to add a pointer to a type.
template <typename T>
using TAddPointer_T = typename TAddPointer<T>::Type;

// clang-format on

}   // namespace GP
