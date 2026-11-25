// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Lightweight non-reference-counted handle describing a sub-allocated or dedicated Vulkan GPU allocation.
/// @details Callers must release owned allocations exactly once via `free()` or `VulkanDeferredDeletionQueue`.
///          Non-owning references created with `reference()` or `disown()` do not release backing memory.
class GP_RHI_VULKAN_API VulkanAllocation final
{
public:
    /// @brief Default constructor producing an empty, unowned allocation handle.
    VulkanAllocation() noexcept = default;

    /// @brief Destructor (asserts in debug builds if an owned allocation was leaked without calling `free()`).
    ~VulkanAllocation() noexcept;

    /// @brief Copy constructor (permitted for transferring descriptors across deferred deletion queues).
    VulkanAllocation(const VulkanAllocation&) noexcept = default;

    /// @brief Copy assignment operator.
    VulkanAllocation& operator=(const VulkanAllocation&) noexcept = default;

    /// @brief Initializes all fields of this allocation and marks it owned (`hasOwnership = 1`).
    /// @param[in] allocType Allocation backing category.
    /// @param[in] allocMetaType Diagnostic usage category.
    /// @param[in] handle Optional backing `VkBuffer` handle (as `gp::UInt64`, or `0` for placed resources).
    /// @param[in] allocSize Logical byte size of this allocation.
    /// @param[in] alignedOffset Byte offset within the backing `VkDeviceMemory` or shared `VkBuffer`.
    /// @param[in] allocIndex Allocator slot index inside `VulkanMemoryManager`.
    /// @param[in] blockIndex Allocation slot index inside the owning `VulkanPoolAllocator` or dedicated table.
    /// @param[in] bufferId Optional monotonic identifier for shared buffer handle tracking.
    void init(
        VulkanAllocationType allocType,
        VulkanAllocationMetaType allocMetaType,
        gp::UInt64 handle,
        gp::UInt32 allocSize,
        gp::UInt32 alignedOffset,
        gp::UInt32 allocIndex,
        gp::UInt32 blockIndex,
        gp::UInt32 bufferId = 0u
    ) noexcept;

    /// @brief Releases this allocation through `memoryManager` and resets all fields to empty.
    /// @param[in,out] memoryManager Owning `VulkanMemoryManager`.
    /// @param[in] freeFlags Controls deferred vs immediate release.
    void free(VulkanMemoryManager& memoryManager, VulkanFreeFlags freeFlags = VulkanFreeFlags::None) noexcept;

    /// @brief Exchanges all fields with `other`.
    /// @param[in,out] other Target allocation to swap with.
    void swap(VulkanAllocation& other) noexcept;

    /// @brief Copies descriptor fields from `other` without taking ownership (`hasOwnership = 0`).
    /// @param[in] other Source allocation to reference.
    void reference(const VulkanAllocation& other) noexcept;

    /// @brief Reports whether this handle owns a live allocation (`hasOwnership != 0 && type != Empty`).
    /// @return True when owned and non-empty.
    [[nodiscard]] bool hasAllocation() const noexcept
    {
        return (hasOwnership != 0u) && (getType() != VulkanAllocationType::Empty);
    }

    /// @brief Relinquishes ownership (used when transferring an allocation to a deferred deletion queue).
    void disown() noexcept
    {
        hasOwnership = 0u;
    }

    /// @brief Reclaims ownership (used when a deferred deletion queue executes the actual release).
    void own() noexcept
    {
        hasOwnership = 1u;
    }

    /// @brief Reports whether this handle describes a non-empty allocation (regardless of ownership).
    /// @return True when `type != VulkanAllocationType::Empty && size > 0`.
    [[nodiscard]] bool isValid() const noexcept
    {
        return (getType() != VulkanAllocationType::Empty) && (size > 0u);
    }

    /// @brief Returns the allocation backing category.
    /// @return `VulkanAllocationType`.
    [[nodiscard]] VulkanAllocationType getType() const noexcept
    {
        return static_cast<VulkanAllocationType>(type);
    }

    /// @brief Updates the allocation backing category.
    /// @param[in] allocType New `VulkanAllocationType`.
    void setType(const VulkanAllocationType allocType) noexcept
    {
        type = static_cast<gp::UInt8>(allocType);
    }

    /// @brief Returns the mapped CPU pointer adjusted by `offset`, or `nullptr` if not host-visible/mapped.
    /// @param[in] memoryManager Owning `VulkanMemoryManager`.
    /// @return Mapped host pointer at `offset`, or `nullptr`.
    [[nodiscard]] void* getMappedPointer(VulkanMemoryManager& memoryManager) const noexcept;

