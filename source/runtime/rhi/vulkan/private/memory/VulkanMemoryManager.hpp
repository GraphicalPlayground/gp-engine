// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"   // IWYU pragma: keep
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "memory/VulkanAllocation.hpp"
#include "memory/VulkanDeviceMemory.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "memory/VulkanPoolAllocator.hpp"
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/SharedMutex.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Creates a `VkBuffer` on `device` with exclusive or concurrent sharing across active queue families.
/// @param[in] device Active `VulkanDevice`.
/// @param[in] size Buffer size in bytes.
/// @param[in] usageFlags Native `VkBufferUsageFlags`.
/// @param[in] createFlags Optional `VkBufferCreateFlags` (sparse binding and residency for reserved buffers).
/// @return Newly created `VkBuffer`, or `VK_NULL_HANDLE` on failure.
[[nodiscard]] GP_RHI_VULKAN_API VkBuffer createVulkanBuffer(
    const VulkanDevice& device, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkBufferCreateFlags createFlags = 0u
) noexcept;

/// @brief Top-level Vulkan GPU memory manager coordinating pool suballocators, dedicated allocations, and deferred
///        frees.
class GP_RHI_VULKAN_API VulkanMemoryManager final
{
public:
    /// @brief Default manual sub-allocation page size for general buffers (1 MB).
    static constexpr gp::UInt64 kDefaultBufferPoolSize = 1ULL * 1024ULL * 1024ULL;

    /// @brief Default manual sub-allocation page size for uniform buffers (2 MB).
    static constexpr gp::UInt64 kUniformBufferPoolSize = 2ULL * 1024ULL * 1024ULL;

    /// @brief Default GPU-only placed pool page size (128 MB, clamped to heapSize / 8).
    static constexpr gp::UInt64 kGpuOnlyPoolPageSize = 128ULL * 1024ULL * 1024ULL;

    /// @brief Default host-visible upload/download pool page size (32 MB).
    static constexpr gp::UInt64 kStagingPoolPageSize = 32ULL * 1024ULL * 1024ULL;

    /// @brief Upper threshold for sub-allocating inside shared pools before falling back to dedicated allocations (64
    /// MB).
    static constexpr gp::UInt64 kMaxSubAllocationSize = 64ULL * 1024ULL * 1024ULL;

    /// @brief Constructs a memory manager bound to `device` and `deviceMemoryManager`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] deviceMemoryManager Low-level device memory manager.
    VulkanMemoryManager(VulkanDevice& device, VulkanDeviceMemoryManager& deviceMemoryManager) noexcept;

    /// @brief Destructor. Calls `deinit()` to release all pools and fallback allocations.
    ~VulkanMemoryManager() noexcept;

    VulkanMemoryManager(const VulkanMemoryManager&) = delete;
    VulkanMemoryManager& operator=(const VulkanMemoryManager&) = delete;
    VulkanMemoryManager(VulkanMemoryManager&&) = delete;
    VulkanMemoryManager& operator=(VulkanMemoryManager&&) = delete;

    /// @brief Initializes placed-resource and pre-created manual sub-allocation pool allocators.
    void init() noexcept;

    /// @brief Destroys all pool allocators, dedicated fallback allocations, and resets internal state.
    void deinit() noexcept;

    /// @brief Attaches an optional `VulkanDeferredDeletionQueue` used when `freeVulkanAllocation()` defers releases.
    /// @param[in] deletionQueue Non-owning pointer to `VulkanDeferredDeletionQueue`, or `nullptr`.
    void setDeferredDeletionQueue(VulkanDeferredDeletionQueue* const deletionQueue) noexcept
    {
        m_deferredDeletionQueue = deletionQueue;
    }

    /// @brief Returns the owning `VulkanDevice`.
    /// @return Mutable reference to `VulkanDevice`.
    [[nodiscard]] VulkanDevice& getDevice() noexcept
    {
        return m_device;
    }

    /// @brief Returns the const owning `VulkanDevice`.
    /// @return Const reference to `VulkanDevice`.
    [[nodiscard]] const VulkanDevice& getDevice() const noexcept
    {
        return m_device;
    }

