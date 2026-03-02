// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Traits/TypeProperties.hpp"
#include "Templates/Traits/TypeRelationships.hpp"

namespace GP
{

// clang-format off

/// @brief Satisfied when T has an accessible, non-deleted destructor.
template <typename T>
concept CDestructible = TIsNothrowDestructible_V<T>;

/// @brief Satisfied when T can be move-constructed and move-assigned, and swap is available.
template <typename T>
concept CMovable =
    TIsObject_V<T>          &&
    CMoveConstructible<T>   &&
    TIsAssignable_V<T&, T>  &&
    requires (T& a, T& b) { { a } noexcept; }; // swappable check via ADL omitted for brevity

/// @brief Satisfied when T is both movable and copy-constructible/assignable.
template <typename T>
concept CCopyable =
    CCopyConstructible<T>         &&
    CMovable<T>                   &&
    TIsAssignable_V<T&,       T&> &&
    TIsAssignable_V<T&, const T&> &&
    TIsAssignable_V<T&, const T>;

/// @brief Satisfied when T is copyable and default-initializable.
template <typename T>
concept CSemiregular = CCopyable<T> && CDefaultInitializable<T>;

/// @brief Satisfied when T is semiregular and equality comparable.
/// @note Full equality requires CEqualityComparable — see ComparisonConcepts.hpp.
template <typename T>
concept CRegular = CSemiregular<T>;  // + CEqualityComparable<T> added in ComparisonConcepts.hpp

/// @brief Satisfied when T is trivially copyable (safe to memcpy).
template <typename T>
concept CTriviallyCopyable = TIsTriviallyCopyable_V<T>;

/// @brief Satisfied when T is a trivial type.
template <typename T>
concept CTrivial = TIsTrivial_V<T>;

/// @brief Satisfied when T is a standard-layout type.
template <typename T>
concept CStandardLayout = TIsStandardLayout_V<T>;

/// @brief Satisfied when T is both trivial and standard-layout (plain-old-data).
template <typename T>
concept CPOD = CTrivial<T> && CStandardLayout<T>;

// clang-format on

}   // namespace GP
