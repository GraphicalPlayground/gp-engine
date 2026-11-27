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

/// @brief Selects how a newly created GPU buffer initializes its backing memory.
enum class BufferInitAction : gp::UInt8;

/// @brief Dimensionality of an RHI texture resource.
enum class TextureDimension : gp::UInt8;

/// @brief Creation, memory placement, and usage flags for GPU textures.
enum class TextureCreateFlags : gp::UInt64;

/// @brief Identifies one of the six faces of a cubemap texture.
enum class CubeFace : gp::UInt32;

/// @brief Status of an asynchronous 2D texture reallocation request.
enum class TextureReallocationStatus : gp::UInt8;

/// @brief Identifies which data or metadata plane of a texture a view accesses.
enum class TexturePlane : gp::UInt8;

/// @brief Selects how a newly created GPU texture initializes its backing memory.
enum class TextureInitAction : gp::UInt8;

/// @brief Expected update frequency and lifetime of a uniform buffer.
enum class UniformBufferUsage : gp::UInt8;

/// @brief Validation policy applied when constructing or updating a uniform buffer.
enum class UniformBufferValidation : gp::UInt8;

/// @brief Behavior and reflection flags for uniform buffer creation.
enum class UniformBufferFlags : gp::UInt8;

/// @brief Zero-based index identifying a globally bound static uniform buffer slot.
using UniformBufferStaticSlot = gp::UInt8;

/// @brief Selects whether a render target binds a color clear value, a depth-stencil clear value, or none.
enum class ClearBinding : gp::UInt8;

/// @brief Packed depth and stencil read/write access mode for render passes and transitions.
enum class ExclusiveDepthStencilMode : gp::UInt8;

/// @brief Integer 2D coordinate or texel extent.
struct IntPoint;

/// @brief Integer 3D coordinate or texel extent.
struct IntVector;

/// @brief Integer 2D axis-aligned rectangle.
struct IntRect;

/// @brief Four-component unsigned 32-bit integer vector for UAV clears and shader root constants.
struct UIntVector4;

/// @brief Four-channel IEEE-754 half-precision (`UInt16`) RGBA color for float surface readback.
struct Float16Color;

/// @brief Unsigned subresource range `[first, first + num)`.
template <typename InType>
struct Range;

/// @brief 8-bit subresource index range for mip levels.
using Range8 = Range<gp::UInt8>;

/// @brief 16-bit subresource index range for array slices.
using Range16 = Range<gp::UInt16>;

/// @brief Interface supplying CPU-side upload bytes for buffer creation.
class IResourceArrayUpload;

/// @brief Non-owning stack adapter exposing a raw view as `IResourceArrayUpload`.
class ResourceArrayUploadView;

/// @brief Interface supplying bulk CPU-side subresource bytes for texture creation.
class IResourceBulkData;

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

/// @brief Immutable layout descriptor for a GPU buffer resource.
struct BufferDesc;

/// @brief Fluent creation parameters for allocating and initializing a GPU buffer.
struct BufferCreateDesc;

/// @brief Move-only RAII initializer granting writable upload access while creating a GPU buffer.
struct BufferInitializer;

/// @brief Move-only RAII wrapper owning an aligned heap allocation for staged resource initialization.
struct InitializerScopedMemory;

/// @brief Buffer initializer returning the created buffer directly on finalize.
struct DefaultBufferInitializer;

/// @brief Buffer initializer locking the buffer on creation and unlocking it on finalize.
struct LockBufferInitializer;

/// @brief Buffer initializer wrapping a custom backend finalization callback.
struct CustomBufferInitializer;

/// @brief Placeholder buffer returned when an unhandled `BufferInitAction` is encountered.
class InvalidBuffer;

/// @brief Identifies a single texture subresource by face, array slice, and mip level.
struct TextureSubresourceIndex;

/// @brief Streaming callback provider supplying texture subresources during creation.
struct TextureDataProvider;

/// @brief Descriptor capturing dimensionality, format, flags, extent, and clear value of a GPU texture.
struct TextureDesc;

/// @brief Configurable hardware dimension limits used to validate `TextureDesc` parameters.
struct TextureValidationLimits;

/// @brief Fluent creation parameters for allocating and initializing a GPU texture.
struct TextureCreateDesc;

/// @brief Writable subresource pointer, stride, and byte size returned by `TextureInitializer`.
struct TextureSubresourceInitializer;

/// @brief Move-only RAII initializer granting writable subresource upload access while creating a GPU texture.
struct TextureInitializer;

/// @brief Base helper specialization exposing writable staging pointer access on `TextureInitializer`.
struct BaseTextureInitializer;

/// @brief Texture initializer returning the created texture directly on finalize.
struct DefaultTextureInitializer;

/// @brief Texture initializer mapping subresources into a contiguous tightly packed staging buffer.
struct DefaultLayoutTextureInitializer;

/// @brief Texture initializer uploading staged subresources via `lockTexture()` and `unlockTexture()` on finalize.
struct LockTextureInitializer;

