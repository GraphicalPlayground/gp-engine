// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/allocators/AllocatorConcepts.hpp"
#include "memory/allocators/AllocatorTypes.hpp"
#include "memory/allocators/PagedLinearAllocator.hpp"
#include "memory/allocators/PagePool.hpp"
#include "memory/MemoryBase.hpp"
#include "memory/MemoryForward.hpp"

namespace gp::memory
{

/// @brief Manages the execution-context-swapped fiber scratchpad arenas and their shared lock-free `PagePool`.
/// @details Inspired by Naughty Dog's fiber job system (GDC 2015), Frostbite's Scope Stacks, and Decima's job
///          scratchpads ([ADR-0048](../../../../docs/adr/ADR-0048.md)).
///          Unlike `ThreadScratchArena` (which is permanently pinned to an OS worker thread and corrupts if a fiber
///          yields at `waitForCounter()`), `FiberScratchArena` binds a `PagedLinearAllocator` to the **currently
///          executing Fiber context** via `bindFiberArena()` / `ScopedFiberArenaBind`.
///          When a fiber yields on Worker Thread A and is stolen/resumed on Worker Thread B, its `PagedLinearAllocator`
///          travels with the fiber, guaranteeing 100% LIFO mark integrity, zero atomic contention on intra-page bumps,
///          and zero idle memory footprint for suspended or unused fibers.
class GP_CORE_API FiberScratchArena final
{
public:
    /// @brief Default byte size of each page in the shared fiber scratch pool (64 KiB).
    static constexpr gp::USize kDefaultPageSizeBytes = PagePool::kDefaultPageSizeBytes;

    /// @brief Default number of 64 KiB pages in the shared fiber scratch pool (128 pages = 8 MiB).
    static constexpr gp::UInt32 kDefaultPoolPageCount = 128u;

public:
    FiberScratchArena() = delete;

    /// @brief Explicitly initializes or resizes the shared lock-free `PagePool` backing all fiber scratch arenas.
    /// @note Call once during engine or Job System startup before worker threads begin leasing pages. If not called
    ///       explicitly, the shared pool is lazily initialized with default parameters on the first page request.
    /// @param[in] pageSizeInBytes Size in bytes of each page (rounded up to a multiple of 64 bytes).
    /// @param[in] pageCount Total number of pages to provision in the shared pool.
    /// @param[in] backingMalloc Optional `Malloc` backend (`nullptr` uses `GlobalMemory`).
    /// @return True if the shared `PagePool` was successfully initialized; false otherwise.
    static bool initializePool(
        const gp::USize pageSizeInBytes = kDefaultPageSizeBytes,
        const gp::UInt32 pageCount = kDefaultPoolPageCount,
        Malloc* backingMalloc = nullptr
    ) noexcept;

    /// @brief Releases thecalling thread's fallback scratch pages and shuts down the shared `PagePool` slab.
    /// @note Must only be called when no fiber arenas hold active leased pages from the shared pool.
    static void shutdownPool() noexcept;

    /// @brief Releases any leased pages held by the calling OS thread's unbound fallback `PagedLinearAllocator`.
    static void shutdownThreadFallbackScratch() noexcept;

    /// @brief Retrieves a reference to the shared lock-free `PagePool`, lazily initializing it if needed.
    /// @return Reference to the shared `PagePool`.
    [[nodiscard]] static PagePool& getSharedPagePool() noexcept;

    /// @brief Retrieves a pointer to the shared `PagePool` if already initialized, or `nullptr` without triggering
    ///        lazy slab allocation.
    /// @return Pointer to the shared `PagePool`, or `nullptr` if uninitialized.
    [[nodiscard]] static PagePool* tryGetSharedPagePool() noexcept;

