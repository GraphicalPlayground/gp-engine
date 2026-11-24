// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"   // IWYU pragma: keep
#include "allocators/PoolAllocatorEnums.hpp"
#include "allocators/PoolMoveRequest.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIBase.hpp"
#include "threading/atomics/Atomic.hpp"
#include "threading/sync/RecursiveMutex.hpp"

namespace gp::rhi::allocator
{

/// @brief Multi-pool sub-allocator managing an array of `MemoryPool` instances with optional defragmentation and
///        asynchronous pool pre-allocation.
/// @details Backend implementations derive from `PoolAllocator` and supply concrete pool creation (`createNewPool()`)
///          and GPU relocation (`handleDefragRequest()`). Pool lookup order is maintained in a separate index array
///          (`m_poolAllocationOrder`) so each `PoolAllocationData` keeps a stable `m_poolIndex`.
class GP_RHI_API PoolAllocator
{
public:
    /// @brief Constructs a pool allocator with fixed pool sizing, alignment, and defragmentation policy.
    /// @param[in] defaultPoolSize Default capacity in bytes for newly created pools.
    /// @param[in] poolAlignment Base alignment granularity in bytes for pools created by this allocator.
    /// @param[in] maxAllocationSize Upper bound in bytes on allocations routed to this allocator.
    /// @param[in] freeListOrder Ordering policy applied to each pool's free-block array.
    /// @param[in] defragEnabled True to enable cross-pool defragmentation and fullest-first pool sorting.
    /// @param[in] poolAllocationStrategy Whether overflow pools are created synchronously or asynchronously.
    /// @param[in] overflowPoolsCount Target number of pre-allocated overflow pools maintained in async mode.
    PoolAllocator(
        gp::UInt64 defaultPoolSize,
        gp::UInt32 poolAlignment,
        gp::UInt32 maxAllocationSize,
        PoolFreeListOrder freeListOrder,
        bool defragEnabled,
        PoolAllocationStrategy poolAllocationStrategy,
        gp::UInt16 overflowPoolsCount
    ) noexcept;

    /// @brief Virtual destructor shutting down any in-flight async pool creation and releasing all pools.
    virtual ~PoolAllocator() noexcept;

    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;
    PoolAllocator(PoolAllocator&&) = delete;
    PoolAllocator& operator=(PoolAllocator&&) = delete;

public:
    /// @brief Initializes the allocator, creating the initial pool and priming background overflow pools in async mode.
    /// @param[in] allocationResourceType Resource category mask used when creating initial and async overflow pools.
    void initialize(PoolResourceTypes allocationResourceType) noexcept;

    /// @brief Waits for any in-flight async pool creation to settle, then destroys and deletes all active and overflow
    ///        pools.
    void destroy() noexcept;

    /// @brief Defragments under-utilized pools by migrating unlocked allocations into fuller pools within a byte
    ///        budget.
    /// @param[in] contexts Active per-pipeline backend command contexts for copy ordering.
    /// @param[in] maxCopySize Maximum cumulative bytes to relocate during this call.
    /// @param[in,out] currentCopySize Running count of relocated bytes updated as moves succeed.
    void defrag(const ContextArray& contexts, gp::UInt32 maxCopySize, gp::UInt32& currentCopySize) noexcept;

    /// @brief Accumulates memory utilization metrics across all live pools into the caller's running counters.
    /// @param[in,out] ioMemoryAllocated Running total of allocated backing pool bytes.
    /// @param[in,out] ioMemoryUsed Running total of logical used bytes (excluding alignment waste).
    /// @param[in,out] ioMemoryFree Running total of free bytes across all pools.
    /// @param[in,out] ioMemoryEndFree Running sum updated with the largest free size found in any single pool.
    /// @param[in,out] ioAlignmentWaste Running total of bytes lost to alignment rounding.
    /// @param[in,out] ioAllocatedPageCount Running count of allocated pools.
    /// @param[in,out] ioFullPageCount Running count of completely full pools.
    void updateMemoryStats(
        gp::UInt32& ioMemoryAllocated,
        gp::UInt32& ioMemoryUsed,
        gp::UInt32& ioMemoryFree,
        gp::UInt32& ioMemoryEndFree,
        gp::UInt32& ioAlignmentWaste,
        gp::UInt32& ioAllocatedPageCount,
        gp::UInt32& ioFullPageCount
    ) noexcept;

public:
    /// @brief Returns the configured pool usage fraction above which defragmentation skips a pool.
    /// @return Usage fraction in `[0.0f, 1.0f]` (default `0.9f`).
    [[nodiscard]] static gp::Float32 getDefragSizeFraction() noexcept;

    /// @brief Sets the pool usage fraction threshold used by `defrag()`.
    /// @param[in] fraction New usage fraction threshold.
    static void setDefragSizeFraction(gp::Float32 fraction) noexcept;

    /// @brief Returns the maximum number of pools cleared per `defrag()` call (negative disables time slicing).
    /// @return Configured pool limit (default `1`).
    [[nodiscard]] static gp::Int32 getDefragMaxPoolsToClear() noexcept;

    /// @brief Sets the maximum number of pools cleared per `defrag()` call.
    /// @param[in] maxPoolsToClear Positive limit for time slicing, or negative to inspect all pools.
    static void setDefragMaxPoolsToClear(gp::Int32 maxPoolsToClear) noexcept;

    /// @brief Reports whether expensive per-operation linked-list validation is enabled for `MemoryPool`.
    /// @return True when linked-list validation is active.
    [[nodiscard]] static bool isLinkedListValidationEnabled() noexcept;

