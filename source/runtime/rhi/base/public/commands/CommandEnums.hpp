// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"

namespace gp::rhi::command
{

/// @brief Action taken on a render target attachment when a render pass begins.
enum class RenderTargetLoadAction : gp::UInt8
{
    /// @brief Existing contents are discarded and left undefined.
    NoAction = 0,
    Load,
    /// @brief Clears to the fast-clear value stored on the resource.
    Clear
};

/// @brief Action taken on a render target attachment when a render pass ends.
enum class RenderTargetStoreAction : gp::UInt8
{
    /// @brief Discards rendered contents without writing back to memory.
    NoAction = 0,
    Store,
    /// @brief Resolves multisampled contents with a box filter before storing.
    MultisampleResolve
};

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

}   // namespace gp::rhi::command
