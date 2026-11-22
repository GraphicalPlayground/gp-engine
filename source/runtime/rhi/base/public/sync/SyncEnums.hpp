// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::sync
{

/// @brief Flags controlling pipeline barrier and split-transition behavior.
enum class TransitionCreateFlags : gp::UInt32
{
    None = 0u,
    /// @brief Omits cross-pipeline fences during the transition.
    NoFence = 1u << 0,
    /// @brief Uses a partial flush instead of a split fence when no work sits between Begin and End.
    NoSplit = 1u << 1,
    AllowDuringRenderPass = 1u << 2,
    /// @brief Allows transitioning from a single prior pipeline after an earlier transition to all pipelines.
    AllowDecayPipelines = 1u << 3
};

/// @brief Per-resource flags applied to a barrier transition.
enum class ResourceTransitionFlags : gp::UInt8
{
    None = 0x00u,
    /// @brief Keeps hardware compression intact so shaders can sample compressed surfaces directly.
    MaintainCompression = 1u << 0,
    /// @brief Leaves the post-transition state untracked by the state tracker.
    IgnoreAfterState = 1u << 1,

    Mask = (1u << 0) | (1u << 1)
};

/// @brief Metric captured by a GPU render query.
enum class RenderQueryType : gp::UInt8
{
    Undefined = 0,
    /// @brief Number of samples passing depth/stencil tests (divide by MSAA sample count for pixels).
    Occlusion,
    /// @brief GPU timestamp reported in microseconds.
    AbsoluteTime
};

}   // namespace gp::rhi::sync

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::sync::TransitionCreateFlags);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::sync::ResourceTransitionFlags);
