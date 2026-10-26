// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "backend/ApplicationBackendDesc.hpp"
#include "CoreMinimal.hpp"
#include "time/EngineClock.hpp"
#include "window/WindowDesc.hpp"

namespace gp
{

/// @brief Configuration parameters for bootstrapping a `gp::application::Application` instance.
struct ApplicationDesc
{
    /// @brief OS backend and hardware subsystem initialization parameters.
    hal::ApplicationBackendDesc backend{};

    /// @brief Configuration for the primary application window (ignored if `createMainWindow == false`).
    hal::WindowDesc mainWindow{};

    /// @brief Configuration for the master engine clock, delta smoothing, and fixed physics rate.
    time::EngineClockDesc clock{};

    /// @brief Byte capacity of the lock-free SPSC event stream (automatically rounded up to a power of two).
    gp::USize eventRingBufferBytes{ 16384u };

    /// @brief Target frame rate in Hz when the application window is focused in the foreground (0.0 = uncapped).
    gp::Float64 targetForegroundFps{ 120.0 };

    /// @brief Target frame rate in Hz when the application window is unfocused in the background (0.0 = uncapped).
    gp::Float64 targetBackgroundFps{ 30.0 };

    /// @brief If true, automatically creates and opens the primary `IWindow` during initialization.
    bool createMainWindow{ true };

    /// @brief If true, blocks on `waitEventsTimeout` when minimized or suspended to drop CPU usage to 0%.
    bool sleepWhenMinimized{ true };
};

}   // namespace gp
