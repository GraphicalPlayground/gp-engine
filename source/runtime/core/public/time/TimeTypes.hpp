// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include <compare>   // IWYU pragma: keep

namespace gp::time
{

/// @brief Canonical integer nanosecond conversion constants.
inline constexpr gp::Int64 kNanosecondsPerMicrosecond = 1'000LL;
inline constexpr gp::Int64 kNanosecondsPerMillisecond = 1'000'000LL;
inline constexpr gp::Int64 kNanosecondsPerSecond = 1'000'000'000LL;
inline constexpr gp::Int64 kNanosecondsPerMinute = 60LL * kNanosecondsPerSecond;
inline constexpr gp::Int64 kNanosecondsPerHour = 60LL * kNanosecondsPerMinute;

/// @brief Strong 64-bit signed nanosecond duration type (ADR-0030).
/// @details Capable of representing $\pm 292.47$ years with 1-nanosecond precision and zero floating-point drift.
struct Duration
{
public:
    gp::Int64 nanoseconds{ 0 };

public:
    /// @brief Default constructor initializes duration to 0ns.
    [[nodiscard]] constexpr Duration() noexcept = default;

    /// @brief Constructs a duration from raw signed nanoseconds.
    /// @param[in] ns Duration in nanoseconds.
    [[nodiscard]] explicit constexpr Duration(gp::Int64 ns) noexcept
        : nanoseconds(ns)
    {}

    /// @brief Returns a zero-length duration.
    [[nodiscard]] static constexpr Duration zero() noexcept
    {
        return Duration(0);
    }

    /// @brief Creates a Duration from raw nanoseconds.
    [[nodiscard]] static constexpr Duration fromNanoseconds(gp::Int64 ns) noexcept
    {
        return Duration(ns);
    }

    /// @brief Creates a Duration from integer microseconds.
    [[nodiscard]] static constexpr Duration fromMicroseconds(gp::Int64 us) noexcept
    {
        return Duration(us * kNanosecondsPerMicrosecond);
    }

    /// @brief Creates a Duration from floating-point milliseconds.
    [[nodiscard]] static constexpr Duration fromMilliseconds(gp::Float64 ms) noexcept
    {
        return Duration(static_cast<gp::Int64>(ms * static_cast<gp::Float64>(kNanosecondsPerMillisecond)));
    }

    /// @brief Creates a Duration from floating-point seconds.
    [[nodiscard]] static constexpr Duration fromSeconds(gp::Float64 seconds) noexcept
    {
        return Duration(static_cast<gp::Int64>(seconds * static_cast<gp::Float64>(kNanosecondsPerSecond)));
    }

    /// @brief Creates a Duration by scaling this duration by a floating-point time dilation factor.
    /// @param[in] scale Multiplier (e.g., 0.5 for half-speed).
    /// @return Scaled `Duration`.
    [[nodiscard]] constexpr Duration scaled(gp::Float64 scale) const noexcept
    {
        return Duration(static_cast<gp::Int64>(static_cast<gp::Float64>(nanoseconds) * scale));
    }

    /// @brief Converts this duration to 64-bit floating-point seconds (`double`).
    [[nodiscard]] constexpr gp::Float64 toSeconds() const noexcept
    {
        return static_cast<gp::Float64>(nanoseconds) / static_cast<gp::Float64>(kNanosecondsPerSecond);
    }

    /// @brief Converts this duration to 32-bit floating-point seconds (`float`).
    [[nodiscard]] constexpr gp::Float32 toSecondsFloat() const noexcept
    {
        return static_cast<gp::Float32>(
            static_cast<gp::Float64>(nanoseconds) / static_cast<gp::Float64>(kNanosecondsPerSecond)
        );
    }

    /// @brief Converts this duration to 64-bit floating-point milliseconds.
    [[nodiscard]] constexpr gp::Float64 toMilliseconds() const noexcept
    {
        return static_cast<gp::Float64>(nanoseconds) / static_cast<gp::Float64>(kNanosecondsPerMillisecond);
    }

    /// @brief Converts this duration to integer microseconds (truncated).
    [[nodiscard]] constexpr gp::Int64 toMicrosecondsInt() const noexcept
    {
        return nanoseconds / kNanosecondsPerMicrosecond;
    }

    /// @brief Converts this duration to integer milliseconds (truncated).
    [[nodiscard]] constexpr gp::Int64 toMillisecondsInt() const noexcept
    {
        return nanoseconds / kNanosecondsPerMillisecond;
    }

public:
    // Arithmetic & Comparison Operators
    [[nodiscard]] constexpr auto operator<=>(const Duration&) const noexcept = default;

    [[nodiscard]] constexpr Duration operator+(const Duration& rhs) const noexcept
    {
        return Duration(nanoseconds + rhs.nanoseconds);
    }

    [[nodiscard]] constexpr Duration operator-(const Duration& rhs) const noexcept
    {
        return Duration(nanoseconds - rhs.nanoseconds);
    }

    [[nodiscard]] constexpr Duration operator-() const noexcept
    {
        return Duration(-nanoseconds);
    }

    constexpr Duration& operator+=(const Duration& rhs) noexcept
    {
        nanoseconds += rhs.nanoseconds;
        return *this;
    }

    constexpr Duration& operator-=(const Duration& rhs) noexcept
    {
        nanoseconds -= rhs.nanoseconds;
        return *this;
    }
};

/// @brief Strong 64-bit unsigned monotonic timestamp in nanoseconds.
struct Timestamp
{
public:
    gp::UInt64 nanoseconds{ 0ULL };

public:
    [[nodiscard]] constexpr Timestamp() noexcept = default;

