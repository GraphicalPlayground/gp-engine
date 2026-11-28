// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"   // IWYU pragma: keep
#include "allocators/PoolAllocationData.hpp"
#include "allocators/PoolAllocatorEnums.hpp"
#include "allocators/PoolMoveRequest.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"

namespace gp::rhi::allocator
{

/// @brief Single GPU memory pool managing sub-allocations through a circular doubly-linked block list.
/// @details Every block in the list is either free or allocated. Free blocks are indexed in a sorted `gp::Vector`
///          ordered either by ascending size (best-fit) or ascending offset (first-fit). Adjacent unlocked free blocks
///          are coalesced automatically on deallocation.
class GP_RHI_API MemoryPool
{
public:
    /// @brief Sentinel pool index used for pre-allocated pools not yet assigned a slot in `PoolAllocator`.
    static constexpr gp::Int16 kInvalidPoolIndex = -1;

    /// @brief Alias for the free-list sorting policy.
    using FreeListOrder = PoolFreeListOrder;

public:
    /// @brief Constructs a memory pool descriptor without yet allocating its internal free-block node pool.
    /// @param[in] poolIndex Slot index assigned by the owning `PoolAllocator`, or `kInvalidPoolIndex` for async pools.
    /// @param[in] poolSize Total backing capacity in bytes. Must fit within 32 bits (`< UINT32_MAX`).
    /// @param[in] poolAlignment Base alignment granularity in bytes for blocks in this pool.
    /// @param[in] supportedResourceTypes Bitmask of resource types allowed to allocate from this pool.
    /// @param[in] freeListOrder Ordering policy maintained for the free-block lookup array.
    MemoryPool(
        gp::Int16 poolIndex,
        gp::UInt64 poolSize,
        gp::UInt32 poolAlignment,
        PoolResourceTypes supportedResourceTypes,
        PoolFreeListOrder freeListOrder
    ) noexcept;

    /// @brief Virtual destructor releasing all internal block tracking structures.
    virtual ~MemoryPool() noexcept;

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;

public:
    /// @brief Initializes the internal node pool and, if `m_poolIndex` is valid, creates the initial full-size free
    ///        block.
    virtual void init() noexcept;

    /// @brief Validates and releases all free blocks and pooled allocation metadata nodes.
    virtual void destroy() noexcept;

    /// @brief Seeds the circular linked list and free-block array with one full-pool free block.
    /// @note Requires `m_poolIndex != kInvalidPoolIndex`.
    void initFreeBlocks() noexcept;

    /// @brief Attempts to sub-allocate `sizeInBytes` aligned to `allocationAlignment` from this pool.
    /// @param[in] sizeInBytes Requested allocation size in bytes.
    /// @param[in] allocationAlignment Required byte alignment (`<= m_poolAlignment`).
    /// @param[in] allocationResourceType Resource category being allocated; must be supported by this pool.
    /// @param[out] allocationData Receives the initialized allocated block descriptor on success.
    /// @return True if a suitable free block was found and allocated; false otherwise.
    [[nodiscard]] bool tryAllocate(
        gp::UInt32 sizeInBytes,
        gp::UInt32 allocationAlignment,
        PoolResourceTypes allocationResourceType,
        PoolAllocationData& allocationData
    ) noexcept;

    /// @brief Returns a previously allocated, locked block back to this pool and coalesces adjacent free blocks.
    /// @param[in,out] allocationData Live allocation belonging to this pool; reset after deallocation.
    void deallocate(PoolAllocationData& allocationData) noexcept;

    /// @brief Walks allocated blocks in reverse order and attempts to relocate them into `targetPools`.
    /// @param[in] contexts Active per-pipeline backend command contexts for copy ordering.
    /// @param[in] allocator Owning `PoolAllocator` handling backend-specific defragmentation copies.
    /// @param[in] maxCopySize Maximum cumulative byte budget for this defragmentation pass.
    /// @param[in,out] copySize Running byte total of bytes scheduled for relocation.
    /// @param[in] targetPools Candidate destination pools ordered by preference.
    void tryClear(
        const ContextArray& contexts,
        PoolAllocator* allocator,
        gp::UInt32 maxCopySize,
        gp::UInt32& copySize,
        const gp::Vector<MemoryPool*>& targetPools
    ) noexcept;

    /// @brief Returns this pool's slot index in its owning `PoolAllocator`, or `kInvalidPoolIndex`.
    /// @return Signed pool index.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::Int16 getPoolIndex() const noexcept
    {
        return m_poolIndex;
    }

    /// @brief Returns total pool capacity in bytes.
    /// @return Total capacity in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt64 getPoolSize() const noexcept
    {
        return m_poolSize;
    }

    /// @brief Returns total unallocated bytes currently available in this pool.
    /// @return Free byte count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt64 getFreeSize() const noexcept
    {
        return m_freeSize;
    }

    /// @brief Returns total bytes currently consumed by allocations, including alignment padding.
    /// @return Used byte count (`m_poolSize - m_freeSize`).
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt64 getUsedSize() const noexcept
    {
        return m_poolSize - m_freeSize;
    }

    /// @brief Returns the base alignment granularity of this pool in bytes.
    /// @return Pool alignment in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getPoolAlignment() const noexcept
    {
        return m_poolAlignment;
    }

    /// @brief Returns cumulative bytes lost to alignment rounding across live allocations.
    /// @return Alignment padding waste in bytes.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getAlignmentWaste() const noexcept
    {
        return static_cast<gp::UInt32>(m_alignmentWaste);
    }

    /// @brief Returns the number of currently live allocated blocks in this pool.
    /// @return Active allocation count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getAllocatedBlocks() const noexcept
    {
        return m_allocatedBlocks;
    }

