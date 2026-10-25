// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "time/TimeTypes.hpp"

namespace gp::time
{

/// @brief Hierarchical simulation clock managing domain-specific time dilation, pausing, single-stepping,
///        and deterministic fixed-timestep accumulation (ADR-0032, ADR-0033).
class SimulationClock final
{
private:
    const SimulationClock* m_parent{ nullptr };

    Duration m_totalElapsed{};
    Duration m_lastDelta{};
    Duration m_accumulator{};
    Duration m_fixedStepDuration{ FrameRate(60u, 1u).toFrameDuration() };

    gp::Float64 m_localTimeScale{ 1.0 };
    gp::UInt64 m_tickCount{ 0ULL };
    gp::UInt32 m_maxSubSteps{ 8u };

    bool m_isPaused{ false };
    bool m_singleStepRequested{ false };

public:
    /// @brief Default constructor initializes a root simulation clock at 1.0x time scale.
    [[nodiscard]] constexpr SimulationClock() noexcept = default;

    /// @brief Constructs a simulation clock with a specific time scale and optional parent.
    /// @param[in] initialScale Initial time dilation multiplier (1.0 = normal speed).
    /// @param[in] parent Optional pointer to a parent `SimulationClock` in the hierarchy.
    [[nodiscard]] explicit constexpr SimulationClock(
        gp::Float64 initialScale, const SimulationClock* parent = nullptr
    ) noexcept
        : m_parent(parent)
        , m_localTimeScale(initialScale >= 0.0 ? initialScale : 0.0)
    {}

    ~SimulationClock() noexcept = default;

    SimulationClock(const SimulationClock&) = delete;
    SimulationClock& operator=(const SimulationClock&) = delete;
    SimulationClock(SimulationClock&&) noexcept = default;
    SimulationClock& operator=(SimulationClock&&) noexcept = default;

public:
    /// @brief Attaches this clock to a parent clock (or detaches if `parent == nullptr`).
    /// @param[in] parent Pointer to parent clock (must not create a cycle).
    constexpr void setParent(const SimulationClock* parent) noexcept
    {
        if (parent != this) [[likely]]
        {
            m_parent = parent;
        }
    }

    /// @brief Returns the parent clock in the hierarchy, or `nullptr` if this is a root clock.
    [[nodiscard]] constexpr const SimulationClock* getParent() const noexcept
    {
        return m_parent;
    }

    /// @brief Sets the local time dilation multiplier for this clock domain.
    /// @param[in] scale Non-negative multiplier (1.0 = real speed, 0.5 = half speed, 2.0 = double speed).
    constexpr void setLocalTimeScale(gp::Float64 scale) noexcept
    {
        m_localTimeScale = (scale >= 0.0) ? scale : 0.0;
    }

    /// @brief Gets the local time scale multiplier (excluding parent scale).
    [[nodiscard]] constexpr gp::Float64 getLocalTimeScale() const noexcept
    {
        return m_localTimeScale;
    }

    /// @brief Computes the effective time scale by multiplying local scale up the parent hierarchy.
    /// @return Effective time scale multiplier (0.0 if this clock or any ancestor is paused).
    [[nodiscard]] constexpr gp::Float64 getEffectiveTimeScale() const noexcept
    {
        if (isEffectivelyPaused())
        {
            return 0.0;
        }

        gp::Float64 scale = m_localTimeScale;
        const SimulationClock* curr = m_parent;
        while (curr != nullptr)
        {
            scale *= curr->m_localTimeScale;
            curr = curr->m_parent;
        }
        return scale;
    }

    /// @brief Pauses or unpauses this clock domain (and all child clocks attached to it).
    /// @param[in] paused True to pause, false to resume.
    constexpr void setPaused(bool paused) noexcept
    {
        m_isPaused = paused;
        if (!paused)
        {
            m_singleStepRequested = false;
        }
    }

    /// @brief Checks if this clock is locally paused.
    [[nodiscard]] constexpr bool isLocallyPaused() const noexcept
    {
        return m_isPaused;
    }

    /// @brief Checks if this clock or any ancestor clock in its hierarchy is paused (unless single-stepping).
    [[nodiscard]] constexpr bool isEffectivelyPaused() const noexcept
    {
        if (m_singleStepRequested)
        {
            return false;
        }

        const SimulationClock* curr = this;
        while (curr != nullptr)
        {
            if (curr->m_isPaused && !curr->m_singleStepRequested)
            {
                return true;
            }
            curr = curr->m_parent;
        }
        return false;
    }

