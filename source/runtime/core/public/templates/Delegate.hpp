// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp
{

template <typename Signature>
class Delegate;

/// @brief A non-allocating callback wrapper for static functions, lambdas, and member functions.
/// @tparam Ret The return type of the function signature.
/// @tparam Args The argument types of the function signature.
template <typename Ret, typename... Args>
class Delegate<Ret(Args...)>
{};

}   // namespace gp
