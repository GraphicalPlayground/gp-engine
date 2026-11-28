// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "validation/ValidationForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::validation
{

/// @brief Controls verbose barrier logging on tracked RHI resources.
enum class LoggingMode : gp::UInt8
{
    /// @brief Do not emit verbose state-transition trace logs.
    None = 0u,

    /// @brief Resource was explicitly opted into barrier trace logging.
    Manual = 1u,

    /// @brief Resource was opted into barrier trace logging via global filter.
    Automatic = 2u
};

/// @brief Logical texture/resource plane tracked independently by the validation barrier machine.
enum class ResourcePlane : gp::UInt8
{
    /// @brief Primary color, depth, or buffer data plane.
    Common = 0u,

    /// @brief Stencil plane (or CMask metadata plane on color targets).
    Stencil = 1u,

    /// @brief Hierarchical depth metadata plane (HTile/HiZ).
    HTile = 2u,

    /// @brief Maximum number of independently tracked planes per mip/slice.
    Max = 3u
};

/// @brief Lifecycle phase of a transient resource allocated from a frame transient heap.
enum class TransientStatus : gp::UInt8
{
    /// @brief Committed/persistent resource (not managed by a transient heap).
    NonTransient = 0u,

    /// @brief Transient resource has not yet been acquired (or finished discarding).
    Released = 1u,

    /// @brief Transient resource has been acquired and is valid for barriers and GPU access.
    Acquired = 2u,

    /// @brief Split discard barrier has begun (`beginTransitions`) and awaits `endTransitions`.
    Discarding = 3u,

    /// @brief Discard barrier completed; memory may now be overlapped by subsequent transient `Acquire` barriers.
    Discarded = 4u
};

/// @brief Selects between graphics and compute UAV binding tables inside `Tracker`.
enum class UAVMode : gp::UInt8
{
    Graphics = 0u,
    Compute = 1u,
    Count = 2u
};

/// @brief Discriminator identifying a recorded `Operation` in `OperationsList`.
enum class OpType : gp::UInt8
{
    Rename = 0u,
    BeginTransition,
    EndTransition,
    AliasingOverlap,
    AcquireTransient,
    DiscardTransientBegin,
    DiscardTransientEnd,
    Assert,
    AssertTracked,
    SetTrackedAccess,
    Signal,
    Wait,
    AllUAVsOverlap,
    SpecificUAVOverlap
};

}   // namespace gp::rhi::validation
