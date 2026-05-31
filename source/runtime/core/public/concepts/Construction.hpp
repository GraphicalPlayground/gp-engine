// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <concepts>

namespace gp::concepts
{

/// @brief Concept to check if a type can be default constructed.
/// @note In many engine systems, we use pools or pre-allocated buffers where objects are
/// default-constructed first and then initialized later with actual data.
template <typename T>
concept IsDefaultConstructible = std::default_initializable<T>;

/// @brief Concept to check if a type can be copy constructed.
/// @note Pedagogical Note: Copying can be expensive! In game engines, we prefer to move or
/// pass by reference whenever possible to avoid unnecessary allocations and data duplication.
template <typename T>
concept IsCopyConstructible = std::copy_constructible<T>;

/// @brief Concept to check if a type can be move constructed.
/// @note Move semantics allow us to "steal" resources from temporary objects, which is
/// essential for returning large containers or objects from functions without performance loss.
template <typename T>
concept IsMoveConstructible = std::move_constructible<T>;

/// @brief Concept to check if a type can be copy assigned.
template <typename T>
concept IsCopyAssignable = std::assignable_from<T&, const T&>;

/// @brief Concept to check if a type can be move assigned.
template <typename T>
concept IsMoveAssignable = std::assignable_from<T&, T>;

/// @brief Concept to check if a type is constructible with specific arguments.
template <typename T, typename... Args>
concept IsConstructibleWith = std::constructible_from<T, Args...>;

/// @brief Concept to check if a type is destructible.
/// @note Destructors are where we release resources (memory, file handles, GPU buffers).
/// Ensuring a type is destructible is vital for preventing resource leaks.
template <typename T>
concept IsDestructible = std::destructible<T>;

}   // namespace gp::concepts
