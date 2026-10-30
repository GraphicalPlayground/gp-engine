// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/allocators/StackAllocator.hpp"
#include "maths/base/Scalar.hpp"
#include "memory/allocators/HeapAllocator.hpp"
#include "memory/HardwareConstants.hpp"
#include "memory/Memory.hpp"
#include "profiling/Profiler.hpp"
#include <limits>
#include <utility>

namespace gp::memory
{

StackAllocator::StackAllocator(const gp::USize capacityInBytes, Malloc* backingMalloc, const char* poolName) noexcept
{
    (void)initialize(capacityInBytes, backingMalloc, poolName);
}

StackAllocator::StackAllocator(void* externalBuffer, const gp::USize capacityInBytes, const char* poolName) noexcept
{
    (void)initialize(externalBuffer, capacityInBytes, poolName);
}

StackAllocator::StackAllocator(StackAllocator&& other) noexcept
    : m_buffer(std::exchange(other.m_buffer, nullptr))
    , m_backingMalloc(std::exchange(other.m_backingMalloc, nullptr))
    , m_poolName(other.m_poolName)
    , m_capacity(std::exchange(other.m_capacity, 0u))
    , m_offset(std::exchange(other.m_offset, 0u))
    , m_stats(std::exchange(other.m_stats, AllocatorStats{}))
    , m_ownsBuffer(std::exchange(other.m_ownsBuffer, false))
{}

StackAllocator& StackAllocator::operator=(StackAllocator&& other) noexcept
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

StackAllocator::~StackAllocator() noexcept
{
    shutdown();
}

bool StackAllocator::initialize(const gp::USize capacityInBytes, Malloc* backingMalloc, const char* poolName) noexcept
{
    shutdown();

    if (capacityInBytes <= sizeof(Header) ||
        capacityInBytes > static_cast<gp::USize>(std::numeric_limits<gp::UInt32>::max())) [[unlikely]]
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
    m_poolName = poolName != nullptr ? poolName : "StackAllocator";
    m_capacity = capacityInBytes;
    m_offset = 0u;
    m_ownsBuffer = true;
    m_stats = AllocatorStats{};
    m_stats.capacityBytes = capacityInBytes;

    GP_MEM_ALLOC_N(m_buffer, m_capacity, m_poolName);
    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));

    return true;
}

bool StackAllocator::initialize(void* externalBuffer, const gp::USize capacityInBytes, const char* poolName) noexcept
{
    shutdown();

    if (externalBuffer == nullptr || capacityInBytes <= sizeof(Header) ||
        capacityInBytes > static_cast<gp::USize>(std::numeric_limits<gp::UInt32>::max())) [[unlikely]]
    {
        return false;
    }

    m_buffer = static_cast<gp::UInt8*>(externalBuffer);
    m_backingMalloc = nullptr;
    m_poolName = poolName != nullptr ? poolName : "StackAllocator.External";
    m_capacity = capacityInBytes;
    m_offset = 0u;
    m_ownsBuffer = false;
    m_stats = AllocatorStats{};
    m_stats.capacityBytes = capacityInBytes;

    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));

    return true;
}

void StackAllocator::shutdown() noexcept
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

void* StackAllocator::allocate(const gp::USize sizeInBytes, const gp::UInt32 alignment) noexcept
{
    if (sizeInBytes == 0u || m_buffer == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(alignment, static_cast<gp::UInt32>(alignof(Header)));
    const gp::UInt64 baseAddress = reinterpret_cast<gp::UInt64>(m_buffer);
    const gp::UInt64 unalignedPayloadAddress = baseAddress + static_cast<gp::UInt64>(m_offset) + sizeof(Header);
    const gp::UInt64 alignedPayloadAddress =
        memory::align(unalignedPayloadAddress, static_cast<gp::UInt64>(effectiveAlignment));
    const gp::USize payloadOffset = static_cast<gp::USize>(alignedPayloadAddress - baseAddress);

    if (payloadOffset > m_capacity || sizeInBytes > (m_capacity - payloadOffset)) [[unlikely]]
    {
        return nullptr;
    }

    Header* const header = reinterpret_cast<Header*>(alignedPayloadAddress - sizeof(Header));
    header->previousOffset = static_cast<gp::UInt32>(m_offset);
    header->allocationSize = static_cast<gp::UInt32>(sizeInBytes);

    m_offset = payloadOffset + sizeInBytes;
    m_stats.allocatedBytes += sizeInBytes;
    m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_offset);
    ++m_stats.activeAllocations;
    ++m_stats.totalAllocations;

    GP_PLOT(m_poolName, static_cast<gp::Int64>(m_offset));

    return reinterpret_cast<void*>(alignedPayloadAddress);
}