    /// @brief Flushes host CPU writes for this allocation's mapped byte range.
    /// @param[in] memoryManager Owning `VulkanMemoryManager`.
    void flushMappedMemory(VulkanMemoryManager& memoryManager) const noexcept;

    /// @brief Invalidates host CPU caches for this allocation's mapped byte range.
    /// @param[in] memoryManager Owning `VulkanMemoryManager`.
    void invalidateMappedMemory(VulkanMemoryManager& memoryManager) const noexcept;

    /// @brief Returns the shared `VkBuffer` handle for `PooledBuffer` allocations, or `VK_NULL_HANDLE`.
    /// @return Native `VkBuffer`.
    [[nodiscard]] VkBuffer getBufferHandle() const noexcept
    {
        return reinterpret_cast<VkBuffer>(vulkanHandle);
    }

    /// @brief Resolves the backing native `VkDeviceMemory` handle via `memoryManager`.
    /// @param[in] memoryManager Owning `VulkanMemoryManager`.
    /// @return Native `VkDeviceMemory`, or `VK_NULL_HANDLE`.
    [[nodiscard]] VkDeviceMemory getDeviceMemoryHandle(VulkanMemoryManager& memoryManager) const noexcept;

    /// @brief Resolves the `VkMemoryPropertyFlags` of the backing device memory heap.
    /// @param[in] memoryManager Owning `VulkanMemoryManager`.
    /// @return `VkMemoryPropertyFlags`.
    [[nodiscard]] VkMemoryPropertyFlags getMemoryPropertyFlags(VulkanMemoryManager& memoryManager) const noexcept;

    /// @brief Binds `buffer` to this allocation's `VkDeviceMemory` at `offset`.
    /// @param[in] memoryManager Owning `VulkanMemoryManager`.
    /// @param[in] buffer Native `VkBuffer` to bind.
    /// @return `VkResult` returned by `vkBindBufferMemory`.
    [[nodiscard]] VkResult bindBuffer(VulkanMemoryManager& memoryManager, VkBuffer buffer) const noexcept;

    /// @brief Binds `image` to this allocation's `VkDeviceMemory` at `offset`.
    /// @param[in] memoryManager Owning `VulkanMemoryManager`.
    /// @param[in] image Native `VkImage` to bind.
    /// @return `VkResult` returned by `vkBindImageMemory`.
    [[nodiscard]] VkResult bindImage(VulkanMemoryManager& memoryManager, VkImage image) const noexcept;

    /// @brief Native backing `VkBuffer` handle encoded as 64-bit integer (for `PooledBuffer`), or `0`.
    gp::UInt64 vulkanHandle{ 0u };

    /// @brief Monotonic buffer identifier for descriptor cache invalidation.
    gp::UInt32 handleId{ 0u };

    /// @brief Logical byte size of this allocation.
    gp::UInt32 size{ 0u };

    /// @brief Byte offset within the backing `VkDeviceMemory` or shared `VkBuffer`.
    gp::UInt32 offset{ 0u };

    /// @brief Block slot index inside the owning `VulkanPoolAllocator` or dedicated allocation table.
    gp::UInt32 allocationIndex{ 0u };

    /// @brief Allocator slot index inside `VulkanMemoryManager`.
    gp::UInt16 allocatorIndex{ 0u };

    /// @brief Diagnostic allocation usage category.
    VulkanAllocationMetaType metaType : 4 { VulkanAllocationMetaType::Unknown };

    /// @brief Backing allocation category (`VulkanAllocationType`).
    gp::UInt8 type : 4 { 0u };

    /// @brief True when served by `VulkanPoolAllocator`.
    gp::UInt8 isPoolAllocated : 1 { 0u };

    /// @brief True when this handle owns the allocation and must free it.
    gp::UInt8 hasOwnership : 1 { 0u };

    /// @brief True for transient allocations.
    gp::UInt8 isTransient : 1 { 0u };

    /// @brief True for volatile temp-block suballocations.
    gp::UInt8 isTempBlockAllocator : 1 { 0u };

    /// @brief True when pinned against defragmentation.
    gp::UInt8 isUnmovable : 1 { 0u };

    /// @brief Reserved padding bits.
    gp::UInt8 reservedFlags : 3 { 0u };

    static_assert(
        static_cast<gp::USize>(VulkanAllocationType::Count) < 16u, "VulkanAllocationType must fit in 4 bits."
    );
    static_assert(
        static_cast<gp::USize>(VulkanAllocationMetaType::Count) <= 16u, "VulkanAllocationMetaType must fit in 4 bits."
    );
};

}   // namespace gp::rhi::vulkan
