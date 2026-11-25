// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanDevice.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/RHIGlobals.hpp"
#include "extensions/VulkanDeviceExtension.hpp"
#include "instance/VulkanPhysicalDevice.hpp"
#include "layers/VulkanLayers.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanLoader.hpp"
#include "logging/LogMacros.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "platform/VulkanPlatform.hpp"
#include "translation/VulkanPixelFormat.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanVrsFormat(
    "r.Vulkan.VRSFormat",
    static_cast<gp::Int32>(VulkanVariableRateShadingPreference::PreferFSR),
    "Variable Rate Shading preference (0=PreferFSR, 1=UseFSROnly, 2=RequireFSR, 3=PreferFDM, 4=UseFDMOnly, " "5=RequireFDM).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

[[nodiscard]] bool readUseCoreStructsCvar() noexcept
{
    if (gp::console::IConsoleManager::isGlobalInitialized())
    {
        gp::console::IConsoleVariable* const cvar =
            gp::console::IConsoleManager::get().findConsoleVariable(gp::StringView("r.Vulkan.UseCoreStructs"));
        if (cvar != nullptr)
        {
            return cvar->getInt() != 0;
        }
    }
    return false;
}

}   // namespace

VulkanDevice::VulkanDevice(
    const VkInstance instance,
    const VkPhysicalDevice physicalDevice,
    const gp::UInt32 apiVersion,
    const bool supportsDebugUtilsExt
) noexcept
    : m_instance(instance)
    , m_physicalDevice(physicalDevice)
    , m_apiVersion(apiVersion)
    , m_supportsDebugUtilsExt(supportsDebugUtilsExt)
{
    for (gp::USize i = 0u; i < m_queueHandles.size(); ++i)
    {
        m_queueHandles[i] = VK_NULL_HANDLE;
    }
    gp::memory::zeroMemory(m_cachedFormatProperties.data(), sizeof(m_cachedFormatProperties));

    if (m_physicalDevice != VK_NULL_HANDLE)
    {
        m_features.query(m_physicalDevice, m_apiVersion);
        m_properties.query(m_physicalDevice, m_apiVersion);
        m_vendorId = resolveVulkanVendorId(m_properties.getCore10().vendorID);
    }
}

VulkanDevice::~VulkanDevice() noexcept
{
    destroy();
}

