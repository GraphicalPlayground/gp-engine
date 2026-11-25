// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "debug/VulkanDebug.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "layers/VulkanLayers.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Manages the lifecycle of a `VkInstance`, enabled layers/extensions, instance entry points, and debug
///        messenger callbacks.
class GP_RHI_VULKAN_API VulkanInstance final
{
public:
    VulkanInstance() noexcept = default;

    /// @brief Destructor. Automatically shuts down the `VkInstance` if still active.
    ~VulkanInstance() noexcept;

    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;
    VulkanInstance(VulkanInstance&&) = delete;
    VulkanInstance& operator=(VulkanInstance&&) = delete;

    /// @brief Creates the `VkInstance`, loads instance-level entry points, and configures optional debug callbacks.
    /// @details Evaluates validation requests, discovers supported instance extensions and layers, chains
    ///          `VkValidationFeaturesEXT` and `VkLayerSettingsCreateInfoEXT` when active, invokes `vkCreateInstance`,
    ///          handles `VK_ERROR_LAYER_NOT_PRESENT` retry and `VK_ERROR_EXTENSION_NOT_PRESENT` diagnostics, loads
    ///          instance function pointers via `VulkanLoader`, and registers `VulkanDebugMessenger`.
    /// @param[in] desc Instance creation parameters.
    /// @return True when the `VkInstance` was created and required entry points loaded; false otherwise.
    [[nodiscard]] bool initialize(const VulkanInstanceCreateDesc& desc = {}) noexcept;

    /// @brief Destroys the debug messenger and `VkInstance`, clears instance function pointers, and resets state.
    void shutdown() noexcept;

    /// @brief Returns the underlying `VkInstance` handle.
    /// @return Active `VkInstance`, or `VK_NULL_HANDLE` if uninitialized.
    [[nodiscard]] VkInstance getHandle() const noexcept
    {
        return m_instance;
    }

    /// @brief Reports whether a live `VkInstance` is currently owned by this object.
    /// @return True when `m_instance != VK_NULL_HANDLE`.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_instance != VK_NULL_HANDLE;
    }

    /// @brief Returns the Vulkan API version negotiated for this instance.
    /// @return Packed Vulkan API version (`VK_API_VERSION_*`).
    [[nodiscard]] gp::UInt32 getApiVersion() const noexcept
    {
        return m_apiVersion;
    }

    /// @brief Returns the optional instance extension flags enabled during `initialize()`.
    /// @return Const reference to `OptionalVulkanInstanceExtensions`.
    [[nodiscard]] const OptionalVulkanInstanceExtensions& getOptionalExtensions() const noexcept
    {
        return m_optionalExtensions;
    }

    /// @brief Returns the list of instance extension names enabled on `m_instance`.
    /// @return Read-only view of null-terminated extension name strings.
    [[nodiscard]] gp::ConstVectorView<const char*> getEnabledExtensions() const noexcept
    {
        return gp::ConstVectorView<const char*>(m_enabledExtensions.data(), m_enabledExtensions.size());
    }

    /// @brief Returns the list of instance layer names enabled on `m_instance`.
    /// @return Read-only view of null-terminated layer name strings.
    [[nodiscard]] gp::ConstVectorView<const char*> getEnabledLayers() const noexcept
    {
        return gp::ConstVectorView<const char*>(m_enabledLayers.data(), m_enabledLayers.size());
    }

    /// @brief Returns the active debug or trace extension selected during instance setup.
    /// @return Active `ActiveDebugLayerExtension`.
    [[nodiscard]] ActiveDebugLayerExtension getActiveDebugLayerExtension() const noexcept
    {
        return m_activeDebugLayerExtension;
    }

    /// @brief Returns the resolved validation configuration applied during `initialize()`.
    /// @return Const reference to `VulkanValidationConfig`.
    [[nodiscard]] const VulkanValidationConfig& getValidationConfig() const noexcept
    {
        return m_validationConfig;
    }

    /// @brief Returns the `VulkanDebugMessenger` owned by this instance.
    /// @return Const reference to `VulkanDebugMessenger`.
    [[nodiscard]] const VulkanDebugMessenger& getDebugMessenger() const noexcept
    {
        return m_debugMessenger;
    }

    /// @brief Returns the `VkResult` returned by the most recent `vkCreateInstance` call.
    /// @return Last `VkResult`.
    [[nodiscard]] VkResult getLastCreateResult() const noexcept
    {
        return m_lastCreateResult;
    }

    /// @brief Builds a `VkApplicationInfo` descriptor from `desc` and `disableRegistration`.
    /// @param[in] desc Instance creation descriptor.
    /// @param[in] disableRegistration True to null out `pApplicationName` and `pEngineName`.
    /// @return Populated `VkApplicationInfo`.
    [[nodiscard]] static VkApplicationInfo
        buildApplicationInfo(const VulkanInstanceCreateDesc& desc, bool disableRegistration) noexcept;

    /// @brief Compares `requestedExtensions` against `availableExtensions` and returns any missing extension names.
    /// @param[in] requestedExtensions Extension names passed to `vkCreateInstance`.
    /// @param[in] availableExtensions Extensions reported by `vkEnumerateInstanceExtensionProperties`.
    /// @return Vector of requested extension names absent from `availableExtensions`.
    [[nodiscard]] static gp::Vector<const char*> findMissingInstanceExtensions(
        gp::ConstVectorView<const char*> requestedExtensions,
        gp::ConstVectorView<VkExtensionProperties> availableExtensions
    ) noexcept;

private:
    VkInstance m_instance{ VK_NULL_HANDLE };
    gp::UInt32 m_apiVersion{ 0u };
    OptionalVulkanInstanceExtensions m_optionalExtensions{};
    gp::Vector<const char*> m_enabledExtensions{};
    gp::Vector<const char*> m_enabledLayers{};
    ActiveDebugLayerExtension m_activeDebugLayerExtension{ ActiveDebugLayerExtension::None };
    VulkanValidationConfig m_validationConfig{};
    VulkanDebugMessenger m_debugMessenger{};
    VkResult m_lastCreateResult{ VK_SUCCESS };
};

}   // namespace gp::rhi::vulkan
