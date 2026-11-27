// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"
#include "commands/CommandForward.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceForward.hpp"       // IWYU pragma: keep
#include "diagnostics/DiagnosticsForward.hpp"
#include "sync/SyncEnums.hpp"
#include "templates/Optional.hpp"

namespace gp::rhi
{

/// @brief Arguments passed to `DynamicRHI::finalizeContexts()`.
struct FinalizeContextsArgs
{
    /// @brief Contexts that recorded commands and must be finalized into platform command lists.
    gp::Vector<command::IComputeContext*> contexts{};

    /// @brief Optional upload context finalized alongside command contexts.
    command::IUploadContext* uploadContext{ nullptr };
};

/// @brief Finalized platform command lists returned by `DynamicRHI::finalizeContexts()`.
struct FinalizeContextsResult
{
    /// @brief Finalized backend command lists ready for `DynamicRHI::submitCommandLists()`.
    gp::Vector<command::IPlatformCommandList*> commandLists{};

    /// @brief Optional finalized upload command list produced from `FinalizeContextsArgs::uploadContext`.
    command::IPlatformCommandList* uploadCommandList{ nullptr };
};

/// @brief Arguments passed to `DynamicRHI::submitCommandLists()`.
struct SubmitCommandListsArgs
{
    /// @brief Platform command lists to submit to hardware queues.
    gp::Vector<command::IPlatformCommandList*> commandLists{};

    /// @brief Optional CPU-visible fences or submission hints.
    command::SubmitFlags flags{ command::SubmitFlags::None };
};

/// @brief Frame boundary metadata passed to `DynamicRHI::endFrame()`.
struct EndFrameArgs
{
    /// @brief Monotonic frame number finishing on the render/RHI timeline.
    gp::UInt64 frameNumber{ 0u };

    /// @brief Active GPU breadcrumb node per execution pipeline at the frame boundary (`nullptr` when none).
    sync::PipelineArray<diagnostics::BreadcrumbNode*> gpuBreadcrumbs{};

    /// @brief Optional explicit stats frame index associated with GPU profiler events.
    gp::Optional<gp::Int64> statsFrame{};

    /// @brief True when the GPU profiler requested a full frame capture on the upcoming frame.
    bool profileNextFrame{ false };
};

/// @brief Supported display output resolution and refresh rate entry.
struct ScreenResolution
{
    /// @brief Width in pixels.
    gp::UInt32 width{ 0u };

    /// @brief Height in pixels.
    gp::UInt32 height{ 0u };

    /// @brief Vertical refresh rate in Hz.
    gp::UInt32 refreshRate{ 0u };

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Resolution to compare against.
    /// @return True when width, height, and refresh rate match.
    [[nodiscard]] constexpr bool operator==(const ScreenResolution& rhs) const noexcept = default;
};

}   // namespace gp::rhi
