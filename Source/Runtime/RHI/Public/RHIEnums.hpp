// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreTypes.hpp"
#include "RHIBuild.hpp"
#include "Templates/Utility/Enums.hpp"

namespace GP
{

/// @brief Identifies the underlying graphics API backend.
enum class ERHIBackend : UInt8
{
    Vulkan,
    D3D12,
    D3D11,
    Metal,
    OpenGL,
    OpenGLES,
    WebGPU,
};

/// @brief Logical GPU command queue type. Maps to a specific Vulkan queue family or D3D12 command-list type.
enum class ECommandQueueType : UInt8
{
    Graphics,      //<! Full graphics + compute + transfer support.
    Compute,       //<! Async compute; no rasterisation hardware.
    Transfer,      //<! DMA copy engine; no compute or draw commands.
    VideoDecode,   //<! Hardware video decode (optional capability).
    VideoEncode,   //<! Hardware video encode (optional capability).
};

/// @brief Specifies which command queue type a command list targets.
using ECommandListType = ECommandQueueType;

/// @brief Pixel / data format. Naming follows DXGI conventions for maximum clarity.
/// @note  Unless suffixed _TYPELESS or _UNORM, packed component layouts are little-endian
///        (least-significant channel first in memory) on all supported platforms.
enum class EFormat : UInt16
{
    Undefined = 0,

    // 8-bit per channel
    R8_UNORM,
    R8_SNORM,
    R8_UINT,
    R8_SINT,
    RG8_UNORM,
    RG8_SNORM,
    RG8_UINT,
    RG8_SINT,
    RGBA8_UNORM,
    RGBA8_SNORM,
    RGBA8_UINT,
    RGBA8_SINT,
    RGBA8_UNORM_SRGB,
    BGRA8_UNORM,
    BGRA8_UNORM_SRGB,

    // 16-bit per channel
    R16_UNORM,
    R16_SNORM,
    R16_UINT,
    R16_SINT,
    R16_FLOAT,
    RG16_UNORM,
    RG16_SNORM,
    RG16_UINT,
    RG16_SINT,
    RG16_FLOAT,
    RGBA16_UNORM,
    RGBA16_SNORM,
    RGBA16_UINT,
    RGBA16_SINT,
    RGBA16_FLOAT,

    // 32-bit per channel
    R32_UINT,
    R32_SINT,
    R32_FLOAT,
    RG32_UINT,
    RG32_SINT,
    RG32_FLOAT,
    RGB32_UINT,
    RGB32_SINT,
    RGB32_FLOAT,
    RGBA32_UINT,
    RGBA32_SINT,
    RGBA32_FLOAT,

    // 64-bit per channel
    RG64_UINT,
    RG64_SINT,
    RG64_FLOAT,
    RGBA64_UINT,
    RGBA64_SINT,
    RGBA64_FLOAT,

    // Packed
    RGB10A2_UNORM,
    RGB10A2_UINT,
    B5G6R5_UNORM,
    B5G5R5A1_UNORM,
    B4G4R4A4_UNORM,
    R11G11B10_FLOAT,
    RGB9E5_FLOAT,   //<! Shared-exponent HDR (read-only).
    A8_UNORM,

    // Depth / Stencil
    D16_UNORM,
    D24_UNORM_S8_UINT,
    D32_FLOAT,
    D32_FLOAT_S8_UINT,
    S8_UINT,

    // BC (Desktop)
    BC1_UNORM,
    BC1_UNORM_SRGB,
    BC2_UNORM,
    BC2_UNORM_SRGB,
    BC3_UNORM,
    BC3_UNORM_SRGB,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC6H_UF16,
    BC6H_SF16,
    BC7_UNORM,
    BC7_UNORM_SRGB,

    // ETC2 / EAC (Android / OpenGLES)
    ETC2_RGB8_UNORM,
    ETC2_RGB8_UNORM_SRGB,
    ETC2_RGB8A1_UNORM,
    ETC2_RGB8A1_UNORM_SRGB,
    ETC2_RGBA8_UNORM,
    ETC2_RGBA8_UNORM_SRGB,
    EAC_R11_UNORM,
    EAC_R11_SNORM,
    EAC_RG11_UNORM,
    EAC_RG11_SNORM,

