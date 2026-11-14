// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "ApplicationDesc.hpp"
#include "ApplicationForward.hpp"
#include "ApplicationTypes.hpp"
#include "backend/IApplicationBackend.hpp"
#include "containers/queues/SPSCRingBuffer.hpp"   // IWYU pragma: keep
#include "containers/views/StringView.hpp"        // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "display/Display.hpp"                    // IWYU pragma: keep
#include "event/data/ClipboardEvent.hpp"
#include "event/data/DropEvent.hpp"
#include "event/data/TextInputEvent.hpp"
#include "event/device/DeviceChangeEvent.hpp"
#include "event/EventCategory.hpp"   // IWYU pragma: keep
#include "event/EventHeader.hpp"     // IWYU pragma: keep
#include "event/input/GamepadEvent.hpp"
#include "event/input/KeyboardEvent.hpp"
#include "event/input/MouseEvent.hpp"
#include "event/input/PenEvent.hpp"
#include "event/input/SensorEvent.hpp"
#include "event/input/TouchEvent.hpp"
#include "event/system/ApplicationEvent.hpp"
#include "event/system/DisplayEvent.hpp"
#include "event/system/WindowEvent.hpp"
#include "input/IInputSystem.hpp"
#include "memory/allocators/LinearAllocator.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "threading/Atomic.hpp"
#include "threading/jobs/JobGraph.hpp"
#include "threading/jobs/JobSystem.hpp"
#include "time/EngineClock.hpp"
#include "window/IWindow.hpp"
#include "window/WindowDesc.hpp"   // IWYU pragma: keep

#if GP_IS_MONOLITHIC
    #define GP_APPLICATION_API
#else
    #if defined(GP_APPLICATION_EXPORTS)
        #define GP_APPLICATION_API GP_DLLEXPORT
    #else
        #define GP_APPLICATION_API GP_DLLIMPORT
    #endif
#endif

namespace gp
{

/// @brief Backend-agnostic engine application host orchestrating HAL interfaces, time, input, event routing,
///        double-buffered frame arenas, and the $M:N$ cooperative fiber `JobSystem` ([ADR-0012], [ADR-0053]).
/// @details Decoupled from concrete platform libraries (such as SDL3). Concrete backends are injected at construction
///          time, falling back to `gp::hal::generic::ApplicationBackend` if omitted. Owns the engine's primary
///          `gp::thread::JobSystem`, two rotating per-frame `gp::memory::LinearAllocator` arenas (`Frame N` and
///          `Frame N-1`), and a zero-allocation per-frame `gp::thread::JobGraph`.
class GP_APPLICATION_API Application
{
private:
    ApplicationDesc m_desc{};
    gp::UniquePtr<hal::IApplicationBackend> m_backend{};
    gp::UniquePtr<hal::input::IInputSystem> m_inputSystem{};
    gp::UniquePtr<hal::IWindow> m_mainWindow{};
    gp::SPSCByteRingBuffer m_eventStream{};
    gp::time::EngineClock m_clock{};
    gp::thread::JobSystem m_jobSystem{};
    gp::memory::LinearAllocator m_frameArenas[kFrameArenaBufferCount]{};
    gp::thread::JobGraph m_frameGraph{};
    hal::EventPumpStats m_lastPumpStats{};

    gp::UInt32 m_activeFrameArenaIndex{ 0u };
    gp::Atomic<bool> m_isRunning{ false };
    bool m_isInitialized{ false };
    bool m_isSuspended{ false };

public:
    /// @brief Constructs the application with injected HAL backend and optional custom input system.
    /// @param[in] desc Configuration parameters for the application, window, clock, job system, and frame arenas.
    /// @param[in] backend Injected `IApplicationBackend` (falls back to `generic::ApplicationBackend` if null).
    /// @param[in] inputSystem Optional custom `IInputSystem` (created via `backend->createInputSystem()` if null).
    explicit Application(
        const ApplicationDesc& desc = ApplicationDesc{},
        gp::UniquePtr<hal::IApplicationBackend> backend = nullptr,
        gp::UniquePtr<hal::input::IInputSystem> inputSystem = nullptr
    ) noexcept;

