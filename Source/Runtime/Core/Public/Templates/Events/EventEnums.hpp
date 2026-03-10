// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"

namespace GP
{

/// @brief Execution priority for event listeners.
///
/// When Broadcast() is called on a TEvent, listeners are invoked in
/// strictly descending priority order: Critical first, Low last. All listeners
/// within the same priority level fire in subscription order.
///
/// @par Guidance
/// | Priority  | Use case                                                          |
/// |-----------|-------------------------------------------------------------------|
/// | Critical  | Must-see-first systems: Save, Anti-cheat, Input forwarding.       |
/// |           | Typically cancel or redirect the event.                           |
/// | High      | Core engine systems: Physics, Animation, UI layout.               |
/// | Normal    | Default. Gameplay code, AI, scripting.                            |
/// | Low       | Observers that must run last: logging, telemetry, analytics.      |
enum class EEventPriority : UInt8
{
    Critical = 0,   //<! Critical events must be processed before all others. Use sparingly.
    High = 1,       //<! High-priority events are processed after critical events, but before normal events.
    Normal = 2,     //<! Normal events are processed after critical and high-priority events.
    Low = 3,        //<! Low-priority events are processed after all higher-priority events.
    COUNT = 4       //<! Sentinel value for iteration and validation; not a valid priority.
};

/// @brief Total number of priority levels.
static constexpr Int32 kEventPriorityCount = static_cast<Int32>(EEventPriority::COUNT);

/// @brief Converts a priority enum value to its array index.
GP_NODISCARD GP_INLINE constexpr SizeT ToIndex(EEventPriority priority) noexcept
{
    return static_cast<SizeT>(priority);
}

}   // namespace GP
