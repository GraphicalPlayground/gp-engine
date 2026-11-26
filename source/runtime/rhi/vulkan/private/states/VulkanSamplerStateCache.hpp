// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"    // IWYU pragma: keep
#include "CoreMinimal.hpp"                 // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "pipeline/PipelineForward.hpp"    // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "states/VulkanStateForward.hpp"   // IWYU pragma: keep
#include "threading/sync/Mutex.hpp"
#include "VulkanForward.hpp"               // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Device-wide cache that hands out one `VulkanSamplerState` per distinct `VkSamplerCreateInfo`.
/// @details Two RHI descriptions that translate to the same native create info (for example two point samplers
///          that differ only by `maxAnisotropy`) share one `VkSampler`. Lookups hash the translated create info with
///          CRC32 and scan a flat vector under a mutex; the sampler count of a frame is small and creation is rare.
///          The cache keeps a reference to every sampler until `reset()` or destruction, so samplers live as long as
///          the device does.
/// @note Thread safe. The owner (the dynamic RHI, or a test fixture) keeps the device and the deletion queue alive
///       for longer than the cache.
class GP_RHI_VULKAN_API VulkanSamplerStateCache final
{
public:
    /// @brief Binds the cache to its device.
    /// @param[in] device Logical device that owns the samplers.
    /// @param[in] deferredDeletionQueue Queue that releases sampler handles; `nullptr` destroys them immediately.
    explicit VulkanSamplerStateCache(
        VulkanDevice& device, VulkanDeferredDeletionQueue* deferredDeletionQueue = nullptr
    ) noexcept;

    /// @brief Drops every cached reference.
    ~VulkanSamplerStateCache() noexcept;

    VulkanSamplerStateCache(const VulkanSamplerStateCache&) = delete;
    VulkanSamplerStateCache& operator=(const VulkanSamplerStateCache&) = delete;
    VulkanSamplerStateCache(VulkanSamplerStateCache&&) = delete;
    VulkanSamplerStateCache& operator=(VulkanSamplerStateCache&&) = delete;

    /// @brief Returns the sampler matching `desc`, creating it on first use.
    /// @param[in] desc RHI sampler description.
    /// @return Shared sampler state; never null, but its handle is null when native creation failed.
    [[nodiscard]] pipeline::SamplerStateRef findOrCreate(const pipeline::SamplerStateDesc& desc) noexcept;

    /// @brief Returns the number of distinct samplers created so far.
    /// @return Cached sampler count.
    [[nodiscard]] gp::USize getNumSamplers() const noexcept;

    /// @brief Drops every cached reference; samplers still referenced elsewhere stay alive.
    void reset() noexcept;

private:
    /// @brief One cached sampler and the hash of its create info.
    struct Entry final
    {
        /// @brief CRC32 of the translated `VkSamplerCreateInfo`.
        gp::UInt32 createInfoHash{ 0u };

        /// @brief Cached sampler.
        gp::RefCountedPtr<VulkanSamplerState> sampler{};
    };

private:
    /// @brief Logical device that owns the samplers.
    VulkanDevice& m_device;

    /// @brief Queue that releases sampler handles.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Guards `m_entries`.
    mutable gp::thread::Mutex m_mutex{};

    /// @brief Cached samplers in creation order.
    gp::Vector<Entry> m_entries{};
};

}   // namespace gp::rhi::vulkan