    /// @brief Virtual destructor ensuring clean shutdown of all owned subsystems.
    virtual ~Application() noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

public:
    /// @section Lifecycle & Execution Control

    /// @brief Initializes all HAL subsystems, frame arenas, and the `JobSystem`, opens the main window, and executes
    ///        the main engine loop.
    /// @return Process exit code (`0` on clean exit, negative value on initialization failure).
    [[nodiscard]] gp::Int32 run() noexcept;

    /// @brief Signals the application loop to exit cleanly at the end of the current frame.
    void requestQuit() noexcept;

    /// @brief Checks whether the main application loop is currently active.
    /// @return True if running, false if stopped or quitting.
    [[nodiscard]] bool isRunning() const noexcept
    {
        return m_isRunning.load(gp::MemoryOrder::Acquire);
    }

    /// @brief Checks whether the application is currently backgrounded/suspended by the OS.
    /// @return True if suspended, false otherwise.
    [[nodiscard]] constexpr bool isSuspended() const noexcept
    {
        return m_isSuspended;
    }

public:
    /// @section Subsystem & Arena Accessors

    /// @brief Retrieves a pointer to the active HAL application backend.
    /// @return Pointer to `IApplicationBackend`.
    [[nodiscard]] constexpr hal::IApplicationBackend* getBackend() const noexcept
    {
        return m_backend.get();
    }

    /// @brief Retrieves a mutable reference to the active input system coordinator.
    /// @return Reference to `IInputSystem`.
    [[nodiscard]] constexpr hal::input::IInputSystem& getInput() noexcept
    {
        return *m_inputSystem;
    }

    /// @brief Retrieves a const reference to the active input system coordinator.
    /// @return Const reference to `IInputSystem`.
    [[nodiscard]] constexpr const hal::input::IInputSystem& getInput() const noexcept
    {
        return *m_inputSystem;
    }

    /// @brief Retrieves a pointer to the primary application window.
    /// @return Pointer to `IWindow`, or `nullptr` if running headless without a window.
    [[nodiscard]] constexpr hal::IWindow* getMainWindow() const noexcept
    {
        return m_mainWindow.get();
    }

    /// @brief Retrieves a mutable reference to the master engine clock.
    /// @return Reference to `gp::time::EngineClock`.
    [[nodiscard]] constexpr gp::time::EngineClock& getClock() noexcept
    {
        return m_clock;
    }

    /// @brief Retrieves a const reference to the master engine clock.
    /// @return Const reference to `gp::time::EngineClock`.
    [[nodiscard]] constexpr const gp::time::EngineClock& getClock() const noexcept
    {
        return m_clock;
    }

    /// @brief Retrieves a mutable reference to the application's cooperative fiber `JobSystem` ([ADR-0052],
    ///        [ADR-0053]).
    /// @return Reference to `gp::thread::JobSystem`.
    [[nodiscard]] constexpr gp::thread::JobSystem& getJobSystem() noexcept
    {
        return m_jobSystem;
    }

    /// @brief Retrieves a const reference to the application's cooperative fiber `JobSystem`.
    /// @return Const reference to `gp::thread::JobSystem`.
    [[nodiscard]] constexpr const gp::thread::JobSystem& getJobSystem() const noexcept
    {
        return m_jobSystem;
    }

    /// @brief Retrieves a mutable reference to the current frame's `LinearAllocator` (`Frame N` arena).
    /// @details Reset in $O(1)$ time at the start of Frame `N`; allocations remain valid until the start of Frame
    ///          `N + 2`.
    /// @return Reference to the active frame's `gp::memory::LinearAllocator`.
    [[nodiscard]] constexpr gp::memory::LinearAllocator& getCurrentFrameArena() noexcept
    {
        return m_frameArenas[m_activeFrameArenaIndex];
    }