    /// @brief Binds `fiberArena` as the active fiber scratchpad on the calling OS worker thread.
    /// @details Invoked by the Fiber Job Scheduler whenever a worker thread switches execution onto a fiber (or passes
    ///          `nullptr` to unbind when returning to the scheduler loop). Automatically increments the calling OS
    ///          thread's fiber-switch epoch counter so any illegal `ScopedScratchMark` (`ThreadScratchAllocator`)
    ///          spanning the context switch is mechanically caught.
    ///          If `fiberArena != nullptr` and `!fiberArena->isValid()`, automatically configures `fiberArena` to draw
    ///          pages from `getSharedPagePool()`.
    /// @param[in,out] fiberArena Pointer to the incoming fiber's `PagedLinearAllocator`, or `nullptr` to unbind.
    /// @return Pointer to the `PagedLinearAllocator` that was previously bound on the calling thread (or `nullptr`).
    static PagedLinearAllocator* bindFiberArena(PagedLinearAllocator* fiberArena) noexcept;

    /// @brief Retrieves the `PagedLinearAllocator` explicitly bound to the calling OS worker thread, if any.
    /// @return Pointer to the bound fiber arena, or `nullptr` if the calling thread is currently unbound.
    [[nodiscard]] static PagedLinearAllocator* getBoundFiberArena() noexcept;

    /// @brief Records a cooperative fiber yield on the calling OS worker thread, advancing the thread's switch epoch.
    /// @return Updated fiber-switch epoch counter on the calling OS thread.
    static gp::UInt64 notifyFiberYield() noexcept;

    /// @brief Queries the calling OS worker thread's monotonic fiber-switch epoch counter.
    /// @return Current fiber-switch epoch on the calling OS thread.
    [[nodiscard]] static gp::UInt64 getThreadSwitchEpoch() noexcept;

    /// @brief Retrieves a reference to the active fiber's `PagedLinearAllocator` (or the calling thread's fallback
    ///        `PagedLinearAllocator` when executing outside a bound fiber).
    /// @note Obtaining this reference never allocates heap or pool memory; pages are leased lazily from the shared
    ///       `PagePool` only when `allocate()` is invoked.
    /// @return Reference to the active `PagedLinearAllocator`.
    [[nodiscard]] static PagedLinearAllocator& get() noexcept;

    /// @brief Retrieves a pointer to the active `PagedLinearAllocator` if a fiber is bound or the shared pool is
    ///        already initialized, or `nullptr` if completely cold.
    /// @return Pointer to the active `PagedLinearAllocator`, or `nullptr`.
    [[nodiscard]] static PagedLinearAllocator* tryGet() noexcept;

    /// @brief Reclaims all pages in the currently active fiber (or fallback) scratch arena in O(1) atomic CAS time.
    static void resetCurrentScratch() noexcept;
};

/// @brief RAII scheduler guard that binds a fiber's `PagedLinearAllocator` to the calling worker thread on construction
///        and restores the previous binding on destruction.
class GP_CORE_API ScopedFiberArenaBind final
{
private:
    PagedLinearAllocator* m_previousArena{ nullptr };

public:
    /// @brief Binds `fiberArena` as the active `FiberScratchArena` on the calling OS thread.
    /// @param[in,out] fiberArena Reference to the fiber's `PagedLinearAllocator` to activate.
    explicit ScopedFiberArenaBind(PagedLinearAllocator& fiberArena) noexcept
        : m_previousArena(FiberScratchArena::bindFiberArena(&fiberArena))
    {}

    /// @brief Restores the previously bound fiber arena (or `nullptr`) on the calling OS thread.
    ~ScopedFiberArenaBind() noexcept
    {
        (void)FiberScratchArena::bindFiberArena(m_previousArena);
    }

    ScopedFiberArenaBind(const ScopedFiberArenaBind&) = delete;
    ScopedFiberArenaBind& operator=(const ScopedFiberArenaBind&) = delete;
    ScopedFiberArenaBind(ScopedFiberArenaBind&&) = delete;
    ScopedFiberArenaBind& operator=(ScopedFiberArenaBind&&) = delete;