    /// @brief Returns the low-level `VulkanDeviceMemoryManager`.
    /// @return Mutable reference to `VulkanDeviceMemoryManager`.
    [[nodiscard]] VulkanDeviceMemoryManager& getDeviceMemoryManager() noexcept
    {
        return m_deviceMemoryManager;
    }

    /// @brief Computes minimum byte alignment for a buffer with `bufferUsageFlags` on `device`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] bufferUsageFlags Native `VkBufferUsageFlags`.
    /// @return Minimum alignment in bytes (`>= 16`).
    [[nodiscard]] static gp::UInt32
        calculateBufferAlignment(const VulkanDevice& device, VkBufferUsageFlags bufferUsageFlags) noexcept;

    /// @brief Computes device memory residency priority for a buffer with `bufferUsageFlags`.
    /// @param[in] bufferUsageFlags Native `VkBufferUsageFlags`.
    /// @return Priority value in `[0.0f, 1.0f]`.
    [[nodiscard]] static gp::Float32 calculateBufferPriority(VkBufferUsageFlags bufferUsageFlags) noexcept;

    /// @brief Sub-allocates a byte range inside a shared `ManualSubAllocation` `VkBuffer` pool.
    /// @param[out] outAllocation Populated `VulkanAllocation` on success.
    /// @param[in] size Logical allocation size in bytes.
    /// @param[in] minAlignment Caller minimum byte alignment.
    /// @param[in] bufferUsageFlags Required `VkBufferUsageFlags`.
    /// @param[in] memoryPropertyFlags Required `VkMemoryPropertyFlags`.
    /// @param[in] metaType Diagnostic usage category.
    /// @return True when allocation succeeded; false otherwise.
    [[nodiscard]] bool allocateBufferPooled(
        VulkanAllocation& outAllocation,
        gp::UInt32 size,
        gp::UInt32 minAlignment,
        VkBufferUsageFlags bufferUsageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VulkanAllocationMetaType metaType
    ) noexcept;

    /// @brief Allocates backing memory for `buffer`, optionally binding it immediately (`AutoBind`).
    /// @param[out] outAllocation Populated `VulkanAllocation` on success.
    /// @param[in] buffer Native `VkBuffer` whose memory requirements are queried via `vkGetBufferMemoryRequirements2`.
    /// @param[in] allocFlags Allocation placement and binding hints.
    /// @param[in] debugName Optional UTF-8 debug name applied to `buffer`.
    /// @param[in] forceMinAlignment Optional caller minimum alignment override.
    /// @return True when allocation (and optional bind) succeeded; false otherwise.
    [[nodiscard]] bool allocateBufferMemory(
        VulkanAllocation& outAllocation,
        VkBuffer buffer,
        VulkanAllocationFlags allocFlags,
        const char* debugName = nullptr,
        gp::UInt32 forceMinAlignment = 1u
    ) noexcept;

    /// @brief Allocates placed or dedicated backing memory for an image matching `memoryReqs`.
    /// @param[out] outAllocation Populated `VulkanAllocation` on success.
    /// @param[in] memoryReqs Image `VkMemoryRequirements`.
    /// @param[in] memoryPropertyFlags Desired `VkMemoryPropertyFlags`.
    /// @param[in] metaType Diagnostic usage category.
    /// @param[in] isExternal True for external memory export.
    /// @return True when allocation succeeded; false otherwise.
    [[nodiscard]] bool allocateImageMemory(
        VulkanAllocation& outAllocation,
        const VkMemoryRequirements& memoryReqs,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VulkanAllocationMetaType metaType,
        bool isExternal = false
    ) noexcept;

