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

/// @brief High-performance O(1) linear bump arena allocator for frame-scoped, job-scoped, or phase-scoped memory.
/// @details Allocates memory by advancing a single cursor (`m_offset`) across a contiguous memory buffer. Incurs
///          zero per-allocation metadata header overhead and zero external fragmentation.
///          Supports O(1) in-place reallocation when resizing the most recent allocation at the top of the bump
///          cursor, O(1) checkpointing/unwinding via `ScopedMemoryMark<LinearAllocator>`, and bulk reclamation via
///          `reset()`.
/// @note Not thread-safe by default; use one `LinearAllocator` per thread (see `ThreadScratchAllocator`) or
///       synchronize external access.
class GP_CORE_API LinearAllocator final : public IAllocator
{
private:
    gp::UInt8* m_buffer{ nullptr };
    Malloc* m_backingMalloc{ nullptr };
    const char* m_poolName{ "LinearAllocator" };
    gp::USize m_capacity{ 0u };
    gp::USize m_offset{ 0u };
    AllocatorStats m_stats{};
    bool m_ownsBuffer{ false };

public:
    /// @brief Constructs an uninitialized linear allocator with zero capacity.
    constexpr LinearAllocator() noexcept = default;

    /// @brief Constructs a linear allocator that allocates its contiguous backing slab from a `Malloc` backend.
    /// @param[in] capacityInBytes Total size in bytes of the contiguous arena slab to allocate.
    /// @param[in] backingMalloc Optional `Malloc` backend used to allocate the slab (`nullptr` uses `GlobalMemory`).
    /// @param[in] poolName Optional static string name for Tracy profiler telemetry (`GP_MEM_ALLOC_N` / `GP_PLOT`).
    explicit LinearAllocator(
        const gp::USize capacityInBytes, Malloc* backingMalloc = nullptr, const char* poolName = "LinearAllocator"
    ) noexcept;

    /// @brief Constructs a linear allocator operating over a caller-owned external memory buffer (zero heap allocs).
    /// @param[in] externalBuffer Pointer to a caller-managed contiguous buffer.
    /// @param[in] capacityInBytes Size in bytes of `externalBuffer`.
    /// @param[in] poolName Optional static string name for Tracy profiler plot telemetry (`GP_PLOT`).
    LinearAllocator(
        void* externalBuffer, const gp::USize capacityInBytes, const char* poolName = "LinearAllocator.External"
    ) noexcept;

    /// @brief Copying a linear allocator is disabled because it uniquely manages its arena cursor.
    LinearAllocator(const LinearAllocator&) = delete;
    LinearAllocator& operator=(const LinearAllocator&) = delete;

    /// @brief Move constructor; transfers buffer ownership and cursor state from `other`.
    /// @param[in,out] other Source linear allocator, left in an empty state.
    LinearAllocator(LinearAllocator&& other) noexcept;

    /// @brief Move assignment operator; releases any owned slab and steals state from `other`.
    /// @param[in,out] other Source linear allocator, left in an empty state.
    /// @return Reference to `*this`.
    LinearAllocator& operator=(LinearAllocator&& other) noexcept;

    /// @brief Destructor; releases the backing slab to `m_backingMalloc` if owned by this instance.
    virtual ~LinearAllocator() noexcept override;

    /// @brief Initializes or re-initializes the arena with a newly allocated slab from `backingMalloc`.
    /// @param[in] capacityInBytes Total capacity in bytes to allocate.
    /// @param[in] backingMalloc Optional `Malloc` backend (`nullptr` uses `GlobalMemory`).
    /// @param[in] poolName Optional static string name for profiler tracking.
    /// @return True if the backing buffer was successfully allocated; false on out-of-memory or zero capacity.
    bool initialize(
        const gp::USize capacityInBytes, Malloc* backingMalloc = nullptr, const char* poolName = "LinearAllocator"
    ) noexcept;