/// @brief Parameters identifying a texture subresource region to lock for CPU access.
struct TextureLockArgs;

/// @brief Mapped CPU pointer, row stride, and byte size returned by `DynamicRHI::lockTexture()`.
struct TextureLockResult;

/// @brief Platform-aligned GPU byte size and alignment returned by `DynamicRHI::calcTexturePlatformSize()`.
struct CalcTextureSizeResult;

/// @brief 2D sub-rectangle specification for `updateTexture2D()`.
struct UpdateTextureRegion2D;

/// @brief 3D sub-box specification for `updateTexture3D()`.
struct UpdateTextureRegion3D;

/// @brief Staged CPU allocation descriptor for `beginUpdateTexture3D()` / `endUpdateTexture3D()`.
struct UpdateTexture3DData;

/// @brief Pairs a destination and source resource for atomic backing-memory replacement.
struct ResourceReplaceInfo;

/// @brief RAII helper batching multiple `ResourceReplaceInfo` records onto a command list.
class ResourceReplaceBatcher;

/// @brief Snapshot of dedicated, streaming, and pool GPU texture memory budgets.
struct TextureMemoryStats;

/// @brief Options controlling cube face, mip index, and compression when reading back a render target surface.
struct ReadSurfaceDataFlags;

/// @brief Describes one resource member slot inside a uniform buffer layout.
struct UniformBufferResource;

/// @brief Mutable creation parameters used to construct an immutable `UniformBufferLayout`.
struct UniformBufferLayoutDesc;

/// @brief Immutable layout metadata describing constant data and resource slots in a uniform buffer.
class UniformBufferLayout;

/// @brief Immutable GPU uniform buffer binding constant memory and referenced RHI resources.
class UniformBuffer;

/// @brief Reference-counted base class for all GPU resources managed by the RHI.
class Resource;

/// @brief Base class for RHI resources that support shader views and barrier state tracking.
class ViewableResource;

/// @brief GPU buffer resource.
class Buffer;

/// @brief CPU-readable staging buffer for GPU-to-CPU readbacks.
class StagingBuffer;

/// @brief Indirection slot binding a replaceable vertex stream buffer and byte offset.
class StreamSourceSlot;

/// @brief GPU texture resource.
class Texture;

/// @brief Indirection wrapper referencing another GPU texture.
class TextureReference;

/// @brief Ray tracing acceleration structure resource.
class RayTracingAccelerationStructure;

/// @brief Bottom-level ray tracing geometry acceleration structure resource.
class RayTracingGeometry;

/// @brief Top-level ray tracing acceleration structure scene resource.
class RayTracingScene;

/// @brief Coordinates deferred RHI resource deletion queues and render-thread lifetime extensions.
class ResourceDeletionQueue;

/// @brief RAII scope guard that extends the lifetime of eligible RHI resources on the render thread.
class ScopedResourceLifetimeExtension;

/// @brief Intrusive smart pointer handle to a `Resource`.
using ResourceRef = gp::RefCountedPtr<Resource, gp::DefaultRefCountPolicy<Resource>>;

/// @brief Intrusive smart pointer handle to a `ViewableResource`.
using ViewableResourceRef = gp::RefCountedPtr<ViewableResource, gp::DefaultRefCountPolicy<ViewableResource>>;

/// @brief Intrusive smart pointer handle to a `Buffer`.
using BufferRef = gp::RefCountedPtr<Buffer, gp::DefaultRefCountPolicy<Buffer>>;

/// @brief Intrusive smart pointer handle to a `StagingBuffer`.
using StagingBufferRef = gp::RefCountedPtr<StagingBuffer, gp::DefaultRefCountPolicy<StagingBuffer>>;

/// @brief Intrusive smart pointer handle to a `StreamSourceSlot`.
using StreamSourceSlotRef = gp::RefCountedPtr<StreamSourceSlot, gp::DefaultRefCountPolicy<StreamSourceSlot>>;

/// @brief Intrusive smart pointer handle to a `Texture`.
using TextureRef = gp::RefCountedPtr<Texture, gp::DefaultRefCountPolicy<Texture>>;

/// @brief Intrusive smart pointer handle to a `TextureReference`.
using TextureReferenceRef = gp::RefCountedPtr<TextureReference, gp::DefaultRefCountPolicy<TextureReference>>;

/// @brief Intrusive smart pointer handle to a `UniformBufferLayout`.
using UniformBufferLayoutRef = gp::RefCountedPtr<UniformBufferLayout, gp::DefaultRefCountPolicy<UniformBufferLayout>>;

/// @brief Intrusive smart pointer handle to an immutable `const UniformBufferLayout`.
using UniformBufferLayoutConstRef =
    gp::RefCountedPtr<const UniformBufferLayout, gp::DefaultRefCountPolicy<const UniformBufferLayout>>;

/// @brief Intrusive smart pointer handle to a `UniformBuffer`.
using UniformBufferRef = gp::RefCountedPtr<UniformBuffer, gp::DefaultRefCountPolicy<UniformBuffer>>;

}   // namespace gp::rhi::resource
