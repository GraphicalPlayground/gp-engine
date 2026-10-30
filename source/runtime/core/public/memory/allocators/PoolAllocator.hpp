// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/AllocatorTypes.hpp"
#include "memory/allocators/IAllocator.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/MemoryForward.hpp"

namespace gp::memory
{

/// @brief Fixed-size block pool allocator using an O(1) intrusive singly-linked free list.
/// @details Pre-partitions a contiguous memory slab into `m_blockCount` uniform blocks of size `m_stride` aligned to
///          `m_blockAlignment`. Unused blocks store their next-pointer directly inside the unallocated payload
///          (`sizeof(FreeBlock)`), yielding zero metadata overhead per block and zero external fragmentation for
///          fixed-size node/object allocations (list nodes, tree nodes, handles, physics contacts).
/// @note Not thread-safe by default; synchronize external access if shared across worker threads.
class GP_CORE_API PoolAllocator final : public IAllocator
{
private:
    /// @brief Intrusive free-list node embedded directly inside unallocated pool blocks.
    struct FreeBlock
    {
        FreeBlock* next{ nullptr };
    };

private:
    gp::UInt8* m_rawBuffer{ nullptr };
    gp::UInt8* m_alignedStart{ nullptr };
    FreeBlock* m_freeListHead{ nullptr };
    Malloc* m_backingMalloc{ nullptr };
    const char* m_poolName{ "PoolAllocator" };
    gp::USize m_rawCapacity{ 0u };
    gp::USize m_stride{ 0u };
    gp::USize m_blockCount{ 0u };
    gp::USize m_usedBlockCount{ 0u };
    gp::UInt32 m_blockAlignment{ kMinimumAlignment };
    AllocatorStats m_stats{};
    bool m_ownsBuffer{ false };

public:
    /// @brief Constructs an uninitialized pool allocator with zero capacity.
    constexpr PoolAllocator() noexcept = default;

    /// @brief Constructs a pool allocator that allocates its contiguous backing slab from a `Malloc` backend.
    /// @param[in] blockSize Size in bytes of each individual block (automatically rounded up to `>= sizeof(void*)` and
    /// aligned).
    /// @param[in] blockCount Total number of fixed-size blocks in the pool.
    /// @param[in] blockAlignment Alignment in bytes of each block (must be a power of two; clamped to `>=
    /// kMinimumAlignment`).
    /// @param[in] backingMalloc Optional `Malloc` backend used to allocate the slab (`nullptr` uses `GlobalMemory`).
    /// @param[in] poolName Optional static string name for Tracy profiler telemetry (`GP_MEM_ALLOC_N` / `GP_PLOT`).
    PoolAllocator(
        const gp::USize blockSize,
        const gp::USize blockCount,
        const gp::UInt32 blockAlignment = kDefaultAlignment,
        Malloc* backingMalloc = nullptr,
        const char* poolName = "PoolAllocator"
    ) noexcept;

    /// @brief Constructs a pool allocator operating over a caller-owned external memory buffer (zero heap allocs).
    /// @param[in] externalBuffer Pointer to a caller-managed contiguous buffer.
    /// @param[in] capacityInBytes Total size in bytes of `externalBuffer`.
    /// @param[in] blockSize Size in bytes of each individual block.
    /// @param[in] blockAlignment Alignment in bytes of each block (must be a power of two; clamped to `>=
    /// kMinimumAlignment`).
    /// @param[in] poolName Optional static string name for Tracy profiler plot telemetry (`GP_PLOT`).
    PoolAllocator(
        void* externalBuffer,
        const gp::USize capacityInBytes,
        const gp::USize blockSize,
        const gp::UInt32 blockAlignment = kDefaultAlignment,
        const char* poolName = "PoolAllocator.External"
    ) noexcept;

    /// @brief Copying a pool allocator is disabled because it uniquely manages its intrusive free list.
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    /// @brief Move constructor; transfers slab ownership and free-list state from `other`.
    /// @param[in,out] other Source pool allocator, left in an empty state.
    PoolAllocator(PoolAllocator&& other) noexcept;

    /// @brief Move assignment operator; releases any owned slab and steals state from `other`.
    /// @param[in,out] other Source pool allocator, left in an empty state.
    /// @return Reference to `*this`.
    PoolAllocator& operator=(PoolAllocator&& other) noexcept;

    /// @brief Destructor; releases the backing slab to `m_backingMalloc` if owned by this instance.
    virtual ~PoolAllocator() noexcept override;

    /// @brief Initializes or re-initializes the pool with a newly allocated slab from `backingMalloc`.
    /// @param[in] blockSize Size in bytes of each block.
    /// @param[in] blockCount Number of blocks to allocate.
    /// @param[in] blockAlignment Alignment in bytes of each block (power of two).
    /// @param[in] backingMalloc Optional `Malloc` backend (`nullptr` uses `GlobalMemory`).
    /// @param[in] poolName Optional static string name for profiler tracking.
    /// @return True if the backing slab was successfully allocated and partitioned; false otherwise.
    bool initialize(
        const gp::USize blockSize,
        const gp::USize blockCount,
        const gp::UInt32 blockAlignment = kDefaultAlignment,
        Malloc* backingMalloc = nullptr,
        const char* poolName = "PoolAllocator"
    ) noexcept;

