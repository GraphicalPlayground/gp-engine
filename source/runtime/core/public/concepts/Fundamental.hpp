// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <cstddef>           // For std::byte
#include <type_traits>

namespace gp::concepts
{

/// @brief Concept to check if a type is a floating-point type (float, double, long double).
/// @note In game engines, we prefer 'float' for most transformations to save memory and bandwidth,
/// using 'double' only when high precision is required (e.g., large world coordinates).
template <typename T>
concept IsFloatingPoint = std::is_floating_point_v<T>;

/// @brief Concept to check if a type is an integral type (int, uint, short, long, etc.).
/// @note Integral types are the backbone of indexing, bitfields, and discrete state machines.
template <typename T>
concept IsIntegral = std::is_integral_v<T>;

/// @brief Concept to check if a type is an arithmetic type (Integral or Floating Point).
template <typename T>
concept IsArithmetic = std::is_arithmetic_v<T>;

/// @brief Concept to check if a type is a strict numeric type (Arithmetic but NOT bool or character).
/// @note Pedagogical Note: In C++, 'bool' and 'char' are technically arithmetic types, which can
/// lead to bugs in mathematical templates. IsNumeric ensures we are dealing with actual numbers.
template <typename T>
concept IsNumeric = IsArithmetic<T> && !std::is_same_v<T, bool> && !std::is_same_v<std::remove_cv_t<T>, char> &&
                    !std::is_same_v<std::remove_cv_t<T>, wchar_t> && !std::is_same_v<std::remove_cv_t<T>, char8_t> &&
                    !std::is_same_v<std::remove_cv_t<T>, char16_t> && !std::is_same_v<std::remove_cv_t<T>, char32_t>;

/// @brief Concept to check if a type is a pointer type.
/// @note While raw pointers are often discouraged in modern C++, they are essential in engine
/// development for low-level memory management and interfacing with hardware APIs like Vulkan/DX12.
template <typename T>
concept IsPointer = std::is_pointer_v<T>;

/// @brief Concept to check if a type is an enumeration.
template <typename T>
concept IsEnum = std::is_enum_v<T>;

/// @brief Concept to check if a type is a scoped enumeration (enum class).
/// @note Pedagogical Note: Scoped enums are preferred in modern C++ as they don't leak names into
/// the surrounding scope and don't implicitly convert to integers, preventing many common bugs.
template <typename T>
concept IsScopedEnum = std::is_scoped_enum_v<T>;

/// @brief Concept to check if a type is a class or struct.
template <typename T>
concept IsClass = std::is_class_v<T>;

/// @brief Concept to check if a type is a character type.
template <typename T>
concept IsCharacter = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char8_t> ||
                      std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

/// @brief Concept to check if a type represents a raw byte of memory.
/// @note Byte types are crucial for raw memory buffers, serialization, and vertex data manipulation.
template <typename T>
concept IsByte = std::is_same_v<T, std::byte> || std::is_same_v<T, unsigned char> || std::is_same_v<T, char>;

/// @brief Concept to check if a type is a void type.
template <typename T>
concept IsVoid = std::is_void_v<T>;

/// @brief Concept to check if a type is a null pointer type (std::nullptr_t).
template <typename T>
concept IsNullPointer = std::is_null_pointer_v<T>;

/// @brief Concept to check if a type is a function.
template <typename T>
concept IsFunction = std::is_function_v<T>;

/// @brief Concept to check if a type is an object type.
/// @note An object type is any type that is not a reference, void, or function.
template <typename T>
concept IsObject = std::is_object_v<T>;

/// @brief Concept to check if a type is a scalar type.
/// @note Scalar types include arithmetic types, pointers, enums, and nullptr_t.
/// They are the "atoms" of the type system and are always passed by value efficiently.
template <typename T>
concept IsScalar = std::is_scalar_v<T>;

/// @brief Concept to check if a type is a fundamental type.
template <typename T>
concept IsFundamental = std::is_fundamental_v<T>;

/// @brief Concept to check if a type is a reference type (L-value or R-value).
template <typename T>
concept IsReference = std::is_reference_v<T>;

/// @brief Concept to check if a type is an array type.
template <typename T>
concept IsArray = std::is_array_v<T>;

}   // namespace gp::concepts
