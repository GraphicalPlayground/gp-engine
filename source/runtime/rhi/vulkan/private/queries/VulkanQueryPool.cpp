// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "queries/VulkanQueryPool.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/VulkanDevice.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "platforms/base/system/Thread.hpp"
#include "queries/VulkanQuerySettings.hpp"
#include "queries/VulkanRenderQuery.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Delay between two readback attempts while results are not ready, in milliseconds.
constexpr gp::UInt32 kNotReadyRetryDelayMs = 5u;

/// @brief Readback attempts before results are given up (about two seconds).
constexpr gp::UInt32 kMaxNotReadyRetries = 400u;

}   // namespace

VulkanQueryPool::VulkanQueryPool(
    VulkanDevice& device, const gp::UInt32 maxQueries, const VulkanQueryPoolType queryType
) noexcept
    : m_device(device)
    , m_maxQueries(maxQueries)
    , m_queryType(queryType)
{
    GP_CHECK(maxQueries > 0u);
    m_scratchResults.resize(maxQueries);

    VkQueryPoolCreateInfo createInfo =
        zeroVulkanStruct<VkQueryPoolCreateInfo>(VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
    createInfo.queryType = toVkQueryType(queryType);
    createInfo.queryCount = maxQueries;

#if defined(VK_KHR_maintenance9)
    // Maintenance9 creates every query in the reset state, so the pool can be used without a reset first.
    if (m_device.getOptionalExtensions().hasKHRMaintenance9 != 0u)
    {
        resetReservations();
        createInfo.flags |= VK_QUERY_POOL_CREATE_RESET_BIT_KHR;
        m_isDirty = false;
    }
#endif

    if (dynamic_api::vkCreateQueryPool == nullptr)
    {
        return;
    }

    const VkResult result = vkCreateQueryPool(m_device.getHandle(), &createInfo, GP_VULKAN_CPU_ALLOCATOR, &m_queryPool);
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateQueryPool failed with {}.", vulkanResultToString(result));
        m_queryPool = VK_NULL_HANDLE;
    }
}

VulkanQueryPool::~VulkanQueryPool() noexcept
{
    if ((m_queryPool != VK_NULL_HANDLE) && (dynamic_api::vkDestroyQueryPool != nullptr))
    {
        vkDestroyQueryPool(m_device.getHandle(), m_queryPool, GP_VULKAN_CPU_ALLOCATOR);
    }
    m_queryPool = VK_NULL_HANDLE;
}

void VulkanQueryPool::reserveQuery(VulkanRenderQuery& query) noexcept
{
    GP_CHECK(query.getQueryPoolType() == m_queryType);
    GP_CHECKF(!isFull(), "Query pool is full.");

    const gp::UInt32 querySlot = m_currentQueryCount++;
    query.indexInPool = querySlot * m_queryStride;
    m_queryRefs[querySlot] = &query;
    m_queryResults[querySlot] = &query.result;
    m_isDirty = true;
}

gp::UInt32 VulkanQueryPool::reserveQuery(gp::UInt64* const resultPtr) noexcept
{
    GP_CHECK(m_queryType == VulkanQueryPoolType::Timestamp);
    GP_CHECKF(!isFull(), "Query pool is full.");

    const gp::UInt32 querySlot = m_currentQueryCount++;
    m_queryRefs[querySlot] = nullptr;
    m_queryResults[querySlot] = resultPtr;
    m_isDirty = true;
    return querySlot * m_queryStride;
}

