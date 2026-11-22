// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::resource
{

/// @brief Number of faces on a cubemap texture.
inline constexpr gp::UInt32 kCubeFaceCount = 6u;

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

}   // namespace gp::rhi::resource

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::resource::TextureCreateFlags);
