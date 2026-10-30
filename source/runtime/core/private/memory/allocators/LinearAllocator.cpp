// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/LinearAllocator.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/Memory.hpp"
#include "profiling/Profiler.hpp"
#include <utility>

namespace gp::memory
{

LinearAllocator::LinearAllocator(const gp::USize capacityInBytes, Malloc* backingMalloc, const char* poolName) noexcept
{
    (void)initialize(capacityInBytes, backingMalloc, poolName);
}

LinearAllocator::LinearAllocator(void* externalBuffer, const gp::USize capacityInBytes, const char* poolName) noexcept
{
    (void)initialize(externalBuffer, capacityInBytes, poolName);
}

LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
    : m_buffer(std::exchange(other.m_buffer, nullptr))
    , m_backingMalloc(std::exchange(other.m_backingMalloc, nullptr))
    , m_poolName(other.m_poolName)
    , m_capacity(std::exchange(other.m_capacity, 0u))
    , m_offset(std::exchange(other.m_offset, 0u))
    , m_stats(std::exchange(other.m_stats, AllocatorStats{}))
    , m_ownsBuffer(std::exchange(other.m_ownsBuffer, false))
{}

LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept
{
    if (this != &other)
    {
        shutdown();

        m_buffer = std::exchange(other.m_buffer, nullptr);
        m_backingMalloc = std::exchange(other.m_backingMalloc, nullptr);
        m_poolName = other.m_poolName;
        m_capacity = std::exchange(other.m_capacity, 0u);
        m_offset = std::exchange(other.m_offset, 0u);
        m_stats = std::exchange(other.m_stats, AllocatorStats{});
        m_ownsBuffer = std::exchange(other.m_ownsBuffer, false);
    }

    return *this;
}

LinearAllocator::~LinearAllocator() noexcept
{
    shutdown();
}

bool LinearAllocator::initialize(const gp::USize capacityInBytes, Malloc* backingMalloc, const char* poolName) noexcept
{
    shutdown();

    if (capacityInBytes == 0u) [[unlikely]]
    {
        return false;
    }

    Malloc* resolvedMalloc = backingMalloc != nullptr ? backingMalloc : detail::resolveActiveMalloc();
    if (resolvedMalloc == nullptr) [[unlikely]]
    {
        return false;
    }

    const gp::UInt32 slabAlignment = static_cast<gp::UInt32>(HardwareConstants::cacheLineSize);
    void* rawBuffer = resolvedMalloc->tryAllocate(capacityInBytes, slabAlignment);
    if (rawBuffer == nullptr) [[unlikely]]
    {
        return false;
    }

    m_buffer = static_cast<gp::UInt8*>(rawBuffer);
    m_backingMalloc = resolvedMalloc;
    m_poolName = poolName != nullptr ? poolName : "LinearAllocator";
    m_capacity = capacityInBytes;
    m_offset = 0u;
    m_ownsBuffer = true;
    m_stats = AllocatorStats{};
    m_stats.capacityBytes = capacityInBytes;

    GP_MEM_ALLOC_N(m_buffer, m_capacity, m_poolName);
    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));

    return true;
}

bool LinearAllocator::initialize(void* externalBuffer, const gp::USize capacityInBytes, const char* poolName) noexcept
{
    shutdown();

    if (externalBuffer == nullptr || capacityInBytes == 0u) [[unlikely]]
    {
        return false;
    }

    m_buffer = static_cast<gp::UInt8*>(externalBuffer);
    m_backingMalloc = nullptr;
    m_poolName = poolName != nullptr ? poolName : "LinearAllocator.External";
    m_capacity = capacityInBytes;
    m_offset = 0u;
    m_ownsBuffer = false;
    m_stats = AllocatorStats{};
    m_stats.capacityBytes = capacityInBytes;

    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));

    return true;
}

void LinearAllocator::shutdown() noexcept
{
    if (m_buffer != nullptr)
    {
        if (m_ownsBuffer && m_backingMalloc != nullptr)
        {
            GP_MEM_FREE_N(m_buffer, m_poolName);
            m_backingMalloc->deallocate(m_buffer);
        }

        GP_PLOT(m_poolName, static_cast<gp::Int64>(0));
    }

    m_buffer = nullptr;
    m_backingMalloc = nullptr;
    m_capacity = 0u;
    m_offset = 0u;
    m_stats = AllocatorStats{};
    m_ownsBuffer = false;
}