    // ASTC (Mobile / Apple)
    ASTC_4x4_UNORM,
    ASTC_4x4_UNORM_SRGB,
    ASTC_4x4_FLOAT,
    ASTC_5x4_UNORM,
    ASTC_5x4_UNORM_SRGB,
    ASTC_5x5_UNORM,
    ASTC_5x5_UNORM_SRGB,
    ASTC_5x5_FLOAT,
    ASTC_6x5_UNORM,
    ASTC_6x5_UNORM_SRGB,
    ASTC_6x6_UNORM,
    ASTC_6x6_UNORM_SRGB,
    ASTC_6x6_FLOAT,
    ASTC_8x5_UNORM,
    ASTC_8x5_UNORM_SRGB,
    ASTC_8x6_UNORM,
    ASTC_8x6_UNORM_SRGB,
    ASTC_8x8_UNORM,
    ASTC_8x8_UNORM_SRGB,
    ASTC_8x8_FLOAT,
    ASTC_10x5_UNORM,
    ASTC_10x5_UNORM_SRGB,
    ASTC_10x6_UNORM,
    ASTC_10x6_UNORM_SRGB,
    ASTC_10x8_UNORM,
    ASTC_10x8_UNORM_SRGB,
    ASTC_10x10_UNORM,
    ASTC_10x10_UNORM_SRGB,
    ASTC_10x10_FLOAT,
    ASTC_12x10_UNORM,
    ASTC_12x10_UNORM_SRGB,
    ASTC_12x12_UNORM,
    ASTC_12x12_UNORM_SRGB,
    ASTC_12x12_FLOAT,

    Count,
};

/// @brief Bit-field describing how a specific EFormat may be used on this device.
enum class EFormatSupport : UInt32
{
    None = 0,
    Sampled = 1 << 0,
    Storage = 1 << 1,
    ColorAttachment = 1 << 2,
    DepthStencil = 1 << 3,
    BlitSource = 1 << 4,
    BlitDest = 1 << 5,
    FilterLinear = 1 << 6,   ///< Linear filtering in sampler.
    FilterCubic = 1 << 7,
    MsaaColorAttachment = 1 << 8,
    TransferSource = 1 << 9,
    TransferDest = 1 << 10,
    VertexBuffer = 1 << 11,
    AtomicReadWrite = 1 << 12,   ///< Image atomic ops (storage images).
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EFormatSupport);

/// @brief Describes how a buffer may be bound and used.
enum class EBufferUsage : UInt32
{
    None = 0,
    VertexBuffer = 1 << 0,        //<! Vertex input data.
    IndexBuffer = 1 << 1,         //<! Index data.
    UniformBuffer = 1 << 2,       //<! Constant / uniform block (CBV/UBO).
    StorageBuffer = 1 << 3,       //<! Read-write shader storage (UAV/SSBO).
    StorageBufferRead = 1 << 4,   //<! Read-only storage / StructuredBuffer.
    IndirectBuffer = 1 << 5,      //<! Indirect draw / dispatch arguments.
    TransferSrc = 1 << 6,         //<! Source of copy operations.
    TransferDst = 1 << 7,         //<! Destination of copy operations.
    /// @note Requires GP_RHI_ENABLE_RAY_TRACING.
    AccelerationStructure = 1 << 8,        //<! BLAS / TLAS storage.
    AccelerationStructureBuild = 1 << 9,   //<! Scratch / build-input buffer.
    ShaderBindingTable = 1 << 10,          //<! SBT for ray tracing.
    /// @note Buffer device address (VK_KHR_buffer_device_address / D3D12 VA).
    DeviceAddress = 1 << 11,
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EBufferUsage);

/// @brief Describes how a texture may be bound and used.
enum class ETextureUsage : UInt32
{
    None = 0,
    RenderTarget = 1 << 0,          //<! Color attachment.
    DepthStencil = 1 << 1,          //<! Depth / stencil attachment.
    ShaderResource = 1 << 2,        //<! Read-only sampled texture (SRV).
    UnorderedAccess = 1 << 3,       //<! Read-write storage image (UAV).
    TransferSrc = 1 << 4,           //<! Copy source.
    TransferDst = 1 << 5,           //<! Copy destination.
    ShadingRate = 1 << 6,           //<! Variable-rate shading image source.
    TransientAttachment = 1 << 7,   //<! On-chip / memoryless attachment (tile-based GPUs).
    InputAttachment = 1 << 8,       //<! Subpass input (Vulkan subpass feature).
    GenerateMips = 1 << 9,          //<! Allow automatic mip generation.
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(ETextureUsage);

/// @brief Dimensionality and topology of a texture resource.
enum class ETextureType : UInt8
{
    Texture1D,
    Texture1DArray,
    Texture2D,
    Texture2DArray,
    Texture2DMS,   //<! Multi-sampled 2-D.
    Texture2DMSArray,
    Texture3D,
    TextureCube,
    TextureCubeArray,
};

/// @brief Aspect bits of a texture (color, depth, stencil planes).
enum class ETextureAspect : UInt8
{
    None = 0,
    Color = 1 << 0,
    Depth = 1 << 1,
    Stencil = 1 << 2,
    All = Color | Depth | Stencil,
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(ETextureAspect);

/// @brief Represents the pipeline stage and access pattern of a resource at a synchronization point.
/// @note Each value maps 1:1 to a Vulkan image-layout + pipeline-stage + access-mask triplet
///       and to a D3D12 resource-state flag.
enum class EResourceState : UInt32
{
    Undefined = 0,
    General = 1 << 0,   //<! Any use; avoids transitions but may penalise hardware.

