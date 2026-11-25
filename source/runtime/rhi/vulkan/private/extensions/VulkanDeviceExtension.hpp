// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanExtensionBase.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "RHIVulkan.hpp"
#include "templates/Function.hpp"
#include "VulkanForward.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <utility>

namespace gp::rhi::vulkan
{

class VulkanDeviceExtension;

/// @brief Owning array of Vulkan device extension descriptors.
using VulkanDeviceExtensionArray = gp::Vector<gp::UniquePtr<VulkanDeviceExtension>>;

/// @brief Descriptor for a Vulkan device extension participating in feature/property discovery and device creation.
class GP_RHI_VULKAN_API VulkanDeviceExtension : public VulkanExtensionBase
{
public:
    /// @brief Callback setting the corresponding flag inside `OptionalVulkanDeviceExtensions`.
    using FlagSetterFunc = gp::UniqueFunction<void(OptionalVulkanDeviceExtensions&)>;

    /// @brief Constructs a device extension descriptor.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (may be `nullptr` during offline or unit-test discovery).
    /// @param[in] inExtensionName Null-terminated Vulkan extension name.
    /// @param[in] inEnabledInCode Compile-time enablement flag (`kVulkanExtensionEnabled` or
    ///                            `kVulkanExtensionDisabled`).
    /// @param[in] inPromotedVersion Vulkan core promotion version, or `kVulkanExtensionNotPromoted`.
    /// @param[in] inFlagSetter Optional callback updating `OptionalVulkanDeviceExtensions`.
    /// @param[in] inActivation Automatic or manual activation mode.
    VulkanDeviceExtension(
        VulkanDevice* const inDevice,
        const char* const inExtensionName,
        const gp::Int32 inEnabledInCode,
        const gp::UInt32 inPromotedVersion = kVulkanExtensionNotPromoted,
        FlagSetterFunc&& inFlagSetter = nullptr,
        const VulkanExtensionActivation inActivation = VulkanExtensionActivation::AutoActivate
    ) noexcept
        : VulkanExtensionBase(inExtensionName, inEnabledInCode, inPromotedVersion, inActivation)
        , m_device(inDevice)
        , m_flagSetter(std::move(inFlagSetter))
        , m_requirementsPassed(true)
    {}

    /// @brief Virtual destructor.
    virtual ~VulkanDeviceExtension() noexcept override = default;

    /// @brief Hook invoked before `vkGetPhysicalDeviceProperties2` to attach extension property structs.
    /// @param[in,out] physicalDeviceProperties2 Head property structure.
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept
    {
        static_cast<void>(physicalDeviceProperties2);
    }

