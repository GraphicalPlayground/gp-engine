// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/MemoryPool.hpp"
#include "allocators/PoolAllocationData.hpp"
#include "allocators/PoolAllocator.hpp"
#include "allocators/PoolAllocatorEnums.hpp"
#include "allocators/PoolResource.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/VulkanAllocation.hpp"
#include "memory/VulkanMemoryEnums.hpp"
#include "RHIVulkan.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Returns a new process-unique identifier for a `VkBuffer` handle.
/// @details Shared by pool-owned buffers and buffers that own their handle (placed and sparse), so descriptor caches
///          can key on the identifier instead of the handle value, which drivers may recycle.
/// @note Thread-safe.
/// @return Non-zero identifier.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt32 generateVulkanBufferHandleId() noexcept;

/// @brief Configuration identifying a single `VulkanPoolAllocator` instance and its backing page properties.
struct VulkanPoolConfig final
{
    /// @brief Resource categories served by this pool allocator.
    gp::rhi::allocator::PoolResourceTypes resourceTypes{ gp::rhi::allocator::PoolResourceTypes::Buffers };

    /// @brief Suballocation strategy (`PlacedResource` or `ManualSubAllocation`).
    gp::rhi::allocator::ResourceAllocationStrategy strategy{
        gp::rhi::allocator::ResourceAllocationStrategy::PlacedResource
    };

    /// @brief Physical device memory type index (used by `PlacedResource` pools).
    gp::UInt32 memoryTypeIndex{ 0u };

    /// @brief Shared `VkBuffer` usage flags (used by `ManualSubAllocation` pools).
    VkBufferUsageFlags bufferUsageFlags{ 0u };

    /// @brief Desired `VkMemoryPropertyFlags` (used by `ManualSubAllocation` pools).
    VkMemoryPropertyFlags memoryPropertyFlags{ 0u };

    /// @brief Default backing page size in bytes.
    gp::UInt64 poolSize{ 0u };

    /// @brief Maximum single allocation size routed to this pool allocator.
    gp::UInt32 maxAllocationSize{ 0u };

    /// @brief Base alignment granularity in bytes for blocks in this pool.
    gp::UInt32 poolAlignment{ 0u };

    /// @brief True when backed by device-local memory (eligible for defragmentation).
    bool isDeviceLocal{ false };

    /// @brief Classifies this config as either `VulkanResourceType::Image` or `VulkanResourceType::Buffer`.
    /// @return `VulkanResourceType`.
    [[nodiscard]] constexpr VulkanResourceType getResourceType() const noexcept
    {
        const gp::rhi::allocator::PoolResourceTypes imageMask = gp::rhi::allocator::PoolResourceTypes::RTDSTextures |
                                                                gp::rhi::allocator::PoolResourceTypes::NonRTDSTextures;
        return ((resourceTypes & imageMask) != gp::rhi::allocator::PoolResourceTypes::None)
                   ? VulkanResourceType::Image
                   : VulkanResourceType::Buffer;
    }
};

/// @brief Single GPU memory page backed by a `VulkanDeviceMemoryAllocation` (and an optional shared `VkBuffer`).
/// @details `ManualSubAllocation` pools create one `VkBuffer` spanning `m_poolSize` and hand out byte ranges inside
///          it. `PlacedResource` pools hand out byte offsets inside `m_memoryAllocation` for placed buffers or images.
class GP_RHI_VULKAN_API VulkanMemoryPool final : public gp::rhi::allocator::MemoryPool
{
public:
    /// @brief Sentinel frame index indicating the pool still has live allocations.
    static constexpr gp::UInt32 kActiveFrameSentinel = 0xFFFFFFFFu;

