// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "queries/VulkanQueryEnums.hpp"
#include "queries/VulkanQueryForward.hpp"   // IWYU pragma: keep
#include "queries/VulkanQueryTypes.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Native `VkQueryPool` plus the destination of every query reserved in it since the last reset.
/// @details Queries are reserved in order while commands are recorded. Once the payload that recorded them
///          completes, `resolveResults()` reads every slot back in a single call and writes each value to its
///          destination. In a multiview render pass a query occupies `getQueryStride()` consecutive slots, one per
///          view.
/// @note Not thread-safe: a pool is used by one recording context at a time.
class GP_RHI_VULKAN_API VulkanQueryPool final
{
public:
    /// @brief Creates the native pool.
    /// @details With `VK_KHR_maintenance9`, the pool is created already reset, so it starts clean.
    /// @param[in] device Initialized device.
    /// @param[in] maxQueries Number of slots in the pool.
    /// @param[in] queryType Pool category.
    VulkanQueryPool(VulkanDevice& device, gp::UInt32 maxQueries, VulkanQueryPoolType queryType) noexcept;

    /// @brief Destroys the native pool.
    /// @note The GPU must no longer reference it.
    ~VulkanQueryPool() noexcept;

    VulkanQueryPool(const VulkanQueryPool&) = delete;
    VulkanQueryPool& operator=(const VulkanQueryPool&) = delete;
    VulkanQueryPool(VulkanQueryPool&&) = delete;
    VulkanQueryPool& operator=(VulkanQueryPool&&) = delete;

    /// @brief Reports whether the native pool was created.
    /// @return True when `getHandle()` is valid.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_queryPool != VK_NULL_HANDLE;
    }

    /// @brief Returns the number of slots.
    /// @return Slot count.
    [[nodiscard]] gp::UInt32 getMaxQueries() const noexcept
    {
        return m_maxQueries;
    }

    /// @brief Returns the native pool.
    /// @return Pool handle.
    [[nodiscard]] VkQueryPool getHandle() const noexcept
    {
        return m_queryPool;
    }

    /// @brief Returns the pool category.
    /// @return Pool type.
    [[nodiscard]] VulkanQueryPoolType getPoolType() const noexcept
    {
        return m_queryType;
    }

    /// @brief Returns the owning device.
    /// @return Device.
    [[nodiscard]] VulkanDevice& getDevice() const noexcept
    {
        return m_device;
    }

    /// @brief Returns the number of slots each query occupies.
    /// @return Query stride (the multiview count, or 1).
    [[nodiscard]] gp::UInt32 getQueryStride() const noexcept
    {
        return m_queryStride;
    }

    /// @brief Returns the number of queries reserved since the last reset.
    /// @return Reserved query count.
    [[nodiscard]] gp::UInt32 getCurrentQueryCount() const noexcept
    {
        return m_currentQueryCount;
    }

    /// @brief Reports whether no further query can be reserved.
    /// @return True when every slot is used.
    [[nodiscard]] bool isFull() const noexcept
    {
        return (m_currentQueryCount * m_queryStride) >= m_maxQueries;
    }

    /// @brief Reserves the next query for `query` and records where its result goes.
    /// @param[in,out] query Query of the same category; receives its `indexInPool`.
    void reserveQuery(VulkanRenderQuery& query) noexcept;

    /// @brief Reserves the next timestamp query and records where its result goes.
    /// @param[in] resultPtr Destination of the raw timestamp; must outlive the payload.
    /// @return First slot of the reserved query.
    [[nodiscard]] gp::UInt32 reserveQuery(gp::UInt64* resultPtr) noexcept;

    /// @brief Resets every slot and forgets all reservations.
    /// @param[in] cmdBuffer Command buffer recording `vkCmdResetQueryPool`, or `VK_NULL_HANDLE` for a host reset
    ///                      (requires `VK_EXT_host_query_reset`).
    /// @param[in] queryStride Number of slots each query occupies from now on.
    void reset(VkCommandBuffer cmdBuffer, gp::UInt32 queryStride = 1u) noexcept;

    /// @brief Reads every reserved query back and writes each value to its destination.
    /// @details Uses `VK_QUERY_RESULT_WITH_AVAILABILITY_BIT` without `VK_QUERY_RESULT_WAIT_BIT` and retries while the
    ///          driver reports `VK_NOT_READY`, because a wait can hang forever on some drivers after a device loss.
    ///          Per-view values are summed for occlusion and reduced to the maximum for timestamps. Timestamps stay
    ///          raw GPU ticks. Every query reference is dropped afterwards.
    /// @return True when the results were read.
    bool resolveResults() noexcept;

    /// @brief Reports whether the pool stayed unused for longer than `r.Vulkan.QueryPoolDeletionDelay` frames.
    /// @return True when the pool can be destroyed.
    [[nodiscard]] bool isStale() const noexcept;

    /// @brief Counts one more frame without use.
    void incrementUnusedFrameCount() noexcept
    {
        ++m_unusedFrameCount;
    }

    /// @brief Returns the number of frames the pool stayed unused.
    /// @return Unused frame count.
    [[nodiscard]] gp::Int32 getUnusedFrameCount() const noexcept
    {
        return m_unusedFrameCount;
    }

    /// @brief Marks the pool as needing a reset before reuse.
    void setDirty() noexcept
    {
        m_isDirty = true;
    }

    /// @brief Reports whether the pool needs a reset before reuse.
    /// @return True when dirty.
    [[nodiscard]] bool isDirty() const noexcept
    {
        return m_isDirty;
    }

private:
    /// @brief Sizes the reservation tables for the current stride.
    void resetReservations() noexcept;

private:
    /// @brief Owning device.
    VulkanDevice& m_device;

    /// @brief Native pool.
    VkQueryPool m_queryPool{ VK_NULL_HANDLE };

    /// @brief Number of slots.
    const gp::UInt32 m_maxQueries;

    /// @brief Pool category.
    const VulkanQueryPoolType m_queryType;

    /// @brief Queries kept alive until their results are written; null for raw timestamp destinations.
    gp::Vector<gp::RefCountedPtr<VulkanRenderQuery>> m_queryRefs{};

    /// @brief Result destination of each reserved query.
    gp::Vector<gp::UInt64*> m_queryResults{};

    /// @brief Readback scratch, one entry per slot.
    gp::Vector<VulkanQueryResultSlot> m_scratchResults{};

    /// @brief Number of queries reserved since the last reset.
    gp::UInt32 m_currentQueryCount{ 0u };

    /// @brief Slots per query: in a multiview render pass each query covers every view.
    gp::UInt32 m_queryStride{ 1u };

    /// @brief Frames since the pool was last reset.
    gp::Int32 m_unusedFrameCount{ 0 };

    /// @brief True when the slots must be reset before reuse.
    bool m_isDirty{ true };
};

}   // namespace gp::rhi::vulkan