    /// @brief Retrieves the arena pointer that was bound prior to this guard.
    /// @return Previously bound `PagedLinearAllocator*`, or `nullptr`.
    [[nodiscard]] constexpr PagedLinearAllocator* getPreviousArena() const noexcept
    {
        return m_previousArena;
    }
};

/// @brief Zero-byte stateless allocator proxy that routes all allocations to the active fiber's `FiberScratchArena`.
/// @details Because `FiberScratchAllocator` has no non-static data members (`sizeof == 1`, `[[no_unique_address]]` ==
///          0 bytes), any engine container (`gp::Vector<T, gp::memory::FiberScratchAllocator>`) can allocate from the
///          current fiber's elastic page-chained scratchpad with zero per-container memory overhead, zero atomic
///          contention on intra-page bumps, and full safety across cooperative fiber yields (`waitForCounter`) and
///          cross-core work-stealing migrations.
class GP_CORE_API FiberScratchAllocator
{
public:
    /// @brief Default-constructs the stateless fiber-scratch allocator proxy.
    constexpr FiberScratchAllocator() noexcept = default;

    /// @brief Allocates `sizeInBytes` from the active `FiberScratchArena`.
    /// @param[in] sizeInBytes Number of bytes to allocate.
    /// @param[in] alignment Required byte alignment (power of two).
    /// @return Pointer to the allocated scratch memory, or `nullptr` if capacity is exhausted.
    [[nodiscard]] void* allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept
    {
        if (sizeInBytes == 0u) [[unlikely]]
        {
            return nullptr;
        }

        return FiberScratchArena::get().allocate(sizeInBytes, alignment);
    }

    /// @brief Resizes an allocation on the active `FiberScratchArena` (in place in O(1) if at top of active page).
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

        return FiberScratchArena::get().reallocate(ptr, oldSizeInBytes, newSizeInBytes, alignment);
    }

    /// @brief Rewinds the active fiber's page cursor if `ptr` sits at the top of the current page; otherwise no-op.
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

        if (PagedLinearAllocator* const arena = FiberScratchArena::tryGet(); arena != nullptr)
        {
            arena->deallocate(ptr, oldSizeInBytes, alignment);
        }
    }

    /// @brief Queries the aligned allocation size from the active `FiberScratchArena` without lazy pool initialization.
    /// @param[in] requestedSize Requested byte count.
    /// @param[in] alignment Requested byte alignment.
    /// @return Aligned byte count.
    [[nodiscard]] gp::USize getActualAllocationSize(
        const gp::USize requestedSize, const gp::UInt32 alignment = kDefaultAlignment
    ) const noexcept
    {
        if (const PagedLinearAllocator* const arena = FiberScratchArena::tryGet(); arena != nullptr)
        {
            return arena->getActualAllocationSize(requestedSize, alignment);
        }

        return requestedSize;
    }

    /// @brief Checks whether `ptr` belongs to the active fiber's `PagedLinearAllocator` without lazy pool init.
    /// @param[in] ptr Pointer to test.
    /// @return True if `ptr` lies within one of the active fiber's leased pages.
    [[nodiscard]] bool owns(const void* ptr) const noexcept
    {
        if (const PagedLinearAllocator* const arena = FiberScratchArena::tryGet(); arena != nullptr)
        {
            return arena->owns(ptr);
        }

        return false;
    }

    /// @brief Captures a checkpoint of the active fiber's `PagedLinearAllocator` without lazy pool initialization.
    /// @return `MemoryMark` for the active fiber scratch arena.
    [[nodiscard]] MemoryMark mark() const noexcept
    {
        return FiberScratchArena::get().mark();
    }

    /// @brief Rewinds the active fiber's `PagedLinearAllocator` back to `savedMark`.
    /// @param[in] savedMark Checkpoint previously captured on the active fiber.
    void rewind(const MemoryMark& savedMark) noexcept
    {
        FiberScratchArena::get().rewind(savedMark);
    }

    /// @brief Releases all leased pages in the active fiber's `PagedLinearAllocator`.
    void reset() noexcept
    {
        if (PagedLinearAllocator* const arena = FiberScratchArena::tryGet(); arena != nullptr)
        {
            arena->reset();
        }
    }

    /// @brief Stateless equality comparison; always returns true for allocations within the same active fiber context.
    [[nodiscard]] constexpr bool operator==(const FiberScratchAllocator&) const noexcept
    {
        return true;
    }
};

