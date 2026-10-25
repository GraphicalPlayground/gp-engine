// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/linux/system/LinuxTime.hpp"
#include <errno.h>
#include <time.h>

namespace gp::platform::linux
{

namespace detail
{

constexpr gp::UInt64 kNanosecondsPerSecond = 1'000'000'000ULL;

}   // namespace detail

void Time::sleepNanoseconds(gp::UInt64 nanoseconds) noexcept
{
    if (nanoseconds == 0ULL) [[unlikely]]
    {
        return;
    }

    struct timespec target{};
    if (::clock_gettime(CLOCK_MONOTONIC, &target) != 0) [[unlikely]]
    {
        gp::platform::unix::Time::sleepNanoseconds(nanoseconds);
        return;
    }

    const gp::UInt64 totalNsec = static_cast<gp::UInt64>(target.tv_nsec) + nanoseconds;
    target.tv_sec += static_cast<time_t>(totalNsec / detail::kNanosecondsPerSecond);
    target.tv_nsec = static_cast<long>(totalNsec % detail::kNanosecondsPerSecond);

    // Sleep until the exact absolute monotonic deadline; immune to cumulative drift on EINTR restarts.
    int result = 0;
    do
    {
        result = ::clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &target, nullptr);
    }
    while (result == EINTR);
}

}   // namespace gp::platform::linux
