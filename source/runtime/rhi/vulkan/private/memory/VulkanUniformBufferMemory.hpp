// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Buffer usage of every persistent uniform buffer allocation.
inline constexpr VkBufferUsageFlags kVulkanUniformBufferUsageFlags =
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

/// @brief Memory properties of every persistent uniform buffer allocation.
inline constexpr VkMemoryPropertyFlags kVulkanUniformBufferMemoryFlags =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

/// @brief Returns the offset alignment every uniform buffer range must honor on `device`.
/// @param[in] device Initialized device.
/// @return `max(minUniformBufferOffsetAlignment, 16)` in bytes.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32 getVulkanUniformBufferAlignment(const VulkanDevice& device) noexcept;

/// @brief Sub-allocates persistent, host-visible and coherent uniform buffer memory from the buffer pools.
/// @param[in] memoryManager Memory manager serving the pooled allocation.
/// @param[out] outAllocation Receives the owning allocation.
/// @param[in] size Constant buffer size in bytes.
/// @return True on success.
[[nodiscard]] GP_RHI_VULKAN_API bool allocateVulkanUniformBufferMemory(
    VulkanMemoryManager& memoryManager, VulkanAllocation& outAllocation, gp::UInt32 size
) noexcept;

/// @brief Releases uniform buffer memory produced by either the temp block allocator or the buffer pools.
/// @details Temp block ranges return their block reference once the GPU is done with the current frame, through
///          `deletionQueue` when one is bound. Pooled ranges go back to the memory manager, which defers the free
///          itself. `allocation` is always left empty.
/// @param[in] memoryManager Memory manager that owns pooled allocations.
/// @param[in] deletionQueue Deferred deletion queue, or `nullptr` to return temp block references immediately.
/// @param[in] tempBlockAllocator Allocator that served temp block ranges; may be null when none were made.
/// @param[in,out] allocation Allocation to release.
GP_RHI_VULKAN_API void freeVulkanUniformBufferMemory(
    VulkanMemoryManager& memoryManager,
    VulkanDeferredDeletionQueue* deletionQueue,
    VulkanTempBlockAllocator* tempBlockAllocator,
    VulkanAllocation& allocation
) noexcept;

}   // namespace gp::rhi::vulkan