    /// @brief Enables or disables per-operation linked-list validation in `MemoryPool::validate()`.
    /// @param[in] enabled True to enable validation, false to disable.
    static void setLinkedListValidationEnabled(bool enabled) noexcept;

protected:
    /// @brief Allocates a block from an existing pool, an async overflow pool, or a newly created pool.
    /// @param[in] sizeInBytes Requested logical allocation size in bytes.
    /// @param[in] allocationAlignment Required byte alignment.
    /// @param[in] allocationResourceType Resource category mask for the allocation.
    /// @param[out] allocationData Receives the initialized block descriptor on success.
    /// @return True if the allocation succeeded; false otherwise.
    [[nodiscard]] bool tryAllocateInternal(
        gp::UInt32 sizeInBytes,
        gp::UInt32 allocationAlignment,
        PoolResourceTypes allocationResourceType,
        PoolAllocationData& allocationData
    ) noexcept;

    /// @brief Returns a live allocation to its owning pool under the allocator lock.
    /// @param[in,out] allocationData Allocation descriptor to free.
    void deallocateInternal(PoolAllocationData& allocationData) noexcept;

    /// @brief Reorders `m_poolAllocationOrder` without moving pool pointers inside `m_pools`.
    /// @details When defragmentation is enabled, orders fullest pools first to keep sparse pools easy to empty.
    ///          Otherwise orders smaller pools first with stable pool-index tie breaking.
    void sortPools() noexcept;

    /// @brief Background entry point that allocates an unassigned overflow pool and registers it under `m_mutex`.
    /// @param[in] sizeInBytes Target minimum pool size in bytes.
    /// @param[in] allocationAlignment Target alignment in bytes.
    /// @param[in] allocationResourceType Resource category mask supported by the new pool.
    void poolAsyncAllocate(
        gp::UInt32 sizeInBytes, gp::UInt32 allocationAlignment, PoolResourceTypes allocationResourceType
    ) noexcept;

    /// @brief Creates and initializes a backend-specific `MemoryPool`.
    /// @param[in] poolIndex Slot index in `m_pools`, or `MemoryPool::kInvalidPoolIndex` for async overflow pools.
    /// @param[in] minimumAllocationSize Minimum byte capacity required for the new pool.
    /// @param[in] allocationResourceType Resource category mask supported by the pool.
    /// @return Newly allocated and initialized `MemoryPool` pointer.
    [[nodiscard]] virtual MemoryPool* createNewPool(
        gp::Int16 poolIndex, gp::UInt32 minimumAllocationSize, PoolResourceTypes allocationResourceType
    ) noexcept = 0;

    friend class MemoryPool;

    /// @brief Executes a backend-specific relocation from `sourceBlock` into `tmpTargetBlock`.
    /// @param[in] contexts Active per-pipeline command contexts.
    /// @param[in] sourceBlock Existing allocated block selected for relocation.
    /// @param[in,out] tmpTargetBlock Pre-allocated destination block in a target pool.
    /// @return True if the backend scheduled the move and consumed `tmpTargetBlock`; false if the move was skipped.
    [[nodiscard]] virtual bool handleDefragRequest(
        const ContextArray& contexts, PoolAllocationData* sourceBlock, PoolAllocationData& tmpTargetBlock
    ) noexcept = 0;

private:
    /// @brief Dispatches `poolAsyncAllocate()` onto the active job system or a background OS thread.
    /// @param[in] sizeInBytes Target pool size in bytes.
    /// @param[in] allocationAlignment Target alignment in bytes.
    /// @param[in] allocationResourceType Resource category mask for the overflow pool.
    void scheduleAsyncPoolAllocation(
        gp::UInt32 sizeInBytes, gp::UInt32 allocationAlignment, PoolResourceTypes allocationResourceType
    ) noexcept;

protected:
    const gp::UInt64 m_defaultPoolSize{ 0u };
    const gp::UInt32 m_poolAlignment{ 0u };
    const gp::UInt64 m_maxAllocationSize{ 0u };
    const PoolFreeListOrder m_freeListOrder{ PoolFreeListOrder::SortBySize };
    const bool m_defragEnabled{ false };

    /// @brief Re-entrant lock serializing pool creation, allocation, deallocation, defragmentation, and stats queries.
    gp::thread::RecursiveMutex m_mutex{};

    /// @brief Active pools indexed stably by `PoolAllocationData::getPoolIndex()`.
    gp::Vector<MemoryPool*> m_pools{};

    /// @brief Cursor tracking time-sliced defragmentation progress across `m_pools`.
    gp::Int32 m_lastDefragPoolIndex{ 0 };

    /// @brief Indirection order used when scanning `m_pools` for free space during allocation.
    gp::Vector<gp::UInt32> m_poolAllocationOrder{};

    /// @brief Total live allocated block count captured by the last `updateMemoryStats()` call.
    gp::UInt32 m_totalAllocatedBlocks{ 0u };

    /// @brief Pre-allocated overflow pools ready to be promoted into `m_pools` in async mode.
    gp::Vector<MemoryPool*> m_overflowPools{};

    /// @brief Pool creation strategy (`Immediate` or `Async`).
    PoolAllocationStrategy m_poolAllocationStrategy{ PoolAllocationStrategy::Immediate };

    /// @brief Current state of background overflow pool allocation.
    PoolAllocationStatus m_poolAllocationStatus{ PoolAllocationStatus::Idle };

    /// @brief Desired number of pre-allocated overflow pools to keep ready in async mode.
    gp::Int16 m_overflowPoolsCount{ 1 };

private:
    /// @brief Tracks whether a background async allocation task is currently executing.
    gp::thread::Atomic<gp::UInt32> m_inFlightAsyncTasks{ 0u };
};

}   // namespace gp::rhi::allocator
