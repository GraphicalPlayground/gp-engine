// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/MemoryForward.hpp"
#include "threading/roles/ThreadRoleTypes.hpp"

namespace gp::thread
{

/// @brief Creation parameters for `NamedThreadDispatcher`.
/// @details Each named thread gets its own bounded queue. Capacities are set per thread because the traffic differs a
///          lot: the render thread usually receives many small commands per frame, while the game thread mostly
///          receives completion callbacks.
struct NamedThreadDispatcherDesc
{
    /// @brief Minimum slot count of each queue, indexed by `NamedThread`. Rounded up to a power of two. Zero is
    ///        rejected.
    gp::USize queueCapacities[kNamedThreadCount]{ kDefaultNamedThreadQueueCapacity,
                                                  kDefaultNamedThreadQueueCapacity,
                                                  kDefaultNamedThreadQueueCapacity };

    /// @brief Allocator for the queue storage. `nullptr` selects `gp::memory::getGlobalMalloc()`.
    gp::memory::Malloc* allocator{ nullptr };
};

}   // namespace gp::thread
