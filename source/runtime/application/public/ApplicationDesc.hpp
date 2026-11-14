// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "ApplicationTypes.hpp"
#include "backend/ApplicationBackendDesc.hpp"
#include "CoreMinimal.hpp"
#include "threading/jobs/JobSystemDesc.hpp"
#include "time/EngineClock.hpp"
#include "window/WindowDesc.hpp"

namespace gp
{

/// @brief Configuration parameters for bootstrapping a `gp::Application` instance ([ADR-0012], [ADR-0053]).
struct ApplicationDesc
{
    /// @brief OS backend and hardware subsystem initialization parameters.
    hal::ApplicationBackendDesc backend{};

    /// @brief Configuration for the primary application window (ignored if `createMainWindow == false`).
    hal::WindowDesc mainWindow{};

    /// @brief Configuration for the master engine clock, delta smoothing, and fixed physics rate.
    time::EngineClockDesc clock{};

    /// @brief Configuration for the `gp::thread::JobSystem` fiber pool, queues, and worker threads ([ADR-0052],
    ///        [ADR-0053]).
    thread::JobSystemDesc jobSystem{};

    /// @brief Byte capacity of the lock-free SPSC event stream (automatically rounded up to a power of two).
    gp::USize eventRingBufferBytes{ 16384u };

    /// @brief Byte capacity of each of the two double-buffered per-frame `LinearAllocator` arenas (`Frame N` and
    ///        `Frame N-1`).
    gp::USize frameArenaBytes{ kDefaultFrameArenaCapacityInBytes };

    /// @brief Target frame rate in Hz when the application window is focused in the foreground (`0.0` = uncapped).
    gp::Float64 targetForegroundFps{ 120.0 };

    /// @brief Target frame rate in Hz when the application window is unfocused in the background (`0.0` = uncapped).
    gp::Float64 targetBackgroundFps{ 30.0 };

    /// @brief Maximum number of DAG nodes supported per frame in `Application::getFrameGraph()`.
    gp::UInt32 maxFrameGraphNodes{ kDefaultFrameGraphMaxNodes };

    /// @brief Maximum outgoing dependent edges per node in `Application::getFrameGraph()`.
    gp::UInt16 maxFrameGraphDependentsPerNode{ kDefaultFrameGraphMaxDependentsPerNode };

    /// @brief If true, automatically creates and opens the primary `IWindow` during initialization.
    bool createMainWindow{ true };

    /// @brief If true, blocks on `waitEventsTimeout` when minimized or suspended to drop CPU usage to 0%.
    bool sleepWhenMinimized{ true };

    /// @brief If true, initializes `Application::getJobSystem()` during startup (can be overridden by
    ///        `-no-jobsystem` on the command line).
    bool enableJobSystem{ true };
};

}   // namespace gp
