// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "allocators/DescriptorAllocator.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Scalar.hpp"
#include "RHIDefinitions.hpp"
#include "templates/NumericLimits.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::rhi::allocator
{

#if GP_DO_CHECK
namespace
{

bool g_trackOutstandingDescriptorAllocations = false;
gp::console::AutoConsoleVariableRef g_cvarTrackOutstandingDescriptorAllocations(
    "rhi.TrackOutstandingDescriptorAllocations",
    g_trackOutstandingDescriptorAllocations,
    "When enabled, tracks outstanding descriptor allocations to detect double-free and leak bugs.",
    gp::console::ConsoleVarFlags::ReadOnly
);

}   // namespace
#endif

bool DescriptorAllocator::isOutstandingAllocationTrackingEnabled() noexcept
{
#if GP_DO_CHECK
    return g_trackOutstandingDescriptorAllocations;
#else
    return false;
#endif
}

void DescriptorAllocator::setOutstandingAllocationTrackingEnabled(const bool enabled) noexcept
{
#if GP_DO_CHECK
    g_trackOutstandingDescriptorAllocations = enabled;
#else
    static_cast<void>(enabled);
#endif
}

DescriptorAllocator::DescriptorAllocator() noexcept = default;

DescriptorAllocator::DescriptorAllocator(
    const gp::UInt32 numDescriptors, const gp::ConstVectorView<DescriptorStatCounter*> stats
) noexcept
{
    init(numDescriptors, stats);
}

DescriptorAllocator::~DescriptorAllocator() noexcept
{
#if GP_DO_CHECK
    if (g_trackOutstandingDescriptorAllocations && !m_debugOutstandingAllocations.isEmpty())
    {
        GP_LOG_ERROR(
            LogRHI,
            "DescriptorAllocator destroyed with {} outstanding allocations. Possible descriptor leak.",
            m_debugOutstandingAllocations.size()
        );

        for (const DebugDescriptorAllocation& entry: m_debugOutstandingAllocations)
        {
            GP_LOG_ERROR(LogRHI, "  Leaked allocation: Offset={}, Count={}", entry.offset, entry.count);
        }

        GP_CHECKF(
            false,
            "DescriptorAllocator destroyed with %llu outstanding allocations.",
            static_cast<unsigned long long>(m_debugOutstandingAllocations.size())
        );
    }
#endif
}

void DescriptorAllocator::init(
    const gp::UInt32 numDescriptors, const gp::ConstVectorView<DescriptorStatCounter*> stats
) noexcept
{
    m_capacity = numDescriptors;
    m_ranges.clear();
    if (numDescriptors > 0u)
    {
        m_ranges.emplaceBack(0u, numDescriptors - 1u);
    }

    m_stats.clear();
    m_stats.reserve(stats.size());
    for (DescriptorStatCounter* const stat: stats)
    {
        m_stats.pushBack(stat);
    }
}

void DescriptorAllocator::shutdown() noexcept
{
    m_ranges.clear();
    m_capacity = 0u;
}

gp::Optional<DescriptorAllocation>
    DescriptorAllocator::resizeGrowAndAllocate(const gp::UInt32 newCapacity, const gp::UInt32 numAllocations) noexcept
{
    GP_CHECK(m_capacity < newCapacity);

    const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);

    bool addEndRange = true;
    if (!m_ranges.isEmpty() && m_capacity > 0u)
    {
        DescriptorAllocatorRange& lastRange = m_ranges[m_ranges.size() - 1u];
        const gp::UInt32 veryLastIndex = getCapacity() - 1u;
        if (lastRange.last == veryLastIndex)
        {
            lastRange.last = newCapacity - 1u;
            addEndRange = false;
        }
    }

    if (addEndRange)
    {
        m_ranges.emplaceBack(m_capacity, newCapacity - 1u);
    }

    m_capacity = newCapacity;

    const gp::Optional<DescriptorAllocation> allocation = allocate(numAllocations);
    GP_VERIFY(allocation.hasValue());
    return allocation;
}

