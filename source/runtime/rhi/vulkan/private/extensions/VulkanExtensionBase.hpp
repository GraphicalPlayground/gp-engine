// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "logging/LogMacros.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <cstring>

namespace gp::rhi::vulkan
{

/// @brief Common base class tracking discovery, activation, and core-promotion state for a Vulkan extension.
/// @details Separates compile-time enablement (`m_enabledInCode`), runtime driver discovery (`m_supported`),
///          explicit activation (`m_activated`), and core Vulkan promotion (`m_core`).
class GP_RHI_VULKAN_API VulkanExtensionBase
{
public:
    /// @brief Constructs an extension descriptor with compile-time enablement and promotion metadata.
    /// @param[in] inExtensionName Null-terminated Vulkan extension name.
    /// @param[in] inEnabledInCode Non-zero (`kVulkanExtensionEnabled`) if compiled in, or `kVulkanExtensionDisabled`.
    /// @param[in] inPromotedVersion Vulkan core version promoting this extension, or `kVulkanExtensionNotPromoted`.
    /// @param[in] inActivation Whether the extension activates automatically when supported.
    VulkanExtensionBase(
        const char* const inExtensionName,
        const gp::Int32 inEnabledInCode,
        const gp::UInt32 inPromotedVersion,
        const VulkanExtensionActivation inActivation
    ) noexcept
        : m_extensionName(inExtensionName)
        , m_promotedVersion(inPromotedVersion)
        , m_enabledInCode(inEnabledInCode != kVulkanExtensionDisabled)
        , m_supported(false)
        , m_activated(inActivation == VulkanExtensionActivation::AutoActivate)
        , m_core(false)
    {}

    /// @brief Virtual destructor.
    virtual ~VulkanExtensionBase() noexcept = default;

    /// @brief Marks this extension as reported by the active Vulkan loader, driver, or layer.
    void setSupported() noexcept
    {
        m_supported = true;
    }

    /// @brief Explicitly activates a manually activated extension.
    void setActivated() noexcept
    {
        m_activated = true;
    }

    /// @brief Evaluates whether `apiVersion` meets or exceeds this extension's core promotion version.
    /// @param[in] apiVersion Active Vulkan instance API version.
    /// @return True when the extension is covered by core Vulkan at `apiVersion`.
    bool setCore(const gp::UInt32 apiVersion) noexcept
    {
        m_core = (apiVersion >= m_promotedVersion);
        return m_core;
    }

    /// @brief Returns the null-terminated Vulkan extension name.
    /// @return Extension name string.
    [[nodiscard]] const char* getExtensionName() const noexcept
    {
        return m_extensionName;
    }

    /// @brief Returns the core promotion version, or `kVulkanExtensionNotPromoted`.
    /// @return Vulkan core version constant.
    [[nodiscard]] gp::UInt32 getPromotedVersion() const noexcept
    {
        return m_promotedVersion;
    }

    /// @brief Reports whether this extension is enabled in engine code.
    /// @return True if enabled at compile time.
    [[nodiscard]] bool isEnabled() const noexcept
    {
        return m_enabledInCode;
    }

    /// @brief Reports whether this extension is supported either as an extension or as core Vulkan.
    /// @return True when driver-supported or promoted to core.
    [[nodiscard]] bool isSupported() const noexcept
    {
        return m_supported || m_core;
    }

    /// @brief Reports whether this extension has been activated (automatically or manually).
    /// @return True when activated.
    [[nodiscard]] bool isActivated() const noexcept
    {
        return m_activated;
    }

    /// @brief Reports whether this extension is handled through core Vulkan structs.
    /// @return True when promoted to core for the active API version.
    [[nodiscard]] bool isCore() const noexcept
    {
        return m_core;
    }

    /// @brief Reports whether this extension should be added to `ppEnabledExtensionNames`.
    /// @return True when enabled in code, activated, supported by the driver, and not handled as core.
    [[nodiscard]] bool usedAsExtension() const noexcept
    {
        return m_enabledInCode && m_activated && !m_core && m_supported;
    }

    /// @brief Reports whether this extension should be configured via core Vulkan feature/property structs.
    /// @return True when enabled in code, activated, and promoted to core.
    [[nodiscard]] bool usedAsCore() const noexcept
    {
        return m_enabledInCode && m_activated && m_core;
    }

