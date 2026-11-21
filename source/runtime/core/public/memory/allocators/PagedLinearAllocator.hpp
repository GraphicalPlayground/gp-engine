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

/// @brief Elastic page-chained O(1) linear bump arena allocator with cross-page `MemoryMark` unwinding and zero idle
///        memory footprint.
/// @details Designed for Fiber-based Work-Stealing Job Systems ([ADR-0048](../../../../docs/adr/ADR-0048.md)) where
///          pre-allocating fixed worst-case contiguous slabs for every fiber in a large pool ($M \gg N$) would waste
///          tens of megabytes of idle RAM.
///          An idle or freshly reset `PagedLinearAllocator` holds zero pages (`0` bytes). As allocations arrive, it
///          leases fixed-size pages (`64 KiB` default) on demand from a shared lock-free `PagePool`, bumps a non-atomic
///          cursor inside the active page in O(1) time, and chains pages via an intrusive 32-byte `PageHeader`.
///          Supports O(1) top-of-page in-place `reallocate()`, oversized heap block fallback, and bulk O(1) atomic
///          page-chain reclamation during `rewind()` and `reset()`.
/// @note Not thread-safe for concurrent mutations on the same instance; bind one `PagedLinearAllocator` per fiber
///       context (see `FiberScratchAllocator`).
class GP_CORE_API PagedLinearAllocator final : public IAllocator
{
public:
    /// @brief Callback signature used to lazily resolve or initialize a shared `PagePool` on the first allocation.
    using PagePoolProviderFn = PagePool* (*)() noexcept;

    /// @brief Intrusive 32-byte header placed at offset 0 of every leased pool page or oversized heap block.
    struct alignas(kDefaultAlignment) PageHeader
    {
        /// @brief Pointer to the previously leased page in chronological LIFO order, or `nullptr` if first page.
        PageHeader* prevPage{ nullptr };

        /// @brief Total byte capacity of this page (including `sizeof(PageHeader)`).
        gp::USize capacityBytes{ 0u };

        /// @brief Current bump cursor offset in bytes from the base of `PageHeader` (`>= sizeof(PageHeader)`).
        gp::USize usedOffset{ 0u };

        /// @brief True if this block was allocated directly from `m_backingMalloc`; false if leased from `m_pagePool`.
        bool isOversizedBlock{ false };
    };

private:
    PageHeader* m_currentPage{ nullptr };
    PageHeader* m_currentOversized{ nullptr };
    PagePool* m_pagePool{ nullptr };
    PagePoolProviderFn m_pagePoolProvider{ nullptr };
    Malloc* m_backingMalloc{ nullptr };
    const char* m_poolName{ "PagedLinearAllocator" };
    AllocatorStats m_stats{};
    gp::UInt32 m_leasedPoolPageCount{ 0u };
    gp::UInt32 m_oversizedBlockCount{ 0u };
    bool m_allowOversizedMalloc{ true };

public:
    /// @brief Constructs an uninitialized elastic paged linear allocator holding zero pages.
    constexpr PagedLinearAllocator() noexcept = default;

    /// @brief Constructs a paged linear allocator bound to an explicit `PagePool`.
    /// @param[in] pagePool Shared lock-free `PagePool` supplying fixed-size pages (may be `nullptr` if oversized
    ///            fallback is enabled).
    /// @param[in] backingMalloc Optional `Malloc` backend for allocations exceeding a single pool page (`nullptr` uses
    ///            `GlobalMemory`).
    /// @param[in] allowOversizedMalloc If true, allocations larger than a pool page fall back to `backingMalloc` and
    ///            are chained into the arena lifetime.
    /// @param[in] poolName Optional static string name for Tracy profiler telemetry (`GP_MEM_ALLOC_N` / `GP_PLOT`).
    explicit PagedLinearAllocator(
        PagePool* pagePool,
        Malloc* backingMalloc = nullptr,
        const bool allowOversizedMalloc = true,
        const char* poolName = "PagedLinearAllocator"
    ) noexcept;

