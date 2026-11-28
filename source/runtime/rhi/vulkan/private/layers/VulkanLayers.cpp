// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "layers/VulkanLayers.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "platform/VulkanPlatform.hpp"
#include "templates/Optional.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanConfiguration.hpp"
#include <cstring>
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanUseCoreStructs(
    "r.Vulkan.UseCoreStructs",
    0,
    "When API version allows it, use core Vulkan feature/property structs instead of extension structs.",
    gp::console::ConsoleVarFlags::ReadOnly
);

#if GP_VULKAN_HAS_DEBUGGING_ENABLED
gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanEnableValidation(
    "r.Vulkan.EnableValidation",
    GP_VULKAN_VALIDATION_DEFAULT_VALUE,
    "0: disable validation layers; 1: errors; 2: errors & warnings; 3: +perf warnings; 4: +info; 5: all messages.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanGPUValidation(
    "r.Vulkan.GPUValidation",
    0,
    "0: disabled; 1: enable GPU-assisted validation; 2: enable GPU-assisted validation and reserve binding slot.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanEnableAPIDump(
    "r.Vulkan.EnableAPIDump",
    0,
    "0: disabled; 1: dump all Vulkan API calls via VK_LAYER_LUNARG_api_dump.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);
#endif

gp::Vector<const char*>& getExternalInstanceLayersStorage() noexcept
{
    static gp::Vector<const char*> externalLayers;
    return externalLayers;
}

gp::Vector<const char*>& getExternalDeviceLayersStorage() noexcept
{
    static gp::Vector<const char*> externalLayers;
    return externalLayers;
}

void sortLayersByName(gp::Vector<VulkanLayerWithExtensions>& layers) noexcept
{
    for (gp::USize i = 1u; i < layers.size(); ++i)
    {
        VulkanLayerWithExtensions key = std::move(layers[i]);
        gp::USize j = i;
        while (j > 0u && std::strcmp(key.layerProperties.layerName, layers[j - 1u].layerProperties.layerName) < 0)
        {
            layers[j] = std::move(layers[j - 1u]);
            --j;
        }
        layers[j] = std::move(key);
    }
}

void deduplicateAndSortNames(gp::Vector<const char*>& names) noexcept
{
    // Remove duplicates while preserving one copy of each unique string.
    gp::USize writeIndex = 0u;
    for (gp::USize readIndex = 0u; readIndex < names.size(); ++readIndex)
    {
        const char* const candidate = names[readIndex];
        if (candidate == nullptr)
        {
            continue;
        }

        bool duplicate = false;
        for (gp::USize prev = 0u; prev < writeIndex; ++prev)
        {
            if (std::strcmp(names[prev], candidate) == 0)
            {
                duplicate = true;
                break;
            }
        }

        if (!duplicate)
        {
            names[writeIndex] = candidate;
            ++writeIndex;
        }
    }
    names.resize(writeIndex);

    // Lexicographical insertion sort.
    for (gp::USize i = 1u; i < names.size(); ++i)
    {
        const char* const key = names[i];
        gp::USize j = i;
        while (j > 0u && std::strcmp(key, names[j - 1u]) < 0)
        {
            names[j] = names[j - 1u];
            --j;
        }
        names[j] = key;
    }
}

}   // namespace

gp::Int32 getVulkanValidationLevel() noexcept
{
#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    return g_cvarVulkanEnableValidation.getValueOnGameThread();
#else
    return 0;
#endif
}

gp::Int32 getVulkanGPUValidationLevel() noexcept
{
#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    return g_cvarVulkanGPUValidation.getValueOnGameThread();
#else
    return 0;
#endif
}

