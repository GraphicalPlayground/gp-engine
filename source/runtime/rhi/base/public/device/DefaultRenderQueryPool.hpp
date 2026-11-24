// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceForward.hpp"            // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "RHIBase.hpp"
#include "sync/RenderQuery.hpp"                // IWYU pragma: keep
#include "sync/RenderQueryPool.hpp"
#include "sync/SyncEnums.hpp"
#include "threading/sync/Mutex.hpp"

namespace gp::rhi
{

/// @brief Default `RenderQueryPool` implementation backed by `DynamicRHI::createRenderQuery()` and a free-list vector.
class GP_RHI_API DefaultRenderQueryPool final : public sync::RenderQueryPool
{
public:
    /// @brief Constructs a query pool allocating queries of `inQueryType` from `inRHI`.
    /// @param[in] inQueryType Query metric type (`Occlusion` or `AbsoluteTime`).
    /// @param[in] inRHI Active backend used to allocate new queries when the free list is empty.
    /// @param[in] inNumQueries Optional initial reservation count.
    DefaultRenderQueryPool(sync::RenderQueryType inQueryType, DynamicRHI* inRHI, gp::UInt32 inNumQueries = 0u) noexcept;

    /// @copydoc sync::RenderQueryPool::allocateQuery()
    [[nodiscard]] virtual sync::PooledRenderQuery allocateQuery() noexcept override;

protected:
    /// @copydoc sync::RenderQueryPool::~RenderQueryPool()
    virtual ~DefaultRenderQueryPool() noexcept override;

private:
    /// @copydoc sync::RenderQueryPool::releaseQuery()
    virtual void releaseQuery(sync::RenderQueryRef&& query) noexcept override;

private:
    /// @brief Active RHI backend used to allocate new queries when the free list is empty.
    DynamicRHI* m_rhi{ nullptr };

    /// @brief Query type allocated by this pool.
    sync::RenderQueryType m_queryType{ sync::RenderQueryType::Undefined };

    /// @brief Recycled free queries awaiting reuse.
    gp::Vector<sync::RenderQueryRef> m_queries{};

    /// @brief Total number of live queries ever created by this pool (verified on destruction).
    gp::UInt32 m_allocatedQueries{ 0u };

    /// @brief Mutex synchronizing free-list access across threads.
    gp::thread::Mutex m_mutex{};
};

}   // namespace gp::rhi
