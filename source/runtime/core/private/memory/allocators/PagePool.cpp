// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/PagePool.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include "memory/MemoryOrder.hpp"
#include "profiling/Profiler.hpp"
#include <limits>

namespace gp::memory
{

PagePool::PagePool(
    const gp::USize pageSizeInBytes, const gp::UInt32 pageCount, Malloc* backingMalloc, const char* poolName
) noexcept
{
    (void)initialize(pageSizeInBytes, pageCount, backingMalloc, poolName);
}

PagePool::PagePool(
    void* externalBuffer, const gp::USize capacityInBytes, const gp::USize pageSizeInBytes, const char* poolName
) noexcept
{
    (void)initialize(externalBuffer, capacityInBytes, pageSizeInBytes, poolName);
}

PagePool::~PagePool() noexcept
{
    shutdown();
}

bool PagePool::initialize(
    const gp::USize pageSizeInBytes, const gp::UInt32 pageCount, Malloc* backingMalloc, const char* poolName
) noexcept
{
    shutdown();

    if (pageSizeInBytes == 0u || pageCount == 0u || pageCount == kInvalidPageIndex) [[unlikely]]
    {
        return false;
    }

    const gp::USize minPageSize = math::max(pageSizeInBytes, static_cast<gp::USize>(kPageAlignment));
    const gp::USize alignedPageSize = static_cast<gp::USize>(
        memory::align(static_cast<gp::UInt64>(minPageSize), static_cast<gp::UInt64>(kPageAlignment))
    );
    if (alignedPageSize < minPageSize ||
        static_cast<gp::USize>(pageCount) > (std::numeric_limits<gp::USize>::max() / alignedPageSize)) [[unlikely]]
    {
        return false;
    }

    const gp::USize totalBytes = alignedPageSize * static_cast<gp::USize>(pageCount);

    Malloc* const resolvedMalloc = (backingMalloc != nullptr) ? backingMalloc : detail::resolveActiveMalloc();
    if (resolvedMalloc == nullptr) [[unlikely]]
    {
        return false;
    }

    void* const allocatedSlab = resolvedMalloc->tryAllocate(totalBytes, kPageAlignment);
    if (allocatedSlab == nullptr) [[unlikely]]
    {
        return false;
    }

    m_rawBuffer = static_cast<gp::UInt8*>(allocatedSlab);
    m_alignedStart = m_rawBuffer;
    m_backingMalloc = resolvedMalloc;
    m_poolName = (poolName != nullptr) ? poolName : "PagePool";
    m_rawCapacity = totalBytes;
    m_pageSize = alignedPageSize;
    m_pageCount = pageCount;
    m_ownsBuffer = true;

    const gp::UInt32 previousTag = unpackTag(m_freeStackHead.value.load(MemoryOrder::Relaxed));
    m_freeStackHead.value.store(packHead(kInvalidPageIndex, previousTag + 1u), MemoryOrder::Release);
    m_carvedPageCount.value.store(0u, MemoryOrder::Release);
    m_leasedPageCount.value.store(0u, MemoryOrder::Release);
    m_peakLeasedPageCount.value.store(0u, MemoryOrder::Release);

    GP_MEM_ALLOC_N(m_rawBuffer, m_rawCapacity, m_poolName);
    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));

    return true;
}

bool PagePool::initialize(
    void* externalBuffer, const gp::USize capacityInBytes, const gp::USize pageSizeInBytes, const char* poolName
) noexcept
{
    shutdown();

    if (externalBuffer == nullptr || capacityInBytes == 0u || pageSizeInBytes == 0u) [[unlikely]]
    {
        return false;
    }

    const gp::USize minPageSize = math::max(pageSizeInBytes, static_cast<gp::USize>(kPageAlignment));
    const gp::USize alignedPageSize = static_cast<gp::USize>(
        memory::align(static_cast<gp::UInt64>(minPageSize), static_cast<gp::UInt64>(kPageAlignment))
    );

    gp::UInt8* const rawStart = static_cast<gp::UInt8*>(externalBuffer);
    gp::UInt8* const alignedStart = memory::align(rawStart, static_cast<gp::UInt64>(kPageAlignment));
    const gp::USize alignmentAdjustment = static_cast<gp::USize>(alignedStart - rawStart);
    if (alignmentAdjustment >= capacityInBytes) [[unlikely]]
    {
        return false;
    }

    const gp::USize usableBytes = capacityInBytes - alignmentAdjustment;
    const gp::USize rawPageCount = usableBytes / alignedPageSize;
    if (rawPageCount == 0u || rawPageCount >= static_cast<gp::USize>(kInvalidPageIndex)) [[unlikely]]
    {
        return false;
    }

    m_rawBuffer = rawStart;
    m_alignedStart = alignedStart;
    m_backingMalloc = nullptr;
    m_poolName = (poolName != nullptr) ? poolName : "PagePool.External";
    m_rawCapacity = capacityInBytes;
    m_pageSize = alignedPageSize;
    m_pageCount = static_cast<gp::UInt32>(rawPageCount);
    m_ownsBuffer = false;

    const gp::UInt32 previousTag = unpackTag(m_freeStackHead.value.load(MemoryOrder::Relaxed));
    m_freeStackHead.value.store(packHead(kInvalidPageIndex, previousTag + 1u), MemoryOrder::Release);
    m_carvedPageCount.value.store(0u, MemoryOrder::Release);
    m_leasedPageCount.value.store(0u, MemoryOrder::Release);
    m_peakLeasedPageCount.value.store(0u, MemoryOrder::Release);

    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));

    return true;
}

