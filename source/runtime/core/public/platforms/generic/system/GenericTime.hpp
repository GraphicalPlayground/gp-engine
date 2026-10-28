// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::platform
{

/// @brief Calendar date and wall-clock time structure (used for logging, crash dumps, and save files).
struct DateTime
{
public:
    gp::UInt16 year{ 1970u };       // Full year (e.g., 2026)
    gp::UInt8 month{ 1u };          // Month of year [1..12]
    gp::UInt8 dayOfWeek{ 0u };      // Day of week [0 = Sunday .. 6 = Saturday]
    gp::UInt8 day{ 1u };            // Day of month [1..31]
    gp::UInt8 hour{ 0u };           // Hour of day [0..23]
    gp::UInt8 minute{ 0u };         // Minute of hour [0..59]
    gp::UInt8 second{ 0u };         // Second of minute [0..59]
    gp::UInt16 millisecond{ 0u };   // Millisecond [0..999]
    gp::UInt16 reserved{ 0u };      // Explicit alignment to 12 bytes

public:
    /// @brief Checks equality between two DateTime snapshots.
    [[nodiscard]] constexpr bool operator==(const DateTime& other) const noexcept
    {
        return year == other.year && month == other.month && day == other.day && hour == other.hour &&
               minute == other.minute && second == other.second && millisecond == other.millisecond;
    }
};

static_assert(sizeof(DateTime) == 12, "DateTime size mismatch!");

}   // namespace gp::platform

namespace gp::platform::generic
{

/// @brief Stateless base contract and unsupported-platform fallback for hardware timekeeping.
/// @details Provides the complete public Time interface with safe constexpr fallbacks when compiled
///          on a target without a dedicated OS time implementation.
class Time
{
public:
    Time() = delete;

    /// @brief Queries the monotonic high-resolution OS performance counter converted to nanoseconds.
    /// @return Monotonic hardware timestamp in nanoseconds (0ULL on unsupported platforms).
    [[nodiscard]] static constexpr gp::UInt64 nowNanoseconds() noexcept
    {
        return 0ULL;
    }

    /// @brief Queries the raw OS performance counter ticks without unit conversion.
    /// @return Raw hardware performance counter value.
    [[nodiscard]] static constexpr gp::UInt64 rawPerformanceCounter() noexcept
    {
        return 0ULL;
    }

    /// @brief Queries the frequency (ticks per second) of `rawPerformanceCounter()`.
    /// @return Hardware counter frequency in Hz (guaranteed >= 1ULL to prevent division by zero).
    [[nodiscard]] static constexpr gp::UInt64 performanceFrequency() noexcept
    {
        return 1'000'000'000ULL;
    }

    /// @brief Reads the ultra-low-overhead processor timestamp counter (e.g., x86 RDTSC or ARM64 CNTVCT_EL0).
    /// @note Intended strictly for micro-profiling CPU scopes; use `nowNanoseconds()` for simulation timing.
    /// @return Current 64-bit CPU cycle counter.
    [[nodiscard]] static constexpr gp::UInt64 cpuCycles() noexcept
    {
        return 0ULL;
    }

    /// @brief Queries the current Unix epoch timestamp in UTC microseconds (since Jan 1, 1970).
    /// @return UTC wall-clock timestamp in microseconds.
    [[nodiscard]] static constexpr gp::UInt64 utcTimestampMicroseconds() noexcept
    {
        return 0ULL;
    }

    /// @brief Queries the current local wall-clock calendar date and time.
    /// @return Populated `DateTime` structure in the system's local timezone.
    [[nodiscard]] static constexpr DateTime localDateTime() noexcept
    {
        return DateTime{};
    }

    /// @brief Queries the current Coordinated Universal Time (UTC) calendar date and time.
    /// @return Populated `DateTime` structure in UTC.
    [[nodiscard]] static constexpr DateTime utcDateTime() noexcept
    {
        return DateTime{};
    }

    /// @brief Suspends the calling thread with sub-millisecond kernel timer precision (ADR-0031).
    /// @param[in] nanoseconds Duration to sleep in nanoseconds.
    static constexpr void sleepNanoseconds([[maybe_unused]] gp::UInt64 nanoseconds) noexcept
    {}
};

}   // namespace gp::platform::generic
