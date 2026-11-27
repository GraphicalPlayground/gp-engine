// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "extensions/VulkanDeviceExtension.hpp"
#include "console/ConsoleManager.hpp"   // IWYU pragma: keep
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanDeviceExtensions.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "platform/VulkanPlatform.hpp"
#include "utils/VulkanResult.hpp"
#include "VulkanConfiguration.hpp"
#include <cstring>
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

gp::Vector<gp::SharedPtr<VulkanExternalDeviceExtensionBase>>& getExternalDeviceExtensionStorage() noexcept
{
    static gp::Vector<gp::SharedPtr<VulkanExternalDeviceExtensionBase>> externalExtensions;
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

void addSimpleDeviceExtension(
    VulkanDeviceExtensionArray& outExtensions,
    VulkanDevice* const device,
    const char* const extensionName,
    const gp::Int32 enabledInCode,
    const gp::UInt32 promotedVersion,
    VulkanDeviceExtension::FlagSetterFunc&& flagSetter = nullptr,
    const VulkanExtensionActivation activation = VulkanExtensionActivation::AutoActivate
) noexcept
{
    outExtensions.pushBack(
        gp::makeUnique<VulkanDeviceExtension>(
            device, extensionName, enabledInCode, promotedVersion, std::move(flagSetter), activation
        )
    );
}

}   // namespace

VulkanExternalDeviceExtensionWrapper::VulkanExternalDeviceExtensionWrapper(
    VulkanDevice* const inDevice, gp::SharedPtr<VulkanExternalDeviceExtensionBase> inExternalExtension
) noexcept
    : VulkanDeviceExtension(
          inDevice,
          (inExternalExtension != nullptr) ? inExternalExtension->getExtensionName().data() : "",
          kVulkanExtensionEnabled,
          (inExternalExtension != nullptr) ? inExternalExtension->getPromotedVersion() : kVulkanExtensionNotPromoted
      )
    , m_externalExtension(std::move(inExternalExtension))
{}

void VulkanExternalDeviceExtensionWrapper::prePhysicalDeviceProperties(
    VkPhysicalDeviceProperties2& physicalDeviceProperties2
) noexcept
{
    if (!m_requirementsPassed || m_externalExtension == nullptr)
    {
        return;
    }

    VkBaseOutStructure* const propertiesStruct = m_externalExtension->getPropertiesStruct();
    if (propertiesStruct != nullptr)
    {
        propertiesStruct->pNext = static_cast<VkBaseOutStructure*>(physicalDeviceProperties2.pNext);
        physicalDeviceProperties2.pNext = propertiesStruct;
    }
}

void VulkanExternalDeviceExtensionWrapper::postPhysicalDeviceProperties(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(extensionFlags);
    if (m_requirementsPassed && m_externalExtension != nullptr)
    {
        m_requirementsPassed = m_externalExtension->propertiesCallback();
    }
}

void VulkanExternalDeviceExtensionWrapper::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    if (!m_requirementsPassed || m_externalExtension == nullptr)
    {
        return;
    }

    VkBaseOutStructure* const featuresStruct = m_externalExtension->getFeaturesStruct();
    if (featuresStruct != nullptr)
    {
        featuresStruct->pNext = static_cast<VkBaseOutStructure*>(physicalDeviceFeatures2.pNext);
        physicalDeviceFeatures2.pNext = featuresStruct;
    }
}

void VulkanExternalDeviceExtensionWrapper::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(extensionFlags);
    if (m_requirementsPassed && m_externalExtension != nullptr)
    {
        m_requirementsPassed = m_externalExtension->featuresCallback();
    }
}

void VulkanExternalDeviceExtensionWrapper::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(extensionFlags);
    if (m_externalExtension != nullptr)
    {
        m_externalExtension->coreCallback(coreFeaturesAndProperties);
    }
}

void VulkanExternalDeviceExtensionWrapper::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (!m_requirementsPassed || m_externalExtension == nullptr)
    {
        return;
    }

    VkBaseOutStructure* const featuresStruct = m_externalExtension->getFeaturesStruct();
    if (featuresStruct != nullptr)
    {
        featuresStruct->pNext =
            const_cast<VkBaseOutStructure*>(static_cast<const VkBaseOutStructure*>(deviceCreateInfo.pNext));
        deviceCreateInfo.pNext = featuresStruct;
    }
}

