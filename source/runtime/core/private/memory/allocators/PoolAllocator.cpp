// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/PoolAllocator.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/Memory.hpp"   // IWYU pragma: keep
#include "profiling/Profiler.hpp"
#include <utility>

namespace gp::memory
{

PoolAllocator::PoolAllocator(
    const gp::USize blockSize,
    const gp::USize blockCount,
    const gp::UInt32 blockAlignment,
    Malloc* backingMalloc,
    const char* poolName
) noexcept
{
    (void)initialize(blockSize, blockCount, blockAlignment, backingMalloc, poolName);
}

PoolAllocator::PoolAllocator(
    void* externalBuffer,
    const gp::USize capacityInBytes,
    const gp::USize blockSize,
    const gp::UInt32 blockAlignment,
    const char* poolName
) noexcept
{
    (void)initialize(externalBuffer, capacityInBytes, blockSize, blockAlignment, poolName);
}

PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
    : m_rawBuffer(std::exchange(other.m_rawBuffer, nullptr))
    , m_alignedStart(std::exchange(other.m_alignedStart, nullptr))
    , m_freeListHead(std::exchange(other.m_freeListHead, nullptr))
    , m_backingMalloc(std::exchange(other.m_backingMalloc, nullptr))
    , m_poolName(other.m_poolName)
    , m_rawCapacity(std::exchange(other.m_rawCapacity, 0u))
    , m_stride(std::exchange(other.m_stride, 0u))
    , m_blockCount(std::exchange(other.m_blockCount, 0u))
    , m_usedBlockCount(std::exchange(other.m_usedBlockCount, 0u))
    , m_blockAlignment(other.m_blockAlignment)
    , m_stats(std::exchange(other.m_stats, AllocatorStats{}))
    , m_ownsBuffer(std::exchange(other.m_ownsBuffer, false))
{}

PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept
{
    if (this != &other)
    {
        shutdown();

        m_rawBuffer = std::exchange(other.m_rawBuffer, nullptr);
        m_alignedStart = std::exchange(other.m_alignedStart, nullptr);
        m_freeListHead = std::exchange(other.m_freeListHead, nullptr);
        m_backingMalloc = std::exchange(other.m_backingMalloc, nullptr);
        m_poolName = other.m_poolName;
        m_rawCapacity = std::exchange(other.m_rawCapacity, 0u);
        m_stride = std::exchange(other.m_stride, 0u);
        m_blockCount = std::exchange(other.m_blockCount, 0u);
        m_usedBlockCount = std::exchange(other.m_usedBlockCount, 0u);
        m_blockAlignment = other.m_blockAlignment;
        m_stats = std::exchange(other.m_stats, AllocatorStats{});
        m_ownsBuffer = std::exchange(other.m_ownsBuffer, false);
    }

    return *this;
}

PoolAllocator::~PoolAllocator() noexcept
{
    shutdown();
}

bool PoolAllocator::initialize(
    const gp::USize blockSize,
    const gp::USize blockCount,
    const gp::UInt32 blockAlignment,
    Malloc* backingMalloc,
    const char* poolName
) noexcept
{
    shutdown();

    if (blockSize == 0u || blockCount == 0u) [[unlikely]]
    {
        return false;
    }

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, blockAlignment);
    GP_ASSERT(
        math::isPowerOfTwo(effectiveAlignment), "PoolAllocator::initialize requires a power-of-two blockAlignment."
    );

    const gp::USize minBlockSize = math::max(blockSize, static_cast<gp::USize>(sizeof(FreeBlock)));
    const gp::USize stride = static_cast<gp::USize>(
        memory::align(static_cast<gp::UInt64>(minBlockSize), static_cast<gp::UInt64>(effectiveAlignment))
    );
    const gp::USize totalBytes = stride * blockCount;

    Malloc* resolvedMalloc = (backingMalloc != nullptr) ? backingMalloc : detail::resolveActiveMalloc();
    if (resolvedMalloc == nullptr) [[unlikely]]
    {
        return false;
    }