    /// @brief Queries `vkGetImageMemoryRequirements2` and allocates dedicated memory when preferred/forced, otherwise
    ///        falls back to `allocateImageMemory()`.
    /// @param[out] outAllocation Populated `VulkanAllocation` on success.
    /// @param[in] image Native `VkImage`.
    /// @param[in] memoryPropertyFlags Desired `VkMemoryPropertyFlags`.
    /// @param[in] metaType Diagnostic usage category.
    /// @param[in] isExternal True for external memory export.
    /// @param[in] forceDedicatedAllocation When true, always uses `VkMemoryDedicatedAllocateInfo`.
    /// @return True when allocation succeeded; false otherwise.
    [[nodiscard]] bool allocateDedicatedImageMemory(
        VulkanAllocation& outAllocation,
        VkImage image,
        VkMemoryPropertyFlags memoryPropertyFlags,
        VulkanAllocationMetaType metaType,
        bool isExternal = false,
        bool forceDedicatedAllocation = false
    ) noexcept;

    /// @brief Frees `allocation` either via `m_deferredDeletionQueue` or immediately when `DontDefer` is set.
    /// @param[in,out] allocation Allocation to release (cleared on return).
    /// @param[in] freeFlags Controls deferred vs immediate release.
    void freeVulkanAllocation(VulkanAllocation& allocation, VulkanFreeFlags freeFlags = VulkanFreeFlags::None) noexcept;

    /// @brief Frees a batch of pooled allocations grouped by `allocatorIndex` so each pool locks only once.
    /// @param[in,out] allocations View of pooled allocations to free (entries cleared on return).
    void freePoolAllocations(gp::VectorView<VulkanAllocation> allocations) noexcept;

    /// @brief Reclaims empty pooled pages across all pool allocators.
    /// @param[in] forceFree When true, frees empty pools immediately regardless of frame lag.
    /// @param[in] currentFrame Current deletion frame number.
    void releaseEmptyPools(bool forceFree, gp::UInt32 currentFrame = 0u) noexcept;

    /// @brief Releases aged empty pools and trims cached raw `VkDeviceMemory` blocks.
    /// @param[in] currentFrame Current deletion frame number.
    void releaseFreedPages(gp::UInt32 currentFrame = 0u) noexcept;

    /// @brief Evaluates primary heap usage against eviction hysteresis thresholds.
    /// @return True when new evictable allocations should spill out of device-local memory.
    [[nodiscard]] bool updateEvictThreshold() noexcept;

    /// @brief Resolves the `VulkanPoolAllocator` at `allocatorIndex`, or `nullptr` if out of range.
    /// @param[in] allocatorIndex Slot index stored in `VulkanAllocation::allocatorIndex`.
    /// @return Pointer to `VulkanPoolAllocator`, or `nullptr`.
    [[nodiscard]] VulkanPoolAllocator* getPoolAllocator(gp::UInt32 allocatorIndex) noexcept;

    /// @brief Registers `owner` as the defragmentation owner of a pooled, movable `allocation`.
    /// @details Allocations served outside the pool allocators, pinned allocations, and null owners are ignored.
    /// @param[in] allocation Live allocation.
    /// @param[in] owner Resource that will receive relocation callbacks, or `nullptr`.
    void setPoolAllocationOwner(const VulkanAllocation& allocation, gp::rhi::allocator::PoolResource* owner) noexcept;

    /// @brief Moves the defragmentation ownership of a pooled, movable `allocation` to `newOwner`.
    /// @param[in] allocation Live allocation.
    /// @param[in] newOwner Resource that now owns the allocation, or `nullptr` (ignored).
    void transferPoolAllocationOwner(
        const VulkanAllocation& allocation, gp::rhi::allocator::PoolResource* newOwner
    ) noexcept;

    /// @brief Pins `allocation` so defragmentation never relocates it.
    /// @details Clearing the block owner is what keeps the pool from offering the range for relocation every frame.
    /// @param[in,out] allocation Live allocation to pin (`isUnmovable` is set).
    void setAllocationUnmovable(VulkanAllocation& allocation) noexcept;

