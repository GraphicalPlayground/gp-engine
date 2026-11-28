// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Single raw `VkDeviceMemory` allocation off a physical device heap.
/// @details Bounded globally by `VkPhysicalDeviceLimits::maxMemoryAllocationCount`. Instances are created and freed
///          exclusively by `VulkanDeviceMemoryManager`.
class GP_RHI_VULKAN_API VulkanDeviceMemoryAllocation final
{
public:
    /// @brief Default constructor initializing an empty allocation record.
    VulkanDeviceMemoryAllocation() noexcept = default;

    VulkanDeviceMemoryAllocation(const VulkanDeviceMemoryAllocation&) = delete;
    VulkanDeviceMemoryAllocation& operator=(const VulkanDeviceMemoryAllocation&) = delete;
    VulkanDeviceMemoryAllocation(VulkanDeviceMemoryAllocation&&) = delete;
    VulkanDeviceMemoryAllocation& operator=(VulkanDeviceMemoryAllocation&&) = delete;

    /// @brief Maps `[offset, offset + size)` into CPU address space (or returns the existing persistent mapping).
    /// @param[in] size Byte range to map (or `VK_WHOLE_SIZE`).
    /// @param[in] offset Byte offset from the start of `m_handle`.
    /// @return Mapped host pointer, or `nullptr` if mapping is unsupported or failed.
    [[nodiscard]] void* map(VkDeviceSize size, VkDeviceSize offset) noexcept;

    /// @brief Unmaps the active CPU mapping if currently mapped.
    void unmap() noexcept;

    /// @brief Reports whether this allocation's memory type includes `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT`.
    /// @return True when host-mappable.
    [[nodiscard]] bool canBeMapped() const noexcept
    {
        return m_canBeMapped != 0u;
    }

    /// @brief Reports whether a persistent CPU mapping is currently active.
    /// @return True when `m_mappedPointer != nullptr`.
    [[nodiscard]] bool isMapped() const noexcept
    {
        return m_mappedPointer != nullptr;
    }

    /// @brief Returns the active persistent CPU mapping pointer (or `nullptr` if unmapped).
    /// @return Host pointer or `nullptr`.
    [[nodiscard]] void* getMappedPointer() const noexcept
    {
        return m_mappedPointer;
    }

    /// @brief Reports whether this allocation's memory type includes `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`.
    /// @return True when host-coherent.
    [[nodiscard]] bool isCoherent() const noexcept
    {
        return m_isCoherent != 0u;
    }

    /// @brief Reports whether this allocation's memory type includes `VK_MEMORY_PROPERTY_HOST_CACHED_BIT`.
    /// @return True when host-cached.
    [[nodiscard]] bool isCached() const noexcept
    {
        return m_isCached != 0u;
    }

    /// @brief Reports whether this allocation was created with `VkMemoryDedicatedAllocateInfo`.
    /// @return True when dedicated.
    [[nodiscard]] bool isDedicated() const noexcept
    {
        return m_dedicatedMemory != 0u;
    }

    /// @brief Flushes host writes for `[offset, offset + size)` when non-coherent (or forced via CVar).
    /// @param[in] offset Starting byte offset.
    /// @param[in] size Range length in bytes.
    void flushMappedMemory(VkDeviceSize offset, VkDeviceSize size) noexcept;

    /// @brief Invalidates host caches for `[offset, offset + size)` when non-coherent (or forced via CVar).
    /// @param[in] offset Starting byte offset.
    /// @param[in] size Range length in bytes.
    void invalidateMappedMemory(VkDeviceSize offset, VkDeviceSize size) noexcept;

    /// @brief Returns the native `VkDeviceMemory` handle.
    /// @return Native `VkDeviceMemory`.
    [[nodiscard]] VkDeviceMemory getHandle() const noexcept
    {
        return m_handle;
    }

    /// @brief Returns the allocated byte capacity.
    /// @return Allocation size in bytes.
    [[nodiscard]] VkDeviceSize getSize() const noexcept
    {
        return m_size;
    }

