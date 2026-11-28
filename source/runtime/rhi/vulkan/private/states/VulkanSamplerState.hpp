// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "pipeline/SamplerState.hpp"
#include "RHIVulkan.hpp"
#include "views/DescriptorHandle.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Sampler state owning one `VkSampler`.
/// @details Samplers are immutable and deduplicated: `VulkanSamplerStateCache` hashes the translated
///          `VkSamplerCreateInfo` and hands out the same object for identical descriptions. The native handle is
///          released when the last reference goes away, through the deferred deletion queue when one is bound.
class GP_RHI_VULKAN_API VulkanSamplerState final : public pipeline::SamplerState
{
public:
    /// @brief Creates the native sampler.
    /// @param[in] device Logical device that owns the sampler.
    /// @param[in] createInfo Translated create info, usually from `setupSamplerCreateInfo`.
    /// @param[in] deferredDeletionQueue Queue that releases the handle; `nullptr` destroys it immediately.
    /// @param[in] isImmutable True when the sampler is baked into descriptor set layouts as an immutable sampler.
    VulkanSamplerState(
        VulkanDevice& device,
        const VkSamplerCreateInfo& createInfo,
        VulkanDeferredDeletionQueue* deferredDeletionQueue,
        bool isImmutable = false
    ) noexcept;

    /// @brief Translates an RHI sampler description into a `VkSamplerCreateInfo`.
    /// @details Anisotropic filters resolve their tap count with `computeVulkanSamplerAnisotropy`, then clamp it to
    ///          `maxSamplerAnisotropy`. Anisotropy is only enabled above one tap and when the device feature is on.
    ///          A zero border color is transparent black, anything else is opaque white.
    /// @param[in] desc RHI sampler description.
    /// @param[in] device Device providing the anisotropy limit and feature.
    /// @param[out] outCreateInfo Receives a fully initialized create info with no `pNext` chain.
    static void setupSamplerCreateInfo(
        const pipeline::SamplerStateDesc& desc, const VulkanDevice& device, VkSamplerCreateInfo& outCreateInfo
    ) noexcept;

    /// @brief Computes the deduplication hash of a create info.
    /// @param[in] createInfo Create info to hash; its `pNext` must be null.
    /// @return CRC32 of the create info bytes.
    [[nodiscard]] static gp::UInt32 computeCreateInfoHash(const VkSamplerCreateInfo& createInfo) noexcept;

    /// @copydoc pipeline::SamplerState::isImmutable()
    [[nodiscard]] virtual bool isImmutable() const noexcept override;

    /// @copydoc pipeline::SamplerState::getBindlessHandle()
    /// @note Always invalid until bindless descriptors exist.
    [[nodiscard]] virtual view::DescriptorHandle getBindlessHandle() const noexcept override;

    /// @brief Returns the native sampler.
    /// @return `VkSampler`, or `VK_NULL_HANDLE` when creation failed.
    [[nodiscard]] VkSampler getHandle() const noexcept
    {
        return m_sampler;
    }

    /// @brief Returns the process-unique identifier used by descriptor caches.
    /// @return Non-zero identifier.
    [[nodiscard]] gp::UInt32 getSamplerId() const noexcept
    {
        return m_samplerId;
    }

protected:
    /// @brief Releases the native sampler.
    virtual ~VulkanSamplerState() noexcept override;

private:
    /// @brief Logical device that owns the sampler.
    VulkanDevice& m_device;

    /// @brief Queue that releases the handle; `nullptr` destroys it immediately.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Native sampler.
    VkSampler m_sampler{ VK_NULL_HANDLE };

    /// @brief Process-unique identifier.
    gp::UInt32 m_samplerId{ 0u };

    /// @brief Bindless slot; invalid until bindless descriptors exist.
    view::DescriptorHandle m_bindlessHandle{};

    /// @brief True for immutable samplers baked into layouts.
    bool m_isImmutable{ false };
};

}   // namespace gp::rhi::vulkan