gp::Optional<DescriptorAllocation> DescriptorAllocator::allocate(const gp::UInt32 numDescriptors) noexcept
{
    const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);
    return allocateInternal(numDescriptors);
}

view::DescriptorHandle DescriptorAllocator::allocate(const view::DescriptorType type) noexcept
{
    const gp::Optional<DescriptorAllocation> allocation = allocate(1u);
    if (allocation.hasValue())
    {
        return view::DescriptorHandle(type, allocation->startIndex);
    }

    return view::DescriptorHandle();
}

gp::Optional<DescriptorAllocation> DescriptorAllocator::allocateInternal(const gp::UInt32 numDescriptors) noexcept
{
    if (const gp::USize numRanges = m_ranges.size(); numRanges > 0u)
    {
        gp::USize index = 0u;
        do
        {
            DescriptorAllocatorRange& currentRange = m_ranges[index];
            const gp::UInt32 size = 1u + currentRange.last - currentRange.first;
            if (numDescriptors <= size)
            {
                const gp::UInt32 first = currentRange.first;
                if (numDescriptors == size && (index + 1u) < numRanges)
                {
                    // Range is full and another range follows it, so drop the exhausted entry.
                    m_ranges.removeAt(index);
                }
                else
                {
                    currentRange.first += numDescriptors;
                }

                recordAlloc(numDescriptors);

#if GP_DO_CHECK
                if (g_trackOutstandingDescriptorAllocations)
                {
                    const DebugDescriptorAllocation newEntry{ first, numDescriptors };
                    const gp::USize insertIndex = m_debugOutstandingAllocations.lowerBound(
                        newEntry,
                        [](const DebugDescriptorAllocation& lhs, const DebugDescriptorAllocation& rhs) noexcept -> bool
                    {
                        return lhs.offset < rhs.offset;
                    }
                    );
                    m_debugOutstandingAllocations.insertAt(insertIndex, newEntry);
                }
#endif

                return DescriptorAllocation(first, numDescriptors);
            }
            ++index;
        }
        while (index < numRanges);
    }

    return gp::nullopt;
}

void DescriptorAllocator::freeInternal(const DescriptorAllocation allocation) noexcept
{
    const gp::UInt32 offset = allocation.startIndex;
    const gp::UInt32 numDescriptors = allocation.count;

    if (offset == gp::NumericLimits<gp::UInt32>::max() || numDescriptors == 0u)
    {
        return;
    }

#if GP_DO_CHECK
    if (g_trackOutstandingDescriptorAllocations)
    {
        const DebugDescriptorAllocation searchKey{ offset, 0u };
        const gp::USize foundIndex = m_debugOutstandingAllocations.binarySearch(
            searchKey,
            [](const DebugDescriptorAllocation& lhs, const DebugDescriptorAllocation& rhs) noexcept -> bool
        {
            return lhs.offset < rhs.offset;
        }
        );

        if (foundIndex == gp::Vector<DebugDescriptorAllocation>::npos)
        {
            GP_CHECKF(
                false,
                "DescriptorAllocator::free - Allocation at offset %u (count %u) was not found in outstanding " "allocations. Possible double-free.",
                offset,
                numDescriptors
            );
        }
        else
        {
            const gp::UInt32 recordedCount = m_debugOutstandingAllocations[foundIndex].count;
            m_debugOutstandingAllocations.removeAt(foundIndex);

            if (recordedCount != numDescriptors)
            {
                GP_CHECKF(
                    false,
                    "DescriptorAllocator::free - Allocation at offset %u has mismatched count: freeing %u but " "allocated %u.",
                    offset,
                    numDescriptors,
                    recordedCount
                );
            }
        }
    }
#endif

    if (m_ranges.isEmpty())
    {
        m_ranges.emplaceBack(offset, offset + numDescriptors - 1u);
        recordFree(numDescriptors);
        return;
    }

    const gp::UInt32 end = offset + numDescriptors;

    // Binary search over sorted disjoint free ranges.
    gp::USize index0 = 0u;
    gp::USize index1 = m_ranges.size() - 1u;
    for (;;)
    {
        const gp::USize index = (index0 + index1) / 2u;
        if (offset < m_ranges[index].first)
        {
            if (end >= m_ranges[index].first)
            {
                GP_CHECK(end == m_ranges[index].first);

                if (index > index0 && (offset - 1u) == m_ranges[index - 1u].last)
                {
                    // Merge with the previous range as well.
                    m_ranges[index - 1u].last = m_ranges[index].last;
                    m_ranges.removeAt(index);
                }
                else
                {
                    m_ranges[index].first = offset;
                }

                recordFree(numDescriptors);
                return;
            }
            else
            {
                if (index != index0)
                {
                    index1 = index - 1u;
                }
                else
                {
                    m_ranges.emplaceAt(index, offset, end - 1u);
                    recordFree(numDescriptors);
                    return;
                }
            }
        }
        else if (offset > m_ranges[index].last)
        {
            if ((offset - 1u) == m_ranges[index].last)
            {
                if (index < index1 && end == m_ranges[index + 1u].first)
                {
                    // Merge with the next range as well.
                    m_ranges[index].last = m_ranges[index + 1u].last;
                    m_ranges.removeAt(index + 1u);
                }
                else
                {
                    m_ranges[index].last += numDescriptors;
                }

                recordFree(numDescriptors);
                return;
            }
            else
            {
                if (index != index1)
                {
                    index0 = index + 1u;
                }
                else
                {
                    m_ranges.emplaceAt(index + 1u, offset, end - 1u);
                    recordFree(numDescriptors);
                    return;
                }
            }
        }
        else
        {
            // Offset falls inside an already-free range.
            GP_CHECK_NO_ENTRY();
            return;
        }
    }
}