    /// @brief Initializes or re-initializes the arena over an external caller-owned buffer.
    /// @param[in] externalBuffer Pointer to the external memory block.
    /// @param[in] capacityInBytes Size in bytes of `externalBuffer`.
    /// @param[in] poolName Optional static string name for profiler tracking.
    /// @return True if `externalBuffer != nullptr` and `capacityInBytes > 0`; false otherwise.
    bool initialize(
        void* externalBuffer, const gp::USize capacityInBytes, const char* poolName = "LinearAllocator.External"
    ) noexcept;

    /// @brief Releases the backing slab (if owned) and resets all state to empty.
    void shutdown() noexcept;

    /// @copydoc IAllocator::allocate()
    /// @note Advances `m_offset` aligned to `max(kMinimumAlignment, alignment)` in O(1) time.
    [[nodiscard]] virtual void*
        allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept override;

    /// @copydoc IAllocator::reallocate()
    /// @note If `ptr` is the most recent allocation at the top of the bump cursor and remains sufficiently aligned,
    ///       resizes the block in place in O(1) time without copying bytes. Otherwise allocates a new block from the
    ///       bumper and copies `min(oldSizeInBytes, newSizeInBytes)` bytes.
    [[nodiscard]] virtual void* reallocate(
        void* ptr,
        const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept override;

    /// @copydoc IAllocator::deallocate()
    /// @note If `ptr` sits at the exact top of the bump cursor (`ptr + oldSizeInBytes == m_buffer + m_offset`),
    ///       rewinds `m_offset` by `oldSizeInBytes` immediately. Otherwise acts as a no-op until `rewind()` or
    ///       `reset()`.
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

    /// @brief Captures a checkpoint of the current bump cursor for subsequent `rewind()`.
    /// @return `MemoryMark` representing the current arena state.
    [[nodiscard]] constexpr MemoryMark mark() const noexcept
    {
        return MemoryMark{ .offset = m_offset,
                           .activeAllocations = m_stats.activeAllocations,
                           .allocatedBytes = m_stats.allocatedBytes };
    }

    /// @brief Rewinds the arena cursor back to a previously captured `MemoryMark` in O(1) time.
    /// @param[in] savedMark Checkpoint previously returned by `mark()`.
    void rewind(const MemoryMark& savedMark) noexcept;

    /// @brief Reclaims all allocations in the arena in O(1) time by resetting `m_offset` to `0`.
    /// @note Preserves `m_stats.peakAllocatedBytes` for high-water mark diagnostics.
    void reset() noexcept;

    /// @brief Retrieves the total capacity of the arena in bytes.
    /// @return Capacity in bytes.
    [[nodiscard]] constexpr gp::USize getCapacity() const noexcept
    {
        return m_capacity;
    }

    /// @brief Retrieves the current byte offset of the bump cursor (including alignment padding).
    /// @return Used bytes from the start of the buffer.
    [[nodiscard]] constexpr gp::USize getUsedBytes() const noexcept
    {
        return m_offset;
    }

    /// @brief Retrieves the remaining unallocated byte capacity from the current cursor to the end of the buffer.
    /// @return Remaining bytes available.
    [[nodiscard]] constexpr gp::USize getRemainingBytes() const noexcept
    {
        return m_capacity >= m_offset ? (m_capacity - m_offset) : 0u;
    }

    /// @brief Retrieves the current telemetry statistics for this arena.
    /// @return Const reference to `AllocatorStats`.
    [[nodiscard]] constexpr const AllocatorStats& getStats() const noexcept
    {
        return m_stats;
    }

    /// @brief Checks whether the arena holds a valid backing buffer.
    /// @return True if initialized with a non-null buffer and positive capacity; false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_buffer != nullptr && m_capacity > 0u;
    }
};

static_assert(
    gp::concepts::IsMarkableAllocator<LinearAllocator>,
    "LinearAllocator must satisfy gp::concepts::IsMarkableAllocator."
);

}   // namespace gp::memory
