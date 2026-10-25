// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/unix/system/UnixTime.hpp"
#include <errno.h>
#include <time.h>

namespace gp::platform::unix
{

namespace detail
{

constexpr gp::UInt64 kNanosecondsPerSecond = 1'000'000'000ULL;
constexpr gp::UInt64 kNanosecondsPerMillisecond = 1'000'000ULL;
constexpr gp::UInt64 kNanosecondsPerMicrosecond = 1'000ULL;

/// @brief Converts a POSIX `struct tm` and nanosecond remainder into an engine `DateTime`.
/// @param[in] posixTm Populated POSIX broken-down time structure.
/// @param[in] nsec Nanosecond fraction of the current second `[0..999,999,999]`.
/// @return Engine `DateTime` POD.
[[nodiscard]] constexpr DateTime fromPosixTm(const struct tm& posixTm, long nsec) noexcept
{
    DateTime dt{};
    dt.year = static_cast<gp::UInt16>(posixTm.tm_year + 1900);
    dt.month = static_cast<gp::UInt8>(posixTm.tm_mon + 1);
    dt.dayOfWeek = static_cast<gp::UInt8>(posixTm.tm_wday);
    dt.day = static_cast<gp::UInt8>(posixTm.tm_mday);
    dt.hour = static_cast<gp::UInt8>(posixTm.tm_hour);
    dt.minute = static_cast<gp::UInt8>(posixTm.tm_min);
    dt.second = static_cast<gp::UInt8>(posixTm.tm_sec > 59 ? 59 : posixTm.tm_sec);
    dt.millisecond =
        static_cast<gp::UInt16>(static_cast<gp::UInt64>(nsec >= 0 ? nsec : 0) / kNanosecondsPerMillisecond);
    return dt;
}

}   // namespace detail

gp::UInt64 Time::nowNanoseconds() noexcept
{
    struct timespec ts{};
    (void)::clock_gettime(CLOCK_MONOTONIC, &ts);
    return (static_cast<gp::UInt64>(ts.tv_sec) * detail::kNanosecondsPerSecond) + static_cast<gp::UInt64>(ts.tv_nsec);
}

gp::UInt64 Time::rawPerformanceCounter() noexcept
{
    return nowNanoseconds();
}

gp::UInt64 Time::cpuCycles() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
    gp::UInt32 lo = 0u;
    gp::UInt32 hi = 0u;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<gp::UInt64>(hi) << 32u) | static_cast<gp::UInt64>(lo);
#elif defined(__aarch64__)
    gp::UInt64 cntvct = 0ULL;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(cntvct));
    return cntvct;
#else
    return nowNanoseconds();
#endif
}

gp::UInt64 Time::utcTimestampMicroseconds() noexcept
{
    struct timespec ts{};
    if (::clock_gettime(CLOCK_REALTIME, &ts) != 0) [[unlikely]]
    {
        return 0ULL;
    }

    return (static_cast<gp::UInt64>(ts.tv_sec) * 1'000'000ULL) +
           (static_cast<gp::UInt64>(ts.tv_nsec) / detail::kNanosecondsPerMicrosecond);
}

DateTime Time::localDateTime() noexcept
{
    struct timespec ts{};
    (void)::clock_gettime(CLOCK_REALTIME, &ts);

    struct tm localTm{};
    (void)::localtime_r(&ts.tv_sec, &localTm);
    return detail::fromPosixTm(localTm, ts.tv_nsec);
}

DateTime Time::utcDateTime() noexcept
{
    struct timespec ts{};
    (void)::clock_gettime(CLOCK_REALTIME, &ts);

    struct tm utcTm{};
    (void)::gmtime_r(&ts.tv_sec, &utcTm);
    return detail::fromPosixTm(utcTm, ts.tv_nsec);
}

void Time::sleepNanoseconds(gp::UInt64 nanoseconds) noexcept
{
    if (nanoseconds == 0ULL) [[unlikely]]
    {
        return;
    }

    struct timespec req{};
    req.tv_sec = static_cast<time_t>(nanoseconds / detail::kNanosecondsPerSecond);
    req.tv_nsec = static_cast<long>(nanoseconds % detail::kNanosecondsPerSecond);

    struct timespec rem{};
    while (::nanosleep(&req, &rem) == -1 && errno == EINTR)
    {
        req = rem;
    }
}

}   // namespace gp::platform::unix
