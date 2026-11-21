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

    /// @brief Retrieves a pointer to the calling thread's `LinearAllocator` if already initialized, or `nullptr`
    ///        without triggering a lazy heap slab allocation.
    /// @return Pointer to the calling thread's `LinearAllocator`, or `nullptr` if uninitialized.
    [[nodiscard]] static LinearAllocator* tryGet() noexcept;

    /// @brief Retrieves a reference to the calling thread's `LinearAllocator` object without triggering lazy slab
    ///        allocation.
    /// @return Reference to the calling thread's `thread_local LinearAllocator` instance.
    [[nodiscard]] static LinearAllocator& getUnchecked() noexcept;

    /// @brief Queries the calling OS thread's monotonic fiber context-switch epoch counter.
    /// @details Incremented whenever `FiberScratchArena` binds a fiber arena or records a fiber yield on the calling
    ///          OS worker thread. Used by `ScopedScratchMark` to mechanically detect illegal fiber yields inside
    ///          OS-thread-bound scratch scopes.
    /// @return Current fiber-switch epoch on the calling OS thread.
    [[nodiscard]] static gp::UInt64 getCurrentThreadEpoch() noexcept;

    /// @brief Advances the calling OS thread's monotonic fiber context-switch epoch counter by 1.
    /// @return New fiber-switch epoch value on the calling OS thread.
    static gp::UInt64 advanceCurrentThreadEpoch() noexcept;
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
        if (sizeInBytes == 0u) [[unlikely]]
        {
            return nullptr;
        }

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
        if (newSizeInBytes == 0u)
        {
            deallocate(ptr, oldSizeInBytes, alignment);
            return nullptr;
        }

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
        if (ptr == nullptr)
        {
            return;
        }

        if (LinearAllocator* const arena = ThreadScratchArena::tryGet(); arena != nullptr)
        {
            arena->deallocate(ptr, oldSizeInBytes, alignment);
        }
    }

    /// @brief Queries the aligned allocation size from the calling thread's `ThreadScratchArena` without lazy init.
    /// @param[in] requestedSize Requested byte count.
    /// @param[in] alignment Requested byte alignment.
    /// @return Aligned byte count.
    [[nodiscard]] gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        if (const LinearAllocator* const arena = ThreadScratchArena::tryGet(); arena != nullptr)
        {
            return arena->getActualAllocationSize(requestedSize, alignment);
        }

        return requestedSize;
    }

    /// @brief Checks whether `ptr` belongs to the calling thread's `ThreadScratchArena` without lazy init.
    /// @param[in] ptr Pointer to test.
    /// @return True if `ptr` lies within the calling thread's scratch slab.
    [[nodiscard]] bool owns(const void* ptr) const noexcept
    {
        if (const LinearAllocator* const arena = ThreadScratchArena::tryGet(); arena != nullptr)
        {
            return arena->owns(ptr);
        }

        return false;
    }

    /// @brief Captures a checkpoint of the calling thread's scratch cursor without forcing lazy initialization.
    /// @return `MemoryMark` for the calling thread's scratch arena.
    [[nodiscard]] MemoryMark mark() const noexcept
    {
        if (const LinearAllocator* const arena = ThreadScratchArena::tryGet(); arena != nullptr)
        {
            return arena->mark();
        }

        return MemoryMark{};
    }

    /// @brief Rewinds the calling thread's scratch cursor back to `savedMark` if the arena is active.
    /// @param[in] savedMark Checkpoint previously captured on the calling thread.
    void rewind(const MemoryMark& savedMark) noexcept
    {
        if (LinearAllocator* const arena = ThreadScratchArena::tryGet(); arena != nullptr)
        {
            arena->rewind(savedMark);
        }
    }

    /// @brief Resets the calling thread's scratch cursor back to `0` if the arena is active.
    void reset() noexcept
    {
        if (LinearAllocator* const arena = ThreadScratchArena::tryGet(); arena != nullptr)
        {
            arena->reset();
        }
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
///          Entering and exiting a `ScopedScratchMark` scope that makes zero allocations does not lazily allocate the
///          thread-local slab.
class GP_CORE_API ScopedScratchMark final
{
public:
    /// @brief Captures the current cursor mark and fiber-switch epoch of the calling thread's `ThreadScratchArena`
    ///        without lazy allocation.
    ScopedScratchMark() noexcept
        : m_mark(ThreadScratchArena::tryGet() != nullptr ? ThreadScratchArena::tryGet()->mark() : MemoryMark{})
    {
        m_mark.pageCookie = static_cast<void*>(&ThreadScratchArena::getUnchecked());
        m_mark.contextEpoch = ThreadScratchArena::getCurrentThreadEpoch();
    }

    /// @brief Rewinds the calling thread's `ThreadScratchArena` back to the mark captured on construction if active.
    /// @note Asserts that no cooperative fiber context switch or cross-thread migration occurred while this guard was
    ///       live on the stack.
    ~ScopedScratchMark() noexcept
    {
        const bool validContext = isContextValid();
        GP_ASSERT(
            validContext,
            "ScopedScratchMark spanned a cooperative fiber context switch or migrated across OS worker threads. "
            "ThreadScratchAllocator is strictly for non-yielding leaf scopes; use FiberScratchAllocator and "
            "ScopedFiberScratchMark across fiber yield points."
        );

        if (validContext) [[likely]]
        {
            if (LinearAllocator* const arena = ThreadScratchArena::tryGet(); arena != nullptr)
            {
                arena->rewind(m_mark);
            }
        }
    }

    ScopedScratchMark(const ScopedScratchMark&) = delete;
    ScopedScratchMark& operator=(const ScopedScratchMark&) = delete;
    ScopedScratchMark(ScopedScratchMark&&) = delete;
    ScopedScratchMark& operator=(ScopedScratchMark&&) = delete;

    /// @brief Manually rewinds the calling thread's scratchpad to the saved checkpoint before the scope exits.
    void rewindNow() noexcept
    {
        const bool validContext = isContextValid();
        GP_ASSERT(
            validContext,
            "ScopedScratchMark::rewindNow called after a cooperative fiber context switch or cross-thread migration."
        );

        if (validContext) [[likely]]
        {
            if (LinearAllocator* const arena = ThreadScratchArena::tryGet(); arena != nullptr)
            {
                arena->rewind(m_mark);
            }
        }
    }

    /// @brief Checks whether the calling OS thread and fiber-switch epoch still match the context where this guard was
    ///        constructed.
    /// @return True if no fiber switch or thread migration occurred since construction; false otherwise.
    [[nodiscard]] bool isContextValid() const noexcept
    {
        return (m_mark.pageCookie == static_cast<const void*>(&ThreadScratchArena::getUnchecked())) &&
               (m_mark.contextEpoch == ThreadScratchArena::getCurrentThreadEpoch());
    }

    /// @brief Retrieves the saved checkpoint captured when this guard was constructed.
    /// @return Const reference to `MemoryMark`.
    [[nodiscard]] constexpr const MemoryMark& getMark() const noexcept
    {
        return m_mark;
    }

private:
    MemoryMark m_mark{};
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
