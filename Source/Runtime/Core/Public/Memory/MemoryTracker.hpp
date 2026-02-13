// Copyright (c) - Graphical Playground. All rights reserved.

#pragma once

#include "CoreBuild.hpp"
#include <atomic>

namespace GP::Memory
{

/// \brief Global memory allocation statistics for debugging. Tracks total allocations, deallocations, and byte counts.
/// Active only when GP_ENABLE_MEMORY_TRACKING is enabled (debug builds by default).
/// \note Every AAA engine has memory tracking. Without it, you cannot diagnose leaks, fragmentation, or budget
/// overruns. Production engines track per-subsystem budgets (rendering: 256 MB, audio: 64 MB, etc.) and fire warnings
/// when budgets are exceeded.
struct FMemoryTracker
{
public:
#if GP_ENABLE_MEMORY_TRACKING
    std::atomic<Int64> totalAllocations{ 0 };      //<! Total number of allocation calls made.
    std::atomic<Int64> totalDeallocations{ 0 };    //<! Total number of deallocation calls made.
    std::atomic<Int64> activeAllocations{ 0 };     //<! Current number of active allocations (total - deallocations).
    std::atomic<Int64> totalBytesAllocated{ 0 };   //<! Total number of bytes allocated across all allocations.
    std::atomic<Int64> totalBytesFreed{ 0 };       //<! Total number of bytes freed across all deallocations.
    std::atomic<Int64> activeBytes{ 0 };           //<! Current number of active bytes allocated.
    std::atomic<Int64> peakActiveBytes{ 0 };       //<! Peak number of active bytes allocated at any point in time.
#endif

public:
    /// \brief Updates memory tracking statistics on allocation. Increments total and active allocation counts and byte
    /// counts.
    /// \param bytes The number of bytes allocated.
    void OnAllocate(SizeT bytes) noexcept
    {
#if GP_ENABLE_MEMORY_TRACKING
        totalAllocations.fetch_add(1, std::memory_order_relaxed);
        activeAllocations.fetch_add(1, std::memory_order_relaxed);
        totalBytesAllocated.fetch_add(static_cast<Int64>(bytes), std::memory_order_relaxed);
        Int64 current =
            activeBytes.fetch_add(static_cast<Int64>(bytes), std::memory_order_relaxed) + static_cast<Int64>(bytes);
        Int64 peak = peakActiveBytes.load(std::memory_order_relaxed);
        while (current > peak && !peakActiveBytes.compare_exchange_weak(peak, current, std::memory_order_relaxed)) {}
#else
        GP_UNUSED(bytes);
#endif
    }

    /// \brief Updates memory tracking statistics on deallocation. Increments total deallocation count and byte count,
    /// and decrements active allocation count and byte count.
    /// \param bytes The number of bytes deallocated.
    void OnDeallocate(SizeT bytes) noexcept
    {
#if GP_ENABLE_MEMORY_TRACKING
        totalDeallocations.fetch_add(1, std::memory_order_relaxed);
        activeAllocations.fetch_sub(1, std::memory_order_relaxed);
        totalBytesFreed.fetch_add(static_cast<Int64>(bytes), std::memory_order_relaxed);
        activeBytes.fetch_sub(static_cast<Int64>(bytes), std::memory_order_relaxed);
#else
        GP_UNUSED(bytes);
#endif
    }

    /// \brief Resets all memory tracking statistics to zero. Useful for starting a new tracking session or clearing
    /// accumulated data.
    void Reset() noexcept
    {
#if GP_ENABLE_MEMORY_TRACKING
        totalAllocations.store(0, std::memory_order_relaxed);
        totalDeallocations.store(0, std::memory_order_relaxed);
        activeAllocations.store(0, std::memory_order_relaxed);
        totalBytesAllocated.store(0, std::memory_order_relaxed);
        totalBytesFreed.store(0, std::memory_order_relaxed);
        activeBytes.store(0, std::memory_order_relaxed);
        peakActiveBytes.store(0, std::memory_order_relaxed);
#endif
    }
};

}   // namespace GP::Memory