void* LinearAllocator::allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment) noexcept
{
    if (sizeInBytes == 0u || m_buffer == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(alignment, kMinimumAlignment);
    const gp::UInt64 baseAddress = reinterpret_cast<gp::UInt64>(m_buffer);
    const gp::UInt64 currentAddress = baseAddress + static_cast<gp::UInt64>(m_offset);
    const gp::UInt64 alignedAddress = memory::align(currentAddress, static_cast<gp::UInt64>(effectiveAlignment));
    const gp::USize alignedOffset = static_cast<gp::USize>(alignedAddress - baseAddress);

    if (alignedOffset > m_capacity || sizeInBytes > (m_capacity - alignedOffset)) [[unlikely]]
    {
        return nullptr;
    }

    m_offset = alignedOffset + sizeInBytes;
    m_stats.allocatedBytes += sizeInBytes;
    m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_offset);
    ++m_stats.activeAllocations;
    ++m_stats.totalAllocations;

    GP_PLOT(m_poolName, static_cast<gp::Int64>(m_offset));

    return reinterpret_cast<void*>(alignedAddress);
}

void* LinearAllocator::reallocate(
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

    if (!owns(ptr)) [[unlikely]]
    {
        return nullptr;
    }

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(alignment, kMinimumAlignment);
    gp::UInt8* const bytePtr = static_cast<gp::UInt8*>(ptr);
    const gp::USize ptrOffset = static_cast<gp::USize>(bytePtr - m_buffer);

    // Deconstructive fast-path: If `ptr` is the most recent allocation at the top of the bump cursor and already
    // satisfies the requested alignment, adjust `m_offset` in place in O(1) time without copying or stranding bytes.
    if ((ptrOffset + oldSizeInBytes == m_offset) && memory::isAligned(ptr, static_cast<gp::UInt64>(effectiveAlignment)))
    {
        if (newSizeInBytes <= (m_capacity - ptrOffset)) [[likely]]
        {
            m_offset = ptrOffset + newSizeInBytes;
            if (newSizeInBytes >= oldSizeInBytes)
            {
                m_stats.allocatedBytes += (newSizeInBytes - oldSizeInBytes);
            }
            else
            {
                const gp::USize delta = oldSizeInBytes - newSizeInBytes;
                m_stats.allocatedBytes = m_stats.allocatedBytes >= delta ? (m_stats.allocatedBytes - delta) : 0u;
            }

            m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_offset);
            GP_PLOT(m_poolName, static_cast<gp::Int64>(m_offset));
            return ptr;
        }

        return nullptr;
    }

    // If shrinking a non-top allocation and alignment is already satisfied, keep the existing block in place.
    if (newSizeInBytes <= oldSizeInBytes && memory::isAligned(ptr, static_cast<gp::UInt64>(effectiveAlignment)))
    {
        return ptr;
    }

    void* const newPtr = allocate(newSizeInBytes, effectiveAlignment);
    if (newPtr != nullptr && oldSizeInBytes > 0u) [[likely]]
    {
        memory::copyMemory(newPtr, ptr, math::min(oldSizeInBytes, newSizeInBytes));
    }

    return newPtr;
}

void LinearAllocator::deallocate(void* ptr, const gp::USize oldSizeInBytes, const gp::UInt32 alignment) noexcept
{
    (void)alignment;

    if (ptr == nullptr || !owns(ptr))
    {
        return;
    }

    ++m_stats.totalDeallocations;
    if (m_stats.activeAllocations > 0ull)
    {
        --m_stats.activeAllocations;
    }

    gp::UInt8* const bytePtr = static_cast<gp::UInt8*>(ptr);
    const gp::USize ptrOffset = static_cast<gp::USize>(bytePtr - m_buffer);

    // Reclaim immediately if this block sits at the exact top of the bump cursor.
    if (oldSizeInBytes > 0u && (ptrOffset + oldSizeInBytes == m_offset))
    {
        m_offset = ptrOffset;
        m_stats.allocatedBytes =
            m_stats.allocatedBytes >= oldSizeInBytes ? (m_stats.allocatedBytes - oldSizeInBytes) : 0u;
        GP_PLOT(m_poolName, static_cast<gp::Int64>(m_offset));
    }
}

gp::USize
    LinearAllocator::getActualAllocationSize(const gp::USize requestedSize, const gp::UInt32 alignment) const noexcept
{
    (void)alignment;
    return requestedSize;
}

bool LinearAllocator::owns(const void* ptr) const noexcept
{
    if (ptr == nullptr || m_buffer == nullptr)
    {
        return false;
    }

    const gp::UInt8* const bytePtr = static_cast<const gp::UInt8*>(ptr);
    return bytePtr >= m_buffer && bytePtr < (m_buffer + m_capacity);
}

const char* LinearAllocator::getName() const noexcept
{
    return m_poolName;
}

void LinearAllocator::rewind(const MemoryMark& savedMark) noexcept
{
    if (savedMark.offset <= m_offset) [[likely]]
    {
        m_offset = savedMark.offset;
        m_stats.activeAllocations = savedMark.activeAllocations;
        m_stats.allocatedBytes = savedMark.allocatedBytes;
        GP_PLOT(m_poolName, static_cast<gp::Int64>(m_offset));
    }
}

void LinearAllocator::reset() noexcept
{
    m_offset = 0u;
    m_stats.allocatedBytes = 0u;
    m_stats.activeAllocations = 0ull;
    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));
}

}   // namespace gp::memory
