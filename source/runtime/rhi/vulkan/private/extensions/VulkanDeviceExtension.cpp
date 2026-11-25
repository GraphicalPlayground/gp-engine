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

    // Manually activated legacy debug marker fallback (used only when VK_EXT_debug_utils is unavailable)
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
        (GP_VULKAN_HAS_DEBUGGING_ENABLED || GP_VULKAN_ENABLE_DRAW_MARKERS) ? kVulkanExtensionEnabled
                                                                           : kVulkanExtensionDisabled,
        kVulkanExtensionNotPromoted,
        nullptr,
        VulkanExtensionActivation::ManuallyActivate
    );

    // Core feature/property and synchronization extensions
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasKHRRenderPass2)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_TOOLING_INFO_EXTENSION_NAME,
        (GP_VULKAN_HAS_DEBUGGING_ENABLED || GP_VULKAN_ENABLE_DRAW_MARKERS) ? kVulkanExtensionEnabled
                                                                           : kVulkanExtensionDisabled,
        VK_API_VERSION_1_3,
        GP_VK_DEVICE_EXT_FLAG(hasEXTToolingInfo)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME,
        GP_VULKAN_SUPPORTS_DRIVER_PROPERTIES,
        VK_API_VERSION_1_2
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_3,
        GP_VK_DEVICE_EXT_FLAG(hasKHRMaintenance4)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_MAINTENANCE_5_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_4,
        GP_VK_DEVICE_EXT_FLAG(hasKHRMaintenance5)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_MAINTENANCE_6_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_4,
        GP_VK_DEVICE_EXT_FLAG(hasKHRMaintenance6)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasSeparateDepthStencilLayouts)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_3,
        GP_VK_DEVICE_EXT_FLAG(hasKHRSynchronization2)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_3,
        GP_VK_DEVICE_EXT_FLAG(hasKHRDynamicRendering)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasKHRTimelineSemaphore)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        GP_VULKAN_SUPPORTS_DESCRIPTOR_INDEXING,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasEXTDescriptorIndexing)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
        GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasEXTScalarBlockLayout)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasEXTHostQueryReset)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasBufferDeviceAddress)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasKHRDepthStencilResolve)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_MULTIVIEW_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_1,
        GP_VK_DEVICE_EXT_FLAG(hasKHRMultiview)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_1,
        GP_VK_DEVICE_EXT_FLAG(hasKHR16bitStorage)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_1,
        GP_VK_DEVICE_EXT_FLAG(hasKHRSamplerYcbcrConversion)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasKHRShaderFloat16)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_2,
        GP_VK_DEVICE_EXT_FLAG(hasKHRShaderAtomicInt64)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasImageAtomicInt64)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_3,
        GP_VK_DEVICE_EXT_FLAG(hasEXTSubgroupSizeControl)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_3,
        GP_VK_DEVICE_EXT_FLAG(hasEXTPipelineCreationCacheControl)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_3,
        GP_VK_DEVICE_EXT_FLAG(hasEXTImageRobustness)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_3,
        GP_VK_DEVICE_EXT_FLAG(hasEXTExtendedDynamicState1)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        VK_API_VERSION_1_3,
        GP_VK_DEVICE_EXT_FLAG(hasEXTExtendedDynamicState2)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasEXTExtendedDynamicState3)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasEXTVertexInputDynamicState)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME,
        (GP_VULKAN_SUPPORTS_TEXTURE_COMPRESSION_ASTC_HDR && VulkanPlatform::supportsASTCTextureFormats())
            ? kVulkanExtensionEnabled
            : kVulkanExtensionDisabled,
        VK_API_VERSION_1_3,
        GP_VK_DEVICE_EXT_FLAG(hasEXTTextureCompressionASTCHDR)
    );
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
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
        GP_VULKAN_SUPPORTS_SHADER_VIEWPORT_INDEX_LAYER,
        VK_API_VERSION_1_2
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasKHRFragmentShadingRate)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasEXTFragmentDensityMap)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasKHRCalibratedTimestamps)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasEXTCalibratedTimestamps)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_EXT_MESH_SHADER_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasEXTMeshShader)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasAccelerationStructure)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasRayTracingPipeline)
    );
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasRayQuery)
    );
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
    addSimpleDeviceExtension(
        outExtensions,
        inDevice,
        VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME,
        kVulkanExtensionEnabled,
        kVulkanExtensionNotPromoted,
        GP_VK_DEVICE_EXT_FLAG(hasNVDeviceDiagnosticConfig)
    );

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
