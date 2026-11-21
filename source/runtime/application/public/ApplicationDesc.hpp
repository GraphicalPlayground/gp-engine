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
public:
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

    /// @brief Additional shared library search directories registered with `gp::modules::ModuleManager` on startup
    ///        ([ADR-0061]).
    gp::StringView moduleSearchDirectories[kMaxApplicationModuleSearchDirectories]{};

    /// @brief Number of active entries populated in `moduleSearchDirectories` (`<=
    /// kMaxApplicationModuleSearchDirectories`).
    gp::UInt32 moduleSearchDirectoryCount{ 0u };

    /// @brief Engine or backend module identifiers (e.g., `"rhi/null"`, `"rhi/d3d12"`, `"audio/openal"`) loaded
    ///        automatically during `Application::initializeInternal()` prior to `onInitialize()` ([ADR-0061]).
    /// @details Can be augmented or overridden on the command line via `-load-module=<name>` or
    ///          `-modules=<modA,modB,...>`.
    gp::StringView startupModules[kMaxApplicationStartupModules]{};

    /// @brief Number of active entries populated in `startupModules` (`<= kMaxApplicationStartupModules`).
    gp::UInt32 startupModuleCount{ 0u };

    /// @brief If true, initializes `gp::modules::ModuleManager` search paths and loads `startupModules` during startup
    ///        (can be disabled via `-no-modules` on the command line) ([ADR-0061]).
    bool enableModuleSystem{ true };

    /// @brief If true, aborts `Application::run()` initialization cleanly if any module in `startupModules` fails to
    ///        load ([ADR-0061]).
    bool failOnStartupModuleLoadError{ true };

    /// @brief If true, automatically unloads all modules loaded by this `Application` in reverse-LIFO order during
    ///        `Application::shutdownInternal()` after `JobSystem` workers and named-thread queues drain ([ADR-0061]).
    bool unloadModulesOnShutdown{ true };

public:
    /// @brief Appends a module identifier to `startupModules` if capacity permits and not already listed.
    /// @param[in] moduleName Module identifier to schedule for startup loading (e.g., `"rhi/null"`).
    /// @return True if added (or already present), false if `moduleName` is empty or capacity is exhausted.
    constexpr bool addStartupModule(const gp::StringView moduleName) noexcept
    {
        if (moduleName.isEmpty())
        {
            return false;
        }
        for (gp::UInt32 i = 0u; i < startupModuleCount; ++i)
        {
            if (startupModules[i].equalsIgnoreCase(moduleName))
            {
                return true;
            }
        }
        if (startupModuleCount >= kMaxApplicationStartupModules)
        {
            return false;
        }
        startupModules[startupModuleCount++] = moduleName;
        return true;
    }

    /// @brief Appends a directory path to `moduleSearchDirectories` if capacity permits.
    /// @param[in] directory Directory path (UTF-8) to register with `ModuleManager`.
    /// @return True if added (or already present), false if `directory` is empty or capacity is exhausted.
    constexpr bool addModuleSearchDirectory(const gp::StringView directory) noexcept
    {
        if (directory.isEmpty())
        {
            return false;
        }
        for (gp::UInt32 i = 0u; i < moduleSearchDirectoryCount; ++i)
        {
            if (moduleSearchDirectories[i].equalsIgnoreCase(directory))
            {
                return true;
            }
        }
        if (moduleSearchDirectoryCount >= kMaxApplicationModuleSearchDirectories)
        {
            return false;
        }
        moduleSearchDirectories[moduleSearchDirectoryCount++] = directory;
        return true;
    }
};

}   // namespace gp
