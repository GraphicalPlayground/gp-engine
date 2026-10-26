// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "time/EngineClock.hpp"
#include <gtest/gtest.h>

namespace gp::time::tests
{

static_assert(EngineClock::kDeltaHistorySize == 11u, "Delta history buffer size must be 11.");

TEST(EngineClockTest, DefaultConstructionAndHierarchySetup)
{
    EngineClock clock;

    // Verify root clocks
    EXPECT_EQ(clock.getDomainClock(ClockDomain::RealTime).getParent(), nullptr);
    EXPECT_EQ(clock.getDomainClock(ClockDomain::UI).getParent(), nullptr);
    EXPECT_EQ(clock.getDomainClock(ClockDomain::Gameplay).getParent(), nullptr);

    // Verify child clocks attached to Gameplay
    const SimulationClock& gameplay = clock.getGameplayClock();
    EXPECT_EQ(clock.getDomainClock(ClockDomain::Physics).getParent(), &gameplay);
    EXPECT_EQ(clock.getDomainClock(ClockDomain::Animation).getParent(), &gameplay);
    EXPECT_EQ(clock.getDomainClock(ClockDomain::Audio).getParent(), &gameplay);

    // Verify const access via const reference
    const EngineClock& constClock = clock;
    EXPECT_EQ(constClock.getDomainClock(ClockDomain::RealTime).getParent(), nullptr);
    EXPECT_EQ(constClock.getDomainClock(ClockDomain::Physics).getParent(), &gameplay);

    // Verify initial smoothing mode and physics fixed-step
    EXPECT_EQ(clock.getSmoothingMode(), DeltaSmoothingMode::OutlierRejectedAverage);
    EXPECT_EQ(clock.getPhysicsClock().getFixedStepDuration().nanoseconds, 16'666'666LL);

    // Verify initial telemetry snapshot
    const FrameTimeStats& stats = clock.getStats();
    EXPECT_EQ(stats.frameIndex, 0ULL);
    EXPECT_EQ(stats.rawDelta.nanoseconds, 0LL);
    EXPECT_FLOAT_EQ(stats.smoothedFps, 60.0f);
}

TEST(EngineClockTest, CustomDescriptorInitialization)
{
    EngineClockDesc desc;
    desc.maxFrameDelta = Duration::fromMilliseconds(100.0);
    desc.fixedPhysicsRate = FrameRate(120u, 1u);
    desc.deterministicRate = FrameRate(30u, 1u);
    desc.maxPhysicsSubSteps = 4u;
    desc.smoothingMode = DeltaSmoothingMode::FixedDeterministic;

    EngineClock clock(desc);

    EXPECT_EQ(clock.getSmoothingMode(), DeltaSmoothingMode::FixedDeterministic);
    EXPECT_EQ(clock.getPhysicsClock().getFixedStepDuration().nanoseconds, 8'333'333LL);
}

TEST(EngineClockTest, ConvenienceAndDomainAccessors)
{
    EngineClock clock;

    // Non-const references
    SimulationClock& gameplay = clock.getGameplayClock();
    SimulationClock& physics = clock.getPhysicsClock();
    SimulationClock& ui = clock.getUIClock();
    FramePacer& pacer = clock.getFramePacer();

    EXPECT_EQ(&gameplay, &clock.getDomainClock(ClockDomain::Gameplay));
    EXPECT_EQ(&physics, &clock.getDomainClock(ClockDomain::Physics));
    EXPECT_EQ(&ui, &clock.getDomainClock(ClockDomain::UI));

    pacer.setTargetFps(144.0);
    EXPECT_DOUBLE_EQ(clock.getFramePacer().getTargetFrameRate().toHz(), 144.0);

    // Smoothing mode accessors
    clock.setSmoothingMode(DeltaSmoothingMode::ExponentialMovingAverage);
    EXPECT_EQ(clock.getSmoothingMode(), DeltaSmoothingMode::ExponentialMovingAverage);
}

TEST(EngineClockTest, ResetRestoresCleanState)
{
    EngineClock clock;

    // Tick a few times to dirty the state
    (void)clock.tick();
    (void)clock.tick();
    EXPECT_EQ(clock.getStats().frameIndex, 2ULL);

    clock.reset();

    const FrameTimeStats& stats = clock.getStats();
    EXPECT_EQ(stats.frameIndex, 0ULL);
    EXPECT_EQ(stats.rawDelta.nanoseconds, 0LL);
    EXPECT_EQ(stats.totalRealTime.nanoseconds, 0LL);
    EXPECT_EQ(stats.totalGameplayTime.nanoseconds, 0LL);
    EXPECT_FLOAT_EQ(stats.smoothedFps, 60.0f);
    EXPECT_EQ(clock.getGameplayClock().getTickCount(), 0ULL);
    EXPECT_EQ(clock.getPhysicsClock().getTickCount(), 0ULL);
}

TEST(EngineClockTest, TickIncrementsFrameIndexAndUpdatesRealTime)
{
    EngineClock clock;

    const FrameTimeStats& stats1 = clock.tick();
    EXPECT_EQ(stats1.frameIndex, 1ULL);
    EXPECT_GE(stats1.rawDelta.nanoseconds, 0LL);
    EXPECT_GE(stats1.totalRealTime.nanoseconds, 0LL);

    const FrameTimeStats& stats2 = clock.tick();
    EXPECT_EQ(stats2.frameIndex, 2ULL);
    EXPECT_GE(stats2.totalRealTime, stats1.totalRealTime);
}

TEST(EngineClockTest, FixedDeterministicSmoothingModeProducesConstantDelta)
{
    EngineClockDesc desc;
    desc.deterministicRate = FrameRate(60u, 1u);
    desc.smoothingMode = DeltaSmoothingMode::FixedDeterministic;

    EngineClock clock(desc);

    const Duration expectedDelta = FrameRate(60u, 1u).toFrameDuration();
    EXPECT_EQ(expectedDelta.nanoseconds, 16'666'666LL);

    for (gp::UInt32 i = 0u; i < 5u; ++i)
    {
        const FrameTimeStats& stats = clock.tick();
        EXPECT_EQ(stats.smoothedUnscaledDelta.nanoseconds, 16'666'666LL);
        EXPECT_FLOAT_EQ(stats.unscaledDeltaTime, expectedDelta.toSecondsFloat());
        EXPECT_FLOAT_EQ(stats.deltaTime, expectedDelta.toSecondsFloat());
        EXPECT_DOUBLE_EQ(stats.deltaTimeDouble, expectedDelta.toSeconds());
    }
}

TEST(EngineClockTest, ExponentialMovingAverageSmoothingMode)
{
    EngineClockDesc desc;
    desc.smoothingMode = DeltaSmoothingMode::ExponentialMovingAverage;

    EngineClock clock(desc);

    // Advance several frames; validates EMA arithmetic and updates without crash
    for (gp::UInt32 i = 0u; i < 5u; ++i)
    {
        const FrameTimeStats& stats = clock.tick();
        EXPECT_GE(stats.smoothedUnscaledDelta.nanoseconds, 0LL);
        EXPECT_GE(stats.unscaledDeltaTime, 0.0f);
    }
}

TEST(EngineClockTest, OutlierRejectedAverageSmoothingMode)
{
    EngineClockDesc desc;
    desc.smoothingMode = DeltaSmoothingMode::OutlierRejectedAverage;

    EngineClock clock(desc);

    // Advance 15 frames to fully exercise the 11-element rolling window and outlier trimming
    for (gp::UInt32 i = 0u; i < 15u; ++i)
    {
        const FrameTimeStats& stats = clock.tick();
        EXPECT_GE(stats.smoothedUnscaledDelta.nanoseconds, 0LL);
        EXPECT_GE(stats.smoothedFps, 0.0f);
    }
}

TEST(EngineClockTest, NoneSmoothingModePassesThroughClampedDelta)
{
    EngineClockDesc desc;
    desc.smoothingMode = DeltaSmoothingMode::None;

    EngineClock clock(desc);

    const FrameTimeStats& stats = clock.tick();
    EXPECT_EQ(stats.smoothedUnscaledDelta, stats.rawDelta);
}

TEST(EngineClockTest, PausingGameplayPausesSimulationSubtreeWhileUIAndRealTimeTick)
{
    EngineClockDesc desc;
    desc.deterministicRate = FrameRate(60u, 1u);
    desc.smoothingMode = DeltaSmoothingMode::FixedDeterministic;

    EngineClock clock(desc);

    // Warm up unpaused
    (void)clock.tick();
    EXPECT_GT(clock.getStats().deltaTime, 0.0f);

    // Pause Gameplay domain
    clock.getGameplayClock().setPaused(true);
    EXPECT_TRUE(clock.getGameplayClock().isLocallyPaused());
    EXPECT_TRUE(clock.getPhysicsClock().isEffectivelyPaused());
    EXPECT_TRUE(clock.getDomainClock(ClockDomain::Animation).isEffectivelyPaused());
    EXPECT_TRUE(clock.getDomainClock(ClockDomain::Audio).isEffectivelyPaused());
    EXPECT_FALSE(clock.getUIClock().isEffectivelyPaused());
    EXPECT_FALSE(clock.getDomainClock(ClockDomain::RealTime).isEffectivelyPaused());

    // Tick while Gameplay is paused
    const Duration gameplayElapsedBefore = clock.getStats().totalGameplayTime;
    const FrameTimeStats& pausedStats = clock.tick();

    // Gameplay delta must be 0
    EXPECT_EQ(pausedStats.gameplayDelta.nanoseconds, 0LL);
    EXPECT_FLOAT_EQ(pausedStats.deltaTime, 0.0f);
    EXPECT_DOUBLE_EQ(pausedStats.deltaTimeDouble, 0.0);
    EXPECT_EQ(pausedStats.totalGameplayTime, gameplayElapsedBefore);

    // Unscaled UI time must still advance
    EXPECT_GT(pausedStats.smoothedUnscaledDelta.nanoseconds, 0LL);
    EXPECT_GT(pausedStats.unscaledDeltaTime, 0.0f);
    EXPECT_EQ(clock.getUIClock().getDelta(), pausedStats.smoothedUnscaledDelta);

    // Physics, Animation, Audio deltas must be 0
    EXPECT_EQ(clock.getPhysicsClock().getDelta().nanoseconds, 0LL);
    EXPECT_EQ(clock.getDomainClock(ClockDomain::Animation).getDelta().nanoseconds, 0LL);
    EXPECT_EQ(clock.getDomainClock(ClockDomain::Audio).getDelta().nanoseconds, 0LL);

    // Unpause Gameplay
    clock.getGameplayClock().setPaused(false);
    const FrameTimeStats& resumedStats = clock.tick();
    EXPECT_GT(resumedStats.gameplayDelta.nanoseconds, 0LL);
    EXPECT_GT(resumedStats.deltaTime, 0.0f);
}

TEST(EngineClockTest, ScalingGameplayPropagatesToSimulationSubtree)
{
    EngineClockDesc desc;
    desc.deterministicRate = FrameRate(60u, 1u);   // ~16.666ms
    desc.smoothingMode = DeltaSmoothingMode::FixedDeterministic;

    EngineClock clock(desc);

    // Scale Gameplay domain to 0.5x half-speed
    clock.getGameplayClock().setLocalTimeScale(0.5);

    const FrameTimeStats& stats = clock.tick();

    // Smoothed unscaled delta is 16,666,666 ns
    EXPECT_EQ(stats.smoothedUnscaledDelta.nanoseconds, 16'666'666LL);

    // Scaled gameplay delta is 0.5 * 16,666,666 = 8,333,333 ns
    EXPECT_EQ(stats.gameplayDelta.nanoseconds, 8'333'333LL);
    EXPECT_EQ(clock.getPhysicsClock().getDelta().nanoseconds, 8'333'333LL);
    EXPECT_EQ(clock.getDomainClock(ClockDomain::Animation).getDelta().nanoseconds, 8'333'333LL);
    EXPECT_EQ(clock.getDomainClock(ClockDomain::Audio).getDelta().nanoseconds, 8'333'333LL);

    // UI and RealTime clocks remain unscaled (16,666,666 ns)
    EXPECT_EQ(clock.getUIClock().getDelta().nanoseconds, 16'666'666LL);
}

TEST(EngineClockTest, PhysicsFixedSubsteppingThroughTick)
{
    EngineClockDesc desc;
    // Tick at 30 FPS (~33.333ms per frame)
    desc.deterministicRate = FrameRate(30u, 1u);
    // Physics at 60 FPS (~16.666ms per step)
    desc.fixedPhysicsRate = FrameRate(60u, 1u);
    desc.smoothingMode = DeltaSmoothingMode::FixedDeterministic;

    EngineClock clock(desc);

    const FrameTimeStats& stats = clock.tick();

    // 33.333ms frame delta advances 60 Hz physics clock by 2 sub-steps
    EXPECT_EQ(stats.fixedStep.stepCount, 2u);
    EXPECT_EQ(stats.fixedStep.fixedDelta.nanoseconds, 16'666'666LL);
    EXPECT_FALSE(stats.fixedStep.wasClamped);
    EXPECT_GE(stats.fixedStep.interpolationAlpha, 0.0f);
    EXPECT_LT(stats.fixedStep.interpolationAlpha, 1.0f);
}

TEST(EngineClockTest, TelemetryConsistency)
{
    EngineClockDesc desc;
    desc.deterministicRate = FrameRate(60u, 1u);
    desc.smoothingMode = DeltaSmoothingMode::FixedDeterministic;

    EngineClock clock(desc);
    const FrameTimeStats& stats = clock.tick();

    EXPECT_FLOAT_EQ(stats.deltaTime, stats.gameplayDelta.toSecondsFloat());
    EXPECT_DOUBLE_EQ(stats.deltaTimeDouble, stats.gameplayDelta.toSeconds());
    EXPECT_FLOAT_EQ(stats.unscaledDeltaTime, stats.smoothedUnscaledDelta.toSecondsFloat());
    EXPECT_GT(stats.smoothedFps, 0.0f);
}

TEST(EngineClockTest, PaceFrameIntegration)
{
    EngineClock clock;
    clock.getFramePacer().setPacingMode(FramePacingMode::Uncapped);

    // Uncapped pacing should return immediately without deadlock or delay
    clock.paceFrame();
    clock.paceFrame(0.0);
}

}   // namespace gp::time::tests
