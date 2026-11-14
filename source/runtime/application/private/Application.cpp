// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "Application.hpp"
#include "backend/GenericApplicationBackend.hpp"
#include "input/GenericInputSystem.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "templates/Optional.hpp"

namespace gp
{

namespace detail
{

/// @brief Maximum stack buffer size (in bytes) for decoding variable-length UTF-8 TLV payloads.
inline constexpr gp::USize kMaxDecodedStringBytes = 2048u;

/// @brief Minimum byte capacity enforced per frame arena so `m_frameGraph` buffers always fit comfortably.
inline constexpr gp::USize kMinFrameArenaBytes = 64ULL * 1024ULL;

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

    // Allocate the lock-free SPSC byte ring buffer.
    if (!m_eventStream.initialize(m_desc.eventRingBufferBytes)) [[unlikely]]
    {
        return false;
    }

    // Fall back to the dependency-free GenericApplicationBackend when no concrete backend was injected.
    if (!m_backend.isValid())
    {
        m_backend = gp::makeUnique<hal::generic::ApplicationBackend>();
        if (!m_backend.isValid()) [[unlikely]]
        {
            m_eventStream.shutdown();
            return false;
        }
    }

    // Initialize the backend and enumerate display topology.
    if (m_backend->initialize(m_desc.backend) != hal::BackendResult::Success) [[unlikely]]
    {
        m_eventStream.shutdown();
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
            m_backend->shutdown();
            m_eventStream.shutdown();
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
            m_inputSystem.reset();
            m_backend->shutdown();
            m_eventStream.shutdown();
            return false;
        }
    }

    applyCommandLineOverrides();

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
        return false;
    }

    m_activeFrameArenaIndex = 0u;
    if (!rebindFrameGraphToCurrentArena()) [[unlikely]]
    {
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
        return false;
    }

    if (m_desc.enableJobSystem)
    {
        if (m_jobSystem.initialize(m_desc.jobSystem) != thread::JobSystemResult::Success) [[unlikely]]
        {
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
            return false;
        }
    }

    m_clock.reset();
    m_isInitialized = true;
    return true;
}

void Application::shutdownInternal() noexcept
{
    if (!m_isInitialized)
    {
        return;
    }

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
    m_isInitialized = false;
}

gp::Int32 Application::run() noexcept
{
    if (!initializeInternal()) [[unlikely]]
    {
        return -1;
    }

    if (!onInitialize()) [[unlikely]]
    {
        shutdownInternal();
        return -2;
    }

    m_isRunning.store(true, gp::MemoryOrder::Release);
    m_clock.reset();

    while (m_isRunning.load(gp::MemoryOrder::Acquire))
    {
        // OS Power-Saving Wait when minimized or background-suspended.
        const bool windowMinimized = m_mainWindow.isValid() && m_mainWindow->isMinimized();
        if (m_isSuspended || (m_desc.sleepWhenMinimized && windowMinimized))
        {
            (void)m_backend->waitEventsTimeout(100u);
        }

        // Clear previous frame's transient input edge latches and motion deltas.
        m_inputSystem->beginFrame();

        // Producer Phase: Poll OS message queue and pack TLV events into the SPSC ring buffer.
        (void)m_backend->pumpEvents(m_eventStream, &m_lastPumpStats);

        // Consumer Phase: Drain TLV packets, update Window/Display/Input shadow states, and invoke hooks.
        drainEventStream();

        if (!m_isRunning.load(gp::MemoryOrder::Acquire))
        {
            break;
        }

        // Advance hierarchical engine clocks and drain fixed-timestep accumulator.
        const gp::time::FrameTimeStats& timeStats = m_clock.tick();

        // Rotate the double-buffered frame arena (rewinding Frame N while keeping Frame N-1 intact) and
        // rebind m_frameGraph over the active frame arena without heap allocations.
        if (!rotateAndResetFrameArena(timeStats.frameIndex)) [[unlikely]]
        {
            break;
        }

        const ApplicationFrameContext frameContext{ .timeStats = timeStats,
                                                    .jobSystem = m_jobSystem,
                                                    .frameGraph = m_frameGraph,
                                                    .currentFrameArena = getCurrentFrameArena(),
                                                    .previousFrameArena = getPreviousFrameArena() };

        // Execute deterministic fixed-timestep simulation sub-steps.
        for (gp::UInt32 step = 0u; step < timeStats.fixedStep.stepCount; ++step)
        {
            onFixedUpdate(timeStats.fixedStep.fixedDelta);
        }

        // Execute variable-timestep gameplay & UI update.
        onUpdate(timeStats);

        // Assemble and execute the per-frame simulation/extraction DAG (Main Thread helps as workerIndex == 0).
        onBuildFrameGraph(m_frameGraph, frameContext);
        if (!m_frameGraph.isEmpty() && m_jobSystem.isInitialized())
        {
            (void)m_frameGraph.executeAndWait(m_jobSystem);
        }

        // Execute rendering and RDG pass graph when visible and not minimized.
        if (!m_isSuspended && (!m_mainWindow.isValid() || !m_mainWindow->isMinimized()))
        {
            m_frameGraph.clear();
            onRender(timeStats);
            onBuildRenderGraph(m_frameGraph, frameContext);
            if (!m_frameGraph.isEmpty() && m_jobSystem.isInitialized())
            {
                (void)m_frameGraph.executeAndWait(m_jobSystem);
            }
        }

        // Frame-end notification while both currentFrameArena and previousFrameArena remain valid.
        onEndFrame(frameContext);

        // Adaptive Hybrid Frame Pacing (Foreground vs. Background FPS target).
        const bool isWindowFocused = !m_mainWindow.isValid() || m_mainWindow->isFocused();
        const gp::Float64 targetFps = isWindowFocused ? m_desc.targetForegroundFps : m_desc.targetBackgroundFps;
        m_clock.paceFrame(targetFps);
    }

    onShutdown();
    shutdownInternal();
    return 0;
}

void Application::drainEventStream() noexcept
{
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
