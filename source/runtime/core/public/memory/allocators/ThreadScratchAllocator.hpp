// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/AllocatorTypes.hpp"
#include "memory/allocators/LinearAllocator.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/MemoryForward.hpp"

namespace gp::memory
{

/// @brief Manages the per-thread `thread_local` linear scratchpad arena for zero-synchronization temporary allocations.
/// @details Inspired by id Tech 7, Decima, and Glacier 2 job-worker scratchpads. Each worker thread owns an independent
///          `LinearAllocator` (lazily initialized to `kDefaultScratchCapacityBytes` on first use, or explicitly sized
///          via `initializeThreadScratch()`).
class GP_CORE_API ThreadScratchArena final
{
public:
    /// @brief Default per-thread scratchpad capacity (256 KiB).
    static constexpr gp::USize kDefaultScratchCapacityBytes = 256u * 1024u;

public:
    ThreadScratchArena() = delete;

    /// @brief Explicitly initializes or resizes the calling thread's scratchpad arena.
    /// @param[in] capacityInBytes Desired capacity in bytes for the calling thread's scratchpad.
    /// @param[in] backingMalloc Optional `Malloc` backend (`nullptr` uses `GlobalMemory`).
    /// @return True if the calling thread's scratchpad was successfully allocated; false otherwise.
    static bool initializeThreadScratch(
        const gp::USize capacityInBytes = kDefaultScratchCapacityBytes, Malloc* backingMalloc = nullptr
    ) noexcept;

    /// @brief Releases the calling thread's scratchpad backing slab immediately.
    static void shutdownThreadScratch() noexcept;

    /// @brief Reclaims all allocations in the calling thread's scratchpad in O(1) time (`get().reset()`).
    static void resetThreadScratch() noexcept;

    /// @brief Retrieves a reference to the calling thread's `LinearAllocator`, lazily initializing it if needed.
    /// @return Reference to the calling thread's `thread_local LinearAllocator`.
    [[nodiscard]] static LinearAllocator& get() noexcept;
};

/// @brief Zero-byte stateless allocator proxy that routes all allocations to the calling thread's `ThreadScratchArena`.
/// @details Because `ThreadScratchAllocator` has no non-static data members (`sizeof == 1`, `[[no_unique_address]]` ==
/// 0 bytes),
///          any container (`gp::Vector<T, gp::memory::ThreadScratchAllocator>`) can use the thread-local scratchpad
///          with zero per-container memory overhead and zero atomic contention.
class GP_CORE_API ThreadScratchAllocator
{
public:
    /// @brief Default-constructs the stateless thread-scratch allocator proxy.
    constexpr ThreadScratchAllocator() noexcept = default;

    /// @brief Allocates `sizeInBytes` from the calling thread's `ThreadScratchArena`.
    /// @param[in] sizeInBytes Number of bytes to allocate.
    /// @param[in] alignment Required byte alignment (power of two).
    /// @return Pointer to the allocated scratch memory, or `nullptr` if capacity is exhausted.
    [[nodiscard]] void* allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept
    {
        return ThreadScratchArena::get().allocate(sizeInBytes, alignment);
    }

    /// @brief Resizes an allocation on the calling thread's `ThreadScratchArena` (in place in O(1) if at top of
    /// cursor).
    /// @param[in] ptr Existing scratch pointer (or `nullptr`).
    /// @param[in] oldSizeInBytes Current allocation size in bytes.
    /// @param[in] newSizeInBytes Desired new size in bytes.
    /// @param[in] alignment Required byte alignment (power of two).
    /// @return Resized scratch pointer, or `nullptr` on failure or zero size.
    [[nodiscard]] void* reallocate(
        void* ptr,
        const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        return ThreadScratchArena::get().reallocate(ptr, oldSizeInBytes, newSizeInBytes, alignment);
    }

