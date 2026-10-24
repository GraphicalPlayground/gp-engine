// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "backend/ApplicationBackendDesc.hpp"
#include "backend/ApplicationBackendEnums.hpp"
#include "backend/EventPumpStats.hpp"
#include "containers/queues/SPSCRingBuffer.hpp"
#include "CoreMinimal.hpp"           // IWYU pragma: keep
#include "event/EventCategory.hpp"   // IWYU pragma: keep
#include "event/EventHeader.hpp"     // IWYU pragma: keep
#include "memory/pointers/UniquePtr.hpp"
#include "window/IWindow.hpp"

namespace gp::hal
{

/// @brief Interface for application backends that manage the initialization and shutdown of platform-specific systems
/// (e.g., SDL, GLFW, Win32) and provide resources creation functionality.
class IApplicationBackend
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

    /// @brief Creates a new native window with the specified description.
    /// @param desc The description of the window to create.
    /// @return A unique pointer to the created window.
    [[nodiscard]] virtual UniquePtr<IWindow> createWindow(const WindowDesc& desc) noexcept = 0;
};

}   // namespace gp::hal
