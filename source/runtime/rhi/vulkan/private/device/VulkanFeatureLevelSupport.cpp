// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanFeatureLevelSupport.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/DeviceEnums.hpp"
#include "instance/VulkanInstance.hpp"
#include "instance/VulkanInstanceDesc.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "logging/LogMacros.hpp"
#include "threading/sync/Mutex.hpp"
#include "threading/sync/UniqueLock.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Guards the cached probe.
gp::thread::Mutex g_probeMutex{};

/// @brief Last complete probe.
VulkanAdapterProbe g_cachedProbe{};

/// @brief True once `g_cachedProbe` holds a complete probe.
bool g_hasCachedProbe{ false };

/// @brief Checks the `SM5` feature set shared by `SM5` and `SM6`.
/// @param[in] features Core device features.
/// @return True when every `SM5` feature is present.
[[nodiscard]] bool hasShaderModel5Features(const VkPhysicalDeviceFeatures& features) noexcept
{
    return (features.independentBlend == VK_TRUE) && (features.imageCubeArray == VK_TRUE) &&
           (features.samplerAnisotropy == VK_TRUE) && (features.fragmentStoresAndAtomics == VK_TRUE) &&
           (features.shaderImageGatherExtended == VK_TRUE) && (features.fullDrawIndexUint32 == VK_TRUE);
}

/// @brief Checks the features `SM6` adds on top of `SM5`.
/// @param[in] features Core device features.
/// @return True when every additional `SM6` feature is present.
[[nodiscard]] bool hasShaderModel6Features(const VkPhysicalDeviceFeatures& features) noexcept
{
    return (features.shaderInt64 == VK_TRUE) && (features.shaderInt16 == VK_TRUE) &&
           (features.multiDrawIndirect == VK_TRUE) && (features.drawIndirectFirstInstance == VK_TRUE);
}

/// @brief Enumerates the adapters of a live instance and runs the selection policy over them.
/// @param[in] instance Live instance.
/// @param[out] outProbe Probe receiving the adapters and the selected index.
void enumerateAdapters(const VulkanInstance& instance, VulkanAdapterProbe& outProbe) noexcept
{
    const gp::Vector<VulkanPhysicalDeviceInfo> infos =
        enumeratePhysicalDevices(instance.getHandle(), instance.getApiVersion());

    outProbe.adapters.reserve(infos.size());
    for (const VulkanPhysicalDeviceInfo& info: infos)
    {
        VulkanAdapterCapabilities capabilities{};
        const gp::UInt32 deviceVersion = info.properties2.properties.apiVersion;
        capabilities.apiVersion = (deviceVersion < instance.getApiVersion()) ? deviceVersion : instance.getApiVersion();
        capabilities.isCpu = (info.properties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);
        if (dynamic_api::vkGetPhysicalDeviceFeatures != nullptr)
        {
            vkGetPhysicalDeviceFeatures(info.physicalDevice, &capabilities.features);
        }
        outProbe.adapters.pushBack(capabilities);
    }

    outProbe.selectedAdapter = selectPhysicalDeviceIndex(
        gp::ConstVectorView<VulkanPhysicalDeviceInfo>(infos.data(), infos.size()), resolvePhysicalDeviceSelectionDesc()
    );
}

/// @brief Probes the host from scratch.
/// @return Fresh probe; incomplete when another instance is alive.
[[nodiscard]] VulkanAdapterProbe probeHost() noexcept
{
    VulkanAdapterProbe probe{};

    // Instance entry points are process-wide: a live instance means the RHI (or a tool) already runs on Vulkan, and a
    // throwaway instance would clear its function table on shutdown. Report the loader only.
    if (VulkanLoader::isLibraryLoaded() && (dynamic_api::vkEnumeratePhysicalDevices != nullptr))
    {
        probe.hasLoader = true;
        return probe;
    }

    const bool wasLoaded = VulkanLoader::isLibraryLoaded();
    if (!VulkanLoader::loadLibrary())
    {
        GP_LOG_INFO(LogVulkanRHI, "Vulkan loader library is unavailable; the Vulkan RHI is not supported.");
        probe.isComplete = true;
        return probe;
    }
    probe.hasLoader = true;

    {
        VulkanInstanceCreateDesc desc{};
        desc.applicationName = "GraphicalPlaygroundVulkanProbe";
        desc.allowRetryWithoutLayers = true;
        desc.allowMissingSurfaceFunctions = true;
        desc.enableDebugCallbacks = false;

        VulkanInstance instance{};
        if (instance.initialize(desc))
        {
            enumerateAdapters(instance, probe);
            instance.shutdown();
        }
        else
        {
            GP_LOG_INFO(LogVulkanRHI, "Vulkan instance creation failed; the Vulkan RHI is not supported.");
        }
    }
    probe.isComplete = true;

    if (!wasLoaded)
    {
        VulkanLoader::freeLibrary();
    }
    return probe;
}

}   // namespace

