// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "backend/ApplicationBackendDesc.hpp"
#include "backend/ApplicationBackendEnums.hpp"
#include "backend/EventPumpStats.hpp"
#include "clipboard/IClipboard.hpp"
#include "containers/queues/SPSCRingBuffer.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "dialog/IDialog.hpp"
#include "display/Display.hpp"
#include "event/EventCategory.hpp"   // IWYU pragma: keep
#include "event/EventHeader.hpp"     // IWYU pragma: keep
#include "HALBase.hpp"
#include "input/IInputSystem.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "window/IWindow.hpp"

namespace gp::hal
{

/// @brief Maximum number of physical monitors tracked simultaneously in the fixed topology table.
inline constexpr gp::UInt32 kMaxDisplays = 16u;

/// @brief Interface for application backends that manage the initialization and shutdown of platform-specific systems
/// (e.g., SDL, GLFW, Win32) and provide resources creation functionality.
class GP_HAL_BASE_API IApplicationBackend
{
public:
    /// @brief Default constructor.
    [[nodiscard]] constexpr IApplicationBackend() noexcept = default;

    /// @brief Virtual destructor.
    virtual ~IApplicationBackend() noexcept = default;

    /// @brief Non-copyable and non-movable interface contract.
    IApplicationBackend(const IApplicationBackend&) = delete;
    IApplicationBackend& operator=(const IApplicationBackend&) = delete;
    IApplicationBackend(IApplicationBackend&&) = delete;
    IApplicationBackend& operator=(IApplicationBackend&&) = delete;

public:
    /// @brief Initializes the underlying platform abstraction library and configured hardware subsystems.
    /// @param[in] desc Configuration parameters and metadata for the platform backend.
    /// @return `BackendResult::Success` on success, or an error code detailing the failure reason.
    [[nodiscard]] virtual BackendResult initialize(const ApplicationBackendDesc& desc) noexcept = 0;

    /// @brief Shuts down all initialized hardware subsystems and releases OS handles.
    virtual void shutdown() noexcept = 0;

    /// @brief Checks whether the backend is currently initialized and ready to pump events.
    /// @return `true` if initialized, `false` otherwise.
    [[nodiscard]] virtual bool isInitialized() const noexcept = 0;

    /// @brief Drains the OS message queue, translates messages into engine PODs, and writes TLV packets
    ///        into the lock-free byte stream.
    /// @note Must be called from the thread that initialized the video/windowing subsystem (the OS Thread).
    /// @param[in,out] eventStream Target lock-free SPSC byte ring buffer shared with the Game Thread.
    /// @param[out] outStats Optional pointer to receive telemetry counters for the pump invocation.
    /// @return `BackendResult::Success` if all critical events were queued, `BackendResult::BufferOverflow`
    ///         if any event was dropped due to ring buffer exhaustion, or `BackendResult::NotInitialized`.
    virtual BackendResult
        pumpEvents(gp::SPSCByteRingBuffer& eventStream, EventPumpStats* outStats = nullptr) noexcept = 0;

    /// @brief Blocks the calling OS thread until at least one OS event arrives or the timeout expires.
    /// @details Used when the application is minimized or backgrounded to reduce OS thread CPU usage to 0%.
    /// @param[in] timeoutMs Maximum duration to wait in milliseconds (0 = non-blocking check).
    /// @return `true` if an OS event is pending in the platform queue, `false` if the timeout expired.
    [[nodiscard]] virtual bool waitEventsTimeout(gp::UInt32 timeoutMs) noexcept = 0;

    /// @brief Pushes a synthetic no-op wake signal into the OS event queue from any thread.
    /// @details Immediately unblocks an OS thread sleeping inside `waitEventsTimeout()`.
    virtual void wakeEventLoop() noexcept = 0;

    /// @brief Returns the static human-readable identifier of the active backend (e.g., "SDL3").
    /// @return Null-terminated ASCII backend name string.
    [[nodiscard]] virtual gp::StringView getBackendName() const noexcept = 0;

