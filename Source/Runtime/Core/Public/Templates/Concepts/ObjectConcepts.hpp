// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Core/TemplateTypes.hpp"
#include "Templates/Traits/CompositeTraits.hpp"
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

/// @brief Satisfied when T has a trivial default constructor.
///        T() leaves the object with indeterminate value (no user-provided ctor).
template <typename T>
concept CTriviallyDefaultConstructible = TIsTriviallyDefaultConstructible_V<T>;

/// @brief Satisfied when T has a trivial copy constructor.
///        Copying T is equivalent to a memcpy of its bytes.
template <typename T>
concept CTriviallyCopyConstructible = TIsTriviallyCopyConstructible_V<T>;

/// @brief Satisfied when T has a trivial move constructor.
///        Moving T is equivalent to a memcpy of its bytes.
template <typename T>
concept CTriviallyMoveConstructible = TIsTriviallyMoveConstructible_V<T>;

/// @brief Satisfied when T is constructible from Args using only trivial operations.
/// @tparam T       The type to construct.
/// @tparam ...TArgs The argument types.
template <typename T, typename... TArgs>
concept CTriviallyConstructibleFrom = TBoolConstant<__is_trivially_constructible(T, TArgs...)>::Value;

/// @brief Satisfied when T has a trivial copy-assignment operator.
///        Assigning from const T& is equivalent to a memcpy of its bytes.
template <typename T>
concept CTriviallyCopyAssignable = TIsTriviallyCopyAssignable_V<T>;

/// @brief Satisfied when T has a trivial move-assignment operator.
///        Assigning from T&& is equivalent to a memcpy of its bytes.
template <typename T>
concept CTriviallyMoveAssignable = TIsTriviallyMoveAssignable_V<T>;

/// @brief Satisfied when T is assignable from U using only trivial operations.
/// @tparam T   The type being assigned to.
/// @tparam U   The type being assigned from.
template <typename T, typename U>
concept CTriviallyAssignableFrom = TBoolConstant<__is_trivially_assignable(T, U)>::Value;

/// @brief Satisfied when T has a trivial destructor.
///        No user-provided, virtual, or non-trivial member destructors.
template <typename T>
concept CTriviallyDestructible = TIsTriviallyDestructible_V<T>;

/// @brief Satisfied when T is both trivially copyable and trivially
///        default-constructible — the minimal requirement for a type
///        to be safely used in raw byte buffers (e.g. ring buffers,
///        fixed-size pools, SIMD arrays).
template <typename T>
concept CTriviallyRelocatable =
    CTriviallyCopyable<T>             &&
    CTriviallyDefaultConstructible<T>;

/// @brief Satisfied when T is an aggregate type:
///        no user-provided constructors, no private/protected non-static
///        data members, no virtual functions, no virtual base classes.
///        Aggregate types support brace initialisation T{ a, b, c }.
template <typename T>
concept CAggregate = TIsAggregate_V<T>;

/// @brief Satisfied when T supports trivial copy AND trivial move construction.
template <typename T>
concept CTriviallyMoveCopyConstructible =
    CTriviallyCopyConstructible<T> &&
    CTriviallyMoveConstructible<T>;

/// @brief Satisfied when T supports trivial copy AND trivial move assignment.
template <typename T>
concept CTriviallyMoveCopyAssignable =
    CTriviallyCopyAssignable<T> &&
    CTriviallyMoveAssignable<T>;

/// @brief Satisfied when T is fully trivially copyable AND trivially movable:
///        construction, assignment, and destruction are all trivial in both
///        copy and move flavours.  The strongest guarantee that a type can
///        be treated as a bag of bytes.
template <typename T>
concept CTriviallyMoveCopyable =
    CTriviallyMoveCopyConstructible<T> &&
    CTriviallyMoveCopyAssignable<T>    &&
    CTriviallyDestructible<T>;

/// @brief Satisfied when T's default constructor is declared noexcept.
template <typename T>
concept CNothrowDefaultConstructible = TIsNothrowConstructible_V<T>;

/// @brief Satisfied when T's copy constructor is declared noexcept.
template <typename T>
concept CNothrowCopyConstructible = TIsNothrowConstructible_V<T, const T&>;

/// @brief Satisfied when T's move constructor is declared noexcept.
template <typename T>
concept CNothrowMoveConstructible = TIsNothrowConstructible_V<T, T&&>;

/// @brief Satisfied when T is constructible from TArgs with a noexcept guarantee.
template <typename T, typename... TArgs>
concept CNothrowConstructibleFrom = TIsNothrowConstructible_V<T, TArgs...>;

/// @brief Satisfied when T's copy-assignment operator is declared noexcept.
template <typename T>
concept CNothrowCopyAssignable = TIsNothrowAssignable_V<T&, const T&>;

/// @brief Satisfied when T's move-assignment operator is declared noexcept.
template <typename T>
concept CNothrowMoveAssignable = TIsNothrowAssignable_V<T&, T&&>;

/// @brief Satisfied when T is assignable from U with a noexcept guarantee.
template <typename T, typename U>
concept CNothrowAssignableFrom = TIsNothrowAssignable_V<T, U>;

/// @brief Satisfied when T's destructor is declared noexcept.
template <typename T>
concept CNothrowDestructible = TIsNothrowDestructible_V<T>;

/// @brief Satisfied when T can be both nothrow copy and nothrow move constructed.
template <typename T>
concept CNothrowMoveCopyConstructible =
    CNothrowCopyConstructible<T> &&
    CNothrowMoveConstructible<T>;

/// @brief Satisfied when T can be both nothrow copy and nothrow move assigned.
template <typename T>
concept CNothrowMoveCopyAssignable =
    CNothrowCopyAssignable<T> &&
    CNothrowMoveAssignable<T>;

// clang-format on

}   // namespace GP
