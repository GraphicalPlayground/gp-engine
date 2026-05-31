// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "concepts/Concepts.hpp"
#include "CoreMinimal.hpp"

namespace gp
{

/// @brief A copyable, assignable wrapper around a reference.
/// @tparam T The type of the reference being wrapped. Must be an object or function type.
template <typename T>
requires concepts::IsObject<T> || concepts::IsFunction<T> class ReferenceWrapper
{};

}   // namespace gp
