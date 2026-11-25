// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "memory/VulkanAllocation.hpp"
#include "resources/VulkanResourceForward.hpp"   // IWYU pragma: keep
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"                  // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Current GPU allocation backing a `VulkanBuffer`.
/// @details A lock rename swaps the whole struct at once, so the handle, host pointer and device address always
///          describe the same memory.
struct VulkanBufferAlloc final
{
    /// @brief Pooled range, or dedicated memory of a placed buffer.
    VulkanAllocation alloc{};

    /// @brief Persistently mapped pointer at the start of the range, or `nullptr` for device-local memory.
    void* hostPtr{ nullptr };

    /// @brief GPU virtual address of the first byte, or `0` without buffer device address support.
    VkDeviceAddress deviceAddress{ 0u };

    /// @brief True when the buffer owns its own `VkBuffer` bound to placed memory (acceleration structures).
    bool isPlacedResource{ false };
};

/// @brief Bookkeeping for a buffer lock that goes through a staging buffer.
/// @details Created by `VulkanBuffer::lock()` and consumed by the matching `unlock()`.
struct VulkanPendingBufferLock final
{
    /// @brief Staging buffer the caller writes into or reads from.
    VulkanStagingBuffer* stagingBuffer{ nullptr };

    /// @brief Destination byte offset inside the buffer (always `0` for readbacks).
    gp::UInt32 offset{ 0u };

    /// @brief Locked byte count.
    gp::UInt32 size{ 0u };

    /// @brief Lock mode requested by the caller.
    resource::ResourceLockMode lockMode{ resource::ResourceLockMode::ReadOnly };

    /// @brief True when this was the first lock of the buffer, so the copy can go on the upload context.
    bool isFirstLock{ false };
};

/// @brief One memory block bound into a sparse (reserved) buffer.
struct VulkanSparseBufferAllocation final
{
    /// @brief Device-local block backing a run of tiles.
    VulkanAllocation allocation{};

    /// @brief Byte offset of the block's first tile inside the buffer.
    gp::UInt64 resourceOffset{ 0u };
};

/// @brief Sparse binding state of a reserved `VulkanBuffer`.
struct VulkanReservedResourceData final
{
    /// @brief Committed memory blocks, in resource-offset order.
    gp::Vector<VulkanSparseBufferAllocation> allocations{};

    /// @brief Memory requirements reported for the sparse `VkBuffer`.
    VkMemoryRequirements memoryRequirements{};

    /// @brief Size in bytes of the final, possibly partial, block.
    gp::UInt64 lastBlockSize{ 0u };

    /// @brief Total number of blocks needed to back the whole buffer.
    gp::UInt32 blockCount{ 0u };

    /// @brief Number of tiles currently bound to memory.
    gp::UInt32 numCommittedTiles{ 0u };
};

}   // namespace gp::rhi::vulkan