    /// @brief Rewinds the calling thread's scratch cursor if `ptr` sits at the top of the bump cursor; otherwise no-op.
    /// @param[in] ptr Scratch pointer to deallocate.
    /// @param[in] oldSizeInBytes Size in bytes of the allocation at `ptr`.
    /// @param[in] alignment Alignment used when allocating `ptr`.
    void deallocate(
        void* ptr, const gp::USize oldSizeInBytes = 0u, const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept
    {
        ThreadScratchArena::get().deallocate(ptr, oldSizeInBytes, alignment);
    }

    /// @brief Queries the aligned allocation size from the calling thread's `ThreadScratchArena`.
    /// @param[in] requestedSize Requested byte count.
    /// @param[in] alignment Requested byte alignment.
    /// @return Aligned byte count.
    [[nodiscard]] gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        return ThreadScratchArena::get().getActualAllocationSize(requestedSize, alignment);
    }

    /// @brief Checks whether `ptr` belongs to the calling thread's `ThreadScratchArena`.
    /// @param[in] ptr Pointer to test.
    /// @return True if `ptr` lies within the calling thread's scratch slab.
    [[nodiscard]] bool owns(const void* ptr) const noexcept
    {
        return ThreadScratchArena::get().owns(ptr);
    }

    /// @brief Captures a checkpoint of the calling thread's scratch cursor.
    /// @return `MemoryMark` for the calling thread's scratch arena.
    [[nodiscard]] MemoryMark mark() const noexcept
    {
        return ThreadScratchArena::get().mark();
    }

    /// @brief Rewinds the calling thread's scratch cursor back to `savedMark`.
    /// @param[in] savedMark Checkpoint previously captured on the calling thread.
    void rewind(const MemoryMark& savedMark) noexcept
    {
        ThreadScratchArena::get().rewind(savedMark);
    }

    /// @brief Resets the calling thread's scratch cursor back to `0`.
    void reset() noexcept
    {
        ThreadScratchArena::get().reset();
    }

    /// @brief Stateless equality comparison; always returns true for allocations on the same thread.
    [[nodiscard]] constexpr bool operator==(const ThreadScratchAllocator&) const noexcept
    {
        return true;
    }
};

/// @brief RAII scope guard that checkpoints the calling thread's `ThreadScratchArena` on construction and rewinds it on
/// destruction.
/// @details Place `ScopedScratchMark scratchScope;` at the top of any function or job that uses
/// `ThreadScratchAllocator`
///          so all temporary allocations are automatically reclaimed in O(1) time when the scope exits.
class GP_CORE_API ScopedScratchMark final
{
private:
    MemoryMark m_mark{};

public:
    /// @brief Captures the current cursor mark of the calling thread's `ThreadScratchArena`.
    ScopedScratchMark() noexcept
        : m_mark(ThreadScratchArena::get().mark())
    {}

    /// @brief Rewinds the calling thread's `ThreadScratchArena` back to the mark captured on construction.
    ~ScopedScratchMark() noexcept
    {
        ThreadScratchArena::get().rewind(m_mark);
    }

    ScopedScratchMark(const ScopedScratchMark&) = delete;
    ScopedScratchMark& operator=(const ScopedScratchMark&) = delete;
    ScopedScratchMark(ScopedScratchMark&&) = delete;
    ScopedScratchMark& operator=(ScopedScratchMark&&) = delete;

    /// @brief Retrieves the saved checkpoint captured when this guard was constructed.
    /// @return Const reference to `MemoryMark`.
    [[nodiscard]] constexpr const MemoryMark& getMark() const noexcept
    {
        return m_mark;
    }
};

static_assert(
    gp::concepts::IsStatelessAllocator<ThreadScratchAllocator>,
    "ThreadScratchAllocator must satisfy gp::concepts::IsStatelessAllocator."
);
static_assert(
    gp::concepts::IsMarkableAllocator<ThreadScratchAllocator>,
    "ThreadScratchAllocator must satisfy gp::concepts::IsMarkableAllocator."
);

}   // namespace gp::memory
