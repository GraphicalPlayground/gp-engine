// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/base/Interpolation.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::math::tests
{

TEST(InterpolationTest, GetMappedRangeValueClampedMapsAndClampsAscendingAndDescendingRanges)
{
    // Standard ascending range
    EXPECT_FLOAT_EQ(getMappedRangeValueClamped(0.0f, 10.0f, 100.0f, 200.0f, 5.0f), 150.0f);
    EXPECT_FLOAT_EQ(getMappedRangeValueClamped(0.0f, 10.0f, 100.0f, 200.0f, -5.0f), 100.0f);
    EXPECT_FLOAT_EQ(getMappedRangeValueClamped(0.0f, 10.0f, 100.0f, 200.0f, 15.0f), 200.0f);

    // Reversed input range (e.g., Good > Bad in shader telemetry metrics)
    EXPECT_FLOAT_EQ(getMappedRangeValueClamped(10.0f, 0.0f, 0.0f, 1.0f, 2.0f), 0.8f);
    EXPECT_FLOAT_EQ(getMappedRangeValueClamped(10.0f, 0.0f, 0.0f, 1.0f, 12.0f), 0.0f);
    EXPECT_FLOAT_EQ(getMappedRangeValueClamped(10.0f, 0.0f, 0.0f, 1.0f, -2.0f), 1.0f);

    // Reversed output range
    EXPECT_DOUBLE_EQ(getMappedRangeValueClamped(0.0, 4.0, 100.0, 0.0, 1.0), 75.0);

    // Degenerate input range falls back safely to outMin
    EXPECT_FLOAT_EQ(getMappedRangeValueClamped(5.0f, 5.0f, 42.0f, 99.0f, 7.0f), 42.0f);

    static_assert(getMappedRangeValueClamped(0.0f, 10.0f, 0.0f, 100.0f, 2.5f) == 25.0f);
    static_assert(getMappedRangeValueClamped(10.0f, 0.0f, 0.0f, 1.0f, 2.0f) > 0.79f);
}

}   // namespace gp::math::tests
