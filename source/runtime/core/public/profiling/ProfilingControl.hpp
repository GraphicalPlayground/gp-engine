// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "profiling/Profiler.hpp"

namespace gp::profiling
{

/// @brief Lock-free runtime profiling telemetry controller bridged to `gp::console` CVars.
/// @details Exposes cache-friendly queries backed by `constinit` `.bss` atomics updated via `prof.*` CVar change
///          callbacks (`prof.Enabled`, `prof.Plots`, `prof.TraceConsole`, `prof.HitchThresholdMs`) so instrumented code
///          never touches hash tables or pulls heavy `Console.hpp` templates into hot headers.
class GP_CORE_API ProfilingControl final
{
public:
    ProfilingControl() = delete;

public:
    /// @brief Reports whether conditional CPU profiling zones (`GP_PROFILE_SCOPE*_ACTIVE`) are active at runtime.
    /// @return True when `prof.Enabled` is enabled, false otherwise.
    [[nodiscard]] static bool isZonesEnabled() noexcept;

    /// @brief Reports whether Tracy numeric plot submissions (`GP_PLOT_IF`) are active at runtime.
    /// @return True when `prof.Plots` is enabled, false otherwise.
    [[nodiscard]] static bool isPlotsEnabled() noexcept;

    /// @brief Reports whether console variable changes and commands emit timeline bookmark messages in Tracy.
    /// @return True when `prof.TraceConsole` is enabled, false otherwise.
    [[nodiscard]] static bool isConsoleTraceEnabled() noexcept;

    /// @brief Returns the configured frame delta threshold in milliseconds that triggers a red Tracy hitch bookmark.
    /// @return Threshold in milliseconds (`<= 0.0` indicates hitch bookmarking is disabled).
    [[nodiscard]] static gp::Float64 getHitchThresholdMs() noexcept;

    /// @brief Emits a colored timeline bookmark message to Tracy if profiling zones/markers are enabled.
    /// @param[in] label Null-safe UTF-8 bookmark label (`StringView`).
    /// @param[in] color Packed `0xRRGGBB` Tracy color (defaults to cyan).
    static void emitBookmark(gp::StringView label, gp::UInt32 color = GP_U32_COLOR_CYAN) noexcept;

    /// @brief Checks whether @p rawDeltaMs meets or exceeds `getHitchThresholdMs()` and emits a red hitch marker.
    /// @param[in] rawDeltaMs Unclamped raw frame duration in milliseconds from `EngineClock::tick()`.
    /// @param[in] frameIndex Monotonic 1-based engine frame index included in the marker label.
    /// @return True if `prof.HitchThresholdMs > 0.0` and @p rawDeltaMs exceeded the threshold, false otherwise.
    static bool recordFrameHitchIfExceeded(gp::Float64 rawDeltaMs, gp::UInt64 frameIndex) noexcept;
};

}   // namespace gp::profiling
