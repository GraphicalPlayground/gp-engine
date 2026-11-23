// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "templates/Enums.hpp"

namespace gp::rhi::sync
{

/// @brief GPU resource usage and synchronization state flags used by barrier transitions.
/// @details Mirrors Unreal Engine's `ERHIAccess` model so render passes and transition barriers can validate
///          exclusive read-only vs. write-only combinations and translate cleanly to Vulkan `synchronization2`,
///          D3D12 `D3D12_BARRIER_*`, and Metal tracking.
enum class ResourceAccess : gp::UInt32
{
    /// @brief Previous state is unknown; requires a conservative full GPU cache flush.
    Unknown = 0u,

    // Read states
    CPURead = 1u << 0,
    Present = 1u << 1,
    IndirectArgs = 1u << 2,
    VertexOrIndexBuffer = 1u << 3,
    SRVCompute = 1u << 4,
    SRVGraphicsPixel = 1u << 5,
    SRVGraphicsNonPixel = 1u << 6,
    CopySrc = 1u << 7,
    ResolveSrc = 1u << 8,
    DSVRead = 1u << 9,

    // Read-write states
    UAVCompute = 1u << 10,
    UAVGraphics = 1u << 11,
    RTV = 1u << 12,
    CopyDest = 1u << 13,
    ResolveDst = 1u << 14,
    DSVWrite = 1u << 15,

    // Ray tracing acceleration structure states
    /// @brief Required for acceleration structure inputs to build, update, copy, or trace commands.
    BVHRead = 1u << 16,
    /// @brief Required for acceleration structure outputs of build, update, or copy commands.
    BVHWrite = 1u << 17,

    /// @brief Invalid/released transient resource state whose prior contents may be discarded.
    Discard = 1u << 18,

    /// @brief Screen-space Variable Rate Shading attachment read state.
    ShadingRateSource = 1u << 19,

    Last = ShadingRateSource,
    None = Unknown,
    Mask = (Last << 1u) - 1u,

    /// @brief Combined graphics shader resource read state (pixel and non-pixel stages).
    SRVGraphics = SRVGraphicsPixel | SRVGraphicsNonPixel,

    /// @brief Mask of all SRV read states across graphics and compute pipelines.
    SRVMask = SRVCompute | SRVGraphics,

    /// @brief Mask of all UAV read-write states across graphics and compute pipelines.
    UAVMask = UAVCompute | UAVGraphics,

    /// @brief Read-only states that cannot be legally combined with any writable state.
    ReadOnlyExclusiveMask = CPURead | Present | IndirectArgs | VertexOrIndexBuffer | SRVGraphics | SRVCompute |
                            CopySrc | ResolveSrc | BVHRead | ShadingRateSource,

    /// @brief Compute-queue subset of exclusive read-only states.
    ReadOnlyExclusiveComputeMask = CPURead | IndirectArgs | SRVCompute | CopySrc | BVHRead,

    /// @brief All read-only states, including `DSVRead` which may combine with `DSVWrite` (depth-write + stencil-read).
    ReadOnlyMask = ReadOnlyExclusiveMask | DSVRead,

    /// @brief All states capable of reading resource data (read-only states plus UAVs).
    ReadableMask = ReadOnlyMask | UAVMask,

    /// @brief Write-only states that cannot be legally combined with any readable state.
    WriteOnlyExclusiveMask = RTV | CopyDest | ResolveDst,

    /// @brief All write-only states, including `DSVWrite`.
    WriteOnlyMask = WriteOnlyExclusiveMask | DSVWrite,

    /// @brief All states capable of mutating resource data (write-only states, UAVs, and `BVHWrite`).
    WritableMask = WriteOnlyMask | UAVMask | BVHWrite,

    /// @brief Readable states exclusive to the graphics queue.
    GraphicsReadableMask = VertexOrIndexBuffer | DSVRead | SRVGraphics | ResolveSrc | ShadingRateSource
};

/// @brief GPU execution queue/pipeline domain targeted by transitions and command lists.
enum class PipelineStage : gp::UInt8
{
    None = 0u,
    Graphics = 1u << 0,
    AsyncCompute = 1u << 1,

    All = Graphics | AsyncCompute
};

/// @brief Total number of distinct hardware execution pipelines represented by `PipelineStage`.
inline constexpr gp::UInt32 kPipelineStageCount = 2u;

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

/// @brief Metric captured by a GPU render query.
enum class RenderQueryType : gp::UInt8
{
    Undefined = 0,
    /// @brief Number of samples passing depth/stencil tests (divide by MSAA sample count for pixels).
    Occlusion,
    /// @brief GPU timestamp reported in microseconds.
    AbsoluteTime
};

/// @brief Concrete resource kind referenced by a `TransitionInfo` entry.
enum class TransitionResourceType : gp::UInt8
{
    Unknown = 0,
    Texture,
    Buffer,
    UAV,
    BVH
};

/// @brief Resource kind participating in a transient memory aliasing barrier.
enum class TransientAliasingResourceType : gp::UInt8
{
    Texture = 0,
    Buffer
};

/// @brief Operation executed by a transient memory aliasing barrier.
enum class TransientAliasingAction : gp::UInt8
{
    Acquire = 0,
    Discard
};

}   // namespace gp::rhi::sync

GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::sync::ResourceAccess);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::sync::PipelineStage);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::sync::TransitionCreateFlags);
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::rhi::sync::ResourceTransitionFlags);

namespace gp::rhi::sync
{

/// @brief Checks whether `access` contains only exclusive read-only states.
/// @param[in] access Resource access mask to test.
/// @return True when at least one exclusive read-only bit is set and no non-exclusive-read-only bits are set.
[[nodiscard]] inline constexpr bool isReadOnlyExclusiveAccess(const ResourceAccess access) noexcept
{
    return ((access & ResourceAccess::ReadOnlyExclusiveMask) != ResourceAccess::None) &&
           ((access & ~ResourceAccess::ReadOnlyExclusiveMask) == ResourceAccess::None);
}

/// @brief Checks whether `access` contains only read-only states (including `DSVRead`).
/// @param[in] access Resource access mask to test.
/// @return True when at least one read-only bit is set and no non-read-only bits are set.
[[nodiscard]] inline constexpr bool isReadOnlyAccess(const ResourceAccess access) noexcept
{
    return ((access & ResourceAccess::ReadOnlyMask) != ResourceAccess::None) &&
           ((access & ~ResourceAccess::ReadOnlyMask) == ResourceAccess::None);
}

/// @brief Checks whether `access` contains only write-only states.
/// @param[in] access Resource access mask to test.
/// @return True when at least one write-only bit is set and no non-write-only bits are set.
[[nodiscard]] inline constexpr bool isWriteOnlyAccess(const ResourceAccess access) noexcept
{
    return ((access & ResourceAccess::WriteOnlyMask) != ResourceAccess::None) &&
           ((access & ~ResourceAccess::WriteOnlyMask) == ResourceAccess::None);
}

/// @brief Checks whether `access` includes any state capable of writing to the resource.
/// @param[in] access Resource access mask to test.
/// @return True when any bit in `ResourceAccess::WritableMask` is set.
[[nodiscard]] inline constexpr bool isWritableAccess(const ResourceAccess access) noexcept
{
    return (access & ResourceAccess::WritableMask) != ResourceAccess::None;
}

/// @brief Checks whether `access` includes any state capable of reading from the resource.
/// @param[in] access Resource access mask to test.
/// @return True when any bit in `ResourceAccess::ReadableMask` is set.
[[nodiscard]] inline constexpr bool isReadableAccess(const ResourceAccess access) noexcept
{
    return (access & ResourceAccess::ReadableMask) != ResourceAccess::None;
}

/// @brief Checks whether `access` combines mutually incompatible read-only-exclusive or write-only-exclusive flags.
/// @param[in] access Resource access mask to validate.
/// @return True when `access` represents an illegal state combination.
[[nodiscard]] inline constexpr bool isInvalidAccess(const ResourceAccess access) noexcept
{
    const bool hasReadOnlyExclusiveConflict =
        ((access & ResourceAccess::ReadOnlyExclusiveMask) != ResourceAccess::None) &&
        ((access & ResourceAccess::WritableMask) != ResourceAccess::None);
    const bool hasWriteOnlyExclusiveConflict =
        ((access & ResourceAccess::WriteOnlyExclusiveMask) != ResourceAccess::None) &&
        ((access & ResourceAccess::ReadableMask) != ResourceAccess::None);

    return hasReadOnlyExclusiveConflict || hasWriteOnlyExclusiveConflict;
}

/// @brief Checks whether `access` is a legally combinable resource state mask.
/// @param[in] access Resource access mask to validate.
/// @return True when `access` contains no mutually exclusive flag conflicts.
[[nodiscard]] inline constexpr bool isValidAccess(const ResourceAccess access) noexcept
{
    return !isInvalidAccess(access);
}

/// @brief Checks whether `stages` selects exactly one hardware execution pipeline.
/// @param[in] stages Bitmask of pipeline stages to test.
/// @return True when exactly one bit is set in `stages`.
[[nodiscard]] inline constexpr bool isSinglePipelineStage(const PipelineStage stages) noexcept
{
    const gp::UInt8 raw = static_cast<gp::UInt8>(stages);
    return (raw != 0u) && ((raw & static_cast<gp::UInt8>(raw - 1u)) == 0u);
}

/// @brief Maps a single `PipelineStage` bit to its dense zero-based array index.
/// @param[in] stage Single pipeline stage (`Graphics` or `AsyncCompute`).
/// @return `0` for `PipelineStage::Graphics`, `1` for `PipelineStage::AsyncCompute`.
[[nodiscard]] inline constexpr gp::UInt32 getPipelineStageIndex(const PipelineStage stage) noexcept
{
    switch (stage)
    {
    case PipelineStage::AsyncCompute:
        return 1u;

    case PipelineStage::Graphics:
    default:
        return 0u;
    }
}

/// @brief Returns the total number of distinct hardware execution pipelines (`kPipelineStageCount`).
/// @return Number of single-bit pipeline stages (`2`).
[[nodiscard]] inline constexpr gp::UInt32 getPipelineStageCount() noexcept
{
    return kPipelineStageCount;
}

}   // namespace gp::rhi::sync
