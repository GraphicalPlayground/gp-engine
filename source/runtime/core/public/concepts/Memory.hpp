// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <type_traits>

namespace gp::concepts
{

/// @brief Concept to check if a type is trivially copyable.
template <typename T>
concept IsTriviallyCopyable = std::is_trivially_copyable_v<T>;

/// @brief Concept to check if a type is trivially destructible.
template <typename T>
concept IsTriviallyDestructible = std::is_trivially_destructible_v<T>;

/// @brief Concept to check if a type is trivially default constructible.
template <typename T>
concept IsTriviallyConstructible = std::is_trivially_default_constructible_v<T>;

/// @brief Concept to check if a type has a standard layout.
template <typename T>
concept IsStandardLayout = std::is_standard_layout_v<T>;

/// @brief Concept to check if a type is Plain Old Data (POD).
template <typename T>
concept IsPod = IsStandardLayout<T> && std::is_trivial_v<T>;

}   // namespace gp::concepts