    /// @brief Advances a paused clock by exactly one fixed frame step on the next `advance()` call, then re-pauses.
    /// @details Essential for frame-by-frame gameplay and physics debugging in editor/development builds.
    constexpr void requestSingleStep() noexcept
    {
        if (m_isPaused)
        {
            m_singleStepRequested = true;
        }
    }

public:
    /// @brief Configures the fixed timestep rate (e.g., `FrameRate(60, 1)` or `FrameRate(120, 1)`) and max sub-steps.
    /// @param[in] rate Rational target tick rate for fixed updates.
    /// @param[in] maxSubSteps Maximum discrete steps allowed per frame to prevent the "Spiral of Death".
    constexpr void configureFixedStep(FrameRate rate, gp::UInt32 maxSubSteps = 8u) noexcept
    {
        const Duration stepDur = rate.toFrameDuration();
        if (stepDur.nanoseconds > 0)
        {
            m_fixedStepDuration = stepDur;
        }
        m_maxSubSteps = (maxSubSteps > 0u) ? maxSubSteps : 1u;
    }

    /// @brief Gets the configured fixed timestep duration.
    [[nodiscard]] constexpr Duration getFixedStepDuration() const noexcept
    {
        return m_fixedStepDuration;
    }

public:
    /// @brief Resets all elapsed time, accumulators, and tick counters to zero.
    constexpr void reset() noexcept
    {
        m_totalElapsed = Duration::zero();
        m_lastDelta = Duration::zero();
        m_accumulator = Duration::zero();
        m_tickCount = 0ULL;
        m_singleStepRequested = false;
    }

    /// @brief Advances the simulation clock by an unscaled base duration (typically smoothed frame delta).
    /// @param[in] unscaledDelta Base frame duration before hierarchy scaling.
    /// @return The resulting scaled `Duration` for this clock domain on this frame.
    constexpr Duration advance(Duration unscaledDelta) noexcept
    {
        if (m_singleStepRequested) [[unlikely]]
        {
            m_singleStepRequested = false;
            m_lastDelta = m_fixedStepDuration;
            m_totalElapsed += m_lastDelta;
            m_accumulator += m_lastDelta;
            ++m_tickCount;
            return m_lastDelta;
        }

        if (isEffectivelyPaused())
        {
            m_lastDelta = Duration::zero();
            return m_lastDelta;
        }

        const gp::Float64 effectiveScale = getEffectiveTimeScale();
        m_lastDelta = unscaledDelta.scaled(effectiveScale);
        m_totalElapsed += m_lastDelta;
        m_accumulator += m_lastDelta;
        ++m_tickCount;

        return m_lastDelta;
    }

    /// @brief Drains the fixed-timestep accumulator and computes the sub-step count and render interpolation alpha.
    /// @return `FixedStepResult` containing `stepCount`, `fixedDelta`, and `interpolationAlpha` $\in [0.0f, 1.0f)$.
    [[nodiscard]] constexpr FixedStepResult consumeFixedSteps() noexcept
    {
        FixedStepResult result{};
        result.fixedDelta = m_fixedStepDuration;

        if (m_fixedStepDuration.nanoseconds <= 0) [[unlikely]]
        {
            return result;
        }

        while (m_accumulator >= m_fixedStepDuration && result.stepCount < m_maxSubSteps)
        {
            m_accumulator -= m_fixedStepDuration;
            ++result.stepCount;
        }

        // Spiral-of-Death guard: discard excess accumulated time if we hit the sub-step ceiling
        if (m_accumulator >= m_fixedStepDuration) [[unlikely]]
        {
            m_accumulator = Duration(m_accumulator.nanoseconds % m_fixedStepDuration.nanoseconds);
            result.wasClamped = true;
        }

        result.interpolationAlpha = static_cast<gp::Float32>(
            static_cast<gp::Float64>(m_accumulator.nanoseconds) /
            static_cast<gp::Float64>(m_fixedStepDuration.nanoseconds)
        );

        return result;
    }

    /// @brief Returns the scaled delta duration from the most recent `advance()` call.
    [[nodiscard]] constexpr Duration getDelta() const noexcept
    {
        return m_lastDelta;
    }

    /// @brief Returns the scaled delta in 32-bit floating-point seconds.
    [[nodiscard]] constexpr gp::Float32 getDeltaSeconds() const noexcept
    {
        return m_lastDelta.toSecondsFloat();
    }

    /// @brief Returns the total accumulated scaled time since `reset()`.
    [[nodiscard]] constexpr Duration getTotalElapsed() const noexcept
    {
        return m_totalElapsed;
    }

    /// @brief Returns the total accumulated scaled time in 64-bit floating-point seconds.
    [[nodiscard]] constexpr gp::Float64 getTotalSeconds() const noexcept
    {
        return m_totalElapsed.toSeconds();
    }

    /// @brief Returns the number of non-paused ticks this clock has executed.
    [[nodiscard]] constexpr gp::UInt64 getTickCount() const noexcept
    {
        return m_tickCount;
    }
};

}   // namespace gp::time
