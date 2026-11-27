// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "profiling/ProfilingForward.hpp"   // IWYU pragma: keep
#include "resources/BufferDesc.hpp"
#include "resources/TextureDesc.hpp"
#include "resources/TextureMemoryStats.hpp"
#include "RHIBase.hpp"

namespace gp::rhi::profiling
{

/// @brief Consistent point-in-time snapshot of all global RHI resource memory counters (in bytes).
struct RHIResourceMemoryCountersSnapshot
{
    gp::Int64 renderTarget2DBytes{ 0 };
    gp::Int64 renderTarget3DBytes{ 0 };
    gp::Int64 renderTargetCubeBytes{ 0 };
    gp::Int64 uavTextureBytes{ 0 };
    gp::Int64 texture2DBytes{ 0 };
    gp::Int64 texture3DBytes{ 0 };
    gp::Int64 textureCubeBytes{ 0 };
    gp::Int64 streamingTextureBytes{ 0 };
    gp::Int64 nonStreamingTextureBytes{ 0 };

    gp::Int64 vertexBufferBytes{ 0 };
    gp::Int64 indexBufferBytes{ 0 };
    gp::Int64 structuredBufferBytes{ 0 };
    gp::Int64 byteAddressBufferBytes{ 0 };
    gp::Int64 drawIndirectBufferBytes{ 0 };
    gp::Int64 accelerationStructureBufferBytes{ 0 };
    gp::Int64 miscBufferBytes{ 0 };
    gp::Int64 uniformBufferBytes{ 0 };

    gp::Int64 reservedUncommittedTextureBytes{ 0 };
    gp::Int64 reservedCommittedTextureBytes{ 0 };
    gp::Int64 reservedUncommittedBufferBytes{ 0 };
    gp::Int64 reservedCommittedBufferBytes{ 0 };

    [[nodiscard]] constexpr bool operator==(const RHIResourceMemoryCountersSnapshot& rhs) const noexcept = default;
};

/// @brief Updates global texture memory counters when a texture is allocated or freed by a backend.
/// @param[in] desc Texture creation descriptor.
/// @param[in] allocatedSizeInBytes Platform-aligned byte footprint of the texture.
/// @param[in] isAllocating True on creation/allocation; false on destruction/release.
/// @param[in] onlyStreamable True to ignore non-streamable textures.
GP_RHI_API void updateGlobalTextureStats(
    const resource::TextureDesc& desc, gp::UInt64 allocatedSizeInBytes, bool isAllocating, bool onlyStreamable = false
) noexcept;

/// @brief Populates `streamingMemorySize` and `nonStreamingMemorySize` on `outStats` from global RHI counters.
/// @param[in,out] outStats Texture memory stats struct updated in place.
GP_RHI_API void fillBaselineTextureMemoryStats(resource::TextureMemoryStats& outStats) noexcept;

/// @brief Updates global buffer memory counters when a GPU buffer is allocated or freed by a backend.
/// @param[in] desc Buffer descriptor carrying `BufferUsageFlags`.
/// @param[in] bufferSizeInBytes Platform-aligned byte size of the buffer.
/// @param[in] isAllocating True on allocation; false on destruction/release.
GP_RHI_API void
    updateGlobalBufferStats(const resource::BufferDesc& desc, gp::Int64 bufferSizeInBytes, bool isAllocating) noexcept;

/// @brief Updates global uniform buffer memory counters on uniform buffer allocation or deallocation.
/// @param[in] bufferSizeInBytes Byte size of the uniform buffer.
/// @param[in] isAllocating True on allocation; false on deallocation.
GP_RHI_API void updateGlobalUniformBufferStats(gp::Int64 bufferSizeInBytes, bool isAllocating) noexcept;

/// @brief Transfers bytes between uncommitted and committed reserved-resource counters during physical page commits.
/// @param[in] commitDeltaBytes Positive byte size being committed or decommitted.
/// @param[in] isBuffer True for reserved buffers; false for reserved textures.
/// @param[in] isCommitting True when committing physical pages; false when decommitting back to virtual-only.
GP_RHI_API void
    updateReservedResourceStatsOnCommit(gp::Int64 commitDeltaBytes, bool isBuffer, bool isCommitting) noexcept;

/// @brief Reads a snapshot of all global atomic RHI resource memory counters.
/// @return `RHIResourceMemoryCountersSnapshot` in bytes.
[[nodiscard]] GP_RHI_API RHIResourceMemoryCountersSnapshot getGlobalRHIResourceMemoryCounters() noexcept;

/// @brief Resets all global atomic RHI resource memory counters to zero (for shutdown and unit tests).
GP_RHI_API void resetGlobalRHIResourceMemoryCounters() noexcept;

}   // namespace gp::rhi::profiling
