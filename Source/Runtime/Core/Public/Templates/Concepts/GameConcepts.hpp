// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "Templates/Concepts/ComparisonConcepts.hpp"
#include "Templates/Concepts/CoreConcepts.hpp"
#include "Templates/Concepts/ObjectConcepts.hpp"

namespace GP
{

// clang-format off

/// @brief Satisfied when T supports the four arithmetic operators.
template <typename T>
concept CNumeric =
    CArithmetic<T> ||
    requires (T a, T b)
    {
        { a + b } -> CSameAs<T>;
        { a - b } -> CSameAs<T>;
        { a * b } -> CSameAs<T>;
        { a / b } -> CSameAs<T>;
    };

/// @brief Satisfied when T behaves as a 2D, 3D, or 4D vector.
///        Requires scalar-typed component access and arithmetic operators.
template <typename T>
concept CVectorType =
    CTriviallyCopyable<T> &&
    requires (T a, T b, float s)
    {
        { a + b  } -> CSameAs<T>;
        { a - b  } -> CSameAs<T>;
        { a * s  } -> CSameAs<T>;
        { a / s  } -> CSameAs<T>;
        { -a     } -> CSameAs<T>;
    };

/// @brief Satisfied when T represents a matrix type.
template <typename T>
concept CMatrixType =
    CTriviallyCopyable<T> &&
    requires (T a, T b)
    {
        { a * b } -> CSameAs<T>;
    };

/// @brief Satisfied when T represents a quaternion type.
template <typename T>
concept CQuaternionType =
    CVectorType<T>        &&
    requires (T a, T b)
    {
        { a * b } -> CSameAs<T>;
    };

/// @brief Satisfied when T is a plain pointer or smart pointer that dereferences.
template <typename T>
concept CDereferenceable =
    requires (T& ptr) { { *ptr }; };

/// @brief Satisfied when T is nullable (comparable to nullptr).
template <typename T>
concept CNullable =
    CDereferenceable<T> &&
    requires (T& ptr) { { ptr == nullptr } -> CConvertibleTo<bool>; };

/// @brief Satisfied when T is hashable via a call operator on a hasher.
template <typename T, typename THasher>
concept CHashable =
    requires (const T& val, THasher& hasher)
    {
        { hasher(val) } -> CUnsignedIntegral;
    };

/// @brief Satisfied when T can be serialized to and from a byte stream.
template <typename T, typename TStream>
concept CSerializable =
    requires (T& val, TStream& stream)
    {
        { stream << val };
        { stream >> val };
    };

/// @brief Satisfied when T behaves as a named, identifiable resource.
template <typename T>
concept CNamedResource =
    requires (const T& res)
    {
        { res.GetName() } -> CConvertibleTo<const char*>;
        { res.GetID()   } -> CUnsignedIntegral;
    };

/// @brief Satisfied when T supports a tick/update method taking a delta-time float.
template <typename T>
concept CTickable =
    requires (T& obj, float DeltaTime) { obj.Tick(DeltaTime); };

// clang-format on

}   // namespace GP
