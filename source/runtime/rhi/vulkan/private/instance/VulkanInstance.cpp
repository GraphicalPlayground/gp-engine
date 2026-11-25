// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "instance/VulkanInstance.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanInstanceExtension.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "logging/LogMacros.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "platform/VulkanPlatform.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include <cstring>
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

[[nodiscard]] bool isEngineAndAppRegistrationDisabled(const bool descOverride) noexcept
{
    if (descOverride)
    {
        return true;
    }

    if (gp::console::IConsoleManager::isGlobalInitialized())
    {
        gp::console::IConsoleVariable* const cvar = gp::console::IConsoleManager::get().findConsoleVariable(
            gp::StringView("r.DisableEngineAndAppRegistration")
        );
        if (cvar != nullptr && cvar->getInt() != 0)
        {
            return true;
        }
    }

    return false;
}

}   // namespace

VulkanInstance::~VulkanInstance() noexcept
{
    shutdown();
}

VkApplicationInfo
    VulkanInstance::buildApplicationInfo(const VulkanInstanceCreateDesc& desc, const bool disableRegistration) noexcept
{
    VkApplicationInfo appInfo = zeroVulkanStruct<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
    appInfo.pApplicationName = disableRegistration ? nullptr : desc.applicationName;
    appInfo.applicationVersion = desc.applicationVersion;
    appInfo.pEngineName = disableRegistration ? nullptr : desc.engineName;
    appInfo.engineVersion = desc.engineVersion;
    appInfo.apiVersion = (desc.apiVersion != 0u) ? desc.apiVersion : kVulkanApiVersion;
    return appInfo;
}

gp::Vector<const char*> VulkanInstance::findMissingInstanceExtensions(
    const gp::ConstVectorView<const char*> requestedExtensions,
    const gp::ConstVectorView<VkExtensionProperties> availableExtensions
) noexcept
{
    gp::Vector<const char*> missing;

    for (const char* const requested: requestedExtensions)
    {
        if (requested == nullptr)
        {
            continue;
        }

        bool found = false;
        for (const VkExtensionProperties& available: availableExtensions)
        {
            if (std::strcmp(available.extensionName, requested) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            missing.pushBack(requested);
        }
    }

    return missing;
}

bool VulkanInstance::initialize(const VulkanInstanceCreateDesc& desc) noexcept
{
    if (m_instance != VK_NULL_HANDLE)
    {
        return true;
    }

    if (!VulkanLoader::isLibraryLoaded() && !VulkanLoader::loadLibrary())
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to load the Vulkan loader library before instance creation.");
        m_lastCreateResult = VK_ERROR_INITIALIZATION_FAILED;
        return false;
    }

    if (dynamic_api::vkCreateInstance == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateInstance entry point is unavailable.");
        m_lastCreateResult = VK_ERROR_INITIALIZATION_FAILED;
        return false;
    }

    m_validationConfig = setupValidationRequests();

    const gp::UInt32 targetApiVersion = (desc.apiVersion != 0u) ? desc.apiVersion : kVulkanApiVersion;
    m_apiVersion = targetApiVersion;

    GP_LOG_INFO(
        LogVulkanRHI,
        "Built with Vulkan header version {}.{}.{}; creating instance with API version {}.{}.",
        VK_API_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE),
        VK_API_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE),
        VK_API_VERSION_PATCH(VK_HEADER_VERSION_COMPLETE),
        VK_API_VERSION_MAJOR(m_apiVersion),
        VK_API_VERSION_MINOR(m_apiVersion)
    );

    const bool disableRegistration = isEngineAndAppRegistrationDisabled(desc.disableEngineAndAppRegistration);
    const VkApplicationInfo appInfo = buildApplicationInfo(desc, disableRegistration);

    VkInstanceCreateInfo instInfo = zeroVulkanStruct<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
    instInfo.pApplicationInfo = &appInfo;

    VulkanInstanceExtensionArray supportedExtensions =
        VulkanInstanceExtension::getSupportedInstanceExtensions(m_apiVersion);
    VulkanInstanceLayerSetupResult layerSetup = setupInstanceLayers(supportedExtensions);

    m_optionalExtensions = OptionalVulkanInstanceExtensions{};
    m_enabledExtensions.clear();
    m_enabledLayers = std::move(layerSetup.enabledLayers);
    m_activeDebugLayerExtension = layerSetup.activeDebugLayerExtension;

    for (gp::UniquePtr<VulkanInstanceExtension>& extension: supportedExtensions)
    {
        if (extension != nullptr && extension->usedAsExtension())
        {
            m_enabledExtensions.pushBack(extension->getExtensionName());
            extension->preCreateInstance(instInfo, m_optionalExtensions);
        }
    }

    instInfo.enabledExtensionCount = static_cast<gp::UInt32>(m_enabledExtensions.size());
    instInfo.ppEnabledExtensionNames = (instInfo.enabledExtensionCount > 0u) ? m_enabledExtensions.data() : nullptr;

    instInfo.enabledLayerCount = static_cast<gp::UInt32>(m_enabledLayers.size());
    instInfo.ppEnabledLayerNames = (instInfo.enabledLayerCount > 0u) ? m_enabledLayers.data() : nullptr;