bool meetsVulkanFeatureLevelRequirements(
    const FeatureLevel featureLevel, const gp::UInt32 deviceApiVersion, const VkPhysicalDeviceFeatures& features
) noexcept
{
    switch (featureLevel)
    {
    case FeatureLevel::ES3_1:
        return deviceApiVersion >= VK_API_VERSION_1_1;
    case FeatureLevel::SM5:
        return (deviceApiVersion >= VK_API_VERSION_1_1) && hasShaderModel5Features(features);
    case FeatureLevel::SM6:
        return (deviceApiVersion >= VK_API_VERSION_1_3) && hasShaderModel5Features(features) &&
               hasShaderModel6Features(features);
    case FeatureLevel::ES2Removed:
    case FeatureLevel::SM4Removed:
    case FeatureLevel::Count:
    default:
        break;
    }
    return false;
}

FeatureLevel
    getMaxVulkanFeatureLevel(const gp::UInt32 deviceApiVersion, const VkPhysicalDeviceFeatures& features) noexcept
{
    if (meetsVulkanFeatureLevelRequirements(FeatureLevel::SM6, deviceApiVersion, features))
    {
        return FeatureLevel::SM6;
    }
    if (meetsVulkanFeatureLevelRequirements(FeatureLevel::SM5, deviceApiVersion, features))
    {
        return FeatureLevel::SM5;
    }
    if (meetsVulkanFeatureLevelRequirements(FeatureLevel::ES3_1, deviceApiVersion, features))
    {
        return FeatureLevel::ES3_1;
    }
    return FeatureLevel::Count;
}

bool isVulkanBackendAvailable(const VulkanAdapterProbe& probe) noexcept
{
    if (!probe.hasLoader)
    {
        return false;
    }
    return !probe.isComplete || (probe.getSelectedAdapter() != nullptr);
}

bool isVulkanFeatureLevelAvailable(
    const FeatureLevel featureLevel,
    const VulkanAdapterProbe& probe,
    const bool enforceRequirements,
    const bool isEditor
) noexcept
{
    if ((featureLevel == FeatureLevel::ES2Removed) || (featureLevel == FeatureLevel::SM4Removed))
    {
        return false;
    }
    if ((featureLevel == FeatureLevel::ES3_1) && isEditor)
    {
        return false;
    }
    if (!isVulkanBackendAvailable(probe))
    {
        return false;
    }
    if ((featureLevel == FeatureLevel::Count) || !enforceRequirements || !probe.isComplete)
    {
        return true;
    }

    const VulkanAdapterCapabilities* const adapter = probe.getSelectedAdapter();
    return (adapter != nullptr) &&
           meetsVulkanFeatureLevelRequirements(featureLevel, adapter->apiVersion, adapter->features);
}

FeatureLevel resolveVulkanFeatureLevel(const FeatureLevel featureLevel, const VulkanAdapterProbe& probe) noexcept
{
    if (featureLevel != FeatureLevel::Count)
    {
        return featureLevel;
    }

    const VulkanAdapterCapabilities* const adapter = probe.isComplete ? probe.getSelectedAdapter() : nullptr;
    if (adapter == nullptr)
    {
        return FeatureLevel::SM5;
    }

    const FeatureLevel maxFeatureLevel = getMaxVulkanFeatureLevel(adapter->apiVersion, adapter->features);
    return (maxFeatureLevel == FeatureLevel::Count) ? FeatureLevel::SM5 : maxFeatureLevel;
}

VulkanAdapterProbe getVulkanAdapterProbe() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(g_probeMutex);
    if (g_hasCachedProbe)
    {
        return g_cachedProbe;
    }

    VulkanAdapterProbe probe = probeHost();
    if (probe.isComplete)
    {
        g_cachedProbe = probe;
        g_hasCachedProbe = true;
    }
    return probe;
}

void resetVulkanAdapterProbeCache() noexcept
{
    gp::thread::UniqueLock<gp::thread::Mutex> lock(g_probeMutex);
    g_cachedProbe = VulkanAdapterProbe{};
    g_hasCachedProbe = false;
}

}   // namespace gp::rhi::vulkan
