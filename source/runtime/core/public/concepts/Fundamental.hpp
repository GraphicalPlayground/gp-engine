// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <type_traits>

namespace gp::concepts
{

/// @brief Concept to check if a type is a floating-point type.
template <typename T>
concept IsFloatingPoint = std::is_floating_point_v<T>;

/// @brief Concept to check if a type is an integral type.
template <typename T>
concept IsIntegral = std::is_integral_v<T>;

/// @brief Concept to check if a type is an arithmetic type.
template <typename T>
concept IsArithmetic = std::is_arithmetic_v<T>;

/// @brief Concept to check if a type is a pointer type.
template <typename T>
concept IsPointer = std::is_pointer_v<T>;

/// @brief Concept to check if a type is an enumeration.
template <typename T>
concept IsEnum = std::is_enum_v<T>;

/// @brief Concept to check if a type is a class or struct.
template <typename T>
concept IsClass = std::is_class_v<T>;

/// @brief Concept to check if a type is a character type.
template <typename T>
concept IsCharacter = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char8_t> ||
                      std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

/// @brief Concept to check if a type is a void type.
template <typename T>
concept IsVoid = std::is_void_v<T>;

/// @brief Concept to check if a type is a null pointer type.
template <typename T>
concept IsNullPointer = std::is_null_pointer_v<T>;

/// @brief Concept to check if a type is a function.
template <typename T>
concept IsFunction = std::is_function_v<T>;

/// @brief Concept to check if a type is an object type.
template <typename T>
concept IsObject = std::is_object_v<T>;

/// @brief Concept to check if a type is a scalar type.
template <typename T>
concept IsScalar = std::is_scalar_v<T>;

/// @brief Concept to check if a type is a fundamental type.
template <typename T>
concept IsFundamental = std::is_fundamental_v<T>;

/// @brief Concept to check if a type is a reference type.
template <typename T>
concept IsReference = std::is_reference_v<T>;

/// @brief Concept to check if a type is an array type.
template <typename T>
concept IsArray = std::is_array_v<T>;

}   // namespace gp::concepts
