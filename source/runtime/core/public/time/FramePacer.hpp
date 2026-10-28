// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "platforms/base/system/Thread.hpp"
#include "platforms/base/system/Time.hpp"
#include "time/TimeEnums.hpp"
#include "time/TimeTypes.hpp"

namespace gp::time
{

/// @brief Adaptive high-precision frame pacer combining OS high-resolution waitable timers with CPU spin-waiting.
/// @details Continuously measures OS scheduler wake overshoot to dynamically size the `_mm_pause` spin window,
///          achieving sub-microsecond frame delivery with minimal CPU power consumption (ADR-0031).
class FramePacer final
{
public:
    /// @brief Initial conservative estimate of OS high-resolution timer wake jitter (500 microseconds).
    static constexpr gp::Int64 kInitialOvershootMarginNs = 500'000LL;

    /// @brief Minimum spin-wait safety margin (150 microseconds).
    static constexpr gp::Int64 kMinOvershootMarginNs = 150'000LL;

    /// @brief Maximum spin-wait safety margin cap (2.5 milliseconds).
    static constexpr gp::Int64 kMaxOvershootMarginNs = 2'500'000LL;

private:
    FrameRate m_targetRate{ 120u, 1u };
    Duration m_targetFrameDuration{ FrameRate(120u, 1u).toFrameDuration() };
    gp::Int64 m_estimatedOvershootNs{ kInitialOvershootMarginNs };
    FramePacingMode m_mode{ FramePacingMode::HybridSleepAndSpin };

public:
    /// @brief Constructs a frame pacer with the specified target frame rate and pacing mode.
    /// @param[in] targetRate Desired rational frame rate (e.g., `FrameRate(120, 1)`).
    /// @param[in] mode Pacing execution strategy.
    [[nodiscard]] explicit constexpr FramePacer(
        FrameRate targetRate = FrameRate(120u, 1u), FramePacingMode mode = FramePacingMode::HybridSleepAndSpin
    ) noexcept
        : m_targetRate(targetRate)
        , m_targetFrameDuration(targetRate.toFrameDuration())
        , m_mode(mode)
    {}

    ~FramePacer() noexcept = default;

    FramePacer(const FramePacer&) = delete;
    FramePacer& operator=(const FramePacer&) = delete;
    FramePacer(FramePacer&&) noexcept = default;
    FramePacer& operator=(FramePacer&&) noexcept = default;

public:
    /// @brief Sets the target rational frame rate.
    /// @param[in] rate Target rational frame rate (pass `FrameRate(0, 1)` to disable pacing).
    constexpr void setTargetFrameRate(FrameRate rate) noexcept
    {
        m_targetRate = rate;
        m_targetFrameDuration = rate.toFrameDuration();
    }

    /// @brief Sets the target frame rate from a floating-point Hz value (e.g., `60.0`, `144.0`).
    /// @param[in] fps Desired frames per second (`<= 0.0` disables pacing).
    constexpr void setTargetFps(gp::Float64 fps) noexcept
    {
        if (fps <= 0.0)
        {
            m_targetRate = FrameRate(0u, 1u);
            m_targetFrameDuration = Duration::zero();
        }
        else
        {
            m_targetRate = FrameRate(static_cast<gp::UInt32>(fps * 1000.0), 1000u);
            m_targetFrameDuration = Duration::fromSeconds(1.0 / fps);
        }
    }

    /// @brief Gets the current rational target frame rate.
    [[nodiscard]] constexpr FrameRate getTargetFrameRate() const noexcept
    {
        return m_targetRate;
    }

    /// @brief Sets the frame pacing execution mode.
    /// @param[in] mode Desired `FramePacingMode`.
    constexpr void setPacingMode(FramePacingMode mode) noexcept
    {
        m_mode = mode;
    }

    /// @brief Gets the current frame pacing execution mode.
    [[nodiscard]] constexpr FramePacingMode getPacingMode() const noexcept
    {
        return m_mode;
    }

    /// @brief Paces the calling thread until `frameStartTimestamp + targetFrameDuration` is reached.
    /// @param[in] frameStartTimestamp Monotonic hardware timestamp captured at the beginning of the frame.
    /// @return The final hardware `Timestamp` immediately after pacing completes.
    Timestamp paceUntilDeadline(Timestamp frameStartTimestamp) noexcept
    {
        if (m_mode == FramePacingMode::Uncapped || m_targetFrameDuration.nanoseconds <= 0)
        {
            return Timestamp(gp::platform::Time::nowNanoseconds());
        }

        const Timestamp deadline = frameStartTimestamp + m_targetFrameDuration;
        Timestamp now(gp::platform::Time::nowNanoseconds());

        if (now >= deadline)
        {
            return now;   // Frame exceeded budget; return immediately without sleeping
        }

        // Stage 1: High-resolution kernel waitable timer sleep
        if (m_mode == FramePacingMode::HybridSleepAndSpin || m_mode == FramePacingMode::SleepOnly)
        {
            const gp::Int64 remainingNs = (deadline - now).nanoseconds;
            const gp::Int64 marginNs = (m_mode == FramePacingMode::HybridSleepAndSpin) ? m_estimatedOvershootNs : 0LL;

            if (remainingNs > marginNs)
            {
                const gp::UInt64 requestedSleepNs = static_cast<gp::UInt64>(remainingNs - marginNs);
                const gp::UInt64 beforeSleepNs = now.nanoseconds;

                gp::platform::Time::sleepNanoseconds(requestedSleepNs);

                now = Timestamp(gp::platform::Time::nowNanoseconds());
                const gp::UInt64 actualSleptNs = now.nanoseconds - beforeSleepNs;

                // Update adaptive overshoot estimate if the OS woke us up later than requested
                if (actualSleptNs > requestedSleepNs)
                {
                    const gp::Int64 overshootNs = static_cast<gp::Int64>(actualSleptNs - requestedSleepNs);
                    m_estimatedOvershootNs = ((m_estimatedOvershootNs * 7LL) + overshootNs) / 8LL;
                }
                else
                {
                    // Gently decay the margin when the OS scheduler wakes on time
                    m_estimatedOvershootNs = (m_estimatedOvershootNs * 31LL) / 32LL;
                }

                if (m_estimatedOvershootNs < kMinOvershootMarginNs)
                {
                    m_estimatedOvershootNs = kMinOvershootMarginNs;
                }
                else if (m_estimatedOvershootNs > kMaxOvershootMarginNs)
                {
                    m_estimatedOvershootNs = kMaxOvershootMarginNs;
                }
            }
        }

        // Stage 2: Architecture-aware spin-wait (_mm_pause) for sub-microsecond precision
        if (m_mode == FramePacingMode::HybridSleepAndSpin || m_mode == FramePacingMode::SpinOnly)
        {
            while (now < deadline)
            {
                gp::platform::Thread::spinWait();
                now = Timestamp(gp::platform::Time::nowNanoseconds());
            }
        }

        return now;
    }
};

}   // namespace gp::time
