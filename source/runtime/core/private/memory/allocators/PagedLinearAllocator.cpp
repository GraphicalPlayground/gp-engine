// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/PagedLinearAllocator.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/allocators/PagePool.hpp"
#include "memory/Memory.hpp"
#include "profiling/Profiler.hpp"
#include <limits>
#include <utility>

namespace gp::memory
{

PagedLinearAllocator::PagedLinearAllocator(
    PagePool* pagePool, Malloc* backingMalloc, const bool allowOversizedMalloc, const char* poolName
) noexcept
{
    (void)initialize(pagePool, backingMalloc, allowOversizedMalloc, poolName);
}

PagedLinearAllocator::PagedLinearAllocator(
    PagePoolProviderFn poolProvider, Malloc* backingMalloc, const bool allowOversizedMalloc, const char* poolName
) noexcept
{
    (void)initialize(poolProvider, backingMalloc, allowOversizedMalloc, poolName);
}

PagedLinearAllocator::PagedLinearAllocator(PagedLinearAllocator&& other) noexcept
    : m_currentPage(std::exchange(other.m_currentPage, nullptr))
    , m_currentOversized(std::exchange(other.m_currentOversized, nullptr))
    , m_pagePool(std::exchange(other.m_pagePool, nullptr))
    , m_pagePoolProvider(std::exchange(other.m_pagePoolProvider, nullptr))
    , m_backingMalloc(std::exchange(other.m_backingMalloc, nullptr))
    , m_poolName(other.m_poolName)
    , m_stats(std::exchange(other.m_stats, AllocatorStats{}))
    , m_leasedPoolPageCount(std::exchange(other.m_leasedPoolPageCount, 0u))
    , m_oversizedBlockCount(std::exchange(other.m_oversizedBlockCount, 0u))
    , m_allowOversizedMalloc(other.m_allowOversizedMalloc)
{}

PagedLinearAllocator& PagedLinearAllocator::operator=(PagedLinearAllocator&& other) noexcept
{
    if (this != &other)
    {
        shutdown();

        m_currentPage = std::exchange(other.m_currentPage, nullptr);
        m_currentOversized = std::exchange(other.m_currentOversized, nullptr);
        m_pagePool = std::exchange(other.m_pagePool, nullptr);
        m_pagePoolProvider = std::exchange(other.m_pagePoolProvider, nullptr);
        m_backingMalloc = std::exchange(other.m_backingMalloc, nullptr);
        m_poolName = other.m_poolName;
        m_stats = std::exchange(other.m_stats, AllocatorStats{});
        m_leasedPoolPageCount = std::exchange(other.m_leasedPoolPageCount, 0u);
        m_oversizedBlockCount = std::exchange(other.m_oversizedBlockCount, 0u);
        m_allowOversizedMalloc = other.m_allowOversizedMalloc;
    }

    return *this;
}

PagedLinearAllocator::~PagedLinearAllocator() noexcept
{
    shutdown();
}

bool PagedLinearAllocator::initialize(
    PagePool* pagePool, Malloc* backingMalloc, const bool allowOversizedMalloc, const char* poolName
) noexcept
{
    shutdown();

    Malloc* resolvedMalloc = backingMalloc;
    if (resolvedMalloc == nullptr && allowOversizedMalloc)
    {
        resolvedMalloc = detail::resolveActiveMalloc();
    }

    m_pagePool = pagePool;
    m_pagePoolProvider = nullptr;
    m_backingMalloc = resolvedMalloc;
    m_poolName = (poolName != nullptr) ? poolName : "PagedLinearAllocator";
    m_allowOversizedMalloc = allowOversizedMalloc;
    m_stats = AllocatorStats{};

    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));

    return isValid();
}

