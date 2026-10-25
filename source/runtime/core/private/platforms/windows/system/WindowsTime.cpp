// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/windows/system/WindowsTime.hpp"
#include "platforms/windows/WindowsHeaderWrapper.hpp"   // IWYU pragma: keep

// Include after Windows.h to avoid conflicts with min/max macros.
#include <intrin.h>

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
    #define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace gp::platform::windows
{

namespace detail
{

/// @brief Standard invariant QPC frequency on modern Windows 10/11 systems (10 MHz = 100ns per tick).
constexpr gp::UInt64 kStandardWin32QpcFrequency = 10'000'000ULL;

/// @brief Number of nanoseconds per second.
constexpr gp::UInt64 kNanosecondsPerSecond = 1'000'000'000ULL;

/// @brief Difference between Windows FILETIME epoch (Jan 1, 1601) and Unix epoch (Jan 1, 1970) in 100ns ticks.
constexpr gp::UInt64 kFileTimeToUnixEpoch100Ns = 116'444'736'000'000'000ULL;

/// @brief RAII wrapper for a thread-local high-resolution Win32 waitable timer handle.
/// @details Avoids calling CreateWaitableTimerExW on every frame while guaranteeing handle cleanup when the thread
/// exits.
struct ThreadLocalHighResTimer
{
    HANDLE timerHandle{ nullptr };

    ThreadLocalHighResTimer() noexcept
    {
        timerHandle =
            ::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

        // Fallback to standard manual-reset timer if running on an older Windows build
        if (timerHandle == nullptr)
        {
            timerHandle = ::CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
        }
    }

    ~ThreadLocalHighResTimer() noexcept
    {
        if (timerHandle != nullptr)
        {
            ::CloseHandle(timerHandle);
            timerHandle = nullptr;
        }
    }

    ThreadLocalHighResTimer(const ThreadLocalHighResTimer&) = delete;
    ThreadLocalHighResTimer& operator=(const ThreadLocalHighResTimer&) = delete;
};

/// @brief Converts a Win32 `SYSTEMTIME` struct into an engine `DateTime` struct.
/// @param[in] st Native Win32 SYSTEMTIME.
/// @return Engine `DateTime` POD.
[[nodiscard]] constexpr DateTime fromWin32SystemTime(const SYSTEMTIME& st) noexcept
{
    DateTime dt{};
    dt.year = static_cast<gp::UInt16>(st.wYear);
    dt.month = static_cast<gp::UInt8>(st.wMonth);
    dt.dayOfWeek = static_cast<gp::UInt8>(st.wDayOfWeek);
    dt.day = static_cast<gp::UInt8>(st.wDay);
    dt.hour = static_cast<gp::UInt8>(st.wHour);
    dt.minute = static_cast<gp::UInt8>(st.wMinute);
    dt.second = static_cast<gp::UInt8>(st.wSecond);
    dt.millisecond = static_cast<gp::UInt16>(st.wMilliseconds);
    return dt;
}

}   // namespace detail

gp::UInt64 Time::performanceFrequency() noexcept
{
    static gp::UInt64 s_frequency = 0ULL;
    if (s_frequency == 0ULL) [[unlikely]]
    {
        LARGE_INTEGER freq{};
        (void)::QueryPerformanceFrequency(&freq);
        s_frequency = (freq.QuadPart > 0) ? static_cast<gp::UInt64>(freq.QuadPart) : detail::kStandardWin32QpcFrequency;
    }
    return s_frequency;
}

gp::UInt64 Time::rawPerformanceCounter() noexcept
{
    LARGE_INTEGER counter{};
    (void)::QueryPerformanceCounter(&counter);
    return static_cast<gp::UInt64>(counter.QuadPart);
}

gp::UInt64 Time::nowNanoseconds() noexcept
{
    const gp::UInt64 ticks = rawPerformanceCounter();
    const gp::UInt64 freq = performanceFrequency();

    // Fast path: On 99% of modern Windows 10/11 hardware, QPC is backed by a 10 MHz invariant timer (1 tick = 100ns).
    if (freq == detail::kStandardWin32QpcFrequency) [[likely]]
    {
        return ticks * 100ULL;
    }

    // Overflow-safe split multiplication for arbitrary frequencies (safe for 584+ years of continuous uptime)
    const gp::UInt64 wholeSeconds = ticks / freq;
    const gp::UInt64 remainderTicks = ticks % freq;
    return (wholeSeconds * detail::kNanosecondsPerSecond) + ((remainderTicks * detail::kNanosecondsPerSecond) / freq);
}

gp::UInt64 Time::cpuCycles() noexcept
{
#if defined(_M_IX86) || defined(_M_X64)
    return ::__rdtsc();
#elif defined(_M_ARM64) || defined(_M_ARM64EC)
    // ARM64 virtual count register (CNTVCT_EL0)
    return static_cast<gp::UInt64>(_ReadStatusReg(ARM64_CNTVCT));
#else
    return rawPerformanceCounter();
#endif
}

gp::UInt64 Time::utcTimestampMicroseconds() noexcept
{
    FILETIME ft{};
    ::GetSystemTimePreciseAsFileTime(&ft);

    ULARGE_INTEGER uli{};
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    if (uli.QuadPart < detail::kFileTimeToUnixEpoch100Ns) [[unlikely]]
    {
        return 0ULL;
    }

    return (uli.QuadPart - detail::kFileTimeToUnixEpoch100Ns) / 10ULL;
}

DateTime Time::localDateTime() noexcept
{
    SYSTEMTIME st{};
    ::GetLocalTime(&st);
    return detail::fromWin32SystemTime(st);
}

DateTime Time::utcDateTime() noexcept
{
    SYSTEMTIME st{};
    ::GetSystemTime(&st);
    return detail::fromWin32SystemTime(st);
}

void Time::sleepNanoseconds(gp::UInt64 nanoseconds) noexcept
{
    if (nanoseconds == 0ULL) [[unlikely]]
    {
        return;
    }

    // Win32 waitable timers use negative 100-nanosecond relative intervals
    const gp::UInt64 intervals100Ns = nanoseconds / 100ULL;
    if (intervals100Ns == 0ULL)
    {
        return;
    }

    static thread_local detail::ThreadLocalHighResTimer s_tlsTimer{};
    if (s_tlsTimer.timerHandle != nullptr) [[likely]]
    {
        LARGE_INTEGER dueTime{};
        dueTime.QuadPart = -static_cast<LONGLONG>(intervals100Ns);

        if (::SetWaitableTimerEx(s_tlsTimer.timerHandle, &dueTime, 0, nullptr, nullptr, nullptr, 0) != FALSE)
        {
            (void)::WaitForSingleObject(s_tlsTimer.timerHandle, INFINITE);
            return;
        }
    }

    // Fallback to coarse Win32 Sleep if waitable timer creation failed
    const DWORD sleepMs = static_cast<DWORD>(nanoseconds / 1'000'000ULL);
    if (sleepMs > 0u)
    {
        ::Sleep(sleepMs);
    }
}

}   // namespace gp::platform::windows