    /// @brief Initializes or re-initializes the pool over an external caller-owned buffer.
    /// @param[in] externalBuffer Pointer to the external memory block.
    /// @param[in] capacityInBytes Size in bytes of `externalBuffer`.
    /// @param[in] blockSize Size in bytes of each block.
    /// @param[in] blockAlignment Alignment in bytes of each block (power of two).
    /// @param[in] poolName Optional static string name for profiler tracking.
    /// @return True if `externalBuffer` has sufficient aligned space for at least one block; false otherwise.
    bool initialize(
        void* externalBuffer,
        const gp::USize capacityInBytes,
        const gp::USize blockSize,
        const gp::UInt32 blockAlignment = kDefaultAlignment,
        const char* poolName = "PoolAllocator.External"
    ) noexcept;

    /// @brief Releases the backing slab (if owned) and resets all pool state to empty.
    void shutdown() noexcept;

    /// @copydoc IAllocator::allocate()
    /// @note Pops one block from the intrusive free list in O(1) time. Returns `nullptr` if `sizeInBytes >
    /// getBlockSize()`,
    ///       `alignment > getBlockAlignment()`, or the pool is exhausted.
    [[nodiscard]] virtual void*
        allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept override;

    /// @copydoc IAllocator::reallocate()
    /// @note Succeeds in place in O(1) time when `newSizeInBytes <= getBlockSize()` and `alignment <=
    /// getBlockAlignment()`.
    ///       Returns `nullptr` if `newSizeInBytes > getBlockSize()`.
    [[nodiscard]] virtual void* reallocate(
        void* ptr,
        const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept override;

    /// @copydoc IAllocator::deallocate()
    /// @note Pushes `ptr` back onto the head of the intrusive free list in O(1) time.
    virtual void deallocate(
        void* ptr, const gp::USize oldSizeInBytes = 0u, const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept override;

    /// @copydoc IAllocator::getActualAllocationSize()
    [[nodiscard]] virtual gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept override;

    /// @copydoc IAllocator::owns()
    [[nodiscard]] virtual bool owns(const void* ptr) const noexcept override;

    /// @copydoc IAllocator::getName()
    [[nodiscard]] virtual const char* getName() const noexcept override;

    /// @brief Re-links all blocks into the intrusive free list in sequential address order, reclaiming all allocations.
    /// @note Preserves `m_stats.peakAllocatedBytes` for high-water mark diagnostics.
    void reset() noexcept;

    /// @brief Retrieves the aligned stride in bytes of each block in the pool.
    /// @return Block stride in bytes.
    [[nodiscard]] constexpr gp::USize getBlockSize() const noexcept
    {
        return m_stride;
    }

    /// @brief Retrieves the alignment in bytes guaranteed for every block in the pool.
    /// @return Block alignment in bytes.
    [[nodiscard]] constexpr gp::UInt32 getBlockAlignment() const noexcept
    {
        return m_blockAlignment;
    }

    /// @brief Retrieves the total number of blocks managed by the pool.
    /// @return Total block count.
    [[nodiscard]] constexpr gp::USize getBlockCount() const noexcept
    {
        return m_blockCount;
    }

    /// @brief Retrieves the number of currently allocated blocks.
    /// @return Used block count.
    [[nodiscard]] constexpr gp::USize getUsedBlockCount() const noexcept
    {
        return m_usedBlockCount;
    }

    /// @brief Retrieves the number of available free blocks remaining in the pool.
    /// @return Free block count.
    [[nodiscard]] constexpr gp::USize getFreeBlockCount() const noexcept
    {
        return m_blockCount >= m_usedBlockCount ? (m_blockCount - m_usedBlockCount) : 0u;
    }

    /// @brief Retrieves the total usable capacity of the pool in bytes (`getBlockSize() * getBlockCount()`).
    /// @return Usable capacity in bytes.
    [[nodiscard]] constexpr gp::USize getCapacity() const noexcept
    {
        return m_stride * m_blockCount;
    }

    /// @brief Retrieves the current telemetry statistics for this pool.
    /// @return Const reference to `AllocatorStats`.
    [[nodiscard]] constexpr const AllocatorStats& getStats() const noexcept
    {
        return m_stats;
    }

    /// @brief Checks whether the pool holds a valid backing slab with at least one block.
    /// @return True if initialized and valid; false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_alignedStart != nullptr && m_blockCount > 0u && m_stride > 0u;
    }

private:
    /// @brief Rebuilds the singly-linked free list across `[0, m_blockCount)` in sequential address order.
    void rebuildFreeList() noexcept;
};

static_assert(gp::concepts::IsAllocator<PoolAllocator>, "PoolAllocator must satisfy gp::concepts::IsAllocator.");

}   // namespace gp::memory
