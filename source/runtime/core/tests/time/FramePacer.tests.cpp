// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "time/FramePacer.hpp"
#include <gtest/gtest.h>

namespace gp::time::tests
{

static_assert(FramePacer::kInitialOvershootMarginNs == 500'000LL, "Initial margin must be 500us.");
static_assert(FramePacer::kMinOvershootMarginNs == 150'000LL, "Min margin must be 150us.");
static_assert(FramePacer::kMaxOvershootMarginNs == 2'500'000LL, "Max margin must be 2.5ms.");

constexpr bool testConstexprFramePacerBasics()
{
    FramePacer pacer;
    if (pacer.getTargetFrameRate().numerator != 120u || pacer.getTargetFrameRate().denominator != 1u)
    {
        return false;
    }
    if (pacer.getPacingMode() != FramePacingMode::HybridSleepAndSpin)
    {
        return false;
    }
    return true;
}

static_assert(testConstexprFramePacerBasics(), "Constexpr FramePacer basic check failed.");

TEST(FramePacerTest, DefaultConstruction)
{
    const FramePacer pacer;

    EXPECT_EQ(pacer.getTargetFrameRate().numerator, 120u);
    EXPECT_EQ(pacer.getTargetFrameRate().denominator, 1u);
    EXPECT_EQ(pacer.getPacingMode(), FramePacingMode::HybridSleepAndSpin);
}

TEST(FramePacerTest, ParameterizedConstruction)
{
    const FramePacer pacer(FrameRate(60u, 1u), FramePacingMode::SpinOnly);

    EXPECT_EQ(pacer.getTargetFrameRate().numerator, 60u);
    EXPECT_EQ(pacer.getTargetFrameRate().denominator, 1u);
    EXPECT_EQ(pacer.getPacingMode(), FramePacingMode::SpinOnly);
}

TEST(FramePacerTest, SetTargetFrameRate)
{
    FramePacer pacer;

    pacer.setTargetFrameRate(FrameRate(144u, 1u));
    EXPECT_EQ(pacer.getTargetFrameRate().numerator, 144u);
    EXPECT_EQ(pacer.getTargetFrameRate().denominator, 1u);

    pacer.setTargetFrameRate(FrameRate(0u, 1u));
    EXPECT_EQ(pacer.getTargetFrameRate().numerator, 0u);
}

TEST(FramePacerTest, SetTargetFps)
{
    FramePacer pacer;

    // Normal positive FPS
    pacer.setTargetFps(60.0);
    EXPECT_DOUBLE_EQ(pacer.getTargetFrameRate().toHz(), 60.0);

    pacer.setTargetFps(144.0);
    EXPECT_DOUBLE_EQ(pacer.getTargetFrameRate().toHz(), 144.0);

    // Zero or negative FPS disables pacing (sets 0/1)
    pacer.setTargetFps(0.0);
    EXPECT_EQ(pacer.getTargetFrameRate().numerator, 0u);

    pacer.setTargetFps(-10.0);
    EXPECT_EQ(pacer.getTargetFrameRate().numerator, 0u);
}

TEST(FramePacerTest, SetAndGetPacingMode)
{
    FramePacer pacer;

    pacer.setPacingMode(FramePacingMode::Uncapped);
    EXPECT_EQ(pacer.getPacingMode(), FramePacingMode::Uncapped);

    pacer.setPacingMode(FramePacingMode::SleepOnly);
    EXPECT_EQ(pacer.getPacingMode(), FramePacingMode::SleepOnly);

    pacer.setPacingMode(FramePacingMode::SpinOnly);
    EXPECT_EQ(pacer.getPacingMode(), FramePacingMode::SpinOnly);

    pacer.setPacingMode(FramePacingMode::HybridSleepAndSpin);
    EXPECT_EQ(pacer.getPacingMode(), FramePacingMode::HybridSleepAndSpin);
}

TEST(FramePacerTest, PaceUntilDeadlineUncappedReturnsImmediately)
{
    FramePacer pacer(FrameRate(60u, 1u), FramePacingMode::Uncapped);

    const Timestamp start(gp::platform::Time::nowNanoseconds());
    const Timestamp end = pacer.paceUntilDeadline(start);

    EXPECT_GE(end, start);
    // Uncapped should return immediately without waiting for 16.666ms
    const Duration elapsed = end - start;
    EXPECT_LT(elapsed.toMilliseconds(), 2.0);
}

TEST(FramePacerTest, PaceUntilDeadlineDisabledTargetReturnsImmediately)
{
    FramePacer pacer;
    pacer.setTargetFps(0.0);   // Duration is zero

    const Timestamp start(gp::platform::Time::nowNanoseconds());
    const Timestamp end = pacer.paceUntilDeadline(start);

    EXPECT_GE(end, start);
    const Duration elapsed = end - start;
    EXPECT_LT(elapsed.toMilliseconds(), 2.0);
}

TEST(FramePacerTest, PaceUntilDeadlineAlreadyExceededReturnsImmediately)
{
    FramePacer pacer(FrameRate(60u, 1u), FramePacingMode::HybridSleepAndSpin);

    // Frame start was 50ms ago; deadline (start + 16.666ms) is already far in the past
    const Timestamp pastStart(gp::platform::Time::nowNanoseconds() - 50'000'000ULL);
    const Timestamp end = pacer.paceUntilDeadline(pastStart);

    EXPECT_GT(end, pastStart);
    // Should not sleep because now >= deadline
    const Timestamp now(gp::platform::Time::nowNanoseconds());
    const Duration diff = now - end;
    EXPECT_LT(diff.toMilliseconds(), 2.0);
}

TEST(FramePacerTest, PaceUntilDeadlineSpinOnlyMeetsDeadline)
{
    // Target 2,000 Hz = 500 microseconds = 0.5 ms
    FramePacer pacer(FrameRate(2'000u, 1u), FramePacingMode::SpinOnly);

    const Timestamp start(gp::platform::Time::nowNanoseconds());
    const Timestamp deadline = start + pacer.getTargetFrameRate().toFrameDuration();
    const Timestamp end = pacer.paceUntilDeadline(start);

    EXPECT_GE(end, deadline);
}

TEST(FramePacerTest, PaceUntilDeadlineHybridSleepAndSpinMeetsDeadline)
{
    // Target 500 Hz = 2 milliseconds
    FramePacer pacer(FrameRate(500u, 1u), FramePacingMode::HybridSleepAndSpin);

    const Timestamp start(gp::platform::Time::nowNanoseconds());
    const Timestamp deadline = start + pacer.getTargetFrameRate().toFrameDuration();
    const Timestamp end = pacer.paceUntilDeadline(start);

    EXPECT_GE(end, deadline);
}

TEST(FramePacerTest, MoveConstructionAndAssignment)
{
    FramePacer original(FrameRate(144u, 1u), FramePacingMode::SpinOnly);

    FramePacer moved(std::move(original));
    EXPECT_EQ(moved.getTargetFrameRate().numerator, 144u);
    EXPECT_EQ(moved.getPacingMode(), FramePacingMode::SpinOnly);

    FramePacer assigned;
    assigned = std::move(moved);
    EXPECT_EQ(assigned.getTargetFrameRate().numerator, 144u);
    EXPECT_EQ(assigned.getPacingMode(), FramePacingMode::SpinOnly);
}

}   // namespace gp::time::tests
