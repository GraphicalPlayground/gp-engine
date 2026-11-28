// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "presentation/PresentationForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::presentation
{

/// @brief Parameters that control swapchain presentation and vertical sync for a frame.
struct PresentArgs
{
    /// @brief Set to 1 to submit the swapchain present call, or 0 to skip presentation.
    gp::UInt8 present : 1 { 1u };

    /// @brief Set to 1 to synchronize presentation with display vertical blank, or 0 to present immediately.
    gp::UInt8 lockToVsync : 1 { 0u };

    /// @brief Constructs default presentation arguments with presentation enabled and vertical sync disabled.
    constexpr PresentArgs() noexcept = default;

    /// @brief Constructs presentation arguments from explicit boolean flags.
    /// @param[in] inPresent True to present the swapchain backbuffer; false to skip presenting.
    /// @param[in] inLockToVsync True to synchronize presentation to vertical blank; false otherwise.
    constexpr PresentArgs(const bool inPresent, const bool inLockToVsync) noexcept
        : present(inPresent ? 1u : 0u)
        , lockToVsync(inLockToVsync ? 1u : 0u)
    {}

    /// @brief Reports whether the swapchain should present this frame.
    /// @return True when `present` is non-zero; false otherwise.
    [[nodiscard]] constexpr bool shouldPresent() const noexcept
    {
        return present != 0u;
    }

    /// @brief Reports whether presentation waits for display vertical sync.
    /// @return True when `lockToVsync` is non-zero; false otherwise.
    [[nodiscard]] constexpr bool isLockedToVsync() const noexcept
    {
        return lockToVsync != 0u;
    }

    /// @brief Compares the logical boolean fields of two `PresentArgs` values.
    /// @details Compares unpacked booleans instead of raw storage bytes so uninitialized bitfield padding bits never
    ///          cause false inequality.
    /// @param[in] rhs Presentation arguments to compare against.
    /// @return True when both `present` and `lockToVsync` flags match.
    [[nodiscard]] constexpr bool operator==(const PresentArgs& rhs) const noexcept
    {
        return (shouldPresent() == rhs.shouldPresent()) && (isLockedToVsync() == rhs.isLockedToVsync());
    }
};

/// @brief Presentation parameters extended with frame index and input timestamp statistics for latency tracking.
struct PresentArgsWithStats : public PresentArgs
{
    /// @brief Monotonic engine frame counter associated with this presentation request.
    gp::UInt64 frameCounter{ 0u };

    /// @brief CPU cycle timestamp sampled when user input was read for this frame.
    gp::UInt64 inputCycles{ 0u };

    /// @brief Constructs default presentation arguments with zeroed frame and input counters.
    constexpr PresentArgsWithStats() noexcept = default;

    /// @brief Constructs presentation arguments with explicit frame and input latency counters.
    /// @param[in] inPresentArgs Base swapchain presentation and vertical sync flags.
    /// @param[in] inFrameCounter Engine frame index for this present operation.
    /// @param[in] inInputCycles CPU cycle counter captured at input sampling time.
    constexpr PresentArgsWithStats(
        const PresentArgs& inPresentArgs, const gp::UInt64 inFrameCounter, const gp::UInt64 inInputCycles
    ) noexcept
        : PresentArgs(inPresentArgs)
        , frameCounter(inFrameCounter)
        , inputCycles(inInputCycles)
    {}

    /// @brief Compares base presentation flags and latency statistics for equality.
    /// @param[in] rhs Presentation statistics record to compare against.
    /// @return True when base flags, `frameCounter`, and `inputCycles` all match.
    [[nodiscard]] constexpr bool operator==(const PresentArgsWithStats& rhs) const noexcept
    {
        return PresentArgs::operator==(rhs) && (frameCounter == rhs.frameCounter) && (inputCycles == rhs.inputCycles);
    }
};

}   // namespace gp::rhi::presentation
