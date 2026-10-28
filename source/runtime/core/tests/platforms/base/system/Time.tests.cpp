// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/system/Time.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::platform::tests
{

static_assert(sizeof(DateTime) == 12u, "DateTime must be exactly 12 bytes.");
static_assert(std::is_standard_layout_v<DateTime>, "DateTime must be standard layout.");

constexpr bool testConstexprDateTimeEquality()
{
    DateTime a{};
    DateTime b{};
    return a == b;
}

static_assert(testConstexprDateTimeEquality(), "Default constructed DateTime must equal itself.");

TEST(PlatformDateTimeTest, DefaultConstructionValues)
{
    const DateTime dt;

    EXPECT_EQ(dt.year, 1970u);
    EXPECT_EQ(dt.month, 1u);
    EXPECT_EQ(dt.dayOfWeek, 0u);
    EXPECT_EQ(dt.day, 1u);
    EXPECT_EQ(dt.hour, 0u);
    EXPECT_EQ(dt.minute, 0u);
    EXPECT_EQ(dt.second, 0u);
    EXPECT_EQ(dt.millisecond, 0u);
    EXPECT_EQ(dt.reserved, 0u);
}

TEST(PlatformDateTimeTest, EqualityAndInequalityOperators)
{
    DateTime a{};
    DateTime b{};
    EXPECT_TRUE(a == b);

    b.year = 2026u;
    EXPECT_FALSE(a == b);

    a.year = 2026u;
    EXPECT_TRUE(a == b);

    b.month = 10u;
    EXPECT_FALSE(a == b);

    a.month = 10u;
    EXPECT_TRUE(a == b);

    b.day = 27u;
    EXPECT_FALSE(a == b);

    a.day = 27u;
    EXPECT_TRUE(a == b);

    b.hour = 14u;
    EXPECT_FALSE(a == b);

    a.hour = 14u;
    EXPECT_TRUE(a == b);

    b.minute = 30u;
    EXPECT_FALSE(a == b);

    a.minute = 30u;
    EXPECT_TRUE(a == b);

    b.second = 45u;
    EXPECT_FALSE(a == b);

    a.second = 45u;
    EXPECT_TRUE(a == b);

    b.millisecond = 500u;
    EXPECT_FALSE(a == b);

    a.millisecond = 500u;
    EXPECT_TRUE(a == b);
}

TEST(PlatformTimeTest, PerformanceFrequencyIsPositiveAndConsistent)
{
    const gp::UInt64 freq1 = Time::performanceFrequency();
    const gp::UInt64 freq2 = Time::performanceFrequency();

    EXPECT_GT(freq1, 0ULL);
    EXPECT_EQ(freq1, freq2);
}

TEST(PlatformTimeTest, RawPerformanceCounterIsMonotonic)
{
    const gp::UInt64 c1 = Time::rawPerformanceCounter();
    EXPECT_GT(c1, 0ULL);

    const gp::UInt64 c2 = Time::rawPerformanceCounter();
    EXPECT_GE(c2, c1);
}

TEST(PlatformTimeTest, NowNanosecondsIsMonotonicAndNonZero)
{
    const gp::UInt64 t1 = Time::nowNanoseconds();
    EXPECT_GT(t1, 0ULL);

    const gp::UInt64 t2 = Time::nowNanoseconds();
    EXPECT_GE(t2, t1);
}

TEST(PlatformTimeTest, NowNanosecondsAdvancesOverTime)
{
    const gp::UInt64 start = Time::nowNanoseconds();

    // Sleep 1ms (1,000,000 ns)
    Time::sleepNanoseconds(1'000'000ULL);

    const gp::UInt64 end = Time::nowNanoseconds();
    const gp::UInt64 elapsed = end - start;

    // Must have elapsed at least 500us (allowing for small OS timer jitter)
    EXPECT_GE(elapsed, 500'000ULL);
}

TEST(PlatformTimeTest, CpuCyclesIsMonotonic)
{
    const gp::UInt64 c1 = Time::cpuCycles();
    EXPECT_GT(c1, 0ULL);

    // Perform a tiny workload
    volatile gp::UInt32 dummy = 0u;
    for (gp::UInt32 i = 0u; i < 1000u; ++i)
    {
        dummy += i;
    }
    (void)dummy;

    const gp::UInt64 c2 = Time::cpuCycles();
    EXPECT_GE(c2, c1);
}

TEST(PlatformTimeTest, UtcTimestampMicrosecondsIsValidUnixEpoch)
{
    const gp::UInt64 u1 = Time::utcTimestampMicroseconds();

    // Timestamp corresponds to year >= 2024 (approx 1,700,000,000 seconds = 1.7e15 us)
    EXPECT_GT(u1, 1'700'000'000'000'000ULL);

    const gp::UInt64 u2 = Time::utcTimestampMicroseconds();
    EXPECT_GE(u2, u1);
}

TEST(PlatformTimeTest, LocalDateTimeReturnsValidCalendarFields)
{
    const DateTime local = Time::localDateTime();

    EXPECT_GE(local.year, 2024u);
    EXPECT_LE(local.year, 2100u);
    EXPECT_GE(local.month, 1u);
    EXPECT_LE(local.month, 12u);
    EXPECT_GE(local.day, 1u);
    EXPECT_LE(local.day, 31u);
    EXPECT_LE(local.dayOfWeek, 6u);
    EXPECT_LT(local.hour, 24u);
    EXPECT_LT(local.minute, 60u);
    EXPECT_LT(local.second, 60u);
    EXPECT_LT(local.millisecond, 1000u);
}

TEST(PlatformTimeTest, UtcDateTimeReturnsValidCalendarFields)
{
    const DateTime utc = Time::utcDateTime();

    EXPECT_GE(utc.year, 2024u);
    EXPECT_LE(utc.year, 2100u);
    EXPECT_GE(utc.month, 1u);
    EXPECT_LE(utc.month, 12u);
    EXPECT_GE(utc.day, 1u);
    EXPECT_LE(utc.day, 31u);
    EXPECT_LE(utc.dayOfWeek, 6u);
    EXPECT_LT(utc.hour, 24u);
    EXPECT_LT(utc.minute, 60u);
    EXPECT_LT(utc.second, 60u);
    EXPECT_LT(utc.millisecond, 1000u);
}

TEST(PlatformTimeTest, SleepNanosecondsZeroCompletesImmediately)
{
    const gp::UInt64 start = Time::nowNanoseconds();
    Time::sleepNanoseconds(0ULL);
    const gp::UInt64 end = Time::nowNanoseconds();

    const gp::UInt64 elapsed = end - start;
    // 0ns sleep should return immediately (< 10ms)
    EXPECT_LT(elapsed, 10'000'000ULL);
}

}   // namespace gp::platform::tests
