// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "profiling/RHIStats.hpp"
#include "CoreMinimal.hpp"
#include "device/DynamicRHI.hpp"
#include "memory/MemoryOrder.hpp"

namespace gp::rhi::profiling
{

gp::thread::Atomic<gp::Int32> g_numDrawCallsRHI[kMaxNumGPUs]{};
gp::thread::Atomic<gp::Int32> g_numPrimitivesDrawnRHI[kMaxNumGPUs]{};
gp::thread::Atomic<gp::Int32> g_numDispatchCallsRHI[kMaxNumGPUs]{};

void DrawStatsCategory::Manager::latchFrameCounters() noexcept
{
    for (gp::UInt32 slot = 0u; slot < kNumCategorySlots; ++slot)
    {
        for (gp::UInt32 gpu = 0u; gpu < kMaxNumGPUs; ++gpu)
        {
            displayCounts[slot][gpu].draws = liveDraws[slot][gpu].exchange(0u, gp::memory::MemoryOrder::AcqRel);
            displayCounts[slot][gpu].primitives =
                livePrimitives[slot][gpu].exchange(0u, gp::memory::MemoryOrder::AcqRel);
        }
    }
}

void DrawStatsCategory::Manager::resetAllCounters() noexcept
{
    for (gp::UInt32 slot = 0u; slot < kNumCategorySlots; ++slot)
    {
        for (gp::UInt32 gpu = 0u; gpu < kMaxNumGPUs; ++gpu)
        {
            liveDraws[slot][gpu].store(0u, gp::memory::MemoryOrder::Relaxed);
            livePrimitives[slot][gpu].store(0u, gp::memory::MemoryOrder::Relaxed);
            displayCounts[slot][gpu] = Counters{};
        }
    }
}

gp::UInt32 DrawStatsCategory::Manager::getTotalDisplayDraws(const gp::UInt32 gpuIndex) const noexcept
{
    if (gpuIndex >= kMaxNumGPUs)
    {
        return 0u;
    }

    gp::UInt32 total = 0u;
    for (gp::UInt32 slot = 0u; slot < kNumCategorySlots; ++slot)
    {
        total += displayCounts[slot][gpuIndex].draws;
    }
    return total;
}

gp::UInt32 DrawStatsCategory::Manager::getTotalDisplayPrimitives(const gp::UInt32 gpuIndex) const noexcept
{
    if (gpuIndex >= kMaxNumGPUs)
    {
        return 0u;
    }

    gp::UInt32 total = 0u;
    for (gp::UInt32 slot = 0u; slot < kNumCategorySlots; ++slot)
    {
        total += displayCounts[slot][gpuIndex].primitives;
    }
    return total;
}

DrawStatsCategory::Manager& DrawStatsCategory::getManager() noexcept
{
    static Manager s_manager{};
    return s_manager;
}

DrawStatsCategory::DrawStatsCategory() noexcept
    : m_name()
    , m_index(kUncategorizedIndex)
{}

DrawStatsCategory::DrawStatsCategory(const gp::Name inName) noexcept
    : m_name(inName)
    , m_index(kUncategorizedIndex)
{
    Manager& mgr = getManager();
    for (gp::UInt32 i = 0u; i < mgr.numCategories; ++i)
    {
        if (mgr.categories[i] != nullptr && mgr.categories[i]->m_name == inName)
        {
            m_index = i;
            return;
        }
    }

    if (mgr.numCategories < kMaxCategories)
    {
        m_index = mgr.numCategories;
        mgr.categories[mgr.numCategories] = this;
        ++mgr.numCategories;
    }
}

void recordDrawCallStats(
    const gp::UInt32 primitives,
    const gp::UInt32 numDraws,
    const GPUMask gpuMask,
    const DrawStatsCategory* const category
) noexcept
{
    const gp::UInt32 slot = (category != nullptr && category->getIndex() < DrawStatsCategory::kNumCategorySlots)
                                ? category->getIndex()
                                : DrawStatsCategory::kUncategorizedIndex;
    DrawStatsCategory::Manager& mgr = DrawStatsCategory::getManager();

    for (const gp::UInt32 gpuIndex: gpuMask)
    {
        if (gpuIndex < kMaxNumGPUs)
        {
            g_numDrawCallsRHI[gpuIndex].fetchAdd(static_cast<gp::Int32>(numDraws), gp::memory::MemoryOrder::Relaxed);
            g_numPrimitivesDrawnRHI[gpuIndex].fetchAdd(
                static_cast<gp::Int32>(primitives), gp::memory::MemoryOrder::Relaxed
            );
            mgr.liveDraws[slot][gpuIndex].fetchAdd(numDraws, gp::memory::MemoryOrder::Relaxed);
            mgr.livePrimitives[slot][gpuIndex].fetchAdd(primitives, gp::memory::MemoryOrder::Relaxed);
        }
    }
}

void recordDispatchCallStats(const gp::UInt32 numDispatches, const GPUMask gpuMask) noexcept
{
    for (const gp::UInt32 gpuIndex: gpuMask)
    {
        if (gpuIndex < kMaxNumGPUs)
        {
            g_numDispatchCallsRHI[gpuIndex].fetchAdd(
                static_cast<gp::Int32>(numDispatches), gp::memory::MemoryOrder::Relaxed
            );
        }
    }
}

void resetDrawCallStats() noexcept
{
    for (gp::UInt32 gpu = 0u; gpu < kMaxNumGPUs; ++gpu)
    {
        g_numDrawCallsRHI[gpu].store(0, gp::memory::MemoryOrder::Relaxed);
        g_numPrimitivesDrawnRHI[gpu].store(0, gp::memory::MemoryOrder::Relaxed);
        g_numDispatchCallsRHI[gpu].store(0, gp::memory::MemoryOrder::Relaxed);
    }
    DrawStatsCategory::getManager().resetAllCounters();
}

RHIMemoryStats updateRHIMemoryStats() noexcept
{
    RHIMemoryStats stats{};
    if (DynamicRHI* const rhi = getDynamicRHI(); rhi != nullptr)
    {
        rhi->getMemoryStats(stats);
    }
    return stats;
}

}   // namespace gp::rhi::profiling
