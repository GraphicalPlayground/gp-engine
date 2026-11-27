// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "extensions/VulkanDeviceExtension.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Descriptor for `VK_KHR_create_renderpass2` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanKHRCreateRenderPass2Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_create_renderpass2` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRCreateRenderPass2Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects `vkCreateRenderPass2KHR`, `vkCmdBeginRenderPass2KHR`, and `vkCmdEndRenderPass2KHR` to core.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;
};

/// @brief Descriptor for `VK_EXT_tooling_info` (promoted to Vulkan 1.3).
class GP_RHI_VULKAN_API VulkanEXTToolingInfoExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_tooling_info` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTToolingInfoExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects `vkGetPhysicalDeviceToolPropertiesEXT` to core Vulkan 1.3.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;
};

/// @brief Descriptor for `VK_KHR_driver_properties` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanKHRDriverPropertiesExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_driver_properties` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRDriverPropertiesExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceProperties()
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceProperties()
    virtual void postPhysicalDeviceProperties(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceDriverProperties m_driverProperties{};
};

/// @brief Descriptor for `VK_KHR_maintenance4` (promoted to Vulkan 1.3).
class GP_RHI_VULKAN_API VulkanKHRMaintenance4Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_maintenance4` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRMaintenance4Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceProperties()
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects `vkGetDeviceImageMemoryRequirementsKHR` and `vkGetDeviceBufferMemoryRequirementsKHR` to core.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceMaintenance4Features m_features{};
    VkPhysicalDeviceMaintenance4Properties m_properties{};
};

/// @brief Descriptor for `VK_KHR_maintenance5` (promoted to Vulkan 1.4).
class GP_RHI_VULKAN_API VulkanKHRMaintenance5Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_maintenance5` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRMaintenance5Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects `vkGetImageSubresourceLayout2KHR` to core Vulkan 1.4.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceMaintenance5FeaturesKHR m_features{};
};

/// @brief Descriptor for `VK_KHR_maintenance6` (promoted to Vulkan 1.4).
class GP_RHI_VULKAN_API VulkanKHRMaintenance6Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_maintenance6` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRMaintenance6Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects `vkCmdPushDescriptorSet2KHR` to core Vulkan 1.4.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceMaintenance6FeaturesKHR m_features{};
};

/// @brief Descriptor for `VK_KHR_maintenance7`.
class GP_RHI_VULKAN_API VulkanKHRMaintenance7Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_maintenance7` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRMaintenance7Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    VkPhysicalDeviceMaintenance7FeaturesKHR m_features{};
};

/// @brief Descriptor for `VK_KHR_maintenance9`.
class GP_RHI_VULKAN_API VulkanKHRMaintenance9Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_maintenance9` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRMaintenance9Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    VkPhysicalDeviceMaintenance9FeaturesKHR m_features{};
};

/// @brief Descriptor for `VK_KHR_shader_atomic_int64` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanKHRShaderAtomicInt64Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_shader_atomic_int64` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRShaderAtomicInt64Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceShaderAtomicInt64Features m_features{};
};

/// @brief Descriptor for `VK_EXT_shader_image_atomic_int64`.
class GP_RHI_VULKAN_API VulkanEXTShaderImageAtomicInt64Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_shader_image_atomic_int64` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTShaderImageAtomicInt64Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT m_features{};
};

/// @brief Descriptor for `VK_EXT_scalar_block_layout` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanEXTScalarBlockLayoutExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_scalar_block_layout` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTScalarBlockLayoutExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceScalarBlockLayoutFeatures m_features{};
};

/// @brief Descriptor for `VK_EXT_shader_viewport_index_layer` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanEXTShaderViewportIndexLayerExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_shader_viewport_index_layer` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTShaderViewportIndexLayerExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;
};

/// @brief Descriptor for `VK_KHR_separate_depth_stencil_layouts` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanKHRSeparateDepthStencilLayoutsExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_separate_depth_stencil_layouts` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRSeparateDepthStencilLayoutsExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures m_features{};
};

