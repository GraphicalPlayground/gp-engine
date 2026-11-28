// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"                 // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "presentation/VulkanPresentationForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "threading/sync/Mutex.hpp"

namespace gp::rhi::vulkan
{

/// @brief Non-owning list of the viewports alive on a device.
/// @details Viewports are reference counted and owned by the application, which may still hold one when the RHI shuts
///          down. Their swapchain, surface and image views are device and instance objects, so they must be destroyed
///          before the device. Every viewport registers itself on construction and unregisters on destruction; at
///          shutdown the dynamic RHI calls `releaseAll()`, which strips every surviving viewport of its native objects
///          while the device still exists. The viewport objects themselves die later, when their last reference goes.
/// @note Thread-safe. `releaseAll()` empties the list under the lock and releases the viewports outside of it, so a
///       viewport locking its own mutex then this registry (its destructor) cannot deadlock against it.
class GP_RHI_VULKAN_API VulkanViewportRegistry final
{
public:
    /// @brief Creates an empty registry.
    VulkanViewportRegistry() noexcept = default;

    /// @brief Destroys the registry; every viewport must have been released or unregistered.
    ~VulkanViewportRegistry() noexcept;

    VulkanViewportRegistry(const VulkanViewportRegistry&) = delete;
    VulkanViewportRegistry& operator=(const VulkanViewportRegistry&) = delete;
    VulkanViewportRegistry(VulkanViewportRegistry&&) = delete;
    VulkanViewportRegistry& operator=(VulkanViewportRegistry&&) = delete;

    /// @brief Registers a viewport.
    /// @param[in] viewport Viewport to track; ignored when `nullptr` or already registered.
    void add(VulkanViewport* viewport) noexcept;

    /// @brief Unregisters a viewport.
    /// @param[in] viewport Viewport to forget.
    /// @return True when the viewport was registered.
    bool remove(VulkanViewport* viewport) noexcept;

    /// @brief Reports whether a viewport is registered.
    /// @param[in] viewport Viewport to look for.
    /// @return True when registered.
    [[nodiscard]] bool contains(const VulkanViewport* viewport) const noexcept;

    /// @brief Returns the number of registered viewports.
    /// @return Viewport count.
    [[nodiscard]] gp::USize getNumViewports() const noexcept;

    /// @brief Releases the native objects of every registered viewport and empties the registry.
    /// @details Calls `VulkanViewport::releaseNativeResources()` on each viewport. The device must be idle.
    /// @note Called by the dynamic RHI at shutdown, before the deferred deletion queue is flushed and the device dies.
    /// @return Number of viewports that were still alive.
    gp::USize releaseAll() noexcept;

private:
    /// @brief Guards `m_viewports`.
    mutable gp::thread::Mutex m_mutex{};

    /// @brief Live viewports, not referenced.
    gp::Vector<VulkanViewport*> m_viewports{};
};

}   // namespace gp::rhi::vulkan