    // Render targets
    RenderTarget = 1 << 1,   //<! Color attachment write.
    DepthWrite = 1 << 2,     //<! Depth-stencil attachment read/write.
    DepthRead = 1 << 3,      //<! Depth-stencil read-only (e.g. depth prepass sampling).

    // Shader access
    ShaderRead = 1 << 4,         //<! Read in any shader stage (SRV / sampled image).
    UnorderedAccess = 1 << 5,    //<! Read-write in any shader stage (UAV / storage).
    VertexBuffer = 1 << 6,       //<! Bound as vertex input.
    IndexBuffer = 1 << 7,        //<! Bound as index input.
    IndirectArgument = 1 << 8,   //<! Indirect draw / dispatch source.
    UniformBuffer = 1 << 9,      //<! Constant / uniform buffer read.

    // Transfer
    CopySource = 1 << 10,
    CopyDest = 1 << 11,
    ResolveSrc = 1 << 12,
    ResolveDst = 1 << 13,

    // Presentation
    Present = 1 << 14,

    // Variable-rate shading
    ShadingRate = 1 << 15,

    // Ray tracing
    AccelerationStructureRead = 1 << 16,
    AccelerationStructureWrite = 1 << 17,

    // Video
    VideoDecodeRead = 1 << 18,
    VideoDecodeWrite = 1 << 19,
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EResourceState);

/// @brief Individual shader stage identifier.
enum class EShaderStage : UInt32
{
    None = 0,
    Vertex = 1 << 0,
    TessControl = 1 << 1,   //<! Hull shader (D3D) / tessellation-control (GL / Vulkan).
    TessEval = 1 << 2,      //<! Domain shader (D3D) / tessellation-evaluation (GL / Vulkan).
    Geometry = 1 << 3,
    Fragment = 1 << 4,      //<! Pixel shader (D3D).
    Compute = 1 << 5,
    Task = 1 << 6,          //<! Amplification shader (D3D) / task shader (Vulkan).
    Mesh = 1 << 7,
    RayGen = 1 << 8,
    RayMiss = 1 << 9,
    RayClosestHit = 1 << 10,
    RayAnyHit = 1 << 11,
    RayIntersection = 1 << 12,
    Callable = 1 << 13,

    /// @brief Shorthand for all graphics rasterisation stages (no compute / RT).
    AllGraphics = Vertex | TessControl | TessEval | Geometry | Fragment,