bool VulkanDevice::initialize(const bool applyToRHIGlobals) noexcept
{
    if (m_device != VK_NULL_HANDLE)
    {
        return true;
    }

    if (m_physicalDevice == VK_NULL_HANDLE || dynamic_api::vkCreateDevice == nullptr ||
        dynamic_api::vkGetPhysicalDeviceQueueFamilyProperties == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Cannot initialize VulkanDevice: physical device or core entry points unavailable.");
        m_lastCreateResult = VK_ERROR_INITIALIZATION_FAILED;
        return false;
    }

    // 1. Enumerate queue family properties.
    gp::UInt32 queueFamilyCount = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0u)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Physical device reported zero queue families.");
        m_lastCreateResult = VK_ERROR_INITIALIZATION_FAILED;
        return false;
    }
    m_queueFamilyProps.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, m_queueFamilyProps.data());

    // 2. Discover supported device extensions and layers.
    VulkanDeviceExtensionArray extensions =
        VulkanDeviceExtension::getSupportedDeviceExtensions(this, m_physicalDevice, m_apiVersion);
    VulkanDeviceLayerSetupResult layerSetup = setupDeviceLayers(m_physicalDevice, m_supportsDebugUtilsExt, extensions);

    m_enabledLayers = std::move(layerSetup.enabledLayers);
    m_renderDocFound = layerSetup.renderDocFound;
    m_useLegacyDebugMarkerExt = layerSetup.useLegacyDebugMarkerExt;
    m_optionalExtensions = OptionalVulkanDeviceExtensions{};
    m_optionalExtensionProperties = OptionalVulkanDeviceExtensionProperties{};
    m_enabledExtensions.clear();

    const bool useCoreStructs = readUseCoreStructsCvar();

    // 3. Process core-promoted extensions when Vulkan 1.2+ core structs are enabled.
    if ((m_apiVersion >= VK_API_VERSION_1_2) && useCoreStructs)
    {
        const VulkanCoreFeaturesAndProperties coreData = m_properties.makeCoreFeaturesAndProperties(m_features);
        for (gp::UniquePtr<VulkanDeviceExtension>& ext: extensions)
        {
            if (ext != nullptr && ext->usedAsCore())
            {
                ext->handleCoreFeaturesAndProperties(coreData, m_optionalExtensions);
            }
        }
    }

    // 4. Query extension features via VkPhysicalDeviceFeatures2.
    if (dynamic_api::vkGetPhysicalDeviceFeatures2 != nullptr)
    {
        VkPhysicalDeviceFeatures2 extFeatures2 =
            zeroVulkanStruct<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        for (gp::UniquePtr<VulkanDeviceExtension>& ext: extensions)
        {
            if (ext != nullptr && ext->usedAsExtension())
            {
                ext->prePhysicalDeviceFeatures(extFeatures2);
            }
        }
        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &extFeatures2);
        for (gp::UniquePtr<VulkanDeviceExtension>& ext: extensions)
        {
            if (ext != nullptr && ext->usedAsExtension())
            {
                ext->postPhysicalDeviceFeatures(m_optionalExtensions);
            }
        }
    }

    // 5. Query extension properties via VkPhysicalDeviceProperties2.
    if (dynamic_api::vkGetPhysicalDeviceProperties2 != nullptr)
    {
        VkPhysicalDeviceProperties2 extProps2 =
            zeroVulkanStruct<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
        for (gp::UniquePtr<VulkanDeviceExtension>& ext: extensions)
        {
            if (ext != nullptr && ext->usedAsExtension())
            {
                ext->prePhysicalDeviceProperties(extProps2);
            }
        }
        vkGetPhysicalDeviceProperties2(m_physicalDevice, &extProps2);
        for (gp::UniquePtr<VulkanDeviceExtension>& ext: extensions)
        {
            if (ext != nullptr && ext->usedAsExtension())
            {
                ext->postPhysicalDeviceProperties(m_optionalExtensions);
            }
        }
    }

    // 6. Choose VRS method (Fragment Shading Rate vs Fragment Density Map).
    const gp::Int32 rawVrsPref = g_cvarVulkanVrsFormat.getValueOnAnyThread(true);
    const VulkanVariableRateShadingPreference vrsPreference =
        (rawVrsPref >= 0 && rawVrsPref <= static_cast<gp::Int32>(VulkanVariableRateShadingPreference::RequireFDM))
            ? static_cast<VulkanVariableRateShadingPreference>(rawVrsPref)
            : VulkanVariableRateShadingPreference::PreferFSR;
    chooseVariableRateShadingMethod(m_optionalExtensions, m_optionalExtensionProperties, vrsPreference, m_vrsCaps);

    // 7. Assemble VkDeviceCreateInfo with features, extensions, layers, and queue families.
    VkDeviceCreateInfo deviceInfo = zeroVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    m_features.addToCreateDevice(deviceInfo, useCoreStructs);

    for (gp::UniquePtr<VulkanDeviceExtension>& ext: extensions)
    {
        if (ext != nullptr && ext->usedAsExtension() && ext->didRequirementsPass())
        {
            m_enabledExtensions.pushBack(ext->getExtensionName());
            ext->preCreateDevice(deviceInfo);
        }
    }

    deviceInfo.enabledExtensionCount = static_cast<gp::UInt32>(m_enabledExtensions.size());
    deviceInfo.ppEnabledExtensionNames = (deviceInfo.enabledExtensionCount > 0u) ? m_enabledExtensions.data() : nullptr;

    deviceInfo.enabledLayerCount = static_cast<gp::UInt32>(m_enabledLayers.size());
    deviceInfo.ppEnabledLayerNames = (deviceInfo.enabledLayerCount > 0u) ? m_enabledLayers.data() : nullptr;

    const VulkanQueueFamilySelectionConfig queueConfig =
        resolveQueueFamilySelectionConfig(supportsParallelRendering(), m_optionalExtensions.hasKHRMaintenance9 != 0u);
    m_queueIndices = selectQueueFamilies(
        gp::ConstVectorView<VkQueueFamilyProperties>(m_queueFamilyProps.data(), m_queueFamilyProps.size()), queueConfig
    );
    if (m_queueIndices.graphicsFamily < 0)
    {
        GP_LOG_ERROR(LogVulkanRHI, "No graphics-capable queue family found on selected physical device.");
        m_lastCreateResult = VK_ERROR_INITIALIZATION_FAILED;
        return false;
    }

    gp::Vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    gp::Vector<gp::Float32> queuePriorities;
    buildDeviceQueueCreateInfos(
        gp::ConstVectorView<VkQueueFamilyProperties>(m_queueFamilyProps.data(), m_queueFamilyProps.size()),
        m_queueIndices,
        queueCreateInfos,
        queuePriorities
    );

    deviceInfo.queueCreateInfoCount = static_cast<gp::UInt32>(queueCreateInfos.size());
    deviceInfo.pQueueCreateInfos = queueCreateInfos.data();

    // 8. Create the logical VkDevice.
    const VkResult result = vkCreateDevice(m_physicalDevice, &deviceInfo, GP_VULKAN_CPU_ALLOCATOR, &m_device);
    m_lastCreateResult = result;
    if (result != VK_SUCCESS)
    {
        GP_LOG_ERROR(LogVulkanRHI, "vkCreateDevice failed with {}.", vulkanResultToString(result));
        m_device = VK_NULL_HANDLE;
        return false;
    }

    VulkanPlatform::notifyFoundDeviceLayersAndExtensions(
        m_physicalDevice,
        gp::ConstVectorView<const char*>(m_enabledLayers.data(), m_enabledLayers.size()),
        gp::ConstVectorView<const char*>(m_enabledExtensions.data(), m_enabledExtensions.size())
    );

    // 9. Upgrade device-dispatchable entry points to direct ICD pointers.
    static_cast<void>(VulkanLoader::loadDeviceFunctions(m_device));

    // 10. Retrieve created VkQueue handles.
    if (dynamic_api::vkGetDeviceQueue != nullptr)
    {
        vkGetDeviceQueue(
            m_device,
            static_cast<gp::UInt32>(m_queueIndices.graphicsFamily),
            0u,
            &m_queueHandles[static_cast<gp::USize>(VulkanQueueType::Graphics)]
        );
        if (m_queueIndices.asyncComputeFamily >= 0)
        {
            vkGetDeviceQueue(
                m_device,
                static_cast<gp::UInt32>(m_queueIndices.asyncComputeFamily),
                0u,
                &m_queueHandles[static_cast<gp::USize>(VulkanQueueType::AsyncCompute)]
            );
        }
        if (m_queueIndices.transferFamily >= 0)
        {
            vkGetDeviceQueue(
                m_device,
                static_cast<gp::UInt32>(m_queueIndices.transferFamily),
                0u,
                &m_queueHandles[static_cast<gp::USize>(VulkanQueueType::Transfer)]
            );
        }
    }

    // 11. Query queue family ownership transfer support if async compute + maintenance9 are active.
    queryQueueOwnershipTransferSupport();

    // 12. Initialize debug markers / object naming dispatch table.
    m_debugMarkers.initialize(
        m_instance,
        m_physicalDevice,
        m_device,
        m_supportsDebugUtilsExt,
        m_useLegacyDebugMarkerExt,
        m_optionalExtensions.hasEXTToolingInfo != 0u,
        false,
        m_renderDocFound
    );

    // 13. Probe pixel and vertex format capabilities.
    setupFormats();

    // 14. Populate global RHI capability table if requested.
    if (applyToRHIGlobals)
    {
        populateRHIGlobalsFromDevice(
            m_features,
            m_properties,
            m_optionalExtensions,
            m_optionalExtensionProperties,
            m_queueIndices,
            gp::ConstVectorView<VkQueueFamilyProperties>(m_queueFamilyProps.data(), m_queueFamilyProps.size()),
            m_formatSupportTable,
            m_vertexElementSupport,
            m_vrsCaps,
            gp::rhi::getRHIGlobals()
        );
    }

    return true;
}

