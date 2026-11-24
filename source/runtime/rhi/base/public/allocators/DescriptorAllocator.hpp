// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "allocators/AllocatorForward.hpp"   // IWYU pragma: keep
#include "allocators/DescriptorAllocatorTypes.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "miscellaneous/BuildDefines.hpp"
#include "RHIBase.hpp"
#include "templates/Optional.hpp"
#include "threading/sync/RecursiveMutex.hpp"
#include "views/DescriptorHandle.hpp"
#include "views/ViewEnums.hpp"

namespace gp::rhi::allocator
{

/// @brief Thread-safe free-list allocator managing contiguous ranges of descriptor indices.
/// @details Maintains a sorted vector of inclusive free ranges (`m_ranges`) and coalesces adjacent freed spans via
///          binary search. Supports growing capacity in place (`resizeGrowAndAllocate()`), querying the tightest
///          allocated span (`getAllocatedRange()`), optional atomic stat counters, and debug double-free/leak tracking.
class GP_RHI_API DescriptorAllocator
{
public:
    /// @brief Constructs an uninitialized descriptor allocator with zero capacity.
    DescriptorAllocator() noexcept;

    /// @brief Constructs and initializes a descriptor allocator with `numDescriptors` slots.
    /// @param[in] numDescriptors Total number of descriptor slots managed by this allocator.
    /// @param[in] stats Optional list of atomic counters incremented on allocation and decremented on free.
    explicit DescriptorAllocator(
        gp::UInt32 numDescriptors, gp::ConstVectorView<DescriptorStatCounter*> stats = {}
    ) noexcept;

    /// @brief Destroys the allocator and reports any leaked allocations when debug tracking is enabled.
    ~DescriptorAllocator() noexcept;

    DescriptorAllocator(const DescriptorAllocator&) = delete;
    DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;
    DescriptorAllocator(DescriptorAllocator&&) = delete;
    DescriptorAllocator& operator=(DescriptorAllocator&&) = delete;

public:
    /// @brief Initializes the free list with `[0, numDescriptors - 1]` and binds optional stat counters.
    /// @param[in] numDescriptors Total number of descriptor slots to manage.
    /// @param[in] stats Optional list of atomic counters updated on allocation and free.
    void init(gp::UInt32 numDescriptors, gp::ConstVectorView<DescriptorStatCounter*> stats = {}) noexcept;

    /// @brief Clears all free ranges and resets capacity to zero.
    void shutdown() noexcept;

    /// @brief Expands capacity to `newCapacity` and allocates `numAllocations` contiguous descriptors atomically.
    /// @param[in] newCapacity New total descriptor capacity; must be strictly greater than `getCapacity()`.
    /// @param[in] numAllocations Number of contiguous descriptors to allocate after growing.
    /// @return Allocated descriptor span.
    [[nodiscard]] gp::Optional<DescriptorAllocation>
        resizeGrowAndAllocate(gp::UInt32 newCapacity, gp::UInt32 numAllocations) noexcept;

    /// @brief Allocates `numDescriptors` contiguous descriptor slots using first-fit range search.
    /// @param[in] numDescriptors Number of contiguous slots requested.
    /// @return Populated `DescriptorAllocation` on success, or `gp::nullopt` if no range is large enough.
    [[nodiscard]] gp::Optional<DescriptorAllocation> allocate(gp::UInt32 numDescriptors) noexcept;

    /// @brief Returns a previously allocated contiguous descriptor span back to the free list.
    /// @param[in] allocation Allocation span to release.
    void free(DescriptorAllocation allocation) noexcept;

    /// @brief Allocates a single descriptor slot and wraps it in a typed `view::DescriptorHandle`.
    /// @param[in] type Concrete descriptor type recorded on the returned handle.
    /// @return Valid `DescriptorHandle` on success, or an invalid handle when capacity is exhausted.
    [[nodiscard]] view::DescriptorHandle allocate(view::DescriptorType type) noexcept;

