// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/HardwareConstants.hpp"

namespace gp::thread
{

/// @brief Stateless C-style function pointer signature for job tasks ([ADR-0022], [ADR-0051]).
/// @param[in] userData Opaque caller-managed task payload pointer.
using JobEntryFunction = void (*)(void* userData);

/// @brief Hardware L1 data cache-line size in bytes (64B on x86/ARM, 128B on Apple Silicon) used to isolate
///        queue cursors and ring slots ([ADR-0025], [ADR-0051]).
inline constexpr gp::USize kJobQueueCacheLineSize = gp::memory::HardwareConstants::cacheLineSize;

/// @brief Minimum valid power-of-two capacity in elements for any lock-free job queue.
inline constexpr gp::USize kMinJobQueueCapacity = 2u;

/// @brief Default power-of-two capacity (`1024` slots) for per-worker `WorkStealingDeque` instances.
inline constexpr gp::USize kDefaultWorkStealingCapacity = 1024u;

/// @brief Default power-of-two capacity (`4096` slots) for shared `MPMCRingQueue` instances.
inline constexpr gp::USize kDefaultMPMCQueueCapacity = 4096u;

}   // namespace gp::thread
