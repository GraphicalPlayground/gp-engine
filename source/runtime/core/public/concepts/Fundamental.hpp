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

}   // namespace gp::concepts