gp::Vector<VkExtensionProperties> VulkanDeviceExtension::getDriverSupportedDeviceExtensions(
    const VkPhysicalDevice gpu, const char* const layerName
) noexcept
{
    gp::Vector<VkExtensionProperties> outExtensions;
    if (gpu == VK_NULL_HANDLE || dynamic_api::vkEnumerateDeviceExtensionProperties == nullptr)
    {
        return outExtensions;
    }

    gp::UInt32 count = 0u;
    VkResult result = vkEnumerateDeviceExtensionProperties(gpu, layerName, &count, nullptr);
    if (result == VK_ERROR_INITIALIZATION_FAILED)
    {
        GP_LOG_ERROR(
            LogVulkanRHI,
            "vkEnumerateDeviceExtensionProperties failed with VK_ERROR_INITIALIZATION_FAILED; verify that a Vulkan " "driver is installed."
        );
        return outExtensions;
    }
    if (result < VK_SUCCESS)
    {
        verifyVulkanResult(result, "vkEnumerateDeviceExtensionProperties", __FILE__, static_cast<gp::UInt32>(__LINE__));
        return outExtensions;
    }

    if (count > 0u)
    {
        outExtensions.resize(count);
        result = vkEnumerateDeviceExtensionProperties(gpu, layerName, &count, outExtensions.data());
        if (result == VK_ERROR_INITIALIZATION_FAILED)
        {
            GP_LOG_ERROR(
                LogVulkanRHI,
                "vkEnumerateDeviceExtensionProperties failed with VK_ERROR_INITIALIZATION_FAILED on second call."
            );
            outExtensions.clear();
            return outExtensions;
        }
        if (result < VK_SUCCESS)
        {
            verifyVulkanResult(
                result, "vkEnumerateDeviceExtensionProperties", __FILE__, static_cast<gp::UInt32>(__LINE__)
            );
            outExtensions.clear();
            return outExtensions;
        }
        outExtensions.resize(count);
        sortExtensionProperties(outExtensions);
    }

    return outExtensions;
}

VulkanDeviceExtensionArray VulkanDeviceExtension::getSupportedDeviceExtensions(
    VulkanDevice* const inDevice,
    const gp::UInt32 apiVersion,
    const gp::ConstVectorView<VkExtensionProperties> driverExtensions
) noexcept
{
    VulkanDeviceExtensionArray outExtensions;

#define GP_VK_DEVICE_EXT_FLAG(FlagName)                              \
    [](OptionalVulkanDeviceExtensions& flags) noexcept -> void       \
    {                                                                \
        flags.FlagName = 1u;                                         \
    }

    // Core swapchain, memory, and shader-model extensions
    addSimpleDeviceExtension(
        outExtensions, inDevice, VK_KHR_SWAPCHAIN_EXTENSION_NAME, kVulkanExtensionEnabled, kVulkanExtensionNotPromoted
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        GP_VULKAN_SUPPORTS_MEMORY_BUDGET,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasMemoryBudget)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
        GP_VULKAN_SUPPORTS_MEMORY_PRIORITY,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasMemoryPriority)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasDeferredHostOperations)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasSPIRV_14)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasShaderFloatControls)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasKHRImageFormatList)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_VALIDATION_CACHE_EXTENSION_NAME,
        GP_VULKAN_SUPPORTS_VALIDATION_CACHE,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasEXTValidationCache)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_4,
        GP_VK_DEVICE_EXT_FLAG(hasEXTLoadStoreOpNone)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_QCOM_RENDER_PASS_SHADER_RESOLVE_EXTENSION_NAME,
        GP_VULKAN_SUPPORTS_QCOM_RENDERPASS_SHADER_RESOLVE,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasQcomRenderPassShaderResolve)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasKHRPipelineLibrary)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_4,
        GP_VK_DEVICE_EXT_FLAG(hasKHRPushDescriptors)
    );

    // Resolved via the preprocessor: a constant ternary makes Clang's -Wunreachable-code fire.
#if GP_VULKAN_HAS_DEBUGGING_ENABLED || GP_VULKAN_ENABLE_DRAW_MARKERS
    constexpr auto kDebugExtensionPolicy = kVulkanExtensionEnabled;
#else
    constexpr auto kDebugExtensionPolicy = kVulkanExtensionDisabled;
#endif

    // Manually activated legacy debug marker fallback (used only when VK_EXT_debug_utils is unavailable)
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
        kDebugExtensionPolicy,
        kVulkanExtensionNotPromoted,
        nullptr,
        VulkanExtensionActivation::ManuallyActivate
    );

    // Core feature/property, synchronization, dynamic state, VRS, and ray tracing extensions.
    // Ordering invariants:
    //   1. VulkanKHRFragmentShadingRateExtension MUST precede VulkanEXTFragmentDensityMapExtension.
    //   2. VulkanKHRCalibratedTimestampsExtension MUST precede VulkanEXTCalibratedTimestampsExtension.
    //   3. VulkanKHR16BitStorageExtension MUST precede VulkanKHRShaderFloat16Int8Extension.
    outExtensions.pushBack(gp::makeUnique<VulkanKHRCreateRenderPass2Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTToolingInfoExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRDriverPropertiesExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRMaintenance4Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRMaintenance5Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRMaintenance6Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRMaintenance7Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRMaintenance9Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRSeparateDepthStencilLayoutsExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRSynchronization2Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRDynamicRenderingExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRTimelineSemaphoreExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTDescriptorIndexingExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTScalarBlockLayoutExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTHostQueryResetExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRBufferDeviceAddressExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRDepthStencilResolveExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRMultiviewExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHR16BitStorageExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRSamplerYcbcrConversionExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRShaderFloat16Int8Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRShaderAtomicInt64Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTShaderImageAtomicInt64Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTSubgroupSizeControlExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTPipelineCreationCacheControlExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTImageRobustnessExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTExtendedDynamicStateExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTExtendedDynamicState2Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTExtendedDynamicState3Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTVertexInputDynamicStateExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTTextureCompressionASTCHDRExtension>(inDevice));
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_ASTC_DECODE_MODE_EXTENSION_NAME,
        (GP_VULKAN_SUPPORTS_ASTC_DECODE_MODE && VulkanPlatform::supportsASTCTextureFormats())
            ? kVulkanExtensionEnabled
            : kVulkanExtensionDisabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasEXTASTCDecodeMode)
    );
    outExtensions.pushBack(gp::makeUnique<VulkanEXTShaderViewportIndexLayerExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRFragmentShadingRateExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTFragmentDensityMapExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTFragmentDensityMap2Extension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRFragmentShaderBarycentricExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRCalibratedTimestampsExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTCalibratedTimestampsExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanEXTMeshShaderExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRAccelerationStructureExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRRayTracingPipelineExtension>(inDevice));
    outExtensions.pushBack(gp::makeUnique<VulkanKHRRayQueryExtension>(inDevice));
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_AMD_BUFFER_MARKER_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasAMDBufferMarker)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasNVDiagnosticCheckpoints)
    );
    outExtensions.pushBack(gp::makeUnique<VulkanNVDeviceDiagnosticConfigExtension>(inDevice));

