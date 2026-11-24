// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ExclusiveDepthStencil.hpp"
#include "CoreMinimal.hpp"

namespace gp::rhi::command
{

/// @brief Action taken on a render target attachment when a render pass begins.
enum class RenderTargetLoadAction : gp::UInt8
{
    /// @brief Existing contents are discarded and left undefined.
    NoAction = 0,
    /// @brief Preserves existing attachment contents on pass entry.
    Load,
    /// @brief Clears to the fast-clear value stored on the resource.
    Clear
};

/// @brief Total number of `RenderTargetLoadAction` enumerators and minimum bit width to pack it.
inline constexpr gp::UInt32 kRenderTargetLoadActionCount = static_cast<gp::UInt32>(RenderTargetLoadAction::Clear) + 1u;
inline constexpr gp::UInt32 kRenderTargetLoadActionNumBits = 2u;
static_assert((1u << kRenderTargetLoadActionNumBits) >= kRenderTargetLoadActionCount);

/// @brief Action taken on a render target attachment when a render pass ends.
enum class RenderTargetStoreAction : gp::UInt8
{
    /// @brief Discards rendered contents without writing back to memory.
    NoAction = 0,
    /// @brief Writes rendered contents back to memory.
    Store,
    /// @brief Resolves multisampled contents with a box filter before storing.
    MultisampleResolve
};

/// @brief Total number of `RenderTargetStoreAction` enumerators and minimum bit width to pack it.
inline constexpr gp::UInt32 kRenderTargetStoreActionCount =
    static_cast<gp::UInt32>(RenderTargetStoreAction::MultisampleResolve) + 1u;
inline constexpr gp::UInt32 kRenderTargetStoreActionNumBits = 2u;
static_assert((1u << kRenderTargetStoreActionNumBits) >= kRenderTargetStoreActionCount);

/// @brief Preset color and depth-stencil load/store combinations for a render pass.
enum class SimpleRenderTargetMode : gp::UInt8
{
    ExistingColorAndDepth = 0,
    UninitializedColorAndDepth,
    UninitializedColorExistingDepth,
    UninitializedColorClearDepth,
    ClearColorExistingDepth,
    ClearColorAndDepth,
    /// @brief Loads existing color and depth, then discards depth writes at pass end.
    ExistingContentsNoDepthStore,
    ExistingColorAndClearDepth,
    ExistingColorAndDepthAndClearStencil
};

/// @brief Targets affected by a depth-stencil clear operation.
enum class ClearDepthStencil : gp::UInt8
{
    Depth = 0,
    Stencil,
    DepthStencil
};

/// @brief Scheduling priority for async compute queue submissions.
enum class AsyncComputePriority : gp::UInt8
{
    Default = 0,
    High
};

/// @brief Driver hint for how much GPU capacity async compute may claim against graphics.
enum class AsyncComputeBudget : gp::UInt8
{
    Least = 0,
    GfxHeavy,
    Balanced,
    ComputeHeavy,
    All
};

/// @brief Driver hint for balancing occupancy between graphics and synchronous compute work.
enum class SyncComputeBudget : gp::UInt8
{
    Default = 0,
    Balanced,
    /// @brief Favors compute occupancy; may serialize concurrent async compute queues.
    ComputeHeavy
};

/// @brief Decodes a `SimpleRenderTargetMode` preset and depth-stencil usage into explicit load/store actions.
/// @details Disables depth or stencil load actions when the corresponding plane is unused and drops store actions when
///          the plane is read-only to avoid redundant memory bandwidth on tile-based GPUs.
/// @param[in] mode Preset render target mode to decode.
/// @param[out] outColorLoadAction Decoded color load action.
/// @param[out] outColorStoreAction Decoded color store action.
/// @param[out] outDepthLoadAction Decoded depth load action.
/// @param[out] outDepthStoreAction Decoded depth store action.
/// @param[out] outStencilLoadAction Decoded stencil load action.
/// @param[out] outStencilStoreAction Decoded stencil store action.
/// @param[in] depthStencilUsage Active depth and stencil read/write access mode for the pass.
inline constexpr void decodeRenderTargetMode(
    const SimpleRenderTargetMode mode,
    RenderTargetLoadAction& outColorLoadAction,
    RenderTargetStoreAction& outColorStoreAction,
    RenderTargetLoadAction& outDepthLoadAction,
    RenderTargetStoreAction& outDepthStoreAction,
    RenderTargetLoadAction& outStencilLoadAction,
    RenderTargetStoreAction& outStencilStoreAction,
    const resource::ExclusiveDepthStencil depthStencilUsage
) noexcept
{
    outColorLoadAction = RenderTargetLoadAction::NoAction;
    outColorStoreAction = RenderTargetStoreAction::Store;
    outDepthLoadAction = RenderTargetLoadAction::NoAction;
    outDepthStoreAction = RenderTargetStoreAction::Store;
    outStencilStoreAction = RenderTargetStoreAction::Store;

    switch (mode)
    {
    case SimpleRenderTargetMode::ExistingColorAndDepth:
        outColorLoadAction = RenderTargetLoadAction::Load;
        outDepthLoadAction = RenderTargetLoadAction::Load;
        break;
    case SimpleRenderTargetMode::UninitializedColorAndDepth:
        outColorLoadAction = RenderTargetLoadAction::NoAction;
        outDepthLoadAction = RenderTargetLoadAction::NoAction;
        break;
    case SimpleRenderTargetMode::UninitializedColorExistingDepth:
        outColorLoadAction = RenderTargetLoadAction::NoAction;
        outDepthLoadAction = RenderTargetLoadAction::Load;
        break;
    case SimpleRenderTargetMode::UninitializedColorClearDepth:
        outColorLoadAction = RenderTargetLoadAction::NoAction;
        outDepthLoadAction = RenderTargetLoadAction::Clear;
        break;
    case SimpleRenderTargetMode::ClearColorExistingDepth:
        outColorLoadAction = RenderTargetLoadAction::Clear;
        outDepthLoadAction = RenderTargetLoadAction::Load;
        break;
    case SimpleRenderTargetMode::ClearColorAndDepth:
        outColorLoadAction = RenderTargetLoadAction::Clear;
        outDepthLoadAction = RenderTargetLoadAction::Clear;
        break;
    case SimpleRenderTargetMode::ExistingContentsNoDepthStore:
        outColorLoadAction = RenderTargetLoadAction::Load;
        outDepthLoadAction = RenderTargetLoadAction::Load;
        outDepthStoreAction = RenderTargetStoreAction::NoAction;
        break;
    case SimpleRenderTargetMode::ExistingColorAndClearDepth:
        outColorLoadAction = RenderTargetLoadAction::Load;
        outDepthLoadAction = RenderTargetLoadAction::Clear;
        break;
    case SimpleRenderTargetMode::ExistingColorAndDepthAndClearStencil:
        outColorLoadAction = RenderTargetLoadAction::Load;
        outDepthLoadAction = RenderTargetLoadAction::Load;
        break;
    default:
        if !consteval
        {
            GP_CHECK_NO_ENTRY();
        }
        break;
    }

    outStencilLoadAction = outDepthLoadAction;

    if (!depthStencilUsage.isUsingDepth())
    {
        outDepthLoadAction = RenderTargetLoadAction::NoAction;
    }

    if (!depthStencilUsage.isDepthWrite())
    {
        outDepthStoreAction = RenderTargetStoreAction::NoAction;
    }

    if (!depthStencilUsage.isUsingStencil())
    {
        outStencilLoadAction = RenderTargetLoadAction::NoAction;
    }

    if (!depthStencilUsage.isStencilWrite())
    {
        outStencilStoreAction = RenderTargetStoreAction::NoAction;
    }
}

}   // namespace gp::rhi::command
