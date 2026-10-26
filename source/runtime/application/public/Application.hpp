// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "ApplicationDesc.hpp"
#include "backend/IApplicationBackend.hpp"
#include "containers/queues/SPSCRingBuffer.hpp"   // IWYU pragma: keep
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "display/Display.hpp"   // IWYU pragma: keep
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
#include "memory/pointers/UniquePtr.hpp"
#include "threading/Atomic.hpp"
#include "time/EngineClock.hpp"
#include "window/IWindow.hpp"
#include "window/WindowDesc.hpp"   // IWYU pragma: keep

namespace gp
{

/// @brief Backend-agnostic engine application host orchestrating HAL interfaces, time, input, and event routing.
/// @details Completely decoupled from concrete platform libraries (such as SDL3). Concrete backends are injected
///          at construction time, falling back to `gp::hal::generic::ApplicationBackend` if omitted.
class Application
{
private:
    ApplicationDesc m_desc{};
    gp::UniquePtr<hal::IApplicationBackend> m_backend{};
    gp::UniquePtr<hal::input::IInputSystem> m_inputSystem{};
    gp::UniquePtr<hal::IWindow> m_mainWindow{};
    gp::SPSCByteRingBuffer m_eventStream{};
    gp::time::EngineClock m_clock{};
    hal::EventPumpStats m_lastPumpStats{};

    gp::Atomic<bool> m_isRunning{ false };
    bool m_isInitialized{ false };
    bool m_isSuspended{ false };

public:
    /// @brief Constructs the application with injected HAL backend and optional custom input system.
    /// @param[in] desc Configuration parameters for the application, window, and clock.
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
    /// @brief Initializes all HAL subsystems, opens the main window, and executes the main engine loop.
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

    /// @brief Retrieves the telemetry counters from the most recent `pumpEvents()` call.
    /// @return Const reference to `hal::EventPumpStats`.
    [[nodiscard]] constexpr const hal::EventPumpStats& getLastEventPumpStats() const noexcept
    {
        return m_lastPumpStats;
    }

protected:
    /// @brief Invoked once after the HAL backend, displays, input system, and main window are initialized.
    /// @return True to proceed into the main loop, or false to abort startup and shut down immediately.
    [[nodiscard]] virtual bool onInitialize() noexcept
    {
        return true;
    }

    /// @brief Invoked zero, one, or more times per frame at the deterministic fixed physics timestep (ADR-0033).
    /// @param[in] fixedDelta Exact fixed timestep duration for the discrete simulation step.
    virtual void onFixedUpdate([[maybe_unused]] gp::time::Duration fixedDelta) noexcept
    {}

    /// @brief Invoked once per frame after all lock-free OS events and fixed-step updates have been processed.
    /// @param[in] timeStats Frame timing metrics including scaled/unscaled deltas and render interpolation alpha.
    virtual void onUpdate([[maybe_unused]] const gp::time::FrameTimeStats& timeStats) noexcept
    {}

    /// @brief Invoked once per frame after `onUpdate()` when the application is visible and not minimized.
    /// @param[in] timeStats Frame timing metrics including `fixedStep.interpolationAlpha`.
    virtual void onRender([[maybe_unused]] const gp::time::FrameTimeStats& timeStats) noexcept
    {}

    /// @brief Invoked once when the main loop exits, prior to closing the main window and shutting down HAL backends.
    virtual void onShutdown() noexcept
    {}

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
    /// @brief Allocates and initializes owned HAL resources prior to entering the main loop.
    /// @return True if initialization succeeded, false otherwise.
    [[nodiscard]] bool initializeInternal() noexcept;

    /// @brief Shuts down and releases owned HAL resources in reverse initialization order.
    void shutdownInternal() noexcept;

    /// @brief Drains all queued TLV packets from `m_eventStream` and dispatches them to subsystems and hooks.
    void drainEventStream() noexcept;
};

}   // namespace gp