VulkanValidationConfig setupValidationRequests() noexcept
{
    VulkanValidationConfig config{};

#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    gp::Int32 validationLevel = g_cvarVulkanEnableValidation.getValueOnGameThread();
    gp::Int32 gpuValidationLevel = g_cvarVulkanGPUValidation.getValueOnGameThread();

    if (gp::CommandLine::hasArgument("vulkandebug"))
    {
        validationLevel = 2;
        static_cast<void>(g_cvarVulkanEnableValidation.set(validationLevel, gp::console::ConsoleVarSetBy::CommandLine));
    }
    else
    {
        const gp::Optional<gp::Int32> cliValidation = gp::CommandLine::tryGetInt32("vulkanvalidation");
        if (cliValidation.hasValue())
        {
            validationLevel = cliValidation.value();
            static_cast<void>(
                g_cvarVulkanEnableValidation.set(validationLevel, gp::console::ConsoleVarSetBy::CommandLine)
            );
        }
    }

    if (gp::CommandLine::hasArgument("gpuvalidation"))
    {
        if (validationLevel < 2)
        {
            validationLevel = 2;
            static_cast<void>(
                g_cvarVulkanEnableValidation.set(validationLevel, gp::console::ConsoleVarSetBy::CommandLine)
            );
        }
        gpuValidationLevel = 2;
        static_cast<void>(g_cvarVulkanGPUValidation.set(gpuValidationLevel, gp::console::ConsoleVarSetBy::CommandLine));
    }

    config.validationLevel = validationLevel;
    config.gpuValidationLevel = gpuValidationLevel;
    config.isDebugLayerEnabled = (validationLevel > 0);
#endif

    return config;
}

gp::Vector<VulkanLayerWithExtensions> enumerateInstanceLayerProperties() noexcept
{
    gp::Vector<VulkanLayerWithExtensions> outLayers;
    if (dynamic_api::vkEnumerateInstanceLayerProperties == nullptr)
    {
        return outLayers;
    }

    gp::UInt32 count = 0u;
    VkResult result = vkEnumerateInstanceLayerProperties(&count, nullptr);
    if (result == VK_ERROR_INITIALIZATION_FAILED)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "vkEnumerateInstanceLayerProperties failed with VK_ERROR_INITIALIZATION_FAILED; verify that a Vulkan " "runtime is installed."
        );
        return outLayers;
    }
    if (result < VK_SUCCESS)
    {
        verifyVulkanResult(result, "vkEnumerateInstanceLayerProperties", __FILE__, static_cast<gp::UInt32>(__LINE__));
        return outLayers;
    }

    if (count > 0u)
    {
        gp::Vector<VkLayerProperties> rawLayers;
        rawLayers.resize(count);
        result = vkEnumerateInstanceLayerProperties(&count, rawLayers.data());
        if (result < VK_SUCCESS)
        {
            if (result != VK_ERROR_INITIALIZATION_FAILED)
            {
                verifyVulkanResult(
                    result, "vkEnumerateInstanceLayerProperties", __FILE__, static_cast<gp::UInt32>(__LINE__)
                );
            }
            return outLayers;
        }

        outLayers.reserve(count);
        for (gp::UInt32 i = 0u; i < count; ++i)
        {
            VulkanLayerWithExtensions entry{};
            entry.layerProperties = rawLayers[i];
            entry.extensionProperties =
                VulkanInstanceExtension::getDriverSupportedInstanceExtensions(rawLayers[i].layerName);
            outLayers.pushBack(std::move(entry));
        }
    }

    return outLayers;
}

gp::Vector<VulkanLayerWithExtensions> enumerateDeviceLayerProperties(const VkPhysicalDevice gpu) noexcept
{
    gp::Vector<VulkanLayerWithExtensions> outLayers;
    if (gpu == VK_NULL_HANDLE || dynamic_api::vkEnumerateDeviceLayerProperties == nullptr)
    {
        return outLayers;
    }

    gp::UInt32 count = 0u;
    VkResult result = vkEnumerateDeviceLayerProperties(gpu, &count, nullptr);
    if (result == VK_ERROR_INITIALIZATION_FAILED)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "vkEnumerateDeviceLayerProperties failed with VK_ERROR_INITIALIZATION_FAILED; verify that a Vulkan driver " "is installed."
        );
        return outLayers;
    }
    if (result < VK_SUCCESS)
    {
        verifyVulkanResult(result, "vkEnumerateDeviceLayerProperties", __FILE__, static_cast<gp::UInt32>(__LINE__));
        return outLayers;
    }

    if (count > 0u)
    {
        gp::Vector<VkLayerProperties> rawLayers;
        rawLayers.resize(count);
        result = vkEnumerateDeviceLayerProperties(gpu, &count, rawLayers.data());
        if (result < VK_SUCCESS)
        {
            if (result != VK_ERROR_INITIALIZATION_FAILED)
            {
                verifyVulkanResult(
                    result, "vkEnumerateDeviceLayerProperties", __FILE__, static_cast<gp::UInt32>(__LINE__)
                );
            }
            return outLayers;
        }

        outLayers.reserve(count);
        for (gp::UInt32 i = 0u; i < count; ++i)
        {
            VulkanLayerWithExtensions entry{};
            entry.layerProperties = rawLayers[i];
            entry.extensionProperties =
                VulkanDeviceExtension::getDriverSupportedDeviceExtensions(gpu, rawLayers[i].layerName);
            outLayers.pushBack(std::move(entry));
        }
    }

    return outLayers;
}