    void* allocatedSlab = resolvedMalloc->tryAllocate(totalBytes, effectiveAlignment);
    if (allocatedSlab == nullptr) [[unlikely]]
    {
        return false;
    }

    m_rawBuffer = static_cast<gp::UInt8*>(allocatedSlab);
    m_alignedStart = m_rawBuffer;
    m_backingMalloc = resolvedMalloc;
    m_poolName = (poolName != nullptr) ? poolName : "PoolAllocator";
    m_rawCapacity = totalBytes;
    m_stride = stride;
    m_blockCount = blockCount;
    m_usedBlockCount = 0u;
    m_blockAlignment = effectiveAlignment;
    m_stats = AllocatorStats{};
    m_stats.capacityBytes = totalBytes;
    m_ownsBuffer = true;

    rebuildFreeList();

    GP_MEM_ALLOC_N(m_rawBuffer, m_rawCapacity, m_poolName);
    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));

    return true;
}

bool PoolAllocator::initialize(
    void* externalBuffer,
    const gp::USize capacityInBytes,
    const gp::USize blockSize,
    const gp::UInt32 blockAlignment,
    const char* poolName
) noexcept
{
    shutdown();

    if (externalBuffer == nullptr || capacityInBytes == 0u || blockSize == 0u) [[unlikely]]
    {
        return false;
    }

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, blockAlignment);
    GP_ASSERT(
        math::isPowerOfTwo(effectiveAlignment), "PoolAllocator::initialize requires a power-of-two blockAlignment."
    );

    const gp::USize minBlockSize = math::max(blockSize, static_cast<gp::USize>(sizeof(FreeBlock)));
    const gp::USize stride = static_cast<gp::USize>(
        memory::align(static_cast<gp::UInt64>(minBlockSize), static_cast<gp::UInt64>(effectiveAlignment))
    );

    gp::UInt8* rawStart = static_cast<gp::UInt8*>(externalBuffer);
    gp::UInt8* alignedStart = memory::align(rawStart, static_cast<gp::UInt64>(effectiveAlignment));
    const gp::USize alignmentAdjustment = static_cast<gp::USize>(alignedStart - rawStart);
    if (alignmentAdjustment >= capacityInBytes) [[unlikely]]
    {
        return false;
    }

    const gp::USize usableBytes = capacityInBytes - alignmentAdjustment;
    const gp::USize blockCount = usableBytes / stride;
    if (blockCount == 0u) [[unlikely]]
    {
        return false;
    }

    m_rawBuffer = rawStart;
    m_alignedStart = alignedStart;
    m_backingMalloc = nullptr;
    m_poolName = (poolName != nullptr) ? poolName : "PoolAllocator.External";
    m_rawCapacity = capacityInBytes;
    m_stride = stride;
    m_blockCount = blockCount;
    m_usedBlockCount = 0u;
    m_blockAlignment = effectiveAlignment;
    m_stats = AllocatorStats{};
    m_stats.capacityBytes = blockCount * stride;
    m_ownsBuffer = false;

    rebuildFreeList();

    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));

    return true;
}

void PoolAllocator::shutdown() noexcept
{
    if (m_rawBuffer != nullptr && m_ownsBuffer && m_backingMalloc != nullptr)
    {
        GP_MEM_FREE_N(m_rawBuffer, m_poolName);
        m_backingMalloc->deallocate(m_rawBuffer);
    }

    m_rawBuffer = nullptr;
    m_alignedStart = nullptr;
    m_freeListHead = nullptr;
    m_backingMalloc = nullptr;
    m_rawCapacity = 0u;
    m_stride = 0u;
    m_blockCount = 0u;
    m_usedBlockCount = 0u;
    m_stats = AllocatorStats{};
    m_ownsBuffer = false;
}

void* PoolAllocator::allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment) noexcept
{
    if (!isValid() || sizeInBytes == 0u || m_freeListHead == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, alignment);
    if (sizeInBytes > m_stride || effectiveAlignment > m_blockAlignment) [[unlikely]]
    {
        return nullptr;
    }

    FreeBlock* block = m_freeListHead;
    m_freeListHead = block->next;

    ++m_usedBlockCount;
    m_stats.allocatedBytes = m_usedBlockCount * m_stride;
    m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_stats.allocatedBytes);
    ++m_stats.activeAllocations;
    ++m_stats.totalAllocations;

    GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));

    return static_cast<void*>(block);
}