    /// @brief Creates a platform-specific input system instance matching this backend.
    /// @return `UniquePtr<input::IInputSystem>` owning the created input coordinator, or `nullptr` on failure.
    [[nodiscard]] virtual gp::UniquePtr<input::IInputSystem> createInputSystem() noexcept = 0;

    /// @brief Creates a new platform-specific window instance managed via the `IWindow` interface.
    /// @param[in] desc Configuration descriptor for the window.
    /// @return `UniquePtr<IWindow>` owning the newly created window, or `nullptr` on failure.
    [[nodiscard]] virtual gp::UniquePtr<IWindow> createWindow(const WindowDesc& desc) noexcept = 0;

    /// @brief Queries the number of active displays currently cached by the backend.
    /// @return Number of connected displays (always >= 1 when initialized due to virtual headless fallback).
    [[nodiscard]] virtual gp::UInt32 getDisplayCount() const noexcept = 0;

    /// @brief Retrieves a pointer to a cached display by its index in `[0, getDisplayCount())`.
    /// @param[in] index Zero-based display index.
    /// @return Pointer to the `IDisplay`, or `nullptr` if out of range.
    [[nodiscard]] virtual const IDisplay* getDisplay(gp::UInt32 index) const noexcept = 0;

    /// @brief Finds a cached display by its unique OS `DisplayId`.
    /// @param[in] displayId Target display identifier.
    /// @return Pointer to the matching `IDisplay`, or `nullptr` if not found.
    [[nodiscard]] virtual const IDisplay* findDisplayById(DisplayId displayId) const noexcept = 0;

    /// @brief Retrieves the primary desktop display designated by the OS (or the virtual fallback if headless).
    /// @return Pointer to the primary `IDisplay` (never `nullptr` while initialized).
    [[nodiscard]] virtual const IDisplay* getPrimaryDisplay() const noexcept = 0;

    /// @brief Finds the display containing the specified virtual desktop point, or the nearest display if off-screen.
    /// @param[in] point Screen-space coordinate `(x, y)`.
    /// @return Pointer to the containing or nearest `IDisplay`.
    [[nodiscard]] virtual const IDisplay* getDisplayForPoint(const Point2D& point) const noexcept = 0;

    /// @brief Finds the display that shares the largest overlapping pixel area with the specified rectangle.
    /// @details Used to determine which monitor a window belongs to when spanning multiple monitors.
    /// @param[in] rect Virtual desktop rectangle `(origin, extent)`.
    /// @return Pointer to the `IDisplay` with maximum overlap, or nearest display if completely off-screen.
    [[nodiscard]] virtual const IDisplay* getDisplayForRect(const DisplayRect& rect) const noexcept = 0;

    /// @brief Synchronizes the backend's cached display topology table with a decoded `DisplayEvent`.
    /// @note Invoked by `Application::drainEventStream()` on the Game Thread.
    /// @param[in] ev The decoded display event packet.
    virtual void notifyDisplayEvent(const event::DisplayEvent& ev) noexcept = 0;

    /// @brief Retrieves the native dialog service for modal message boxes and async file pickers.
    /// @return Reference to `IDialog`.
    [[nodiscard]] virtual IDialog& getDialog() noexcept = 0;

    /// @brief Retrieves a const reference to the native dialog service.
    /// @return Const reference to `IDialog`.
    [[nodiscard]] virtual const IDialog& getDialog() const noexcept = 0;

    /// @brief Retrieves the OS clipboard service for UTF-8 text, primary selections, and MIME payloads.
    /// @return Reference to `IClipboard`.
    [[nodiscard]] virtual IClipboard& getClipboard() noexcept = 0;

    /// @brief Retrieves a const reference to the OS clipboard service.
    /// @return Const reference to `IClipboard`.
    [[nodiscard]] virtual const IClipboard& getClipboard() const noexcept = 0;
};

}   // namespace gp::hal
