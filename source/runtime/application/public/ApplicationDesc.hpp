// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "ApplicationTypes.hpp"
#include "backend/ApplicationBackendDesc.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "logging/LogDesc.hpp"
#include "threading/jobs/JobSystemDesc.hpp"
#include "threading/roles/NamedThreadDispatcherDesc.hpp"
#include "time/EngineClock.hpp"
#include "window/WindowDesc.hpp"

namespace gp
{

/// @brief Configuration parameters for bootstrapping a `gp::Application` instance ([ADR-0012], [ADR-0053],
///        [ADR-0054]).
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

    /// @brief Queue capacities and allocator for `Application::getNamedThreadDispatcher()`.
    thread::NamedThreadDispatcherDesc namedThreadDispatcher{};

    /// @brief Configuration for the central `gp::log::Logger` router ([ADR-0054]).
    log::LoggerDesc logger{};

    /// @brief Configuration for the built-in ANSI terminal `ConsoleSink` ([ADR-0054]).
    log::ConsoleSinkDesc consoleSink{};

    /// @brief Configuration for the built-in buffered `FileSink` ([ADR-0054]).
    log::FileSinkDesc fileSink{};

    /// @brief Optional application subfolder name passed to `gp::vfs::VirtualFileSystem::initializeDefaultMounts()`
    ///        ([ADR-0044]).
    /// @details When empty, `Application` falls back to `backend.appName` (and `VirtualFileSystem` falls back to the
    ///          executable stem if that is also empty). Can be overridden via `-vfs-app=<name>` on the command line.
    gp::StringView vfsAppName{};

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

    /// @brief If true, claims `NamedThread::Render` and `NamedThread::RHI` on the main loop thread alongside
    ///        `NamedThread::Game` and pumps their queues each frame. Set to false when dedicated Render/RHI OS threads
    ///        own those roles.
    bool singleThreadedRenderAndRHI{ true };

    /// @brief If true, initializes `gp::log::Logger::instance()` during startup and shuts it down on exit (can be
    ///        overridden by `-no-log` on the command line).
    bool enableLogging{ true };

    /// @brief If true, registers the application's `ConsoleSink` with `Logger::instance()` (can be overridden by
    ///        `-no-console-log` on the command line).
    bool enableConsoleLogSink{ true };

    /// @brief If true, opens and registers the application's `FileSink` with `Logger::instance()` (can be overridden
    ///        by `-log-file` or `-no-file-log` on the command line).
    bool enableFileLogSink{ false };

    /// @brief If true, registers the application's OS debugger `DebugOutputSink` with `Logger::instance()`.
    bool enableDebugOutputLogSink{ true };

    /// @brief If true, registers the application's `TracySink` with `Logger::instance()`.
    bool enableTracyLogSink{ true };

    /// @brief If true, mounts the six default ADR-0044 VFS schemes (`engine://`, `game://`, `user://`, `appdata://`,
    ///        `cache://`, `temp://`) during startup and unmounts them on exit (can be overridden by `-no-vfs` on the
    ///        command line).
    bool enableVirtualFileSystem{ true };
};

}   // namespace gp