void DescriptorAllocator::free(const DescriptorAllocation allocation) noexcept
{
    const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);
    freeInternal(allocation);
}

void DescriptorAllocator::free(const gp::ConstVectorView<view::DescriptorHandle> handles) noexcept
{
    if (handles.isEmpty())
    {
        return;
    }

    const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);
    for (const view::DescriptorHandle handle: handles)
    {
        if (handle.isValid())
        {
            freeInternal(DescriptorAllocation(handle.getIndex(), 1u));
        }
    }
}

void DescriptorAllocator::free(const view::DescriptorHandle handle) noexcept
{
    if (handle.isValid())
    {
        const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);
        freeInternal(DescriptorAllocation(handle.getIndex(), 1u));
    }
}

bool DescriptorAllocator::getAllocatedRange(DescriptorAllocatorRange& outRange) noexcept
{
    const gp::thread::UniqueLock<gp::thread::RecursiveMutex> lock(m_mutex);

    if (m_capacity == 0u)
    {
        outRange.first = 0u;
        outRange.last = 0u;
        return false;
    }

    const gp::UInt32 veryFirstIndex = 0u;
    const gp::UInt32 veryLastIndex = getCapacity() - 1u;

    outRange.first = veryFirstIndex;
    outRange.last = veryLastIndex;

    if (!m_ranges.isEmpty())
    {
        const DescriptorAllocatorRange firstRange = m_ranges[0u];

        // A single free range covering the entire heap means zero live allocations.
        if (firstRange.first == veryFirstIndex && firstRange.last == veryLastIndex)
        {
            return false;
        }

        // A leading free range pushes the first live allocation right after it.
        if (firstRange.first == veryFirstIndex)
        {
            outRange.first = gp::math::min(firstRange.last + 1u, veryLastIndex);
        }

        const DescriptorAllocatorRange lastRange = m_ranges[m_ranges.size() - 1u];

        // A trailing free range pulls the last live allocation right before it.
        if (lastRange.last == veryLastIndex)
        {
            outRange.last = (lastRange.first > 0u) ? (lastRange.first - 1u) : 0u;
        }
    }

    return true;
}

}   // namespace gp::rhi::allocator
