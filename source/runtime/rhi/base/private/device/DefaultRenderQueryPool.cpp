// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/DefaultRenderQueryPool.hpp"
#include "device/DynamicRHI.hpp"
#include "threading/sync/UniqueLock.hpp"
#include <utility>

namespace gp::rhi
{

DefaultRenderQueryPool::DefaultRenderQueryPool(
    const sync::RenderQueryType inQueryType, DynamicRHI* const inRHI, const gp::UInt32 inNumQueries
) noexcept
    : m_rhi(inRHI)
    , m_queryType(inQueryType)
{
    if (inNumQueries > 0u)
    {
        m_queries.reserve(inNumQueries);
    }
}

DefaultRenderQueryPool::~DefaultRenderQueryPool() noexcept
{
    GP_CHECKF(
        m_allocatedQueries == m_queries.size(),
        "DefaultRenderQueryPool destroyed while pooled render queries are still checked out."
    );
}

sync::PooledRenderQuery DefaultRenderQueryPool::allocateQuery() noexcept
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

    if (!query.isValid() && m_rhi != nullptr)
    {
        query = m_rhi->createRenderQuery(m_queryType);
        if (query.isValid())
        {
            gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
            ++m_allocatedQueries;
        }
    }

    return sync::PooledRenderQuery(this, std::move(query));
}

void DefaultRenderQueryPool::releaseQuery(sync::RenderQueryRef&& query) noexcept
{
    if (!query.isValid())
    {
        return;
    }

    gp::thread::UniqueLock<gp::thread::Mutex> lock(m_mutex);
    m_queries.emplaceBack(std::move(query));
}

}   // namespace gp::rhi