bool PagedLinearAllocator::initialize(
    PagePoolProviderFn poolProvider, Malloc* backingMalloc, const bool allowOversizedMalloc, const char* poolName
) noexcept
{
    shutdown();

    Malloc* resolvedMalloc = backingMalloc;
    if (resolvedMalloc == nullptr && allowOversizedMalloc)
    {
        resolvedMalloc = detail::resolveActiveMalloc();
    }

    m_pagePool = nullptr;
    m_pagePoolProvider = poolProvider;
    m_backingMalloc = resolvedMalloc;
    m_poolName = (poolName != nullptr) ? poolName : "PagedLinearAllocator";
    m_allowOversizedMalloc = allowOversizedMalloc;
    m_stats = AllocatorStats{};

    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));

    return isValid();
}

void PagedLinearAllocator::shutdown() noexcept
{
    unwindOversizedBlocksTo(0u);
    unwindPagesTo(nullptr);

    m_currentPage = nullptr;
    m_currentOversized = nullptr;
    m_pagePool = nullptr;
    m_pagePoolProvider = nullptr;
    m_backingMalloc = nullptr;
    m_leasedPoolPageCount = 0u;
    m_oversizedBlockCount = 0u;
    m_allowOversizedMalloc = false;
    m_stats = AllocatorStats{};

    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));
}

void* PagedLinearAllocator::allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment) noexcept
{
    if (sizeInBytes == 0u) [[unlikely]]
    {
        return nullptr;
    }

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(alignment, kMinimumAlignment);
    GP_ASSERT(
        math::isPowerOfTwo(effectiveAlignment), "PagedLinearAllocator::allocate requires a power-of-two byte alignment."
    );

    // 1. Hot path: non-atomic pointer bump inside the currently active page.
    if (m_currentPage != nullptr) [[likely]]
    {
        if (void* const fastResult = tryAllocateInPage(m_currentPage, sizeInBytes, effectiveAlignment);
            fastResult != nullptr) [[likely]]
        {
            return fastResult;
        }
    }

    // 2. Lazily resolve `m_pagePool` on first page miss if configured with a provider callback.
    if ((m_pagePool == nullptr || !m_pagePool->isValid()) && m_pagePoolProvider != nullptr)
    {
        m_pagePool = m_pagePoolProvider();
    }

    // 3. Lease a new fixed-size page from `m_pagePool` if the request can fit inside a standard page.
    if (m_pagePool != nullptr && m_pagePool->isValid())
    {
        const gp::USize poolPageSize = m_pagePool->getPageSize();
        if (poolPageSize > sizeof(PageHeader) && sizeInBytes <= (poolPageSize - sizeof(PageHeader)))
        {
            void* const rawPage = m_pagePool->acquirePage();
            if (rawPage != nullptr) [[likely]]
            {
                PageHeader* const newPage = static_cast<PageHeader*>(rawPage);
                newPage->prevPage = m_currentPage;
                newPage->capacityBytes = poolPageSize;
                newPage->usedOffset = sizeof(PageHeader);
                newPage->isOversizedBlock = false;

                if (void* const pageResult = tryAllocateInPage(newPage, sizeInBytes, effectiveAlignment);
                    pageResult != nullptr) [[likely]]
                {
                    m_currentPage = newPage;
                    ++m_leasedPoolPageCount;
                    m_stats.capacityBytes += poolPageSize;
                    return pageResult;
                }

                // Alignment padding exceeded `poolPageSize`; return the page to the pool and fall through to
                // oversized allocation.
                m_pagePool->releasePage(rawPage);
            }
        }
    }

    // 4. Fallback path: allocate an oversized block from `m_backingMalloc` and chain it into the page list.
    if (m_allowOversizedMalloc)
    {
        return allocateOversizedBlock(sizeInBytes, effectiveAlignment);
    }

    return nullptr;
}

