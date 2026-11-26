// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"     // IWYU pragma: keep
#include "CoreMinimal.hpp"                  // IWYU pragma: keep
#include "queries/VulkanQueryEnums.hpp"
#include "queries/VulkanQueryForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"

namespace gp::rhi::vulkan
{

/// @brief Device-wide cache of idle query pools, shared by every recording context.
/// @details Contexts acquire pools while recording and payloads give them back once their results were read. Idle
///          pools are kept for `r.Vulkan.QueryPoolDeletionDelay` frames, so steady-state frames never create pools.
///          Occlusion pools all share one size that only grows, sized to the largest batch seen so far.
/// @note Thread-safe.
class GP_RHI_VULKAN_API VulkanQueryPoolManager final
{
public:
    /// @brief Granularity of the occlusion pool size.
    static constexpr gp::UInt32 kOcclusionPoolSizeGranularity = 256u;

    /// @brief Creates an empty manager.
    /// @param[in] device Initialized device.
    explicit VulkanQueryPoolManager(VulkanDevice& device) noexcept;

    /// @brief Destroys every idle pool.
    /// @note Pools still borrowed by payloads must have been released first.
    ~VulkanQueryPoolManager() noexcept;

    VulkanQueryPoolManager(const VulkanQueryPoolManager&) = delete;
    VulkanQueryPoolManager& operator=(const VulkanQueryPoolManager&) = delete;
    VulkanQueryPoolManager(VulkanQueryPoolManager&&) = delete;
    VulkanQueryPoolManager& operator=(VulkanQueryPoolManager&&) = delete;

    /// @brief Returns an occlusion pool able to hold at least `numQueries` slots.
    /// @details Raises the shared occlusion pool size when needed and destroys idle pools that became too small.
    /// @param[in] numQueries Slots needed by the batch (query count times multiview count).
    /// @return Borrowed pool; may be dirty.
    [[nodiscard]] VulkanQueryPool* acquireOcclusionQueryPool(gp::UInt32 numQueries) noexcept;

    /// @brief Returns a timestamp pool of `r.Vulkan.TimingQueryPoolSize` slots.
    /// @return Borrowed pool; may be dirty.
    [[nodiscard]] VulkanQueryPool* acquireTimingQueryPool() noexcept;

    /// @brief Takes back a pool and marks it dirty.
    /// @param[in] pool Pool returned by an acquire call.
    void releaseQueryPool(VulkanQueryPool* pool) noexcept;

    /// @brief Destroys idle pools that stayed unused for too long and ages the others by one frame.
    /// @note Call once per frame.
    void removeStaleQueryPools() noexcept;

    /// @brief Returns the number of idle pools of `type`.
    /// @param[in] type Pool category.
    /// @return Idle pool count.
    [[nodiscard]] gp::USize getFreePoolCount(VulkanQueryPoolType type) const noexcept;

    /// @brief Returns the current shared occlusion pool size.
    /// @return Slot count, or 0 before the first occlusion batch.
    [[nodiscard]] gp::UInt32 getOcclusionQueryPoolSize() const noexcept;

    /// @brief Returns the device pools are created on.
    /// @return Device.
    [[nodiscard]] VulkanDevice& getDevice() const noexcept
    {
        return m_device;
    }

private:
    /// @brief Device pools are created on.
    VulkanDevice& m_device;

    /// @brief Guards every member below.
    mutable gp::thread::Mutex m_mutex{};

    /// @brief Size of every occlusion pool.
    gp::UInt32 m_occlusionQueryPoolSize{ 0u };

    /// @brief Idle pools per type.
    gp::Vector<VulkanQueryPool*> m_freeQueryPools[kVulkanQueryPoolTypeCount]{};
};

}   // namespace gp::rhi::vulkan
