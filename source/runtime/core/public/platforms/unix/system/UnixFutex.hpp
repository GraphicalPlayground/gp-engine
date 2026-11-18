// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "platforms/generic/system/GenericFutex.hpp"

namespace gp::platform::unix
{

/// @brief Shared POSIX fallback for 32-bit address-based thread parking ([ADR-0058]).
/// @details Inherits the user-space backoff wait loop from `gp::platform::generic::Futex` for Unix targets that do not
///          provide Linux `sys_futex` or Darwin `__ulock_wait`.
class GP_CORE_API Futex : public gp::platform::generic::Futex
{
public:
    Futex() = delete;
};

}   // namespace gp::platform::unix
