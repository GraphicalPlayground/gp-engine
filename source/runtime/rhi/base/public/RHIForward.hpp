// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep

namespace gp::rhi
{

/// @brief Defines the RHI feature level tiers supported by the engine.
enum class FeatureLevel : gp::Int32;

/// @brief Identifies a target shader compilation platform.
enum class ShaderPlatform : gp::UInt16;

/// @brief Defines pixel formats for textures, render targets, and typed views.
enum class PixelFormat : gp::UInt8;

/// @brief Bitmask flags describing the active color, depth, or stencil channels of a PixelFormat.
enum class PixelFormatChannelFlags : gp::UInt8;

/// @brief Graphics backend API implemented by the active RHI.
enum class InterfaceType : gp::UInt8;

/// @brief Runtime availability of an RHI hardware or driver feature.
enum class FeatureSupport : gp::UInt8;

/// @brief Hardware support level for static shader binding layouts.
enum class StaticShaderBindingLayoutSupport : gp::UInt8;

/// @brief PCI vendor IDs for known GPU manufacturers and software rasterizers.
enum class GpuVendorId : gp::UInt32;

/// @brief Scope of bindless descriptor indexing enabled for shaders.
enum class BindlessConfiguration : gp::UInt8;

namespace command
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

/// @brief Flags controlling pipeline barrier and split-transition behavior.
enum class TransitionCreateFlags : gp::UInt32;

/// @brief Per-resource flags applied to a barrier transition.
enum class ResourceTransitionFlags : gp::UInt8;

// Contexts
class IComputeContext;
class ICommandContext;
class IUploadContext;
class ContextArray;

// Command Lists
class CommandListBase;
class ComputeCommandList;
class CommandList;
class CommandListImmediate;
class SubCommandList;

}   // namespace command

namespace resource
{

/// @brief Runtime type tag identifying concrete `Resource` subclasses without RTTI.
enum class ResourceType : gp::UInt8;

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

/// @brief CPU mapping access mode when locking a GPU buffer or texture.
enum class ResourceLockMode : gp::UInt8;

/// @brief Expected update frequency and lifetime of a uniform buffer.
enum class UniformBufferUsage : gp::UInt8;

/// @brief Validation policy applied when constructing or updating a uniform buffer.
enum class UniformBufferValidation : gp::UInt8;

/// @brief Behavior and reflection flags for uniform buffer creation.
enum class UniformBufferFlags : gp::UInt8;

class Resource;
class ViewableResource;
class ResourceCollection;

class Buffer;
class StagingBuffer;
class UniformBuffer;

class Texture;
class TextureReference;

// Resource Management / Streaming
class ResourceReplaceInfo;
class ResourceReplaceBatcher;

}   // namespace resource

namespace view
{

/// @brief Categories of hardware descriptor heaps managed by modern backends.
enum class DescriptorHeapType : gp::UInt8;

/// @brief Concrete view type stored in a descriptor handle.
enum class DescriptorType : gp::UInt8;

/// @brief Bitmask over `DescriptorType` values for descriptor range and heap filtering.
enum class DescriptorTypeMask : gp::UInt16;

class ShaderResourceView;
class UnorderedAccessView;
class DescriptorRange;

}   // namespace view

namespace shader
{

/// @brief Programmable pipeline stage represented by a shader object.
enum class ShaderFrequency : gp::UInt8;

/// @brief Reflected shader resource binding kind extracted from compiled shader metadata.
enum class ShaderCodeResourceBindingType : gp::UInt8;

/// @brief Member types permitted inside a reflected uniform buffer parameter layout.
enum class UniformBufferBaseType : gp::UInt8;

/// @brief Binding models permitted for a uniform buffer layout.
enum class UniformBufferBindingFlags : gp::UInt8;

/// @brief Execution path used to dispatch shader bundles.
enum class ShaderBundleMethod : gp::UInt8;

class Shader;
class ShaderData;
class ShaderLibrary;
class ShaderBundle;

// Stages
class VertexShader;
class PixelShader;
class GeometryShader;
class ComputeShader;
class MeshShader;
class AmplificationShader;
class WorkGraphShader;

// Layouts
struct UniformBufferLayout;

}   // namespace shader

namespace state
{

/// @brief Texture sampling filter mode.
enum class SamplerFilter : gp::UInt8;

/// @brief Texture coordinate wrapping behavior outside `[0, 1]`.
enum class SamplerAddressMode : gp::UInt8;

/// @brief Depth comparison mode for shadow/comparison samplers.
enum class SamplerCompareFunction : gp::UInt8;

/// @brief Polygon rasterization fill mode.
enum class RasterizerFillMode : gp::UInt8;

/// @brief Triangle face culling mode.
enum class RasterizerCullMode : gp::UInt8;

/// @brief Depth clipping vs. clamping behavior outside the near/far planes.
enum class RasterizerDepthClipMode : gp::UInt8;

/// @brief Per-channel color write enable mask for blend targets.
enum class ColorWriteMask : gp::UInt8;

/// @brief Comparison operator used by depth and stencil tests.
enum class CompareFunction : gp::UInt8;

/// @brief Predefined stencil bitmask presets.
enum class StencilMask : gp::UInt8;

/// @brief Operation applied to the stencil buffer on test pass or fail.
enum class StencilOp : gp::UInt8;

/// @brief Arithmetic operation combining source and destination blend terms.
enum class BlendOperation : gp::UInt8;

/// @brief Multiplier applied to source or destination channels during blending.
enum class BlendFactor : gp::UInt8;

/// @brief Component format of a vertex input attribute.
enum class VertexElementType : gp::UInt8;

/// @brief Coarse geometry topology class specified on a graphics pipeline state.
enum class PrimitiveTopologyType : gp::UInt8;

/// @brief Exact primitive topology passed to draw commands.
enum class PrimitiveType : gp::UInt8;

/// @brief Per-axis pixel coarseness for Variable Rate Shading.
enum class VRSAxisShadingRate : gp::UInt8;

/// @brief Combined X/Y shading rate tile dimensions for Variable Rate Shading.
enum class VRSShadingRate : gp::UInt8;

/// @brief Combiner applied between draw-level, primitive-level, and image-level VRS rates.
enum class VRSRateCombiner : gp::UInt8;

/// @brief Encoding format of a screen-space VRS rate attachment.
enum class VRSImageDataType : gp::UInt8;

// Fixed-Function States
class BlendState;
class DepthStencilState;
class RasterizerState;
class SamplerState;

// Initializers
struct SamplerStateInitializer;
struct RasterizerStateInitializer;
struct DepthStencilStateInitializer;
struct BlendStateInitializer;

// Input Assembly
class VertexDeclaration;
class StreamSourceSlot;

// Pipelines
class BoundShaderState;
class GraphicsPipelineState;
class ComputePipelineState;
class WorkGraphPipelineState;

}   // namespace state

namespace raytracing
{

class Geometry;
class Scene;
class Shader;
class PipelineState;
class ShaderBindingTable;

}   // namespace raytracing

namespace sync
{

/// @brief Metric captured by a GPU render query.
enum class RenderQueryType : gp::UInt8;

class GPUFence;
class RenderQuery;
class RenderQueryPool;

}   // namespace sync

namespace system
{

/// @brief Packed color gamut and electro-optical transfer function (EOTF) for swapchain output.
enum class ColorSpaceAndEOTF : gp::UInt8;

class Viewport;
class CustomPresent;

}   // namespace system

}   // namespace gp::rhi