/// @brief Descriptor for `VK_KHR_synchronization2` (promoted to Vulkan 1.3).
class GP_RHI_VULKAN_API VulkanKHRSynchronization2Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_synchronization2` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRSynchronization2Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects barrier2, event2, and queueSubmit2 entry points to core Vulkan 1.3.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceSynchronization2Features m_features{};
};

/// @brief Descriptor for `VK_KHR_multiview` (promoted to Vulkan 1.1).
class GP_RHI_VULKAN_API VulkanKHRMultiviewExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_multiview` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRMultiviewExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceMultiviewFeatures m_features{};
};

/// @brief Descriptor for `VK_EXT_descriptor_indexing` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanEXTDescriptorIndexingExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_descriptor_indexing` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTDescriptorIndexingExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceDescriptorIndexingFeatures m_features{};
};

/// @brief Descriptor for `VK_KHR_fragment_shading_rate`.
/// @note Must be registered before `VulkanEXTFragmentDensityMapExtension` so VRS resolution sees FSR features first.
class GP_RHI_VULKAN_API VulkanKHRFragmentShadingRateExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_fragment_shading_rate` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRFragmentShadingRateExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceProperties()
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    /// @brief Resolves either the device's retained FSR features struct or the local fallback struct.
    /// @return Mutable reference to `VkPhysicalDeviceFragmentShadingRateFeaturesKHR`.
    [[nodiscard]] VkPhysicalDeviceFragmentShadingRateFeaturesKHR& resolveFeatures() noexcept;

    VkPhysicalDeviceFragmentShadingRateFeaturesKHR m_fallbackFeatures{};
    VkPhysicalDeviceFragmentShadingRatePropertiesKHR m_properties{};
};

/// @brief Descriptor for `VK_EXT_fragment_density_map`.
/// @note Must be registered after `VulkanKHRFragmentShadingRateExtension`.
class GP_RHI_VULKAN_API VulkanEXTFragmentDensityMapExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_fragment_density_map` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTFragmentDensityMapExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceProperties()
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    /// @brief Resolves either the device's retained FDM features struct or the local fallback struct.
    /// @return Mutable reference to `VkPhysicalDeviceFragmentDensityMapFeaturesEXT`.
    [[nodiscard]] VkPhysicalDeviceFragmentDensityMapFeaturesEXT& resolveFeatures() noexcept;

    VkPhysicalDeviceFragmentDensityMapFeaturesEXT m_fallbackFeatures{};
    VkPhysicalDeviceFragmentDensityMapPropertiesEXT m_properties{};
};

/// @brief Descriptor for `VK_EXT_fragment_density_map2`.
class GP_RHI_VULKAN_API VulkanEXTFragmentDensityMap2Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_fragment_density_map2` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTFragmentDensityMap2Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    /// @brief Resolves either the device's retained FDM2 features struct or the local fallback struct.
    /// @return Mutable reference to `VkPhysicalDeviceFragmentDensityMap2FeaturesEXT`.
    [[nodiscard]] VkPhysicalDeviceFragmentDensityMap2FeaturesEXT& resolveFeatures() noexcept;

    VkPhysicalDeviceFragmentDensityMap2FeaturesEXT m_fallbackFeatures{};
};

/// @brief Descriptor for `VK_KHR_fragment_shader_barycentric`.
class GP_RHI_VULKAN_API VulkanKHRFragmentShaderBarycentricExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_fragment_shader_barycentric` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRFragmentShaderBarycentricExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceProperties()
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR m_features{};
    VkPhysicalDeviceFragmentShaderBarycentricPropertiesKHR m_properties{};
};

/// @brief Descriptor for `VK_EXT_host_query_reset` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanEXTHostQueryResetExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_host_query_reset` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTHostQueryResetExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects `vkResetQueryPoolEXT` to core `vkResetQueryPool`.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceHostQueryResetFeatures m_features{};
};

/// @brief Descriptor for `VK_EXT_subgroup_size_control` (promoted to Vulkan 1.3).
class GP_RHI_VULKAN_API VulkanEXTSubgroupSizeControlExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_subgroup_size_control` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTSubgroupSizeControlExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceProperties()
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    /// @brief Resolves either the device's retained subgroup properties struct or the local fallback struct.
    /// @return Mutable reference to `VkPhysicalDeviceSubgroupSizeControlPropertiesEXT`.
    [[nodiscard]] VkPhysicalDeviceSubgroupSizeControlPropertiesEXT& resolveProperties() noexcept;

    VkPhysicalDeviceSubgroupSizeControlFeatures m_features{};
    VkPhysicalDeviceSubgroupSizeControlPropertiesEXT m_fallbackProperties{};
};

/// @brief Descriptor for `VK_KHR_calibrated_timestamps`.
/// @note Must be registered before `VulkanEXTCalibratedTimestampsExtension`.
class GP_RHI_VULKAN_API VulkanKHRCalibratedTimestampsExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_calibrated_timestamps` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRCalibratedTimestampsExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;
};

