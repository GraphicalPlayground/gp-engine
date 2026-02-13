// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include <type_traits>

namespace GP::Template
{

/// \brief Alias to remove the const qualifier from a type.
template <typename T>
using RemoveConst = std::remove_const_t<T>;

/// \brief Alias to remove the volatile qualifier from a type.
template <typename T>
using RemoveVolatile = std::remove_volatile_t<T>;

/// \brief Alias to remove both const and volatile qualifiers from a type.
template <typename T>
using RemoveCV = std::remove_cv_t<T>;

/// \brief Alias to add const qualifier to a type.
template <typename T>
using AddConst = std::add_const_t<T>;

/// \brief Alias to add volatile qualifier to a type.
template <typename T>
using AddVolatile = std::add_volatile_t<T>;

/// \brief Alias to add both const and volatile qualifiers to a type.
template <typename T>
using AddCV = std::add_cv_t<T>;

/// \brief Alias to remove reference qualifiers from a type.
template <typename T>
using RemoveReference = std::remove_reference_t<T>;

/// \brief Alias to add lvalue reference qualifier to a type.
template <typename T>
using AddLValueReference = std::add_lvalue_reference_t<T>;

/// \brief Alias to add rvalue reference qualifier to a type.
template <typename T>
using AddRValueReference = std::add_rvalue_reference_t<T>;

/// \brief Alias to remove pointer qualifiers from a type.
template <typename T>
using RemovePointer = std::remove_pointer_t<T>;

/// \brief Alias to add pointer qualifier to a type.
template <typename T>
using AddPointer = std::add_pointer_t<T>;

/// \brief Alias to remove extent from an array type, yielding the element type.
template <typename T>
using RemoveExtent = std::remove_extent_t<T>;

/// \brief Alias to remove all extents from an array type, yielding the base element type.
template <typename T>
using RemoveAllExtents = std::remove_all_extents_t<T>;

/// \brief Alias to remove the signedness from an integral type, yielding the corresponding unsigned type.
template <typename T>
using MakeUnsigned = std::make_unsigned_t<T>;

/// \brief Alias to add the signedness to an integral type, yielding the corresponding signed type.
template <typename T>
using MakeSigned = std::make_signed_t<T>;

/// \brief Alias to remove the pointer and reference qualifiers from a type, yielding the underlying type.
template <typename T>
using Decay = std::decay_t<T>;

/// \brief Alias to add const, volatile, reference, and pointer qualifiers to a type as needed to make it a valid
/// function parameter type.
template <typename T>
using DecayParameter = std::decay_t<T>;

/// \brief Alias to add const, volatile, reference, and pointer qualifiers to a type as needed to make it a valid
/// function return type.
template <typename T>
using DecayReturn = std::decay_t<T>;

/// \brief Alias to remove the top-level const qualifier from a type if it exists, otherwise yields the original type.
template <typename T>
using RemoveTopLevelConst = std::conditional_t<std::is_const_v<T>, std::remove_const_t<T>, T>;

}   // namespace GP::Template