gp::Int32 findLayerIndexInList(
    const char* const layerName, const gp::ConstVectorView<VulkanLayerWithExtensions> layerProperties
) noexcept
{
    if (layerName == nullptr)
    {
        return -1;
    }

    for (gp::USize index = 0u; index < layerProperties.size(); ++index)
    {
        if (std::strcmp(layerProperties[index].layerProperties.layerName, layerName) == 0)
        {
            return static_cast<gp::Int32>(index);
        }
    }

    return -1;
}

VulkanInstanceLayerSetupResult setupInstanceLayers(
    const gp::ConstVectorView<VulkanLayerWithExtensions> availableLayers,
    VulkanInstanceExtensionArray& extensions,
    const VulkanValidationConfig& validationConfig
) noexcept
{
    VulkanInstanceLayerSetupResult result{};

    gp::Vector<VulkanLayerWithExtensions> sortedLayers;
    sortedLayers.reserve(availableLayers.size());
    for (const VulkanLayerWithExtensions& layer: availableLayers)
    {
        sortedLayers.pushBack(layer);
    }
    sortLayersByName(sortedLayers);

    const gp::ConstVectorView<VulkanLayerWithExtensions> sortedView(sortedLayers.data(), sortedLayers.size());

    GP_LOG_INFO(LogVulkanRHI, "Found {} available instance layers.", static_cast<gp::UInt32>(sortedLayers.size()));
    for (const VulkanLayerWithExtensions& prop: sortedLayers)
    {
        GP_LOG_TRACE(LogVulkanRHI, "  * {}", prop.layerProperties.layerName);
    }

    // Externally registered layers
    for (const char* const externalLayer: getExternalInstanceLayersStorage())
    {
        if (!addRequestedLayer(externalLayer, sortedView, extensions, result.enabledLayers))
        {
            GP_LOG_WARNING(LogVulkanRHI, "Unable to find externally requested instance layer '{}'.", externalLayer);
        }
    }

    // Platform-specific layers
    gp::Vector<const char*> platformLayers;
    VulkanPlatform::getInstanceLayers(platformLayers);
    for (const char* const platformLayer: platformLayers)
    {
        if (!addRequestedLayer(platformLayer, sortedView, extensions, result.enabledLayers))
        {
            GP_LOG_WARNING(LogVulkanRHI, "Unable to find platform instance layer '{}'.", platformLayer);
        }
    }

    // Capture layers requested via -vktrace
    if (gp::CommandLine::hasArgument("vktrace"))
    {
        if (addRequestedLayer(kGfxReconstructLayerName, sortedView, extensions, result.enabledLayers))
        {
            result.activeDebugLayerExtension = ActiveDebugLayerExtension::GfxReconstructLayer;
        }
        else if (addRequestedLayer(kVkTraceLayerName, sortedView, extensions, result.enabledLayers))
        {
            result.activeDebugLayerExtension = ActiveDebugLayerExtension::VkTraceLayer;
        }
    }

    const bool hasTraceLayer = (result.activeDebugLayerExtension != ActiveDebugLayerExtension::None);

#if GP_VULKAN_HAS_DEBUGGING_ENABLED
    if (gp::CommandLine::hasArgument("vulkanapidump") || (g_cvarVulkanEnableAPIDump.getValueOnGameThread() != 0))
    {
        if (hasTraceLayer)
        {
            GP_LOG_WARNING(LogVulkanRHI, "Cannot enable api_dump when GfxReconstruct or VkTrace is active.");
        }
        else if (addRequestedLayer(kApiDumpLayerName, sortedView, extensions, result.enabledLayers))
        {
            static constexpr VkBool32 kVulkanBoolTrue = VK_TRUE;
            static constexpr const char* kApiDumpLogFileNames[1] = { "vk_apidump.txt" };

            result.layerSettings.reserve(result.layerSettings.size() + 4u);

            VkLayerSettingEXT fileSetting{};
            fileSetting.pLayerName = kApiDumpLayerName;
            fileSetting.pSettingName = "file";
            fileSetting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
            fileSetting.valueCount = 1u;
            fileSetting.pValues = &kVulkanBoolTrue;
            result.layerSettings.pushBack(fileSetting);

            VkLayerSettingEXT filenameSetting{};
            filenameSetting.pLayerName = kApiDumpLayerName;
            filenameSetting.pSettingName = "log_filename";
            filenameSetting.type = VK_LAYER_SETTING_TYPE_STRING_EXT;
            filenameSetting.valueCount = 1u;
            filenameSetting.pValues = kApiDumpLogFileNames;
            result.layerSettings.pushBack(filenameSetting);

            VkLayerSettingEXT detailedSetting{};
            detailedSetting.pLayerName = kApiDumpLayerName;
            detailedSetting.pSettingName = "detailed";
            detailedSetting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
            detailedSetting.valueCount = 1u;
            detailedSetting.pValues = &kVulkanBoolTrue;
            result.layerSettings.pushBack(detailedSetting);

            VkLayerSettingEXT flushSetting{};
            flushSetting.pLayerName = kApiDumpLayerName;
            flushSetting.pSettingName = "flush";
            flushSetting.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
            flushSetting.valueCount = 1u;
            flushSetting.pValues = &kVulkanBoolTrue;
            result.layerSettings.pushBack(flushSetting);
        }
        else
        {
            GP_LOG_WARNING(LogVulkanRHI, "Unable to find Vulkan instance layer '{}'.", kApiDumpLayerName);
        }
    }

    const bool useValidation = validationConfig.isDebugLayerEnabled;
    if (!hasTraceLayer && useValidation)
    {
        if (!addRequestedLayer(kKhronosStandardValidationLayerName, sortedView, extensions, result.enabledLayers))
        {
            GP_LOG_WARNING(
                LogVulkanRHI,
                "Unable to find Vulkan validation layer '{}'; verify that the Vulkan SDK is installed.",
                kKhronosStandardValidationLayerName
            );
        }
    }

    #if GP_VULKAN_ENABLE_DRAW_MARKERS
    const bool forceDebugUtils = true;
    #else
    const bool forceDebugUtils = gp::CommandLine::hasArgument("vulkandebugutils");
    #endif
    if ((useValidation || forceDebugUtils) && !hasTraceLayer)
    {
        const auto activateDebugExtension = [&extensions](const char* const extensionName) noexcept -> bool
        {
            const gp::Int32 extIndex = VulkanInstanceExtension::findExtension(extensions, extensionName);
            if (extIndex >= 0 && extensions[static_cast<gp::USize>(extIndex)]->isSupported())
            {
                extensions[static_cast<gp::USize>(extIndex)]->setActivated();
                return true;
            }
            return false;
        };

        if (activateDebugExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            result.activeDebugLayerExtension = ActiveDebugLayerExtension::DebugUtilsExtension;
        }

        const bool requiresValidationFeatures = (validationConfig.gpuValidationLevel != 0) ||
                                                gp::CommandLine::hasArgument("vulkanbestpractices") ||
                                                gp::CommandLine::hasArgument("vulkandebugsync");
        if (requiresValidationFeatures)
        {
            static_cast<void>(activateDebugExtension(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME));
        }
    }
#else
    static_cast<void>(validationConfig);
    static_cast<void>(hasTraceLayer);
#endif

    deduplicateAndSortNames(result.enabledLayers);
    return result;
}

