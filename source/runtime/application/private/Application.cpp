// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "Application.hpp"
#include "backend/GenericApplicationBackend.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleCommand.hpp"   // IWYU pragma: keep
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/strings/FixedString.hpp"
#include "input/GenericInputSystem.hpp"
#include "logging/LogFormatter.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "profiling/Profiler.hpp"
#include "profiling/ProfilingControl.hpp"
#include "templates/Optional.hpp"

namespace gp
{

GP_DEFINE_LOG_CATEGORY(LogApplication, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);

namespace detail
{

/// @brief Maximum stack buffer size (in bytes) for decoding variable-length UTF-8 TLV payloads.
inline constexpr gp::USize kMaxDecodedStringBytes = 2048u;

/// @brief Minimum byte capacity enforced per frame arena so `m_frameGraph` buffers always fit comfortably.
inline constexpr gp::USize kMinFrameArenaBytes = 64ULL * 1024ULL;

[[nodiscard]] gp::StringView smoothingModeToString(const time::DeltaSmoothingMode mode) noexcept
{
    switch (mode)
    {
    case time::DeltaSmoothingMode::None:
        return "None";
    case time::DeltaSmoothingMode::ExponentialMovingAverage:
        return "ExponentialMovingAverage";
    case time::DeltaSmoothingMode::OutlierRejectedAverage:
        return "OutlierRejectedAverage";
    case time::DeltaSmoothingMode::FixedDeterministic:
        return "FixedDeterministic";
    default:
        return "OutlierRejectedAverage";
    }
}

[[nodiscard]] bool tryParseSmoothingMode(const gp::StringView text, time::DeltaSmoothingMode& outMode) noexcept
{
    if (text.equalsIgnoreCase("none") || text.equalsIgnoreCase("raw"))
    {
        outMode = time::DeltaSmoothingMode::None;
        return true;
    }
    if (text.equalsIgnoreCase("ema") || text.equalsIgnoreCase("exponentialmovingaverage"))
    {
        outMode = time::DeltaSmoothingMode::ExponentialMovingAverage;
        return true;
    }
    if (text.equalsIgnoreCase("outlier") || text.equalsIgnoreCase("outlierrejected") ||
        text.equalsIgnoreCase("outlierrejectedaverage"))
    {
        outMode = time::DeltaSmoothingMode::OutlierRejectedAverage;
        return true;
    }
    if (text.equalsIgnoreCase("fixed") || text.equalsIgnoreCase("fixeddeterministic"))
    {
        outMode = time::DeltaSmoothingMode::FixedDeterministic;
        return true;
    }
    return false;
}

/// @brief Peeks a fixed-size POD payload following an `EventHeader` and advances the SPSC consumer cursor.
/// @tparam InPayloadType Trivially copyable event struct type.
/// @param[in,out] stream Source SPSC byte ring buffer.
/// @param[out] outPayload Destination struct receiving the copied event bytes.
/// @return True if the payload was read and consumed, false if insufficient bytes were available.
template <typename InPayloadType>
[[nodiscard]] GP_FORCEINLINE_HINT bool
    consumeFixedPayload(gp::SPSCByteRingBuffer& stream, InPayloadType& outPayload) noexcept
{
    if (!stream.tryPeekAt(sizeof(hal::event::EventHeader), &outPayload, sizeof(InPayloadType))) [[unlikely]]
    {
        return false;
    }
    stream.advanceConsumer(sizeof(hal::event::EventHeader) + sizeof(InPayloadType));
    return true;
}

}   // namespace detail

Application::Application(
    const ApplicationDesc& desc,
    gp::UniquePtr<hal::IApplicationBackend> customBackend,
    gp::UniquePtr<hal::input::IInputSystem> customInput
) noexcept
    : m_desc(desc)
    , m_backend(std::move(customBackend))
    , m_inputSystem(std::move(customInput))
    , m_clock(desc.clock)
    , m_consoleSink(desc.consoleSink)
{}

Application::~Application() noexcept
{
    if (m_isInitialized)
    {
        shutdownInternal();
    }
}

void Application::requestQuit() noexcept
{
    m_isRunning.store(false, gp::MemoryOrder::Release);
    if (m_backend.isValid())
    {
        m_backend->wakeEventLoop();
    }
}

console::IConsoleManager& Application::getConsoleManager() noexcept
{
    return console::IConsoleManager::get();
}

console::ConsoleResult Application::executeConsoleCommand(
    const gp::StringView commandLine, console::ConsoleOutputDelegate* const output
) noexcept
{
    return console::IConsoleManager::get().processUserConsoleInput(commandLine, output);
}

void Application::applyCommandLineOverrides() noexcept
{
    if (!CommandLine::isInitialized())
    {
        return;
    }

    const Optional<bool> optEnableJobSystem = CommandLine::tryGetBool("jobsystem");
    if (optEnableJobSystem.hasValue())
    {
        m_desc.enableJobSystem = *optEnableJobSystem;
    }

    Optional<gp::UInt32> optWorkers = CommandLine::tryGetUInt32("threads");
    if (!optWorkers.hasValue())
    {
        optWorkers = CommandLine::tryGetUInt32("workers");
    }

    if (optWorkers.hasValue() && *optWorkers > 0u)
    {
        const gp::UInt32 requested = *optWorkers;
        m_desc.jobSystem.workerThreadCount =
            (requested > thread::kMaxJobSystemWorkers) ? thread::kMaxJobSystemWorkers : requested;
    }

    const Optional<bool> optAffinity = CommandLine::tryGetBool("core-affinity");
    if (optAffinity.hasValue())
    {
        m_desc.jobSystem.enableCoreAffinity = *optAffinity;
    }

    const Optional<bool> optLog = CommandLine::tryGetBool("log");
    if (optLog.hasValue())
    {
        m_desc.enableLogging = *optLog;
    }

    const Optional<bool> optConsoleLog = CommandLine::tryGetBool("console-log");
    if (optConsoleLog.hasValue())
    {
        m_desc.enableConsoleLogSink = *optConsoleLog;
    }

    const Optional<bool> optFileLog = CommandLine::tryGetBool("file-log");
    if (optFileLog.hasValue())
    {
        m_desc.enableFileLogSink = *optFileLog;
    }

    if (CommandLine::hasArgument("log-file"))
    {
        m_desc.enableFileLogSink = true;
        const Optional<StringView> customLogPath = CommandLine::tryGetString("log-file");
        if (customLogPath.hasValue() && !customLogPath->isEmpty())
        {
            m_desc.fileSink.filePath = *customLogPath;
        }
    }
}

void Application::initializeLoggingInternal() noexcept
{
    if (!m_desc.enableLogging)
    {
        return;
    }

    log::Logger& logger = log::Logger::instance();

    m_consoleSink.setMinLevel(m_desc.consoleSink.minLevel);
    m_consoleSink.setFlags(m_desc.consoleSink.flags);

    if (m_desc.enableConsoleLogSink)
    {
        (void)logger.addSink(&m_consoleSink);
    }

    if (m_desc.enableDebugOutputLogSink)
    {
        (void)logger.addSink(&m_debugOutputSink);
    }

    if (m_desc.enableTracyLogSink)
    {
        (void)logger.addSink(&m_tracySink);
    }

    if (m_desc.enableFileLogSink)
    {
        if (m_fileSink.open(m_desc.fileSink) == log::LogResult::Success)
        {
            (void)logger.addSink(&m_fileSink);
        }
    }

    if (!logger.isInitialized())
    {
        if (logger.initialize(m_desc.logger) == log::LogResult::Success)
        {
            m_ownsLoggerInitialization = true;
        }
    }
}

void Application::shutdownLoggingInternal() noexcept
{
    log::Logger& logger = log::Logger::instance();
    logger.flush();

    (void)logger.removeSink(&m_consoleSink);
    (void)logger.removeSink(&m_debugOutputSink);
    (void)logger.removeSink(&m_tracySink);
    (void)logger.removeSink(&m_fileSink);
    m_fileSink.close();

    if (m_ownsLoggerInitialization)
    {
        logger.shutdown();
        m_ownsLoggerInitialization = false;
    }
}

void Application::syncConsoleControlValues() noexcept
{
    if (m_consoleBindings.targetForegroundFps != nullptr)
    {
        const gp::Float64 fps = m_consoleBindings.targetForegroundFps->getFloat64();
        m_desc.targetForegroundFps = (fps > 0.0) ? fps : 0.0;
    }

    if (m_consoleBindings.targetBackgroundFps != nullptr)
    {
        const gp::Float64 bgFps = m_consoleBindings.targetBackgroundFps->getFloat64();
        m_desc.targetBackgroundFps = (bgFps > 0.0) ? bgFps : 0.0;
    }

    if (m_consoleBindings.sleepWhenMinimized != nullptr)
    {
        m_desc.sleepWhenMinimized = m_consoleBindings.sleepWhenMinimized->getBool();
    }

    if (m_consoleBindings.timeScale != nullptr)
    {
        m_clock.getGameplayClock().setLocalTimeScale(m_consoleBindings.timeScale->getFloat64());
    }

    if (m_consoleBindings.paused != nullptr)
    {
        m_clock.getGameplayClock().setPaused(m_consoleBindings.paused->getBool());
    }

    if (m_consoleBindings.smoothingMode != nullptr)
    {
        time::DeltaSmoothingMode mode = m_clock.getSmoothingMode();
        if (detail::tryParseSmoothingMode(m_consoleBindings.smoothingMode->getString().view(), mode))
        {
            m_clock.setSmoothingMode(mode);
        }
    }
}

void Application::initializeConsoleInternal() noexcept
{
    console::IConsoleManager& console = console::IConsoleManager::get();

    const console::ConsoleVarChangedDelegate syncCallback = console::ConsoleVarChangedDelegate::createLambda(
        [this](console::IConsoleVariable*) noexcept -> void
    {
        syncConsoleControlValues();
    }
    );

    m_consoleBindings.targetForegroundFps = console.registerConsoleVariable(
        "app.TargetFps",
        m_desc.targetForegroundFps,
        "Target foreground frame pacing rate in Hz (<= 0 disables pacing).",
        console::ConsoleVarFlags::None
    );
    m_consoleBindings.targetBackgroundFps = console.registerConsoleVariable(
        "app.BackgroundFps",
        m_desc.targetBackgroundFps,
        "Target background frame pacing rate in Hz (<= 0 disables pacing).",
        console::ConsoleVarFlags::None
    );
    m_consoleBindings.sleepWhenMinimized = console.registerConsoleVariable(
        "app.SleepWhenMinimized",
        m_desc.sleepWhenMinimized,
        "Suspends CPU polling with waitEventsTimeout while minimized or backgrounded.",
        console::ConsoleVarFlags::None
    );
    m_consoleBindings.timeScale = console.registerConsoleVariable(
        "t.TimeScale",
        m_clock.getGameplayClock().getLocalTimeScale(),
        "Gameplay simulation clock time dilation multiplier (>= 0.0).",
        console::ConsoleVarFlags::None
    );
    m_consoleBindings.paused = console.registerConsoleVariable(
        "t.Paused",
        m_clock.getGameplayClock().isLocallyPaused(),
        "Pauses the gameplay clock and all dependent domain simulation clocks.",
        console::ConsoleVarFlags::None
    );
    m_consoleBindings.smoothingMode = console.registerConsoleVariable(
        "t.SmoothingMode",
        detail::smoothingModeToString(m_clock.getSmoothingMode()),
        "EngineClock delta smoothing filter (None, EMA, OutlierRejectedAverage, FixedDeterministic).",
        console::ConsoleVarFlags::None
    );

    console::IConsoleVariable* const vars[] = { m_consoleBindings.targetForegroundFps,
                                                m_consoleBindings.targetBackgroundFps,
                                                m_consoleBindings.sleepWhenMinimized,
                                                m_consoleBindings.timeScale,
                                                m_consoleBindings.paused,
                                                m_consoleBindings.smoothingMode };
    for (console::IConsoleVariable* const var: vars)
    {
        if (var != nullptr)
        {
            var->setOnChangedCallback(syncCallback);
        }
    }

    m_consoleBindings.stepCmd = console.registerConsoleCommand(
        "t.Step",
        "Advances a paused gameplay simulation clock by a single fixed step.",
        console::ConsoleCommandWithArgsDelegate::createLambda(
            [this](const console::ConsoleCommandArgs& args) noexcept -> void
    {
        time::SimulationClock& gameplayClock = m_clock.getGameplayClock();
        if (gameplayClock.isLocallyPaused())
        {
            gameplayClock.requestSingleStep();
            args.writeLine("Requested single simulation step.");
        }
        else
        {
            args.writeLine("Gameplay clock is not paused (set t.Paused=1 first).");
        }
    }
        ),
        console::ConsoleVarFlags::None
    );

    m_consoleBindings.quitCmd = console.registerConsoleCommand(
        "app.Quit",
        "Signals the Application main loop to shut down cleanly at the end of the frame.",
        console::ConsoleCommandDelegate::createLambda(
            [this]() noexcept -> void
    {
        requestQuit();
    }
        ),
        console::ConsoleVarFlags::None
    );

    m_consoleBindings.statsCmd = console.registerConsoleCommand(
        "app.Stats",
        "Prints frame timing, pacing targets, and double-buffered frame arena utilization.",
        console::ConsoleCommandWithArgsDelegate::createLambda(
            [this](const console::ConsoleCommandArgs& args) noexcept -> void
    {
        const gp::time::FrameTimeStats& stats = m_clock.getStats();
        gp::FixedString<383> line{};
        (void)log::formatTo(
            line,
            "App='{}' frame={} fps={:.2f} targetFg={:.1f} targetBg={:.1f} arenaUsed={} arenaCap={}",
            m_desc.backend.appName,
            stats.frameIndex,
            static_cast<gp::Float64>(stats.smoothedFps),
            m_desc.targetForegroundFps,
            m_desc.targetBackgroundFps,
            getCurrentFrameArena().getUsedBytes(),
            getCurrentFrameArena().getCapacity()
        );
        args.writeLine(line.view());
    }
        ),
        console::ConsoleVarFlags::None
    );

    m_consoleBindings.jobsStatsCmd = console.registerConsoleCommand(
        "jobs.Stats",
        "Prints JobSystem worker counts, dispatched/stolen tasks, and fiber pool availability.",
        console::ConsoleCommandWithArgsDelegate::createLambda(
            [this](const console::ConsoleCommandArgs& args) noexcept -> void
    {
        if (!m_jobSystem.isInitialized())
        {
            args.writeLine("JobSystem is disabled.");
            return;
        }
        const thread::JobSystemStats stats = m_jobSystem.getStats();
        gp::FixedString<383> line{};
        (void)log::formatTo(
            line,
            "Jobs[workers={} sleeping={}] dispatched={} executed={} stolen={} switches={} freeStd={} freeLarge={}",
            stats.workerThreadCount,
            stats.sleepingWorkerCount,
            stats.totalJobsDispatched,
            stats.totalJobsExecuted,
            stats.totalJobsStolen,
            stats.totalFiberSwitches,
            stats.availableStandardFibers,
            stats.availableLargeFibers
        );
        args.writeLine(line.view());
    }
        ),
        console::ConsoleVarFlags::None
    );

    m_consoleBindings.sinkHandle = console.registerConsoleVariableSink(
        console::ConsoleSinkDelegate::createLambda(
            [this]() noexcept -> void
    {
        syncConsoleControlValues();
    }
        )
    );

    syncConsoleControlValues();
}

void Application::shutdownConsoleInternal() noexcept
{
    console::IConsoleManager& console = console::IConsoleManager::get();

    if (m_consoleBindings.sinkHandle.isValid())
    {
        (void)console.unregisterConsoleVariableSink(m_consoleBindings.sinkHandle);
        m_consoleBindings.sinkHandle.reset();
    }

    console::IConsoleObject* const objects[] = { m_consoleBindings.jobsStatsCmd,
                                                 m_consoleBindings.statsCmd,
                                                 m_consoleBindings.quitCmd,
                                                 m_consoleBindings.stepCmd,
                                                 m_consoleBindings.smoothingMode,
                                                 m_consoleBindings.paused,
                                                 m_consoleBindings.timeScale,
                                                 m_consoleBindings.sleepWhenMinimized,
                                                 m_consoleBindings.targetBackgroundFps,
                                                 m_consoleBindings.targetForegroundFps };
    for (console::IConsoleObject* const obj: objects)
    {
        if (obj != nullptr)
        {
            (void)console.unregisterConsoleObject(obj);
        }
    }
    m_consoleBindings = ConsoleControlBindings{};
}

bool Application::rebindFrameGraphToCurrentArena() noexcept
{
    m_frameGraph.shutdown();

    const gp::UInt32 maxNodes = (m_desc.maxFrameGraphNodes == 0u) ? 1u : m_desc.maxFrameGraphNodes;
    const gp::UInt16 maxDeps =
        (m_desc.maxFrameGraphDependentsPerNode == 0u) ? 1u : m_desc.maxFrameGraphDependentsPerNode;

    gp::memory::LinearAllocator& currentArena = getCurrentFrameArena();
    if (!currentArena.isValid()) [[unlikely]]
    {
        return false;
    }

    const gp::USize nodesBytes = static_cast<gp::USize>(maxNodes) * sizeof(thread::JobGraphNode);
    const gp::USize totalEdgeSlots = static_cast<gp::USize>(maxNodes) * static_cast<gp::USize>(maxDeps);
    const gp::USize edgesBytes = totalEdgeSlots * sizeof(thread::JobGraphNode*);

    void* const rawNodes = currentArena.allocate(nodesBytes, static_cast<gp::UInt32>(alignof(thread::JobGraphNode)));
    void* const rawEdges = currentArena.allocate(edgesBytes, static_cast<gp::UInt32>(alignof(thread::JobGraphNode*)));

    if (rawNodes == nullptr || rawEdges == nullptr) [[unlikely]]
    {
        return false;
    }

    const thread::JobSystemResult initRes = m_frameGraph.initializeWithBuffers(
        static_cast<thread::JobGraphNode*>(rawNodes), maxNodes, static_cast<thread::JobGraphNode**>(rawEdges), maxDeps
    );
    return initRes == thread::JobSystemResult::Success;
}

bool Application::rotateAndResetFrameArena(const gp::UInt64 frameIndex) noexcept
{
    // Detach m_frameGraph before rotating so ~JobGraphNode() runs on the outgoing graph without touching
    // the previous frame's arena cursor.
    m_frameGraph.shutdown();

    m_activeFrameArenaIndex = static_cast<gp::UInt32>(frameIndex & 1ULL);
    m_frameArenas[m_activeFrameArenaIndex].reset();

    return rebindFrameGraphToCurrentArena();
}

bool Application::initializeInternal() noexcept
{
    if (m_isInitialized) [[unlikely]]
    {
        return true;
    }

    GP_SET_THREAD_NAME("GP_MainThread");
    GP_PROFILE_SCOPE_NC_ACTIVE(
        "Application::InitializeInternal", GP_U32_COLOR_PURPLE, profiling::ProfilingControl::isZonesEnabled()
    );

    applyCommandLineOverrides();
    initializeLoggingInternal();
    (void)console::IConsoleManager::get();

    {
        gp::FixedString<256> appInfo{};
        log::formatTo(appInfo, "{} v{}", m_desc.backend.appName, m_desc.backend.appVersion);
        GP_APP_INFO(appInfo.cString(), appInfo.size());
    }

    GP_LOG_INFO(
        LogApplication,
        "Bootstrapping Application '{}' (version {})...",
        m_desc.backend.appName,
        m_desc.backend.appVersion
    );

    // Allocate the lock-free SPSC byte ring buffer.
    if (!m_eventStream.initialize(m_desc.eventRingBufferBytes)) [[unlikely]]
    {
        GP_LOG_ERROR(
            LogApplication, "Failed to initialize SPSC event ring buffer ({} bytes).", m_desc.eventRingBufferBytes
        );
        shutdownLoggingInternal();
        return false;
    }

    // Fall back to the dependency-free GenericApplicationBackend when no concrete backend was injected.
    if (!m_backend.isValid())
    {
        m_backend = gp::makeUnique<hal::generic::ApplicationBackend>();
        if (!m_backend.isValid()) [[unlikely]]
        {
            GP_LOG_ERROR(LogApplication, "Failed to allocate fallback GenericApplicationBackend.");
            m_eventStream.shutdown();
            shutdownLoggingInternal();
            return false;
        }
    }

    // Initialize the backend and enumerate display topology.
    if (m_backend->initialize(m_desc.backend) != hal::BackendResult::Success) [[unlikely]]
    {
        GP_LOG_ERROR(LogApplication, "HAL ApplicationBackend initialization failed.");
        m_eventStream.shutdown();
        shutdownLoggingInternal();
        return false;
    }

    // Ask the initialized backend to create its matching IInputSystem if none was injected.
    if (!m_inputSystem.isValid())
    {
        m_inputSystem = m_backend->createInputSystem();
        if (!m_inputSystem.isValid())
        {
            m_inputSystem = gp::makeUnique<hal::input::generic::InputSystem>();
        }
        if (!m_inputSystem.isValid()) [[unlikely]]
        {
            GP_LOG_ERROR(LogApplication, "Failed to create HAL IInputSystem.");
            m_backend->shutdown();
            m_eventStream.shutdown();
            shutdownLoggingInternal();
            return false;
        }
    }

    // Create the primary application window via the backend factory if requested.
    if (m_desc.createMainWindow)
    {
        if (m_desc.mainWindow.targetDisplay == nullptr)
        {
            m_desc.mainWindow.targetDisplay = m_backend->getPrimaryDisplay();
        }

        m_mainWindow = m_backend->createWindow(m_desc.mainWindow);
        if (!m_mainWindow.isValid() || !m_mainWindow->isValid()) [[unlikely]]
        {
            GP_LOG_ERROR(
                LogApplication,
                "Failed to create primary window '{}' ({}x{}).",
                m_desc.mainWindow.title,
                m_desc.mainWindow.size.width,
                m_desc.mainWindow.size.height
            );
            m_inputSystem.reset();
            m_backend->shutdown();
            m_eventStream.shutdown();
            shutdownLoggingInternal();
            return false;
        }
    }

    // Compute the minimum required arena size so m_frameGraph plus transient frame allocations always fit.
    const gp::UInt32 maxNodes = (m_desc.maxFrameGraphNodes == 0u) ? 1u : m_desc.maxFrameGraphNodes;
    const gp::UInt16 maxDeps =
        (m_desc.maxFrameGraphDependentsPerNode == 0u) ? 1u : m_desc.maxFrameGraphDependentsPerNode;
    const gp::USize graphFootprintBytes =
        (static_cast<gp::USize>(maxNodes) * sizeof(thread::JobGraphNode)) +
        (static_cast<gp::USize>(maxNodes) * static_cast<gp::USize>(maxDeps) * sizeof(thread::JobGraphNode*)) +
        detail::kMinFrameArenaBytes;
    const gp::USize arenaCapacityBytes =
        (m_desc.frameArenaBytes < graphFootprintBytes) ? graphFootprintBytes : m_desc.frameArenaBytes;

    if (!m_frameArenas[0].initialize(arenaCapacityBytes, m_desc.jobSystem.backingMalloc, "gp::FrameArena_0") ||
        !m_frameArenas[1].initialize(arenaCapacityBytes, m_desc.jobSystem.backingMalloc, "gp::FrameArena_1"))
        [[unlikely]]
    {
        GP_LOG_ERROR(
            LogApplication, "Failed to allocate double-buffered frame arenas ({} bytes each).", arenaCapacityBytes
        );
        m_frameArenas[1].shutdown();
        m_frameArenas[0].shutdown();
        if (m_mainWindow.isValid())
        {
            m_mainWindow->close();
            m_mainWindow.reset();
        }
        m_inputSystem.reset();
        m_backend->shutdown();
        m_eventStream.shutdown();
        shutdownLoggingInternal();
        return false;
    }

    m_activeFrameArenaIndex = 0u;
    if (!rebindFrameGraphToCurrentArena()) [[unlikely]]
    {
        GP_LOG_ERROR(LogApplication, "Failed to bind initial per-frame JobGraph to FrameArena_0.");
        m_frameArenas[1].shutdown();
        m_frameArenas[0].shutdown();
        if (m_mainWindow.isValid())
        {
            m_mainWindow->close();
            m_mainWindow.reset();
        }
        m_inputSystem.reset();
        m_backend->shutdown();
        m_eventStream.shutdown();
        shutdownLoggingInternal();
        return false;
    }

    if (m_desc.enableJobSystem)
    {
        if (m_jobSystem.initialize(m_desc.jobSystem) != thread::JobSystemResult::Success) [[unlikely]]
        {
            GP_LOG_ERROR(LogApplication, "Failed to initialize fiber JobSystem.");
            m_frameGraph.shutdown();
            m_frameArenas[1].shutdown();
            m_frameArenas[0].shutdown();
            if (m_mainWindow.isValid())
            {
                m_mainWindow->close();
                m_mainWindow.reset();
            }
            m_inputSystem.reset();
            m_backend->shutdown();
            m_eventStream.shutdown();
            shutdownLoggingInternal();
            return false;
        }
    }

    initializeConsoleInternal();
    m_clock.reset();
    m_isInitialized = true;
    GP_LOG_INFO(LogApplication, "Application core subsystems initialized.");
    return true;
}

void Application::shutdownInternal() noexcept
{
    if (!m_isInitialized)
    {
        return;
    }

    GP_PROFILE_SCOPE_NC_ACTIVE(
        "Application::ShutdownInternal", GP_U32_COLOR_RED, profiling::ProfilingControl::isZonesEnabled()
    );
    GP_LOG_INFO(LogApplication, "Shutting down Application core subsystems...");

    shutdownConsoleInternal();
    m_frameGraph.shutdown();

    if (m_jobSystem.isInitialized())
    {
        m_jobSystem.shutdown();
    }

    m_frameArenas[1].shutdown();
    m_frameArenas[0].shutdown();
    m_activeFrameArenaIndex = 0u;

    if (m_inputSystem.isValid())
    {
        m_inputSystem->reset();
        m_inputSystem.reset();
    }

    if (m_mainWindow.isValid())
    {
        m_mainWindow->close();
        m_mainWindow.reset();
    }

    if (m_backend.isValid())
    {
        m_backend->shutdown();
    }

    m_eventStream.shutdown();
    shutdownLoggingInternal();
    m_isInitialized = false;
}

gp::Int32 Application::run() noexcept
{
    if (!initializeInternal()) [[unlikely]]
    {
        return -1;
    }

    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "Application::OnInitialize", GP_U32_COLOR_PURPLE, profiling::ProfilingControl::isZonesEnabled()
        );
        if (!onInitialize()) [[unlikely]]
        {
            shutdownInternal();
            return -2;
        }
    }

    m_isRunning.store(true, gp::MemoryOrder::Release);
    m_clock.reset();

    while (m_isRunning.load(gp::MemoryOrder::Acquire))
    {
        const bool profZones = profiling::ProfilingControl::isZonesEnabled();
        const bool profPlots = profiling::ProfilingControl::isPlotsEnabled();
        GP_PROFILE_SCOPE_NC_ACTIVE("Application::Frame", GP_U32_COLOR_WHITE, profZones);

        // OS Power-Saving Wait when minimized or background-suspended.
        const bool windowMinimized = m_mainWindow.isValid() && m_mainWindow->isMinimized();
        if (m_isSuspended || (m_desc.sleepWhenMinimized && windowMinimized))
        {
            GP_PROFILE_SCOPE_NC_ACTIVE("Application::WaitEventsMinimized", GP_U32_COLOR_GRAY, profZones);
            (void)m_backend->waitEventsTimeout(100u);
        }

        // Clear previous frame's transient input edge latches and motion deltas, then pump OS events.
        {
            GP_PROFILE_SCOPE_NC_ACTIVE("Application::PumpEvents", GP_U32_COLOR_CYAN, profZones);
            m_inputSystem->beginFrame();
            (void)m_backend->pumpEvents(m_eventStream, &m_lastPumpStats);
            GP_PLOT_IF(profPlots, "Events Polled", static_cast<gp::Int64>(m_lastPumpStats.polledCount));
        }

        // Consumer Phase: Drain TLV packets, update Window/Display/Input shadow states, and invoke hooks.
        drainEventStream();

        if (!m_isRunning.load(gp::MemoryOrder::Acquire))
        {
            break;
        }

        // Frame-boundary CVar flush & dirty sink dispatch before advancing simulation clocks.
        console::IConsoleManager& console = console::IConsoleManager::get();
        (void)console.flushLatchedVariables();
        (void)console.callAllConsoleVariableSinks();

        // Advance hierarchical engine clocks and drain fixed-timestep accumulator.
        const gp::time::FrameTimeStats& timeStats = m_clock.tick();
        const gp::Float64 rawDeltaMs = timeStats.rawDelta.toMilliseconds();
        (void)profiling::ProfilingControl::recordFrameHitchIfExceeded(rawDeltaMs, timeStats.frameIndex);
        GP_PROFILE_ZONE_VALUE(timeStats.frameIndex);
        GP_PLOT_IF(profPlots, "Frame Delta (ms)", rawDeltaMs);
        GP_PLOT_IF(profPlots, "Smoothed FPS", static_cast<gp::Float64>(timeStats.smoothedFps));
        GP_PLOT_IF(profPlots, "Fixed Steps", static_cast<gp::Int64>(timeStats.fixedStep.stepCount));

        // Rotate the double-buffered frame arena (rewinding Frame N while keeping Frame N-1 intact) and
        // rebind m_frameGraph over the active frame arena without heap allocations.
        {
            GP_PROFILE_SCOPE_NC_ACTIVE("Application::RotateFrameArena", GP_U32_COLOR_ORANGE, profZones);
            if (!rotateAndResetFrameArena(timeStats.frameIndex)) [[unlikely]]
            {
                break;
            }
        }

        const ApplicationFrameContext frameContext{ .timeStats = timeStats,
                                                    .jobSystem = m_jobSystem,
                                                    .frameGraph = m_frameGraph,
                                                    .currentFrameArena = getCurrentFrameArena(),
                                                    .previousFrameArena = getPreviousFrameArena() };

        // Execute deterministic fixed-timestep simulation sub-steps.
        if (timeStats.fixedStep.stepCount > 0u)
        {
            GP_PROFILE_SCOPE_NC_ACTIVE("Application::FixedUpdate", GP_U32_COLOR_YELLOW, profZones);
            GP_PROFILE_ZONE_VALUE(static_cast<gp::UInt64>(timeStats.fixedStep.stepCount));
            for (gp::UInt32 step = 0u; step < timeStats.fixedStep.stepCount; ++step)
            {
                GP_PROFILE_SCOPE_NC_ACTIVE("Application::FixedUpdateStep", GP_U32_COLOR_YELLOW, profZones);
                onFixedUpdate(timeStats.fixedStep.fixedDelta);
            }
        }

        // Execute variable-timestep gameplay & UI update.
        {
            GP_PROFILE_SCOPE_NC_ACTIVE("Application::OnUpdate", GP_U32_COLOR_GREEN, profZones);
            onUpdate(timeStats);
        }

        // Assemble and execute the per-frame simulation/extraction DAG (Main Thread helps as workerIndex == 0).
        {
            GP_PROFILE_SCOPE_NC_ACTIVE("Application::UpdateFrameGraph", GP_U32_COLOR_GREEN, profZones);
            {
                GP_PROFILE_SCOPE_NC_ACTIVE("Application::OnBuildFrameGraph", GP_U32_COLOR_GREEN, profZones);
                onBuildFrameGraph(m_frameGraph, frameContext);
            }
            if (!m_frameGraph.isEmpty() && m_jobSystem.isInitialized())
            {
                GP_PROFILE_SCOPE_NC_ACTIVE("Application::ExecuteFrameGraph", GP_U32_COLOR_GREEN, profZones);
                (void)m_frameGraph.executeAndWait(m_jobSystem);
            }
        }

        // Game-to-Render thread CVar snapshot handoff before rendering extraction / RDG graph passes.
        (void)console.syncRenderThreadValues();

        // Execute rendering and RDG pass graph when visible and not minimized.
        if (!m_isSuspended && (!m_mainWindow.isValid() || !m_mainWindow->isMinimized()))
        {
            GP_PROFILE_SCOPE_NC_ACTIVE("Application::RenderPhase", GP_U32_COLOR_BLUE, profZones);
            m_frameGraph.clear();
            {
                GP_PROFILE_SCOPE_NC_ACTIVE("Application::OnRender", GP_U32_COLOR_BLUE, profZones);
                onRender(timeStats);
            }
            {
                GP_PROFILE_SCOPE_NC_ACTIVE("Application::OnBuildRenderGraph", GP_U32_COLOR_BLUE, profZones);
                onBuildRenderGraph(m_frameGraph, frameContext);
            }
            if (!m_frameGraph.isEmpty() && m_jobSystem.isInitialized())
            {
                GP_PROFILE_SCOPE_NC_ACTIVE("Application::ExecuteRenderGraph", GP_U32_COLOR_BLUE, profZones);
                (void)m_frameGraph.executeAndWait(m_jobSystem);
            }
        }

        // Frame-end notification while both currentFrameArena and previousFrameArena remain valid.
        {
            GP_PROFILE_SCOPE_NC_ACTIVE("Application::OnEndFrame", GP_U32_COLOR_PURPLE, profZones);
            onEndFrame(frameContext);
        }

        if (profPlots && m_jobSystem.isInitialized())
        {
            const thread::JobSystemStats jobStats = m_jobSystem.getStats();
            GP_PLOT_IF(true, "JobSystem::Dispatched", static_cast<gp::Int64>(jobStats.totalJobsDispatched));
            GP_PLOT_IF(true, "JobSystem::Stolen", static_cast<gp::Int64>(jobStats.totalJobsStolen));
            GP_PLOT_IF(true, "JobSystem::SleepingWorkers", static_cast<gp::Int64>(jobStats.sleepingWorkerCount));
        }

        // Adaptive Hybrid Frame Pacing (Foreground vs. Background FPS target).
        {
            GP_PROFILE_SCOPE_NC_ACTIVE("Application::PaceFrame", GP_U32_COLOR_GRAY, profZones);
            const bool isWindowFocused = !m_mainWindow.isValid() || m_mainWindow->isFocused();
            const gp::Float64 targetFps = isWindowFocused ? m_desc.targetForegroundFps : m_desc.targetBackgroundFps;
            m_clock.paceFrame(targetFps);
        }

        GP_FRAME_MARK();
    }

    {
        GP_PROFILE_SCOPE_NC_ACTIVE(
            "Application::OnShutdown", GP_U32_COLOR_RED, profiling::ProfilingControl::isZonesEnabled()
        );
        onShutdown();
    }
    shutdownInternal();
    return 0;
}

