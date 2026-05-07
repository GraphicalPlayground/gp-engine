// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <concepts>

namespace gp::concepts
{

/// @brief Concept to check if a callable type can be invoked with specific arguments.
template <typename Callable, typename... Args>
concept IsInvocable = std::invocable<Callable, Args...>;

/// @brief Concept to check if a callable returns a boolean when invoked.
template <typename Callable, typename... Args>
concept IsPredicate = std::predicate<Callable, Args...>;

}   // namespace gp::concepts