    /// @brief All supported stages.
    All =
        AllGraphics | Compute | Task | Mesh | RayGen | RayMiss | RayClosestHit | RayAnyHit | RayIntersection | Callable,
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EShaderStage);

/// @brief Input primitive assembly topology.
enum class EPrimitiveTopology : UInt8
{
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan,
    LineListAdjacency,
    LineStripAdjacency,
    TriangleListAdjacency,
    TriangleStripAdjacency,
    PatchList,   //<! For tessellation, patch-vertex count set in TessellationDesc.
};

/// @brief Element stepping rate for vertex input.
enum class EVertexInputRate : UInt8
{
    PerVertex,
    PerInstance,
};

/// @brief Index buffer element width.
enum class EIndexType : UInt8
{
    UInt16,
    UInt32,
    UInt8,   //<! Optional; requires EXT_index_type_uint8 (Vulkan) or SM 6.5+ (D3D12).
};

/// @brief Multi-sample anti-aliasing (MSAA) count.
enum class ESampleCount : UInt8
{
    Count1 = 1,
    Count2 = 2,
    Count4 = 4,
    Count8 = 8,
    Count16 = 16,
    Count32 = 32,
    Count64 = 64,
};

enum class EFillMode : UInt8
{
    Solid,
    Wireframe,
    Point,
};

enum class ECullMode : UInt8
{
    None,
    Front,
    Back,
};

enum class EFrontFace : UInt8
{
    CounterClockwise,
    Clockwise,
};

enum class ECompareFunc : UInt8
{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

enum class EStencilOp : UInt8
{
    Keep,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert,
    IncrementWrap,
    DecrementWrap,
};

enum class EBlendFactor : UInt8
{
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha,
    SrcAlphaSaturate,
    Src1Color,
    OneMinusSrc1Color,
    Src1Alpha,
    OneMinusSrc1Alpha,
};

enum class EBlendOp : UInt8
{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class ELogicOp : UInt8
{
    Clear,
    And,
    AndReverse,
    Copy,
    AndInverted,
    NoOp,
    Xor,
    Or,
    Nor,
    Equivalent,
    Invert,
    OrReverse,
    CopyInverted,
    OrInverted,
    Nand,
    Set,
};

enum class EColorWriteMask : UInt8
{
    None = 0,
    Red = 1 << 0,
    Green = 1 << 1,
    Blue = 1 << 2,
    Alpha = 1 << 3,
    All = Red | Green | Blue | Alpha,
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EColorWriteMask);

enum class EFilter : UInt8
{
    Nearest,
    Linear,
    Cubic,   //<! Requires extension on some backends.
};

enum class EMipMapMode : UInt8
{
    Nearest,
    Linear,
};

enum class EAddressMode : UInt8
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
    MirrorClampToEdge,
};

enum class EBorderColor : UInt8
{
    TransparentBlack,
    OpaqueBlack,
    OpaqueWhite,
};

enum class EReductionMode : UInt8
{
    WeightedAverage,
    Min,
    Max,
};

/// @brief Type of a single binding within a descriptor set layout.
enum class EDescriptorType : UInt8
{
    Sampler,
    CombinedImageSampler,
    SampledImage,            //<! SRV texture.
    StorageImage,            //<! UAV texture.
    UniformTexelBuffer,      //<! Read-only typed buffer view.
    StorageTexelBuffer,      //<! Read-write typed buffer view.
    UniformBuffer,           //<! CBV / UBO.
    StorageBuffer,           //<! SSBO / structured buffer.
    UniformBufferDynamic,    //<! CBV with per-draw offset.
    StorageBufferDynamic,    //<! SSBO with per-draw offset.
    InputAttachment,         //<! Subpass input (Vulkan-specific).
    AccelerationStructure,   //<! Ray-tracing TLAS.
};

/// @brief Flags that modify descriptor set layout binding behaviour.
enum class EDescriptorFlags : UInt32
{
    None = 0,
    UpdateAfterBind = 1 << 0,            //<! Descriptors may be updated after binding.
    UpdateUnusedWhilePending = 1 << 1,   //<! Update allowed while GPU uses other set elements.
    PartiallyBound = 1 << 2,             //<! Allows sparse arrays (bindless).
    VariableDescriptorCount = 1 << 3,    //<! Last binding may have a variable count.
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EDescriptorFlags);

/// @brief Action performed on an attachment when the render pass begins.
enum class ELoadOp : UInt8
{
    Load,       //<! Preserve existing contents.
    Clear,      //<! Fill with a clear value.
    DontCare,   //<! Contents are undefined; optimal for tile-based GPUs.
};

/// @brief Action performed on an attachment when the render pass ends.
enum class EStoreOp : UInt8
{
    Store,             //<! Write results to memory.
    DontCare,          //<! Results are discarded (on-chip only).
    None,              //<! Attachment is not written (read-only depth).
    Resolve,           //<! MSAA resolve to the resolve attachment.
    StoreAndResolve,   //<! Store AND resolve simultaneously.
};

/// @brief GPU memory domain.
enum class EMemoryType : UInt8
{
    DeviceLocal,     //<! GPU VRAM; fastest read/write from GPU. Not CPU-visible.
    HostVisible,     //<! CPU-writable; may be slower for GPU access (pinned / BAR memory).
    HostCoherent,    //<! CPU-writable without explicit flush/invalidate.
    HostCached,      //<! CPU-readable with page cache (good for readback).
    LazyAllocated,   //<! Allocated only when accessed (tile-based memoryless attachments).
    Shared,          //<! Shared between CPU and GPU (unified memory, Apple Silicon / ARM).
};

/// @brief Optional allocation behaviour modifiers.
enum class EMemoryFlags : UInt32
{
    None = 0,
    Dedicated = 1 << 0,    //<! Request a dedicated allocation (large render targets).
    NeverAlias = 1 << 1,   //<! Prevent aliasing with other resources.
    Mappable = 1 << 2,     //<! Must be persistently mappable by the CPU.
    Sequential = 1 << 3,   //<! Coherent sequential-write access pattern.
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EMemoryFlags);

/// @brief Pipeline state fields that may be set dynamically at draw time.
enum class EDynamicState : UInt32
{
    None = 0,
    Viewport = 1 << 0,
    Scissor = 1 << 1,
    LineWidth = 1 << 2,
    DepthBias = 1 << 3,
    BlendConstants = 1 << 4,
    DepthBounds = 1 << 5,
    StencilCompareMask = 1 << 6,
    StencilWriteMask = 1 << 7,
    StencilReference = 1 << 8,
    CullMode = 1 << 9,
    FrontFace = 1 << 10,
    PrimitiveTopology = 1 << 11,
    VertexInputBindings = 1 << 12,
    DepthTestEnable = 1 << 13,
    DepthWriteEnable = 1 << 14,
    DepthCompareOp = 1 << 15,
    ShadingRate = 1 << 16,
    FragmentShadingRate = 1 << 16,   //<! Alias.
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EDynamicState);

/// @brief Fragment shading rates; notation is WxH (pixel tiles per shader invocation).
enum class EShadingRate : UInt8
{
    Rate_1x1,
    Rate_1x2,
    Rate_2x1,
    Rate_2x2,
    Rate_2x4,
    Rate_4x2,
    Rate_4x4,
};

/// @brief Combiner operation applied between two shading rate sources.
enum class EShadingRateCombiner : UInt8
{
    Keep,      //<! Use the first source rate unchanged.
    Replace,   //<! Replace with the second source rate.
    Min,       //<! Take the minimum rate (finest).
    Max,       //<! Take the maximum rate (coarsest).
    Sum,       //<! Multiply rates (coarsen further).
};

#if GP_RHI_ENABLE_RAY_TRACING

/// @brief Whether an acceleration structure is bottom-level (geometry) or top-level (instances).
enum class EAccelerationStructureType : UInt8
{
    BottomLevel,   //<! BLAS, contains triangles or AABBs.
    TopLevel,      //<! TLAS, contains BLAS instances.
};

/// @brief Geometry type within a BLAS.
enum class EGeometryType : UInt8
{
    Triangles,
    AABBs,   //<! Axis-aligned bounding boxes (custom intersection shaders).
};

/// @brief Build / update flags for an acceleration structure.
enum class EBuildAccelerationStructureFlags : UInt32
{
    None = 0,
    AllowUpdate = 1 << 0,       //<! Support incremental refit (deformable geometry).
    AllowCompaction = 1 << 1,   //<! Allow compacted copy (static geometry).
    PreferFastTrace = 1 << 2,   //<! Optimise for ray traversal speed.
    PreferFastBuild = 1 << 3,   //<! Optimise for build speed.
    LowMemory = 1 << 4,         //<! Reduce scratch / storage memory usage.
    MotionBlur = 1 << 5,        //<! Enable motion-blur interpolation keys.
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EBuildAccelerationStructureFlags);

/// @brief Flags per geometry instance in a TLAS.
enum class EAccelerationStructureInstanceFlags : UInt8
{
    None = 0,
    TriangleCullDisable = 1 << 0,
    TriangleFrontCounterClockwise = 1 << 1,
    ForceOpaque = 1 << 2,
    ForceNoOpaque = 1 << 3,
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EAccelerationStructureInstanceFlags);

/// @brief Flags applied per-geometry within a BLAS.
enum class EGeometryFlags : UInt8
{
    None = 0,
    Opaque = 1 << 0,   //<! Disable any-hit shader invocations.
    NoDuplicateHit = 1 << 1,
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EGeometryFlags);

/// @brief Shader group types in a ray-tracing pipeline.
enum class ERayTracingShaderGroupType : UInt8
{
    General,         //<! RayGen, Miss, or Callable.
    TrianglesHit,    //<! ClosestHit + optional AnyHit for triangle geometry.
    ProceduralHit,   //<! ClosestHit + AnyHit + Intersection for AABB geometry.
};

#endif   // GP_RHI_ENABLE_RAY_TRACING

enum class EQueryType : UInt8
{
    Occlusion,            //<! Fragment / pixel passing depth/stencil tests.
    OcclusionPrecise,     //<! Exact fragment count (may be slower).
    Timestamp,            //<! GPU timestamp in device-clock ticks.
    PipelineStatistics,   //<! Per-stage hardware performance counters.
    /// @note Requires GP_RHI_ENABLE_MESH_SHADERS.
    MeshPipelineStatistics,
    /// @note Requires GP_RHI_ENABLE_RAY_TRACING.
    AccelerationStructureSize,
};

/// @brief Pipeline statistics query flags (mirrors D3D12_QUERY_DATA_PIPELINE_STATISTICS).
enum class EPipelineStatistics : UInt32
{
    None = 0,
    InputAssemblyVertices = 1 << 0,
    InputAssemblyPrimitives = 1 << 1,
    VertexShaderInvocations = 1 << 2,
    GeometryShaderInvocations = 1 << 3,
    GeometryShaderPrimitives = 1 << 4,
    ClippingInvocations = 1 << 5,
    ClippingPrimitives = 1 << 6,
    FragmentShaderInvocations = 1 << 7,
    TessControlPatches = 1 << 8,
    TessEvalInvocations = 1 << 9,
    ComputeShaderInvocations = 1 << 10,
    TaskShaderInvocations = 1 << 11,
    MeshShaderInvocations = 1 << 12,
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EPipelineStatistics);

/// @brief Swap chain presentation mode.
enum class EPresentMode : UInt8
{
    Immediate,     //<! No vsync; may tear.
    FIFO,          //<! Standard vsync.
    FIFORelaxed,   //<! Vsync but presents immediately if a frame is missed (may tear).
    Mailbox,       //<! Triple-buffer vsync; lowest latency without tearing.
};

/// @brief Colour space / transfer function for swap chain surfaces.
enum class EColorSpace : UInt8
{
    SRGB_NonLinear,   //<! Standard sRGB (the default).
    DisplayP3_NonLinear,
    ExtendedSRGB_Linear,
    HDR10_ST2084,
    Dolby_Vision,
    HDR10_HLG,
    AdobeRGB_Linear,
    AdobeRGB_NonLinear,
    PassThrough,   //<! No colour-space transformation.
};

/// @brief GPU texture/resource barrier pipeline stage granularity.
enum class EPipelineStage : UInt32
{
    None = 0,
    TopOfPipe = 1 << 0,
    DrawIndirect = 1 << 1,
    VertexInput = 1 << 2,
    VertexShader = 1 << 3,
    TessControlShader = 1 << 4,
    TessEvalShader = 1 << 5,
    GeometryShader = 1 << 6,
    FragmentShader = 1 << 7,
    EarlyFragmentTests = 1 << 8,
    LateFragmentTests = 1 << 9,
    ColorAttachmentOutput = 1 << 10,
    ComputeShader = 1 << 11,
    Transfer = 1 << 12,
    BottomOfPipe = 1 << 13,
    Host = 1 << 14,
    AllGraphics = 1 << 15,
    AllCommands = 1 << 16,
    TaskShader = 1 << 17,
    MeshShader = 1 << 18,
    RayTracingShader = 1 << 19,
    AccelerationStructureBuild = 1 << 20,
    ShadingRate = 1 << 21,
    VideoDecoder = 1 << 22,
    VideoEncoder = 1 << 23,
};
GP_ENABLE_ENUM_BITWISE_OPERATIONS(EPipelineStage);

/// @brief Result codes returned by RHI operations.
enum class ERHIResult : Int32
{
    Success = 0,
    NotReady = 1,   //<! Operation not yet complete (e.g. fence not signalled).
    Timeout = 2,
    Incomplete = 3,
    SuboptimalSwapChain = 4,   //<! Present succeeded but swap chain should be recreated.

    // Errors (< 0)
    OutOfDeviceMemory = -1,
    OutOfHostMemory = -2,
    DeviceLost = -3,
    OutOfDateSwapChain = -4,   //<! Swap chain must be recreated before next present.
    InvalidArgument = -5,
    NotSupported = -6,
    ShaderCompilationFailed = -7,
    Unknown = -8,
};

}   // namespace GP
