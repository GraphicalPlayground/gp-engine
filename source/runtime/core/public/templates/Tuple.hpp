// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp
{

template <typename... Types>
class Tuple;

template <>
class Tuple<>
{};

template <typename Head, typename... Tail>
class Tuple<Head, Tail...> : public Tuple<Tail...>
{};

}   // namespace gp