void VulkanDevice::waitUntilIdle() const noexcept
{
    if (m_device != VK_NULL_HANDLE && dynamic_api::vkDeviceWaitIdle != nullptr)
    {
        const VkResult result = vkDeviceWaitIdle(m_device);
        if (result != VK_SUCCESS)
        {
            GP_LOG_WARNING(LogVulkanRHI, "vkDeviceWaitIdle returned {}.", vulkanResultToString(result));
        }
    }
}

void VulkanDevice::destroy() noexcept
{
    if (m_device != VK_NULL_HANDLE)
    {
        waitUntilIdle();
        if (dynamic_api::vkDestroyDevice != nullptr)
        {
            vkDestroyDevice(m_device, GP_VULKAN_CPU_ALLOCATOR);
        }
        m_device = VK_NULL_HANDLE;
    }

    m_debugMarkers.reset();
    for (gp::USize i = 0u; i < m_queueHandles.size(); ++i)
    {
        m_queueHandles[i] = VK_NULL_HANDLE;
    }
    m_enabledExtensions.clear();
    m_enabledLayers.clear();
    m_queueFamilyProps.clear();
    m_queueIndices = VulkanQueueFamilyIndices{};
    m_optionalExtensions = OptionalVulkanDeviceExtensions{};
    m_optionalExtensionProperties = OptionalVulkanDeviceExtensionProperties{};
    m_vrsCaps = gp::rhi::VariableRateShadingCapabilities{};
    m_formatSupportTable = VulkanFormatSupportTable{};
    m_vertexElementSupport = gp::rhi::VertexElementTypeSupportInfo{};
    gp::memory::zeroMemory(m_cachedFormatProperties.data(), sizeof(m_cachedFormatProperties));
    m_formatPropertiesCached = false;
    m_renderDocFound = false;
    m_useLegacyDebugMarkerExt = false;
    m_lastCreateResult = VK_SUCCESS;
}

