// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "queries/VulkanQueryEnums.hpp"
#include "queries/VulkanQueryForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief One 64-bit result followed by its 64-bit availability word.
/// @details Matches the layout `vkGetQueryPoolResults` writes with `VK_QUERY_RESULT_64_BIT |
///          VK_QUERY_RESULT_WITH_AVAILABILITY_BIT` and a stride of `sizeof(VulkanQueryResultSlot)`.
struct VulkanQueryResultSlot final
{
    /// @brief Query value.
    gp::UInt64 result{ 0u };

    /// @brief Non-zero when `result` is available.
    gp::UInt64 availability{ 0u };
};

static_assert(sizeof(VulkanQueryResultSlot) == 16u, "VulkanQueryResultSlot must be two packed 64-bit words.");

/// @brief Query pools a payload recorded into, grouped by type.
/// @details The pools stay owned by the `VulkanQueryPoolManager`; the payload only borrows them until its results are
///          read back on completion.
struct VulkanPayloadQueryPools final
{
    /// @brief Borrowed pools per type, in recording order.
    gp::Vector<VulkanQueryPool*> pools[kVulkanQueryPoolTypeCount]{};

    /// @brief Returns the pools recorded for `type`.
    /// @param[in] type Pool category.
    /// @return Mutable pool list.
    [[nodiscard]] gp::Vector<VulkanQueryPool*>& get(const VulkanQueryPoolType type) noexcept
    {
        return pools[static_cast<gp::USize>(type)];
    }

    /// @brief Returns the pools recorded for `type`.
    /// @param[in] type Pool category.
    /// @return Immutable pool list.
    [[nodiscard]] const gp::Vector<VulkanQueryPool*>& get(const VulkanQueryPoolType type) const noexcept
    {
        return pools[static_cast<gp::USize>(type)];
    }

    /// @brief Reports whether no pool of any type is recorded.
    /// @return True when every list is empty.
    [[nodiscard]] bool isEmpty() const noexcept
    {
        for (gp::USize index = 0u; index < kVulkanQueryPoolTypeCount; ++index)
        {
            if (!pools[index].isEmpty())
            {
                return false;
            }
        }
        return true;
    }

    /// @brief Moves every pool of `other` to the end of the matching lists.
    /// @param[in,out] other Pools to take; left empty.
    void append(VulkanPayloadQueryPools& other) noexcept
    {
        for (gp::USize typeIndex = 0u; typeIndex < kVulkanQueryPoolTypeCount; ++typeIndex)
        {
            gp::Vector<VulkanQueryPool*>& source = other.pools[typeIndex];
            for (gp::USize index = 0u; index < source.size(); ++index)
            {
                pools[typeIndex].pushBack(source[index]);
            }
            source.clear();
        }
    }
};

}   // namespace gp::rhi::vulkan