void* StackAllocator::reallocate(
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

    const gp::UInt32 effectiveAlignment = math::max<gp::UInt32>(alignment, static_cast<gp::UInt32>(alignof(Header)));
    gp::UInt8* const bytePtr = static_cast<gp::UInt8*>(ptr);
    const gp::USize payloadOffset = static_cast<gp::USize>(bytePtr - m_buffer);
    Header* const header = reinterpret_cast<Header*>(bytePtr - sizeof(Header));
    const gp::USize recordedOldSize = static_cast<gp::USize>(header->allocationSize);

    // Top-of-stack fast-path: resize in place if `ptr` is the active top allocation and satisfies alignment.
    if ((payloadOffset + recordedOldSize == m_offset) &&
        memory::isAligned(ptr, static_cast<gp::UInt64>(effectiveAlignment)))
    {
        if (newSizeInBytes <= (m_capacity - payloadOffset)) [[likely]]
        {
            header->allocationSize = static_cast<gp::UInt32>(newSizeInBytes);
            m_offset = payloadOffset + newSizeInBytes;

            if (newSizeInBytes >= recordedOldSize)
            {
                m_stats.allocatedBytes += (newSizeInBytes - recordedOldSize);
            }
            else
            {
                const gp::USize delta = recordedOldSize - newSizeInBytes;
                m_stats.allocatedBytes = m_stats.allocatedBytes >= delta ? (m_stats.allocatedBytes - delta) : 0u;
            }

            m_stats.peakAllocatedBytes = math::max(m_stats.peakAllocatedBytes, m_offset);
            GP_PLOT(m_poolName, static_cast<gp::Int64>(m_offset));
            return ptr;
        }

        return nullptr;
    }

    void* const newPtr = allocate(newSizeInBytes, effectiveAlignment);
    if (newPtr != nullptr) [[likely]]
    {
        const gp::USize bytesToCopy = math::min(
            oldSizeInBytes > 0u ? math::min(oldSizeInBytes, recordedOldSize) : recordedOldSize, newSizeInBytes
        );
        if (bytesToCopy > 0u)
        {
            memory::copyMemory(newPtr, ptr, bytesToCopy);
        }
    }

    return newPtr;
}

void StackAllocator::deallocate(void* ptr, const gp::USize oldSizeInBytes, const gp::UInt32 alignment) noexcept
{
    (void)oldSizeInBytes;
    (void)alignment;

    if (ptr == nullptr || !owns(ptr))
    {
        return;
    }

    gp::UInt8* const bytePtr = static_cast<gp::UInt8*>(ptr);
    const gp::USize payloadOffset = static_cast<gp::USize>(bytePtr - m_buffer);
    if (payloadOffset < sizeof(Header)) [[unlikely]]
    {
        return;
    }

    const Header* const header = reinterpret_cast<const Header*>(bytePtr - sizeof(Header));
    const gp::USize recordedSize = static_cast<gp::USize>(header->allocationSize);

    ++m_stats.totalDeallocations;
    if (m_stats.activeAllocations > 0ull)
    {
        --m_stats.activeAllocations;
    }

    m_stats.allocatedBytes = m_stats.allocatedBytes >= recordedSize ? (m_stats.allocatedBytes - recordedSize) : 0u;

    // Pop stack cursor if this block is at the top of the LIFO stack.
    if (payloadOffset + recordedSize == m_offset && header->previousOffset <= payloadOffset)
    {
        m_offset = static_cast<gp::USize>(header->previousOffset);
        GP_PLOT(m_poolName, static_cast<gp::Int64>(m_offset));
    }
}

gp::USize
    StackAllocator::getActualAllocationSize(const gp::USize requestedSize, const gp::UInt32 alignment) const noexcept
{
    (void)alignment;
    return requestedSize;
}

bool StackAllocator::owns(const void* ptr) const noexcept
{
    if (ptr == nullptr || m_buffer == nullptr)
    {
        return false;
    }

    const gp::UInt8* const bytePtr = static_cast<const gp::UInt8*>(ptr);
    return bytePtr >= (m_buffer + sizeof(Header)) && bytePtr < (m_buffer + m_capacity);
}

const char* StackAllocator::getName() const noexcept
{
    return m_poolName;
}

void StackAllocator::rewind(const MemoryMark& savedMark) noexcept
{
    if (savedMark.offset <= m_offset) [[likely]]
    {
        m_offset = savedMark.offset;
        m_stats.activeAllocations = savedMark.activeAllocations;
        m_stats.allocatedBytes = savedMark.allocatedBytes;
        GP_PLOT(m_poolName, static_cast<gp::Int64>(m_offset));
    }
}

void StackAllocator::reset() noexcept
{
    m_offset = 0u;
    m_stats.allocatedBytes = 0u;
    m_stats.activeAllocations = 0ull;
    GP_PLOT(m_poolName, static_cast<gp::Int64>(0));
}

}   // namespace gp::memory