VkFormatProperties VulkanDevice::getFormatProperties(const VkFormat format) const noexcept
{
    const gp::Int32 formatInt = static_cast<gp::Int32>(format);
    if (m_formatPropertiesCached && formatInt >= 0 && static_cast<gp::USize>(formatInt) < kCoreFormatRangeSize)
    {
        return m_cachedFormatProperties[static_cast<gp::USize>(formatInt)];
    }

    VkFormatProperties props{};
    gp::memory::zeroMemory(&props, sizeof(props));
    if (m_physicalDevice != VK_NULL_HANDLE && dynamic_api::vkGetPhysicalDeviceFormatProperties != nullptr)
    {
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
    }
    return props;
}

const VkComponentMapping& VulkanDevice::getFormatComponentMapping(const PixelFormat format) const noexcept
{
    const gp::USize index = static_cast<gp::USize>(format);
    if (index < m_formatSupportTable.componentMappings.size())
    {
        return m_formatSupportTable.componentMappings[index];
    }
    return kComponentMappingRGBA;
}

void VulkanDevice::setupFormats() noexcept
{
    gp::memory::zeroMemory(m_cachedFormatProperties.data(), sizeof(m_cachedFormatProperties));
    if (m_physicalDevice != VK_NULL_HANDLE && dynamic_api::vkGetPhysicalDeviceFormatProperties != nullptr)
    {
        for (gp::USize index = 0u; index < kCoreFormatRangeSize; ++index)
        {
            vkGetPhysicalDeviceFormatProperties(
                m_physicalDevice, static_cast<VkFormat>(index), &m_cachedFormatProperties[index]
            );
        }
        m_formatPropertiesCached = true;
    }

    VulkanFormatPropertiesQueryFn formatQuery = [this](const VkFormat format) noexcept -> VkFormatProperties
    {
        return getFormatProperties(format);
    };

    VulkanMipMapSupportQueryFn mipQuery = [this](const VkFormat format) noexcept -> bool
    {
        if (m_physicalDevice == VK_NULL_HANDLE || dynamic_api::vkGetPhysicalDeviceImageFormatProperties == nullptr)
        {
            return false;
        }
        VkImageFormatProperties imgProps{};
        const VkResult res = vkGetPhysicalDeviceImageFormatProperties(
            m_physicalDevice,
            format,
            VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT,
            0u,
            &imgProps
        );
        return (res == VK_SUCCESS) && (imgProps.maxMipLevels > 1u);
    };

    populateFormatSupportTable(m_formatSupportTable, m_features, m_renderDocFound, formatQuery, mipQuery);
    populateVertexElementTypeSupport(m_vertexElementSupport, formatQuery);
}

