// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::time
{

/// @brief Predefined engine simulation clock domains (Frostbite / Decima pattern, ADR-0032).
enum class ClockDomain : gp::UInt8
{
    /// @brief Unscaled, unpausable real wall-clock time (used for profiling, networking timeouts, and OS transitions).
    RealTime = 0,

    /// @brief Unpausable interface clock (used for menus, HUD animations, and debug cameras while game is paused).
    UI = 1,

    /// @brief Primary gameplay clock (pauses when the game is paused; scales during slow-motion / bullet-time).
    Gameplay = 2,

    /// @brief Deterministic fixed-timestep accumulator clock child of `Gameplay` (used for rigid-body physics).
    Physics = 3,

    /// @brief Character skeletal animation and VFX clock child of `Gameplay` (supports localized hit-stop).
    Animation = 4,

    /// @brief Audio pitch/crossfade modulation clock.
    Audio = 5,

    /// @brief Total number of built-in engine clock domains.
    Count = 6
};

/// @brief Filtering algorithm applied to raw frame delta times to eliminate micro-stutter (ADR-0033).
enum class DeltaSmoothingMode : gp::UInt8
{
    /// @brief Uses the exact, unfiltered hardware delta time (clamped only by `maxDelta`).
    None = 0,

    /// @brief Applies an exponential moving average (90% history, 10% current frame).
    ExponentialMovingAverage = 1,

    /// @brief Unreal-style rolling window that discards the highest and lowest outliers before averaging.
    OutlierRejectedAverage = 2,

    /// @brief Ignores hardware timer variance and advances by a constant `1 / targetFps` on every tick
    ///        (used for offline video rendering, deterministic lockstep replays, and automated tests).
    FixedDeterministic = 3
};

/// @brief Execution strategy for a `FramePacer` at the end of a frame.
enum class FramePacingMode : gp::UInt8
{
    /// @brief No software pacing; runs uncapped or relies strictly on RHI VSync presentation blocking.
    Uncapped = 0,

    /// @brief Hybrid two-stage pacing: sleeps via OS high-res waitable timer, then spins (`_mm_pause`) to exact
    /// deadline.
    HybridSleepAndSpin = 1,

    /// @brief Sleeps using only the OS waitable timer without spinning (lowest battery usage for background/editor
    /// mode).
    SleepOnly = 2,

    /// @brief Pure CPU spin-wait loop (`_mm_pause`) with zero OS scheduler involvement (lowest latency, highest power).
    SpinOnly = 3
};

}   // namespace gp::time
