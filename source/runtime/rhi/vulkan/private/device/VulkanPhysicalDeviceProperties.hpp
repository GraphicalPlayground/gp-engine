// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

class VulkanPhysicalDeviceFeatures;

/// @brief Queries and retains Vulkan 1.0 through 1.4 physical device properties, identity UUID/LUID, and subgroup
///        properties.
class GP_RHI_VULKAN_API VulkanPhysicalDeviceProperties final
{
public:
    /// @brief Constructs zero-initialized Vulkan 1.0..1.4, identity, and subgroup property structs.
    VulkanPhysicalDeviceProperties() noexcept;

    /// @brief Queries properties from `physicalDevice` using `vkGetPhysicalDeviceProperties2`.
    /// @param[in] physicalDevice Target `VkPhysicalDevice`.
    /// @param[in] apiVersion Active Vulkan API version (`VK_API_VERSION_1_1`..`1_4`).
    void query(VkPhysicalDevice physicalDevice, gp::UInt32 apiVersion) noexcept;

    /// @brief Builds a `VulkanCoreFeaturesAndProperties` view binding `features` and this properties block for the
    ///        active API version.
    /// @param[in] features Queried physical device features.
    /// @return `VulkanCoreFeaturesAndProperties` with version-gated pointers populated.
    [[nodiscard]] VulkanCoreFeaturesAndProperties
        makeCoreFeaturesAndProperties(const VulkanPhysicalDeviceFeatures& features) const noexcept;

    /// @brief Returns the active Vulkan API version associated with these properties.
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

    /// @brief Returns Vulkan 1.0 core physical device properties.
    /// @return Const reference to `VkPhysicalDeviceProperties`.
    [[nodiscard]] const VkPhysicalDeviceProperties& getCore10() const noexcept
    {
        return m_core10.properties;
    }

    /// @brief Returns mutable Vulkan 1.0 core physical device properties.
    /// @return Mutable reference to `VkPhysicalDeviceProperties`.
    [[nodiscard]] VkPhysicalDeviceProperties& getCore10Mutable() noexcept
    {
        return m_core10.properties;
    }

    /// @brief Returns Vulkan 1.0 hardware limits.
    /// @return Const reference to `VkPhysicalDeviceLimits`.
    [[nodiscard]] const VkPhysicalDeviceLimits& getLimits() const noexcept
    {
        return m_core10.properties.limits;
    }

    /// @brief Returns Vulkan 1.1 core properties (populated when `m_apiVersion >= VK_API_VERSION_1_2`).
    /// @return Const reference to `VkPhysicalDeviceVulkan11Properties`.
    [[nodiscard]] const VkPhysicalDeviceVulkan11Properties& getCore11() const noexcept
    {
        return m_core11;
    }

    /// @brief Returns Vulkan 1.2 core properties (populated when `m_apiVersion >= VK_API_VERSION_1_2`).
    /// @return Const reference to `VkPhysicalDeviceVulkan12Properties`.
    [[nodiscard]] const VkPhysicalDeviceVulkan12Properties& getCore12() const noexcept
    {
        return m_core12;
    }

    /// @brief Returns Vulkan 1.3 core properties (populated when `m_apiVersion >= VK_API_VERSION_1_3`).
    /// @return Const reference to `VkPhysicalDeviceVulkan13Properties`.
    [[nodiscard]] const VkPhysicalDeviceVulkan13Properties& getCore13() const noexcept
    {
        return m_core13;
    }

    /// @brief Returns Vulkan 1.4 core properties (populated when `m_apiVersion >= VK_API_VERSION_1_4`).
    /// @return Const reference to `VkPhysicalDeviceVulkan14Properties`.
    [[nodiscard]] const VkPhysicalDeviceVulkan14Properties& getCore14() const noexcept
    {
        return m_core14;
    }

    /// @brief Returns Vulkan 1.1+ physical device UUID/LUID identity properties.
    /// @return Const reference to `VkPhysicalDeviceIDProperties`.
    [[nodiscard]] const VkPhysicalDeviceIDProperties& getId() const noexcept
    {
        return m_id;
    }

    /// @brief Returns mutable identity properties.
    /// @return Mutable reference to `VkPhysicalDeviceIDProperties`.
    [[nodiscard]] VkPhysicalDeviceIDProperties& getIdMutable() noexcept
    {
        return m_id;
    }

    /// @brief Returns Vulkan 1.1+ subgroup (wave) properties.
    /// @return Const reference to `VkPhysicalDeviceSubgroupProperties`.
    [[nodiscard]] const VkPhysicalDeviceSubgroupProperties& getSubgroup() const noexcept
    {
        return m_subgroup;
    }

    /// @brief Returns mutable subgroup properties.
    /// @return Mutable reference to `VkPhysicalDeviceSubgroupProperties`.
    [[nodiscard]] VkPhysicalDeviceSubgroupProperties& getSubgroupMutable() noexcept
    {
        return m_subgroup;
    }

private:
    gp::UInt32 m_apiVersion{ 0u };
    VkPhysicalDeviceProperties2 m_core10{};
    VkPhysicalDeviceVulkan11Properties m_core11{};
    VkPhysicalDeviceVulkan12Properties m_core12{};
    VkPhysicalDeviceVulkan13Properties m_core13{};
    VkPhysicalDeviceVulkan14Properties m_core14{};
    VkPhysicalDeviceIDProperties m_id{};
    VkPhysicalDeviceSubgroupProperties m_subgroup{};
};

}   // namespace gp::rhi::vulkan