void* PagedLinearAllocator::reallocate(
    void* ptr, const gp::USize oldSizeInBytes, const gp::USize newSizeInBytes, const gp::UInt32 alignment
) noexcept
{
    if (ptr == nullptr)
    {
        return allocate(newSizeInBytes, alignment);
    }

    if (newSizeInBytes == 0u)
    {
        deallocate(ptr, oldSizeInBytes, alignment);
        return nullptr;
    }

    if (m_currentPage == nullptr && m_currentOversized == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(alignment, kMinimumAlignment);
    gp::UInt8* const bytePtr = static_cast<gp::UInt8*>(ptr);

    // Deconstructive fast-path: If `ptr` is the most recent allocation at the top of `m_currentPage` and remains
    // aligned, resize it in place in O(1) time without copying bytes or leasing another page.
    if (m_currentPage != nullptr) [[likely]]
    {
        gp::UInt8* const currentPageStart = reinterpret_cast<gp::UInt8*>(m_currentPage);
        if (bytePtr >= (currentPageStart + sizeof(PageHeader)) &&
            bytePtr < (currentPageStart + m_currentPage->capacityBytes))
        {
            const gp::USize ptrOffset = static_cast<gp::USize>(bytePtr - currentPageStart);
            if ((ptrOffset + oldSizeInBytes == m_currentPage->usedOffset) &&
                memory::isAligned(ptr, static_cast<gp::UInt64>(effectiveAlignment)))
            {
                if (newSizeInBytes <= (m_currentPage->capacityBytes - ptrOffset)) [[likely]]
                {
                    m_currentPage->usedOffset = ptrOffset + newSizeInBytes;
                    if (newSizeInBytes >= oldSizeInBytes)
                    {
                        m_stats.allocatedBytes += (newSizeInBytes - oldSizeInBytes);
                    }
                    else
                    {
                        const gp::USize delta = oldSizeInBytes - newSizeInBytes;
                        m_stats.allocatedBytes =
                            m_stats.allocatedBytes >= delta ? (m_stats.allocatedBytes - delta) : 0u;
                    }

                    m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_stats.allocatedBytes);
                    GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
                    return ptr;
                }
            }
        }
    }

    if (!owns(ptr)) [[unlikely]]
    {
        return nullptr;
    }

    // If shrinking a non-top (or oversized) allocation and alignment is already satisfied, keep the block in place.
    if (newSizeInBytes <= oldSizeInBytes && memory::isAligned(ptr, static_cast<gp::UInt64>(effectiveAlignment)))
    {
        const gp::USize delta = oldSizeInBytes - newSizeInBytes;
        m_stats.allocatedBytes = m_stats.allocatedBytes >= delta ? (m_stats.allocatedBytes - delta) : 0u;
        GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
        return ptr;
    }

    PageHeader* const previousTopOversized = m_currentOversized;
    void* const newPtr = allocate(newSizeInBytes, effectiveAlignment);
    if (newPtr != nullptr) [[likely]]
    {
        if (oldSizeInBytes > 0u)
        {
            memory::copyMemory(newPtr, ptr, math::min(oldSizeInBytes, newSizeInBytes));
            m_stats.allocatedBytes =
                m_stats.allocatedBytes >= oldSizeInBytes ? (m_stats.allocatedBytes - oldSizeInBytes) : 0u;
        }

        if (m_stats.activeAllocations > 0ull)
        {
            --m_stats.activeAllocations;
        }

        // If `ptr` belonged to the previous top oversized block, unlink and free that oversized block immediately.
        if (previousTopOversized != nullptr)
        {
            const gp::UInt8* const oversizedStart = reinterpret_cast<const gp::UInt8*>(previousTopOversized);
            if (bytePtr >= (oversizedStart + sizeof(PageHeader)) &&
                bytePtr < (oversizedStart + previousTopOversized->capacityBytes))
            {
                if (m_currentOversized == previousTopOversized)
                {
                    m_currentOversized = previousTopOversized->prevPage;
                }
                else if (m_currentOversized != nullptr && m_currentOversized->prevPage == previousTopOversized)
                {
                    m_currentOversized->prevPage = previousTopOversized->prevPage;
                }

                m_stats.capacityBytes = m_stats.capacityBytes >= previousTopOversized->capacityBytes
                                            ? (m_stats.capacityBytes - previousTopOversized->capacityBytes)
                                            : 0u;
                if (m_backingMalloc != nullptr)
                {
                    GP_MEM_FREE_N(previousTopOversized, m_poolName);
                    m_backingMalloc->deallocate(previousTopOversized);
                }
                if (m_oversizedBlockCount > 0u)
                {
                    --m_oversizedBlockCount;
                }
            }
        }

        GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
    }

    return newPtr;
}