    /// @brief Constructs a paged linear allocator that lazily resolves its `PagePool` via `poolProvider` on first
    ///        allocation.
    /// @param[in] poolProvider Function pointer invoked on the first allocation to obtain the backing `PagePool*`.
    /// @param[in] backingMalloc Optional `Malloc` backend for oversized allocations (`nullptr` uses `GlobalMemory`).
    /// @param[in] allowOversizedMalloc If true, allocations larger than a pool page fall back to `backingMalloc`.
    /// @param[in] poolName Optional static string name for Tracy profiler telemetry.
    explicit PagedLinearAllocator(
        PagePoolProviderFn poolProvider,
        Malloc* backingMalloc = nullptr,
        const bool allowOversizedMalloc = true,
        const char* poolName = "PagedLinearAllocator"
    ) noexcept;

    /// @brief Copying a paged linear allocator is disabled because it uniquely owns its leased page chain.
    PagedLinearAllocator(const PagedLinearAllocator&) = delete;
    PagedLinearAllocator& operator=(const PagedLinearAllocator&) = delete;

    /// @brief Move constructor; transfers the leased page chain and telemetry state from `other`.
    /// @param[in,out] other Source allocator, left in an empty zero-page state.
    PagedLinearAllocator(PagedLinearAllocator&& other) noexcept;

    /// @brief Move assignment operator; releases any currently held pages and steals the page chain from `other`.
    /// @param[in,out] other Source allocator, left in an empty zero-page state.
    /// @return Reference to `*this`.
    PagedLinearAllocator& operator=(PagedLinearAllocator&& other) noexcept;

    /// @brief Destructor; returns all leased pages to `m_pagePool` and frees any oversized blocks to `m_backingMalloc`.
    virtual ~PagedLinearAllocator() noexcept override;

    /// @brief Initializes or re-binds the allocator to a `PagePool`, releasing any previously held pages.
    /// @param[in] pagePool Shared lock-free `PagePool` supplying fixed-size pages.
    /// @param[in] backingMalloc Optional `Malloc` backend for oversized allocations (`nullptr` uses `GlobalMemory`).
    /// @param[in] allowOversizedMalloc If true, allocations exceeding a single pool page fall back to `backingMalloc`.
    /// @param[in] poolName Optional static string name for profiler tracking.
    /// @return True if configured with a valid page source or oversized fallback; false otherwise.
    bool initialize(
        PagePool* pagePool,
        Malloc* backingMalloc = nullptr,
        const bool allowOversizedMalloc = true,
        const char* poolName = "PagedLinearAllocator"
    ) noexcept;

    /// @brief Initializes the allocator with a lazy `PagePoolProviderFn` callback invoked on first allocation.
    /// @param[in] poolProvider Function pointer returning the shared `PagePool*`.
    /// @param[in] backingMalloc Optional `Malloc` backend for oversized allocations (`nullptr` uses `GlobalMemory`).
    /// @param[in] allowOversizedMalloc If true, allocations exceeding a single pool page fall back to `backingMalloc`.
    /// @param[in] poolName Optional static string name for profiler tracking.
    /// @return True if `poolProvider != nullptr` or `allowOversizedMalloc` is enabled; false otherwise.
    bool initialize(
        PagePoolProviderFn poolProvider,
        Malloc* backingMalloc = nullptr,
        const bool allowOversizedMalloc = true,
        const char* poolName = "PagedLinearAllocator"
    ) noexcept;

    /// @brief Returns all leased pages and oversized blocks and resets configuration state to uninitialized.
    void shutdown() noexcept;

