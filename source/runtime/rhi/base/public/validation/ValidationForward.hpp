// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi
{

/// @brief Validation wrapper around a backend `IComputeContext`.
class ValidationComputeContext;

/// @brief Validation wrapper around a backend `ICommandContext`.
class ValidationContext;

/// @brief Validation wrapper `DynamicRHI` auditing barriers, transient aliasing, render passes, and bindings.
class ValidationRHI;

}   // namespace gp::rhi

namespace gp::rhi::validation
{

/// @brief Controls how validation state tracker operations are logged.
enum class LoggingMode : gp::UInt8;

/// @brief Logical subresource plane tracked independently during barrier validation.
enum class ResourcePlane : gp::UInt8;

/// @brief Lifecycle status of a transient heap-backed resource.
enum class TransientStatus : gp::UInt8;

/// @brief Distinguishes graphics vs compute UAV slot tables inside `Tracker`.
enum class UAVMode : gp::UInt8;

/// @brief Discriminator for deferred validation state-machine operations.
enum class OpType : gp::UInt8;

/// @brief Coordinates of a single `(mip, arraySlice, plane)` subresource.
struct SubresourceIndex;

/// @brief Pairs a `sync::ResourceAccess` mask with its owning `sync::PipelineStage` mask.
struct State;

/// @brief Contiguous `(mip, slice, plane)` range within a tracked `Resource`.
struct SubresourceRange;

/// @brief Pairs a tracked `Resource*` with a `SubresourceRange`.
struct ResourceIdentity;

/// @brief Extends `ResourceIdentity` with view element stride metadata.
struct ViewIdentity;

/// @brief Synchronization token tracking cross-pipeline split transitions.
struct Fence;

/// @brief Tracks global static uniform buffer slots on a validation context.
struct StaticUniformBuffers;

/// @brief Tracks uniform buffers bound on a single programmable shader stage.
struct StageBoundUniformBuffers;

/// @brief Aggregates `StageBoundUniformBuffers` across all shader frequencies.
struct BoundUniformBuffers;

/// @brief Per-subresource barrier, split-transition, and UAV-overlap state machine.
struct SubresourceState;

/// @brief Per-resource transient acquire/discard/aliasing state machine.
struct TransientState;

/// @brief Base barrier and subresource state tracker attached to buffers, textures, and acceleration structures.
class Resource;

/// @brief Barrier state tracker specialization for `resource::Buffer`.
class BufferResource;

/// @brief Barrier state tracker specialization for `resource::Texture` (and `TextureReference` indirection).
class TextureResource;

/// @brief Frame-lifetime validator embedded in `resource::UniformBuffer`.
class UniformBufferResource;

/// @brief Tagged state-tracker command replayed in queue submission order.
struct Operation;

/// @brief Ordered batch of `Operation` records attached to transitions or command lists.
class OperationsList;

/// @brief Validation operations embedded inside `sync::Transition`.
struct TransitionResource;

/// @brief Per-pipeline resource barrier, UAV overlap, and transient aliasing state tracker.
class Tracker;

/// @brief Stateless validation helpers for texture/buffer copies and views.
class ValidationUtils;

}   // namespace gp::rhi::validation