void PagedLinearAllocator::deallocate(void* ptr, const gp::USize oldSizeInBytes, const gp::UInt32 alignment) noexcept
{
    (void)alignment;

    if (ptr == nullptr || (m_currentPage == nullptr && m_currentOversized == nullptr) || !owns(ptr))
    {
        return;
    }

    ++m_stats.totalDeallocations;
    if (m_stats.activeAllocations > 0ull)
    {
        --m_stats.activeAllocations;
    }

    gp::UInt8* const bytePtr = static_cast<gp::UInt8*>(ptr);
    if (m_currentPage != nullptr)
    {
        gp::UInt8* const currentPageStart = reinterpret_cast<gp::UInt8*>(m_currentPage);
        if (bytePtr >= (currentPageStart + sizeof(PageHeader)) &&
            bytePtr < (currentPageStart + m_currentPage->capacityBytes))
        {
            const gp::USize ptrOffset = static_cast<gp::USize>(bytePtr - currentPageStart);
            if (oldSizeInBytes > 0u && (ptrOffset + oldSizeInBytes == m_currentPage->usedOffset))
            {
                m_currentPage->usedOffset = ptrOffset;
                m_stats.allocatedBytes =
                    m_stats.allocatedBytes >= oldSizeInBytes ? (m_stats.allocatedBytes - oldSizeInBytes) : 0u;
                GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
            }
            return;
        }
    }

    // If `ptr` sits at the top of the oversized block chain, reclaim the heap block immediately.
    if (m_currentOversized != nullptr)
    {
        gp::UInt8* const oversizedStart = reinterpret_cast<gp::UInt8*>(m_currentOversized);
        if (bytePtr >= (oversizedStart + sizeof(PageHeader)) &&
            bytePtr < (oversizedStart + m_currentOversized->capacityBytes))
        {
            m_stats.allocatedBytes =
                m_stats.allocatedBytes >= oldSizeInBytes ? (m_stats.allocatedBytes - oldSizeInBytes) : 0u;
            unwindOversizedBlocksTo(m_oversizedBlockCount - 1u);
            GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
        }
    }
}

gp::USize PagedLinearAllocator::getActualAllocationSize(
    const gp::USize requestedSize, const gp::UInt32 alignment
) const noexcept
{
    (void)alignment;
    return requestedSize;
}

bool PagedLinearAllocator::owns(const void* ptr) const noexcept
{
    if (ptr == nullptr || (m_currentPage == nullptr && m_currentOversized == nullptr))
    {
        return false;
    }

    const gp::UInt8* const bytePtr = static_cast<const gp::UInt8*>(ptr);

    // O(1) range check against the backing PagePool before walking `m_currentPage`
    if (m_currentPage != nullptr && (m_pagePool == nullptr || m_pagePool->owns(ptr)))
    {
        for (const PageHeader* page = m_currentPage; page != nullptr; page = page->prevPage)
        {
            const gp::UInt8* const pageStart = reinterpret_cast<const gp::UInt8*>(page);
            if (bytePtr >= (pageStart + sizeof(PageHeader)) && bytePtr < (pageStart + page->usedOffset))
            {
                return true;
            }
        }
    }

    for (const PageHeader* oversized = m_currentOversized; oversized != nullptr; oversized = oversized->prevPage)
    {
        const gp::UInt8* const blockStart = reinterpret_cast<const gp::UInt8*>(oversized);
        if (bytePtr >= (blockStart + sizeof(PageHeader)) && bytePtr < (blockStart + oversized->usedOffset))
        {
            return true;
        }
    }

    return false;
}

