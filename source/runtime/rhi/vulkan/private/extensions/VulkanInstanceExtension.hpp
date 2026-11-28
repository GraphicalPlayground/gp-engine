// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanExtensionBase.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "RHIVulkan.hpp"
#include "templates/Function.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <utility>

namespace gp::rhi::vulkan
{

class VulkanInstanceExtension;

/// @brief Owning array of Vulkan instance extension descriptors.
using VulkanInstanceExtensionArray = gp::Vector<gp::UniquePtr<VulkanInstanceExtension>>;

/// @brief Descriptor for a Vulkan instance extension with optional `VkInstanceCreateInfo` `pNext` configuration hook.
class GP_RHI_VULKAN_API VulkanInstanceExtension : public VulkanExtensionBase
{
public:
    /// @brief Callback setting the corresponding flag inside `OptionalVulkanInstanceExtensions`.
    using FlagSetterFunc = gp::UniqueFunction<void(OptionalVulkanInstanceExtensions&)>;

    /// @brief Constructs an instance extension descriptor.
    /// @param[in] inExtensionName Null-terminated Vulkan extension name.
    /// @param[in] inEnabledInCode Compile-time enablement flag (`kVulkanExtensionEnabled` or
    ///                            `kVulkanExtensionDisabled`).
    /// @param[in] inPromotedVersion Vulkan core promotion version, or `kVulkanExtensionNotPromoted`.
    /// @param[in] inFlagSetter Optional callback updating `OptionalVulkanInstanceExtensions`.
    /// @param[in] inActivation Automatic or manual activation mode.
    VulkanInstanceExtension(
        const char* const inExtensionName,
        const gp::Int32 inEnabledInCode,
        const gp::UInt32 inPromotedVersion = kVulkanExtensionNotPromoted,
        FlagSetterFunc&& inFlagSetter = nullptr,
        const VulkanExtensionActivation inActivation = VulkanExtensionActivation::AutoActivate
    ) noexcept
        : VulkanExtensionBase(inExtensionName, inEnabledInCode, inPromotedVersion, inActivation)
        , m_flagSetter(std::move(inFlagSetter))
    {}

    /// @brief Virtual destructor.
    virtual ~VulkanInstanceExtension() noexcept override = default;

    /// @brief Hook invoked before `vkCreateInstance` to update `extensionFlags` or attach `pNext` structs to
    ///        `createInfo`.
    /// @param[in,out] createInfo Instance creation descriptor being assembled.
    /// @param[in,out] extensionFlags Bitfield of enabled optional instance extensions.
    virtual void
        preCreateInstance(VkInstanceCreateInfo& createInfo, OptionalVulkanInstanceExtensions& extensionFlags) noexcept
    {
        static_cast<void>(createInfo);
        if (m_flagSetter)
        {
            m_flagSetter(extensionFlags);
        }
    }

    /// @brief Builds the engine's supported instance extension list and flags driver support by querying Vulkan.
    /// @param[in] apiVersion Target Vulkan instance API version.
    /// @return Array of configured `VulkanInstanceExtension` descriptors with support flags populated.
    [[nodiscard]] static VulkanInstanceExtensionArray getSupportedInstanceExtensions(gp::UInt32 apiVersion) noexcept;

    /// @brief Builds the engine's supported instance extension list and flags support against `driverExtensions`.
    /// @details Allows deterministic discovery and unit testing without calling into a live Vulkan loader.
    /// @param[in] apiVersion Target Vulkan instance API version.
    /// @param[in] driverExtensions Pre-enumerated driver instance extensions.
    /// @return Array of configured `VulkanInstanceExtension` descriptors with support flags populated.
    [[nodiscard]] static VulkanInstanceExtensionArray getSupportedInstanceExtensions(
        gp::UInt32 apiVersion, gp::ConstVectorView<VkExtensionProperties> driverExtensions
    ) noexcept;

    /// @brief Enumerates instance extensions supported by the Vulkan loader or by a specific `layerName`.
    /// @param[in] layerName Optional layer name to query, or `nullptr` for base loader extensions.
    /// @return Lexicographically sorted list of `VkExtensionProperties`.
    [[nodiscard]] static gp::Vector<VkExtensionProperties>
        getDriverSupportedInstanceExtensions(const char* layerName = nullptr) noexcept;

    /// @brief Appends externally requested instance extension names (e.g., from plugins) before RHI creation.
    /// @param[in] extensions View of null-terminated extension names.
    static void addExternalInstanceExtensions(gp::ConstVectorView<const char*> extensions) noexcept;

    /// @brief Clears all previously registered external instance extension names.
    static void clearExternalInstanceExtensions() noexcept;

    /// @brief Returns the currently registered external instance extension names.
    /// @return Read-only view of external instance extension names.
    [[nodiscard]] static gp::ConstVectorView<const char*> getExternalInstanceExtensions() noexcept;

protected:
    /// @brief Optional callback marking this extension in `OptionalVulkanInstanceExtensions`.
    FlagSetterFunc m_flagSetter{};
};

/// @brief Instance extension configuring `VK_EXT_validation_features` on `VkInstanceCreateInfo::pNext`.
/// @details Inspects `r.Vulkan.GPUValidation`, `-vulkanbestpractices`, and `-vulkandebugsync` to populate
///          `VkValidationFeaturesEXT` when validation layers are active.
class GP_RHI_VULKAN_API VulkanEXTValidationFeaturesExtension final : public VulkanInstanceExtension
{
public:
    /// @brief Constructs the manually activated `VK_EXT_validation_features` extension descriptor.
    VulkanEXTValidationFeaturesExtension() noexcept;

    /// @copydoc VulkanInstanceExtension::preCreateInstance()
    /// @note Chains `VkValidationFeaturesEXT` into `createInfo.pNext` when at least one validation feature is active.
    virtual void preCreateInstance(
        VkInstanceCreateInfo& createInfo, OptionalVulkanInstanceExtensions& extensionFlags
    ) noexcept override;

    /// @brief Collects enabled `VkValidationFeatureEnableEXT` flags from GPU validation level and command-line flags.
    /// @param[in] gpuValidationLevel Value of `r.Vulkan.GPUValidation` (`0` = off, `1` = GPU-assisted, `2` = +reserve
    ///                               binding slot).
    /// @param[in] enableBestPractices True when `-vulkanbestpractices` is active.
    /// @param[in] enableSyncValidation True when `-vulkandebugsync` is active.
    /// @return Vector of `VkValidationFeatureEnableEXT` values to attach to `VkValidationFeaturesEXT`.
    [[nodiscard]] static gp::Vector<VkValidationFeatureEnableEXT> collectValidationFeatures(
        gp::Int32 gpuValidationLevel, bool enableBestPractices, bool enableSyncValidation
    ) noexcept;

    /// @brief Returns the list of validation features attached during the last `preCreateInstance()` call.
    /// @return Read-only view of enabled `VkValidationFeatureEnableEXT` values.
    [[nodiscard]] gp::ConstVectorView<VkValidationFeatureEnableEXT> getEnabledFeatures() const noexcept
    {
        return gp::ConstVectorView<VkValidationFeatureEnableEXT>(
            m_validationFeaturesEnabled.data(), m_validationFeaturesEnabled.size()
        );
    }

private:
    VkValidationFeaturesEXT m_validationFeatures{};
    gp::Vector<VkValidationFeatureEnableEXT> m_validationFeaturesEnabled{};
};

}   // namespace gp::rhi::vulkan
