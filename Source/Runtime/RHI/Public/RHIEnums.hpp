// Copyright (c) - Graphical Playground. All rights reserved.

#include "RHIBuild.hpp"
#include "Templates/Enums.hpp"

namespace GP::RHI
{

/// \brief Enumeration of RHI backends
enum class ERHIBackend : UInt8
{
    None = 0,    //<! No backend selected (null backend)
    DirectX11,   //<! DirectX 11 backend (Windows only)
    DirectX12,   //<! DirectX 12 backend (Windows only)
    Vulkan,      //<! Vulkan backend (cross-platform)
    Metal,       //<! Metal backend (macOS, iOS only)
    OpenGL,      //<! OpenGL backend (cross-platform)
    OpenGLES,    //<! OpenGL ES backend (mobile platforms)
    WebGPU,      //<! WebGPU backend (web platforms)
    Auto         //<! Automatically select the best available backend for the platform
};

/// \brief Enumeration of resource types
enum class EResourceType : UInt8
{
    Unknown = 0,
    Buffer,
    Texture1D,
    Texture1DArray,
    Texture2D,
    Texture2DArray,
    Texture3D,
    TextureCube,
    TextureCubeArray,
    Texture2DMS,   //<! Multi-sampled
    Texture2DMSArray,
    Sampler
};

/// \brief Enumeration of resource dimensions
enum class EResourceDimension : UInt8
{
    Unknown = 0,
    Buffer,
    Texture1D,
    Texture2D,
    Texture3D
};

/// \brief Enumeration of buffer usage flags
enum class EBufferUsage : UInt32
{
    None = 0,
    VertexBuffer = 1 << 0,
    IndexBuffer = 1 << 1,
    ConstantBuffer = 1 << 2,
    StructuredBuffer = 1 << 3,
    RawBuffer = 1 << 4,         //<! ByteAddressBuffer
    IndirectArguments = 1 << 5,
    ShaderResource = 1 << 6,    //<! SRV
    UnorderedAccess = 1 << 7,   //<! UAV
    RayTracing = 1 << 8,        //<! AS building
    AccelerationStructure = 1 << 9,
    ShaderBindingTable = 1 << 10,

    // Combined flags
    Dynamic = 1 << 16,   //<! CPU write, GPU read (frequent updates)
    Staging = 1 << 17,   //<! CPU read/write (for transfers)
};

/// \brief Enumeration of texture usage flags
enum class ETextureUsage : UInt32
{
    None = 0,
    ShaderResource = 1 << 0,    //<! SRV
    RenderTarget = 1 << 1,      //<! RTV
    DepthStencil = 1 << 2,      //<! DSV
    UnorderedAccess = 1 << 3,   //<! UAV
    Sampled = 1 << 4,           //<! For sampling in shaders
    Storage = 1 << 5,           //<! For image load/store
    InputAttachment = 1 << 6,   //<! For subpass input
    TransferSrc = 1 << 7,
    TransferDst = 1 << 8,
    ShadingRate = 1 << 9,   //<! Variable rate shading
};

/// \brief Enumeration of CPU access modes for resources
enum class ECPUAccessMode : UInt8
{
    None = 0,
    Read,
    Write,
    ReadWrite
};

/// \brief Enumeration of memory types for resource allocation
enum class EMemoryType : UInt8
{
    Default = 0,     //<! GPU only (best performance)
    Upload,          //<! CPU to GPU (dynamic resources)
    Readback,        //<! GPU to CPU (query results, screenshots)
    CPUAccessible,   //<! CPU read/write (mapped)
    Auto,            //<! Automatic selection based on usage
};

/// \brief Enumeration of resource states for barriers and transitions
enum class EResourceState : UInt32
{
    Undefined = 0,
    Common = 1 << 0,
    VertexBuffer = 1 << 1,
    IndexBuffer = 1 << 2,
    ConstantBuffer = 1 << 3,
    RenderTarget = 1 << 4,
    DepthWrite = 1 << 5,
    DepthRead = 1 << 6,
    ShaderResource = 1 << 7,
    UnorderedAccess = 1 << 8,
    IndirectArgument = 1 << 9,
    CopySource = 1 << 10,
    CopyDest = 1 << 11,
    ResolveSrc = 1 << 12,
    ResolveDest = 1 << 13,
    Present = 1 << 14,
    RayTracingAS = 1 << 15,
    ShadingRate = 1 << 16,
    GenericRead = 1 << 17,   //<! Combined read states

