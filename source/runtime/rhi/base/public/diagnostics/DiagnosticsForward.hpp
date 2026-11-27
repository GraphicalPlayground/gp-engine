// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi::diagnostics
{

/// @brief Verbosity tier filtering whether a breadcrumb event is emitted.
enum class BreadcrumbVerbosity : gp::UInt8;

/// @brief Execution state reported for a breadcrumb node during GPU crash analysis.
enum class BreadcrumbMarkerState : gp::UInt8;

/// @brief Source file and line metadata captured at a breadcrumb call site.
struct BreadcrumbLocation;

/// @brief Static descriptor metadata shared by instances of a breadcrumb event.
struct BreadcrumbData;

/// @brief Fixed-capacity stack character buffer used to format dynamic breadcrumb labels.
struct BreadcrumbBuffer;

/// @brief Identifies a `(deviceIndex, pipeline)` hardware queue in breadcrumb crash state.
struct BreadcrumbQueueId;

/// @brief Tree node representing a single CPU/GPU breadcrumb scope in the RHI timeline.
struct BreadcrumbNode;

/// @brief Singly-linked list helper used to link and unlink breadcrumb nodes.
struct BreadcrumbList;

/// @brief Inclusive `[first, last]` sub-sequence of breadcrumb nodes executed on a queue.
struct BreadcrumbRange;

/// @brief Captures per-queue GPU marker progress and formats active breadcrumb trees on crash.
class BreadcrumbState;

/// @brief Page-pooled linear arena owning `BreadcrumbNode` instances and copied string payloads.
class BreadcrumbAllocator;

/// @brief Reference-counted handle keeping both a `BreadcrumbNode*` and its owning `BreadcrumbAllocator` alive.
class BreadcrumbNodeRef;

/// @brief RAII scope pushing and popping a breadcrumb event on a `CommandListBase`.
class BreadcrumbScope;

/// @brief Manually ended breadcrumb event spanning non-lexical scopes such as render passes.
class BreadcrumbEventManual;

/// @brief Shader assertion and GPU diagnostic queue buffer shared by hardware backends.
class DiagnosticBuffer;

/// @brief Per-resource memory and flag record used by `ResourceSnapshot` and resource dumps.
struct ResourceStats;

/// @brief System, adapter, and memory summary header stored inside a serialized `ResourceSnapshot`.
struct ResourceSnapshotMetadata;

/// @brief Serializable snapshot of all tracked RHI resources and baseline memory statistics.
struct ResourceSnapshot;

namespace intel
{

/// @brief Bitflags controlling vendor breadcrumb crash dump outputs.
enum class CrashDumpFlags : gp::UInt8;

/// @brief Resolves a `BreadcrumbNode*` into a vendor marker payload and `<root>` fallback.
class Marker;

/// @brief Driver callback table registered by backends supporting Intel GPU crash dumps.
struct BackendCallbacks;

}   // namespace intel

namespace nvidia
{

/// @brief Bitflags controlling GPU crash dump capture features.
enum class FeatureFlags : gp::UInt32;

/// @brief Status returned after waiting for and writing a GPU crash dump.
enum class CrashResult : gp::UInt8;

/// @brief Resolves a `BreadcrumbNode*` into a crash marker pointer and byte size.
class Marker;

/// @brief Opaque 64-bit shader binary hash used to associate crashed GPU instructions with shaders.
using ShaderHash = gp::UInt64;

/// @brief Driver callback table registered by backends supporting NVIDIA GPU crash dumps.
struct BackendCallbacks;

}   // namespace nvidia

}   // namespace gp::rhi::diagnostics