#undef GP_VK_DEVICE_EXT_FLAG

    // Platform-specific device extensions
    gp::Vector<const char*> platformExtensions;
    VulkanPlatform::getPlatformDeviceExtensionNames(platformExtensions);
    for (const char* const extName: platformExtensions)
    {
        if (extName == nullptr)
        {
            continue;
        }
        const gp::Int32 existingIndex = findExtension(outExtensions, extName);
        if (existingIndex < 0)
        {
            addSimpleDeviceExtension(
                outExtensions, inDevice, extName, kVulkanExtensionEnabled, kVulkanExtensionNotPromoted
            );
        }
        else
        {
            outExtensions[static_cast<gp::USize>(existingIndex)]->setActivated();
        }
    }

    // Merge externally registered device extensions
    gp::Vector<gp::SharedPtr<VulkanExternalDeviceExtensionBase>>& externalExtensions =
        getExternalDeviceExtensionStorage();
    for (gp::SharedPtr<VulkanExternalDeviceExtensionBase>& externalExt: externalExtensions)
    {
        if (externalExt == nullptr)
        {
            continue;
        }

        const gp::Int32 existingIndex = findExtension(outExtensions, externalExt->getExtensionName().data());
        if (existingIndex < 0)
        {
            GP_LOG_WARNING(
                LogVulkanRHI,
                "Adding externally requested device extension '{}'.",
                externalExt->getExtensionName().data()
            );
            outExtensions.pushBack(gp::makeUnique<VulkanExternalDeviceExtensionWrapper>(inDevice, externalExt));
        }
        else
        {
            outExtensions[static_cast<gp::USize>(existingIndex)]->setActivated();
            externalExt->duplicateCallback();
        }
    }
    externalExtensions.clear();

    const bool useCoreStructs = (readConsoleInt("r.Vulkan.UseCoreStructs", 0) != 0);
    flagExtensionSupport(driverExtensions, outExtensions, apiVersion, useCoreStructs, "device");

    return outExtensions;
}

VulkanDeviceExtensionArray VulkanDeviceExtension::getSupportedDeviceExtensions(
    VulkanDevice* const inDevice, const VkPhysicalDevice physicalDevice, const gp::UInt32 apiVersion
) noexcept
{
    const gp::Vector<VkExtensionProperties> driverExtensions =
        getDriverSupportedDeviceExtensions(physicalDevice, nullptr);
    return getSupportedDeviceExtensions(
        inDevice,
        apiVersion,
        gp::ConstVectorView<VkExtensionProperties>(driverExtensions.data(), driverExtensions.size())
    );
}

void VulkanDeviceExtension::addExternalDeviceExtensions(
    const gp::ConstVectorView<gp::SharedPtr<VulkanExternalDeviceExtensionBase>> extensions
) noexcept
{
    gp::Vector<gp::SharedPtr<VulkanExternalDeviceExtensionBase>>& storage = getExternalDeviceExtensionStorage();
    for (const gp::SharedPtr<VulkanExternalDeviceExtensionBase>& ext: extensions)
    {
        if (ext != nullptr)
        {
            storage.pushBack(ext);
        }
    }
}

void VulkanDeviceExtension::clearExternalDeviceExtensions() noexcept
{
    getExternalDeviceExtensionStorage().clear();
}

gp::ConstVectorView<gp::SharedPtr<VulkanExternalDeviceExtensionBase>>
    VulkanDeviceExtension::getExternalDeviceExtensions() noexcept
{
    const gp::Vector<gp::SharedPtr<VulkanExternalDeviceExtensionBase>>& storage = getExternalDeviceExtensionStorage();
    return gp::ConstVectorView<gp::SharedPtr<VulkanExternalDeviceExtensionBase>>(storage.data(), storage.size());
}

}   // namespace gp::rhi::vulkan
