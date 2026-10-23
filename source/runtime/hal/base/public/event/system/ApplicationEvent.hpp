// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::hal::event
{

/// @brief Defines lifecycle and OS-level state transitions for the application process.
enum class ApplicationAction : gp::UInt8
{
    None = 0,
    QuitRequested,         // User or OS requested process termination (e.g., Alt+F4, Cmd+Q)
    WillEnterBackground,   // Application is about to lose foreground status (mobile/console suspend)
    DidEnterBackground,    // Application is now backgrounded; release non-critical GPU resources
    WillEnterForeground,   // Application is about to resume from background
    DidEnterForeground,    // Application is active in the foreground again
    LowMemoryWarning,      // OS memory pressure is critical; flush caches immediately
    LocaleChanged,         // User changed OS language/region settings at runtime
    SystemThemeChanged     // OS toggled between Dark Mode and Light Mode
};

/// @brief Fixed-size POD payload for EventCategory::Application (4 bytes, zero implicit padding).
struct ApplicationEvent
{
    ApplicationAction action = ApplicationAction::None;
    gp::UInt8 reserved[3] = { 0u, 0u, 0u };   // Explicit 4-byte alignment padding
};

static_assert(sizeof(ApplicationEvent) == 4, "ApplicationEvent size mismatch!");

}   // namespace gp::hal::event
