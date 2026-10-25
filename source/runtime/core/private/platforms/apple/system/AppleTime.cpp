// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/apple/system/AppleTime.hpp"
#include <mach/mach_time.h>
#include <time.h>   // IWYU pragma: keep

namespace gp::platform::apple
{

namespace detail
{

constexpr gp::UInt64 kNanosecondsPerSecond = 1'000'000'000ULL;

/// @brief Queries and caches the Darwin `mach_timebase_info_data_t` conversion ratio.
/// @return Const reference to the initialized `mach_timebase_info_data_t`.
[[nodiscard]] const mach_timebase_info_data_t& getMachTimebase() noexcept
{
    static mach_timebase_info_data_t s_timebase{ 0u, 0u };
    if (s_timebase.denom == 0u) [[unlikely]]
    {
        (void)::mach_timebase_info(&s_timebase);
        if (s_timebase.numer == 0u || s_timebase.denom == 0u) [[unlikely]]
        {
            s_timebase.numer = 1u;
            s_timebase.denom = 1u;
        }
    }
    return s_timebase;
}

/// @brief Converts nanoseconds into Mach absolute time ticks without 64-bit integer overflow.
/// @param[in] nanoseconds Duration in nanoseconds.
/// @return Equivalent duration in Mach hardware ticks.
[[nodiscard]] gp::UInt64 nanosecondsToMachTicks(gp::UInt64 nanoseconds) noexcept
{
    const mach_timebase_info_data_t& tb = getMachTimebase();
    if (tb.numer == tb.denom)
    {
        return nanoseconds;
    }

    const gp::UInt64 wholeUnits = nanoseconds / tb.numer;
    const gp::UInt64 remainder = nanoseconds % tb.numer;
    return (wholeUnits * tb.denom) + ((remainder * tb.denom) / tb.numer);
}

}   // namespace detail

gp::UInt64 Time::nowNanoseconds() noexcept
{
    // Darwin commpage fast path: returns monotonic nanoseconds directly without a kernel trap
    return ::clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}

gp::UInt64 Time::rawPerformanceCounter() noexcept
{
    return ::mach_absolute_time();
}

gp::UInt64 Time::performanceFrequency() noexcept
{
    const mach_timebase_info_data_t& tb = detail::getMachTimebase();
    return (detail::kNanosecondsPerSecond * static_cast<gp::UInt64>(tb.denom)) / static_cast<gp::UInt64>(tb.numer);
}

void Time::sleepNanoseconds(gp::UInt64 nanoseconds) noexcept
{
    if (nanoseconds == 0ULL) [[unlikely]]
    {
        return;
    }

    const gp::UInt64 deltaTicks = detail::nanosecondsToMachTicks(nanoseconds);
    const gp::UInt64 deadlineTicks = ::mach_absolute_time() + deltaTicks;

    // Sleep directly on the Mach kernel deadline; loop if interrupted by a Mach thread abort/signal
    while (::mach_wait_until(deadlineTicks) != KERN_SUCCESS)
    {
        if (::mach_absolute_time() >= deadlineTicks)
        {
            break;
        }
    }
}

}   // namespace gp::platform::apple