    /// @brief Retrieves a const reference to the current frame's `LinearAllocator` (`Frame N` arena).
    /// @return Const reference to the active frame's `gp::memory::LinearAllocator`.
    [[nodiscard]] constexpr const gp::memory::LinearAllocator& getCurrentFrameArena() const noexcept
    {
        return m_frameArenas[m_activeFrameArenaIndex];
    }

    /// @brief Retrieves a mutable reference to the previous frame's `LinearAllocator` (`Frame N-1` arena).
    /// @details Contains intact transient allocations from Frame `N - 1` throughout the entirety of Frame `N`.
    /// @return Reference to the previous frame's `gp::memory::LinearAllocator`.
    [[nodiscard]] constexpr gp::memory::LinearAllocator& getPreviousFrameArena() noexcept
    {
        return m_frameArenas[1u - m_activeFrameArenaIndex];
    }

    /// @brief Retrieves a const reference to the previous frame's `LinearAllocator` (`Frame N-1` arena).
    /// @return Const reference to the previous frame's `gp::memory::LinearAllocator`.
    [[nodiscard]] constexpr const gp::memory::LinearAllocator& getPreviousFrameArena() const noexcept
    {
        return m_frameArenas[1u - m_activeFrameArenaIndex];
    }

    /// @brief Retrieves a mutable reference to the per-frame `JobGraph` backed by `getCurrentFrameArena()`.
    /// @return Reference to `gp::thread::JobGraph`.
    [[nodiscard]] constexpr gp::thread::JobGraph& getFrameGraph() noexcept
    {
        return m_frameGraph;
    }

    /// @brief Retrieves a const reference to the per-frame `JobGraph` backed by `getCurrentFrameArena()`.
    /// @return Const reference to `gp::thread::JobGraph`.
    [[nodiscard]] constexpr const gp::thread::JobGraph& getFrameGraph() const noexcept
    {
        return m_frameGraph;
    }

    /// @brief Queries the active frame arena buffer slot index (`0u` or `1u`).
    /// @return Active double-buffer index in `[0, kFrameArenaBufferCount - 1]`.
    [[nodiscard]] constexpr gp::UInt32 getActiveFrameArenaIndex() const noexcept
    {
        return m_activeFrameArenaIndex;
    }

    /// @brief Retrieves the telemetry counters from the most recent `pumpEvents()` call.
    /// @return Const reference to `hal::EventPumpStats`.
    [[nodiscard]] constexpr const hal::EventPumpStats& getLastEventPumpStats() const noexcept
    {
        return m_lastPumpStats;
    }

protected:
    /// @section Frame Lifecycle & Task Graph Specialization Hooks

    /// @brief Invoked once after the HAL backend, displays, input system, main window, frame arenas, and `JobSystem`
    ///        are initialized.
    /// @return True to proceed into the main loop, or false to abort startup and shut down immediately.
    [[nodiscard]] virtual bool onInitialize() noexcept
    {
        return true;
    }

    /// @brief Invoked zero, one, or more times per frame at the deterministic fixed physics timestep ([ADR-0033]).
    /// @param[in] fixedDelta Exact fixed timestep duration for the discrete simulation step.
    virtual void onFixedUpdate([[maybe_unused]] gp::time::Duration fixedDelta) noexcept
    {}

    /// @brief Invoked once per frame after all lock-free OS events and fixed-step updates have been processed.
    /// @param[in] timeStats Frame timing metrics including scaled/unscaled deltas and render interpolation alpha.
    virtual void onUpdate([[maybe_unused]] const gp::time::FrameTimeStats& timeStats) noexcept
    {}