const char* PagedLinearAllocator::getName() const noexcept
{
    return m_poolName;
}

void PagedLinearAllocator::rewind(const MemoryMark& savedMark) noexcept
{
    unwindOversizedBlocksTo(static_cast<gp::UInt32>(savedMark.contextEpoch));

    if (savedMark.pageCookie == nullptr)
    {
        unwindPagesTo(nullptr);
        m_stats.activeAllocations = savedMark.activeAllocations;
        m_stats.allocatedBytes = savedMark.allocatedBytes;
        GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
        return;
    }

    PageHeader* const targetPage = static_cast<PageHeader*>(savedMark.pageCookie);
    if (targetPage != m_currentPage && !isPageInActiveChain(targetPage)) [[unlikely]]
    {
        GP_ASSERT(
            false,
            "PagedLinearAllocator::rewind called with a MemoryMark whose pageCookie is not in this allocator's active " "page chain."
        );
        return;
    }

    unwindPagesTo(targetPage);

    const bool isOffsetValid = (m_currentPage != nullptr) && (savedMark.offset >= sizeof(PageHeader)) &&
                               (savedMark.offset <= m_currentPage->usedOffset);
    GP_ASSERT(
        isOffsetValid,
        "PagedLinearAllocator::rewind called with an out-of-bounds or out-of-order byte offset on the target page."
    );
    if (isOffsetValid) [[likely]]
    {
        m_currentPage->usedOffset = savedMark.offset;
        m_stats.activeAllocations = savedMark.activeAllocations;
        m_stats.allocatedBytes = savedMark.allocatedBytes;
        GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
    }
}

void PagedLinearAllocator::reset() noexcept
{
    unwindOversizedBlocksTo(0u);
    unwindPagesTo(nullptr);
    m_stats.allocatedBytes = 0u;
    m_stats.activeAllocations = 0ull;
    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));
}

void* PagedLinearAllocator::tryAllocateInPage(
    PageHeader* page, const gp::USize sizeInBytes, const gp::UInt32 effectiveAlignment
) noexcept
{
    const gp::UInt64 baseAddress = reinterpret_cast<gp::UInt64>(page);
    const gp::UInt64 currentAddress = baseAddress + static_cast<gp::UInt64>(page->usedOffset);
    const gp::UInt64 alignedAddress = memory::align(currentAddress, static_cast<gp::UInt64>(effectiveAlignment));
    const gp::USize alignedOffset = static_cast<gp::USize>(alignedAddress - baseAddress);

    if (alignedOffset > page->capacityBytes || sizeInBytes > (page->capacityBytes - alignedOffset))
    {
        return nullptr;
    }

    page->usedOffset = alignedOffset + sizeInBytes;
    m_stats.allocatedBytes += sizeInBytes;
    m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_stats.allocatedBytes);
    ++m_stats.activeAllocations;
    ++m_stats.totalAllocations;

    GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));

    return reinterpret_cast<void*>(alignedAddress);
}

