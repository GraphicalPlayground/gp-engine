// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/MemoryBase.hpp"   // IWYU pragma: keep
#include "memory/MemoryForward.hpp"
#include "threading/atomics/CacheAlignedAtomic.hpp"

namespace gp::memory
{

/// @brief Lock-free, ABA-safe fixed-size memory page pool for elastic linear and fiber scratchpad arenas.
/// @details Inspired by Naughty Dog's `TaggedHeap` page supplier (GDC 2015) and Unreal Engine's `FPageAllocator`.
///          Manages a contiguous memory slab partitioned into uniform pages of `m_pageSize` bytes (default 64 KiB,
///          aligned to 64-byte hardware cache lines).
///          Unallocated pages are tracked via a 64-bit ABA-tagged Treiber stack (`m_freeStackHead`) paired with a
///          cache-line-isolated lazy carving watermark (`m_carvedPageCount`), guaranteeing O(1) initialization,
///          zero external metadata overhead, and single-CAS bulk page-chain reclamation (`releasePageChain`).
class GP_CORE_API PagePool final
{
public:
    /// @brief Default size in bytes of each individual memory page (64 KiB).
    static constexpr gp::USize kDefaultPageSizeBytes = 64u * 1024u;

    /// @brief Default number of pages allocated when initializing a pool with default arguments (64 pages = 4 MiB).
    static constexpr gp::UInt32 kDefaultPageCount = 64u;

    /// @brief Minimum and default byte alignment enforced for every page boundary (64-byte cache line).
    static constexpr gp::UInt32 kPageAlignment = static_cast<gp::UInt32>(HardwareConstants::cacheLineSize);

    /// @brief Sentinel index indicating the end of the intrusive page free stack.
    static constexpr gp::UInt32 kInvalidPageIndex = 0xFFFFFFFFu;

private:
    gp::UInt8* m_rawBuffer{ nullptr };
    gp::UInt8* m_alignedStart{ nullptr };
    Malloc* m_backingMalloc{ nullptr };
    const char* m_poolName{ "PagePool" };
    gp::USize m_rawCapacity{ 0u };
    gp::USize m_pageSize{ 0u };
    gp::UInt32 m_pageCount{ 0u };
    bool m_ownsBuffer{ false };

    /// @brief Cache-line-isolated 64-bit ABA-tagged Treiber stack head (`{ headPageIndex, abaTag }`).
    gp::thread::CacheAlignedAtomic<gp::UInt64> m_freeStackHead{ packHead(kInvalidPageIndex, 0u) };

    /// @brief Cache-line-isolated lazy bump carving watermark (`[0 .. m_pageCount]`).
    gp::thread::CacheAlignedAtomic<gp::UInt32> m_carvedPageCount{ 0u };

    /// @brief Cache-line-isolated count of currently leased pages for telemetry.
    gp::thread::CacheAlignedAtomic<gp::UInt32> m_leasedPageCount{ 0u };

    /// @brief Cache-line-isolated historical peak count of simultaneously leased pages.
    gp::thread::CacheAlignedAtomic<gp::UInt32> m_peakLeasedPageCount{ 0u };

public:
    /// @brief Constructs an uninitialized page pool with zero capacity.
    constexpr PagePool() noexcept = default;

    /// @brief Constructs a page pool that allocates its contiguous backing slab from a `Malloc` backend.
    /// @param[in] pageSizeInBytes Size in bytes of each page (rounded up to a multiple of `kPageAlignment`).
    /// @param[in] pageCount Total number of pages to provision in the pool.
    /// @param[in] backingMalloc Optional `Malloc` backend used to allocate the slab (`nullptr` uses `GlobalMemory`).
    /// @param[in] poolName Optional static string name for Tracy profiler telemetry (`GP_MEM_ALLOC_N` / `GP_PLOT`).
    PagePool(
        const gp::USize pageSizeInBytes,
        const gp::UInt32 pageCount,
        Malloc* backingMalloc = nullptr,
        const char* poolName = "PagePool"
    ) noexcept;

    /// @brief Constructs a page pool operating over a caller-owned external memory buffer (zero heap allocations).
    /// @param[in] externalBuffer Pointer to a caller-managed contiguous buffer.
    /// @param[in] capacityInBytes Total size in bytes of `externalBuffer`.
    /// @param[in] pageSizeInBytes Size in bytes of each page (rounded up to a multiple of `kPageAlignment`).
    /// @param[in] poolName Optional static string name for Tracy profiler plot telemetry (`GP_PLOT`).
    PagePool(
        void* externalBuffer,
        const gp::USize capacityInBytes,
        const gp::USize pageSizeInBytes = kDefaultPageSizeBytes,
        const char* poolName = "PagePool.External"
    ) noexcept;