    /// @brief Returns the physical device memory type index.
    /// @return Memory type index.
    [[nodiscard]] gp::UInt32 getMemoryTypeIndex() const noexcept
    {
        return m_memoryTypeIndex;
    }

private:
    /// @brief Private destructor enforcing release exclusively through `VulkanDeviceMemoryManager::free()`.
    ~VulkanDeviceMemoryAllocation() noexcept;

    VkDeviceSize m_size{ 0u };
    VkDevice m_deviceHandle{ VK_NULL_HANDLE };
    VkDeviceMemory m_handle{ VK_NULL_HANDLE };
    void* m_mappedPointer{ nullptr };

    gp::UInt32 m_memoryTypeIndex : 8 { 0u };
    gp::UInt32 m_canBeMapped : 1 { 0u };
    gp::UInt32 m_isCoherent : 1 { 0u };
    gp::UInt32 m_isCached : 1 { 0u };
    gp::UInt32 m_freedBySystem : 1 { 0u };
    gp::UInt32 m_dedicatedMemory : 1 { 0u };
    gp::UInt32 m_isExternal : 1 { 0u };

    friend class VulkanDeviceMemoryManager;
};

/// @brief Adjusts a mapped memory flush/invalidate size to satisfy `VkPhysicalDeviceLimits::nonCoherentAtomSize`.
/// @param[in] requestedOffset Starting byte offset (assumed aligned to `nonCoherentAtomSize`).
/// @param[in] requestedSize Logical byte size to flush or invalidate.
/// @param[in] wholeAllocationSize Total size of the `VkDeviceMemory` allocation in bytes.
/// @param[in] nonCoherentAtomSize Hardware non-coherent atom size in bytes.
/// @return Adjusted byte size clamped to `wholeAllocationSize - requestedOffset`.
[[nodiscard]] GP_RHI_VULKAN_API VkDeviceSize adjustToNonCoherentAtomSize(
    VkDeviceSize requestedOffset,
    VkDeviceSize requestedSize,
    VkDeviceSize wholeAllocationSize,
    VkDeviceSize nonCoherentAtomSize
) noexcept;

/// @brief Flushes a mapped memory range, honoring `nonCoherentAtomSize` and `r.Vulkan.ForceCoherentOperations`.
/// @param[in] device Owning `VulkanDevice`.
/// @param[in,out] allocation Target device memory allocation.
/// @param[in] offset Starting byte offset.
/// @param[in] size Logical range length in bytes.
GP_RHI_VULKAN_API void flushMappedMemoryRange(
    const VulkanDevice& device, VulkanDeviceMemoryAllocation& allocation, VkDeviceSize offset, VkDeviceSize size
) noexcept;

/// @brief Invalidates a mapped memory range, honoring `nonCoherentAtomSize` and `r.Vulkan.ForceCoherentOperations`.
/// @param[in] device Owning `VulkanDevice`.
/// @param[in,out] allocation Target device memory allocation.
/// @param[in] offset Starting byte offset.
/// @param[in] size Logical range length in bytes.
GP_RHI_VULKAN_API void invalidateMappedMemoryRange(
    const VulkanDevice& device, VulkanDeviceMemoryAllocation& allocation, VkDeviceSize offset, VkDeviceSize size
) noexcept;

/// @brief Manages low-level `VkDeviceMemory` allocations, memory-type selection, heap budgets, and block caching.
class GP_RHI_VULKAN_API VulkanDeviceMemoryManager final
{
public:
    /// @brief Default constructor.
    VulkanDeviceMemoryManager() noexcept;

    /// @brief Destructor. Calls `deinit()` to release cached blocks.
    ~VulkanDeviceMemoryManager() noexcept;

    VulkanDeviceMemoryManager(const VulkanDeviceMemoryManager&) = delete;
    VulkanDeviceMemoryManager& operator=(const VulkanDeviceMemoryManager&) = delete;
    VulkanDeviceMemoryManager(VulkanDeviceMemoryManager&&) = delete;
    VulkanDeviceMemoryManager& operator=(VulkanDeviceMemoryManager&&) = delete;

