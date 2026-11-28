// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "queries/VulkanQueryEnums.hpp"
#include "RHIVulkan.hpp"
#include "sync/RenderQuery.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/VulkanSyncPoint.hpp"

namespace gp::rhi::vulkan
{

/// @brief Render query that borrows one slot of a `VulkanQueryPool` each time it is recorded.
/// @details The query pool keeps a reference to the query until the payload that recorded it completes and writes
///          `result`. `syncPoint` is signaled after that write, so `result` is valid once it reports complete.
class GP_RHI_VULKAN_API VulkanRenderQuery final : public sync::RenderQuery
{
public:
    /// @brief Sentinel for a query that does not own a pool slot.
    static constexpr gp::UInt32 kInvalidIndexInPool = 0xFFFFFFFFu;

    /// @brief Creates a query of `queryType`.
    /// @param[in] queryType `Occlusion` or `AbsoluteTime`.
    explicit VulkanRenderQuery(sync::RenderQueryType queryType) noexcept;

    /// @brief Returns the render query type.
    /// @return Query type.
    [[nodiscard]] sync::RenderQueryType getQueryType() const noexcept
    {
        return m_queryType;
    }

    /// @brief Returns the pool category serving this query.
    /// @return `Occlusion` or `Timestamp`.
    [[nodiscard]] VulkanQueryPoolType getQueryPoolType() const noexcept;

public:
    /// @brief Raw result: samples passed, or GPU ticks for a timestamp.
    gp::UInt64 result{ 0u };

    /// @brief First slot of the query in its current pool.
    gp::UInt32 indexInPool{ kInvalidIndexInPool };

    /// @brief Signaled once `result` holds the value of the last recording.
    VulkanSyncPointRef syncPoint{};

protected:
    /// @brief Destroys the query.
    /// @note The sync point may still be pending: pools drop their reference before signaling it.
    virtual ~VulkanRenderQuery() noexcept override;

private:
    /// @brief Render query type.
    const sync::RenderQueryType m_queryType;
};

}   // namespace gp::rhi::vulkan