void Application::drainEventStream() noexcept
{
    GP_PROFILE_SCOPE_NC_ACTIVE(
        "Application::DrainEventStream", GP_U32_COLOR_CYAN, profiling::ProfilingControl::isZonesEnabled()
    );

    using namespace gp::hal;
    using namespace gp::hal::event;

    while (!m_eventStream.isEmptyApprox())
    {
        EventHeader header{};
        if (!m_eventStream.tryPeekAt(0u, &header, sizeof(EventHeader))) [[unlikely]]
        {
            break;
        }

        const gp::USize totalPacketBytes = sizeof(EventHeader) + header.payloadSize;
        if (!m_eventStream.canRead(totalPacketBytes)) [[unlikely]]
        {
            break;
        }

        switch (header.category)
        {
        case EventCategory::Application:
        {
            ApplicationEvent ev{};
            if (gp::detail::consumeFixedPayload(m_eventStream, ev))
            {
                if (ev.action == ApplicationAction::WillEnterBackground ||
                    ev.action == ApplicationAction::DidEnterBackground)
                {
                    m_isSuspended = true;
                    m_inputSystem->releaseAllInputs();
                }
                else if (
                    ev.action == ApplicationAction::WillEnterForeground ||
                    ev.action == ApplicationAction::DidEnterForeground
                )
                {
                    m_isSuspended = false;
                    m_clock.reset();
                }
                else if (ev.action == ApplicationAction::QuitRequested)
                {
                    requestQuit();
                }

                onApplicationEvent(ev);
            }
            break;
        }

        case EventCategory::Window:
        {
            WindowEvent ev{};
            if (gp::detail::consumeFixedPayload(m_eventStream, ev))
            {
                if (m_mainWindow.isValid() && m_mainWindow->getId() == ev.windowId)
                {
                    m_mainWindow->onWindowEvent(ev);
                }

                // Sanitize held inputs on focus loss, minimization, or hiding to prevent stuck keys.
                if (ev.action == WindowAction::FocusLost || ev.action == WindowAction::Hidden ||
                    (ev.action == WindowAction::StateChanged && ev.state == WindowState::Minimized))
                {
                    m_inputSystem->releaseAllInputs();
                }

                onWindowEvent(ev);

                if (m_mainWindow.isValid() && m_mainWindow->isCloseRequested())
                {
                    requestQuit();
                }
            }
            break;
        }

        case EventCategory::Display:
        {
            DisplayEvent ev{};
            if (gp::detail::consumeFixedPayload(m_eventStream, ev))
            {
                m_backend->notifyDisplayEvent(ev);
                onDisplayEvent(ev);
            }
            break;
        }

        case EventCategory::Keyboard:
        {
            KeyboardEvent ev{};
            if (gp::detail::consumeFixedPayload(m_eventStream, ev))
            {
                m_inputSystem->onKeyboardEvent(ev);
                onKeyboardEvent(ev);
            }
            break;
        }

        case EventCategory::Mouse:
        {
            MouseEvent ev{};
            if (gp::detail::consumeFixedPayload(m_eventStream, ev))
            {
                m_inputSystem->onMouseEvent(ev);
                onMouseEvent(ev);
            }
            break;
        }

        case EventCategory::Gamepad:
        {
            GamepadEvent ev{};
            if (gp::detail::consumeFixedPayload(m_eventStream, ev))
            {
                m_inputSystem->onGamepadEvent(ev);
                onGamepadEvent(ev);
            }
            break;
        }

        case EventCategory::Touch:
        {
            TouchEvent ev{};
            if (gp::detail::consumeFixedPayload(m_eventStream, ev))
            {
                m_inputSystem->onTouchEvent(ev);
                onTouchEvent(ev);
            }
            break;
        }

        case EventCategory::Pen:
        {
            PenEvent ev{};
            if (gp::detail::consumeFixedPayload(m_eventStream, ev))
            {
                m_inputSystem->onPenEvent(ev);
                onPenEvent(ev);
            }
            break;
        }

        case EventCategory::Sensor:
        {
            SensorEvent ev{};
            if (gp::detail::consumeFixedPayload(m_eventStream, ev))
            {
                m_inputSystem->onSensorEvent(ev);
                onSensorEvent(ev);
            }
            break;
        }

        case EventCategory::TextInput:
        {
            TextInputEvent desc{};
            if (header.payloadSize >= sizeof(TextInputEvent) &&
                m_eventStream.tryPeekAt(sizeof(EventHeader), &desc, sizeof(TextInputEvent)))
            {
                char textBuffer[gp::detail::kMaxDecodedStringBytes + 1u]{};
                const gp::UInt16 copyLen = (desc.byteLength <= gp::detail::kMaxDecodedStringBytes)
                                               ? desc.byteLength
                                               : static_cast<gp::UInt16>(gp::detail::kMaxDecodedStringBytes);

                if (copyLen > 0u)
                {
                    (void)m_eventStream.tryPeekAt(sizeof(EventHeader) + sizeof(TextInputEvent), textBuffer, copyLen);
                }
                textBuffer[copyLen] = '\0';
                m_eventStream.advanceConsumer(totalPacketBytes);

                onTextInputEvent(desc, StringView(textBuffer, copyLen));
            }
            else
            {
                m_eventStream.advanceConsumer(totalPacketBytes);
            }
            break;
        }

        case EventCategory::Drop:
        {
            DropEvent desc{};
            if (header.payloadSize >= sizeof(DropEvent) &&
                m_eventStream.tryPeekAt(sizeof(EventHeader), &desc, sizeof(DropEvent)))
            {
                char dropBuffer[gp::detail::kMaxDecodedStringBytes + 1u]{};
                const gp::UInt16 copyLen = (desc.byteLength <= gp::detail::kMaxDecodedStringBytes)
                                               ? desc.byteLength
                                               : static_cast<gp::UInt16>(gp::detail::kMaxDecodedStringBytes);

                if (copyLen > 0u)
                {
                    (void)m_eventStream.tryPeekAt(sizeof(EventHeader) + sizeof(DropEvent), dropBuffer, copyLen);
                }
                dropBuffer[copyLen] = '\0';
                m_eventStream.advanceConsumer(totalPacketBytes);

                onDropEvent(desc, StringView(dropBuffer, copyLen));
            }
            else
            {
                m_eventStream.advanceConsumer(totalPacketBytes);
            }
            break;
        }

        case EventCategory::Clipboard:
        {
            ClipboardEvent ev{};
            if (header.payloadSize >= sizeof(ClipboardEvent) &&
                m_eventStream.tryPeekAt(sizeof(EventHeader), &ev, sizeof(ClipboardEvent)))
            {
                m_eventStream.advanceConsumer(totalPacketBytes);
                m_backend->getClipboard().onClipboardEvent(ev);
                onClipboardEvent(ev);
            }
            else
            {
                m_eventStream.advanceConsumer(totalPacketBytes);
            }
            break;
        }

        case EventCategory::DeviceChange:
        {
            DeviceChangeEvent ev{};
            if (gp::detail::consumeFixedPayload(m_eventStream, ev))
            {
                m_inputSystem->onDeviceChangeEvent(ev);
                onDeviceChangeEvent(ev);
            }
            break;
        }

        default:
            m_eventStream.advanceConsumer(totalPacketBytes);
            break;
        }
    }
}

}   // namespace gp
