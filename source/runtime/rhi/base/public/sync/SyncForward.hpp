// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::sync
{

/// @brief GPU resource usage and synchronization state flags used by barrier transitions.
enum class ResourceAccess : gp::UInt32;

/// @brief GPU execution queue/pipeline domain targeted by transitions and command lists.
enum class PipelineStage : gp::UInt8;

/// @brief Flags controlling pipeline barrier and split-transition behavior.
enum class TransitionCreateFlags : gp::UInt32;

/// @brief Per-resource flags applied to a barrier transition.
enum class ResourceTransitionFlags : gp::UInt8;

/// @brief Metric captured by a GPU render query.
enum class RenderQueryType : gp::UInt8;

}   // namespace gp::rhi::sync