    // Combined states
    DepthStencilRead = DepthRead | ShaderResource,
    DepthStencilWrite = DepthWrite,
};

/// \brief Enumeration of shader stages
enum class EShaderStage : UInt32
{
    None = 0,
    Vertex = 1 << 0,
    Hull = 1 << 1,     //<! Tessellation control
    Domain = 1 << 2,   //<! Tessellation evaluation
    Geometry = 1 << 3,
    Pixel = 1 << 4,    //<! Fragment
    Compute = 1 << 5,

    // Ray tracing stages
    RayGen = 1 << 6,
    Miss = 1 << 7,
    ClosestHit = 1 << 8,
    AnyHit = 1 << 9,
    Intersection = 1 << 10,
    Callable = 1 << 11,

    // Mesh shading stages
    Amplification = 1 << 12,   //<! Task shader
    Mesh = 1 << 13,

    // Combined stages
    AllGraphics = Vertex | Hull | Domain | Geometry | Pixel,
    AllRayTracing = RayGen | Miss | ClosestHit | AnyHit | Intersection | Callable,
    All = 0xFFFFFFFF,
};

/// \brief Enumeration of pixel formats
enum class EPixelFormat : UInt16
{
    Unknown = 0,

    // 8-bit formats
    R8_UNORM,
    R8_SNORM,
    R8_UINT,
    R8_SINT,

    RG8_UNORM,
    RG8_SNORM,
    RG8_UINT,
    RG8_SINT,

    RGBA8_UNORM,
    RGBA8_UNORM_SRGB,
    RGBA8_SNORM,
    RGBA8_UINT,
    RGBA8_SINT,

    BGRA8_UNORM,
    BGRA8_UNORM_SRGB,

    // 16-bit formats
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

    // 32-bit formats
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

    // Packed formats
    RGB10A2_UNORM,
    RGB10A2_UINT,
    RG11B10_FLOAT,
    RGB9E5_FLOAT,

    // Depth/Stencil formats
    D16_UNORM,
    D24_UNORM_S8_UINT,
    D32_FLOAT,
    D32_FLOAT_S8X24_UINT,

    // Compressed formats - BC (Desktop)
    BC1_UNORM,   //<! DXT1
    BC1_UNORM_SRGB,
    BC2_UNORM,   //<! DXT3
    BC2_UNORM_SRGB,
    BC3_UNORM,   //<! DXT5
    BC3_UNORM_SRGB,
    BC4_UNORM,   //<! RGTC1
    BC4_SNORM,
    BC5_UNORM,   //<! RGTC2
    BC5_SNORM,
    BC6H_UFLOAT,
    BC6H_SFLOAT,
    BC7_UNORM,
    BC7_UNORM_SRGB,

    // Compressed formats - ETC2 (Mobile)
    ETC2_RGB8_UNORM,
    ETC2_RGB8_SRGB,
    ETC2_RGB8A1_UNORM,
    ETC2_RGB8A1_SRGB,
    ETC2_RGBA8_UNORM,
    ETC2_RGBA8_SRGB,

    // Compressed formats - ASTC
    ASTC_4x4_UNORM,
    ASTC_4x4_SRGB,
    ASTC_5x5_UNORM,
    ASTC_5x5_SRGB,
    ASTC_6x6_UNORM,
    ASTC_6x6_SRGB,
    ASTC_8x8_UNORM,
    ASTC_8x8_SRGB,
    ASTC_10x10_UNORM,
    ASTC_10x10_SRGB,
    ASTC_12x12_UNORM,
    ASTC_12x12_SRGB,

    // Special formats
    R1_UNORM,   //<! 1-bit per pixel