    /// @brief Copying or moving a shared lock-free page pool is forbidden to prevent concurrent dangling references.
    PagePool(const PagePool&) = delete;
    PagePool& operator=(const PagePool&) = delete;
    PagePool(PagePool&&) = delete;
    PagePool& operator=(PagePool&&) = delete;

    /// @brief Destructor; releases the backing slab to `m_backingMalloc` if owned by this instance.
    ~PagePool() noexcept;

    /// @brief Initializes or re-initializes the page pool with a newly allocated slab from `backingMalloc`.
    /// @param[in] pageSizeInBytes Size in bytes of each page (rounded up to a multiple of `kPageAlignment`).
    /// @param[in] pageCount Total number of pages to allocate.
    /// @param[in] backingMalloc Optional `Malloc` backend (`nullptr` uses `GlobalMemory`).
    /// @param[in] poolName Optional static string name for profiler tracking.
    /// @return True if the backing slab was successfully allocated; false on invalid arguments or out-of-memory.
    bool initialize(
        const gp::USize pageSizeInBytes = kDefaultPageSizeBytes,
        const gp::UInt32 pageCount = kDefaultPageCount,
        Malloc* backingMalloc = nullptr,
        const char* poolName = "PagePool"
    ) noexcept;

    /// @brief Initializes or re-initializes the page pool over an external caller-owned buffer.
    /// @param[in] externalBuffer Pointer to the external memory block.
    /// @param[in] capacityInBytes Size in bytes of `externalBuffer`.
    /// @param[in] pageSizeInBytes Size in bytes of each page (rounded up to a multiple of `kPageAlignment`).
    /// @param[in] poolName Optional static string name for profiler tracking.
    /// @return True if `externalBuffer` has sufficient aligned space for at least one page; false otherwise.
    bool initialize(
        void* externalBuffer,
        const gp::USize capacityInBytes,
        const gp::USize pageSizeInBytes = kDefaultPageSizeBytes,
        const char* poolName = "PagePool.External"
    ) noexcept;

    /// @brief Releases the backing slab (if owned) and resets all pool state to empty.
    /// @note Not thread-safe with concurrent `acquirePage()` or `releasePage()` calls; invoke during subsystem
    /// shutdown.
    void shutdown() noexcept;

    /// @brief Leases a single cache-line-aligned page from the pool in lock-free O(1) time.
    /// @details Attempts to pop a recycled page from the ABA-tagged Treiber stack (`m_freeStackHead`) first; if empty,
    ///          atomically carves a fresh page from `m_carvedPageCount` without paging in untouched memory.
    /// @return Pointer to a `getPageSize()`-byte aligned memory page, or `nullptr` if all pages are currently leased.
    [[nodiscard]] void* acquirePage() noexcept;

    /// @brief Returns a single previously leased page back to the lock-free pool in O(1) time.
    /// @param[in] pagePtr Base address of the page to return (safe no-op if `nullptr`).
    void releasePage(void* pagePtr) noexcept;

    /// @brief Splices a pre-linked chain of pages back onto the lock-free free stack in a single atomic CAS loop.
    /// @details Used by `PagedLinearAllocator::rewind()` and `reset()` to return multiple pages in O(1) atomic
    ///          operations after linking intermediate pages via `linkPageForChainRelease()`.
    /// @param[in] firstPagePtr First page at the head of the chain being released.
    /// @param[in] lastPagePtr Last page at the tail of the chain being released (its next-index slot will be linked to
    ///            the old stack head).
    /// @param[in] chainPageCount Exact number of pages in `[firstPagePtr .. lastPagePtr]`.
    void releasePageChain(void* firstPagePtr, void* lastPagePtr, const gp::UInt32 chainPageCount) noexcept;

    /// @brief Writes `nextPageIndex` into the intrusive 32-bit free-list slot at the start of `pagePtr`.
    /// @note Only call on a page that is about to be passed to `releasePageChain()` after its active payload/header is
    ///       no longer needed.
    /// @param[in,out] pagePtr Page whose intrusive next-index slot will be written.
    /// @param[in] nextPageIndex Pool index of the next page in the release chain (`getPageIndex(nextPage)`).
    static void linkPageForChainRelease(void* pagePtr, const gp::UInt32 nextPageIndex) noexcept
    {
        if (pagePtr != nullptr) [[likely]]
        {
            *static_cast<gp::UInt32*>(pagePtr) = nextPageIndex;
        }
    }

    /// @brief Reclaims all pages in the pool in O(1) time by resetting the free stack and lazy carving watermark.
    /// @note Preserves `getPeakLeasedPageCount()` for high-water mark diagnostics. Caller must ensure no external
    ///       arenas continue referencing previously leased pages.
    void reset() noexcept;

    /// @brief Retrieves the usable byte size of each individual page in the pool.
    /// @return Page size in bytes (multiple of `kPageAlignment`).
    [[nodiscard]] constexpr gp::USize getPageSize() const noexcept
    {
        return m_pageSize;
    }

