// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include <atomic>

namespace GP
{

/// @note We prefer to use std::atomic directly instead of wrapping it in our own class, as it is already a thin wrapper
///     around platform intrinsics and adding another layer of indirection would only add overhead. However, we provide
///     this alias for convenience and consistency with our naming conventions.
template <typename T>
using TAtomic = std::atomic<T>;

}   // namespace GP