    Count
};

/// \brief Enumeration of primitive topologies
enum class EPrimitiveTopology : UInt8
{
    Undefined = 0,
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    LineListAdj,
    LineStripAdj,
    TriangleListAdj,
    TriangleStripAdj,
    PatchList,   //<! For tessellation
};

/// \brief Enumeration of index buffer types
enum class EIndexType : UInt8
{
    None = 0,   //<! No index buffer (non-indexed draw)
    Uint16,
    Uint32
};

/// \brief Enumeration of texture filter modes
enum class EFilterMode : UInt8
{
    Point = 0,   //<! Nearest
    Linear,
    Anisotropic,
};

/// \brief Enumeration of filter reduction modes
enum class EFilterReduction : UInt8
{
    Standard = 0,
    Comparison,
    Minimum,
    Maximum,
};

/// \brief Enumeration of texture address modes
enum class EAddressMode : UInt8
{
    Wrap = 0,   //<! Repeat
    Mirror,
    Clamp,
    Border,
    MirrorOnce,
};

/// \brief Enumeration of border color for samplers
enum class EBorderColor : UInt8
{
    TransparentBlack = 0,   //<! (0, 0, 0, 0)
    OpaqueBlack,            //<! (0, 0, 0, 1)
    OpaqueWhite,            //<! (1, 1, 1, 1)
};

/// \brief Enumeration of comparison functions
enum class EComparisonFunc : UInt8
{
    Never = 0,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

/// \brief Enumeration of blend factors
enum class EBlendFactor : UInt8
{
    Zero = 0,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DstColor,
    InvDstColor,
    DstAlpha,
    InvDstAlpha,
    SrcAlphaSaturate,
    BlendFactor,
    InvBlendFactor,
    Src1Color,
    InvSrc1Color,
    Src1Alpha,
    InvSrc1Alpha,
};

/// \brief Enumeration of blend operations
enum class EBlendOp : UInt8
{
    Add = 0,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

/// \brief Enumeration of color write mask flags
enum class EColorWriteMask : UInt8
{
    None = 0,
    Red = 1 << 0,
    Green = 1 << 1,
    Blue = 1 << 2,
    Alpha = 1 << 3,
    All = Red | Green | Blue | Alpha,
};

/// \brief Enumeration of polygon fill modes
enum class EFillMode : UInt8
{
    Solid = 0,
    Wireframe,
};

/// \brief Enumeration of polygon culling modes
enum class ECullMode : UInt8
{
    None = 0,
    Front,
    Back,
};

/// \brief Enumeration of front face winding order
enum class EFrontFace : UInt8
{
    Clockwise = 0,
    CounterClockwise,
};

/// \brief Enumeration of stencil operations
enum class EStencilOp : UInt8
{
    Keep = 0,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert,
    IncrementWrap,
    DecrementWrap,
};

/// \brief Enumeration of load operations for render pass attachments
enum class ELoadOp : UInt8
{
    Load = 0,   //<! Preserve existing contents
    Clear,      //<! Clear to a specified value
    DontCare,   //<! Don't care about existing contents
};

/// \brief Enumeration of store operations for render pass attachments
enum class EStoreOp : UInt8
{
    Store = 0,   //<! Store results
    DontCare,    //<! Don't care about results
    Resolve,     //<! Resolve MSAA
};

/// \brief Enumeration of attachment types
enum class EAttachmentType : UInt8
{
    Color = 0,
    Depth,
    Stencil,
    DepthStencil,
    Resolve,
    ShadingRate,
};

/// \brief Enumeration of pipeline bind points
enum class EPipelineBindPoint : UInt8
{
    Graphics = 0,
    Compute,
    RayTracing,
};

/// \brief Enumeration of command queue types
enum class ECommandQueueType : UInt8
{
    Graphics = 0,   //<! Can do graphics, compute, and transfer
    Compute,        //<! Can do compute and transfer
    Transfer,       //<! Can do transfer only
    Present,        //<! Special queue for presentation
};

/// \brief Enumeration of command list recording levels
enum class ECommandListLevel : UInt8
{
    Primary = 0,
    Secondary,
};

/// \brief Enumeration of descriptor types for resource binding
enum class EDescriptorType : UInt8
{
    Sampler = 0,
    CombinedImageSampler,
    SampledImage,
    StorageImage,
    UniformTexelBuffer,
    StorageTexelBuffer,
    UniformBuffer,
    StorageBuffer,
    UniformBufferDynamic,
    StorageBufferDynamic,
    InputAttachment,
    AccelerationStructure,
};

/// \brief Enumeration of GPU query types
enum class EQueryType : UInt8
{
    Occlusion = 0,
    BinaryOcclusion,
    Timestamp,
    PipelineStatistics,
    StreamOutput,
};

/// \brief Enumeration of fence flags
enum class EFenceFlags : UInt8
{
    None = 0,
    Shared = 1 << 0,
};

/// \brief Enumeration of shader source languages
enum class EShaderLanguage : UInt8
{
    HLSL = 0,
    GLSL,
    MSL,
    SPIRV,
    DXIL,
};

/// \brief Enumeration of shader optimization levels
enum class EShaderOptimization : UInt8
{
    None = 0,
    Debug,
    Size,
    Speed,
};

/// \brief Enumeration of presentation modes for swap chains
enum class EPresentMode : UInt8
{
    Immediate = 0,   //<! No V-Sync
    Mailbox,         //<! V-Sync with triple buffering
    FIFO,            //<! V-Sync (standard)
    FIFORelaxed,     //<! V-Sync with late swap tearing
};

/// \brief Enumeration of vertex input rates
enum class EVertexInputRate : UInt8
{
    Vertex = 0,   //<! Per-vertex data
    Instance,     //<! Per-instance data
};

/// \brief Enumeration of logic operations for framebuffer blending
enum class ELogicOp : UInt8
{
    Clear = 0,
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

/// \brief Enumeration of sample count flags for MSAA
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

/// \brief Enumeration of resource view types
enum class EResourceViewType : UInt8
{
    Unknown = 0,
    ShaderResource,    //<! SRV
    UnorderedAccess,   //<! UAV
    RenderTarget,      //<! RTV
    DepthStencil,      //<! DSV
};

/// \brief Enumeration of barrier sync points
enum class EBarrierSync : UInt32
{
    None = 0,
    All = 1 << 0,
    Draw = 1 << 1,
    IndexInput = 1 << 2,
    VertexShading = 1 << 3,
    PixelShading = 1 << 4,
    DepthStencil = 1 << 5,
    RenderTarget = 1 << 6,
    Compute = 1 << 7,
    RayTracing = 1 << 8,
    Copy = 1 << 9,
    Resolve = 1 << 10,
    ExecuteIndirect = 1 << 11,
    AllShading = VertexShading | PixelShading | Compute,
    AllGraphics = Draw | IndexInput | VertexShading | PixelShading | DepthStencil | RenderTarget,
};

/// \brief Enumeration of barrier access types
enum class EBarrierAccess : UInt32
{
    None = 0,
    VertexBuffer = 1 << 0,
    IndexBuffer = 1 << 1,
    ConstantBuffer = 1 << 2,
    ShaderRead = 1 << 3,
    ShaderWrite = 1 << 4,
    RenderTarget = 1 << 5,
    DepthStencilRead = 1 << 6,
    DepthStencilWrite = 1 << 7,
    CopySource = 1 << 8,
    CopyDest = 1 << 9,
    ResolveSource = 1 << 10,
    ResolveDest = 1 << 11,
    Present = 1 << 12,
    IndirectArgument = 1 << 13,
    RayTracingAS = 1 << 14,
    ShadingRate = 1 << 15,
    Common = 1 << 16,
};

/// \brief Enumeration of texture view dimension types
enum class ETextureDimension : UInt8
{
    Unknown = 0,
    Texture1D,
    Texture1DArray,
    Texture2D,
    Texture2DArray,
    Texture2DMS,
    Texture2DMSArray,
    Texture3D,
    TextureCube,
    TextureCubeArray,
};

/// \brief Enumeration of input element semantic types
enum class EInputSemantic : UInt8
{
    Position = 0,
    Normal,
    Tangent,
    Binormal,
    Color,
    TexCoord,
    BlendIndices,
    BlendWeight,
    Custom,
};

/// \brief Enumeration of clear flags for render targets
enum class EClearFlags : UInt8
{
    None = 0,
    Color = 1 << 0,
    Depth = 1 << 1,
    Stencil = 1 << 2,
    DepthStencil = Depth | Stencil,
    All = Color | Depth | Stencil,
};

/// \brief Enumeration of vertex attribute formats
enum class EVertexFormat : UInt8
{
    Unknown = 0,
    Float,
    Float2,
    Float3,
    Float4,
    Half,
    Half2,
    Half4,
    UInt,
    UInt2,
    UInt3,
    UInt4,
    SInt,
    SInt2,
    SInt3,
    SInt4,
    UByte4,
    UByte4Norm,
    SByte4,
    SByte4Norm,
    UShort2,
    UShort2Norm,
    UShort4,
    UShort4Norm,
    SShort2,
    SShort2Norm,
    SShort4,
    SShort4Norm,
    UInt1010102Norm,   //<! RGB10A2
};

/// \brief Enumeration of descriptor binding flags for bindless descriptors
enum class EDescriptorBindingFlags : UInt32
{
    None = 0,
    UpdateAfterBind = 1 << 0,            //<! Descriptors can be updated after binding
    UpdateUnusedWhilePending = 1 << 1,   //<! Unused descriptors can be updated while in use
    PartiallyBound = 1 << 2,             //<! Not all descriptors need to be valid
    VariableDescriptorCount = 1 << 3,    //<! Variable number of descriptors
};

/// \brief Enumeration of image aspect flags for subresources
enum class EImageAspectFlags : UInt8
{
    None = 0,
    Color = 1 << 0,
    Depth = 1 << 1,
    Stencil = 1 << 2,
    Metadata = 1 << 3,
    DepthStencil = Depth | Stencil,
};

/// \brief Enumeration of queue priority levels
enum class EQueuePriority : UInt8
{
    Normal = 0,
    High,
    Realtime,
};

/// \brief Enumeration of conservative rasterization modes
enum class EConservativeRasterization : UInt8
{
    Disabled = 0,
    Overestimate,    //<! Conservative over-rasterization
    Underestimate,   //<! Conservative under-rasterization
};

/// \brief Enumeration of shading rate modes for variable rate shading
enum class EShadingRateMode : UInt8
{
    None = 0,   //<! No VRS
    _1x1,       //<! 1 shading sample per pixel
    _1x2,       //<! 1x2 coarse pixel size
    _2x1,       //<! 2x1 coarse pixel size
    _2x2,       //<! 2x2 coarse pixel size
    _2x4,       //<! 2x4 coarse pixel size
    _4x2,       //<! 4x2 coarse pixel size
    _4x4,       //<! 4x4 coarse pixel size
};

/// \brief Enumeration of shading rate combiner operations
enum class EShadingRateCombiner : UInt8
{
    Passthrough = 0,   //<! Use the rate from the source
    Override,          //<! Override with the new rate
    Min,               //<! Use the minimum (finest) rate
    Max,               //<! Use the maximum (coarsest) rate
    Sum,               //<! Sum the rates
};

};   // namespace GP::RHI

// Enable bitwise operators for EBufferUsage
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::EBufferUsage);

// Enable bitwise operators for ETextureUsage
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::ETextureUsage);

// Enable bitwise operators for EResourceState
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::EResourceState);

// Enable bitwise operators for EShaderStage
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::EShaderStage);

// Enable bitwise operators for EColorWriteMask
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::EColorWriteMask);

// Enable bitwise operators for EFenceFlags
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::EFenceFlags);

// Enable bitwise operators for EBarrierSync
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::EBarrierSync);

// Enable bitwise operators for EBarrierAccess
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::EBarrierAccess);

// Enable bitwise operators for EClearFlags
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::EClearFlags);

// Enable bitwise operators for EDescriptorBindingFlags
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::EDescriptorBindingFlags);

// Enable bitwise operators for EImageAspectFlags
GP_ENABLE_ENUM_BITWISE_OPERATIONS(GP::RHI::EImageAspectFlags);