VulkanInstanceLayerSetupResult setupInstanceLayers(VulkanInstanceExtensionArray& extensions) noexcept
{
    const VulkanValidationConfig validationConfig = setupValidationRequests();
    const gp::Vector<VulkanLayerWithExtensions> availableLayers = enumerateInstanceLayerProperties();
    return setupInstanceLayers(
        gp::ConstVectorView<VulkanLayerWithExtensions>(availableLayers.data(), availableLayers.size()),
        extensions,
        validationConfig
    );
}

VulkanDeviceLayerSetupResult setupDeviceLayers(
    const gp::ConstVectorView<VulkanLayerWithExtensions> availableLayers,
    const bool supportsDebugUtilsExt,
    VulkanDeviceExtensionArray& extensions
) noexcept
{
    VulkanDeviceLayerSetupResult result{};

    gp::Vector<VulkanLayerWithExtensions> sortedLayers;
    sortedLayers.reserve(availableLayers.size());
    for (const VulkanLayerWithExtensions& layer: availableLayers)
    {
        sortedLayers.pushBack(layer);
    }
    sortLayersByName(sortedLayers);

    const gp::ConstVectorView<VulkanLayerWithExtensions> sortedView(sortedLayers.data(), sortedLayers.size());

    GP_LOG_INFO(LogVulkanRHI, "Found {} available device layers.", static_cast<gp::UInt32>(sortedLayers.size()));
    for (const VulkanLayerWithExtensions& prop: sortedLayers)
    {
        GP_LOG_TRACE(LogVulkanRHI, "  * {}", prop.layerProperties.layerName);
    }

    // Externally registered device layers
    for (const char* const externalLayer: getExternalDeviceLayersStorage())
    {
        if (!addRequestedLayer(externalLayer, sortedView, extensions, result.enabledLayers))
        {
            GP_LOG_WARNING(LogVulkanRHI, "Unable to find externally requested device layer '{}'.", externalLayer);
        }
    }

    // Platform-specific device layers
    gp::Vector<const char*> platformLayers;
    VulkanPlatform::getDeviceLayers(platformLayers);
    for (const char* const platformLayer: platformLayers)
    {
        if (!addRequestedLayer(platformLayer, sortedView, extensions, result.enabledLayers))
        {
            GP_LOG_WARNING(LogVulkanRHI, "Unable to find platform device layer '{}'.", platformLayer);
        }
    }

#if GP_VULKAN_HAS_DEBUGGING_ENABLED && GP_VULKAN_ENABLE_DRAW_MARKERS
    result.renderDocFound = (findLayerIndexInList(kRenderDocLayerName, sortedView) >= 0);
#else
    result.renderDocFound = false;
#endif

#if GP_VULKAN_ENABLE_DRAW_MARKERS
    if (!supportsDebugUtilsExt)
    {
        const gp::Int32 extIndex = VulkanDeviceExtension::findExtension(extensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        if (extIndex >= 0 && extensions[static_cast<gp::USize>(extIndex)]->isSupported())
        {
            extensions[static_cast<gp::USize>(extIndex)]->setActivated();
            result.useLegacyDebugMarkerExt = true;
        }
    }
#else
    static_cast<void>(supportsDebugUtilsExt);
#endif

    deduplicateAndSortNames(result.enabledLayers);
    return result;
}

VulkanDeviceLayerSetupResult setupDeviceLayers(
    const VkPhysicalDevice gpu, const bool supportsDebugUtilsExt, VulkanDeviceExtensionArray& extensions
) noexcept
{
    const gp::Vector<VulkanLayerWithExtensions> availableLayers = enumerateDeviceLayerProperties(gpu);
    return setupDeviceLayers(
        gp::ConstVectorView<VulkanLayerWithExtensions>(availableLayers.data(), availableLayers.size()),
        supportsDebugUtilsExt,
        extensions
    );
}

void addExternalInstanceLayers(const gp::ConstVectorView<const char*> layers) noexcept
{
    gp::Vector<const char*>& storage = getExternalInstanceLayersStorage();
    for (const char* const layerName: layers)
    {
        if (layerName != nullptr)
        {
            storage.pushBack(layerName);
        }
    }
}

void clearExternalInstanceLayers() noexcept
{
    getExternalInstanceLayersStorage().clear();
}

void addExternalDeviceLayers(const gp::ConstVectorView<const char*> layers) noexcept
{
    gp::Vector<const char*>& storage = getExternalDeviceLayersStorage();
    for (const char* const layerName: layers)
    {
        if (layerName != nullptr)
        {
            storage.pushBack(layerName);
        }
    }
}

void clearExternalDeviceLayers() noexcept
{
    getExternalDeviceLayersStorage().clear();
}

}   // namespace gp::rhi::vulkan
