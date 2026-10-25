// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "time/EngineClock.hpp"
#include "platforms/base/system/Time.hpp"

namespace gp::time
{

EngineClock::EngineClock(const EngineClockDesc& desc) noexcept
    : m_config(desc)
{
    setupDomainHierarchy();
    reset();
}

constexpr void EngineClock::setupDomainHierarchy() noexcept
{
    // Root Clocks: RealTime and UI have no parent (pausing Gameplay does not pause RealTime or UI)
    m_domainClocks[static_cast<gp::USize>(ClockDomain::RealTime)].setParent(nullptr);
    m_domainClocks[static_cast<gp::USize>(ClockDomain::UI)].setParent(nullptr);

    // Gameplay Clock is the root of the simulation subtree
    SimulationClock& gameplayClock = m_domainClocks[static_cast<gp::USize>(ClockDomain::Gameplay)];
    gameplayClock.setParent(nullptr);

    // Physics, Animation, and Audio inherit pause and time-dilation state from Gameplay
    SimulationClock& physicsClock = m_domainClocks[static_cast<gp::USize>(ClockDomain::Physics)];
    physicsClock.setParent(&gameplayClock);
    physicsClock.configureFixedStep(m_config.fixedPhysicsRate, m_config.maxPhysicsSubSteps);

    m_domainClocks[static_cast<gp::USize>(ClockDomain::Animation)].setParent(&gameplayClock);
    m_domainClocks[static_cast<gp::USize>(ClockDomain::Audio)].setParent(&gameplayClock);
}

void EngineClock::reset() noexcept
{
    const Timestamp now(gp::platform::Time::nowNanoseconds());
    m_bootTimestamp = now;
    m_frameStartTimestamp = now;
    m_frameIndex = 0ULL;
    m_historyCount = 0u;
    m_historyWriteIndex = 0u;
    m_smoothedFps = 60.0f;

    for (gp::USize i = 0u; i < static_cast<gp::USize>(ClockDomain::Count); ++i)
    {
        m_domainClocks[i].reset();
    }

    m_latestStats = FrameTimeStats{};
}

const FrameTimeStats& EngineClock::tick() noexcept
{
    const Timestamp now(gp::platform::Time::nowNanoseconds());
    Duration rawDelta = now - m_frameStartTimestamp;
    if (rawDelta.nanoseconds < 0) [[unlikely]]
    {
        rawDelta = Duration::zero();
    }

    m_frameStartTimestamp = now;
    ++m_frameIndex;

    // 1. Breakpoint / Hitch Clamping
    const Duration clampedDelta = (rawDelta > m_config.maxFrameDelta) ? m_config.maxFrameDelta : rawDelta;

    // 2. Delta Outlier Filtering / Smoothing
    const Duration smoothedDelta = computeSmoothedDelta(clampedDelta);

    // 3. Advance all 6 Domain Clocks
    //    - RealTime advances by raw unclamped delta
    //    - UI, Gameplay, Physics, Animation, and Audio advance by smoothedDelta (scaled by their hierarchy)
    (void)m_domainClocks[static_cast<gp::USize>(ClockDomain::RealTime)].advance(rawDelta);
    (void)m_domainClocks[static_cast<gp::USize>(ClockDomain::UI)].advance(smoothedDelta);

    const Duration gameplayDelta = m_domainClocks[static_cast<gp::USize>(ClockDomain::Gameplay)].advance(smoothedDelta);

    SimulationClock& physicsClock = m_domainClocks[static_cast<gp::USize>(ClockDomain::Physics)];
    (void)physicsClock.advance(smoothedDelta);
    const FixedStepResult fixedStep = physicsClock.consumeFixedSteps();

    (void)m_domainClocks[static_cast<gp::USize>(ClockDomain::Animation)].advance(smoothedDelta);
    (void)m_domainClocks[static_cast<gp::USize>(ClockDomain::Audio)].advance(smoothedDelta);

    // 4. Update Smoothed FPS Telemetry
    const gp::Float64 unscaledSeconds = smoothedDelta.toSeconds();
    if (unscaledSeconds > 1e-7) [[likely]]
    {
        const gp::Float32 instantFps = static_cast<gp::Float32>(1.0 / unscaledSeconds);
        m_smoothedFps = (m_smoothedFps * 0.90f) + (instantFps * 0.10f);
    }

    // 5. Populate FrameTimeStats Snapshot
    m_latestStats.frameIndex = m_frameIndex;
    m_latestStats.rawDelta = rawDelta;
    m_latestStats.smoothedUnscaledDelta = smoothedDelta;
    m_latestStats.gameplayDelta = gameplayDelta;
    m_latestStats.totalRealTime = now - m_bootTimestamp;
    m_latestStats.totalGameplayTime = m_domainClocks[static_cast<gp::USize>(ClockDomain::Gameplay)].getTotalElapsed();
    m_latestStats.deltaTime = gameplayDelta.toSecondsFloat();
    m_latestStats.deltaTimeDouble = gameplayDelta.toSeconds();
    m_latestStats.unscaledDeltaTime = smoothedDelta.toSecondsFloat();
    m_latestStats.smoothedFps = m_smoothedFps;
    m_latestStats.fixedStep = fixedStep;

    return m_latestStats;
}

void EngineClock::paceFrame() noexcept
{
    (void)m_framePacer.paceUntilDeadline(m_frameStartTimestamp);
}

void EngineClock::paceFrame(gp::Float64 targetFps) noexcept
{
    m_framePacer.setTargetFps(targetFps);
    (void)m_framePacer.paceUntilDeadline(m_frameStartTimestamp);
}

constexpr Duration EngineClock::computeSmoothedDelta(Duration clampedDelta) noexcept
{
    switch (m_config.smoothingMode)
    {
    case DeltaSmoothingMode::FixedDeterministic:
        return m_config.deterministicRate.toFrameDuration();

    case DeltaSmoothingMode::ExponentialMovingAverage:
    {
        if (m_frameIndex <= 1ULL) [[unlikely]]
        {
            return clampedDelta;
        }
        const gp::Int64 prevNs = m_latestStats.smoothedUnscaledDelta.nanoseconds;
        return Duration(((prevNs * 9LL) + clampedDelta.nanoseconds) / 10LL);
    }

    case DeltaSmoothingMode::OutlierRejectedAverage:
    {
        m_deltaHistory[m_historyWriteIndex] = clampedDelta;
        m_historyWriteIndex = (m_historyWriteIndex + 1u) % kDeltaHistorySize;
        if (m_historyCount < kDeltaHistorySize)
        {
            ++m_historyCount;
        }

        // Need at least 5 samples to discard the min and max outliers meaningfully
        if (m_historyCount < 5u) [[unlikely]]
        {
            return clampedDelta;
        }

        gp::Int64 sumNs = 0LL;
        gp::Int64 minNs = m_deltaHistory[0].nanoseconds;
        gp::Int64 maxNs = m_deltaHistory[0].nanoseconds;

        for (gp::UInt32 i = 0u; i < m_historyCount; ++i)
        {
            const gp::Int64 val = m_deltaHistory[i].nanoseconds;
            sumNs += val;
            if (val < minNs)
            {
                minNs = val;
            }
            if (val > maxNs)
            {
                maxNs = val;
            }
        }

        // Discard highest and lowest outlier samples and average the remaining (m_historyCount - 2) frames
        const gp::Int64 trimmedSumNs = sumNs - minNs - maxNs;
        return Duration(trimmedSumNs / static_cast<gp::Int64>(m_historyCount - 2u));
    }

    case DeltaSmoothingMode::None:
    default:
        return clampedDelta;
    }
}

}   // namespace gp::time
