// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "memory/pointers/IntrusiveRefCounted.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Reference-counted owner of one `VkShaderModule`.
/// @details A shader keeps one module per pipeline layout hash (and per input attachment patch), and pipelines that
///          are still being compiled hold their own reference, so modules outlive the shader that created them when
///          needed. The handle is released through the deferred deletion queue when one is bound, which keeps it alive
///          until in-flight pipeline creation that references it has completed.
class GP_RHI_VULKAN_API VulkanShaderModule final : public gp::IntrusiveRefCounted<VulkanShaderModule>
{
public:
    /// @brief Creates the native module from SPIR-V words.
    /// @details Creation is skipped (and the handle stays null) on an offline device or when the entry point is not
    ///          loaded; failures are logged.
    /// @param[in] device Logical device that owns the module.
    /// @param[in] spirv SPIR-V words.
    /// @param[in] deferredDeletionQueue Queue that releases the handle; `nullptr` destroys it immediately.
    VulkanShaderModule(
        VulkanDevice& device, gp::ConstVectorView<gp::UInt32> spirv, VulkanDeferredDeletionQueue* deferredDeletionQueue
    ) noexcept;

    /// @brief Releases the native module.
    ~VulkanShaderModule() noexcept;

    VulkanShaderModule(const VulkanShaderModule&) = delete;
    VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;
    VulkanShaderModule(VulkanShaderModule&&) = delete;
    VulkanShaderModule& operator=(VulkanShaderModule&&) = delete;

    /// @brief Returns the native module.
    /// @return `VkShaderModule`, or `VK_NULL_HANDLE` when creation was skipped or failed.
    [[nodiscard]] VkShaderModule getHandle() const noexcept
    {
        return m_module;
    }

    /// @brief Reports whether the native module exists.
    /// @return True when `getHandle()` is not null.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_module != VK_NULL_HANDLE;
    }

private:
    /// @brief Logical device that owns the module.
    VulkanDevice& m_device;

    /// @brief Queue that releases the handle; `nullptr` destroys it immediately.
    VulkanDeferredDeletionQueue* m_deferredDeletionQueue{ nullptr };

    /// @brief Native module.
    VkShaderModule m_module{ VK_NULL_HANDLE };
};

}   // namespace gp::rhi::vulkan
