// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <concepts>
#include <type_traits>

namespace gp::concepts
{

/// @brief Concept to check if two types are exactly the same.
template <typename T, typename U>
concept IsSameAs = std::same_as<T, U>;

/// @brief Concept to check if type Derived inherits from type Base.
/// @note In engine architecture, we often use inheritance for interfaces (e.g., IModule),
/// but prefer composition for performance-critical data (Data-Oriented Design).
template <typename Derived, typename Base>
concept IsDerivedFrom = std::derived_from<Derived, Base>;

/// @brief Concept to check if type From is implicitly convertible to type To.
template <typename From, typename To>
concept IsConvertibleTo = std::convertible_to<From, To>;

/// @brief Concept to check if type From is a pointer that can be implicitly converted to a pointer of type To.
template <typename From, typename To>
concept IsPointerConvertibleTo = std::convertible_to<From*, To*>;

/// @brief Concept to check if two types are identical after decaying.
/// @note Pedagogical Note: Decaying a type removes references and cv-qualifiers (const, volatile).
/// This is extremely useful in template metaprogramming when we want to compare a type
/// passed by forwarded reference to a base type.
template <typename T, typename U>
concept IsDecayedSameAs = IsSameAs<std::decay_t<T>, std::decay_t<U>>;

/// @brief Concept to check if type T is present in a variadic pack of types Us.
/// @note This is a powerful tool for ECS (Entity Component System) queries or variant-like structures,
/// allowing us to check if a component type belongs to a specific set at compile-time.
template <typename T, typename... Us>
concept IsOneOf = (std::same_as<T, Us> || ...);

}   // namespace gp::concepts