void* PoolAllocator::reallocate(
    void* ptr, const gp::USize oldSizeInBytes, const gp::USize newSizeInBytes, const gp::UInt32 alignment
) noexcept
{
    if (newSizeInBytes == 0u)
    {
        deallocate(ptr, oldSizeInBytes, alignment);
        return nullptr;
    }

    if (ptr == nullptr)
    {
        return allocate(newSizeInBytes, alignment);
    }

    if (!owns(ptr)) [[unlikely]]
    {
        return nullptr;
    }

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, alignment);
    if (newSizeInBytes <= m_stride && effectiveAlignment <= m_blockAlignment)
    {
        return ptr;
    }

    return nullptr;
}

void PoolAllocator::deallocate(
    void* ptr, const gp::USize /*oldSizeInBytes*/, const gp::UInt32 /*alignment*/
) noexcept
{
    if (ptr == nullptr)
    {
        return;
    }

    GP_ASSERT(owns(ptr), "PoolAllocator::deallocate called with a pointer outside this pool's buffer.");

    const gp::UIntPtr byteOffset = reinterpret_cast<gp::UIntPtr>(ptr) - reinterpret_cast<gp::UIntPtr>(m_alignedStart);
    GP_ASSERT((byteOffset % m_stride) == 0u, "PoolAllocator::deallocate called with a misaligned block pointer.");
    (void)byteOffset;

    FreeBlock* block = static_cast<FreeBlock*>(ptr);
    block->next = m_freeListHead;
    m_freeListHead = block;

    if (m_usedBlockCount > 0u)
    {
        --m_usedBlockCount;
    }
    m_stats.allocatedBytes = m_usedBlockCount * m_stride;
    if (m_stats.activeAllocations > 0ull)
    {
        --m_stats.activeAllocations;
    }
    ++m_stats.totalDeallocations;

    GP_PLOT(m_poolName, static_cast<gp::Int64>(m_stats.allocatedBytes));
}

gp::USize
    PoolAllocator::getActualAllocationSize(const gp::USize requestedSize, const gp::UInt32 alignment) const noexcept
{
    if (!isValid() || requestedSize == 0u)
    {
        return 0u;
    }

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(kMinimumAlignment, alignment);
    if (requestedSize > m_stride || effectiveAlignment > m_blockAlignment)
    {
        return 0u;
    }

    return m_stride;
}

bool PoolAllocator::owns(const void* ptr) const noexcept
{
    if (!isValid() || ptr == nullptr)
    {
        return false;
    }

    const gp::UIntPtr address = reinterpret_cast<gp::UIntPtr>(ptr);
    const gp::UIntPtr startAddress = reinterpret_cast<gp::UIntPtr>(m_alignedStart);
    const gp::UIntPtr endAddress = startAddress + (m_blockCount * m_stride);

    return address >= startAddress && address < endAddress;
}

const char* PoolAllocator::getName() const noexcept
{
    return m_poolName;
}

void PoolAllocator::reset() noexcept
{
    if (!isValid())
    {
        return;
    }

    rebuildFreeList();
    m_usedBlockCount = 0u;
    m_stats.allocatedBytes = 0u;
    m_stats.activeAllocations = 0ull;

    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));
}

void PoolAllocator::rebuildFreeList() noexcept
{
    if (m_alignedStart == nullptr || m_blockCount == 0u || m_stride == 0u)
    {
        m_freeListHead = nullptr;
        return;
    }

    m_freeListHead = reinterpret_cast<FreeBlock*>(m_alignedStart);
    FreeBlock* current = m_freeListHead;

    for (gp::USize index = 1u; index < m_blockCount; ++index)
    {
        FreeBlock* nextBlock = reinterpret_cast<FreeBlock*>(m_alignedStart + (index * m_stride));
        current->next = nextBlock;
        current = nextBlock;
    }

    current->next = nullptr;
}

}   // namespace gp::memory