    /// @brief Releases a batch of descriptor handles under a single lock acquisition.
    /// @param[in] handles Read-only view of descriptor handles to free; invalid handles are skipped.
    void free(gp::ConstVectorView<view::DescriptorHandle> handles) noexcept;

    /// @brief Releases a single descriptor handle if it is valid.
    /// @param[in] handle Descriptor handle to free.
    void free(view::DescriptorHandle handle) noexcept;

    /// @brief Computes the tightest inclusive range covering all currently allocated descriptor indices.
    /// @details Useful when copying descriptors between heaps to avoid copying unused leading or trailing slots.
    /// @param[out] outRange Receives the `[first, last]` inclusive allocated range when at least one descriptor is
    ///                      live.
    /// @return True if at least one descriptor is allocated; false if the allocator is completely empty.
    [[nodiscard]] bool getAllocatedRange(DescriptorAllocatorRange& outRange) noexcept;

    /// @brief Returns the current total descriptor slot capacity of this allocator.
    /// @return Total slot count.
    [[nodiscard]] GP_FORCEINLINE_HINT gp::UInt32 getCapacity() const noexcept
    {
        return m_capacity;
    }

public:
    /// @brief Reports whether debug outstanding descriptor tracking is currently enabled.
    /// @return True when debug allocation tracking is active (always false when `GP_DO_CHECK` is disabled).
    [[nodiscard]] static bool isOutstandingAllocationTrackingEnabled() noexcept;

    /// @brief Enables or disables debug tracking of outstanding allocations for double-free and leak detection.
    /// @param[in] enabled True to enable tracking, false to disable.
    static void setOutstandingAllocationTrackingEnabled(bool enabled) noexcept;

private:
    /// @brief Internal unlocked implementation of `allocate(gp::UInt32)`.
    /// @param[in] numDescriptors Number of contiguous descriptor slots requested.
    /// @return Populated `DescriptorAllocation` on success, or `gp::nullopt`.
    [[nodiscard]] gp::Optional<DescriptorAllocation> allocateInternal(gp::UInt32 numDescriptors) noexcept;

    /// @brief Internal unlocked implementation of `free(DescriptorAllocation)`.
    /// @param[in] allocation Allocation span to return to `m_ranges`.
    void freeInternal(DescriptorAllocation allocation) noexcept;

    /// @brief Increments every bound stat counter by `count`.
    /// @param[in] count Number of descriptors allocated.
    void recordAlloc(const gp::UInt32 count) noexcept
    {
        for (DescriptorStatCounter* const stat: m_stats)
        {
            if (stat != nullptr)
            {
                stat->fetchAdd(static_cast<gp::Int64>(count), gp::MemoryOrder::Relaxed);
            }
        }
    }

    /// @brief Decrements every bound stat counter by `count`.
    /// @param[in] count Number of descriptors freed.
    void recordFree(const gp::UInt32 count) noexcept
    {
        for (DescriptorStatCounter* const stat: m_stats)
        {
            if (stat != nullptr)
            {
                stat->fetchSub(static_cast<gp::Int64>(count), gp::MemoryOrder::Relaxed);
            }
        }
    }

private:
    gp::Vector<DescriptorAllocatorRange> m_ranges{};
    gp::UInt32 m_capacity{ 0u };

    gp::thread::RecursiveMutex m_mutex{};

#if GP_DO_CHECK
    /// @brief Debug record tracking a single live descriptor allocation for leak and double-free checks.
    struct DebugDescriptorAllocation
    {
        /// @brief Starting descriptor index of the live allocation.
        gp::UInt32 offset{ 0u };

        /// @brief Number of contiguous descriptors in the live allocation.
        gp::UInt32 count{ 0u };
    };

    gp::Vector<DebugDescriptorAllocation> m_debugOutstandingAllocations{};
#endif

    gp::Vector<DescriptorStatCounter*> m_stats{};
};

}   // namespace gp::rhi::allocator
