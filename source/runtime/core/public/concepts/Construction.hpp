// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <concepts>

namespace gp::concepts
{

/// @brief Concept to check if a type can be default constructed.
template <typename T>
concept IsDefaultConstructible = std::default_initializable<T>;

/// @brief Concept to check if a type can be copy constructed.
template <typename T>
concept IsCopyConstructible = std::copy_constructible<T>;

/// @brief Concept to check if a type can be move constructed.
template <typename T>
concept IsMoveConstructible = std::move_constructible<T>;

/// @brief Concept to check if a type can be copy assigned.
template <typename T>
concept IsCopyAssignable = std::assignable_from<T&, const T&>;

/// @brief Concept to check if a type can be move assigned.
template <typename T>
concept IsMoveAssignable = std::assignable_from<T&, T>;

}   // namespace gp::concepts
