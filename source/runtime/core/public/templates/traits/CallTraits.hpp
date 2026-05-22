// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::trait
{

namespace detail
{

/// @brief Resolves ParamType and ConstParamType based on whether T qualifies for pass-by-value. Partial specialisation
/// for T* preserves pointer mutability.
/// @tparam T The type for which to determine the parameter types.
/// @tparam kPassByValue Whether T should be passed by value (true) or by const reference (false).
template <typename T, bool kPassByValue>
struct CallTraitsParamHelper
{
public:
    using ParamType = const T&;
    using ConstParamType = const T&;
};

/// @brief Specialization for types that qualify for pass-by-value, where ParamType is simply T (or T* for pointers).
/// @tparam T The type for which to determine the parameter types.
template <typename T>
struct CallTraitsParamHelper<T, true>
{
public:
    using ParamType = const T;
    using ConstParamType = const T;
};

/// @brief Specialization for pointer types that qualify for pass-by-value, where ParamType is T* (preserving pointer
/// mutability).
/// @tparam T The type pointed to by the pointer.
template <typename T>
struct CallTraitsParamHelper<T*, true>
{
public:
    using ParamType = T*;
    using ConstParamType = const T*;
};

}   // namespace detail

/// @brief Provides type traits to determine the optimal way to pass parameters of type T (by value or by const
/// reference).
/// @tparam T The type for which to determine the call traits.
/// @note The primary template handles general types, while specializations are provided for reference and array types
/// to ensure correct parameter type deduction.
template <typename T>
struct CallTraits
{
private:
    // Pass by value when the type is POD and register-sized, or simply arithmetic.
    static constexpr bool kPassByValue =
        (concepts::IsPod<T> && sizeof(T) <= sizeof(void*)) || concepts::IsArithmetic<T>;
    using ParamHelper = detail::CallTraitsParamHelper<T, kPassByValue>;

public:
    using ValueType = T;
    using Reference = T&;
    using ConstReference = const T&;
    using ParamType = typename ParamHelper::ParamType;
    using ConstParamType = typename ParamHelper::ConstParamType;
};

/// @brief Specialization for reference types to ensure that the parameter types are correctly deduced as references.
/// @tparam T The type being passed by reference.
/// @note Collapses reference-to-reference, both param forms are T& since the reference already binds without copying.
template <typename T>
struct CallTraits<T&>
{
public:
    using ValueType = T&;
    using Reference = T&;
    using ConstReference = const T&;
    using ParamType = T&;
    using ConstParamType = T&;
};

/// @brief Specialization for arrays to ensure that the parameter types are correctly deduced as pointers.
/// @tparam T The type of the elements in the array.
/// @tparam N The size of the array.
/// @note Arrays decay to a const pointer when passed; storing by reference is only meaningful for Reference /
/// ConstReference (e.g. the array as a local).
template <typename T, gp::USize N>
struct CallTraits<T[N]>
{
private:
    using ArrayType = T[N];

public:
    using ValueType = const T*;
    using Reference = ArrayType&;
    using ConstReference = const ArrayType&;
    using ParamType = const T* const;
    using ConstParamType = const T* const;
};

/// @brief Specialization for const arrays to ensure that the parameter types are correctly deduced as const pointers.
/// @tparam T The type of the elements in the array.
/// @tparam N The size of the array.
template <typename T, gp::USize N>
struct CallTraits<const T[N]>
{
private:
    using ArrayType = const T[N];

public:
    using ValueType = const T*;
    using Reference = ArrayType&;
    using ConstReference = const ArrayType&;
    using ParamType = const T* const;
    using ConstParamType = const T* const;
};

/// @brief Resolves to the optimal parameter type for T (value or const-ref).
template <typename T>
using CallParamType = typename CallTraits<T>::ParamType;

/// @brief Resolves to the optimal const parameter type for T.
template <typename T>
using CallConstParamType = typename CallTraits<T>::ConstParamType;

}   // namespace gp::trait
