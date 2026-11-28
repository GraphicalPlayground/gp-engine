// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::profiling
{

/// @brief Per-adapter video and system memory budget/usage snapshot reported by `DynamicRHI::getMemoryStats()`.
struct RHIMemoryStats;

/// @brief Named draw-call statistics bucket (e.g., `"ShadowDepths"`, `"BasePass"`, `"Translucency"`).
class DrawStatsCategory;

/// @brief Snapshot of global atomic RHI resource allocation counters maintained by `RHICoreStats`.
struct RHIResourceMemoryCountersSnapshot;

/// @brief Formats key-value metadata strings attached to GPU profiler trace events.
class MetadataSerializer;

/// @brief Structured trace event recorder for GPU queues, frames, and breadcrumb scopes.
class GpuProfilerTrace;

/// @brief Identifies a hardware queue `(Type, gpu, index)` inside `GPUProfiler`.
struct Queue;

/// @brief Tagged GPU profiler event record (`FrameBoundary`, `BeginWork`, `EndWork`, `BeginBreadcrumb`, etc.).
struct Event;

/// @brief Pooled 16 KiB chunked stream of `Event` records produced per submission.
class EventStream;

/// @brief Action returned by `GPUStat` timing callbacks (`Keep` or `Delete`).
enum class OnTimingResultsAction : gp::UInt8;

/// @brief Hierarchical inclusive/exclusive GPU timing stat node.
class GPUStat;

/// @brief Ring buffer smoothing and validating per-frame GPU busy cycles.
class GPUFrameTimeHistory;

}   // namespace gp::rhi::profiling
