// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanQueryPoolManager.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "queries/VulkanQueryPool.hpp"
#include "queries/VulkanQuerySettings.hpp"
#include "threading/sync/UniqueLock.hpp"

namespace gp::rhi::vulkan
{

VulkanQueryPoolManager::VulkanQueryPoolManager(VulkanDevice& device) noexcept
    : m_device(device)
{}

VulkanQueryPoolManager::~VulkanQueryPoolManager() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    for (gp::USize typeIndex = 0u; typeIndex < kVulkanQueryPoolTypeCount; ++typeIndex)
    {
        gp::Vector<VulkanQueryPool*>& pools = m_freeQueryPools[typeIndex];
        for (gp::USize index = 0u; index < pools.size(); ++index)
        {
            delete pools[index];
        }
        pools.clear();
    }
}

VulkanQueryPool* VulkanQueryPoolManager::acquireOcclusionQueryPool(gp::UInt32 numQueries) noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);

    // Round to the pool granularity, with at least one query.
    numQueries = ((numQueries + kOcclusionPoolSizeGranularity - 1u) / kOcclusionPoolSizeGranularity) *
                 kOcclusionPoolSizeGranularity;
    numQueries = gp::math::max<gp::UInt32>(1u, numQueries);
    m_occlusionQueryPoolSize = gp::math::max<gp::UInt32>(m_occlusionQueryPoolSize, numQueries);

    // Destroy idle pools that cannot hold the new minimum size.
    gp::Vector<VulkanQueryPool*>& freePools = m_freeQueryPools[static_cast<gp::USize>(VulkanQueryPoolType::Occlusion)];
    for (gp::USize index = freePools.size(); index > 0u; --index)
    {
        VulkanQueryPool* const pool = freePools[index - 1u];
        GP_CHECK((pool != nullptr) && (pool->getPoolType() == VulkanQueryPoolType::Occlusion));
        if (pool->getMaxQueries() < m_occlusionQueryPoolSize)
        {
            delete pool;
            freePools.removeAtSwap(index - 1u);
        }
    }

    if (!freePools.isEmpty())
    {
        VulkanQueryPool* const pool = freePools.back();
        freePools.popBack();
        return pool;
    }

    return new VulkanQueryPool(m_device, m_occlusionQueryPoolSize, VulkanQueryPoolType::Occlusion);
}

VulkanQueryPool* VulkanQueryPoolManager::acquireTimingQueryPool() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);

    gp::Vector<VulkanQueryPool*>& freePools = m_freeQueryPools[static_cast<gp::USize>(VulkanQueryPoolType::Timestamp)];
    if (!freePools.isEmpty())
    {
        VulkanQueryPool* const pool = freePools.back();
        freePools.popBack();
        GP_CHECK(pool->getPoolType() == VulkanQueryPoolType::Timestamp);
        return pool;
    }

    return new VulkanQueryPool(m_device, getVulkanTimingQueryPoolSize(), VulkanQueryPoolType::Timestamp);
}

void VulkanQueryPoolManager::releaseQueryPool(VulkanQueryPool* const pool) noexcept
{
    if (pool == nullptr)
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    pool->setDirty();
    m_freeQueryPools[static_cast<gp::USize>(pool->getPoolType())].pushBack(pool);
}

void VulkanQueryPoolManager::removeStaleQueryPools() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    for (gp::USize typeIndex = 0u; typeIndex < kVulkanQueryPoolTypeCount; ++typeIndex)
    {
        gp::Vector<VulkanQueryPool*>& pools = m_freeQueryPools[typeIndex];
        for (gp::USize index = pools.size(); index > 0u; --index)
        {
            VulkanQueryPool* const pool = pools[index - 1u];
            GP_CHECK(pool != nullptr);
            if (pool->isStale())
            {
                delete pool;
                pools.removeAtSwap(index - 1u);
            }
            else
            {
                pool->incrementUnusedFrameCount();
            }
        }
    }
}

gp::USize VulkanQueryPoolManager::getFreePoolCount(const VulkanQueryPoolType type) const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_freeQueryPools[static_cast<gp::USize>(type)].size();
}

gp::UInt32 VulkanQueryPoolManager::getOcclusionQueryPoolSize() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_occlusionQueryPoolSize;
}

}   // namespace gp::rhi::vulkan