    /// @brief Invoked once per frame immediately after `onUpdate()` to populate the frame simulation/extraction DAG
    ///        ([ADR-0053]).
    /// @details Any nodes registered in `frameGraph` are validated and executed to completion via
    ///          `frameGraph.executeAndWait(m_jobSystem)` before the render phase begins.
    /// @param[in,out] frameGraph Pre-allocated, empty `JobGraph` backed by `frameContext.currentFrameArena`.
    /// @param[in] frameContext Active frame context providing `timeStats`, `jobSystem`, and double-buffered arenas.
    virtual void onBuildFrameGraph(
        [[maybe_unused]] gp::thread::JobGraph& frameGraph, [[maybe_unused]] const ApplicationFrameContext& frameContext
    ) noexcept
    {}

    /// @brief Invoked once per frame after the update phase when the application is visible and not minimized.
    /// @param[in] timeStats Frame timing metrics including `fixedStep.interpolationAlpha`.
    virtual void onRender([[maybe_unused]] const gp::time::FrameTimeStats& timeStats) noexcept
    {}

    /// @brief Invoked once per frame immediately after `onRender()` (when visible) to populate the render / Render
    ///        Dependency Graph (RDG) pass DAG ([ADR-0053]).
    /// @details `renderGraph` is cleared over `frameContext.currentFrameArena` prior to this hook and executed via
    ///          `renderGraph.executeAndWait(m_jobSystem)` before `onEndFrame()`.
    /// @param[in,out] renderGraph Pre-allocated, empty `JobGraph` backed by `frameContext.currentFrameArena`.
    /// @param[in] frameContext Active frame context providing `timeStats`, `jobSystem`, and double-buffered arenas.
    virtual void onBuildRenderGraph(
        [[maybe_unused]] gp::thread::JobGraph& renderGraph, [[maybe_unused]] const ApplicationFrameContext& frameContext
    ) noexcept
    {}

    /// @brief Invoked at the end of every frame after update and render task graphs have completed, before frame
    ///        pacing and arena rotation ([ADR-0053]).
    /// @param[in] frameContext Active frame context while both `currentFrameArena` and `previousFrameArena` remain
    ///                         valid.
    virtual void onEndFrame([[maybe_unused]] const ApplicationFrameContext& frameContext) noexcept
    {}

    /// @brief Invoked once when the main loop exits, while `m_jobSystem` and `m_frameArenas` are still active and
    ///        prior to closing the main window and shutting down HAL backends.
    virtual void onShutdown() noexcept
    {}

protected:
    /// @section Event Routing Specialization Hooks

    /// @brief Invoked when an OS process lifecycle event (`QuitRequested`, `LowMemoryWarning`, etc.) is drained.
    /// @param[in] ev The decoded application event packet.
    virtual void onApplicationEvent([[maybe_unused]] const hal::event::ApplicationEvent& ev) noexcept
    {}

    /// @brief Invoked when a window event is drained (after `m_mainWindow->onWindowEvent(ev)` updates shadow state).
    /// @param[in] ev The decoded window event packet.
    virtual void onWindowEvent([[maybe_unused]] const hal::event::WindowEvent& ev) noexcept
    {}

    /// @brief Invoked when a hardware monitor topology event is drained (after backend display tables update).
    /// @param[in] ev The decoded display event packet.
    virtual void onDisplayEvent([[maybe_unused]] const hal::event::DisplayEvent& ev) noexcept
    {}

    /// @brief Invoked when a physical keyboard key state changes (after `m_inputSystem` updates state tables).
    /// @param[in] ev The decoded keyboard event packet.
    virtual void onKeyboardEvent([[maybe_unused]] const hal::event::KeyboardEvent& ev) noexcept
    {}

    /// @brief Invoked when a mouse motion, button, or scroll wheel event is drained.
    /// @param[in] ev The decoded mouse event packet.
    virtual void onMouseEvent([[maybe_unused]] const hal::event::MouseEvent& ev) noexcept
    {}

    /// @brief Invoked when a gamepad button, analog axis, hat, or touchpad event is drained.
    /// @param[in] ev The decoded gamepad event packet.
    virtual void onGamepadEvent([[maybe_unused]] const hal::event::GamepadEvent& ev) noexcept
    {}