    /// @brief Initializes memory properties, heap tracking, primary heap detection, and budget queries for `device`.
    /// @param[in] device Active `VulkanDevice` (must remain valid until `deinit()`).
    void init(VulkanDevice* device) noexcept;

    /// @brief Trims all reserved blocks and checks for leaked allocations.
    void deinit() noexcept;

    /// @brief Refreshes `VkPhysicalDeviceMemoryProperties` and `VK_EXT_memory_budget` usage/budget stats.
    void updateMemoryProperties() noexcept;

    /// @brief Reports whether the device uses a unified memory architecture.
    /// @return True when unified memory is active.
    [[nodiscard]] bool hasUnifiedMemory() const noexcept
    {
        return m_hasUnifiedMemory;
    }

    /// @brief Reports whether any memory type exposes `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT`.
    /// @return True when memoryless allocations are supported.
    [[nodiscard]] bool supportsMemoryless() const noexcept
    {
        return m_supportsMemoryless;
    }

    /// @brief Returns the number of memory types reported by the physical device.
    /// @return Memory type count.
    [[nodiscard]] gp::UInt32 getNumMemoryTypes() const noexcept
    {
        return m_memoryProperties.memoryTypeCount;
    }

    /// @brief Reports whether a memory type exists whose property flags exactly equal `properties`.
    /// @param[in] properties Exact `VkMemoryPropertyFlags` combination to test.
    /// @return True when a matching memory type exists.
    [[nodiscard]] bool supportsMemoryType(VkMemoryPropertyFlags properties) const noexcept;

    /// @brief Returns the current allocation footprint and limit of the primary device-local heap.
    /// @param[out] outAllocated Bytes currently allocated on the primary heap.
    /// @param[out] outLimit Total capacity (or budget) of the primary heap in bytes.
    void getPrimaryHeapStatus(gp::UInt64& outAllocated, gp::UInt64& outLimit) const noexcept;

    /// @brief Selects the first memory type matching `typeBits` and containing all bits in `properties`.
    /// @param[in] typeBits Bitmask of allowed memory types from `VkMemoryRequirements::memoryTypeBits`.
    /// @param[in] properties Required `VkMemoryPropertyFlags`.
    /// @param[out] outTypeIndex Receives the selected memory type index on success.
    /// @return `VK_SUCCESS` if found, or `VK_ERROR_FEATURE_NOT_PRESENT` otherwise.
    [[nodiscard]] VkResult getMemoryTypeFromProperties(
        gp::UInt32 typeBits, VkMemoryPropertyFlags properties, gp::UInt32* outTypeIndex
    ) const noexcept;

    /// @brief Selects the first memory type matching `typeBits` and `properties` while excluding `excludeTypeIndex`.
    /// @param[in] typeBits Bitmask of allowed memory types.
    /// @param[in] properties Required `VkMemoryPropertyFlags`.
    /// @param[in] excludeTypeIndex Memory type index to skip.
    /// @param[out] outTypeIndex Receives the selected memory type index on success.
    /// @return `VK_SUCCESS` if found, or `VK_ERROR_FEATURE_NOT_PRESENT` otherwise.
    [[nodiscard]] VkResult getMemoryTypeFromPropertiesExcluding(
        gp::UInt32 typeBits, VkMemoryPropertyFlags properties, gp::UInt32 excludeTypeIndex, gp::UInt32* outTypeIndex
    ) const noexcept;

    /// @brief Returns the cached `VkPhysicalDeviceMemoryProperties`.
    /// @return Const reference to `VkPhysicalDeviceMemoryProperties`.
    [[nodiscard]] const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const noexcept
    {
        return m_memoryProperties;
    }

    /// @brief Returns the latest `VK_EXT_memory_budget` properties snapshot (zeroed if unsupported).
    /// @return Const reference to `VkPhysicalDeviceMemoryBudgetPropertiesEXT`.
    [[nodiscard]] const VkPhysicalDeviceMemoryBudgetPropertiesEXT& getMemoryBudget() const noexcept
    {
        return m_memoryBudget;
    }

