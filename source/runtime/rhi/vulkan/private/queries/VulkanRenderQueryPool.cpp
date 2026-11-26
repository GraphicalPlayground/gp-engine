// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanRenderQueryPool.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"
#include "queries/VulkanRenderQuery.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

VulkanRenderQueryPool::VulkanRenderQueryPool(
    const sync::RenderQueryType queryType, const gp::UInt32 numQueries
) noexcept
    : m_queryType(queryType)
{
    if (numQueries > 0u)
    {
        m_queries.reserve(numQueries);
    }
}

VulkanRenderQueryPool::~VulkanRenderQueryPool() noexcept
{
    GP_CHECKF(
        m_allocatedQueries == m_queries.size(),
        "VulkanRenderQueryPool destroyed while pooled render queries are still checked out."
    );
}

sync::PooledRenderQuery VulkanRenderQueryPool::allocateQuery() noexcept
{
    sync::RenderQueryRef query{};
    {
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        if (!m_queries.isEmpty())
        {
            query = std::move(m_queries.back());
            m_queries.popBack();
        }
    }

    if (!query.isValid())
    {
        query = sync::RenderQueryRef(new VulkanRenderQuery(m_queryType));
        gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
        ++m_allocatedQueries;
    }

    return sync::PooledRenderQuery(this, std::move(query));
}

gp::UInt32 VulkanRenderQueryPool::getAllocatedQueryCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_allocatedQueries;
}

gp::USize VulkanRenderQueryPool::getFreeQueryCount() const noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    return m_queries.size();
}

void VulkanRenderQueryPool::releaseQuery(sync::RenderQueryRef&& query) noexcept
{
    if (!query.isValid())
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_queries.emplaceBack(std::move(query));
}

}   // namespace gp::rhi::vulkan
