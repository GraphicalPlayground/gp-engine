// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryForward.hpp"

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

/// @brief Concrete resource kind stored inside a `TransitionInfo` payload.
enum class TransitionResourceType : gp::UInt8;

/// @brief Resource kind referenced by a transient memory aliasing barrier.
enum class TransientAliasingResourceType : gp::UInt8;

/// @brief Action performed on a transient resource during memory aliasing.
enum class TransientAliasingAction : gp::UInt8;

/// @brief Identifies a mip level, array slice, and plane slice subresource range.
struct SubresourceRange;

/// @brief Physical memory commit size request for a reserved GPU resource.
struct CommitResourceInfo;

/// @brief Barrier transition descriptor for a single RHI resource or subresource range.
struct TransitionInfo;

/// @brief Identifies a prior transient resource whose memory overlaps an acquiring resource.
struct TransientAliasingOverlap;

/// @brief Transient memory acquire or discard descriptor for aliasing barriers.
struct TransientAliasingInfo;

/// @brief Creation parameters for a pipeline barrier transition object.
struct TransitionCreateInfo;

/// @brief Pairs a tracked `ResourceAccess` state with the active `PipelineStage` mask.
struct TrackedAccess;

/// @brief Associates a `ViewableResource` pointer with its tracked access state and pipeline mask.
struct TrackedAccessInfo;

/// @brief Backend lifecycle callbacks registered for opaque `Transition` private data.
struct TransitionBackendHooks;

/// @brief Opaque runtime object tracking a pending split or immediate RHI resource transition.
class Transition;

/// @brief Entry point for creating, beginning, and ending `Transition` objects.
class TransitionController;

/// @brief GPU-to-CPU synchronization fence signaled from GPU command queues.
class GPUFence;

/// @brief GPU render query capturing occlusion counts or hardware timestamps.
class RenderQuery;

/// @brief Pool managing reusable `RenderQuery` allocations.
class RenderQueryPool;

/// @brief Move-only RAII handle returning a leased `RenderQuery` to its owning `RenderQueryPool`.
class PooledRenderQuery;

/// @brief Intrusive smart pointer handle to a `GPUFence`.
using GPUFenceRef = gp::RefCountedPtr<GPUFence, gp::DefaultRefCountPolicy<GPUFence>>;

/// @brief Intrusive smart pointer handle to a `RenderQuery`.
using RenderQueryRef = gp::RefCountedPtr<RenderQuery, gp::DefaultRefCountPolicy<RenderQuery>>;

/// @brief Intrusive smart pointer handle to a `RenderQueryPool`.
using RenderQueryPoolRef = gp::RefCountedPtr<RenderQueryPool, gp::DefaultRefCountPolicy<RenderQueryPool>>;

}   // namespace gp::rhi::sync