/// @brief Descriptor for `VK_EXT_calibrated_timestamps`.
/// @note Disables itself automatically when `VK_KHR_calibrated_timestamps` has already been enabled.
class GP_RHI_VULKAN_API VulkanEXTCalibratedTimestampsExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_calibrated_timestamps` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTCalibratedTimestampsExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;
};

/// @brief Descriptor for `VK_KHR_16bit_storage` (promoted to Vulkan 1.1).
/// @note Must be registered before `VulkanKHRShaderFloat16Int8Extension`.
class GP_RHI_VULKAN_API VulkanKHR16BitStorageExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_16bit_storage` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHR16BitStorageExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDevice16BitStorageFeatures m_features{};
};

/// @brief Descriptor for `VK_KHR_shader_float16_int8` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanKHRShaderFloat16Int8Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_shader_float16_int8` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRShaderFloat16Int8Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceShaderFloat16Int8Features m_features{};
};

/// @brief Descriptor for `VK_KHR_depth_stencil_resolve` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanKHRDepthStencilResolveExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_depth_stencil_resolve` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRDepthStencilResolveExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;
};

/// @brief Descriptor for `VK_EXT_image_robustness` (promoted to Vulkan 1.3).
class GP_RHI_VULKAN_API VulkanEXTImageRobustnessExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_image_robustness` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTImageRobustnessExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceImageRobustnessFeatures m_features{};
};

/// @brief Descriptor for `VK_EXT_pipeline_creation_cache_control` (promoted to Vulkan 1.3).
class GP_RHI_VULKAN_API VulkanEXTPipelineCreationCacheControlExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_pipeline_creation_cache_control` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTPipelineCreationCacheControlExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDevicePipelineCreationCacheControlFeatures m_features{};
};

/// @brief Descriptor for `VK_KHR_sampler_ycbcr_conversion` (promoted to Vulkan 1.1).
class GP_RHI_VULKAN_API VulkanKHRSamplerYcbcrConversionExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_sampler_ycbcr_conversion` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRSamplerYcbcrConversionExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceSamplerYcbcrConversionFeatures m_features{};
};

/// @brief Descriptor for `VK_KHR_timeline_semaphore` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanKHRTimelineSemaphoreExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_timeline_semaphore` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRTimelineSemaphoreExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects `vkGetSemaphoreCounterValueKHR`, `vkWaitSemaphoresKHR`, and `vkSignalSemaphoreKHR` to core.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceTimelineSemaphoreFeatures m_features{};
};

/// @brief Descriptor for `VK_EXT_mesh_shader`.
class GP_RHI_VULKAN_API VulkanEXTMeshShaderExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_mesh_shader` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTMeshShaderExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceProperties()
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    /// @brief Resolves either the device's retained mesh shader properties struct or the local fallback struct.
    /// @return Mutable reference to `VkPhysicalDeviceMeshShaderPropertiesEXT`.
    [[nodiscard]] VkPhysicalDeviceMeshShaderPropertiesEXT& resolveProperties() noexcept;

    VkPhysicalDeviceMeshShaderFeaturesEXT m_features{};
    VkPhysicalDeviceMeshShaderPropertiesEXT m_fallbackProperties{};
};

/// @brief Descriptor for `VK_KHR_dynamic_rendering` (promoted to Vulkan 1.3).
class GP_RHI_VULKAN_API VulkanKHRDynamicRenderingExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_dynamic_rendering` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRDynamicRenderingExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects `vkCmdBeginRenderingKHR` and `vkCmdEndRenderingKHR` to core Vulkan 1.3.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceDynamicRenderingFeatures m_features{};
};

