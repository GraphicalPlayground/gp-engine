// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "time/SimulationClock.hpp"
#include <gtest/gtest.h>

namespace gp::time::tests
{

constexpr bool testConstexprSimulationClockBasics()
{
    SimulationClock clock;
    if (clock.getParent() != nullptr)
    {
        return false;
    }
    if (clock.getLocalTimeScale() != 1.0)
    {
        return false;
    }
    if (clock.getEffectiveTimeScale() != 1.0)
    {
        return false;
    }
    if (clock.isLocallyPaused() || clock.isEffectivelyPaused())
    {
        return false;
    }
    if (clock.getTickCount() != 0ULL)
    {
        return false;
    }
    if (clock.getTotalElapsed().nanoseconds != 0LL)
    {
        return false;
    }
    return true;
}

static_assert(testConstexprSimulationClockBasics(), "Constexpr SimulationClock basic check failed.");

TEST(SimulationClockTest, DefaultConstruction)
{
    const SimulationClock clock;

    EXPECT_EQ(clock.getParent(), nullptr);
    EXPECT_DOUBLE_EQ(clock.getLocalTimeScale(), 1.0);
    EXPECT_DOUBLE_EQ(clock.getEffectiveTimeScale(), 1.0);
    EXPECT_FALSE(clock.isLocallyPaused());
    EXPECT_FALSE(clock.isEffectivelyPaused());
    EXPECT_EQ(clock.getFixedStepDuration().nanoseconds, 16'666'666LL);
    EXPECT_EQ(clock.getTotalElapsed().nanoseconds, 0LL);
    EXPECT_DOUBLE_EQ(clock.getTotalSeconds(), 0.0);
    EXPECT_EQ(clock.getDelta().nanoseconds, 0LL);
    EXPECT_FLOAT_EQ(clock.getDeltaSeconds(), 0.0f);
    EXPECT_EQ(clock.getTickCount(), 0ULL);
}

TEST(SimulationClockTest, ParameterizedConstruction)
{
    const SimulationClock halfSpeed(0.5);
    EXPECT_DOUBLE_EQ(halfSpeed.getLocalTimeScale(), 0.5);
    EXPECT_DOUBLE_EQ(halfSpeed.getEffectiveTimeScale(), 0.5);
    EXPECT_EQ(halfSpeed.getParent(), nullptr);

    // Negative initial scale must clamp to 0.0
    const SimulationClock negativeSpeed(-1.5);
    EXPECT_DOUBLE_EQ(negativeSpeed.getLocalTimeScale(), 0.0);
    EXPECT_DOUBLE_EQ(negativeSpeed.getEffectiveTimeScale(), 0.0);

    const SimulationClock parentClock(2.0);
    const SimulationClock childClock(0.5, &parentClock);
    EXPECT_EQ(childClock.getParent(), &parentClock);
    EXPECT_DOUBLE_EQ(childClock.getLocalTimeScale(), 0.5);
    EXPECT_DOUBLE_EQ(childClock.getEffectiveTimeScale(), 1.0);
}

TEST(SimulationClockTest, LocalTimeScaleClamping)
{
    SimulationClock clock;

    clock.setLocalTimeScale(2.5);
    EXPECT_DOUBLE_EQ(clock.getLocalTimeScale(), 2.5);
    EXPECT_DOUBLE_EQ(clock.getEffectiveTimeScale(), 2.5);

    // Negative scale must clamp to 0.0
    clock.setLocalTimeScale(-0.5);
    EXPECT_DOUBLE_EQ(clock.getLocalTimeScale(), 0.0);
    EXPECT_DOUBLE_EQ(clock.getEffectiveTimeScale(), 0.0);
}

TEST(SimulationClockTest, HierarchyTimeDilationMultiplication)
{
    SimulationClock grandparent(2.0);
    SimulationClock parent(1.5, &grandparent);
    SimulationClock child(0.5, &parent);

    // Grandparent = 2.0
    EXPECT_DOUBLE_EQ(grandparent.getEffectiveTimeScale(), 2.0);

    // Parent = 1.5 * 2.0 = 3.0
    EXPECT_DOUBLE_EQ(parent.getEffectiveTimeScale(), 3.0);

    // Child = 0.5 * 1.5 * 2.0 = 1.5
    EXPECT_DOUBLE_EQ(child.getEffectiveTimeScale(), 1.5);

    // Change grandparent time scale; verifies propagation down the tree
    grandparent.setLocalTimeScale(1.0);
    EXPECT_DOUBLE_EQ(grandparent.getEffectiveTimeScale(), 1.0);
    EXPECT_DOUBLE_EQ(parent.getEffectiveTimeScale(), 1.5);
    EXPECT_DOUBLE_EQ(child.getEffectiveTimeScale(), 0.75);
}

TEST(SimulationClockTest, HierarchyParentAttachmentAndDetachment)
{
    SimulationClock parent(2.0);
    SimulationClock child(0.5);

    EXPECT_EQ(child.getParent(), nullptr);
    EXPECT_DOUBLE_EQ(child.getEffectiveTimeScale(), 0.5);

    child.setParent(&parent);
    EXPECT_EQ(child.getParent(), &parent);
    EXPECT_DOUBLE_EQ(child.getEffectiveTimeScale(), 1.0);

    // Self-parenting must be rejected
    child.setParent(&child);
    EXPECT_EQ(child.getParent(), &parent);

    // Detaching restores independent local scale
    child.setParent(nullptr);
    EXPECT_EQ(child.getParent(), nullptr);
    EXPECT_DOUBLE_EQ(child.getEffectiveTimeScale(), 0.5);
}

TEST(SimulationClockTest, PauseAndEffectivePausePropagation)
{
    SimulationClock root;
    SimulationClock child(1.0, &root);
    SimulationClock grandchild(1.0, &child);

    // Initial unpaused state
    EXPECT_FALSE(root.isLocallyPaused());
    EXPECT_FALSE(root.isEffectivelyPaused());
    EXPECT_FALSE(child.isLocallyPaused());
    EXPECT_FALSE(child.isEffectivelyPaused());
    EXPECT_FALSE(grandchild.isLocallyPaused());
    EXPECT_FALSE(grandchild.isEffectivelyPaused());

    // Pause root: propagates to child and grandchild
    root.setPaused(true);
    EXPECT_TRUE(root.isLocallyPaused());
    EXPECT_TRUE(root.isEffectivelyPaused());
    EXPECT_FALSE(child.isLocallyPaused());
    EXPECT_TRUE(child.isEffectivelyPaused());
    EXPECT_FALSE(grandchild.isLocallyPaused());
    EXPECT_TRUE(grandchild.isEffectivelyPaused());
    EXPECT_DOUBLE_EQ(child.getEffectiveTimeScale(), 0.0);
    EXPECT_DOUBLE_EQ(grandchild.getEffectiveTimeScale(), 0.0);

    // Unpause root: restores tree
    root.setPaused(false);
    EXPECT_FALSE(root.isEffectivelyPaused());
    EXPECT_FALSE(child.isEffectivelyPaused());
    EXPECT_FALSE(grandchild.isEffectivelyPaused());

    // Pause child only: root unaffected, grandchild paused
    child.setPaused(true);
    EXPECT_FALSE(root.isEffectivelyPaused());
    EXPECT_TRUE(child.isLocallyPaused());
    EXPECT_TRUE(child.isEffectivelyPaused());
    EXPECT_FALSE(grandchild.isLocallyPaused());
    EXPECT_TRUE(grandchild.isEffectivelyPaused());

    // Both root and child paused: unpausing root leaves child and grandchild paused
    root.setPaused(true);
    root.setPaused(false);
    EXPECT_FALSE(root.isEffectivelyPaused());
    EXPECT_TRUE(child.isEffectivelyPaused());
    EXPECT_TRUE(grandchild.isEffectivelyPaused());
}

TEST(SimulationClockTest, AdvanceNormalUnpaused)
{
    SimulationClock clock;
    const Duration dt = Duration::fromMilliseconds(16.0);

    const Duration delta1 = clock.advance(dt);
    EXPECT_EQ(delta1.nanoseconds, 16'000'000LL);
    EXPECT_EQ(clock.getDelta().nanoseconds, 16'000'000LL);
    EXPECT_FLOAT_EQ(clock.getDeltaSeconds(), 0.016f);
    EXPECT_EQ(clock.getTotalElapsed().nanoseconds, 16'000'000LL);
    EXPECT_DOUBLE_EQ(clock.getTotalSeconds(), 0.016);
    EXPECT_EQ(clock.getTickCount(), 1ULL);

    const Duration delta2 = clock.advance(dt);
    EXPECT_EQ(delta2.nanoseconds, 16'000'000LL);
    EXPECT_EQ(clock.getTotalElapsed().nanoseconds, 32'000'000LL);
    EXPECT_DOUBLE_EQ(clock.getTotalSeconds(), 0.032);
    EXPECT_EQ(clock.getTickCount(), 2ULL);
}

TEST(SimulationClockTest, AdvanceScaled)
{
    SimulationClock clock(0.5);
    const Duration dt = Duration::fromMilliseconds(20.0);

    const Duration scaledDelta = clock.advance(dt);
    EXPECT_EQ(scaledDelta.nanoseconds, 10'000'000LL);
    EXPECT_EQ(clock.getDelta().nanoseconds, 10'000'000LL);
    EXPECT_FLOAT_EQ(clock.getDeltaSeconds(), 0.01f);
    EXPECT_EQ(clock.getTotalElapsed().nanoseconds, 10'000'000LL);
    EXPECT_EQ(clock.getTickCount(), 1ULL);
}

TEST(SimulationClockTest, AdvanceZeroScale)
{
    SimulationClock clock(0.0);
    const Duration dt = Duration::fromMilliseconds(16.0);

    const Duration zeroDelta = clock.advance(dt);
    EXPECT_EQ(zeroDelta.nanoseconds, 0LL);
    EXPECT_EQ(clock.getDelta().nanoseconds, 0LL);
    EXPECT_FLOAT_EQ(clock.getDeltaSeconds(), 0.0f);
    EXPECT_EQ(clock.getTotalElapsed().nanoseconds, 0LL);
    EXPECT_EQ(clock.getTickCount(), 1ULL);
}

TEST(SimulationClockTest, AdvanceWhileEffectivelyPausedReturnsZero)
{
    SimulationClock parent;
    SimulationClock child(1.0, &parent);

    parent.setPaused(true);

    const Duration dt = Duration::fromMilliseconds(16.0);
    const Duration delta = child.advance(dt);

    EXPECT_EQ(delta.nanoseconds, 0LL);
    EXPECT_EQ(child.getDelta().nanoseconds, 0LL);
    EXPECT_FLOAT_EQ(child.getDeltaSeconds(), 0.0f);
    EXPECT_EQ(child.getTotalElapsed().nanoseconds, 0LL);
    EXPECT_EQ(child.getTickCount(), 0ULL);
}

TEST(SimulationClockTest, SingleStepExecution)
{
    SimulationClock clock;
    clock.configureFixedStep(FrameRate(60u, 1u));
    clock.setPaused(true);

    // Initial paused state
    EXPECT_TRUE(clock.isEffectivelyPaused());
    EXPECT_EQ(clock.advance(Duration::fromMilliseconds(16.0)).nanoseconds, 0LL);
    EXPECT_EQ(clock.getTickCount(), 0ULL);

    // Request single step
    clock.requestSingleStep();
    EXPECT_FALSE(clock.isEffectivelyPaused());   // Temporarily unpaused for 1 step

    // Advance must execute exactly one fixed step duration (16,666,666 ns)
    const Duration stepDelta = clock.advance(Duration::fromMilliseconds(50.0));
    EXPECT_EQ(stepDelta.nanoseconds, 16'666'666LL);
    EXPECT_EQ(clock.getDelta().nanoseconds, 16'666'666LL);
    EXPECT_EQ(clock.getTotalElapsed().nanoseconds, 16'666'666LL);
    EXPECT_EQ(clock.getTickCount(), 1ULL);

    // Clock must automatically re-pause after the single step is consumed
    EXPECT_TRUE(clock.isEffectivelyPaused());
    const Duration postStepDelta = clock.advance(Duration::fromMilliseconds(16.0));
    EXPECT_EQ(postStepDelta.nanoseconds, 0LL);
    EXPECT_EQ(clock.getTickCount(), 1ULL);
}

TEST(SimulationClockTest, SingleStepWhenNotPausedIsNoOp)
{
    SimulationClock clock;
    clock.requestSingleStep();   // Should do nothing because clock is unpaused

    const Duration dt = Duration::fromMilliseconds(10.0);
    const Duration delta = clock.advance(dt);
    EXPECT_EQ(delta.nanoseconds, 10'000'000LL);
}

TEST(SimulationClockTest, UnpausingClearsSingleStepRequest)
{
    SimulationClock clock;
    clock.setPaused(true);
    clock.requestSingleStep();
    EXPECT_FALSE(clock.isEffectivelyPaused());

    clock.setPaused(false);
    EXPECT_FALSE(clock.isEffectivelyPaused());

    // Advancing should now follow normal unpaused logic, not single-step fixed delta
    const Duration dt = Duration::fromMilliseconds(5.0);
    const Duration delta = clock.advance(dt);
    EXPECT_EQ(delta.nanoseconds, 5'000'000LL);
}

TEST(SimulationClockTest, FixedStepAccumulatorSubStepDraining)
{
    SimulationClock clock;
    // 60 Hz -> fixedDelta = 16,666,666 ns
    clock.configureFixedStep(FrameRate(60u, 1u), 8u);

    // Advance with 10ms (< 16.666ms): 0 sub-steps, interpolationAlpha approx 0.6
    clock.advance(Duration::fromMilliseconds(10.0));
    FixedStepResult result = clock.consumeFixedSteps();
    EXPECT_EQ(result.stepCount, 0u);
    EXPECT_EQ(result.fixedDelta.nanoseconds, 16'666'666LL);
    EXPECT_NEAR(result.interpolationAlpha, 10'000'000.0 / 16'666'666.0, 1e-4);
    EXPECT_FALSE(result.wasClamped);

    // Advance another 10ms (total accumulated = 20ms >= 16.666ms): 1 sub-step
    clock.advance(Duration::fromMilliseconds(10.0));
    result = clock.consumeFixedSteps();
    EXPECT_EQ(result.stepCount, 1u);
    EXPECT_FALSE(result.wasClamped);
    // Remainder = 20,000,000 - 16,666,666 = 3,333,334 ns
    EXPECT_NEAR(result.interpolationAlpha, 3'333'334.0 / 16'666'666.0, 1e-4);
}

TEST(SimulationClockTest, FixedStepAccumulatorMultipleSteps)
{
    SimulationClock clock;
    clock.configureFixedStep(FrameRate(60u, 1u), 8u);

    // 50ms = 3 steps (3 * 16.666ms = 49.999998ms) + remainder
    clock.advance(Duration::fromMilliseconds(50.0));
    const FixedStepResult result = clock.consumeFixedSteps();

    EXPECT_EQ(result.stepCount, 3u);
    EXPECT_FALSE(result.wasClamped);
    EXPECT_LT(result.interpolationAlpha, 0.01f);
}

TEST(SimulationClockTest, FixedStepAccumulatorSpiralOfDeathClamping)
{
    SimulationClock clock;
    // Configure with maximum 3 sub-steps allowed per frame
    clock.configureFixedStep(FrameRate(60u, 1u), 3u);

    // Advance with a huge 200ms hitch (~12 steps)
    clock.advance(Duration::fromMilliseconds(200.0));
    const FixedStepResult result = clock.consumeFixedSteps();

    // Must be clamped to maxSubSteps = 3
    EXPECT_EQ(result.stepCount, 3u);
    EXPECT_TRUE(result.wasClamped);
    EXPECT_GE(result.interpolationAlpha, 0.0f);
    EXPECT_LT(result.interpolationAlpha, 1.0f);
}

TEST(SimulationClockTest, ConfigureFixedStepClampsZeroMaxSubSteps)
{
    SimulationClock clock;
    // maxSubSteps = 0 must clamp to 1
    clock.configureFixedStep(FrameRate(60u, 1u), 0u);

    clock.advance(Duration::fromMilliseconds(50.0));
    const FixedStepResult result = clock.consumeFixedSteps();
    EXPECT_EQ(result.stepCount, 1u);
    EXPECT_TRUE(result.wasClamped);
}

TEST(SimulationClockTest, ResetRestoresCleanState)
{
    SimulationClock clock;
    clock.advance(Duration::fromMilliseconds(100.0));
    clock.setPaused(true);
    clock.requestSingleStep();

    EXPECT_GT(clock.getTotalElapsed().nanoseconds, 0LL);
    EXPECT_GT(clock.getTickCount(), 0ULL);

    clock.reset();

    EXPECT_EQ(clock.getTotalElapsed().nanoseconds, 0LL);
    EXPECT_DOUBLE_EQ(clock.getTotalSeconds(), 0.0);
    EXPECT_EQ(clock.getDelta().nanoseconds, 0LL);
    EXPECT_FLOAT_EQ(clock.getDeltaSeconds(), 0.0f);
    EXPECT_EQ(clock.getTickCount(), 0ULL);
}

TEST(SimulationClockTest, MoveConstructionAndAssignment)
{
    SimulationClock original;
    original.setLocalTimeScale(1.75);
    original.advance(Duration::fromMilliseconds(40.0));

    SimulationClock moved(std::move(original));
    EXPECT_DOUBLE_EQ(moved.getLocalTimeScale(), 1.75);
    EXPECT_EQ(moved.getTickCount(), 1ULL);
    EXPECT_EQ(moved.getTotalElapsed().nanoseconds, 70'000'000LL);

    SimulationClock assigned;
    assigned = std::move(moved);
    EXPECT_DOUBLE_EQ(assigned.getLocalTimeScale(), 1.75);
    EXPECT_EQ(assigned.getTickCount(), 1ULL);
    EXPECT_EQ(assigned.getTotalElapsed().nanoseconds, 70'000'000LL);
}

}   // namespace gp::time::tests
