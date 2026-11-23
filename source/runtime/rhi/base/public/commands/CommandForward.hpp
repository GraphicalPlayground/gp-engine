// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::command
{

/// @brief Action taken on a render target attachment when a render pass begins.
enum class RenderTargetLoadAction : gp::UInt8;

/// @brief Action taken on a render target attachment when a render pass ends.
enum class RenderTargetStoreAction : gp::UInt8;

/// @brief Preset color and depth-stencil load/store combinations for a render pass.
enum class SimpleRenderTargetMode : gp::UInt8;

/// @brief Targets affected by a depth-stencil clear operation.
enum class ClearDepthStencil : gp::UInt8;

/// @brief Scheduling priority for async compute queue submissions.
enum class AsyncComputePriority : gp::UInt8;

/// @brief Driver hint for how much GPU capacity async compute may claim against graphics.
enum class AsyncComputeBudget : gp::UInt8;

/// @brief Driver hint for balancing occupancy between graphics and synchronous compute work.
enum class SyncComputeBudget : gp::UInt8;

/// @brief Immediate RHI command list used by the render thread to record and flush GPU work.
class CommandListImmediate;

/// @brief Base class for recorded RHI command lists.
class CommandListBase;

/// @brief Command list interface supporting compute and barrier recording.
class ComputeCommandList;

/// @brief Low-level backend execution context supporting compute work and resource state tracking.
class IComputeContext;

/// @brief Dispatches recorded RHI command lists and drives deferred resource deletion batches.
class CommandListExecutor;

/// @brief Command packet beginning a batch of split resource transitions.
struct CommandBeginTransitions;

/// @brief Command packet ending a batch of split resource transitions.
struct CommandEndTransitions;

/// @brief Command packet executing an immediate resource transition.
struct CommandResourceTransition;

}   // namespace gp::rhi::command
