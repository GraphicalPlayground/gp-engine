// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp
{

/// @brief A type-safe union that can hold one of the specified types.
template <typename... Ts>
requires(sizeof...(Ts) > 0) class Variant
{};

}   // namespace gp
