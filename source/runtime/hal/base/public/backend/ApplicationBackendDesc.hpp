// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "backend/ApplicationBackendEnums.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"

namespace gp::hal
{

/// @brief Configuration descriptor passed to `IApplicationBackend::initialize()`.
struct ApplicationBackendDesc
{
    /// @brief UTF-8 application name surfaced to the OS window manager and audio mixer.
    gp::StringView appName = "GP-Engine";

    /// @brief UTF-8 semantic version string (e.g., "1.0.0").
    gp::StringView appVersion = "1.0.0-alpha";

    /// @brief Reverse-domain application identifier (e.g., "com.graphical-playground.engine").
    gp::StringView appIdentifier = "com.graphical-playground.engine";

    /// @brief Bitmask of hardware subsystems to initialize.
    BackendSubsystemFlags subsystems = BackendSubsystemFlags::Default;

    /// @brief Maximum number of OS events to translate in a single `pumpEvents()` call (0 = unlimited).
    /// @note Prevents a maliciously flooded OS message queue from starving the OS thread loop.
    gp::UInt32 maxEventsPerPump = 1024u;

    /// @brief Bytes reserved at the tail of the SPSC ring buffer strictly for discrete critical events.
    /// @details High-frequency continuous events (mouse motion, analog stick deltas, IMU sensors) are shed
    ///          when remaining ring buffer space drops below this threshold, guaranteeing that critical state
    ///          transitions (Quit, Window Resize, Key Release) are never dropped during a Game Thread stall.
    gp::UInt32 criticalReserveBytes = 256u;
};

}   // namespace gp::hal