/// @brief Migration-safe RAII scope guard that checkpoints the active fiber's `PagedLinearAllocator` on construction
///        and rewinds it (returning any newly leased pages to `PagePool`) on destruction.
/// @details Unlike `ScopedScratchMark` (`ThreadScratchAllocator`), `ScopedFiberScratchMark` records a direct pointer to
///          the originating fiber's `PagedLinearAllocator` (`m_ownerArena`). When the fiber yields at `waitForCounter`
///          and resumes later on a different OS worker thread, `ScopedFiberScratchMark` verifies that `m_ownerArena`
///          is bound on the resuming worker thread (`isArenaBoundToCurrentContext()`) and unwinds that exact fiber
///          arena in O(1) atomic CAS operations.
class GP_CORE_API ScopedFiberScratchMark final
{
private:
    PagedLinearAllocator* m_ownerArena{ nullptr };
    MemoryMark m_mark{};

public:
    /// @brief Captures the active fiber's `PagedLinearAllocator` and its current `MemoryMark` without lazy allocation.
    ScopedFiberScratchMark() noexcept
        : m_ownerArena(&FiberScratchArena::get())
        , m_mark(m_ownerArena->mark())
    {}

    /// @brief Rewinds the originating fiber's `PagedLinearAllocator` back to the mark captured on construction.
    /// @note Asserts that `m_ownerArena` is the currently bound `FiberScratchArena` on the executing worker thread.
    ~ScopedFiberScratchMark() noexcept
    {
        GP_ASSERT(
            isArenaBoundToCurrentContext(),
            "ScopedFiberScratchMark destroyed while a different fiber arena is bound on the current worker thread. "
            "Ensure the Fiber Scheduler restores the fiber's PagedLinearAllocator via "
            "FiberScratchArena::bindFiberArena() when resuming a yielded fiber."
        );

        if (m_ownerArena != nullptr) [[likely]]
        {
            m_ownerArena->rewind(m_mark);
        }
    }

    ScopedFiberScratchMark(const ScopedFiberScratchMark&) = delete;
    ScopedFiberScratchMark& operator=(const ScopedFiberScratchMark&) = delete;
    ScopedFiberScratchMark(ScopedFiberScratchMark&&) = delete;
    ScopedFiberScratchMark& operator=(ScopedFiberScratchMark&&) = delete;

    /// @brief Manually rewinds the fiber arena to the saved checkpoint before the lexical scope exits.
    void rewindNow() noexcept
    {
        GP_ASSERT(
            isArenaBoundToCurrentContext(),
            "ScopedFiberScratchMark::rewindNow called while a different fiber arena is bound on the current worker " "thread."
        );

        if (m_ownerArena != nullptr) [[likely]]
        {
            m_ownerArena->rewind(m_mark);
        }
    }

    /// @brief Checks whether the fiber arena captured on construction is currently bound as the active arena on the
    ///        calling OS worker thread.
    /// @return True if `&FiberScratchArena::get() == m_ownerArena`; false if the fiber context was not restored.
    [[nodiscard]] bool isArenaBoundToCurrentContext() const noexcept
    {
        return m_ownerArena == &FiberScratchArena::get();
    }

    /// @brief Retrieves the originating fiber's `PagedLinearAllocator` bound when this guard was constructed.
    /// @return Pointer to the owning `PagedLinearAllocator`.
    [[nodiscard]] constexpr PagedLinearAllocator* getArena() const noexcept
    {
        return m_ownerArena;
    }

    /// @brief Retrieves the saved checkpoint captured when this guard was constructed.
    /// @return Const reference to `MemoryMark`.
    [[nodiscard]] constexpr const MemoryMark& getMark() const noexcept
    {
        return m_mark;
    }
};

static_assert(
    gp::concepts::IsStatelessAllocator<FiberScratchAllocator>,
    "FiberScratchAllocator must satisfy gp::concepts::IsStatelessAllocator."
);
static_assert(
    gp::concepts::IsMarkableAllocator<FiberScratchAllocator>,
    "FiberScratchAllocator must satisfy gp::concepts::IsMarkableAllocator."
);

}   // namespace gp::memory