void PagePool::shutdown() noexcept
{
    if (m_rawBuffer != nullptr)
    {
        if (m_ownsBuffer && m_backingMalloc != nullptr)
        {
            GP_MEM_FREE_N(m_rawBuffer, m_poolName);
            m_backingMalloc->deallocate(m_rawBuffer);
        }

        GP_PLOT(m_poolName, static_cast<gp::Int64>(0));
    }

    m_rawBuffer = nullptr;
    m_alignedStart = nullptr;
    m_backingMalloc = nullptr;
    m_rawCapacity = 0u;
    m_pageSize = 0u;
    m_pageCount = 0u;
    m_ownsBuffer = false;

    const gp::UInt32 previousTag = unpackTag(m_freeStackHead.value.load(MemoryOrder::Relaxed));
    m_freeStackHead.value.store(packHead(kInvalidPageIndex, previousTag + 1u), MemoryOrder::Release);
    m_carvedPageCount.value.store(0u, MemoryOrder::Release);
    m_leasedPageCount.value.store(0u, MemoryOrder::Release);
    m_peakLeasedPageCount.value.store(0u, MemoryOrder::Release);
}

void* PagePool::acquirePage() noexcept
{
    if (!isValid()) [[unlikely]]
    {
        return nullptr;
    }

    // 1. Fast path: pop a recycled page from the ABA-tagged lock-free Treiber stack.
    if (void* const recycledPage = tryPopFreeStack(); recycledPage != nullptr)
    {
        recordPageAcquired();
        return recycledPage;
    }

    // 2. Cold/carving path: lazily carve an untouched page from the slab watermark without overshooting `m_pageCount`.
    gp::UInt32 currentCarved = m_carvedPageCount.value.load(MemoryOrder::Relaxed);
    while (currentCarved < m_pageCount)
    {
        if (m_carvedPageCount.value.compareExchangeWeak(
                currentCarved, currentCarved + 1u, MemoryOrder::Acquire, MemoryOrder::Relaxed
            ))
        {
            void* const carvedPage =
                static_cast<void*>(m_alignedStart + (static_cast<gp::USize>(currentCarved) * m_pageSize));
            recordPageAcquired();
            return carvedPage;
        }
    }

    // 3. Final race check: if all pages were carved, another worker thread may have returned a page to the free stack
    //    between step 1 and step 2.
    if (void* const lateRecycledPage = tryPopFreeStack(); lateRecycledPage != nullptr)
    {
        recordPageAcquired();
        return lateRecycledPage;
    }

    return nullptr;
}

void PagePool::releasePage(void* pagePtr) noexcept
{
    if (pagePtr == nullptr)
    {
        return;
    }

    releasePageChain(pagePtr, pagePtr, 1u);
}

void PagePool::releasePageChain(void* firstPagePtr, void* lastPagePtr, const gp::UInt32 chainPageCount) noexcept
{
    if (firstPagePtr == nullptr || lastPagePtr == nullptr || chainPageCount == 0u || !isValid()) [[unlikely]]
    {
        return;
    }

    const bool isFirstValid = ownsPage(firstPagePtr);
    const bool isLastValid = ownsPage(lastPagePtr);
    const bool isChainTopologyConsistent =
        (chainPageCount == 1u) ? (firstPagePtr == lastPagePtr) : (firstPagePtr != lastPagePtr);
    GP_ASSERT(
        isFirstValid && isLastValid && isChainTopologyConsistent,
        "PagePool::releasePageChain called with invalid page boundaries or inconsistent chain length."
    );
    if (!isFirstValid || !isLastValid || !isChainTopologyConsistent) [[unlikely]]
    {
        return;
    }

    const gp::UInt32 firstIndex = getPageIndex(firstPagePtr);
    gp::UInt32* const lastPageNextSlot = static_cast<gp::UInt32*>(lastPagePtr);

    gp::UInt64 expectedHead = m_freeStackHead.value.load(MemoryOrder::Relaxed);
    for (;;)
    {
        const gp::UInt32 currentHeadIndex = unpackIndex(expectedHead);
        const gp::UInt32 currentTag = unpackTag(expectedHead);

        GP_ASSERT(
            firstIndex != currentHeadIndex,
            "PagePool::releasePageChain detected an immediate double-release at the head of the free stack."
        );
        if (firstIndex == currentHeadIndex) [[unlikely]]
        {
            return;
        }

        *lastPageNextSlot = currentHeadIndex;
        const gp::UInt64 desiredHead = packHead(firstIndex, currentTag + 1u);

        if (m_freeStackHead.value.compareExchangeWeak(
                expectedHead, desiredHead, MemoryOrder::Release, MemoryOrder::Relaxed
            ))
        {
            break;
        }
    }

    const gp::UInt32 previousLeased = m_leasedPageCount.value.fetchSub(chainPageCount, MemoryOrder::Relaxed);
    GP_ASSERT(
        previousLeased >= chainPageCount,
        "PagePool::releasePageChain released more pages than were actively leased from this pool."
    );
    const gp::UInt32 newLeased = (previousLeased >= chainPageCount) ? (previousLeased - chainPageCount) : 0u;
    GP_PLOT(m_poolName, static_cast<gp::Int64>(static_cast<gp::USize>(newLeased) * m_pageSize));
}

