// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi
{

/// @brief Graphics backend API implemented by the active RHI.
enum class InterfaceType : gp::UInt8
{
    /// @brief No RHI backend is initialized yet.
    NotSet = 0,
    Hidden,
    Null,
    D3D11,
    D3D12,
    Vulkan,
    Metal,
    OpenGL
};

/// @brief Runtime availability of an RHI hardware or driver feature.
enum class FeatureSupport : gp::UInt8
{
    Unsupported = 0,
    RuntimeDependent,
    RuntimeGuaranteed
};

/// @brief Hardware support level for static shader binding layouts.
enum class StaticShaderBindingLayoutSupport : gp::UInt8
{
    Unsupported = 0,
    RayTracingOnly,
    AllShaderTypes
};

/// @brief PCI vendor IDs for known GPU manufacturers and software rasterizers.
enum class GpuVendorId : gp::UInt32
{
    Unknown = 0xFFFFFFFFu,
    NotQueried = 0x00000000u,
    Amd = 0x1002u,
    ImgTec = 0x1010u,
    Nvidia = 0x10DEu,
    Arm = 0x13B5u,
    Broadcom = 0x14E4u,
    Qualcomm = 0x5143u,
    Intel = 0x8086u,
    Apple = 0x106Bu,
    Vivante = 0x7A05u,
    VeriSilicon = 0x1EB1u,
    SamsungAMD = 0x144Du,
    Microsoft = 0x1414u,
    Kazan = 0x10003u,
    Codeplay = 0x10004u,
    Mesa = 0x10005u
};

/// @brief Scope of bindless descriptor indexing enabled for shaders.
/// @details Values are ordered from least to most permissive so comparison operators apply.
enum class BindlessConfiguration : gp::UInt8
{
    Disabled = 0,
    RayTracing,
    /// @brief Ray tracing, materials, and shaders that explicitly opt in.
    Minimal,
    All
};

namespace command
{

/// @brief Action taken on a render target attachment when a render pass begins.
enum class RenderTargetLoadAction : gp::UInt8
{
    /// @brief Existing contents are discarded and left undefined.
    NoAction = 0,
    Load,
    /// @brief Clears to the fast-clear value stored on the resource.
    Clear
};

/// @brief Action taken on a render target attachment when a render pass ends.
enum class RenderTargetStoreAction : gp::UInt8
{
    /// @brief Discards rendered contents without writing back to memory.
    NoAction = 0,
    Store,
    /// @brief Resolves multisampled contents with a box filter before storing.
    MultisampleResolve
};

/// @brief Preset color and depth-stencil load/store combinations for a render pass.
enum class SimpleRenderTargetMode : gp::UInt8
{
    ExistingColorAndDepth = 0,
    UninitializedColorAndDepth,
    UninitializedColorExistingDepth,
    UninitializedColorClearDepth,
    ClearColorExistingDepth,
    ClearColorAndDepth,
    /// @brief Loads existing color and depth, then discards depth writes at pass end.
    ExistingContentsNoDepthStore,
    ExistingColorAndClearDepth,
    ExistingColorAndDepthAndClearStencil
};

/// @brief Targets affected by a depth-stencil clear operation.
enum class ClearDepthStencil : gp::UInt8
{
    Depth = 0,
    Stencil,
    DepthStencil
};

/// @brief Scheduling priority for async compute queue submissions.
enum class AsyncComputePriority : gp::UInt8
{
    Default = 0,
    High
};

/// @brief Driver hint for how much GPU capacity async compute may claim against graphics.
enum class AsyncComputeBudget : gp::UInt8
{
    Least = 0,
    GfxHeavy,
    Balanced,
    ComputeHeavy,
    All
};

/// @brief Driver hint for balancing occupancy between graphics and synchronous compute work.
enum class SyncComputeBudget : gp::UInt8
{
    Default = 0,
    Balanced,
    /// @brief Favors compute occupancy; may serialize concurrent async compute queues.
    ComputeHeavy
};

/// @brief Flags controlling pipeline barrier and split-transition behavior.
enum class TransitionCreateFlags : gp::UInt32
{
    None = 0u,
    /// @brief Omits cross-pipeline fences during the transition.
    NoFence = 1u << 0,
    /// @brief Uses a partial flush instead of a split fence when no work sits between Begin and End.
    NoSplit = 1u << 1,
    AllowDuringRenderPass = 1u << 2,
    /// @brief Allows transitioning from a single prior pipeline after an earlier transition to all pipelines.
    AllowDecayPipelines = 1u << 3
};

/// @brief Per-resource flags applied to a barrier transition.
enum class ResourceTransitionFlags : gp::UInt8
{
    None = 0x00u,
    /// @brief Keeps hardware compression intact so shaders can sample compressed surfaces directly.
    MaintainCompression = 1u << 0,
    /// @brief Leaves the post-transition state untracked by the state tracker.
    IgnoreAfterState = 1u << 1,

    Mask = (1u << 0) | (1u << 1)
};

}   // namespace command

namespace resource
{

/// @brief Runtime type tag identifying concrete `Resource` subclasses without RTTI.
enum class ResourceType : gp::UInt8
{
    None = 0,
    SamplerState,
    RasterizerState,
    DepthStencilState,
    BlendState,
    VertexDeclaration,
    VertexShader,
    MeshShader,
    AmplificationShader,
    PixelShader,
    GeometryShader,
    RayTracingShader,
    ComputeShader,
    GraphicsPipelineState,
    ComputePipelineState,
    RayTracingPipelineState,
    BoundShaderState,
    UniformBufferLayout,
    UniformBuffer,
    Buffer,
    Texture,
    TextureReference,
    GPUFence,
    RenderQuery,
    RenderQueryPool,
    Viewport,
    UnorderedAccessView,
    ShaderResourceView,
    RayTracingGeometry,
    RayTracingScene,
    RayTracingShaderBindingTable,
    StagingBuffer,
    CustomPresent,
    ShaderLibrary,
    ShaderBundle,
    WorkGraphShader,
    WorkGraphPipelineState,
    StreamSourceSlot,
    ResourceCollection,
    DescriptorRange
};

/// @brief Usage, lifetime, and binding flags for GPU buffers.
enum class BufferUsageFlags : gp::UInt32
{
    None = 0u,
    /// @brief Written once at or shortly after creation.
    Static = 1u << 0,
    /// @brief Updated occasionally by the CPU; data lasts until the next update or destruction.
    Dynamic = 1u << 1,
    /// @brief Valid for a single frame; must be rewritten or recreated every frame.
    Volatile = 1u << 2,
    UnorderedAccess = 1u << 3,
    ByteAddressBuffer = 1u << 4,
    SourceCopy = 1u << 5,
    DrawIndirect = 1u << 7,
    ShaderResource = 1u << 8,
    KeepCPUAccessible = 1u << 9,
    FastVRAM = 1u << 10,
    Shared = 1u << 12,
    /// @brief Holds opaque ray tracing acceleration data; cannot bind directly to standard shader stages.
    AccelerationStructure = 1u << 13,
    VertexBuffer = 1u << 14,
    IndexBuffer = 1u << 15,
    StructuredBuffer = 1u << 16,
    MultiGPUAllocate = 1u << 17,
    MultiGPUGraphIgnore = 1u << 18,
    /// @brief Scratch buffer for acceleration structure builds; implies UnorderedAccess.
    RayTracingScratch = (1u << 19) | (1u << 3),
    /// @brief Placeholder without backing GPU memory for streaming.
    NullResource = 1u << 20,
    UniformBuffer = 1u << 21,
    /// @brief Tiled or sparse resource allocated without initial physical memory backing.
    ReservedResource = 1u << 22,

    AnyDynamic = (1u << 1) | (1u << 2)
};

/// @brief Dimensionality of an RHI texture resource.
enum class TextureDimension : gp::UInt8
{
    Texture2D = 0,
    Texture2DArray,
    Texture3D,
    TextureCube,
    TextureCubeArray
};

/// @brief Creation, memory placement, and usage flags for GPU textures.
enum class TextureCreateFlags : gp::UInt64
{
    None = 0ull,
    RenderTargetable = 1ull << 0,
    ResolveTargetable = 1ull << 1,
    DepthStencilTargetable = 1ull << 2,
    ShaderResource = 1ull << 3,
    SRGB = 1ull << 4,
    CPUWritable = 1ull << 5,
    NoTiling = 1ull << 6,
    VideoDecode = 1ull << 7,
    Dynamic = 1ull << 8,
    InputAttachmentRead = 1ull << 9,
    Foveation = 1ull << 10,
    Tiling3D = 1ull << 11,
    /// @brief Exists only in tile memory on TBDR GPUs with no backing allocation in main memory.
    Memoryless = 1ull << 12,
    FastVRAMPartialAlloc = 1ull << 14,
    DisableSRVCreation = 1ull << 15,
    DisableDCC = 1ull << 16,
    UAV = 1ull << 17,
    Presentable = 1ull << 18,
    CPUReadback = 1ull << 19,
    FastVRAM = 1ull << 21,
    /// @brief Allocates virtual address space only; physical pages must be mapped separately.
    Virtual = 1ull << 23,
    /// @brief Creates an RTV per array slice; prevents routing slices via SV_RenderTargetArrayIndex.
    TargetArraySlicesIndependently = 1ull << 24,
    Shared = 1ull << 25,
    NoFastClear = 1ull << 26,
    DepthStencilResolveTarget = 1ull << 27,
    Streamable = 1ull << 28,
    NoFastClearFinalize = 1ull << 29,
    Atomic64Compatible = 1ull << 30,
    ReduceMemoryWithTilingMode = 1ull << 31,
    AtomicCompatible = 1ull << 33,
    External = 1ull << 34,
    MultiGPUGraphIgnore = 1ull << 35,
    /// @brief Tiled or sparse texture created without physical memory backing.
    ReservedResource = 1ull << 37,
    /// @brief Commits physical pages immediately alongside ReservedResource.
    ImmediateCommit = 1ull << 38,
    ForceIntoNonStreamingMemoryTracking = 1ull << 39,
    /// @brief Deliberately evicted on creation for GPU page-fault diagnostics.
    Invalid = 1ull << 40,
    LossyCompression = 1ull << 41,
    LossyCompressionLowBitrate = 1ull << 42
};

/// @brief Identifies one of the six faces of a cubemap texture.
enum class CubeFace : gp::UInt32
{
    PosX = 0,
    NegX,
    PosY,
    NegY,
    PosZ,
    NegZ
};

/// @brief Status of an asynchronous 2D texture reallocation request.
enum class TextureReallocationStatus : gp::UInt8
{
    Succeeded = 0,
    Failed,
    InProgress
};

/// @brief CPU mapping access mode when locking a GPU buffer or texture.
enum class ResourceLockMode : gp::UInt8
{
    ReadOnly = 0,
    WriteOnly,
    WriteOnlyNoOverwrite
};

/// @brief Expected update frequency and lifetime of a uniform buffer.
enum class UniformBufferUsage : gp::UInt8
{
    /// @brief Used for one draw call and then discarded.
    SingleDraw = 0,
    /// @brief Reused across multiple draw calls within the current frame only.
    SingleFrame,
    /// @brief Persisted across multiple frames.
    MultiFrame
};

/// @brief Validation policy applied when constructing or updating a uniform buffer.
enum class UniformBufferValidation : gp::UInt8
{
    None = 0,
    ValidateResources
};

/// @brief Behavior and reflection flags for uniform buffer creation.
enum class UniformBufferFlags : gp::UInt8
{
    None = 0x00u,
    /// @brief Forces a hardware uniform buffer even when the backend uses emulated uniform buffers.
    NoEmulatedUniformBuffer = 1u << 0,
    NeedsReflectedMembers = 1u << 1,
    HasNonGraphOutputs = 1u << 2,
    /// @brief Exposes the uniform buffer to bindless descriptor tables when bindless is active.
    BindlessAccessible = 1u << 4
};

}   // namespace resource

namespace view
{

/// @brief Categories of hardware descriptor heaps managed by modern backends.
enum class DescriptorHeapType : gp::UInt8
{
    Standard = 0,
    Sampler,
    RenderTarget,
    DepthStencil,
    Invalid = 0xFFu
};

/// @brief Concrete view type stored in a descriptor handle.
enum class DescriptorType : gp::UInt8
{
    BufferSRV = 0,
    BufferUAV,
    TypedBufferSRV,
    TypedBufferUAV,
    TextureSRV,
    TextureUAV,
    CBV,
    Sampler,
    AccelerationStructure,
    Invalid
};

/// @brief Bitmask over `DescriptorType` values for descriptor range and heap filtering.
enum class DescriptorTypeMask : gp::UInt16
{
    None = 0u,
    BufferSRV = 1u << 0,
    BufferUAV = 1u << 1,
    TypedBufferSRV = 1u << 2,
    TypedBufferUAV = 1u << 3,
    TextureSRV = 1u << 4,
    TextureUAV = 1u << 5,
    CBV = 1u << 6,
    Sampler = 1u << 7,
    AccelerationStructure = 1u << 8,

    SRV = (1u << 0) | (1u << 2) | (1u << 4),
    UAV = (1u << 1) | (1u << 3) | (1u << 5),
    Standard = ((1u << 0) | (1u << 2) | (1u << 4)) | ((1u << 1) | (1u << 3) | (1u << 5)) | (1u << 6) | (1u << 8)
};

}   // namespace view

namespace shader
{

/// @brief Programmable pipeline stage represented by a shader object.
enum class ShaderFrequency : gp::UInt8
{
    Vertex = 0,
    Mesh = 1,
    Amplification = 2,
    Pixel = 3,
    Geometry = 4,
    Compute = 5,
    RayGen = 6,
    RayMiss = 7,
    RayHitGroup = 8,
    RayCallable = 9,
    WorkGraphRoot = 10,
    WorkGraphComputeNode = 11
};

/// @brief Reflected shader resource binding kind extracted from compiled shader metadata.
enum class ShaderCodeResourceBindingType : gp::UInt8
{
    Invalid = 0,
    SamplerState,
    Texture2D,
    Texture2DArray,
    Texture2DMS,
    Texture3D,
    TextureCube,
    TextureCubeArray,
    TextureMetadata,
    Buffer,
    StructuredBuffer,
    ByteAddressBuffer,
    RaytracingAccelerationStructure,
    RWTexture2D,
    RWTexture2DArray,
    RWTexture3D,
    RWTextureCube,
    RWTextureMetadata,
    RWBuffer,
    RWStructuredBuffer,
    RWByteAddressBuffer,
    RasterizerOrderedTexture2D,
    ResourceCollection,
    DescriptorRange,
    Texture2DMultiView,
    RWTexture2DMultiView
};

/// @brief Member types permitted inside a reflected uniform buffer parameter layout.
enum class UniformBufferBaseType : gp::UInt8
{
    Invalid = 0,
    /// @brief Reserved to diagnose illegal `bool` members in shader parameter structs.
    Bool,
    Int32,
    UInt32,
    Float32,
    Texture,
    SRV,
    UAV,
    Sampler,
    NestedStruct,
    IncludedStruct,
    ReferencedStruct,
    RenderTargetBindingSlots,
    ResourceCollection,
    DescriptorRange,
    UniformBlock
};

/// @brief Binding models permitted for a uniform buffer layout.
enum class UniformBufferBindingFlags : gp::UInt8
{
    None = 0x00u,
    /// @brief Bound per shader via `setShaderUniformBuffer`.
    Shader = 1u << 0,
    /// @brief Bound globally through a static slot.
    Static = 1u << 1,
    /// @brief Allows either binding model; validation rejects using both simultaneously on one instance.
    StaticAndShader = (1u << 0) | (1u << 1)
};

/// @brief Execution path used to dispatch shader bundles.
enum class ShaderBundleMethod : gp::UInt8
{
    Emulation = 0,
    Native = 1
};

}   // namespace shader

namespace state
{

/// @brief Texture sampling filter mode.
enum class SamplerFilter : gp::UInt8
{
    Point = 0,
    Bilinear,
    Trilinear,
    AnisotropicPoint,
    AnisotropicLinear
};

/// @brief Texture coordinate wrapping behavior outside `[0, 1]`.
enum class SamplerAddressMode : gp::UInt8
{
    Wrap = 0,
    Clamp,
    Mirror,
    /// @brief Not supported on all backends.
    Border
};

/// @brief Depth comparison mode for shadow/comparison samplers.
enum class SamplerCompareFunction : gp::UInt8
{
    Never = 0,
    Less
};

/// @brief Polygon rasterization fill mode.
enum class RasterizerFillMode : gp::UInt8
{
    Point = 0,
    Wireframe,
    Solid
};

/// @brief Triangle face culling mode.
enum class RasterizerCullMode : gp::UInt8
{
    None = 0,
    CW,
    CCW
};

/// @brief Depth clipping vs. clamping behavior outside the near/far planes.
enum class RasterizerDepthClipMode : gp::UInt8
{
    DepthClip = 0,
    DepthClamp
};

/// @brief Per-channel color write enable mask for blend targets.
enum class ColorWriteMask : gp::UInt8
{
    None = 0x00u,
    Red = 0x01u,
    Green = 0x02u,
    Blue = 0x04u,
    Alpha = 0x08u,
    RGB = 0x01u | 0x02u | 0x04u,
    RGBA = 0x01u | 0x02u | 0x04u | 0x08u,
    RG = 0x01u | 0x02u,
    BA = 0x04u | 0x08u
};

/// @brief Comparison operator used by depth and stencil tests.
enum class CompareFunction : gp::UInt8
{
    Less = 0,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    Never,
    Always,

    /// @brief Reversed-Z depth aliases.
    DepthNearOrEqual = GreaterEqual,
    DepthNear = Greater,
    DepthFartherOrEqual = LessEqual,
    DepthFarther = Less
};

/// @brief Predefined stencil bitmask presets.
enum class StencilMask : gp::UInt8
{
    Default = 0,
    Mask255,
    Mask1,
    Mask2,
    Mask4,
    Mask8,
    Mask16,
    Mask32,
    Mask64,
    Mask128
};

/// @brief Operation applied to the stencil buffer on test pass or fail.
enum class StencilOp : gp::UInt8
{
    Keep = 0,
    Zero,
    Replace,
    SaturatedIncrement,
    SaturatedDecrement,
    Invert,
    Increment,
    Decrement
};

/// @brief Arithmetic operation combining source and destination blend terms.
enum class BlendOperation : gp::UInt8
{
    Add = 0,
    Subtract,
    Min,
    Max,
    ReverseSubtract
};

/// @brief Multiplier applied to source or destination channels during blending.
enum class BlendFactor : gp::UInt8
{
    Zero = 0,
    One,
    SourceColor,
    InverseSourceColor,
    SourceAlpha,
    InverseSourceAlpha,
    DestAlpha,
    InverseDestAlpha,
    DestColor,
    InverseDestColor,
    ConstantBlendFactor,
    InverseConstantBlendFactor,
    Source1Color,
    InverseSource1Color,
    Source1Alpha,
    InverseSource1Alpha
};

/// @brief Component format of a vertex input attribute.
enum class VertexElementType : gp::UInt8
{
    None = 0,
    Float1,
    Float2,
    Float3,
    Float4,
    PackedNormal,
    UByte4,
    UByte4N,
    Color,
    Short2,
    Short4,
    /// @brief Signed 16-bit normalized pair `(value / 32767.0, value / 32767.0, 0, 1)`.
    Short2N,
    Half2,
    Half4,
    Short4N,
    UShort2,
    UShort4,
    /// @brief Unsigned 16-bit normalized pair `(value / 65535.0, value / 65535.0, 0, 1)`.
    UShort2N,
    UShort4N,
    /// @brief 10-bit RGB and 2-bit alpha normalized unsigned integer.
    URGB10A2N,
    UInt,
    UInt2
};

/// @brief Coarse geometry topology class specified on a graphics pipeline state.
enum class PrimitiveTopologyType : gp::UInt8
{
    Triangle = 0,
    Patch,
    Line,
    Point
};

/// @brief Exact primitive topology passed to draw commands.
enum class PrimitiveType : gp::UInt8
{
    TriangleList = 0,
    TriangleStrip,
    LineList,
    /// @brief Supported only when the backend reports quad topology support.
    QuadList,
    PointList,
    /// @brief Three-vertex screen-aligned rectangle (upper-left, upper-right, lower-left); backend-dependent.
    RectList
};

/// @brief Per-axis pixel coarseness for Variable Rate Shading.
enum class VRSAxisShadingRate : gp::UInt8
{
    Rate1X = 0x0u,
    Rate2X = 0x1u,
    Rate4X = 0x2u
};

/// @brief Combined X/Y shading rate tile dimensions for Variable Rate Shading.
enum class VRSShadingRate : gp::UInt8
{
    Rate1x1 = (0x0u << 2) | 0x0u,
    Rate1x2 = (0x0u << 2) | 0x1u,
    Rate2x1 = (0x1u << 2) | 0x0u,
    Rate2x2 = (0x1u << 2) | 0x1u,
    Rate2x4 = (0x1u << 2) | 0x2u,
    Rate4x2 = (0x2u << 2) | 0x1u,
    Rate4x4 = (0x2u << 2) | 0x2u
};

/// @brief Combiner applied between draw-level, primitive-level, and image-level VRS rates.
enum class VRSRateCombiner : gp::UInt8
{
    Passthrough = 0,
    Override,
    Min,
    Max,
    Sum
};

/// @brief Encoding format of a screen-space VRS rate attachment.
enum class VRSImageDataType : gp::UInt8
{
    NotSupported = 0,
    Palette,
    Fractional
};

}   // namespace state

namespace sync
{

/// @brief Metric captured by a GPU render query.
enum class RenderQueryType : gp::UInt8
{
    Undefined = 0,
    /// @brief Number of samples passing depth/stencil tests (divide by MSAA sample count for pixels).
    Occlusion,
    /// @brief GPU timestamp reported in microseconds.
    AbsoluteTime
};

}   // namespace sync

namespace system
{

/// @brief Packed color gamut and electro-optical transfer function (EOTF) for swapchain output.
enum class ColorSpaceAndEOTF : gp::UInt8
{
    Unknown = 0x00u,

    ColorSpaceRec709 = 1u,
    ColorSpaceRec2020 = 2u,
    ColorSpaceDCIP3 = 3u,
    ColorSpaceMask = 0x0Fu,

    EOTFLinear = 1u << 4,
    EOTFsRGB = 2u << 4,
    EOTFPQ = 3u << 4,
    EOTFMask = 0xF0u,

    Rec709sRGB = 1u | (2u << 4),
    Rec709Linear = 1u | (1u << 4),

    Rec2020PQ = 2u | (3u << 4),
    Rec2020Linear = 2u | (1u << 4),

    DCIP3PQ = 3u | (3u << 4),
    DCIP3Linear = 3u | (1u << 4)
};

}   // namespace system

}   // namespace gp::rhi

GP_ENABLE_ENUM_COMPARISON_OPERATIONS(gp::rhi::BindlessConfiguration);

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::command::TransitionCreateFlags);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::command::ResourceTransitionFlags);

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::resource::BufferUsageFlags);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::resource::TextureCreateFlags);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::resource::UniformBufferFlags);

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::view::DescriptorTypeMask);

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::shader::UniformBufferBindingFlags);

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::state::ColorWriteMask);

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::system::ColorSpaceAndEOTF);
