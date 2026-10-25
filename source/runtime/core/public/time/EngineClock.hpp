// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "time/FramePacer.hpp"
#include "time/SimulationClock.hpp"
#include "time/TimeEnums.hpp"
#include "time/TimeTypes.hpp"

namespace gp::time
{

/// @brief Configuration descriptor for initializing `EngineClock`.
struct EngineClockDesc
{
    /// @brief Maximum allowed raw frame delta before clamping (default 250ms / 4 FPS floor for breakpoint safety).
    Duration maxFrameDelta{ Duration::fromMilliseconds(250.0) };

    /// @brief Fixed timestep rate for the `ClockDomain::Physics` simulation clock (default 60 Hz).
    FrameRate fixedPhysicsRate{ 60u, 1u };

    /// @brief Target frame rate used when `smoothingMode == DeltaSmoothingMode::FixedDeterministic`.
    FrameRate deterministicRate{ 60u, 1u };

    /// @brief Maximum discrete physics sub-steps allowed in a single frame.
    gp::UInt32 maxPhysicsSubSteps{ 8u };

    /// @brief Delta-time smoothing filter mode.
    DeltaSmoothingMode smoothingMode{ DeltaSmoothingMode::OutlierRejectedAverage };
};

/// @brief Master engine clock orchestrating hardware timestamps, delta outlier filtering, and hierarchical domain
/// clocks.
/// @details Maintains the 6 canonical engine clock domains (`RealTime`, `UI`, `Gameplay`, `Physics`, `Animation`,
/// `Audio`) and produces a unified `FrameTimeStats` snapshot on every frame tick.
class EngineClock final
{
public:
    /// @brief Number of historical frame deltas retained for outlier-rejected rolling average smoothing.
    static constexpr gp::UInt32 kDeltaHistorySize = 11u;

private:
    EngineClockDesc m_config{};

    Timestamp m_bootTimestamp{};
    Timestamp m_frameStartTimestamp{};
    gp::UInt64 m_frameIndex{ 0ULL };

    // Rolling delta history ring buffer for OutlierRejectedAverage smoothing
    Duration m_deltaHistory[kDeltaHistorySize]{};
    gp::UInt32 m_historyCount{ 0u };
    gp::UInt32 m_historyWriteIndex{ 0u };

    gp::Float32 m_smoothedFps{ 60.0f };

    // Canonical Hierarchical Domain Clocks
    SimulationClock m_domainClocks[static_cast<gp::USize>(ClockDomain::Count)]{};

    FramePacer m_framePacer{};
    FrameTimeStats m_latestStats{};

public:
    /// @brief Constructs and initializes the master engine clock and its domain hierarchy.
    /// @param[in] desc Configuration parameters for clamping, smoothing, and fixed physics rate.
    [[nodiscard]] explicit EngineClock(const EngineClockDesc& desc = EngineClockDesc{}) noexcept;

    ~EngineClock() noexcept = default;

    EngineClock(const EngineClock&) = delete;
    EngineClock& operator=(const EngineClock&) = delete;
    EngineClock(EngineClock&&) = delete;
    EngineClock& operator=(EngineClock&&) = delete;

public:
    /// @brief Resets all domain clocks, history buffers, and timestamps to the current hardware time.
    void reset() noexcept;

    /// @brief Samples the hardware clock, filters the frame delta, advances all domain clocks, and drains fixed steps.
    /// @return Const reference to the updated `FrameTimeStats` for the current frame.
    const FrameTimeStats& tick() noexcept;

    /// @brief Paces the current frame to the configured `FramePacer` target rate.
    void paceFrame() noexcept;

    /// @brief Paces the current frame to an explicit floating-point FPS target (e.g., foreground vs background FPS).
    /// @param[in] targetFps Desired frame rate in Hz (`<= 0.0` for uncapped).
    void paceFrame(gp::Float64 targetFps) noexcept;

    /// @brief Retrieves a mutable reference to one of the built-in simulation clock domains.
    /// @param[in] domain Target `ClockDomain` enumerator.
    /// @return Reference to the corresponding `SimulationClock`.
    [[nodiscard]] constexpr SimulationClock& getDomainClock(ClockDomain domain) noexcept
    {
        return m_domainClocks[static_cast<gp::USize>(domain)];
    }

    /// @brief Retrieves a const reference to one of the built-in simulation clock domains.
    /// @param[in] domain Target `ClockDomain` enumerator.
    /// @return Const reference to the corresponding `SimulationClock`.
    [[nodiscard]] constexpr const SimulationClock& getDomainClock(ClockDomain domain) const noexcept
    {
        return m_domainClocks[static_cast<gp::USize>(domain)];
    }

    /// @brief Convenience accessor for the primary `ClockDomain::Gameplay` clock.
    [[nodiscard]] constexpr SimulationClock& getGameplayClock() noexcept
    {
        return getDomainClock(ClockDomain::Gameplay);
    }

    /// @brief Convenience accessor for the deterministic `ClockDomain::Physics` clock.
    [[nodiscard]] constexpr SimulationClock& getPhysicsClock() noexcept
    {
        return getDomainClock(ClockDomain::Physics);
    }

    /// @brief Convenience accessor for the unpausable `ClockDomain::UI` clock.
    [[nodiscard]] constexpr SimulationClock& getUIClock() noexcept
    {
        return getDomainClock(ClockDomain::UI);
    }

    /// @brief Returns a reference to the internal `FramePacer`.
    [[nodiscard]] constexpr FramePacer& getFramePacer() noexcept
    {
        return m_framePacer;
    }

    /// @brief Returns the latest per-frame timing telemetry snapshot.
    [[nodiscard]] constexpr const FrameTimeStats& getStats() const noexcept
    {
        return m_latestStats;
    }

    /// @brief Changes the delta-time smoothing filter mode at runtime.
    /// @param[in] mode New `DeltaSmoothingMode`.
    constexpr void setSmoothingMode(DeltaSmoothingMode mode) noexcept
    {
        m_config.smoothingMode = mode;
    }

    /// @brief Gets the active delta-time smoothing filter mode.
    [[nodiscard]] constexpr DeltaSmoothingMode getSmoothingMode() const noexcept
    {
        return m_config.smoothingMode;
    }

private:
    /// @brief Configures the parent-child links between the 6 built-in domain clocks.
    constexpr void setupDomainHierarchy() noexcept;

    /// @brief Applies the configured `DeltaSmoothingMode` filter to a clamped frame delta.
    [[nodiscard]] constexpr Duration computeSmoothedDelta(Duration clampedDelta) noexcept;
};

}   // namespace gp::time
