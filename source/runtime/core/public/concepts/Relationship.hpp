// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <concepts>

namespace gp::concepts
{

/// @brief Concept to check if two types are exactly the same.
template <typename T, typename U>
concept IsSameAs = std::same_as<T, U>;

/// @brief Concept to check if type Derived inherits from type Base.
template <typename Derived, typename Base>
concept IsDerivedFrom = std::derived_from<Derived, Base>;

/// @brief Concept to check if type From is implicitly convertible to type To.
template <typename From, typename To>
concept IsConvertibleTo = std::convertible_to<From, To>;

}   // namespace gp::concepts
