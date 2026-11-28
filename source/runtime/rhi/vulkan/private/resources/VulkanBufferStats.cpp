// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "resources/VulkanBufferStats.hpp"
#include "CoreMinimal.hpp"
#include "memory/MemoryOrder.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/VulkanBufferEnums.hpp"
#include "resources/VulkanBufferUsage.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Process-wide buffer memory counters.
struct VulkanBufferStatCounters final
{
    gp::thread::Atomic<gp::Int64> categoryBytes[kVulkanBufferStatCategoryCount]{};
    gp::thread::Atomic<gp::Int64> totalBufferBytes{ 0 };
    gp::thread::Atomic<gp::Int64> reservedVirtualBytes{ 0 };
    gp::thread::Atomic<gp::Int64> reservedCommittedBytes{ 0 };
};

VulkanBufferStatCounters g_bufferStatCounters{};

constexpr gp::memory::MemoryOrder kStatOrder = gp::memory::MemoryOrder::Relaxed;

}   // namespace

VulkanBufferStatCategory getVulkanBufferStatCategory(const resource::BufferUsageFlags usage) noexcept
{
    using resource::BufferUsageFlags;

    if (hasAnyBufferUsage(usage, BufferUsageFlags::ReservedResource))
    {
        return VulkanBufferStatCategory::ReservedUncommitted;
    }
    else if (hasAnyBufferUsage(usage, BufferUsageFlags::VertexBuffer))
    {
        return VulkanBufferStatCategory::Vertex;
    }
    else if (hasAnyBufferUsage(usage, BufferUsageFlags::IndexBuffer))
    {
        return VulkanBufferStatCategory::Index;
    }
    else if (hasAnyBufferUsage(usage, BufferUsageFlags::AccelerationStructure))
    {
        return VulkanBufferStatCategory::AccelerationStructure;
    }
    else if (hasAnyBufferUsage(usage, BufferUsageFlags::ByteAddressBuffer))
    {
        return VulkanBufferStatCategory::ByteAddress;
    }
    else if (hasAnyBufferUsage(usage, BufferUsageFlags::DrawIndirect))
    {
        return VulkanBufferStatCategory::DrawIndirect;
    }
    else if (hasAnyBufferUsage(usage, BufferUsageFlags::StructuredBuffer))
    {
        return VulkanBufferStatCategory::Structured;
    }
    return VulkanBufferStatCategory::Misc;
}

void updateVulkanBufferStats(
    const resource::BufferUsageFlags usage, const gp::UInt64 bufferSize, const bool isAllocating
) noexcept
{
    const gp::Int64 delta = isAllocating ? static_cast<gp::Int64>(bufferSize) : -static_cast<gp::Int64>(bufferSize);
    const gp::USize categoryIndex = static_cast<gp::USize>(getVulkanBufferStatCategory(usage));

    static_cast<void>(g_bufferStatCounters.categoryBytes[categoryIndex].fetchAdd(delta, kStatOrder));
    static_cast<void>(g_bufferStatCounters.totalBufferBytes.fetchAdd(delta, kStatOrder));

    if (hasAnyBufferUsage(usage, resource::BufferUsageFlags::ReservedResource))
    {
        static_cast<void>(g_bufferStatCounters.reservedVirtualBytes.fetchAdd(delta, kStatOrder));
    }
}

void updateVulkanReservedBufferStatsOnCommit(const gp::UInt64 commitDelta, const bool isCommitting) noexcept
{
    const gp::Int64 delta = isCommitting ? static_cast<gp::Int64>(commitDelta) : -static_cast<gp::Int64>(commitDelta);
    const gp::USize uncommittedIndex = static_cast<gp::USize>(VulkanBufferStatCategory::ReservedUncommitted);

    static_cast<void>(g_bufferStatCounters.reservedCommittedBytes.fetchAdd(delta, kStatOrder));
    static_cast<void>(g_bufferStatCounters.categoryBytes[uncommittedIndex].fetchSub(delta, kStatOrder));
}

VulkanBufferMemoryStats getVulkanBufferMemoryStats() noexcept
{
    VulkanBufferMemoryStats stats{};
    for (gp::USize index = 0u; index < kVulkanBufferStatCategoryCount; ++index)
    {
        stats.categoryBytes[index] = g_bufferStatCounters.categoryBytes[index].load(kStatOrder);
    }
    stats.totalBufferBytes = g_bufferStatCounters.totalBufferBytes.load(kStatOrder);
    stats.reservedVirtualBytes = g_bufferStatCounters.reservedVirtualBytes.load(kStatOrder);
    stats.reservedCommittedBytes = g_bufferStatCounters.reservedCommittedBytes.load(kStatOrder);
    return stats;
}

}   // namespace gp::rhi::vulkan

