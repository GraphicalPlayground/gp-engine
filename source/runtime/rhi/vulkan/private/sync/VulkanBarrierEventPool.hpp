// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierForward.hpp"   // IWYU pragma: keep
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Thread-safe pool recycling device-only `VkEvent` handles used by split barriers.
/// @details A split barrier signals an event at `beginTransitions()` and waits on it at `endTransitions()`. The
///          command buffer resets the event before it ends and hands it back here once the GPU has finished with the
///          buffer, so each event is created once and reused for the lifetime of the device.
/// @note Every method may be called from any thread. Events are created with `VK_EVENT_CREATE_DEVICE_ONLY_BIT`, so
///       the host never sets or waits on them.
class GP_RHI_VULKAN_API VulkanBarrierEventPool final
{
public:
    /// @brief Constructs an empty pool.
    VulkanBarrierEventPool() noexcept = default;

    /// @brief Destructor. Every pooled event must already have been destroyed through `destroy()`.
    ~VulkanBarrierEventPool() noexcept;

    VulkanBarrierEventPool(const VulkanBarrierEventPool&) = delete;
    VulkanBarrierEventPool& operator=(const VulkanBarrierEventPool&) = delete;
    VulkanBarrierEventPool(VulkanBarrierEventPool&&) = delete;
    VulkanBarrierEventPool& operator=(VulkanBarrierEventPool&&) = delete;

    /// @brief Pops a recycled event, or creates a new device-only event when the pool is empty.
    /// @param[in] device Logical device owning the events.
    /// @return Unsignaled `VkEvent`, or `VK_NULL_HANDLE` if creation failed.
    [[nodiscard]] VkEvent acquire(VkDevice device) noexcept;

    /// @brief Returns an event to the pool.
    /// @note The event must already be reset on the GPU timeline; the owning command buffer records the reset when it
    ///       ends.
    /// @param[in] event Event previously obtained from `acquire()` (`VK_NULL_HANDLE` is ignored).
    void release(VkEvent event) noexcept;

    /// @brief Destroys every pooled event.
    /// @note Call after the device is idle; events still in flight are not tracked by the pool.
    /// @param[in] device Logical device that created the events.
    void destroy(VkDevice device) noexcept;

    /// @brief Returns the number of events currently waiting in the pool.
    /// @return Pooled event count.
    [[nodiscard]] gp::USize getPooledCount() const noexcept;

    /// @brief Returns the number of events created over the pool's lifetime.
    /// @return Total created event count.
    [[nodiscard]] gp::USize getCreatedCount() const noexcept;

private:
    mutable gp::thread::Mutex m_mutex{};
    gp::Vector<VkEvent> m_events{};
    gp::USize m_createdCount{ 0u };
};

}   // namespace gp::rhi::vulkan
