// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "backend/GenericApplicationBackend.hpp"
#include "display/SDL3Display.hpp"

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
    /// @brief Fixed-capacity inline storage for SDL3 physical display wrappers (zero heap allocations).
    sdl3::Display m_sdlDisplays[kMaxDisplays]{};
    gp::UInt32 m_wakeEventType{ 0u };

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
    /// @copydoc IApplicationBackend::initialize()
    [[nodiscard]] BackendResult initialize(const ApplicationBackendDesc& desc) noexcept override;

    /// @copydoc IApplicationBackend::shutdown()
    void shutdown() noexcept override;

    /// @copydoc IApplicationBackend::pumpEvents()
    BackendResult pumpEvents(gp::SPSCByteRingBuffer& eventStream, EventPumpStats* outStats = nullptr) noexcept override;

    /// @copydoc IApplicationBackend::waitEventsTimeout()
    [[nodiscard]] bool waitEventsTimeout(gp::UInt32 timeoutMs) noexcept override;

    /// @copydoc IApplicationBackend::wakeEventLoop()
    void wakeEventLoop() noexcept override;

    /// @copydoc IApplicationBackend::getBackendName()
    [[nodiscard]] constexpr gp::StringView getBackendName() const noexcept override
    {
        return "SDL3";
    }

    /// @copydoc IApplicationBackend::createWindow()
    [[nodiscard]] gp::UniquePtr<IWindow> createWindow(const WindowDesc& desc) noexcept override;

    /// @brief Queries SDL3 for all currently connected monitors and populates `m_sdlDisplays` and `m_displayTable`.
    /// @note Must only be called on the main OS thread.
    void refreshDisplays() noexcept;

protected:
    /// @copydoc gp::hal::generic::ApplicationBackend::onBackendDisplayConnected()
    void onBackendDisplayConnected(DisplayId displayId) noexcept override;

    /// @copydoc gp::hal::generic::ApplicationBackend::onBackendDisplayDisconnected()
    void onBackendDisplayDisconnected(DisplayId displayId) noexcept override;

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
