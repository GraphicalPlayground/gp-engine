// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "time/TimeTypes.hpp"
#include <gtest/gtest.h>
#include <type_traits>   // IWYU pragma: keep

namespace gp::time::tests
{

static_assert(kNanosecondsPerMicrosecond == 1'000LL, "1us must be 1,000ns.");
static_assert(kNanosecondsPerMillisecond == 1'000'000LL, "1ms must be 1,000,000ns.");
static_assert(kNanosecondsPerSecond == 1'000'000'000LL, "1s must be 1,000,000,000ns.");
static_assert(kNanosecondsPerMinute == 60LL * 1'000'000'000LL, "1min must be 60s in ns.");
static_assert(kNanosecondsPerHour == 3'600LL * 1'000'000'000LL, "1h must be 3600s in ns.");

// Duration constexpr assertions
static_assert(Duration{}.nanoseconds == 0LL, "Default Duration must be 0ns.");
static_assert(Duration::zero().nanoseconds == 0LL, "Duration::zero() must be 0ns.");
static_assert(Duration::fromNanoseconds(500LL).nanoseconds == 500LL, "fromNanoseconds must preserve value.");
static_assert(Duration::fromMicroseconds(42LL).nanoseconds == 42'000LL, "fromMicroseconds must scale by 1,000.");
static_assert(Duration::fromMilliseconds(1.5).nanoseconds == 1'500'000LL, "fromMilliseconds must scale by 1,000,000.");
static_assert(Duration::fromSeconds(2.0).nanoseconds == 2'000'000'000LL, "fromSeconds must scale by 1,000,000,000.");
static_assert(Duration(100LL) + Duration(50LL) == Duration(150LL), "Duration operator+ failed.");
static_assert(Duration(100LL) - Duration(40LL) == Duration(60LL), "Duration operator- failed.");
static_assert(-Duration(100LL) == Duration(-100LL), "Duration unary operator- failed.");
static_assert(Duration(100LL) > Duration(50LL), "Duration operator> failed.");
static_assert(Duration(50LL) < Duration(100LL), "Duration operator< failed.");
static_assert(Duration(100LL) == Duration(100LL), "Duration operator== failed.");
static_assert(Duration(100LL) != Duration(50LL), "Duration operator!= failed.");
static_assert(Duration(100LL).scaled(0.5) == Duration(50LL), "Duration::scaled failed.");

// Timestamp constexpr assertions
static_assert(Timestamp{}.nanoseconds == 0ULL, "Default Timestamp must be 0ns.");
static_assert(Timestamp(1000ULL).nanoseconds == 1000ULL, "Explicit Timestamp must preserve value.");
static_assert(Timestamp(2000ULL) - Timestamp(500ULL) == Duration(1500LL), "Timestamp difference failed.");
static_assert(Timestamp(500ULL) - Timestamp(2000ULL) == Duration(-1500LL), "Timestamp negative difference failed.");
static_assert(Timestamp(1000ULL) + Duration(500LL) == Timestamp(1500ULL), "Timestamp advance failed.");
static_assert(Timestamp(1000ULL) + Duration(-400LL) == Timestamp(600ULL), "Timestamp rewind failed.");
static_assert(Timestamp(1000ULL) > Timestamp(500ULL), "Timestamp operator> failed.");
static_assert(Timestamp(500ULL) < Timestamp(1000ULL), "Timestamp operator< failed.");

// FrameRate constexpr assertions
static_assert(FrameRate{}.numerator == 60u && FrameRate{}.denominator == 1u, "Default FrameRate must be 60/1.");
static_assert(FrameRate(60u, 0u).denominator == 1u, "Denominator 0 must be clamped to 1.");
static_assert(
    FrameRate(60u, 1u).toFrameDuration().nanoseconds == 16'666'666LL, "60 FPS frame duration must be 16.666ms."
);
static_assert(FrameRate(0u, 1u).toFrameDuration().nanoseconds == 0LL, "0 FPS frame duration must be 0ns.");
static_assert(FrameRate(60u, 1u).isValid(), "60 FPS FrameRate must be valid.");
static_assert(!FrameRate(0u, 1u).isValid(), "0 FPS FrameRate must not be valid.");

TEST(DurationTest, DefaultAndExplicitConstruction)
{
    const Duration defaultDur;
    EXPECT_EQ(defaultDur.nanoseconds, 0LL);

    const Duration explicitDur(123456789LL);
    EXPECT_EQ(explicitDur.nanoseconds, 123456789LL);

    const Duration zeroDur = Duration::zero();
    EXPECT_EQ(zeroDur.nanoseconds, 0LL);
}

TEST(DurationTest, FactoryConversions)
{
    EXPECT_EQ(Duration::fromNanoseconds(42LL).nanoseconds, 42LL);
    EXPECT_EQ(Duration::fromMicroseconds(10LL).nanoseconds, 10'000LL);
    EXPECT_EQ(Duration::fromMilliseconds(2.5).nanoseconds, 2'500'000LL);
    EXPECT_EQ(Duration::fromSeconds(1.25).nanoseconds, 1'250'000'000LL);
}

TEST(DurationTest, ConversionMethods)
{
    const Duration dur = Duration::fromSeconds(1.5);

    EXPECT_DOUBLE_EQ(dur.toSeconds(), 1.5);
    EXPECT_FLOAT_EQ(dur.toSecondsFloat(), 1.5f);
    EXPECT_DOUBLE_EQ(dur.toMilliseconds(), 1500.0);
    EXPECT_EQ(dur.toMicrosecondsInt(), 1'500'000LL);
    EXPECT_EQ(dur.toMillisecondsInt(), 1'500LL);
}

TEST(DurationTest, FractionalConversionPrecision)
{
    const Duration dur(1'234'567LL);   // 1.234567 ms

    EXPECT_NEAR(dur.toMilliseconds(), 1.234567, 1e-9);
    EXPECT_EQ(dur.toMicrosecondsInt(), 1'234LL);
    EXPECT_EQ(dur.toMillisecondsInt(), 1LL);
}

TEST(DurationTest, Scaling)
{
    const Duration base = Duration::fromMilliseconds(100.0);

    EXPECT_EQ(base.scaled(1.0).nanoseconds, 100'000'000LL);
    EXPECT_EQ(base.scaled(0.5).nanoseconds, 50'000'000LL);
    EXPECT_EQ(base.scaled(2.0).nanoseconds, 200'000'000LL);
    EXPECT_EQ(base.scaled(0.0).nanoseconds, 0LL);
    EXPECT_EQ(base.scaled(-1.0).nanoseconds, -100'000'000LL);
}

TEST(DurationTest, ArithmeticOperators)
{
    const Duration a = Duration::fromMilliseconds(10.0);
    const Duration b = Duration::fromMilliseconds(4.0);

    const Duration sum = a + b;
    EXPECT_EQ(sum.nanoseconds, 14'000'000LL);

    const Duration diff = a - b;
    EXPECT_EQ(diff.nanoseconds, 6'000'000LL);

    const Duration neg = -a;
    EXPECT_EQ(neg.nanoseconds, -10'000'000LL);

    Duration c = a;
    c += b;
    EXPECT_EQ(c.nanoseconds, 14'000'000LL);

    c -= b;
    EXPECT_EQ(c.nanoseconds, 10'000'000LL);
}

TEST(DurationTest, ComparisonOperators)
{
    const Duration smallDur = Duration::fromMicroseconds(100LL);
    const Duration largeDur = Duration::fromMicroseconds(200LL);
    const Duration equalDur = Duration::fromMicroseconds(100LL);

    EXPECT_TRUE(smallDur < largeDur);
    EXPECT_TRUE(smallDur <= largeDur);
    EXPECT_TRUE(smallDur <= equalDur);
    EXPECT_TRUE(largeDur > smallDur);
    EXPECT_TRUE(largeDur >= smallDur);
    EXPECT_TRUE(smallDur >= equalDur);
    EXPECT_TRUE(smallDur == equalDur);
    EXPECT_TRUE(smallDur != largeDur);
}

TEST(DurationTest, NegativeDurations)
{
    const Duration neg = Duration::fromSeconds(-2.5);

    EXPECT_EQ(neg.nanoseconds, -2'500'000'000LL);
    EXPECT_DOUBLE_EQ(neg.toSeconds(), -2.5);
    EXPECT_FLOAT_EQ(neg.toSecondsFloat(), -2.5f);
    EXPECT_DOUBLE_EQ(neg.toMilliseconds(), -2500.0);
    EXPECT_EQ(neg.toMicrosecondsInt(), -2'500'000LL);
    EXPECT_EQ(neg.toMillisecondsInt(), -2'500LL);
}

TEST(DurationTest, LargeDurationPreservation)
{
    // Approximately 100 years in nanoseconds
    const gp::Int64 hundredYearsNs = 100LL * 365LL * 24LL * 3600LL * 1'000'000'000LL;
    const Duration longDur(hundredYearsNs);

    EXPECT_EQ(longDur.nanoseconds, hundredYearsNs);
    EXPECT_NEAR(longDur.toSeconds(), static_cast<gp::Float64>(hundredYearsNs) / 1e9, 1.0);
}

TEST(TimestampTest, DefaultAndExplicitConstruction)
{
    const Timestamp defaultTime;
    EXPECT_EQ(defaultTime.nanoseconds, 0ULL);

    const Timestamp explicitTime(987654321ULL);
    EXPECT_EQ(explicitTime.nanoseconds, 987654321ULL);
}

TEST(TimestampTest, ComparisonOperators)
{
    const Timestamp t1(1000ULL);
    const Timestamp t2(2000ULL);
    const Timestamp t3(1000ULL);

    EXPECT_TRUE(t1 < t2);
    EXPECT_TRUE(t1 <= t2);
    EXPECT_TRUE(t1 <= t3);
    EXPECT_TRUE(t2 > t1);
    EXPECT_TRUE(t2 >= t1);
    EXPECT_TRUE(t1 >= t3);
    EXPECT_TRUE(t1 == t3);
    EXPECT_TRUE(t1 != t2);
}

TEST(TimestampTest, TimestampDifferenceReturnsDuration)
{
    const Timestamp t1(10'000'000ULL);
    const Timestamp t2(25'000'000ULL);

    const Duration deltaPos = t2 - t1;
    EXPECT_EQ(deltaPos.nanoseconds, 15'000'000LL);

    const Duration deltaNeg = t1 - t2;
    EXPECT_EQ(deltaNeg.nanoseconds, -15'000'000LL);

    const Duration deltaZero = t1 - t1;
    EXPECT_EQ(deltaZero.nanoseconds, 0LL);
}

TEST(TimestampTest, TimestampAdvanceAndRewind)
{
    const Timestamp start(100'000'000ULL);
    const Duration forward = Duration::fromMilliseconds(50.0);
    const Duration backward = Duration::fromMilliseconds(-30.0);

    const Timestamp advanced = start + forward;
    EXPECT_EQ(advanced.nanoseconds, 150'000'000ULL);

    const Timestamp rewound = start + backward;
    EXPECT_EQ(rewound.nanoseconds, 70'000'000ULL);
}

TEST(FrameRateTest, DefaultAndParameterizedConstruction)
{
    const FrameRate defaultRate;
    EXPECT_EQ(defaultRate.numerator, 60u);
    EXPECT_EQ(defaultRate.denominator, 1u);
    EXPECT_TRUE(defaultRate.isValid());

    const FrameRate customRate(144u, 1u);
    EXPECT_EQ(customRate.numerator, 144u);
    EXPECT_EQ(customRate.denominator, 1u);
    EXPECT_TRUE(customRate.isValid());

    // Zero denominator protection: denominator must clamp to 1u
    const FrameRate zeroDenomRate(60u, 0u);
    EXPECT_EQ(zeroDenomRate.numerator, 60u);
    EXPECT_EQ(zeroDenomRate.denominator, 1u);
}

TEST(FrameRateTest, StandardRatesToFrameDuration)
{
    // 60 Hz -> 1,000,000,000 / 60 = 16,666,666 ns
    EXPECT_EQ(FrameRate(60u, 1u).toFrameDuration().nanoseconds, 16'666'666LL);

    // 30 Hz -> 1,000,000,000 / 30 = 33,333,333 ns
    EXPECT_EQ(FrameRate(30u, 1u).toFrameDuration().nanoseconds, 33'333'333LL);

    // 120 Hz -> 1,000,000,000 / 120 = 8,333,333 ns
    EXPECT_EQ(FrameRate(120u, 1u).toFrameDuration().nanoseconds, 8'333'333LL);

    // 240 Hz -> 1,000,000,000 / 240 = 4,166,666 ns
    EXPECT_EQ(FrameRate(240u, 1u).toFrameDuration().nanoseconds, 4'166'666LL);

    // 0 Hz -> Duration::zero()
    EXPECT_EQ(FrameRate(0u, 1u).toFrameDuration().nanoseconds, 0LL);
}

TEST(FrameRateTest, NtscBroadcastRates)
{
    // 59.94 Hz = 60000 / 1001
    const FrameRate ntsc60(60'000u, 1001u);
    EXPECT_TRUE(ntsc60.isValid());
    EXPECT_NEAR(ntsc60.toHz(), 59.94005994, 1e-6);

    // (1e9 * 1001) / 60000 = 16,683,333 ns
    EXPECT_EQ(ntsc60.toFrameDuration().nanoseconds, 16'683'333LL);

    // 23.976 Hz = 24000 / 1001
    const FrameRate ntsc24(24'000u, 1001u);
    EXPECT_TRUE(ntsc24.isValid());
    EXPECT_NEAR(ntsc24.toHz(), 23.97602398, 1e-6);

    // (1e9 * 1001) / 24000 = 41,708,333 ns
    EXPECT_EQ(ntsc24.toFrameDuration().nanoseconds, 41'708'333LL);
}

TEST(FrameRateTest, ToHzCalculation)
{
    EXPECT_DOUBLE_EQ(FrameRate(60u, 1u).toHz(), 60.0);
    EXPECT_DOUBLE_EQ(FrameRate(144u, 1u).toHz(), 144.0);
    EXPECT_DOUBLE_EQ(FrameRate(0u, 1u).toHz(), 0.0);
}

TEST(FrameRateTest, ValidityChecks)
{
    EXPECT_TRUE(FrameRate(60u, 1u).isValid());
    EXPECT_FALSE(FrameRate(0u, 1u).isValid());
}

TEST(FixedStepResultTest, DefaultInitialization)
{
    const FixedStepResult result;

    EXPECT_EQ(result.stepCount, 0u);
    EXPECT_EQ(result.fixedDelta.nanoseconds, 16'666'667LL);
    EXPECT_FLOAT_EQ(result.interpolationAlpha, 0.0f);
    EXPECT_FALSE(result.wasClamped);
}

TEST(FrameTimeStatsTest, DefaultInitialization)
{
    const FrameTimeStats stats;

    EXPECT_EQ(stats.frameIndex, 0ULL);
    EXPECT_EQ(stats.rawDelta.nanoseconds, 0LL);
    EXPECT_EQ(stats.smoothedUnscaledDelta.nanoseconds, 0LL);
    EXPECT_EQ(stats.gameplayDelta.nanoseconds, 0LL);
    EXPECT_EQ(stats.totalRealTime.nanoseconds, 0LL);
    EXPECT_EQ(stats.totalGameplayTime.nanoseconds, 0LL);
    EXPECT_FLOAT_EQ(stats.deltaTime, 0.0f);
    EXPECT_DOUBLE_EQ(stats.deltaTimeDouble, 0.0);
    EXPECT_FLOAT_EQ(stats.unscaledDeltaTime, 0.0f);
    EXPECT_FLOAT_EQ(stats.smoothedFps, 60.0f);
    EXPECT_EQ(stats.fixedStep.stepCount, 0u);
}

}   // namespace gp::time::tests
