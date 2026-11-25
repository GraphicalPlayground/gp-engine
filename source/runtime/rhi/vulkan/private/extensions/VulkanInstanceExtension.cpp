// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "extensions/VulkanInstanceExtension.hpp"
#include "console/ConsoleManager.hpp"        // IWYU pragma: keep
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "platform/VulkanPlatform.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include <cstring>

namespace gp::rhi::vulkan
{

namespace
{

gp::Vector<const char*>& getExternalInstanceExtensionStorage() noexcept
{
    static gp::Vector<const char*> externalExtensions;
    return externalExtensions;
}

void sortExtensionProperties(gp::Vector<VkExtensionProperties>& properties) noexcept
{
    for (gp::USize i = 1u; i < properties.size(); ++i)
    {
        const VkExtensionProperties key = properties[i];
        gp::USize j = i;
        while (j > 0u && std::strcmp(key.extensionName, properties[j - 1u].extensionName) < 0)
        {
            properties[j] = properties[j - 1u];
            --j;
        }
        properties[j] = key;
    }
}

[[nodiscard]] gp::Int32 readConsoleInt(const gp::StringView name, const gp::Int32 fallback) noexcept
{
    if (const gp::console::IConsoleVariable* const cvar = gp::console::ConsoleManager::get().findConsoleVariable(name))
    {
        return cvar->getInt();
    }
    return fallback;
}

}   // namespace

VulkanEXTValidationFeaturesExtension::VulkanEXTValidationFeaturesExtension() noexcept
    : VulkanInstanceExtension(
          VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME,
          (GP_VULKAN_HAS_DEBUGGING_ENABLED && GP_VULKAN_HAS_VALIDATION_FEATURES) ? kVulkanExtensionEnabled
                                                                                 : kVulkanExtensionDisabled,
          kVulkanExtensionNotPromoted,
          nullptr,
          VulkanExtensionActivation::ManuallyActivate
      )
{}

gp::Vector<VkValidationFeatureEnableEXT> VulkanEXTValidationFeaturesExtension::collectValidationFeatures(
    const gp::Int32 gpuValidationLevel, const bool enableBestPractices, const bool enableSyncValidation
) noexcept
{
    gp::Vector<VkValidationFeatureEnableEXT> features;

    if (gpuValidationLevel > 0)
    {
        features.pushBack(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
        if (gpuValidationLevel > 1)
        {
            features.pushBack(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT);
        }
    }

    if (enableBestPractices)
    {
        features.pushBack(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
    }

    if (enableSyncValidation)
    {
        features.pushBack(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
    }

    return features;
}

void VulkanEXTValidationFeaturesExtension::preCreateInstance(
    VkInstanceCreateInfo& createInfo, OptionalVulkanInstanceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(extensionFlags);

#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    const gp::Int32 gpuValidationLevel = readConsoleInt("r.Vulkan.GPUValidation", 0);
    const bool enableBestPractices = gp::CommandLine::hasArgument("vulkanbestpractices");
    const bool enableSyncValidation = gp::CommandLine::hasArgument("vulkandebugsync");

    m_validationFeaturesEnabled =
        collectValidationFeatures(gpuValidationLevel, enableBestPractices, enableSyncValidation);

    if (!m_validationFeaturesEnabled.isEmpty())
    {
        zeroVulkanStruct(m_validationFeatures, VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT);
        m_validationFeatures.enabledValidationFeatureCount =
            static_cast<gp::UInt32>(m_validationFeaturesEnabled.size());
        m_validationFeatures.pEnabledValidationFeatures = m_validationFeaturesEnabled.data();
        addToPNext(createInfo, m_validationFeatures);
    }
#else
    static_cast<void>(createInfo);
#endif
}

gp::Vector<VkExtensionProperties>
    VulkanInstanceExtension::getDriverSupportedInstanceExtensions(const char* const layerName) noexcept
{
    gp::Vector<VkExtensionProperties> outExtensions;
    if (dynamic_api::vkEnumerateInstanceExtensionProperties == nullptr)
    {
        return outExtensions;
    }

    gp::UInt32 count = 0u;
    VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &count, nullptr);
    if (result == VK_ERROR_INITIALIZATION_FAILED)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "vkEnumerateInstanceExtensionProperties failed with VK_ERROR_INITIALIZATION_FAILED; verify that a Vulkan " "driver or runtime is installed."
        );
        return outExtensions;
    }
    if (result < VK_SUCCESS)
    {
        verifyVulkanResult(
            result, "vkEnumerateInstanceExtensionProperties", __FILE__, static_cast<gp::UInt32>(__LINE__)
        );
        return outExtensions;
    }

    if (count > 0u)
    {
        outExtensions.resize(count);
        result = vkEnumerateInstanceExtensionProperties(layerName, &count, outExtensions.data());
        if (result == VK_ERROR_INITIALIZATION_FAILED)
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "vkEnumerateInstanceExtensionProperties failed with VK_ERROR_INITIALIZATION_FAILED on second call."
            );
            outExtensions.clear();
            return outExtensions;
        }
        if (result < VK_SUCCESS)
        {
            verifyVulkanResult(
                result, "vkEnumerateInstanceExtensionProperties", __FILE__, static_cast<gp::UInt32>(__LINE__)
            );
            outExtensions.clear();
            return outExtensions;
        }
        outExtensions.resize(count);
        sortExtensionProperties(outExtensions);
    }

    return outExtensions;
}