    /// @brief Constructs a Vulkan memory pool page for `config`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] deviceMemoryManager Low-level device memory manager backing this page.
    /// @param[in] config Pool configuration descriptor.
    /// @param[in] poolIndex Slot index in the owning `VulkanPoolAllocator`.
    /// @param[in] poolSize Total backing capacity in bytes.
    /// @param[in] poolAlignment Base alignment granularity in bytes.
    /// @param[in] freeListOrder Free-list sorting policy.
    VulkanMemoryPool(
        VulkanDevice& device,
        VulkanDeviceMemoryManager& deviceMemoryManager,
        const VulkanPoolConfig& config,
        gp::Int16 poolIndex,
        gp::UInt64 poolSize,
        gp::UInt32 poolAlignment,
        gp::rhi::allocator::PoolFreeListOrder freeListOrder
    ) noexcept;

    /// @brief Destructor. Releases native Vulkan buffer and device memory handles.
    virtual ~VulkanMemoryPool() noexcept override;

    /// @copydoc gp::rhi::allocator::MemoryPool::init()
    virtual void init() noexcept override;

    /// @copydoc gp::rhi::allocator::MemoryPool::destroy()
    virtual void destroy() noexcept override;

    /// @brief Reports whether backing `VkDeviceMemory` was successfully allocated.
    /// @return True when `m_memoryAllocation != nullptr`.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_memoryAllocation != nullptr;
    }

    /// @brief Returns the backing `VulkanDeviceMemoryAllocation`, or `nullptr`.
    /// @return Backing device memory allocation.
    [[nodiscard]] VulkanDeviceMemoryAllocation* getMemoryAllocation() const noexcept
    {
        return m_memoryAllocation;
    }

    /// @brief Returns the shared `VkBuffer` handle (or `VK_NULL_HANDLE` for `PlacedResource` pools).
    /// @return Native `VkBuffer`.
    [[nodiscard]] VkBuffer getBufferHandle() const noexcept
    {
        return m_buffer;
    }

    /// @brief Returns the monotonic buffer identifier assigned to this page.
    /// @return Buffer identifier.
    [[nodiscard]] gp::UInt32 getBufferId() const noexcept
    {
        return m_bufferId;
    }

    /// @brief Returns the resolved physical device memory type index backing this page.
    /// @return Memory type index.
    [[nodiscard]] gp::UInt32 getMemoryTypeIndex() const noexcept
    {
        return m_memoryTypeIndex;
    }

    /// @brief Returns the persistent CPU mapping pointer for host-visible pages, or `nullptr`.
    /// @return Mapped host pointer or `nullptr`.
    [[nodiscard]] void* getMappedPointer() const noexcept;

    /// @brief Flushes host CPU writes over `[offset, offset + size)` on this page.
    /// @param[in] offset Starting byte offset.
    /// @param[in] size Range length in bytes.
    void flushMappedMemory(VkDeviceSize offset, VkDeviceSize size) const noexcept;

    /// @brief Invalidates host CPU caches over `[offset, offset + size)` on this page.
    /// @param[in] offset Starting byte offset.
    /// @param[in] size Range length in bytes.
    void invalidateMappedMemory(VkDeviceSize offset, VkDeviceSize size) const noexcept;

    /// @brief Returns the frame index when this pool became empty, or `kActiveFrameSentinel` while active.
    /// @return Frame index when emptied.
    [[nodiscard]] gp::UInt32 getFrameEmptied() const noexcept
    {
        return m_frameEmptied;
    }

    /// @brief Records the frame index when this pool became empty (or `kActiveFrameSentinel` when re-used).
    /// @param[in] frameEmptied Frame index.
    void setFrameEmptied(const gp::UInt32 frameEmptied) noexcept
    {
        m_frameEmptied = frameEmptied;
    }

private:
    /// @brief Destroys the shared `VkBuffer` and frees `m_memoryAllocation` without touching base block lists.
    void releaseNativeResources() noexcept;

    VulkanDevice& m_device;
    VulkanDeviceMemoryManager& m_deviceMemoryManager;
    const VulkanPoolConfig m_config;

    VulkanDeviceMemoryAllocation* m_memoryAllocation{ nullptr };
    VkBuffer m_buffer{ VK_NULL_HANDLE };
    gp::UInt32 m_bufferId{ 0u };
    gp::UInt32 m_memoryTypeIndex{ 0u };
    gp::UInt32 m_frameEmptied{ kActiveFrameSentinel };
};