    /// @copydoc IAllocator::allocate()
    /// @note Bumps `m_currentPage->usedOffset` in O(1) time with zero atomic operations when space permits; otherwise
    ///       leases a new page from `m_pagePool` (or allocates an oversized block from `m_backingMalloc`).
    [[nodiscard]] virtual void*
        allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment = kDefaultAlignment) noexcept override;

    /// @copydoc IAllocator::reallocate()
    /// @note If `ptr` sits at the top of `m_currentPage` and `newSizeInBytes` fits within `m_currentPage`, resizes the
    ///       allocation in place in O(1) time without copying bytes. Otherwise allocates a new block and copies
    ///       `min(oldSizeInBytes, newSizeInBytes)` bytes.
    [[nodiscard]] virtual void* reallocate(
        void* ptr,
        const gp::USize oldSizeInBytes,
        const gp::USize newSizeInBytes,
        const gp::UInt32 alignment = kDefaultAlignment
    ) noexcept override;

    /// @copydoc IAllocator::deallocate()
    /// @note If `ptr` sits at the exact top of `m_currentPage`, rewinds `m_currentPage->usedOffset` immediately.
    ///       Otherwise acts as a no-op until `rewind()` or `reset()`.
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

    /// @brief Captures a checkpoint of the current active page, bump offset, and oversized block count for `rewind()`.
    /// @return `MemoryMark` encoding the current page pointer (`pageCookie`), byte offset, oversized block count
    ///         (`contextEpoch`), and allocation counters.
    [[nodiscard]] constexpr MemoryMark mark() const noexcept
    {
        return MemoryMark{ .offset = (m_currentPage != nullptr) ? m_currentPage->usedOffset : 0u,
                           .activeAllocations = m_stats.activeAllocations,
                           .allocatedBytes = m_stats.allocatedBytes,
                           .pageCookie = static_cast<void*>(m_currentPage),
                           .contextEpoch = static_cast<gp::UInt64>(m_oversizedBlockCount) };
    }

    /// @brief Unwinds the arena back to `savedMark`, returning all newer pages to `m_pagePool` in a single atomic CAS
    ///        chain release and restoring the target page's bump cursor.
    /// @param[in] savedMark Checkpoint previously returned by `mark()` on this allocator instance.
    void rewind(const MemoryMark& savedMark) noexcept;

    /// @brief Releases all leased pages back to `m_pagePool` (and frees any oversized blocks), returning the arena to a
    ///        0-byte idle footprint in O(1) atomic CAS operations.
    /// @note Preserves `m_stats.peakAllocatedBytes` for high-water mark diagnostics.
    void reset() noexcept;

    /// @brief Retrieves the bound `PagePool` pointer (may be `nullptr` if lazy provider has not yet fired).
    /// @return Pointer to the backing `PagePool`, or `nullptr`.
    [[nodiscard]] constexpr PagePool* getPagePool() const noexcept
    {
        return m_pagePool;
    }

    /// @brief Retrieves the number of fixed-size pool pages currently leased by this arena.
    /// @return Active pool page count (`0` when idle or reset).
    [[nodiscard]] constexpr gp::UInt32 getLeasedPageCount() const noexcept
    {
        return m_leasedPoolPageCount;
    }

    /// @brief Retrieves the number of oversized heap blocks currently chained into this arena.
    /// @return Active oversized block count (`0` when none are allocated).
    [[nodiscard]] constexpr gp::UInt32 getOversizedBlockCount() const noexcept
    {
        return m_oversizedBlockCount;
    }

    /// @brief Retrieves the total number of chained pages/blocks currently held (`getLeasedPageCount() +
    /// getOversizedBlockCount()`).
    /// @return Total chained block count.
    [[nodiscard]] constexpr gp::UInt32 getTotalPageCount() const noexcept
    {
        return m_leasedPoolPageCount + m_oversizedBlockCount;
    }

    /// @brief Retrieves the total payload bytes currently allocated across all chained pages.
    /// @return Active allocated bytes.
    [[nodiscard]] constexpr gp::USize getUsedBytes() const noexcept
    {
        return m_stats.allocatedBytes;
    }

    /// @brief Retrieves the total byte capacity across all currently leased pages and oversized blocks.
    /// @return Current leased capacity in bytes (`0` when holding zero pages).
    [[nodiscard]] constexpr gp::USize getCapacity() const noexcept
    {
        return m_stats.capacityBytes;
    }

    /// @brief Retrieves the unallocated bytes remaining at the tail of the currently active page.
    /// @return Remaining tail bytes in `m_currentPage`, or `0` if no page is currently leased.
    [[nodiscard]] constexpr gp::USize getCurrentPageRemainingBytes() const noexcept
    {
        if (m_currentPage == nullptr || m_currentPage->usedOffset > m_currentPage->capacityBytes)
        {
            return 0u;
        }

        return m_currentPage->capacityBytes - m_currentPage->usedOffset;
    }

    /// @brief Retrieves the current telemetry statistics for this arena.
    /// @return Const reference to `AllocatorStats`.
    [[nodiscard]] constexpr const AllocatorStats& getStats() const noexcept
    {
        return m_stats;
    }

    /// @brief Checks whether this arena is configured with a valid page source or oversized heap fallback.
    /// @return True if capable of servicing allocations; false otherwise.
    [[nodiscard]] bool isValid() const noexcept;

    /// @brief Compares two `PagedLinearAllocator` instances for identity equality.
    /// @param[in] rhs Other `PagedLinearAllocator` instance.
    /// @return True if both references refer to the exact same arena instance.
    [[nodiscard]] constexpr bool operator==(const PagedLinearAllocator& rhs) const noexcept
    {
        return this == &rhs;
    }