    [[nodiscard]] explicit constexpr Timestamp(gp::UInt64 ns) noexcept
        : nanoseconds(ns)
    {}

public:
    [[nodiscard]] constexpr auto operator<=>(const Timestamp&) const noexcept = default;

    /// @brief Computes the signed `Duration` elapsed between `rhs` and this timestamp (`*this - rhs`).
    [[nodiscard]] constexpr Duration operator-(const Timestamp& rhs) const noexcept
    {
        return Duration(static_cast<gp::Int64>(nanoseconds - rhs.nanoseconds));
    }

    /// @brief Advances a timestamp by a `Duration`.
    [[nodiscard]] constexpr Timestamp operator+(const Duration& delta) const noexcept
    {
        return Timestamp(static_cast<gp::UInt64>(static_cast<gp::Int64>(nanoseconds) + delta.nanoseconds));
    }
};

/// @brief Rational frame rate representation (`numerator / denominator` Hz) used by Frostbite and Unreal.
/// @details Eliminates repeating-decimal drift for both integer rates (`60/1`, `120/1`, `240/1`) and
///          broadcast NTSC rates (`60000/1001` = 59.94 Hz, `24000/1001` = 23.976 Hz).
struct FrameRate
{
public:
    gp::UInt32 numerator{ 60u };
    gp::UInt32 denominator{ 1u };

public:
    [[nodiscard]] constexpr FrameRate() noexcept = default;

    /// @brief Constructs a rational frame rate (`numerator / denominator` frames per second).
    /// @param[in] inNumerator Frames count (e.g., 60).
    /// @param[in] inDenominator Time interval in seconds (e.g., 1).
    [[nodiscard]] constexpr FrameRate(gp::UInt32 inNumerator, gp::UInt32 inDenominator = 1u) noexcept
        : numerator(inNumerator)
        , denominator(inDenominator > 0u ? inDenominator : 1u)
    {}

    /// @brief Converts this frame rate into an exact frame interval `Duration` in nanoseconds.
    /// @return Exact nanosecond duration of a single frame (`(1e9 * denominator) / numerator`).
    [[nodiscard]] constexpr Duration toFrameDuration() const noexcept
    {
        if (numerator == 0u) [[unlikely]]
        {
            return Duration::zero();
        }
        return Duration(
            static_cast<gp::Int64>((static_cast<gp::UInt64>(kNanosecondsPerSecond) * denominator) / numerator)
        );
    }

    /// @brief Converts this rational rate to floating-point Hz.
    [[nodiscard]] constexpr gp::Float64 toHz() const noexcept
    {
        return (numerator > 0u) ? (static_cast<gp::Float64>(numerator) / static_cast<gp::Float64>(denominator)) : 0.0;
    }

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return numerator > 0u && denominator > 0u;
    }
};

/// @brief Output of a fixed-timestep accumulator update (ADR-0033).
struct FixedStepResult
{
    /// @brief Number of discrete fixed-delta sub-steps to execute on this frame (clamped to `maxSubSteps`).
    gp::UInt32 stepCount{ 0u };

    /// @brief Fixed time step duration for each discrete iteration.
    Duration fixedDelta{ Duration::fromNanoseconds(16'666'667LL) };

    /// @brief Normalized remainder $\alpha \in [0.0, 1.0)$ used to interpolate visual transforms between
    ///        the previous and current physics states (`lerp(prevTransform, currTransform, alpha)`).
    gp::Float32 interpolationAlpha{ 0.0f };

    /// @brief True if the raw delta exceeded `maxSubSteps * fixedDelta` and excess time had to be discarded.
    bool wasClamped{ false };
};

/// @brief Comprehensive per-frame timing telemetry snapshot produced by `EngineClock::tick()`.
struct FrameTimeStats
{
    /// @brief Monotonically increasing frame counter since engine boot.
    gp::UInt64 frameIndex{ 0ULL };

    /// @brief Raw, unscaled, unclamped wall-clock duration of the previous frame.
    Duration rawDelta{};

    /// @brief Clamped and outlier-smoothed unscaled frame duration.
    Duration smoothedUnscaledDelta{};

    /// @brief Primary Gameplay domain scaled delta duration.
    Duration gameplayDelta{};

    /// @brief Total unscaled wall-clock time elapsed since `EngineClock::reset()`.
    Duration totalRealTime{};

    /// @brief Total scaled Gameplay simulation time elapsed since `EngineClock::reset()`.
    Duration totalGameplayTime{};

    /// @brief Convenience 32-bit float Gameplay delta time in seconds (`gameplayDelta.toSecondsFloat()`).
    gp::Float32 deltaTime{ 0.0f };

    /// @brief Convenience 64-bit double Gameplay delta time in seconds (`gameplayDelta.toSeconds()`).
    gp::Float64 deltaTimeDouble{ 0.0 };

    /// @brief Convenience 32-bit float unscaled delta time in seconds (for UI/camera updates).
    gp::Float32 unscaledDeltaTime{ 0.0f };

    /// @brief Exponentially smoothed frames-per-second telemetry.
    gp::Float32 smoothedFps{ 60.0f };

    /// @brief Fixed-timestep physics accumulator output for the current frame.
    FixedStepResult fixedStep{};
};

}   // namespace gp::time