/// @brief Multi-page Vulkan pool suballocator backed by `gp::rhi::allocator::PoolAllocator`.
/// @details One instance exists per `VulkanPoolConfig`; its slot index in `VulkanMemoryManager` is stored in
///          `VulkanAllocation::allocatorIndex`.
class GP_RHI_VULKAN_API VulkanPoolAllocator final : public gp::rhi::allocator::PoolAllocator
{
public:
    /// @brief Constructs and initializes a `VulkanPoolAllocator` for `config`.
    /// @param[in] device Active `VulkanDevice`.
    /// @param[in] deviceMemoryManager Low-level device memory manager.
    /// @param[in] config Pool configuration descriptor.
    /// @param[in] allocatorIndex Unique slot index assigned by `VulkanMemoryManager`.
    VulkanPoolAllocator(
        VulkanDevice& device,
        VulkanDeviceMemoryManager& deviceMemoryManager,
        const VulkanPoolConfig& config,
        gp::UInt32 allocatorIndex
    ) noexcept;

    /// @brief Destructor. Frees all block metadata and destroys all backing `VulkanMemoryPool` pages.
    virtual ~VulkanPoolAllocator() noexcept override;

    /// @brief Reports whether this pool allocator can serve requests matching `config`.
    /// @param[in] config Requested pool configuration.
    /// @return True when strategy, resource type, and usage/memory requirements match.
    [[nodiscard]] bool supportsAllocation(const VulkanPoolConfig& config) const noexcept;

    /// @brief Attempts to sub-allocate `size` bytes aligned to `alignment` from this pool allocator.
    /// @param[out] outAllocation Populated `VulkanAllocation` handle on success.
    /// @param[in] size Logical allocation size in bytes.
    /// @param[in] alignment Required byte alignment (`<= m_poolAlignment`).
    /// @param[in] allocType Backing allocation category.
    /// @param[in] metaType Diagnostic usage category.
    /// @return True when sub-allocation succeeded; false otherwise.
    [[nodiscard]] bool tryAllocate(
        VulkanAllocation& outAllocation,
        gp::UInt32 size,
        gp::UInt32 alignment,
        VulkanAllocationType allocType,
        VulkanAllocationMetaType metaType
    ) noexcept;

    /// @brief Attaches an opaque defragmentation resource owner to a pooled allocation.
    /// @param[in] allocation Live allocation served by this allocator.
    /// @param[in] owner Non-null `PoolResource` owner.
    void setAllocationOwner(const VulkanAllocation& allocation, gp::rhi::allocator::PoolResource* owner) noexcept;

    /// @brief Reassigns defragmentation ownership of a pooled allocation to `newOwner`.
    /// @param[in] allocation Live allocation served by this allocator.
    /// @param[in] newOwner New non-null `PoolResource` owner.
    void transferAllocationOwner(
        const VulkanAllocation& allocation, gp::rhi::allocator::PoolResource* newOwner
    ) noexcept;

    /// @brief Clears the defragmentation owner of a pooled allocation, pinning it in place.
    /// @param[in] allocation Live allocation served by this allocator.
    void clearAllocationOwner(const VulkanAllocation& allocation) noexcept;

    /// @brief Releases a single pooled allocation back to its `VulkanMemoryPool`.
    /// @note Callers must disown or reset `allocation` after freeing; `VulkanMemoryManager::freePoolAllocations()`
    ///       handles this automatically.
    /// @param[in] allocation Allocation to free.
    void free(VulkanAllocation& allocation) noexcept;

    /// @brief Releases a batch of allocations belonging to this allocator under a single lock acquisition.
    /// @note Callers must disown or reset each entry after freeing.
    /// @param[in] allocations View of allocations to free.
    void freeBatch(gp::VectorView<VulkanAllocation> allocations) noexcept;