private:
    /// @brief Attempts to bump-allocate `sizeInBytes` with `effectiveAlignment` inside `page`.
    /// @param[in,out] page Target page header to bump.
    /// @param[in] sizeInBytes Requested payload size in bytes.
    /// @param[in] effectiveAlignment Power-of-two byte alignment (`>= kMinimumAlignment`).
    /// @return Aligned payload pointer if the allocation fits inside `page`; `nullptr` otherwise.
    [[nodiscard]] void*
        tryAllocateInPage(PageHeader* page, const gp::USize sizeInBytes, const gp::UInt32 effectiveAlignment) noexcept;

    /// @brief Allocates an oversized block directly from `m_backingMalloc` and links it at the top of `m_currentPage`.
    /// @param[in] sizeInBytes Requested payload size in bytes.
    /// @param[in] effectiveAlignment Power-of-two byte alignment (`>= kMinimumAlignment`).
    /// @return Aligned payload pointer, or `nullptr` on failure.
    [[nodiscard]] void*
        allocateOversizedBlock(const gp::USize sizeInBytes, const gp::UInt32 effectiveAlignment) noexcept;

    /// @brief Checks whether `targetPage` is currently present in the `m_currentPage` linked chain.
    /// @param[in] targetPage Page header pointer to search for.
    /// @return True if `targetPage` belongs to the active chain; false otherwise.
    [[nodiscard]] bool isPageInActiveChain(const PageHeader* targetPage) const noexcept;

    /// @brief Unwinds and releases all pool pages from `m_currentPage` down to (but not including) `targetPage`.
    /// @param[in] targetPage Pool page at which to stop unwinding (`nullptr` releases all pool pages).
    void unwindPagesTo(PageHeader* targetPage) noexcept;

    /// @brief Unwinds and frees oversized heap blocks from `m_currentOversized` until `m_oversizedBlockCount` reaches
    ///        `targetOversizedCount`.
    /// @param[in] targetOversizedCount Number of oversized blocks to retain (`0` frees all oversized blocks).
    void unwindOversizedBlocksTo(const gp::UInt32 targetOversizedCount) noexcept;
};

static_assert(
    gp::concepts::IsMarkableAllocator<PagedLinearAllocator>,
    "PagedLinearAllocator must satisfy gp::concepts::IsMarkableAllocator."
);

}   // namespace gp::memory
