// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "sync/RenderQueryPool.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"   // IWYU pragma: keep
#include "threading/sync/Mutex.hpp"

namespace gp::rhi::vulkan
{

/// @brief Render query pool that recycles `VulkanRenderQuery` objects of one type.
/// @details Allocating returns an idle query when one is available and creates a new one otherwise; queries come
///          back when their `PooledRenderQuery` is released.
/// @note Thread-safe.
class GP_RHI_VULKAN_API VulkanRenderQueryPool final : public sync::RenderQueryPool
{
public:
    /// @brief Creates an empty pool.
    /// @param[in] queryType `Occlusion` or `AbsoluteTime`.
    /// @param[in] numQueries Capacity to reserve for idle queries; 0 for none.
    explicit VulkanRenderQueryPool(sync::RenderQueryType queryType, gp::UInt32 numQueries = 0u) noexcept;

    /// @copydoc sync::RenderQueryPool::allocateQuery()
    [[nodiscard]] virtual sync::PooledRenderQuery allocateQuery() noexcept override;

    /// @brief Returns the query type served by this pool.
    /// @return Query type.
    [[nodiscard]] sync::RenderQueryType getQueryType() const noexcept
    {
        return m_queryType;
    }

    /// @brief Returns the number of queries this pool created.
    /// @return Created query count.
    [[nodiscard]] gp::UInt32 getAllocatedQueryCount() const noexcept;

    /// @brief Returns the number of idle queries.
    /// @return Idle query count.
    [[nodiscard]] gp::USize getFreeQueryCount() const noexcept;

protected:
    /// @brief Destroys the pool; every query must have been returned.
    virtual ~VulkanRenderQueryPool() noexcept override;

private:
    /// @copydoc sync::RenderQueryPool::releaseQuery()
    virtual void releaseQuery(sync::RenderQueryRef&& query) noexcept override;

private:
    /// @brief Query type served by this pool.
    const sync::RenderQueryType m_queryType;

    /// @brief Idle queries.
    gp::Vector<sync::RenderQueryRef> m_queries{};

    /// @brief Number of queries this pool created.
    gp::UInt32 m_allocatedQueries{ 0u };

    /// @brief Guards the members above.
    mutable gp::thread::Mutex m_mutex{};
};

}   // namespace gp::rhi::vulkan