VulkanInstanceExtensionArray VulkanInstanceExtension::getSupportedInstanceExtensions(
    const gp::UInt32 apiVersion, const gp::ConstVectorView<VkExtensionProperties> driverExtensions
) noexcept
{
    VulkanInstanceExtensionArray outExtensions;

    // Core surface and presentation extensions
    outExtensions.pushBack(
        gp::makeUnique<VulkanInstanceExtension>(
            VK_KHR_SURFACE_EXTENSION_NAME, kVulkanExtensionEnabled, kVulkanExtensionNotPromoted
        )
    );
    outExtensions.pushBack(
        gp::makeUnique<VulkanInstanceExtension>(
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, kVulkanExtensionEnabled, kVulkanExtensionNotPromoted
        )
    );
    outExtensions.pushBack(
        gp::makeUnique<VulkanInstanceExtension>(
            VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
            kVulkanExtensionEnabled,
            kVulkanExtensionNotPromoted,
            [](OptionalVulkanInstanceExtensions& flags) noexcept -> void
    {
        flags.hasKHRGetSurfaceCapabilities2 = 1u;
    }
        )
    );

    // Debug extensions (activated explicitly when validation or debug markers are requested)
    outExtensions.pushBack(
        gp::makeUnique<VulkanInstanceExtension>(
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            GP_VULKAN_HAS_DEBUGGING_ENABLED ? kVulkanExtensionEnabled : kVulkanExtensionDisabled,
            kVulkanExtensionNotPromoted,
            nullptr,
            VulkanExtensionActivation::ManuallyActivate
        )
    );
    outExtensions.pushBack(gp::makeUnique<VulkanEXTValidationFeaturesExtension>());

    // Platform WSI surface extensions
    gp::Vector<const char*> platformExtensions;
    VulkanPlatform::getPlatformInstanceExtensionNames(platformExtensions);
    for (const char* const extName: platformExtensions)
    {
        if (extName == nullptr)
        {
            continue;
        }
        const gp::Int32 existingIndex = findExtension(outExtensions, extName);
        if (existingIndex < 0)
        {
            outExtensions.pushBack(
                gp::makeUnique<VulkanInstanceExtension>(extName, kVulkanExtensionEnabled, kVulkanExtensionNotPromoted)
            );
        }
        else
        {
            outExtensions[static_cast<gp::USize>(existingIndex)]->setActivated();
        }
    }

    // Externally requested extensions (e.g., plugins)
    const gp::Vector<const char*>& externalExtensions = getExternalInstanceExtensionStorage();
    for (const char* const extName: externalExtensions)
    {
        if (extName == nullptr)
        {
            continue;
        }
        const gp::Int32 existingIndex = findExtension(outExtensions, extName);
        if (existingIndex < 0)
        {
            outExtensions.pushBack(
                gp::makeUnique<VulkanInstanceExtension>(extName, kVulkanExtensionEnabled, kVulkanExtensionNotPromoted)
            );
            GP_LOG_WARNING(
                LogVulkanRHI,
                "Externally requested instance extension '{}' is not in the core list; adding on the fly.",
                extName
            );
        }
        else
        {
            outExtensions[static_cast<gp::USize>(existingIndex)]->setActivated();
        }
    }

    const bool useCoreStructs = (readConsoleInt("r.Vulkan.UseCoreStructs", 0) != 0);
    flagExtensionSupport(driverExtensions, outExtensions, apiVersion, useCoreStructs, "instance");

    return outExtensions;
}

VulkanInstanceExtensionArray
    VulkanInstanceExtension::getSupportedInstanceExtensions(const gp::UInt32 apiVersion) noexcept
{
    const gp::Vector<VkExtensionProperties> driverExtensions = getDriverSupportedInstanceExtensions(nullptr);
    return getSupportedInstanceExtensions(
        apiVersion, gp::ConstVectorView<VkExtensionProperties>(driverExtensions.data(), driverExtensions.size())
    );
}

void VulkanInstanceExtension::addExternalInstanceExtensions(const gp::ConstVectorView<const char*> extensions) noexcept
{
    gp::Vector<const char*>& storage = getExternalInstanceExtensionStorage();
    for (const char* const extName: extensions)
    {
        if (extName != nullptr)
        {
            storage.pushBack(extName);
        }
    }
}

void VulkanInstanceExtension::clearExternalInstanceExtensions() noexcept
{
    getExternalInstanceExtensionStorage().clear();
}

gp::ConstVectorView<const char*> VulkanInstanceExtension::getExternalInstanceExtensions() noexcept
{
    const gp::Vector<const char*>& storage = getExternalInstanceExtensionStorage();
    return gp::ConstVectorView<const char*>(storage.data(), storage.size());
}

}   // namespace gp::rhi::vulkan