    /// @brief Describes the destination range of a pooled allocation that is being relocated to `block`.
    /// @details The returned allocation is disowned: the owning handle stays with the resource, so this one must never
    ///          free anything.
    /// @param[out] outAllocation Receives the destination description.
    /// @param[in] existing Current pooled allocation of the resource.
    /// @param[in] block Destination pool block.
    void initPoolAllocation(
        VulkanAllocation& outAllocation,
        const VulkanAllocation& existing,
        const gp::rhi::allocator::PoolAllocationData& block
    ) noexcept;

    /// @brief Resolves the backing `VulkanDeviceMemoryAllocation*` for any live `VulkanAllocation`.
    /// @param[in] allocation Live allocation handle.
    /// @return Backing `VulkanDeviceMemoryAllocation*`, or `nullptr`.
    [[nodiscard]] VulkanDeviceMemoryAllocation* getDeviceMemoryAllocation(const VulkanAllocation& allocation) noexcept;

    /// @brief Accumulates per-pool and dedicated fallback memory statistics into `outStats`.
    /// @param[out] outStats Destination stats snapshot.
    void getMemoryDump(VulkanResourceHeapStats& outStats) noexcept;

    /// @brief Returns the probed image pool alignment in bytes.
    /// @return Image pool alignment.
    [[nodiscard]] gp::UInt32 getImagePoolAlignment() const noexcept
    {
        return m_imagePoolAlignment;
    }

    /// @brief Returns the computed buffer pool alignment in bytes.
    /// @return Buffer pool alignment.
    [[nodiscard]] gp::UInt32 getBufferPoolAlignment() const noexcept
    {
        return m_bufferPoolAlignment;
    }

private:
    /// @brief Fallback slot storing a single direct/dedicated `VulkanDeviceMemoryAllocation` (and optional `VkBuffer`).
    struct DirectAllocationSlot final
    {
        VulkanDeviceMemoryAllocation* memoryAllocation{ nullptr };
        VkBuffer ownedBuffer{ VK_NULL_HANDLE };
        VulkanAllocationType type{ VulkanAllocationType::Empty };
        VulkanAllocationMetaType metaType{ VulkanAllocationMetaType::Unknown };
    };

    /// @brief Creates placed and dedicated manual pool allocators during `init()`.
    void initPoolAllocators() noexcept;

    /// @brief Builds a placed-resource `VulkanPoolConfig` for `(resourceType, memoryTypeIndex)`.
    /// @param[in] resourceType Buffer or Image.
    /// @param[in] memoryTypeIndex Target memory type index.
    /// @return Populated `VulkanPoolConfig`.
    [[nodiscard]] VulkanPoolConfig
        makePlacedPoolConfig(VulkanResourceType resourceType, gp::UInt32 memoryTypeIndex) const noexcept;

    /// @brief Builds a manual sub-allocation `VulkanPoolConfig` for `(bufferUsageFlags, memoryPropertyFlags)`.
    /// @param[in] bufferUsageFlags Shared `VkBuffer` usage flags.
    /// @param[in] memoryPropertyFlags Desired `VkMemoryPropertyFlags`.
    /// @param[in] poolSize Backing page size in bytes.
    /// @return Populated `VulkanPoolConfig`.
    [[nodiscard]] VulkanPoolConfig makeManualPoolConfig(
        VkBufferUsageFlags bufferUsageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
        gp::UInt64 poolSize = kDefaultBufferPoolSize
    ) const noexcept;

    /// @brief Computes default pool page size for `memoryTypeIndex`.
    /// @param[in] memoryTypeIndex Physical device memory type index.
    /// @return Page size in bytes.
    [[nodiscard]] gp::UInt64 getPoolSizeForMemoryType(gp::UInt32 memoryTypeIndex) const noexcept;

    /// @brief Returns the total number of placed-resource pool allocator slots (`2 * numMemoryTypes`).
    /// @return Placed pool count.
    [[nodiscard]] gp::UInt32 getPlacedPoolCount() const noexcept
    {
        return static_cast<gp::UInt32>(kVulkanResourceTypeCount) * m_deviceMemoryManager.getNumMemoryTypes();
    }

