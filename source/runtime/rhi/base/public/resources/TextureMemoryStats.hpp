// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "RHIBase.hpp"

namespace gp::rhi::resource
{

/// @brief Snapshot of GPU video memory budgets and current texture pool utilization.
/// @details Populated by `DynamicRHI::getTextureMemoryStats()`. Hardware capacity fields remain invariant after device
///          creation (`-1` or `0` when unreported by the driver), while streaming, non-streaming, and pool fields
///          reflect live runtime consumption in bytes.
struct GP_RHI_API TextureMemoryStats
{
    /// @brief Dedicated local video memory reported by the adapter in bytes (`-1` if unknown).
    gp::Int64 dedicatedVideoMemory{ 0 };

    /// @brief Dedicated system memory reported by the adapter in bytes (`-1` if unknown).
    gp::Int64 dedicatedSystemMemory{ 0 };

    /// @brief Shared system memory accessible to the GPU in bytes (`-1` if unknown).
    gp::Int64 sharedSystemMemory{ 0 };

    /// @brief Total graphics memory budget usable by GPU resources in bytes (`-1` if unknown).
    gp::Int64 totalGraphicsMemory{ 0 };

    /// @brief Memory currently allocated to streaming textures in bytes.
    gp::Int64 streamingMemorySize{ 0 };

    /// @brief Memory currently allocated to non-streaming textures in bytes.
    gp::Int64 nonStreamingMemorySize{ 0 };

    /// @brief Size of the largest contiguous free allocation block in bytes (`0` if untracked).
    gp::Int64 largestContiguousAllocation{ 0 };

    /// @brief Configured texture streaming pool budget in bytes (`0` when pool size is unlimited).
    gp::Int64 texturePoolSize{ 0 };

    /// @brief Pending streaming memory delta scheduled by in-flight reallocations in bytes.
    gp::Int64 pendingMemoryAdjustment{ 0 };

    /// @brief Total memory currently committed inside the texture pool in bytes.
    gp::Int64 allocatedMemorySize{ 0 };

    /// @brief Reports whether hardware memory budget statistics were populated by the active backend.
    /// @return True when `totalGraphicsMemory > 0` or `dedicatedVideoMemory > 0`.
    [[nodiscard]] constexpr bool areHardwareStatsValid() const noexcept
    {
        return (totalGraphicsMemory > 0) || (dedicatedVideoMemory > 0);
    }

    /// @brief Reports whether texture streaming is bounded by a finite pool size.
    /// @return True when `texturePoolSize > 0`.
    [[nodiscard]] constexpr bool isUsingLimitedPoolSize() const noexcept
    {
        return texturePoolSize > 0;
    }

    /// @brief Computes remaining bytes available in the streaming texture pool (`0` when unlimited or over budget).
    /// @return `max(texturePoolSize - streamingMemorySize, 0)`.
    [[nodiscard]] constexpr gp::Int64 computeAvailableMemorySize() const noexcept
    {
        return gp::math::max<gp::Int64>(texturePoolSize - streamingMemorySize, 0);
    }

    /// @brief Computes the effective working memory ceiling for the device given integrated vs. discrete topology.
    /// @param[in] deviceIsIntegrated True when the active adapter uses unified/integrated memory.
    /// @return `max(totalGraphicsMemory, dedicatedVideoMemory)` on integrated GPUs, or `dedicatedVideoMemory` on
    ///         discrete GPUs.
    [[nodiscard]] constexpr gp::Int64 getTotalDeviceWorkingMemory(const bool deviceIsIntegrated) const noexcept
    {
        if (deviceIsIntegrated)
        {
            return gp::math::max<gp::Int64>(totalGraphicsMemory, dedicatedVideoMemory);
        }
        return dedicatedVideoMemory;
    }

    /// @brief Computes the effective working memory ceiling using `getRHIGlobals().deviceIsIntegrated`.
    /// @return Total device working memory in bytes.
    [[nodiscard]] gp::Int64 getTotalDeviceWorkingMemory() const noexcept;

    /// @brief Default memberwise equality comparison.
    /// @param[in] rhs Statistics snapshot to compare against.
    /// @return True when every field matches.
    [[nodiscard]] constexpr bool operator==(const TextureMemoryStats& rhs) const noexcept = default;
};

}   // namespace gp::rhi::resource