#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo =
        zeroVulkanStruct<VkLayerSettingsCreateInfoEXT>(VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT);
    if (!layerSetup.layerSettings.isEmpty())
    {
        layerSettingsCreateInfo.settingCount = static_cast<gp::UInt32>(layerSetup.layerSettings.size());
        layerSettingsCreateInfo.pSettings = layerSetup.layerSettings.data();
        addToPNext(instInfo, layerSettingsCreateInfo);
    }
#endif

    VkResult result = vkCreateInstance(&instInfo, GP_VULKAN_CPU_ALLOCATOR, &m_instance);

    if (result == VK_ERROR_LAYER_NOT_PRESENT)
    {
        GP_LOG_WARNING(
            LogVulkanRHI,
            "Vulkan instance creation failed with VK_ERROR_LAYER_NOT_PRESENT for {} requested layer(s):",
            static_cast<gp::UInt32>(m_enabledLayers.size())
        );
        for (const char* const layerName: m_enabledLayers)
        {
            GP_LOG_WARNING(LogVulkanRHI, "  - {}", (layerName != nullptr) ? layerName : "<null>");
        }

        if (desc.allowRetryWithoutLayers)
        {
            GP_LOG_WARNING(LogVulkanRHI, "Retrying vkCreateInstance without enabled layers.");
            instInfo.enabledLayerCount = 0u;
            instInfo.ppEnabledLayerNames = nullptr;
            m_enabledLayers.clear();
            result = vkCreateInstance(&instInfo, GP_VULKAN_CPU_ALLOCATOR, &m_instance);
        }
    }

    m_lastCreateResult = result;

    VulkanPlatform::notifyFoundInstanceLayersAndExtensions(
        gp::ConstVectorView<const char*>(m_enabledLayers.data(), m_enabledLayers.size()),
        gp::ConstVectorView<const char*>(m_enabledExtensions.data(), m_enabledExtensions.size())
    );

    if (result != VK_SUCCESS)
    {
        if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "Cannot find a compatible Vulkan driver (ICD): vkCreateInstance returned VK_ERROR_INCOMPATIBLE_DRIVER."
            );
        }
        else if (result == VK_ERROR_EXTENSION_NOT_PRESENT)
        {
            const gp::Vector<VkExtensionProperties> available =
                VulkanInstanceExtension::getDriverSupportedInstanceExtensions(nullptr);
            const gp::Vector<const char*> missing = findMissingInstanceExtensions(
                gp::ConstVectorView<const char*>(m_enabledExtensions.data(), m_enabledExtensions.size()),
                gp::ConstVectorView<VkExtensionProperties>(available.data(), available.size())
            );

            for (const char* const missingExt: missing)
            {
                GP_LOG_ERROR(LogVulkanRHI, "Missing required Vulkan instance extension: {}", missingExt);
            }
        }
        else
        {
            GP_LOG_ERROR(LogVulkanRHI, "vkCreateInstance failed with {}.", vulkanResultToString(result));
        }

        m_instance = VK_NULL_HANDLE;
        m_apiVersion = 0u;
        m_optionalExtensions = OptionalVulkanInstanceExtensions{};
        m_enabledExtensions.clear();
        m_enabledLayers.clear();
        m_activeDebugLayerExtension = ActiveDebugLayerExtension::None;
        m_validationConfig = VulkanValidationConfig{};
        return false;
    }

    if (!VulkanLoader::loadInstanceFunctions(m_instance, desc.allowMissingSurfaceFunctions))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Failed to resolve required Vulkan instance entry points.");
        shutdown();
        m_lastCreateResult = VK_ERROR_INITIALIZATION_FAILED;
        return false;
    }

#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    if (desc.enableDebugCallbacks && m_validationConfig.isDebugLayerEnabled)
    {
        static_cast<void>(m_debugMessenger.create(
            m_instance, m_activeDebugLayerExtension, m_validationConfig.validationLevel, desc.debugUserData
        ));
    }
#endif

    return true;
}

void VulkanInstance::shutdown() noexcept
{
    const bool hadInstance = (m_instance != VK_NULL_HANDLE);

#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    if (hadInstance)
    {
        m_debugMessenger.destroy(m_instance);
    }
#endif

    if (hadInstance)
    {
        PFN_vkDestroyInstance destroyFn = dynamic_api::vkDestroyInstance;
        if (destroyFn == nullptr)
        {
            destroyFn = reinterpret_cast<PFN_vkDestroyInstance>(
                VulkanLoader::getInstanceProcAddr(m_instance, "vkDestroyInstance")
            );
        }
        if (destroyFn != nullptr)
        {
            destroyFn(m_instance, GP_VULKAN_CPU_ALLOCATOR);
        }
    }

    m_instance = VK_NULL_HANDLE;
    m_apiVersion = 0u;
    m_optionalExtensions = OptionalVulkanInstanceExtensions{};
    m_enabledExtensions.clear();
    m_enabledLayers.clear();
    m_activeDebugLayerExtension = ActiveDebugLayerExtension::None;
    m_validationConfig = VulkanValidationConfig{};
    m_lastCreateResult = VK_SUCCESS;

    if (hadInstance)
    {
        VulkanLoader::clearInstanceFunctions();
    }
}

}   // namespace gp::rhi::vulkan