    /// @brief Hook invoked after `vkGetPhysicalDeviceProperties2` to inspect returned properties.
    /// @param[in,out] extensionFlags Bitfield of enabled optional device extensions.
    virtual void postPhysicalDeviceProperties(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
    {
        static_cast<void>(extensionFlags);
    }

    /// @brief Hook invoked before `vkGetPhysicalDeviceFeatures2` to attach extension feature structs.
    /// @param[in,out] physicalDeviceFeatures2 Head feature structure.
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept
    {
        static_cast<void>(physicalDeviceFeatures2);
    }

    /// @brief Hook invoked after `vkGetPhysicalDeviceFeatures2` to validate features and set `extensionFlags`.
    /// @param[in,out] extensionFlags Bitfield of enabled optional device extensions.
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
    {
        if (m_flagSetter)
        {
            m_flagSetter(extensionFlags);
        }
    }

    /// @brief Hook invoked instead of extension feature/property queries when `usedAsCore()` is true.
    /// @param[in] coreFeaturesAndProperties Core Vulkan 1.1..1.4 features and properties.
    /// @param[in,out] extensionFlags Bitfield of enabled optional device extensions.
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept
    {
        static_cast<void>(coreFeaturesAndProperties);
        if (m_flagSetter)
        {
            m_flagSetter(extensionFlags);
        }
    }

    /// @brief Hook invoked before `vkCreateDevice` to chain enabled feature structs onto `deviceCreateInfo.pNext`.
    /// @param[in,out] deviceCreateInfo Logical device creation descriptor.
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
    {
        static_cast<void>(deviceCreateInfo);
    }

    /// @brief Reports whether this extension satisfied all feature and property checks.
    /// @return True when requirements passed.
    [[nodiscard]] bool didRequirementsPass() const noexcept
    {
        return m_requirementsPassed;
    }

    /// @brief Builds the engine's supported device extension list and queries `physicalDevice` for support.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable for standalone discovery).
    /// @param[in] physicalDevice Target `VkPhysicalDevice`.
    /// @param[in] apiVersion Active Vulkan API version.
    /// @return Array of configured `VulkanDeviceExtension` descriptors with support flags populated.
    [[nodiscard]] static VulkanDeviceExtensionArray getSupportedDeviceExtensions(
        VulkanDevice* inDevice, VkPhysicalDevice physicalDevice, gp::UInt32 apiVersion
    ) noexcept;

    /// @brief Builds the engine's supported device extension list and flags support against `driverExtensions`.
    /// @details Allows deterministic discovery and unit testing without calling into a live physical device.
    /// @param[in] inDevice Owning `VulkanDevice` pointer (nullable).
    /// @param[in] apiVersion Active Vulkan API version.
    /// @param[in] driverExtensions Pre-enumerated driver device extensions.
    /// @return Array of configured `VulkanDeviceExtension` descriptors with support flags populated.
    [[nodiscard]] static VulkanDeviceExtensionArray getSupportedDeviceExtensions(
        VulkanDevice* inDevice, gp::UInt32 apiVersion, gp::ConstVectorView<VkExtensionProperties> driverExtensions
    ) noexcept;

    /// @brief Enumerates device extensions supported by `gpu` or by a specific `layerName`.
    /// @param[in] gpu Target `VkPhysicalDevice`.
    /// @param[in] layerName Optional layer name to query, or `nullptr` for base driver extensions.
    /// @return Lexicographically sorted list of `VkExtensionProperties`.
    [[nodiscard]] static gp::Vector<VkExtensionProperties>
        getDriverSupportedDeviceExtensions(VkPhysicalDevice gpu, const char* layerName = nullptr) noexcept;

    /// @brief Appends externally requested device extensions (e.g., from plugins) before device creation.
    /// @param[in] extensions View of shared external extension descriptors.
    static void addExternalDeviceExtensions(
        gp::ConstVectorView<gp::SharedPtr<VulkanExternalDeviceExtensionBase>> extensions
    ) noexcept;

    /// @brief Clears all previously registered external device extensions.
    static void clearExternalDeviceExtensions() noexcept;

    /// @brief Returns the currently registered external device extensions.
    /// @return Read-only view of external device extension descriptors.
    [[nodiscard]] static gp::ConstVectorView<gp::SharedPtr<VulkanExternalDeviceExtensionBase>>
        getExternalDeviceExtensions() noexcept;

protected:
    /// @brief Owning `VulkanDevice` pointer (may be `nullptr`).
    VulkanDevice* m_device{ nullptr };

    /// @brief Optional callback marking this extension in `OptionalVulkanDeviceExtensions`.
    FlagSetterFunc m_flagSetter{};

    /// @brief True when all feature and property prerequisites for this extension succeeded.
    bool m_requirementsPassed{ true };
};

/// @brief Adapts a `VulkanExternalDeviceExtensionBase` into the internal `VulkanDeviceExtension` lifecycle.
class GP_RHI_VULKAN_API VulkanExternalDeviceExtensionWrapper final : public VulkanDeviceExtension
{
public:
    /// @brief Constructs a wrapper around `inExternalExtension`.
    /// @param[in] inDevice Owning `VulkanDevice` pointer.
    /// @param[in] inExternalExtension Shared external extension descriptor (must be non-null).
    VulkanExternalDeviceExtensionWrapper(
        VulkanDevice* inDevice, gp::SharedPtr<VulkanExternalDeviceExtensionBase> inExternalExtension
    ) noexcept;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceProperties()
    virtual void prePhysicalDeviceProperties(VkPhysicalDeviceProperties2& physicalDeviceProperties2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceProperties()
    virtual void postPhysicalDeviceProperties(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::prePhysicalDeviceFeatures()
    virtual void prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept override;

    /// @copydoc VulkanDeviceExtension::postPhysicalDeviceFeatures()
    virtual void postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept override;

    /// @copydoc VulkanDeviceExtension::handleCoreFeaturesAndProperties()
    virtual void handleCoreFeaturesAndProperties(
        const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
    ) noexcept override;

    /// @copydoc VulkanDeviceExtension::preCreateDevice()
    virtual void preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept override;

private:
    gp::SharedPtr<VulkanExternalDeviceExtensionBase> m_externalExtension{};
};

}   // namespace gp::rhi::vulkan