/// @brief Descriptor for `VK_EXT_extended_dynamic_state` (promoted to Vulkan 1.3).
class GP_RHI_VULKAN_API VulkanEXTExtendedDynamicStateExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_extended_dynamic_state` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTExtendedDynamicStateExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects all 12 extended dynamic state 1 command entry points to core Vulkan 1.3.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT m_features{};
};

/// @brief Descriptor for `VK_EXT_extended_dynamic_state2` (promoted to Vulkan 1.3).
class GP_RHI_VULKAN_API VulkanEXTExtendedDynamicState2Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_extended_dynamic_state2` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTExtendedDynamicState2Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects `vkCmdSetRasterizerDiscardEnableEXT`, `vkCmdSetDepthBiasEnableEXT`, and
    ///       `vkCmdSetPrimitiveRestartEnableEXT` to core Vulkan 1.3.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceExtendedDynamicState2FeaturesEXT m_features{};
};

/// @brief Descriptor for `VK_EXT_extended_dynamic_state3`.
class GP_RHI_VULKAN_API VulkanEXTExtendedDynamicState3Extension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_extended_dynamic_state3` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTExtendedDynamicState3Extension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT m_features{};
};

/// @brief Descriptor for `VK_EXT_vertex_input_dynamic_state`.
class GP_RHI_VULKAN_API VulkanEXTVertexInputDynamicStateExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_vertex_input_dynamic_state` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTVertexInputDynamicStateExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT m_features{};
};

/// @brief Descriptor for `VK_EXT_texture_compression_astc_hdr` (promoted to Vulkan 1.3).
class GP_RHI_VULKAN_API VulkanEXTTextureCompressionASTCHDRExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_EXT_texture_compression_astc_hdr` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanEXTTextureCompressionASTCHDRExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceTextureCompressionASTCHDRFeatures m_features{};
};

/// @brief Descriptor for `VK_KHR_buffer_device_address` (promoted to Vulkan 1.2).
class GP_RHI_VULKAN_API VulkanKHRBufferDeviceAddressExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_buffer_device_address` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRBufferDeviceAddressExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    /// @note Redirects `vkGetBufferDeviceAddressKHR` to core Vulkan 1.2.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

private:
    VkPhysicalDeviceBufferDeviceAddressFeatures m_features{};
};

/// @brief Descriptor for `VK_KHR_acceleration_structure`.
class GP_RHI_VULKAN_API VulkanKHRAccelerationStructureExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_acceleration_structure` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRAccelerationStructureExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceProperties()
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    /// @brief Resolves either the device's retained acceleration structure properties or the local fallback struct.
    /// @return Mutable reference to `VkPhysicalDeviceAccelerationStructurePropertiesKHR`.
    [[nodiscard]] VkPhysicalDeviceAccelerationStructurePropertiesKHR& resolveProperties() noexcept;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR m_features{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR m_fallbackProperties{};
};

/// @brief Descriptor for `VK_KHR_ray_tracing_pipeline`.
class GP_RHI_VULKAN_API VulkanKHRRayTracingPipelineExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_ray_tracing_pipeline` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRRayTracingPipelineExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceProperties()
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    /// @brief Resolves either the device's retained ray tracing pipeline properties or the local fallback struct.
    /// @return Mutable reference to `VkPhysicalDeviceRayTracingPipelinePropertiesKHR`.
    [[nodiscard]] VkPhysicalDeviceRayTracingPipelinePropertiesKHR& resolveProperties() noexcept;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_features{};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_fallbackProperties{};
};

/// @brief Descriptor for `VK_KHR_ray_query`.
class GP_RHI_VULKAN_API VulkanKHRRayQueryExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_KHR_ray_query` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanKHRRayQueryExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    VkPhysicalDeviceRayQueryFeaturesKHR m_features{};
};

/// @brief Descriptor for `VK_NV_device_diagnostics_config`.
class GP_RHI_VULKAN_API VulkanNVDeviceDiagnosticConfigExtension final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs the `VK_NV_device_diagnostics_config` extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable during standalone discovery).
    explicit VulkanNVDeviceDiagnosticConfigExtension(VulkanDevice* inDevice) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    VkPhysicalDeviceDiagnosticsConfigFeaturesNV m_features{};
    VkDeviceDiagnosticsConfigCreateInfoNV m_configInfo{};
};

}   // namespace gp::rhi::vulkan
