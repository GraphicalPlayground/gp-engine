// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "backend/GenericApplicationBackend.hpp"

namespace gp::hal::sdl3
{

/// @brief Represents the SDL3 application backend, responsible for initializing and managing the SDL3 library HAL.
/// @details Translates raw `SDL_Event` unions into engine-native POD structs and commits them into the
///          lock-free `SPSCByteRingBuffer` using two-tier backpressure admission control.
class ApplicationBackend : public generic::ApplicationBackend
{
public:
    /// @brief Maximum UTF-8 string payload length (in bytes) permitted in a single variable-length TLV event.
    static constexpr gp::UInt16 kMaxVariablePayloadBytes = 2048u;

private:
    ApplicationBackendDesc m_config{};
    gp::UInt32 m_wakeEventType{ 0u };
    bool m_isInitialized{ false };

public:
    /// @brief Constructs an uninitialized SDL3 application backend.
    [[nodiscard]] constexpr ApplicationBackend() noexcept = default;

    /// @brief Destroys the backend, automatically invoking `shutdown()` if still initialized.
    ~ApplicationBackend() noexcept override;

    ApplicationBackend(const ApplicationBackend&) = delete;
    ApplicationBackend& operator=(const ApplicationBackend&) = delete;
    ApplicationBackend(ApplicationBackend&&) = delete;
    ApplicationBackend& operator=(ApplicationBackend&&) = delete;

public:
    /// @brief Initializes the underlying platform abstraction library and configured hardware subsystems.
    /// @param[in] desc Configuration parameters and metadata for the platform backend.
    /// @return `BackendResult::Success` on success, or an error code detailing the failure reason.
    [[nodiscard]] virtual BackendResult initialize(const ApplicationBackendDesc& desc) noexcept override;

    /// @brief Shuts down all initialized hardware subsystems and releases OS handles.
    virtual void shutdown() noexcept override;

    /// @brief Checks whether the backend is currently initialized and ready to pump events.
    /// @return `true` if initialized, `false` otherwise.
    [[nodiscard]] constexpr bool isInitialized() const noexcept override
    {
        return m_isInitialized;
    }

    /// @brief Drains the OS message queue, translates messages into engine PODs, and writes TLV packets
    ///        into the lock-free byte stream.
    /// @note Must be called from the thread that initialized the video/windowing subsystem (the OS Thread).
    /// @param[in,out] eventStream Target lock-free SPSC byte ring buffer shared with the Game Thread.
    /// @param[out] outStats Optional pointer to receive telemetry counters for the pump invocation.
    /// @return `BackendResult::Success` if all critical events were queued, `BackendResult::BufferOverflow`
    ///         if any event was dropped due to ring buffer exhaustion, or `BackendResult::NotInitialized`.
    virtual BackendResult
        pumpEvents(gp::SPSCByteRingBuffer& eventStream, EventPumpStats* outStats = nullptr) noexcept override;

    /// @brief Blocks the calling OS thread until at least one OS event arrives or the timeout expires.
    /// @details Used when the application is minimized or backgrounded to reduce OS thread CPU usage to 0%.
    /// @param[in] timeoutMs Maximum duration to wait in milliseconds (0 = non-blocking check).
    /// @return `true` if an OS event is pending in the platform queue, `false` if the timeout expired.
    [[nodiscard]] virtual bool waitEventsTimeout(gp::UInt32 timeoutMs) noexcept override;

    /// @brief Pushes a synthetic no-op wake signal into the OS event queue from any thread.
    /// @details Immediately unblocks an OS thread sleeping inside `waitEventsTimeout()`.
    virtual void wakeEventLoop() noexcept override;

    /// @brief Returns the static human-readable identifier of the active backend (e.g., "SDL3").
    /// @return Null-terminated ASCII backend name string.
    [[nodiscard]] constexpr gp::StringView getBackendName() const noexcept override
    {
        return "SDL3";
    }

    /// @brief Creates a new native window with the specified description.
    /// @param desc The description of the window to create.
    /// @return A unique pointer to the created window.
    [[nodiscard]] virtual UniquePtr<IWindow> createWindow(const WindowDesc& desc) noexcept override;

private:
    /// @brief Commits a fixed-size POD event into the SPSC byte ring buffer with backpressure filtering.
    /// @tparam PayloadT Trivially copyable engine event struct type.
    /// @param[in,out] stream Target SPSC byte ring buffer.
    /// @param[in] category High-level routing category for the header.
    /// @param[in] payload Fixed-size POD event struct.
    /// @param[in] isContinuous True if the event is a high-frequency continuous stream (eligible for shedding).
    /// @param[in,out] stats Telemetry counters updated by the write attempt.
    template <typename PayloadT>
    void commitFixedEvent(
        gp::SPSCByteRingBuffer& stream,
        gp::hal::event::EventCategory category,
        const PayloadT& payload,
        bool isContinuous,
        EventPumpStats& stats
    ) const noexcept;

    /// @brief Commits a variable-length TLV event (descriptor struct + trailing UTF-8 bytes) atomically.
    /// @tparam DescriptorT Trivially copyable descriptor struct (`TextInputEvent`, `DropEvent`, `ClipboardEvent`).
    /// @param[in,out] stream Target SPSC byte ring buffer.
    /// @param[in] category High-level routing category for the header.
    /// @param[in] descriptor Fixed-size descriptor header preceding the trailing bytes.
    /// @param[in] trailingData Pointer to trailing UTF-8 bytes (may be `nullptr` if `trailingBytes == 0`).
    /// @param[in] trailingBytes Number of trailing bytes to append immediately after `descriptor`.
    /// @param[in,out] stats Telemetry counters updated by the write attempt.
    template <typename DescriptorT>
    void commitVariableEvent(
        gp::SPSCByteRingBuffer& stream,
        gp::hal::event::EventCategory category,
        const DescriptorT& descriptor,
        const void* trailingData,
        gp::UInt16 trailingBytes,
        EventPumpStats& stats
    ) const noexcept;
};

}   // namespace gp::hal::sdl3
