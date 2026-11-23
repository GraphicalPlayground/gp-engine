// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "memory/MemoryForward.hpp"

namespace gp::rhi::resource
{

/// @brief Runtime type tag identifying concrete `Resource` subclasses without RTTI.
enum class ResourceType : gp::UInt8;

/// @brief CPU mapping access mode when locking a GPU buffer or texture.
enum class ResourceLockMode : gp::UInt8;

/// @brief Usage, lifetime, and binding flags for GPU buffers.
enum class BufferUsageFlags : gp::UInt32;

/// @brief Dimensionality of an RHI texture resource.
enum class TextureDimension : gp::UInt8;

/// @brief Creation, memory placement, and usage flags for GPU textures.
enum class TextureCreateFlags : gp::UInt64;

/// @brief Identifies one of the six faces of a cubemap texture.
enum class CubeFace : gp::UInt32;

/// @brief Status of an asynchronous 2D texture reallocation request.
enum class TextureReallocationStatus : gp::UInt8;

/// @brief Expected update frequency and lifetime of a uniform buffer.
enum class UniformBufferUsage : gp::UInt8;

/// @brief Validation policy applied when constructing or updating a uniform buffer.
enum class UniformBufferValidation : gp::UInt8;

/// @brief Behavior and reflection flags for uniform buffer creation.
enum class UniformBufferFlags : gp::UInt8;

/// @brief Selects whether a render target binds a color clear value, a depth-stencil clear value, or none.
enum class ClearBinding : gp::UInt8;

/// @brief Packed depth and stencil read/write access mode for render passes and transitions.
enum class ExclusiveDepthStencilMode : gp::UInt8;

/// @brief Pairs a buffer or allocation byte size with its element stride.
struct SizeAndStride;

/// @brief Describes the start offset and byte span of a GPU video memory allocation.
struct VRamAllocation;

/// @brief Diagnostic snapshot describing an RHI resource's identity, type, and VRAM footprint.
struct ResourceInfo;

/// @brief Tagged clear value bound to a color or depth-stencil render target.
struct ClearValueBinding;

/// @brief Bitmask wrapper describing exclusive depth and stencil access states.
class ExclusiveDepthStencil;

/// @brief Reference-counted base class for all GPU resources managed by the RHI.
class Resource;

/// @brief Base class for RHI resources that support shader views and barrier state tracking.
class ViewableResource;

/// @brief GPU buffer resource.
class Buffer;

/// @brief GPU texture resource.
class Texture;

/// @brief Indirection wrapper referencing another GPU texture.
class TextureReference;

/// @brief Ray tracing acceleration structure resource.
class RayTracingAccelerationStructure;

/// @brief Coordinates deferred RHI resource deletion queues and render-thread lifetime extensions.
class ResourceDeletionQueue;

/// @brief RAII scope guard that extends the lifetime of eligible RHI resources on the render thread.
class ScopedResourceLifetimeExtension;

/// @brief Intrusive smart pointer handle to a `Resource`.
using ResourceRef = gp::RefCountedPtr<Resource, gp::DefaultRefCountPolicy<Resource>>;

}   // namespace gp::rhi::resource
