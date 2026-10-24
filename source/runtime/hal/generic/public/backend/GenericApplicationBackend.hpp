// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "backend/ApplicationBackendEnums.hpp"
#include "backend/IApplicationBackend.hpp"

namespace gp::hal::generic
{

/// @brief Generic application backend that serves as a base for platform-specific backends (e.g., SDL, GLFW, Win32).
class ApplicationBackend : public IApplicationBackend
{
public:
    [[nodiscard]] constexpr ApplicationBackend() noexcept = default;
    virtual ~ApplicationBackend() noexcept override = default;

    ApplicationBackend(const ApplicationBackend&) = delete;
    ApplicationBackend& operator=(const ApplicationBackend&) = delete;
    ApplicationBackend(ApplicationBackend&&) = delete;
    ApplicationBackend& operator=(ApplicationBackend&&) = delete;

public:
    /// @brief Initializes the underlying platform abstraction library and configured hardware subsystems.
    /// @param[in] desc Configuration parameters and metadata for the platform backend.
    /// @return `BackendResult::Success` on success, or an error code detailing the failure reason.
    [[nodiscard]] constexpr BackendResult
        initialize([[maybe_unused]] const ApplicationBackendDesc& desc) noexcept override
    {
        return BackendResult::UnsupportedPlatform;
    }

    /// @brief Shuts down all initialized hardware subsystems and releases OS handles.
    constexpr void shutdown() noexcept override
    {
        // No-op for generic backend.
    }

    /// @brief Checks whether the backend is currently initialized and ready to pump events.
    /// @return `true` if initialized, `false` otherwise.
    [[nodiscard]] constexpr bool isInitialized() const noexcept override
    {
        return false;
    }

    /// @brief Drains the OS message queue, translates messages into engine PODs, and writes TLV packets
    ///        into the lock-free byte stream.
    /// @note Must be called from the thread that initialized the video/windowing subsystem (the OS Thread).
    /// @param[in,out] eventStream Target lock-free SPSC byte ring buffer shared with the Game Thread.
    /// @param[out] outStats Optional pointer to receive telemetry counters for the pump invocation.
    /// @return `BackendResult::Success` if all critical events were queued, `BackendResult::BufferOverflow`
    ///         if any event was dropped due to ring buffer exhaustion, or `BackendResult::NotInitialized`.
    virtual BackendResult pumpEvents(
        [[maybe_unused]] gp::SPSCByteRingBuffer& eventStream, [[maybe_unused]] EventPumpStats* outStats = nullptr
    ) noexcept override
    {
        return BackendResult::UnsupportedPlatform;
    }

    /// @brief Blocks the calling OS thread until at least one OS event arrives or the timeout expires.
    /// @details Used when the application is minimized or backgrounded to reduce OS thread CPU usage to 0%.
    /// @param[in] timeoutMs Maximum duration to wait in milliseconds (0 = non-blocking check).
    /// @return `true` if an OS event is pending in the platform queue, `false` if the timeout expired.
    [[nodiscard]] constexpr bool waitEventsTimeout([[maybe_unused]] gp::UInt32 timeoutMs) noexcept override
    {
        return false;
    }

    /// @brief Pushes a synthetic no-op wake signal into the OS event queue from any thread.
    /// @details Immediately unblocks an OS thread sleeping inside `waitEventsTimeout()`.
    constexpr void wakeEventLoop() noexcept override
    {
        // No-op for generic backend.
    }

    /// @brief Returns the static human-readable identifier of the active backend (e.g., "SDL3").
    /// @return Null-terminated ASCII backend name string.
    [[nodiscard]] constexpr gp::StringView getBackendName() const noexcept override
    {
        return "Generic";
    }

    /// @brief Creates a new native window with the specified description.
    /// @param desc The description of the window to create.
    /// @return A unique pointer to the created window.
    [[nodiscard]] constexpr UniquePtr<IWindow> createWindow([[maybe_unused]] const WindowDesc& desc) noexcept override
    {
        return {};
    }
};

}   // namespace gp::hal::generic
