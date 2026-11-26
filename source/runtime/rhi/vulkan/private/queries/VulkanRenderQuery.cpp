// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanRenderQuery.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "diagnostics/AssertionMacros.hpp"

namespace gp::rhi::vulkan
{

VulkanRenderQuery::VulkanRenderQuery(const sync::RenderQueryType queryType) noexcept
    : m_queryType(queryType)
{
    GP_CHECKF(
        (queryType == sync::RenderQueryType::Occlusion) || (queryType == sync::RenderQueryType::AbsoluteTime),
        "Unknown render query type {}.",
        static_cast<gp::UInt32>(queryType)
    );
}

VulkanRenderQuery::~VulkanRenderQuery() noexcept = default;

VulkanQueryPoolType VulkanRenderQuery::getQueryPoolType() const noexcept
{
    if (m_queryType == sync::RenderQueryType::Occlusion)
    {
        return VulkanQueryPoolType::Occlusion;
    }

    GP_CHECK(m_queryType == sync::RenderQueryType::AbsoluteTime);
    return VulkanQueryPoolType::Timestamp;
}

}   // namespace gp::rhi::vulkan