    /// @brief Searches `extensions` for an entry matching `extensionName`.
    /// @tparam InExtensionType Concrete extension subclass (`VulkanInstanceExtension` or `VulkanDeviceExtension`).
    /// @param[in] extensions List of registered engine extensions.
    /// @param[in] extensionName Null-terminated extension name to locate.
    /// @return Zero-based index if found, or `-1` if absent.
    template <typename InExtensionType>
    [[nodiscard]] static gp::Int32 findExtension(
        const gp::Vector<gp::UniquePtr<InExtensionType>>& extensions, const char* const extensionName
    ) noexcept
    {
        if (extensionName == nullptr)
        {
            return -1;
        }

        for (gp::USize index = 0u; index < extensions.size(); ++index)
        {
            if (extensions[index] != nullptr && extensions[index]->getExtensionName() != nullptr &&
                std::strcmp(extensions[index]->getExtensionName(), extensionName) == 0)
            {
                return static_cast<gp::Int32>(index);
            }
        }
        return -1;
    }

    /// @brief Marks core promotion and driver support on `engineExtensions` from a driver-enumerated property list.
    /// @tparam InExtensionType Concrete extension subclass (`VulkanInstanceExtension` or `VulkanDeviceExtension`).
    /// @param[in] driverExtensions Extensions reported by the Vulkan loader or physical device.
    /// @param[in,out] engineExtensions Engine extension objects whose `supported` and `core` flags will be updated.
    /// @param[in] apiVersion Active Vulkan API version.
    /// @param[in] useCoreStructs When true and `apiVersion >= VK_API_VERSION_1_2`, marks promoted extensions as core.
    /// @param[in] extensionTypeName Human-readable category label (`"instance"` or `"device"`) used in logs.
    template <typename InExtensionType>
    static void flagExtensionSupport(
        const gp::ConstVectorView<VkExtensionProperties> driverExtensions,
        gp::Vector<gp::UniquePtr<InExtensionType>>& engineExtensions,
        const gp::UInt32 apiVersion,
        const bool useCoreStructs,
        const char* const extensionTypeName
    ) noexcept
    {
        // Core feature/property structs were introduced in Vulkan 1.2.
        if ((apiVersion >= VK_API_VERSION_1_2) && useCoreStructs)
        {
            for (gp::UniquePtr<InExtensionType>& extension: engineExtensions)
            {
                if (extension != nullptr)
                {
                    extension->setCore(apiVersion);
                }
            }
        }

        GP_LOG_INFO(
            LogVulkanRHI,
            "Found {} available {} extensions:",
            static_cast<gp::UInt32>(driverExtensions.size()),
            (extensionTypeName != nullptr) ? extensionTypeName : "unknown"
        );

        for (const VkExtensionProperties& extProp: driverExtensions)
        {
            const gp::Int32 extensionIndex = findExtension(engineExtensions, extProp.extensionName);
            const bool found = (extensionIndex >= 0);
            bool isCoreExt = false;
            if (found)
            {
                engineExtensions[static_cast<gp::USize>(extensionIndex)]->setSupported();
                isCoreExt = engineExtensions[static_cast<gp::USize>(extensionIndex)]->usedAsCore();
            }

            GP_LOG_TRACE(LogVulkanRHI, "  {} {}", isCoreExt ? "*" : (found ? "+" : "-"), extProp.extensionName);
        }
    }

protected:
    /// @brief Null-terminated extension name string.
    const char* m_extensionName{ nullptr };

    /// @brief Core Vulkan version that promoted this extension, or `kVulkanExtensionNotPromoted`.
    const gp::UInt32 m_promotedVersion{ kVulkanExtensionNotPromoted };

    /// @brief True when the extension is enabled in engine code.
    bool m_enabledInCode{ false };

    /// @brief True when the active driver or layer advertises this extension.
    bool m_supported{ false };

    /// @brief True when the runtime wants this extension enabled.
    bool m_activated{ false };

    /// @brief True when this extension is promoted to core for the active API version.
    bool m_core{ false };
};

}   // namespace gp::rhi::vulkan
