// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "sync/SyncEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Native query pool category; each one maps to a single `VkQueryType`.
enum class VulkanQueryPoolType : gp::UInt8
{
    /// @brief Samples passing the depth and stencil tests.
    Occlusion = 0u,

    /// @brief Pipeline statistics counters.
    PipelineStats,

    /// @brief GPU timestamps.
    Timestamp,

    /// @brief Compacted size of acceleration structures.
    ASCompactedSize,

    /// @brief Number of pool types.
    Count
};

/// @brief Number of `VulkanQueryPoolType` values.
inline constexpr gp::USize kVulkanQueryPoolTypeCount = static_cast<gp::USize>(VulkanQueryPoolType::Count);

/// @brief Maps a query pool category to its native query type.
/// @param[in] type Pool category.
/// @return Native query type, or `VK_QUERY_TYPE_MAX_ENUM` for `Count`.
[[nodiscard]] constexpr VkQueryType toVkQueryType(const VulkanQueryPoolType type) noexcept
{
    switch (type)
    {
    case VulkanQueryPoolType::Occlusion:
        return VK_QUERY_TYPE_OCCLUSION;
    case VulkanQueryPoolType::PipelineStats:
        return VK_QUERY_TYPE_PIPELINE_STATISTICS;
    case VulkanQueryPoolType::Timestamp:
        return VK_QUERY_TYPE_TIMESTAMP;
    case VulkanQueryPoolType::ASCompactedSize:
        return VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
    case VulkanQueryPoolType::Count:
    default:
        return VK_QUERY_TYPE_MAX_ENUM;
    }
}

/// @brief Maps a render query type to the pool category that serves it.
/// @param[in] type Render query type; must be `Occlusion` or `AbsoluteTime`.
/// @return `Occlusion` or `Timestamp`, or `Count` for an unsupported type.
[[nodiscard]] constexpr VulkanQueryPoolType toVulkanQueryPoolType(const sync::RenderQueryType type) noexcept
{
    switch (type)
    {
    case sync::RenderQueryType::Occlusion:
        return VulkanQueryPoolType::Occlusion;
    case sync::RenderQueryType::AbsoluteTime:
        return VulkanQueryPoolType::Timestamp;
    case sync::RenderQueryType::Undefined:
    default:
        return VulkanQueryPoolType::Count;
    }
}

}   // namespace gp::rhi::vulkan
