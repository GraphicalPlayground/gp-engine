// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "templates/NumericLimits.hpp"
#include "CoreMinimal.hpp"
#include "maths/base/FloatingPoint.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace gp::tests
{

TEST(NumericLimitsTest, IntegralBoundsAndPropertiesMatchFixedWidthTypes)
{
    static_assert(NumericLimits<UInt8>::kIsSpecialized);
    static_assert(!NumericLimits<UInt8>::kIsSigned);
    static_assert(NumericLimits<UInt8>::kIsInteger);
    static_assert(NumericLimits<UInt8>::min() == 0u);
    static_assert(NumericLimits<UInt8>::lowest() == 0u);
    static_assert(NumericLimits<UInt8>::max() == 0xFFu);

    static_assert(NumericLimits<UInt16>::max() == 0xFFFFu);
    static_assert(NumericLimits<UInt32>::max() == 0xFFFFFFFFu);
    static_assert(NumericLimits<UInt64>::max() == 0xFFFFFFFFFFFFFFFFull);

    static_assert(NumericLimits<Int8>::kIsSigned);
    static_assert(NumericLimits<Int8>::min() == -128);
    static_assert(NumericLimits<Int8>::lowest() == -128);
    static_assert(NumericLimits<Int8>::max() == 127);

    static_assert(NumericLimits<Int32>::min() == NumericLimits<Int32>::lowest());
    static_assert(NumericLimits<Int32>::max() == 2147483647);
    static_assert(NumericLimits<Int64>::max() == 9223372036854775807ll);

    // CV-qualifiers are stripped transparently
    static_assert(NumericLimits<const UInt32>::max() == NumericLimits<UInt32>::max());
    static_assert(NumericLimits<volatile Int16>::lowest() == NumericLimits<Int16>::lowest());

    EXPECT_EQ(NumericLimits<UInt8>::max(), std::numeric_limits<UInt8>::max());
    EXPECT_EQ(NumericLimits<Int32>::min(), std::numeric_limits<Int32>::min());
    EXPECT_EQ(NumericLimits<UInt64>::max(), std::numeric_limits<UInt64>::max());
}

TEST(NumericLimitsTest, FloatingPointDistinguishesMinPositiveFromLowestFinite)
{
    static_assert(NumericLimits<Float32>::kIsSpecialized);
    static_assert(NumericLimits<Float32>::kIsSigned);
    static_assert(!NumericLimits<Float32>::kIsInteger);
    static_assert(NumericLimits<Float32>::kIsIEC559);
    static_assert(NumericLimits<Float32>::kHasInfinity);
    static_assert(NumericLimits<Float32>::kHasQuietNaN);
    static_assert(NumericLimits<Float32>::kDigits == 24);

    static_assert(NumericLimits<Float32>::min() > 0.0f);
    static_assert(NumericLimits<Float32>::lowest() < 0.0f);
    static_assert(NumericLimits<Float32>::lowest() == -NumericLimits<Float32>::max());
    static_assert(NumericLimits<Float32>::epsilon() > 0.0f);
    static_assert(!math::isFinite(NumericLimits<Float32>::infinity()));
    static_assert(math::isNaN(NumericLimits<Float32>::quietNaN()));

    static_assert(NumericLimits<Float64>::kIsIEC559);
    static_assert(NumericLimits<Float64>::kDigits == 53);
    static_assert(NumericLimits<Float64>::lowest() == -NumericLimits<Float64>::max());
    static_assert(NumericLimits<Float64>::denormMin() > 0.0);

    EXPECT_FLOAT_EQ(NumericLimits<Float32>::min(), std::numeric_limits<Float32>::min());
    EXPECT_FLOAT_EQ(NumericLimits<Float32>::max(), std::numeric_limits<Float32>::max());
    EXPECT_FLOAT_EQ(NumericLimits<Float32>::lowest(), std::numeric_limits<Float32>::lowest());
    EXPECT_DOUBLE_EQ(NumericLimits<Float64>::epsilon(), std::numeric_limits<Float64>::epsilon());
    EXPECT_TRUE(math::isNaN(NumericLimits<Float32>::signalingNaN()));
    EXPECT_FALSE(math::isFinite(NumericLimits<Float64>::infinity()));
}

TEST(NumericLimitsTest, NonSpecializedCustomStructReportsUnspecialized)
{
    struct CustomOpaque
    {
        Int32 value = 0;
    };

    static_assert(!NumericLimits<CustomOpaque>::kIsSpecialized);
    EXPECT_FALSE(NumericLimits<CustomOpaque>::kIsSpecialized);
}

}   // namespace gp::tests