    /// @brief Retrieves the total number of pages managed by the pool.
    /// @return Total page count.
    [[nodiscard]] constexpr gp::UInt32 getPageCount() const noexcept
    {
        return m_pageCount;
    }

    /// @brief Retrieves the total usable capacity of the pool in bytes (`getPageSize() * getPageCount()`).
    /// @return Total pool capacity in bytes.
    [[nodiscard]] constexpr gp::USize getCapacity() const noexcept
    {
        return m_pageSize * static_cast<gp::USize>(m_pageCount);
    }

    /// @brief Retrieves the number of pages currently leased out to arenas.
    /// @return Active leased page count.
    [[nodiscard]] gp::UInt32 getLeasedPageCount() const noexcept
    {
        return m_leasedPageCount.value.load(MemoryOrder::Relaxed);
    }

    /// @brief Retrieves the number of available pages remaining in the pool.
    /// @return Available page count.
    [[nodiscard]] gp::UInt32 getAvailablePageCount() const noexcept
    {
        const gp::UInt32 leased = getLeasedPageCount();
        return m_pageCount >= leased ? (m_pageCount - leased) : 0u;
    }

    /// @brief Retrieves the historical high-water mark of simultaneously leased pages.
    /// @return Peak leased page count.
    [[nodiscard]] gp::UInt32 getPeakLeasedPageCount() const noexcept
    {
        return m_peakLeasedPageCount.value.load(MemoryOrder::Relaxed);
    }

    /// @brief Checks whether `ptr` lies anywhere within the pool's managed page slab.
    /// @param[in] ptr Memory address to test.
    /// @return True if `ptr` falls within `[m_alignedStart, m_alignedStart + getCapacity())`.
    [[nodiscard]] bool owns(const void* ptr) const noexcept;

    /// @brief Checks whether `pagePtr` is the exact base address of a valid page in this pool.
    /// @param[in] pagePtr Candidate page base pointer to test.
    /// @return True if `owns(pagePtr)` is true and `pagePtr` is aligned to a `getPageSize()` stride from
    /// `m_alignedStart`.
    [[nodiscard]] bool ownsPage(const void* pagePtr) const noexcept;

    /// @brief Computes the zero-based pool index `[0 .. getPageCount())` for a valid `pagePtr`.
    /// @param[in] pagePtr Base address of a page owned by this pool.
    /// @return Page index, or `kInvalidPageIndex` if `!ownsPage(pagePtr)`.
    [[nodiscard]] gp::UInt32 getPageIndex(const void* pagePtr) const noexcept;

    /// @brief Checks whether the page pool is initialized with at least one valid page.
    /// @return True if initialized and valid; false otherwise.
    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return m_alignedStart != nullptr && m_pageSize > 0u && m_pageCount > 0u;
    }

    /// @brief Retrieves the human-readable profiler name associated with this page pool.
    /// @return Null-terminated static string name.
    [[nodiscard]] constexpr const char* getName() const noexcept
    {
        return m_poolName;
    }

private:
    /// @brief Packs a 32-bit page index and 32-bit ABA generation tag into a single 64-bit atomic word.
    /// @param[in] pageIndex Index of the page at the head of the free stack (`kInvalidPageIndex` if empty).
    /// @param[in] abaTag Monotonically incrementing generation counter preventing ABA races.
    /// @return Bit-packed 64-bit value.
    [[nodiscard]] static constexpr gp::UInt64 packHead(const gp::UInt32 pageIndex, const gp::UInt32 abaTag) noexcept
    {
        return (static_cast<gp::UInt64>(abaTag) << 32u) | static_cast<gp::UInt64>(pageIndex);
    }

    /// @brief Extracts the 32-bit page index from a packed 64-bit stack head word.
    /// @param[in] packedHead Bit-packed 64-bit value.
    /// @return 32-bit page index.
    [[nodiscard]] static constexpr gp::UInt32 unpackIndex(const gp::UInt64 packedHead) noexcept
    {
        return static_cast<gp::UInt32>(packedHead & 0xFFFFFFFFull);
    }

    /// @brief Extracts the 32-bit ABA generation tag from a packed 64-bit stack head word.
    /// @param[in] packedHead Bit-packed 64-bit value.
    /// @return 32-bit ABA generation tag.
    [[nodiscard]] static constexpr gp::UInt32 unpackTag(const gp::UInt64 packedHead) noexcept
    {
        return static_cast<gp::UInt32>(packedHead >> 32u);
    }

    /// @brief Attempts to pop a single recycled page from `m_freeStackHead`.
    /// @return Pointer to the popped page, or `nullptr` if the recycled stack was empty.
    [[nodiscard]] void* tryPopFreeStack() noexcept;

    /// @brief Updates `m_leasedPageCount` and `m_peakLeasedPageCount` when a page is acquired.
    void recordPageAcquired() noexcept;
};

}   // namespace gp::memory