void* PagedLinearAllocator::allocateOversizedBlock(
    const gp::USize sizeInBytes, const gp::UInt32 effectiveAlignment
) noexcept
{
    if (m_backingMalloc == nullptr)
    {
        m_backingMalloc = detail::resolveActiveMalloc();
        if (m_backingMalloc == nullptr) [[unlikely]]
        {
            return nullptr;
        }
    }

    const gp::UInt32 blockAlignment =
        math::max<gp::UInt32>(effectiveAlignment, static_cast<gp::UInt32>(alignof(PageHeader)));
    const gp::USize headerReservation =
        math::max(static_cast<gp::USize>(sizeof(PageHeader)), static_cast<gp::USize>(blockAlignment));
    if (sizeInBytes > (std::numeric_limits<gp::USize>::max() - headerReservation)) [[unlikely]]
    {
        return nullptr;
    }

    const gp::USize totalBlockBytes = headerReservation + sizeInBytes;
    void* const rawBlock = m_backingMalloc->tryAllocate(totalBlockBytes, blockAlignment);
    if (rawBlock == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    PageHeader* const oversizedHeader = static_cast<PageHeader*>(rawBlock);
    oversizedHeader->prevPage = m_currentOversized;
    oversizedHeader->capacityBytes = totalBlockBytes;
    oversizedHeader->usedOffset = sizeof(PageHeader);
    oversizedHeader->isOversizedBlock = true;

    GP_MEM_ALLOC_N(rawBlock, totalBlockBytes, m_poolName);

    void* const resultPtr = tryAllocateInPage(oversizedHeader, sizeInBytes, effectiveAlignment);
    GP_ASSERT(resultPtr != nullptr, "PagedLinearAllocator::allocateOversizedBlock failed to align oversized payload.");

    m_currentOversized = oversizedHeader;
    ++m_oversizedBlockCount;
    m_stats.capacityBytes += totalBlockBytes;

    return resultPtr;
}

bool PagedLinearAllocator::isPageInActiveChain(const PageHeader* targetPage) const noexcept
{
    for (const PageHeader* page = m_currentPage; page != nullptr; page = page->prevPage)
    {
        if (page == targetPage)
        {
            return true;
        }
    }

    return false;
}

void PagedLinearAllocator::unwindPagesTo(PageHeader* targetPage) noexcept
{
    void* batchFirstPoolPage = nullptr;
    void* batchLastPoolPage = nullptr;
    gp::UInt32 batchPoolPageCount = 0u;

    while (m_currentPage != nullptr && m_currentPage != targetPage)
    {
        PageHeader* const pageToFree = m_currentPage;
        m_currentPage = pageToFree->prevPage;

        const gp::USize pageCapacity = pageToFree->capacityBytes;
        if (m_stats.capacityBytes >= pageCapacity)
        {
            m_stats.capacityBytes -= pageCapacity;
        }
        else
        {
            m_stats.capacityBytes = 0u;
        }

        if (m_pagePool != nullptr) [[likely]]
        {
            if (m_leasedPoolPageCount > 0u)
            {
                --m_leasedPoolPageCount;
            }

            if (batchFirstPoolPage == nullptr)
            {
                batchFirstPoolPage = static_cast<void*>(pageToFree);
                batchLastPoolPage = static_cast<void*>(pageToFree);
            }
            else
            {
                PagePool::linkPageForChainRelease(batchLastPoolPage, m_pagePool->getPageIndex(pageToFree));
                batchLastPoolPage = static_cast<void*>(pageToFree);
            }

            ++batchPoolPageCount;
        }
    }

    if (batchPoolPageCount > 0u && m_pagePool != nullptr)
    {
        m_pagePool->releasePageChain(batchFirstPoolPage, batchLastPoolPage, batchPoolPageCount);
    }
}

void PagedLinearAllocator::unwindOversizedBlocksTo(const gp::UInt32 targetOversizedCount) noexcept
{
    while (m_currentOversized != nullptr && m_oversizedBlockCount > targetOversizedCount)
    {
        PageHeader* const blockToFree = m_currentOversized;
        m_currentOversized = blockToFree->prevPage;

        const gp::USize blockCapacity = blockToFree->capacityBytes;
        if (m_stats.capacityBytes >= blockCapacity)
        {
            m_stats.capacityBytes -= blockCapacity;
        }
        else
        {
            m_stats.capacityBytes = 0u;
        }

        if (m_backingMalloc != nullptr) [[likely]]
        {
            GP_MEM_FREE_N(blockToFree, m_poolName);
            m_backingMalloc->deallocate(blockToFree);
        }

        --m_oversizedBlockCount;
    }
}

bool PagedLinearAllocator::isValid() const noexcept
{
    return (m_pagePool != nullptr && m_pagePool->isValid()) || (m_pagePoolProvider != nullptr) ||
           (m_allowOversizedMalloc && m_backingMalloc != nullptr);
}

}   // namespace gp::memory