    /// @brief Finds an existing matching `VulkanPoolAllocator` or creates one on demand for `config`.
    /// @param[in] config Pool configuration key.
    /// @return Pointer to `VulkanPoolAllocator`, or `nullptr` if unavailable.
    [[nodiscard]] VulkanPoolAllocator* findOrCreatePoolAllocator(const VulkanPoolConfig& config) noexcept;

    /// @brief Attempts to sub-allocate via a `VulkanPoolAllocator` matching `config`.
    /// @param[out] outAllocation Populated allocation handle on success.
    /// @param[in] config Pool configuration key.
    /// @param[in] size Logical allocation size in bytes.
    /// @param[in] alignment Required byte alignment.
    /// @param[in] allocType Allocation category.
    /// @param[in] metaType Diagnostic usage category.
    /// @return True on success; false to fall back to direct allocation.
    [[nodiscard]] bool tryAllocateFromPool(
        VulkanAllocation& outAllocation,
        const VulkanPoolConfig& config,
        gp::UInt32 size,
        gp::UInt32 alignment,
        VulkanAllocationType allocType,
        VulkanAllocationMetaType metaType
    ) noexcept;

    /// @brief Allocates a direct/dedicated `VulkanDeviceMemoryAllocation` and registers it in `m_directAllocations`.
    /// @param[out] outAllocation Populated `VulkanAllocation` on success.
    /// @param[in] size Requested byte size.
    /// @param[in] memoryTypeIndex Physical device memory type index.
    /// @param[in] allocType Allocation category.
    /// @param[in] metaType Diagnostic usage category.
    /// @param[in] mapAllocation True to persistently map host-visible memory.
    /// @param[in] dedicatedAllocateInfo Optional `VkMemoryDedicatedAllocateInfo*`.
    /// @param[in] priority Memory priority in `[0.0f, 1.0f]`.
    /// @param[in] isExternal True for external memory export.
    /// @param[in] ownedBuffer Optional `VkBuffer` owned alongside this allocation.
    /// @return True on success; false otherwise.
    [[nodiscard]] bool allocateDirect(
        VulkanAllocation& outAllocation,
        VkDeviceSize size,
        gp::UInt32 memoryTypeIndex,
        VulkanAllocationType allocType,
        VulkanAllocationMetaType metaType,
        bool mapAllocation,
        void* dedicatedAllocateInfo,
        gp::Float32 priority,
        bool isExternal,
        VkBuffer ownedBuffer = VK_NULL_HANDLE
    ) noexcept;

    /// @brief Releases a non-pooled direct allocation registered in `m_directAllocations`.
    /// @param[in,out] allocation Direct allocation to free.
    void freeDirectAllocation(VulkanAllocation& allocation) noexcept;

    /// @brief Collects a thread-safe snapshot of all active `VulkanPoolAllocator*` instances.
    /// @param[out] outAllocators Destination snapshot array.
    void getPoolAllocatorsSnapshot(gp::Vector<VulkanPoolAllocator*>& outAllocators) noexcept;

    VulkanDevice& m_device;
    VulkanDeviceMemoryManager& m_deviceMemoryManager;
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    gp::Array<gp::Array<gp::UniquePtr<VulkanPoolAllocator>, VK_MAX_MEMORY_TYPES>, kVulkanResourceTypeCount>
        m_placedPoolAllocators{};
    gp::Vector<gp::UniquePtr<VulkanPoolAllocator>> m_dedicatedManualPoolAllocators{};
    mutable gp::thread::SharedMutex m_manualPoolsMutex{};
    gp::Vector<gp::UniquePtr<VulkanPoolAllocator>> m_manualPoolAllocators{};

    mutable gp::thread::Mutex m_directAllocationsMutex{};
    gp::Vector<DirectAllocationSlot> m_directAllocations{};
    gp::Vector<gp::UInt32> m_freeDirectSlotIndices{};

    gp::UInt32 m_imagePoolAlignment{ 256u };
    gp::UInt32 m_bufferPoolAlignment{ 256u };
    bool m_isInitialized{ false };
    bool m_isEvicting{ false };
};

}   // namespace gp::rhi::vulkan
