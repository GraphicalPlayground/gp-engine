// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "sync/GPUFence.hpp"
#include "containers/strings/Name.hpp"
#include "core/GPUMask.hpp"
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "sync/RenderQuery.hpp"
#include "sync/RenderQueryPool.hpp"
#include <utility>

namespace gp::rhi::sync
{

GPUFence::GPUFence(const gp::Name name) noexcept
    : resource::Resource(resource::ResourceType::GPUFence)
    , m_fenceName(name)
{}

GPUFence::~GPUFence() noexcept = default;

bool GPUFence::poll(const GPUMask gpuMask) const noexcept
{
    GP_CHECKF(
        gpuMask == GPUMask::gpu0(),
        "GPUFence::poll(GPUMask) expects GPUMask::gpu0() unless overridden by a multi-GPU backend."
    );
    return poll();
}

RenderQuery::RenderQuery() noexcept
    : resource::Resource(resource::ResourceType::RenderQuery)
{}

RenderQuery::~RenderQuery() noexcept = default;

PooledRenderQuery::PooledRenderQuery() noexcept = default;

PooledRenderQuery::PooledRenderQuery(RenderQueryPool* const queryPool, RenderQueryRef&& query) noexcept
    : m_query(std::move(query))
    , m_queryPool(queryPool)
{}

PooledRenderQuery::~PooledRenderQuery() noexcept
{
    releaseQuery();
}

PooledRenderQuery::PooledRenderQuery(PooledRenderQuery&& other) noexcept
    : m_query(std::move(other.m_query))
    , m_queryPool(other.m_queryPool)
{
    other.m_queryPool = nullptr;
}

PooledRenderQuery& PooledRenderQuery::operator=(PooledRenderQuery&& other) noexcept
{
    if (this != &other)
    {
        releaseQuery();
        m_query = std::move(other.m_query);
        m_queryPool = other.m_queryPool;
        other.m_queryPool = nullptr;
    }
    return *this;
}

void PooledRenderQuery::releaseQuery() noexcept
{
    if (m_queryPool != nullptr && m_query.isValid())
    {
        m_queryPool->releaseQuery(std::move(m_query));
        m_queryPool = nullptr;
        GP_CHECK(!m_query.isValid());
    }
    else
    {
        m_query.reset();
        m_queryPool = nullptr;
    }
}

RenderQueryPool::RenderQueryPool() noexcept
    : resource::Resource(resource::ResourceType::RenderQueryPool)
{}

RenderQueryPool::~RenderQueryPool() noexcept = default;

}   // namespace gp::rhi::sync
