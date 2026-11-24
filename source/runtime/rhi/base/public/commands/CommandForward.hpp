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

/// @brief Combined load and store action packed for a single render target attachment.
enum class RenderTargetActions : gp::UInt8;

/// @brief Combined depth and stencil load/store actions packed for a depth-stencil attachment.
enum class DepthStencilTargetActions : gp::UInt8;

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

/// @brief Flush level requested on `CommandListImmediate::immediateFlush()`.
enum class ImmediateFlushType : gp::UInt8;

/// @brief Bitmask flags controlling `CommandListExecutor::submit()` behavior.
enum class SubmitFlags : gp::UInt32;

/// @brief Viewport bounds rectangle with minimum and maximum depth.
struct ViewportBounds;

/// @brief Indirect compute dispatch argument layout matching GPU hardware buffers.
struct DispatchIndirectParameters;

/// @brief Indirect non-indexed draw argument layout matching GPU hardware buffers.
struct DrawIndirectParameters;

/// @brief Indirect indexed draw argument layout matching GPU hardware buffers.
struct DrawIndexedIndirectParameters;

/// @brief Sub-range `[offset, offset + size)` inside a GPU buffer.
struct BufferRange;

/// @brief Parameters controlling texture-to-texture copy regions, slices, and mip ranges.
struct CopyTextureInfo;

/// @brief Parameters controlling buffer-to-buffer copy regions.
struct CopyBufferRegionParams;

/// @brief Axis-aligned integer rectangle specifying a resolve region.
struct ResolveRect;

/// @brief Single color render target attachment descriptor.
struct RenderTargetView;

/// @brief Depth-stencil attachment descriptor with load/store actions and exclusive access mode.
struct DepthRenderTargetView;

/// @brief Legacy/converted render target binding array used by backends and render pass validation.
struct SetRenderTargetsInfo;

/// @brief Complete render pass attachment, resolve, subpass, and VRS descriptor.
struct RenderPassInfo;

/// @brief Intrusive base node for every recorded RHI command packet.
struct CommandBase;

/// @brief Opaque finalized backend command list handle submitted via `DynamicRHI::submitCommandLists()`.
class IPlatformCommandList;

/// @brief Base tag interface for backend upload contexts.
class IUploadContext;

/// @brief Base class for recorded RHI command lists.
class CommandListBase;

/// @brief Command list interface supporting compute and barrier recording.
class ComputeCommandList;

/// @brief Command list interface supporting graphics, render pass, compute, and barrier recording.
class CommandList;

/// @brief Immediate RHI command list used by the render thread to record and flush GPU work.
class CommandListImmediate;

/// @brief Low-level backend execution context supporting compute work and resource state tracking.
class IComputeContext;

/// @brief Low-level backend graphics and compute command recording context.
class ICommandContext;

/// @brief Adapter context translating PSO binds to legacy disjoint fixed-function state binds.
class ICommandContextPSOFallback;

/// @brief Dispatches recorded RHI command lists and drives deferred resource deletion batches.
class CommandListExecutor;

/// @brief Command packet beginning a batch of split resource transitions.
struct CommandBeginTransitions;

/// @brief Command packet ending a batch of split resource transitions.
struct CommandEndTransitions;

/// @brief Command packet executing an immediate resource transition.
struct CommandResourceTransition;

}   // namespace gp::rhi::command