void PagePool::reset() noexcept
{
    if (!isValid())
    {
        return;
    }

    const gp::UInt32 previousTag = unpackTag(m_freeStackHead.value.load(MemoryOrder::Relaxed));
    m_freeStackHead.value.store(packHead(kInvalidPageIndex, previousTag + 1u), MemoryOrder::Release);
    m_carvedPageCount.value.store(0u, MemoryOrder::Release);
    m_leasedPageCount.value.store(0u, MemoryOrder::Release);

    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));
}

bool PagePool::owns(const void* ptr) const noexcept
{
    if (!isValid() || ptr == nullptr)
    {
        return false;
    }

    const gp::UIntPtr address = reinterpret_cast<gp::UIntPtr>(ptr);
    const gp::UIntPtr startAddress = reinterpret_cast<gp::UIntPtr>(m_alignedStart);
    const gp::UIntPtr endAddress = startAddress + getCapacity();

    return address >= startAddress && address < endAddress;
}

bool PagePool::ownsPage(const void* pagePtr) const noexcept
{
    if (!owns(pagePtr))
    {
        return false;
    }

    const gp::UIntPtr byteOffset =
        reinterpret_cast<gp::UIntPtr>(pagePtr) - reinterpret_cast<gp::UIntPtr>(m_alignedStart);
    return (byteOffset % m_pageSize) == 0u;
}

gp::UInt32 PagePool::getPageIndex(const void* pagePtr) const noexcept
{
    if (!ownsPage(pagePtr)) [[unlikely]]
    {
        return kInvalidPageIndex;
    }

    const gp::UIntPtr byteOffset =
        reinterpret_cast<gp::UIntPtr>(pagePtr) - reinterpret_cast<gp::UIntPtr>(m_alignedStart);
    return static_cast<gp::UInt32>(byteOffset / m_pageSize);
}

void* PagePool::tryPopFreeStack() noexcept
{
    gp::UInt64 expectedHead = m_freeStackHead.value.load(MemoryOrder::Acquire);
    for (;;)
    {
        const gp::UInt32 headIndex = unpackIndex(expectedHead);
        if (headIndex == kInvalidPageIndex)
        {
            return nullptr;
        }

        const gp::UInt32 currentTag = unpackTag(expectedHead);
        gp::UInt8* const pagePtr = m_alignedStart + (static_cast<gp::USize>(headIndex) * m_pageSize);
        const gp::UInt32 nextIndex = *reinterpret_cast<const gp::UInt32*>(pagePtr);
        const gp::UInt64 desiredHead = packHead(nextIndex, currentTag + 1u);

        if (m_freeStackHead.value.compareExchangeWeak(
                expectedHead, desiredHead, MemoryOrder::Acquire, MemoryOrder::Acquire
            ))
        {
            return static_cast<void*>(pagePtr);
        }
    }
}

void PagePool::recordPageAcquired() noexcept
{
    const gp::UInt32 newLeased = m_leasedPageCount.value.fetchAdd(1u, MemoryOrder::Relaxed) + 1u;

    gp::UInt32 observedPeak = m_peakLeasedPageCount.value.load(MemoryOrder::Relaxed);
    while (newLeased > observedPeak)
    {
        if (m_peakLeasedPageCount.value.compareExchangeWeak(
                observedPeak, newLeased, MemoryOrder::Relaxed, MemoryOrder::Relaxed
            ))
        {
            break;
        }
    }

    GP_PLOT(m_poolName, static_cast<gp::Int64>(static_cast<gp::USize>(newLeased) * m_pageSize));
}

}   // namespace gp::memory
