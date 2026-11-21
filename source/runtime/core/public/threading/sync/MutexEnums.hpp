// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::thread
{

/// @brief Lifecycle state machine values for `gp::thread::OnceFlag` ([ADR-0058]).
enum class OnceState : gp::UInt32
{
    /// @brief One-time initialization has not yet started.
    NotStarted = 0u,

    /// @brief A thread or fiber is actively executing the one-time initialization callable with no waiters yet.
    Running = 1u,

    /// @brief A thread is executing the initialization callable and one or more contending threads are parked.
    RunningWithWaiters = 2u,

    /// @brief The one-time initialization callable has finished and published its effects with Release ordering.
    Completed = 3u
};

}   // namespace gp::thread