    /// @brief Returns the bitmask of resource types supported by this pool.
    /// @return Supported `PoolResourceTypes` bitmask.
    [[nodiscard]] GP_FORCEINLINE_HINT PoolResourceTypes getSupportedResourceTypes() const noexcept
    {
        return m_supportedResourceTypes;
    }

    /// @brief Checks whether this pool supports any of the resource categories in `type`.
    /// @param[in] type Resource category mask to test.
    /// @return True when `(m_supportedResourceTypes & type) != PoolResourceTypes::None`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isResourceTypeSupported(const PoolResourceTypes type) const noexcept
    {
        return (m_supportedResourceTypes & type) != PoolResourceTypes::None;
    }

    /// @brief Reports whether zero bytes are currently allocated from this pool.
    /// @return True when `getUsedSize() == 0`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isEmpty() const noexcept
    {
        return getUsedSize() == 0u;
    }

    /// @brief Reports whether this pool has zero remaining free bytes.
    /// @return True when `m_freeSize == 0`.
    [[nodiscard]] GP_FORCEINLINE_HINT bool isFull() const noexcept
    {
        return m_freeSize == 0u;
    }

    /// @brief Assigns a concrete pool slot index to an asynchronously pre-allocated pool.
    /// @param[in] poolIndex Slot index assigned by the owning `PoolAllocator`.
    GP_FORCEINLINE_HINT void setPoolIndex(const gp::Int16 poolIndex) noexcept
    {
        m_poolIndex = poolIndex;
    }

    /// @brief Computes the conservative pool-aligned byte size required to satisfy `sizeInBytes` and
    ///        `allocationAlignment`.
    /// @param[in] sizeInBytes Requested logical allocation size in bytes.
    /// @param[in] poolAlignment Pool-level alignment granularity in bytes.
    /// @param[in] allocationAlignment Requested byte alignment (`<= poolAlignment`).
    /// @return Aligned byte size rounded up to a multiple of `poolAlignment`.
    [[nodiscard]] static gp::UInt32
        getAlignedSize(gp::UInt32 sizeInBytes, gp::UInt32 poolAlignment, gp::UInt32 allocationAlignment) noexcept;

    /// @brief Adjusts a pool-aligned `offset` upward when `poolAlignment` is not a multiple of `allocationAlignment`.
    /// @param[in] offset Starting byte offset of the candidate free block.
    /// @param[in] poolAlignment Pool-level alignment granularity in bytes.
    /// @param[in] allocationAlignment Requested byte alignment.
    /// @return Byte offset aligned to `allocationAlignment`.
    [[nodiscard]] static gp::UInt32
        getAlignedOffset(gp::UInt32 offset, gp::UInt32 poolAlignment, gp::UInt32 allocationAlignment) noexcept;

protected:
    /// @brief Searches `m_freeBlocks` for a free block large enough for the aligned allocation.
    /// @param[in] sizeInBytes Requested logical size in bytes.
    /// @param[in] allocationAlignment Requested byte alignment.
    /// @return Zero-based index into `m_freeBlocks`, or `-1` if no block fits.
    [[nodiscard]] gp::Int32 findFreeBlock(gp::UInt32 sizeInBytes, gp::UInt32 allocationAlignment) const noexcept;

    /// @brief Coalesces `freeBlock` with adjacent unlocked free neighbors and inserts it into `m_freeBlocks` in sorted
    ///        order.
    /// @param[in,out] freeBlock Free block to merge and register.
    /// @return Pointer to the resulting coalesced free block stored in `m_freeBlocks`.
    PoolAllocationData* addToFreeBlocks(PoolAllocationData* freeBlock) noexcept;

    /// @brief Removes `freeBlock` from `m_freeBlocks` without shrinking the array's backing storage.
    /// @param[in] freeBlock Pointer to the free block entry to remove.
    void removeFromFreeBlocks(PoolAllocationData* freeBlock) noexcept;

    /// @brief Obtains a recycled `PoolAllocationData` node from `m_allocationDataPool`, or allocates a new one.
    /// @return Pointer to an available `PoolAllocationData` instance.
    [[nodiscard]] PoolAllocationData* getNewAllocationData() noexcept;

    /// @brief Resets `data` and returns it to `m_allocationDataPool`, or deletes it when the pool is full.
    /// @param[in] data Node to recycle or delete.
    void releaseAllocationData(PoolAllocationData* data) noexcept;

    /// @brief Audits linked-list integrity, alignment invariants, and free-list accounting when validation is enabled.
    void validate() noexcept;

protected:
    gp::Int16 m_poolIndex{ kInvalidPoolIndex };
    const gp::UInt64 m_poolSize{ 0u };
    const gp::UInt32 m_poolAlignment{ 0u };
    const PoolResourceTypes m_supportedResourceTypes{ PoolResourceTypes::None };
    const PoolFreeListOrder m_freeListOrder{ PoolFreeListOrder::SortBySize };

    gp::UInt64 m_freeSize{ 0u };
    gp::UInt64 m_alignmentWaste{ 0u };
    gp::UInt32 m_allocatedBlocks{ 0u };

    /// @brief Circular sentinel node anchoring the start and end of this pool's block list.
    PoolAllocationData m_headBlock{};

    /// @brief Sorted lookup table of currently available free blocks.
    gp::Vector<PoolAllocationData*> m_freeBlocks{};

    /// @brief Recycled node pool reducing heap allocation overhead for `PoolAllocationData` instances.
    gp::Vector<PoolAllocationData*> m_allocationDataPool{};
};

}   // namespace gp::rhi::allocator
