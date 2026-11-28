// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Queries, filters, and chains Vulkan 1.0 through 1.4 physical device feature structs for `vkCreateDevice`.
class GP_RHI_VULKAN_API VulkanPhysicalDeviceFeatures final
{
public:
    /// @brief Constructs zero-initialized Vulkan 1.0..1.4 feature structs with valid `sType` headers.
    VulkanPhysicalDeviceFeatures() noexcept;

    /// @brief Queries features from `physicalDevice` using `vkGetPhysicalDeviceFeatures2` and strips unused features.
    /// @param[in] physicalDevice Target `VkPhysicalDevice`.
    /// @param[in] apiVersion Active Vulkan API version (`VK_API_VERSION_1_1`..`1_4`).
    void query(VkPhysicalDevice physicalDevice, gp::UInt32 apiVersion) noexcept;

    /// @brief Disables unused or CVar-gated features (`r.Vulkan.RobustBufferAccess`, `r.Vulkan.AllowImageRobustness`,
    ///        `r.Vulkan.AllowSparseResidency`, and unused core 1.1..1.4 features).
    void removeUnusedFeatures() noexcept;

    /// @brief Attaches enabled feature structs to `deviceInfo` either via core `pNext` chains (Vulkan 1.2+ when
    ///        `useCoreStructs` is true) or via `deviceInfo.pEnabledFeatures`.
    /// @param[in,out] deviceInfo Logical device creation descriptor being assembled.
    /// @param[in] useCoreStructs True to attach `Core10..Core14` structs on `pNext` instead of `pEnabledFeatures`.
    void addToCreateDevice(VkDeviceCreateInfo& deviceInfo, bool useCoreStructs) noexcept;

    /// @brief Returns the active Vulkan API version associated with these features.
    /// @return Packed Vulkan API version.
    [[nodiscard]] gp::UInt32 getApiVersion() const noexcept
    {
        return m_apiVersion;
    }

    /// @brief Sets the API version explicitly (primarily for deterministic unit testing).
    /// @param[in] apiVersion Packed Vulkan API version.
    void setApiVersion(const gp::UInt32 apiVersion) noexcept
    {
        m_apiVersion = apiVersion;
    }

    /// @brief Returns Vulkan 1.0 core physical device features.
    /// @return Const reference to `VkPhysicalDeviceFeatures`.
    [[nodiscard]] const VkPhysicalDeviceFeatures& getCore10() const noexcept
    {
        return m_core10.features;
    }

    /// @brief Returns mutable Vulkan 1.0 core physical device features.
    /// @return Mutable reference to `VkPhysicalDeviceFeatures`.
    [[nodiscard]] VkPhysicalDeviceFeatures& getCore10Mutable() noexcept
    {
        return m_core10.features;
    }

    /// @brief Returns Vulkan 1.1 core features (populated when `m_apiVersion >= VK_API_VERSION_1_2`).
    /// @return Const reference to `VkPhysicalDeviceVulkan11Features`.
    [[nodiscard]] const VkPhysicalDeviceVulkan11Features& getCore11() const noexcept
    {
        return m_core11;
    }

    /// @brief Returns mutable Vulkan 1.1 core features.
    /// @return Mutable reference to `VkPhysicalDeviceVulkan11Features`.
    [[nodiscard]] VkPhysicalDeviceVulkan11Features& getCore11Mutable() noexcept
    {
        return m_core11;
    }

    /// @brief Returns Vulkan 1.2 core features (populated when `m_apiVersion >= VK_API_VERSION_1_2`).
    /// @return Const reference to `VkPhysicalDeviceVulkan12Features`.
    [[nodiscard]] const VkPhysicalDeviceVulkan12Features& getCore12() const noexcept
    {
        return m_core12;
    }

    /// @brief Returns mutable Vulkan 1.2 core features.
    /// @return Mutable reference to `VkPhysicalDeviceVulkan12Features`.
    [[nodiscard]] VkPhysicalDeviceVulkan12Features& getCore12Mutable() noexcept
    {
        return m_core12;
    }

    /// @brief Returns Vulkan 1.3 core features (populated when `m_apiVersion >= VK_API_VERSION_1_3`).
    /// @return Const reference to `VkPhysicalDeviceVulkan13Features`.
    [[nodiscard]] const VkPhysicalDeviceVulkan13Features& getCore13() const noexcept
    {
        return m_core13;
    }

    /// @brief Returns mutable Vulkan 1.3 core features.
    /// @return Mutable reference to `VkPhysicalDeviceVulkan13Features`.
    [[nodiscard]] VkPhysicalDeviceVulkan13Features& getCore13Mutable() noexcept
    {
        return m_core13;
    }

    /// @brief Returns Vulkan 1.4 core features (populated when `m_apiVersion >= VK_API_VERSION_1_4`).
    /// @return Const reference to `VkPhysicalDeviceVulkan14Features`.
    [[nodiscard]] const VkPhysicalDeviceVulkan14Features& getCore14() const noexcept
    {
        return m_core14;
    }

    /// @brief Returns mutable Vulkan 1.4 core features.
    /// @return Mutable reference to `VkPhysicalDeviceVulkan14Features`.
    [[nodiscard]] VkPhysicalDeviceVulkan14Features& getCore14Mutable() noexcept
    {
        return m_core14;
    }

private:
    gp::UInt32 m_apiVersion{ 0u };
    VkPhysicalDeviceFeatures2 m_core10{};
    VkPhysicalDeviceVulkan11Features m_core11{};
    VkPhysicalDeviceVulkan12Features m_core12{};
    VkPhysicalDeviceVulkan13Features m_core13{};
    VkPhysicalDeviceVulkan14Features m_core14{};
};

}   // namespace gp::rhi::vulkan