void VulkanDevice::queryQueueOwnershipTransferSupport() noexcept
{
    if (!hasAsyncComputeQueue() || m_optionalExtensions.hasKHRMaintenance9 == 0u ||
        dynamic_api::vkGetPhysicalDeviceQueueFamilyProperties2 == nullptr || m_queueFamilyProps.isEmpty())
    {
        return;
    }

    const gp::USize familyCount = m_queueFamilyProps.size();
    gp::Vector<VkQueueFamilyProperties2> familyProps2(familyCount);
    gp::Vector<VkQueueFamilyOwnershipTransferPropertiesKHR> transferProps(familyCount);

    for (gp::USize i = 0u; i < familyCount; ++i)
    {
        transferProps[i] = zeroVulkanStruct<VkQueueFamilyOwnershipTransferPropertiesKHR>(
            VK_STRUCTURE_TYPE_QUEUE_FAMILY_OWNERSHIP_TRANSFER_PROPERTIES_KHR
        );
        familyProps2[i] = zeroVulkanStruct<VkQueueFamilyProperties2>(VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2);
        familyProps2[i].pNext = &transferProps[i];
    }

    gp::UInt32 count = static_cast<gp::UInt32>(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &count, familyProps2.data());

    const gp::USize gfxIdx = static_cast<gp::USize>(m_queueIndices.graphicsFamily);
    const gp::USize computeIdx = static_cast<gp::USize>(m_queueIndices.asyncComputeFamily);
    const bool gfxToCompute = (transferProps[gfxIdx].optimalImageTransferToQueueFamilies & (1u << computeIdx)) != 0u;
    const bool computeToGfx = (transferProps[computeIdx].optimalImageTransferToQueueFamilies & (1u << gfxIdx)) != 0u;
    bool transferOk = true;
    if (m_queueIndices.transferFamily >= 0)
    {
        const gp::USize xferIdx = static_cast<gp::USize>(m_queueIndices.transferFamily);
        transferOk = ((transferProps[xferIdx].optimalImageTransferToQueueFamilies & (1u << gfxIdx)) != 0u) &&
                     ((transferProps[xferIdx].optimalImageTransferToQueueFamilies & (1u << computeIdx)) != 0u);
    }

    m_queueIndices.needsQueueFamilyOwnershipTransfers = !(gfxToCompute && computeToGfx && transferOk);
}

}   // namespace gp::rhi::vulkan