    /// @brief Allocates (or reuses a cached block of) `allocationSize` bytes from `memoryTypeIndex`.
    /// @param[in] canFail When true, OOM returns `nullptr` cleanly without fatal logging.
    /// @param[in] allocationSize Requested byte size (`> 0`).
    /// @param[in] memoryTypeIndex Target memory type index.
    /// @param[in] dedicatedAllocateInfo Optional `VkMemoryDedicatedAllocateInfo*` for dedicated allocations.
    /// @param[in] priority Memory residency priority in `[0.0f, 1.0f]` (for `VK_EXT_memory_priority`).
    /// @param[in] isExternal True to attach external memory export info.
    /// @return Pointer to `VulkanDeviceMemoryAllocation`, or `nullptr` on failure.
    [[nodiscard]] VulkanDeviceMemoryAllocation* alloc(
        bool canFail,
        VkDeviceSize allocationSize,
        gp::UInt32 memoryTypeIndex,
        void* dedicatedAllocateInfo = nullptr,
        gp::Float32 priority = kVulkanMemoryMediumPriority,
        bool isExternal = false
    ) noexcept;

    /// @brief Resolves a memory type from `(memoryTypeBits, memoryPropertyFlags)` and delegates to `alloc()`.
    /// @param[in] canFail When true, failure returns `nullptr`.
    /// @param[in] allocationSize Requested byte size.
    /// @param[in] memoryTypeBits Allowed memory type bitmask.
    /// @param[in] memoryPropertyFlags Required `VkMemoryPropertyFlags`.
    /// @param[in] dedicatedAllocateInfo Optional dedicated allocation info.
    /// @param[in] priority Memory residency priority.
    /// @param[in] isExternal True for external memory export.
    /// @return Pointer to `VulkanDeviceMemoryAllocation`, or `nullptr` on failure.
    [[nodiscard]] VulkanDeviceMemoryAllocation* alloc(
        bool canFail,
        VkDeviceSize allocationSize,
        gp::UInt32 memoryTypeBits,
        VkMemoryPropertyFlags memoryPropertyFlags,
        void* dedicatedAllocateInfo = nullptr,
        gp::Float32 priority = kVulkanMemoryMediumPriority,
        bool isExternal = false
    ) noexcept;

    /// @brief Returns `allocation` to the block cache (or immediately frees dedicated allocations) and nulls the
    /// pointer.
    /// @param[in,out] allocation Reference to allocation pointer; set to `nullptr` on return.
    /// @param[in] currentFrame Current render frame index used for block-cache aging.
    void free(VulkanDeviceMemoryAllocation*& allocation, gp::UInt32 currentFrame = 0u) noexcept;

    /// @brief Releases aged reserved blocks back to the Vulkan driver via `vkFreeMemory`.
    /// @param[in] fullTrim When true, frees every cached reserved block immediately regardless of age.
    /// @param[in] currentFrame Current render frame index used for block-cache aging.
    void trimMemory(bool fullTrim, gp::UInt32 currentFrame = 0u) noexcept;

    /// @brief Returns total physical capacity of GPU-local (`gpu == true`) or host-system (`gpu == false`) heaps.
    /// @param[in] gpu True to sum `VK_MEMORY_HEAP_DEVICE_LOCAL_BIT` heaps; false for non-local heaps.
    /// @return Total heap capacity in bytes.
    [[nodiscard]] gp::UInt64 getTotalMemory(bool gpu) const noexcept;

    /// @brief Returns the budget (when `VK_EXT_memory_budget` is active) or physical size of `heapIndex`.
    /// @param[in] heapIndex Memory heap index.
    /// @return Heap capacity or budget in bytes.
    [[nodiscard]] VkDeviceSize getBaseHeapSize(gp::UInt32 heapIndex) const noexcept;

    /// @brief Returns the heap index backing `memoryTypeIndex`.
    /// @param[in] memoryTypeIndex Physical device memory type index.
    /// @return Heap index (or `0` if out of range).
    [[nodiscard]] gp::UInt32 getHeapIndex(gp::UInt32 memoryTypeIndex) const noexcept;

