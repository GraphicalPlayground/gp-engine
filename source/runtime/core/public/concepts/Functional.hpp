// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <concepts>

namespace gp::concepts
{

/// @brief Concept to check if a callable type can be invoked with specific arguments.
/// @note Pedagogical Note: Using concepts to constrain callables is often superior to using
/// std::function because it avoids the overhead of type erasure, heap allocation, and virtual
/// dispatch, allowing the compiler to inline the call directly.
template <typename Callable, typename... Args>
concept IsInvocable = std::invocable<Callable, Args...>;

/// @brief Concept to check if a callable returns a boolean when invoked.
/// @note Predicates are the foundation of many engine algorithms, such as visibility culling
/// or event filtering.
template <typename Callable, typename... Args>
concept IsPredicate = std::predicate<Callable, Args...>;

}   // namespace gp::concepts
