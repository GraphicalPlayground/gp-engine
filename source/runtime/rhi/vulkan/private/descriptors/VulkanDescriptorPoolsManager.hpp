// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"                  // IWYU pragma: keep
#include "CoreMinimal.hpp"                               // IWYU pragma: keep
#include "descriptors/VulkanDescriptorPoolSetContainer.hpp"
#include "descriptors/VulkanDescriptorSetsForward.hpp"   // IWYU pragma: keep
#include "memory/pointers/UniquePtr.hpp"
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Device-wide recycler of descriptor pool set containers.
/// @details Contexts acquire a container per frame and release it when the GPU is done with it. Released containers
///          are reset and reused by the next acquirer, so a steady-state frame creates no descriptor pools at all.
///          `gc()` destroys at most one container per call, the most recently created unused one that has not been
///          acquired for more than `r.Vulkan.NumFramesToWaitForDescriptorDelete` frames, so pool memory follows the
///          load down after a spike without a frame hitch.
/// @note Thread-safe for `acquirePoolSetContainer()` and `gc()`. Frame numbers are passed in by the caller: the frame
///       counter lives in the dynamic RHI. The reference design destroys containers on a worker thread; here they are
///       destroyed synchronously, since `vkDestroyDescriptorPool` on an idle pool is cheap.
class GP_RHI_VULKAN_API VulkanDescriptorPoolsManager
{
public:
    /// @brief Creates a manager bound to no device.
    VulkanDescriptorPoolsManager() noexcept = default;

    /// @brief Destroys every container.
    ~VulkanDescriptorPoolsManager() noexcept = default;

    VulkanDescriptorPoolsManager(const VulkanDescriptorPoolsManager&) = delete;
    VulkanDescriptorPoolsManager& operator=(const VulkanDescriptorPoolsManager&) = delete;
    VulkanDescriptorPoolsManager(VulkanDescriptorPoolsManager&&) = delete;
    VulkanDescriptorPoolsManager& operator=(VulkanDescriptorPoolsManager&&) = delete;

    /// @brief Binds the manager to a device.
    /// @param[in] device Logical device used by every created container.
    void init(VulkanDevice& device) noexcept;

    /// @brief Returns an unused container, creating one when none is free.
    /// @param[in] currentFrame Current frame number.
    /// @return Container marked used; stays valid until released and collected.
    /// @note `init()` must have been called.
    [[nodiscard]] VulkanDescriptorPoolSetContainer& acquirePoolSetContainer(gp::UInt64 currentFrame) noexcept;

    /// @brief Resets a container and returns it to the free list.
    /// @param[in,out] poolSet Container acquired from this manager.
    void releasePoolSet(VulkanDescriptorPoolSetContainer& poolSet) noexcept;

    /// @brief Destroys one stale unused container, using `r.Vulkan.NumFramesToWaitForDescriptorDelete`.
    /// @param[in] currentFrame Current frame number.
    /// @return True when a container was destroyed.
    bool gc(gp::UInt64 currentFrame) noexcept;

    /// @brief Destroys one unused container not acquired for more than `framesToWait` frames.
    /// @param[in] currentFrame Current frame number.
    /// @param[in] framesToWait Minimum age, in frames, of a destroyed container.
    /// @return True when a container was destroyed.
    bool gc(gp::UInt64 currentFrame, gp::UInt32 framesToWait) noexcept;

    /// @brief Returns the number of containers, used or not.
    /// @return Container count.
    [[nodiscard]] gp::USize getNumPoolSetContainers() const noexcept;

private:
    /// @brief Logical device; null before `init()`.
    VulkanDevice* m_device{ nullptr };

    /// @brief Guards `m_poolSets`.
    mutable gp::thread::Mutex m_mutex{};

    /// @brief Containers in creation order.
    gp::Vector<gp::UniquePtr<VulkanDescriptorPoolSetContainer>> m_poolSets{};
};

}   // namespace gp::rhi::vulkan