    /// @brief Returns the fallback memory property flags used when evicting resources from device-local memory.
    /// @return Vendor-appropriate evicted `VkMemoryPropertyFlags`.
    [[nodiscard]] VkMemoryPropertyFlags getEvictedMemoryProperties() const noexcept;

    /// @brief Returns the latest `VulkanMemoryStats` snapshot computed by `updateMemoryProperties()`.
    /// @return Const reference to `VulkanMemoryStats`.
    [[nodiscard]] const VulkanMemoryStats& getMemoryStats() const noexcept
    {
        return m_memoryStats;
    }

    /// @brief Populates per-heap allocation and reserved-block statistics into `outDeviceHeapsStats`.
    /// @param[out] outDeviceHeapsStats Destination array resized to `memoryHeapCount`.
    void getMemoryDump(gp::Vector<VulkanResourceHeapStats>& outDeviceHeapsStats) const noexcept;

    /// @brief Returns the current number of live `VkDeviceMemory` allocations owned by this manager.
    /// @return Active `vkAllocateMemory` count.
    [[nodiscard]] gp::UInt32 getNumAllocations() const noexcept;

    /// @brief Returns the high-water mark of simultaneous `VkDeviceMemory` allocations.
    /// @return Peak `vkAllocateMemory` count.
    [[nodiscard]] gp::UInt32 getPeakNumAllocations() const noexcept;

private:
    /// @brief Lookup key pairing `(memoryTypeIndex, blockSize)` for cached reusable `VkDeviceMemory` blocks.
    struct BlockKey final
    {
        gp::UInt32 memoryTypeIndex{ 0u };
        VkDeviceSize blockSize{ 0u };

        /// @brief Compares two block keys for identical memory type and allocation size.
        /// @param[in] other Key to compare against.
        /// @return True when both fields match.
        [[nodiscard]] constexpr bool operator==(const BlockKey& other) const noexcept
        {
            return (memoryTypeIndex == other.memoryTypeIndex) && (blockSize == other.blockSize);
        }
    };

    /// @brief Single cached free `VkDeviceMemory` block awaiting reuse or trim.
    struct FreeBlock final
    {
        VulkanDeviceMemoryAllocation* allocation{ nullptr };
        gp::UInt32 frameFreed{ 0u };
    };

    /// @brief Bucket of cached free blocks sharing the same `(memoryTypeIndex, blockSize)`.
    struct BlockBucket final
    {
        BlockKey key{};
        gp::Vector<FreeBlock> freeBlocks{};
    };

    /// @brief Per-heap live allocation tracking.
    struct HeapInfo final
    {
        VkDeviceSize usedSize{ 0u };
        VkDeviceSize peakSize{ 0u };
        gp::Vector<VulkanDeviceMemoryAllocation*> allocations{};
    };

    /// @brief Unmaps (if mapped), invokes `vkFreeMemory`, updates heap stats, and deletes `allocation`.
    /// @param[in] allocation Allocation to destroy (caller holds `m_mutex`).
    void freeInternalLocked(VulkanDeviceMemoryAllocation* allocation) noexcept;

    /// @brief Finds or creates the `BlockBucket` matching `key` (caller holds `m_mutex`).
    /// @param[in] key Lookup key.
    /// @return Reference to matching `BlockBucket`.
    BlockBucket& findOrAddBucketLocked(const BlockKey& key) noexcept;

    VulkanDevice* m_device{ nullptr };
    VkPhysicalDeviceMemoryBudgetPropertiesEXT m_memoryBudget{};
    VkPhysicalDeviceMemoryProperties m_memoryProperties{};
    VulkanMemoryStats m_memoryStats{};

    gp::UInt32 m_numAllocations{ 0u };
    gp::UInt32 m_peakNumAllocations{ 0u };
    bool m_hasUnifiedMemory{ false };
    bool m_supportsMemoryless{ false };
    gp::Int32 m_primaryHeapIndex{ -1 };

    gp::Vector<HeapInfo> m_heapInfos{};
    gp::Vector<BlockBucket> m_cachedBlocks{};
    mutable gp::thread::Mutex m_mutex{};
};

}   // namespace gp::rhi::vulkan