    /// @brief Invoked when a multi-touch finger contact event is drained.
    /// @param[in] ev The decoded touch event packet.
    virtual void onTouchEvent([[maybe_unused]] const hal::event::TouchEvent& ev) noexcept
    {}

    /// @brief Invoked when a stylus / pen tablet event is drained.
    /// @param[in] ev The decoded pen event packet.
    virtual void onPenEvent([[maybe_unused]] const hal::event::PenEvent& ev) noexcept
    {}

    /// @brief Invoked when an IMU gyroscope or accelerometer reading is drained.
    /// @param[in] ev The decoded sensor event packet.
    virtual void onSensorEvent([[maybe_unused]] const hal::event::SensorEvent& ev) noexcept
    {}

    /// @brief Invoked when committed UTF-8 text or IME composition text is drained from the TLV stream.
    /// @param[in] ev The decoded text input descriptor.
    /// @param[in] utf8Text View over the trailing UTF-8 characters decoded from the ring buffer.
    virtual void onTextInputEvent(
        [[maybe_unused]] const hal::event::TextInputEvent& ev, [[maybe_unused]] StringView utf8Text
    ) noexcept
    {}

    /// @brief Invoked when an OS drag-and-drop sequence or dropped file path is drained from the TLV stream.
    /// @param[in] ev The decoded drop event descriptor.
    /// @param[in] droppedData View over the trailing UTF-8 file path or text string.
    virtual void
        onDropEvent([[maybe_unused]] const hal::event::DropEvent& ev, [[maybe_unused]] StringView droppedData) noexcept
    {}

    /// @brief Invoked when the OS clipboard content changes.
    /// @param[in] ev The decoded clipboard event packet.
    virtual void onClipboardEvent([[maybe_unused]] const hal::event::ClipboardEvent& ev) noexcept
    {}

    /// @brief Invoked when an input, audio, or camera peripheral is connected or disconnected.
    /// @param[in] ev The decoded device change event packet.
    virtual void onDeviceChangeEvent([[maybe_unused]] const hal::event::DeviceChangeEvent& ev) noexcept
    {}

private:
    /// @brief Applies command-line concurrency overrides (`-threads`, `-workers`, `-no-jobsystem`, `-core-affinity`)
    ///        from `gp::CommandLine` to `m_desc` prior to initializing `m_jobSystem`.
    void applyCommandLineOverrides() noexcept;

    /// @brief Rotates `m_activeFrameArenaIndex` to `frameIndex & 1u`, resets the new current frame arena in $O(1)$
    ///        time, and rebinds `m_frameGraph` over the reset arena.
    /// @param[in] frameIndex Monotonic 1-based frame index from `EngineClock::tick()`.
    /// @return True if `m_frameGraph` was rebound over the current frame arena, false if arena capacity was too small.
    [[nodiscard]] bool rotateAndResetFrameArena(gp::UInt64 frameIndex) noexcept;

    /// @brief Carves `JobGraphNode` and edge pointer buffers from `getCurrentFrameArena()` and binds `m_frameGraph`
    ///        via `JobGraph::initializeWithBuffers()`.
    /// @return True if the arena had sufficient space for `maxFrameGraphNodes` and `m_frameGraph` is valid.
    [[nodiscard]] bool rebindFrameGraphToCurrentArena() noexcept;

    /// @brief Allocates and initializes owned HAL resources, frame arenas, and `m_jobSystem` prior to entering the
    ///        main loop.
    /// @return True if initialization succeeded, false otherwise.
    [[nodiscard]] bool initializeInternal() noexcept;

    /// @brief Shuts down and releases `m_jobSystem`, frame arenas, and owned HAL resources in reverse initialization
    ///        order.
    void shutdownInternal() noexcept;

    /// @brief Drains all queued TLV packets from `m_eventStream` and dispatches them to subsystems and hooks.
    void drainEventStream() noexcept;
};

}   // namespace gp