void VulkanQueryPool::reset(const VkCommandBuffer cmdBuffer, const gp::UInt32 queryStride) noexcept
{
    GP_CHECK(queryStride > 0u);
    m_currentQueryCount = 0u;
    m_queryStride = queryStride;
    m_unusedFrameCount = 0;
    resetReservations();

    if (cmdBuffer != VK_NULL_HANDLE)
    {
        vkCmdResetQueryPool(cmdBuffer, m_queryPool, 0u, m_maxQueries);
    }
    else
    {
        GP_CHECKF(
            (m_device.getOptionalExtensions().hasEXTHostQueryReset != 0u) &&
                (dynamic_api::vkResetQueryPoolEXT != nullptr),
            "VK_EXT_host_query_reset must be supported to reset queries without a command buffer."
        );
        if (dynamic_api::vkResetQueryPoolEXT != nullptr)
        {
            dynamic_api::vkResetQueryPoolEXT(m_device.getHandle(), m_queryPool, 0u, m_maxQueries);
        }
    }
    m_isDirty = false;
}

bool VulkanQueryPool::resolveResults() noexcept
{
    if (m_currentQueryCount == 0u)
    {
        m_queryRefs.clear();
        return true;
    }

    const gp::UInt32 slotCount = m_currentQueryCount * m_queryStride;
    GP_CHECK(slotCount <= m_maxQueries);

    // A wait bit can hang forever on some drivers after a device loss. The completion fence was already reached, so
    // every result is about to be available: poll instead.
    const VkQueryResultFlags queryFlags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
    VkResult result = VK_NOT_READY;
    for (gp::UInt32 attempt = 0u; attempt < kMaxNotReadyRetries; ++attempt)
    {
        result = vkGetQueryPoolResults(
            m_device.getHandle(),
            m_queryPool,
            0u,
            slotCount,
            m_scratchResults.size() * sizeof(VulkanQueryResultSlot),
            m_scratchResults.data(),
            sizeof(VulkanQueryResultSlot),
            queryFlags
        );
        if (result != VK_NOT_READY)
        {
            break;
        }
        gp::platform::Thread::sleep(kNotReadyRetryDelayMs);
    }

    if (result < VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkGetQueryPoolResults failed with {}.", vulkanResultToString(result));
    }
    else if (result == VK_NOT_READY)
    {
        GP_LOG_WARNING(LogVulkanRHI, "Query results were still not ready after {} attempts.", kMaxNotReadyRetries);
    }

    // In multiview, one query covers one slot per view. Begin/end queries report values whose sum is the total over
    // all views. Timestamps either put the total in the first slot and zero in the others, or write one timestamp per
    // view: in both cases the maximum is the wanted value.
    const bool useMaxValue = (m_queryType == VulkanQueryPoolType::Timestamp);
    const bool hasResults = (result >= VK_SUCCESS);
    for (gp::UInt32 queryIndex = 0u; queryIndex < m_currentQueryCount; ++queryIndex)
    {
        gp::UInt64* const resultPtr = m_queryResults[queryIndex];
        if (resultPtr == nullptr)
        {
            continue;
        }

        gp::UInt64 queryResult = 0u;
        for (gp::UInt32 viewIndex = 0u; viewIndex < m_queryStride; ++viewIndex)
        {
            const VulkanQueryResultSlot& slot = m_scratchResults[(queryIndex * m_queryStride) + viewIndex];
            if (hasResults && (slot.availability != 0u))
            {
                queryResult =
                    useMaxValue ? gp::math::max<gp::UInt64>(queryResult, slot.result) : (queryResult + slot.result);
            }
            else
            {
                GP_LOG_WARNING(
                    LogVulkanRHI,
                    "Could not read back query: [PoolType={}, Index={}, View={}]",
                    static_cast<gp::UInt32>(m_queryType),
                    queryIndex,
                    viewIndex
                );
            }
        }
        *resultPtr = queryResult;
    }

    m_queryRefs.clear();
    return hasResults;
}

bool VulkanQueryPool::isStale() const noexcept
{
    return m_unusedFrameCount >= getVulkanQueryPoolDeletionDelay();
}

void VulkanQueryPool::resetReservations() noexcept
{
    const gp::UInt32 queryCapacity = m_maxQueries / m_queryStride;
    m_queryRefs.clear();
    m_queryRefs.resize(queryCapacity);
    m_queryResults.clear();
    m_queryResults.resizeZeroed(queryCapacity);
}

}   // namespace gp::rhi::vulkan