    /// @brief Reclaims empty `VulkanMemoryPool` pages older than `frameLag` frames (or all empty pages if `forceFree`).
    /// @param[in] frameLag Minimum number of frames an empty pool must age before release.
    /// @param[in] forceFree When true, frees empty or invalid pools immediately.
    /// @param[in] currentFrame Current deletion frame number.
    void releaseEmptyPools(gp::UInt32 frameLag, bool forceFree, gp::UInt32 currentFrame) noexcept;

    /// @brief Reports whether this pool allocator serves images rather than buffers.
    /// @return True for image pools.
    [[nodiscard]] bool isImagePool() const noexcept
    {
        return m_config.getResourceType() == VulkanResourceType::Image;
    }

    /// @brief Returns the unique allocator slot index assigned by `VulkanMemoryManager`.
    /// @return Allocator index.
    [[nodiscard]] gp::UInt32 getAllocatorIndex() const noexcept
    {
        return m_allocatorIndex;
    }

    /// @brief Returns the effective physical device memory type index used by this allocator's pages, or `UINT32_MAX`.
    /// @return Resolved memory type index, or `0xFFFFFFFFu` if no valid page exists yet.
    [[nodiscard]] gp::UInt32 getEffectiveMemoryTypeIndex() noexcept;

    /// @brief Resolves the `VulkanMemoryPool` backing `allocation`.
    /// @param[in] allocation Live allocation served by this allocator.
    /// @return Pointer to the backing `VulkanMemoryPool`, or `nullptr` if invalid.
    [[nodiscard]] VulkanMemoryPool* getPool(const VulkanAllocation& allocation) noexcept;

    /// @brief Resolves the `VulkanMemoryPool` backing `block`.
    /// @param[in] block Live `PoolAllocationData` block.
    /// @return Pointer to the backing `VulkanMemoryPool`, or `nullptr` if invalid.
    [[nodiscard]] VulkanMemoryPool* getPoolForBlock(const gp::rhi::allocator::PoolAllocationData& block) noexcept;

    /// @brief Accumulates live pool and allocation metrics into `outStats`.
    /// @param[in,out] outStats Destination stats accumulator.
    void getMemoryDump(VulkanResourceHeapStats& outStats) noexcept;

    /// @brief Returns the configuration descriptor governing this pool allocator.
    /// @return Const reference to `VulkanPoolConfig`.
    [[nodiscard]] const VulkanPoolConfig& getConfig() const noexcept
    {
        return m_config;
    }

    /// @brief Returns the number of live allocated blocks currently tracked by this allocator.
    /// @return Live block count.
    [[nodiscard]] gp::USize getLiveBlockCount() const noexcept;

protected:
    /// @copydoc gp::rhi::allocator::PoolAllocator::createNewPool()
    [[nodiscard]] virtual gp::rhi::allocator::MemoryPool* createNewPool(
        gp::Int16 poolIndex,
        gp::UInt32 minimumAllocationSize,
        gp::rhi::allocator::PoolResourceTypes allocationResourceType
    ) noexcept override;

    /// @copydoc gp::rhi::allocator::PoolAllocator::handleDefragRequest()
    [[nodiscard]] virtual bool handleDefragRequest(
        const gp::rhi::allocator::ContextArray& contexts,
        gp::rhi::allocator::PoolAllocationData* sourceBlock,
        gp::rhi::allocator::PoolAllocationData& tmpTargetBlock
    ) noexcept override;

private:
    /// @brief Slot entry storing an active `PoolAllocationData*` (or `nullptr` when free-listed).
    struct BlockSlot final
    {
        gp::rhi::allocator::PoolAllocationData* data{ nullptr };
    };

    VulkanDevice& m_device;
    VulkanDeviceMemoryManager& m_deviceMemoryManager;
    const VulkanPoolConfig m_config;
    const gp::UInt32 m_allocatorIndex{ 0u };

    mutable gp::thread::Mutex m_blocksMutex{};
    gp::Vector<BlockSlot> m_blocks{};
    gp::Vector<gp::UInt32> m_freeBlockIndices{};
    gp::USize m_liveBlockCount{ 0u };

    gp::thread::Atomic<bool> m_hasEmptyPools{ false };
};

}   // namespace gp::rhi::vulkan
