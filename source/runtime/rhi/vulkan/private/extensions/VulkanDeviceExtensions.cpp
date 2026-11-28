// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "extensions/VulkanDeviceExtensions.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "device/VulkanDevice.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "loader/VulkanEntryPoints.hpp"
#include "loader/VulkanLoader.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "platform/VulkanPlatform.hpp"
#include "VulkanCommon.hpp"
#include "VulkanConfiguration.hpp"

namespace gp::rhi::vulkan
{

namespace
{

gp::console::AutoConsoleVariable<gp::Int32> g_cvarAllow64BitShaderAtomics(
    "r.Vulkan.Allow64bitShaderAtomics",
    1,
    "Whether to enable 64-bit shader atomic device extensions when supported (0 = disable, 1 = enable).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarAllow16BitOps(
    "r.Vulkan.Allow16bitOps",
    1,
    "Whether to enable 16-bit storage and float16/int8 shader operations when supported (0 = disable, 1 = enable).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarAllowRayTracing(
    "r.Vulkan.RayTracing",
    1,
    "Whether to enable hardware ray tracing device extensions when supported (0 = disable, 1 = enable).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarAllowHostQueryReset(
    "r.Vulkan.AllowHostQueryReset",
    1,
    "Whether to enable VK_EXT_host_query_reset when supported (0 = disable, 1 = enable).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarAllowSynchronization2(
    "r.Vulkan.AllowSynchronization2",
    1,
    "Whether to enable VK_KHR_synchronization2 and separate depth-stencil layouts (0 = disable, 1 = enable).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarAllowDynamicStates(
    "r.Vulkan.AllowDynamicStates",
    15,
    "Bitmask controlling extended dynamic state extensions (1 = EDS1, 2 = EDS2, 4 = EDS3, 8 = VertexInputDynamicState).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

/// @brief Reads an existing integer console variable by name or returns `fallback` when unavailable.
/// @param[in] name Console variable name.
/// @param[in] fallback Default value returned when the console manager or variable is absent.
/// @return Resolved integer value.
[[nodiscard]] gp::Int32 readForeignConsoleInt(const gp::StringView name, const gp::Int32 fallback) noexcept
{
    if (gp::console::IConsoleManager::isGlobalInitialized())
    {
        const gp::console::IConsoleVariable* const variable =
            gp::console::IConsoleManager::get().findConsoleVariable(name);
        if (variable != nullptr)
        {
            return variable->getInt();
        }
    }
    return fallback;
}

/// @brief Reports whether ray tracing extensions are allowed by CVars and command-line flags.
/// @return True when `r.Vulkan.RayTracing != 0` and `-noraytracing` is not set.
[[nodiscard]] bool isRayTracingAllowed() noexcept
{
    const bool disabledByCli = gp::CommandLine::isInitialized() && gp::CommandLine::hasArgument("noraytracing");
    return !disabledByCli && (g_cvarAllowRayTracing.getValueOnAnyThread(true) != 0);
}

/// @brief Redirects an extension function pointer to its promoted core Vulkan entry point when an instance is active.
/// @tparam InPfnType Vulkan function pointer type.
/// @param[in] device Owning `VulkanDevice` pointer (nullable).
/// @param[in,out] outExtensionFunc Destination extension function pointer in `dynamic_api`.
/// @param[in] coreProcName Null-terminated name of the promoted core Vulkan entry point.
template <typename InPfnType>
void bindCoreFunctionPointer(
    const VulkanDevice* const device, InPfnType& outExtensionFunc, const char* const coreProcName
) noexcept
{
    if (device == nullptr)
    {
        return;
    }

    const VkInstance instance = device->getInstanceHandle();
    if (instance == VK_NULL_HANDLE)
    {
        return;
    }

    const PFN_vkVoidFunction resolved = VulkanLoader::getInstanceProcAddr(instance, coreProcName);
    if (resolved != nullptr)
    {
        outExtensionFunc = reinterpret_cast<InPfnType>(reinterpret_cast<gp::UIntPtr>(resolved));
    }
}

#if GP_VULKAN_HAS_DEBUGGING_ENABLED || GP_VULKAN_ENABLE_DRAW_MARKERS
constexpr gp::Int32 kToolingExtensionEnabled = kVulkanExtensionEnabled;
#else
constexpr gp::Int32 kToolingExtensionEnabled = kVulkanExtensionDisabled;
#endif

}   // namespace

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_create_renderpass2
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRCreateRenderPass2Extension::VulkanKHRCreateRenderPass2Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_2
      )
{}

void VulkanKHRCreateRenderPass2Extension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    extensionFlags.hasKHRRenderPass2 = 1u;
}

void VulkanKHRCreateRenderPass2Extension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(coreFeaturesAndProperties);
    extensionFlags.hasKHRRenderPass2 = 1u;
    bindCoreFunctionPointer(m_device, dynamic_api::vkCreateRenderPass2KHR, "vkCreateRenderPass2");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdBeginRenderPass2KHR, "vkCmdBeginRenderPass2");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdEndRenderPass2KHR, "vkCmdEndRenderPass2");
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_tooling_info
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTToolingInfoExtension::VulkanEXTToolingInfoExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_EXT_TOOLING_INFO_EXTENSION_NAME, kToolingExtensionEnabled, VK_API_VERSION_1_3)
{}

void VulkanEXTToolingInfoExtension::postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
{
    extensionFlags.hasEXTToolingInfo = 1u;
}

void VulkanEXTToolingInfoExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(coreFeaturesAndProperties);
    extensionFlags.hasEXTToolingInfo = 1u;
    bindCoreFunctionPointer(
        m_device, dynamic_api::vkGetPhysicalDeviceToolPropertiesEXT, "vkGetPhysicalDeviceToolProperties"
    );
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_driver_properties
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRDriverPropertiesExtension::VulkanKHRDriverPropertiesExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, GP_VULKAN_SUPPORTS_DRIVER_PROPERTIES, VK_API_VERSION_1_2
      )
    , m_driverProperties(
          zeroVulkanStruct<VkPhysicalDeviceDriverProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES)
      )
{}

void VulkanKHRDriverPropertiesExtension::prePhysicalDeviceProperties(
    VkPhysicalDeviceProperties2& physicalDeviceProperties2
) noexcept
{
    m_driverProperties =
        zeroVulkanStruct<VkPhysicalDeviceDriverProperties>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES);
    addToPNext(physicalDeviceProperties2, m_driverProperties);
}

void VulkanKHRDriverPropertiesExtension::postPhysicalDeviceProperties(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(extensionFlags);
    if (m_device != nullptr && m_driverProperties.driverID != VK_DRIVER_ID_MAX_ENUM &&
        m_driverProperties.driverID != static_cast<VkDriverId>(0))
    {
        m_device->getOptionalExtensionPropertiesMutable().driverId = m_driverProperties.driverID;
    }
}

void VulkanKHRDriverPropertiesExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(extensionFlags);
    if (m_device != nullptr && coreFeaturesAndProperties.properties12 != nullptr &&
        coreFeaturesAndProperties.properties12->driverID != static_cast<VkDriverId>(0))
    {
        m_device->getOptionalExtensionPropertiesMutable().driverId = coreFeaturesAndProperties.properties12->driverID;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_maintenance4
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRMaintenance4Extension::VulkanKHRMaintenance4Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_MAINTENANCE_4_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_3)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceMaintenance4Features>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES
          )
      )
    , m_properties(
          zeroVulkanStruct<VkPhysicalDeviceMaintenance4Properties>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES
          )
      )
{}

void VulkanKHRMaintenance4Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceMaintenance4Features>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRMaintenance4Extension::postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
{
    m_requirementsPassed = (m_features.maintenance4 == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRMaintenance4 = 1u;
    }
}

void VulkanKHRMaintenance4Extension::prePhysicalDeviceProperties(
    VkPhysicalDeviceProperties2& physicalDeviceProperties2
) noexcept
{
    if (m_requirementsPassed)
    {
        m_properties = zeroVulkanStruct<VkPhysicalDeviceMaintenance4Properties>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES
        );
        addToPNext(physicalDeviceProperties2, m_properties);
    }
}

void VulkanKHRMaintenance4Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRMaintenance4Extension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features13 != nullptr) &&
                           (coreFeaturesAndProperties.features13->maintenance4 == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHRMaintenance4 = 1u;
        bindCoreFunctionPointer(
            m_device, dynamic_api::vkGetDeviceImageMemoryRequirementsKHR, "vkGetDeviceImageMemoryRequirements"
        );
        bindCoreFunctionPointer(
            m_device, dynamic_api::vkGetDeviceBufferMemoryRequirementsKHR, "vkGetDeviceBufferMemoryRequirements"
        );
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_maintenance5
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRMaintenance5Extension::VulkanKHRMaintenance5Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_MAINTENANCE_5_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_4)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceMaintenance5FeaturesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR
          )
      )
{}

void VulkanKHRMaintenance5Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceMaintenance5FeaturesKHR>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRMaintenance5Extension::postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
{
    m_requirementsPassed = (m_features.maintenance5 == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRMaintenance5 = 1u;
    }
}

void VulkanKHRMaintenance5Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRMaintenance5Extension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features14 != nullptr) &&
                           (coreFeaturesAndProperties.features14->maintenance5 == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHRMaintenance5 = 1u;
        bindCoreFunctionPointer(m_device, dynamic_api::vkGetImageSubresourceLayout2KHR, "vkGetImageSubresourceLayout2");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_maintenance6
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRMaintenance6Extension::VulkanKHRMaintenance6Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_MAINTENANCE_6_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_4)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceMaintenance6FeaturesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR
          )
      )
{}

void VulkanKHRMaintenance6Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceMaintenance6FeaturesKHR>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRMaintenance6Extension::postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
{
    m_requirementsPassed = (m_features.maintenance6 == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRMaintenance6 = 1u;
    }
}

void VulkanKHRMaintenance6Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRMaintenance6Extension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features14 != nullptr) &&
                           (coreFeaturesAndProperties.features14->maintenance6 == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHRMaintenance6 = 1u;
        bindCoreFunctionPointer(m_device, dynamic_api::vkCmdPushDescriptorSet2KHR, "vkCmdPushDescriptorSet2");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_maintenance7
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRMaintenance7Extension::VulkanKHRMaintenance7Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_MAINTENANCE_7_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceMaintenance7FeaturesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_7_FEATURES_KHR
          )
      )
{}

void VulkanKHRMaintenance7Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceMaintenance7FeaturesKHR>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_7_FEATURES_KHR
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRMaintenance7Extension::postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
{
    m_requirementsPassed = (m_features.maintenance7 == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRMaintenance7 = 1u;
    }
}

void VulkanKHRMaintenance7Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_maintenance9
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRMaintenance9Extension::VulkanKHRMaintenance9Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_MAINTENANCE_9_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceMaintenance9FeaturesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_9_FEATURES_KHR
          )
      )
{}

void VulkanKHRMaintenance9Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceMaintenance9FeaturesKHR>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_9_FEATURES_KHR
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRMaintenance9Extension::postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
{
    m_requirementsPassed = (m_features.maintenance9 == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRMaintenance9 = 1u;
    }
}

void VulkanKHRMaintenance9Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_shader_atomic_int64
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRShaderAtomicInt64Extension::VulkanKHRShaderAtomicInt64Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_2
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceShaderAtomicInt64Features>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES
          )
      )
{
    m_enabledInCode = m_enabledInCode && (g_cvarAllow64BitShaderAtomics.getValueOnAnyThread(true) != 0);
}

void VulkanKHRShaderAtomicInt64Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceShaderAtomicInt64Features>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRShaderAtomicInt64Extension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.shaderBufferInt64Atomics == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRShaderAtomicInt64 = 1u;
    }
}

void VulkanKHRShaderAtomicInt64Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRShaderAtomicInt64Extension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features12 != nullptr) &&
                           (coreFeaturesAndProperties.features12->shaderBufferInt64Atomics == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHRShaderAtomicInt64 = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_shader_image_atomic_int64
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTShaderImageAtomicInt64Extension::VulkanEXTShaderImageAtomicInt64Extension(
    VulkanDevice* const inDevice
) noexcept
    : VulkanDeviceExtension(inDevice, VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT
          )
      )
{
    m_enabledInCode = m_enabledInCode && (g_cvarAllow64BitShaderAtomics.getValueOnAnyThread(true) != 0);
}

void VulkanEXTShaderImageAtomicInt64Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTShaderImageAtomicInt64Extension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.shaderImageInt64Atomics == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasImageAtomicInt64 = 1u;
    }
}

void VulkanEXTShaderImageAtomicInt64Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_scalar_block_layout
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTScalarBlockLayoutExtension::VulkanEXTScalarBlockLayoutExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice,
          VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
          GP_VULKAN_SUPPORTS_SCALAR_BLOCK_LAYOUT,
          VK_API_VERSION_1_2
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceScalarBlockLayoutFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES
          )
      )
{}

void VulkanEXTScalarBlockLayoutExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceScalarBlockLayoutFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTScalarBlockLayoutExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.scalarBlockLayout == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTScalarBlockLayout = 1u;
    }
}

void VulkanEXTScalarBlockLayoutExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanEXTScalarBlockLayoutExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features12 != nullptr) &&
                           (coreFeaturesAndProperties.features12->scalarBlockLayout == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasEXTScalarBlockLayout = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_shader_viewport_index_layer
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTShaderViewportIndexLayerExtension::VulkanEXTShaderViewportIndexLayerExtension(
    VulkanDevice* const inDevice
) noexcept
    : VulkanDeviceExtension(
          inDevice,
          VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME,
          GP_VULKAN_SUPPORTS_SHADER_VIEWPORT_INDEX_LAYER,
          VK_API_VERSION_1_2
      )
{}

void VulkanEXTShaderViewportIndexLayerExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(extensionFlags);
    m_requirementsPassed = (coreFeaturesAndProperties.features12 != nullptr) &&
                           (coreFeaturesAndProperties.features12->shaderOutputLayer == VK_TRUE) &&
                           (coreFeaturesAndProperties.features12->shaderOutputViewportIndex == VK_TRUE);
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_separate_depth_stencil_layouts
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRSeparateDepthStencilLayoutsExtension::VulkanKHRSeparateDepthStencilLayoutsExtension(
    VulkanDevice* const inDevice
) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_2
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES
          )
      )
{
    m_enabledInCode = m_enabledInCode && (g_cvarAllowSynchronization2.getValueOnAnyThread(true) != 0);
}

void VulkanKHRSeparateDepthStencilLayoutsExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRSeparateDepthStencilLayoutsExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.separateDepthStencilLayouts == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasSeparateDepthStencilLayouts = 1u;
    }
}

void VulkanKHRSeparateDepthStencilLayoutsExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRSeparateDepthStencilLayoutsExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features12 != nullptr) &&
                           (coreFeaturesAndProperties.features12->separateDepthStencilLayouts == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasSeparateDepthStencilLayouts = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_synchronization2
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRSynchronization2Extension::VulkanKHRSynchronization2Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_3
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceSynchronization2Features>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES
          )
      )
{
    m_enabledInCode = m_enabledInCode && (g_cvarAllowSynchronization2.getValueOnAnyThread(true) != 0);
}

void VulkanKHRSynchronization2Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceSynchronization2Features>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRSynchronization2Extension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.synchronization2 == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRSynchronization2 = 1u;
    }
}

void VulkanKHRSynchronization2Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRSynchronization2Extension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features13 != nullptr) &&
                           (coreFeaturesAndProperties.features13->synchronization2 == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHRSynchronization2 = 1u;
        bindCoreFunctionPointer(m_device, dynamic_api::vkCmdPipelineBarrier2KHR, "vkCmdPipelineBarrier2");
        bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetEvent2KHR, "vkCmdSetEvent2");
        bindCoreFunctionPointer(m_device, dynamic_api::vkCmdResetEvent2KHR, "vkCmdResetEvent2");
        bindCoreFunctionPointer(m_device, dynamic_api::vkCmdWaitEvents2KHR, "vkCmdWaitEvents2");
        bindCoreFunctionPointer(m_device, dynamic_api::vkQueueSubmit2KHR, "vkQueueSubmit2");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_multiview
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRMultiviewExtension::VulkanKHRMultiviewExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_MULTIVIEW_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_1)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceMultiviewFeatures>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES)
      )
{}

void VulkanKHRMultiviewExtension::prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept
{
    m_features =
        zeroVulkanStruct<VkPhysicalDeviceMultiviewFeatures>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES);
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRMultiviewExtension::postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
{
    m_requirementsPassed = (m_features.multiview == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRMultiview = 1u;
    }
}

void VulkanKHRMultiviewExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRMultiviewExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features11 != nullptr) &&
                           (coreFeaturesAndProperties.features11->multiview == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHRMultiview = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_descriptor_indexing
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTDescriptorIndexingExtension::VulkanEXTDescriptorIndexingExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice,
          VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
          GP_VULKAN_SUPPORTS_DESCRIPTOR_INDEXING,
          VK_API_VERSION_1_2
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceDescriptorIndexingFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES
          )
      )
{}

void VulkanEXTDescriptorIndexingExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceDescriptorIndexingFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTDescriptorIndexingExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.runtimeDescriptorArray == VK_TRUE) &&
                           (m_features.descriptorBindingPartiallyBound == VK_TRUE) &&
                           (m_features.descriptorBindingUpdateUnusedWhilePending == VK_TRUE) &&
                           (m_features.descriptorBindingVariableDescriptorCount == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTDescriptorIndexing = 1u;
    }
}

void VulkanEXTDescriptorIndexingExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanEXTDescriptorIndexingExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported =
        (coreFeaturesAndProperties.features12 != nullptr) &&
        (coreFeaturesAndProperties.features12->runtimeDescriptorArray == VK_TRUE) &&
        (coreFeaturesAndProperties.features12->descriptorBindingPartiallyBound == VK_TRUE) &&
        (coreFeaturesAndProperties.features12->descriptorBindingUpdateUnusedWhilePending == VK_TRUE) &&
        (coreFeaturesAndProperties.features12->descriptorBindingVariableDescriptorCount == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasEXTDescriptorIndexing = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_fragment_shading_rate
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRFragmentShadingRateExtension::VulkanKHRFragmentShadingRateExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_fallbackFeatures(
          zeroVulkanStruct<VkPhysicalDeviceFragmentShadingRateFeaturesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR
          )
      )
    , m_properties(
          zeroVulkanStruct<VkPhysicalDeviceFragmentShadingRatePropertiesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR
          )
      )
{}

VkPhysicalDeviceFragmentShadingRateFeaturesKHR& VulkanKHRFragmentShadingRateExtension::resolveFeatures() noexcept
{
    if (m_device != nullptr)
    {
        return m_device->getOptionalExtensionPropertiesMutable().fragmentShadingRateFeatures;
    }
    return m_fallbackFeatures;
}

void VulkanKHRFragmentShadingRateExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR& features = resolveFeatures();
    features = zeroVulkanStruct<VkPhysicalDeviceFragmentShadingRateFeaturesKHR>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR
    );
    addToPNext(physicalDeviceFeatures2, features);
}

void VulkanKHRFragmentShadingRateExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const VkPhysicalDeviceFragmentShadingRateFeaturesKHR& features = resolveFeatures();
    m_requirementsPassed =
        (features.attachmentFragmentShadingRate == VK_TRUE) || (features.pipelineFragmentShadingRate == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRFragmentShadingRate = 1u;
    }
}

void VulkanKHRFragmentShadingRateExtension::prePhysicalDeviceProperties(
    VkPhysicalDeviceProperties2& physicalDeviceProperties2
) noexcept
{
    if (m_requirementsPassed)
    {
        m_properties = zeroVulkanStruct<VkPhysicalDeviceFragmentShadingRatePropertiesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR
        );
        addToPNext(physicalDeviceProperties2, m_properties);
    }
}

void VulkanKHRFragmentShadingRateExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (!m_requirementsPassed)
    {
        return;
    }

    VkPhysicalDeviceFragmentShadingRateFeaturesKHR& features = resolveFeatures();
    const bool stillEnabledOnDevice =
        (m_device == nullptr) || (m_device->getOptionalExtensions().hasKHRFragmentShadingRate != 0u);
    if (stillEnabledOnDevice &&
        ((features.attachmentFragmentShadingRate == VK_TRUE) || (features.pipelineFragmentShadingRate == VK_TRUE)))
    {
        addToPNext(deviceCreateInfo, features);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_fragment_density_map
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTFragmentDensityMapExtension::VulkanEXTFragmentDensityMapExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_fallbackFeatures(
          zeroVulkanStruct<VkPhysicalDeviceFragmentDensityMapFeaturesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT
          )
      )
    , m_properties(
          zeroVulkanStruct<VkPhysicalDeviceFragmentDensityMapPropertiesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT
          )
      )
{}

VkPhysicalDeviceFragmentDensityMapFeaturesEXT& VulkanEXTFragmentDensityMapExtension::resolveFeatures() noexcept
{
    if (m_device != nullptr)
    {
        return m_device->getOptionalExtensionPropertiesMutable().fragmentDensityMapFeatures;
    }
    return m_fallbackFeatures;
}

void VulkanEXTFragmentDensityMapExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT& features = resolveFeatures();
    features = zeroVulkanStruct<VkPhysicalDeviceFragmentDensityMapFeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT
    );
    addToPNext(physicalDeviceFeatures2, features);
}

void VulkanEXTFragmentDensityMapExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const VkPhysicalDeviceFragmentDensityMapFeaturesEXT& features = resolveFeatures();
    m_requirementsPassed = (features.fragmentDensityMap == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTFragmentDensityMap = 1u;
    }
}

void VulkanEXTFragmentDensityMapExtension::prePhysicalDeviceProperties(
    VkPhysicalDeviceProperties2& physicalDeviceProperties2
) noexcept
{
    if (m_requirementsPassed)
    {
        m_properties = zeroVulkanStruct<VkPhysicalDeviceFragmentDensityMapPropertiesEXT>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT
        );
        addToPNext(physicalDeviceProperties2, m_properties);
    }
}

void VulkanEXTFragmentDensityMapExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (!m_requirementsPassed)
    {
        return;
    }

    VkPhysicalDeviceFragmentDensityMapFeaturesEXT& features = resolveFeatures();
    const bool stillEnabledOnDevice =
        (m_device == nullptr) || (m_device->getOptionalExtensions().hasEXTFragmentDensityMap != 0u);
    if (stillEnabledOnDevice && (features.fragmentDensityMap == VK_TRUE))
    {
        addToPNext(deviceCreateInfo, features);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_fragment_density_map2
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTFragmentDensityMap2Extension::VulkanEXTFragmentDensityMap2Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_EXT_FRAGMENT_DENSITY_MAP_2_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_fallbackFeatures(
          zeroVulkanStruct<VkPhysicalDeviceFragmentDensityMap2FeaturesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT
          )
      )
{}

VkPhysicalDeviceFragmentDensityMap2FeaturesEXT& VulkanEXTFragmentDensityMap2Extension::resolveFeatures() noexcept
{
    if (m_device != nullptr)
    {
        return m_device->getOptionalExtensionPropertiesMutable().fragmentDensityMap2Features;
    }
    return m_fallbackFeatures;
}

void VulkanEXTFragmentDensityMap2Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    VkPhysicalDeviceFragmentDensityMap2FeaturesEXT& features = resolveFeatures();
    features = zeroVulkanStruct<VkPhysicalDeviceFragmentDensityMap2FeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT
    );
    addToPNext(physicalDeviceFeatures2, features);
}

void VulkanEXTFragmentDensityMap2Extension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const VkPhysicalDeviceFragmentDensityMap2FeaturesEXT& features = resolveFeatures();
    m_requirementsPassed = (features.fragmentDensityMapDeferred == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTFragmentDensityMap2 = 1u;
    }
}

void VulkanEXTFragmentDensityMap2Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        VkPhysicalDeviceFragmentDensityMap2FeaturesEXT& features = resolveFeatures();
        if (features.fragmentDensityMapDeferred == VK_TRUE)
        {
            addToPNext(deviceCreateInfo, features);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_fragment_shader_barycentric
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRFragmentShaderBarycentricExtension::VulkanKHRFragmentShaderBarycentricExtension(
    VulkanDevice* const inDevice
) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR
          )
      )
    , m_properties(
          zeroVulkanStruct<VkPhysicalDeviceFragmentShaderBarycentricPropertiesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_PROPERTIES_KHR
          )
      )
{}

void VulkanKHRFragmentShaderBarycentricExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRFragmentShaderBarycentricExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.fragmentShaderBarycentric == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRFragmentShaderBarycentric = 1u;
    }
}

void VulkanKHRFragmentShaderBarycentricExtension::prePhysicalDeviceProperties(
    VkPhysicalDeviceProperties2& physicalDeviceProperties2
) noexcept
{
    if (m_requirementsPassed)
    {
        m_properties = zeroVulkanStruct<VkPhysicalDeviceFragmentShaderBarycentricPropertiesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_PROPERTIES_KHR
        );
        addToPNext(physicalDeviceProperties2, m_properties);
    }
}

void VulkanKHRFragmentShaderBarycentricExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_host_query_reset
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTHostQueryResetExtension::VulkanEXTHostQueryResetExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_2
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceHostQueryResetFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES
          )
      )
{
    m_enabledInCode = m_enabledInCode && (g_cvarAllowHostQueryReset.getValueOnAnyThread(true) != 0);
}

void VulkanEXTHostQueryResetExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceHostQueryResetFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTHostQueryResetExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.hostQueryReset == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTHostQueryReset = 1u;
    }
}

void VulkanEXTHostQueryResetExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanEXTHostQueryResetExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features12 != nullptr) &&
                           (coreFeaturesAndProperties.features12->hostQueryReset == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasEXTHostQueryReset = 1u;
        bindCoreFunctionPointer(m_device, dynamic_api::vkResetQueryPoolEXT, "vkResetQueryPool");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_subgroup_size_control
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTSubgroupSizeControlExtension::VulkanEXTSubgroupSizeControlExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_3
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceSubgroupSizeControlFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES
          )
      )
    , m_fallbackProperties(
          zeroVulkanStruct<VkPhysicalDeviceSubgroupSizeControlPropertiesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES
          )
      )
{}

VkPhysicalDeviceSubgroupSizeControlPropertiesEXT& VulkanEXTSubgroupSizeControlExtension::resolveProperties() noexcept
{
    if (m_device != nullptr)
    {
        return m_device->getOptionalExtensionPropertiesMutable().subgroupSizeControlProperties;
    }
    return m_fallbackProperties;
}

void VulkanEXTSubgroupSizeControlExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceSubgroupSizeControlFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTSubgroupSizeControlExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.subgroupSizeControl == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTSubgroupSizeControl = 1u;
    }
}

void VulkanEXTSubgroupSizeControlExtension::prePhysicalDeviceProperties(
    VkPhysicalDeviceProperties2& physicalDeviceProperties2
) noexcept
{
    if (m_requirementsPassed)
    {
        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT& props = resolveProperties();
        props = zeroVulkanStruct<VkPhysicalDeviceSubgroupSizeControlPropertiesEXT>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES
        );
        addToPNext(physicalDeviceProperties2, props);
    }
}

void VulkanEXTSubgroupSizeControlExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanEXTSubgroupSizeControlExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features13 != nullptr) &&
                           (coreFeaturesAndProperties.features13->subgroupSizeControl == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasEXTSubgroupSizeControl = 1u;
        if (coreFeaturesAndProperties.properties13 != nullptr)
        {
            VkPhysicalDeviceSubgroupSizeControlPropertiesEXT& props = resolveProperties();
            props.minSubgroupSize = coreFeaturesAndProperties.properties13->minSubgroupSize;
            props.maxSubgroupSize = coreFeaturesAndProperties.properties13->maxSubgroupSize;
            props.maxComputeWorkgroupSubgroups = coreFeaturesAndProperties.properties13->maxComputeWorkgroupSubgroups;
            props.requiredSubgroupSizeStages = coreFeaturesAndProperties.properties13->requiredSubgroupSizeStages;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_calibrated_timestamps & VK_EXT_calibrated_timestamps
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRCalibratedTimestampsExtension::VulkanKHRCalibratedTimestampsExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME, kVulkanExtensionEnabled)
{}

void VulkanKHRCalibratedTimestampsExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    if (m_device == nullptr || m_device->getPhysicalHandle() == VK_NULL_HANDLE ||
        dynamic_api::vkGetPhysicalDeviceCalibrateableTimeDomainsKHR == nullptr)
    {
        extensionFlags.hasKHRCalibratedTimestamps = 1u;
        return;
    }

    gp::UInt32 domainCount = 0u;
    const VkPhysicalDevice gpu = m_device->getPhysicalHandle();
    if (vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(gpu, &domainCount, nullptr) != VK_SUCCESS || domainCount == 0u)
    {
        m_requirementsPassed = false;
        return;
    }

    gp::Vector<VkTimeDomainKHR> domains(domainCount);
    if (vkGetPhysicalDeviceCalibrateableTimeDomainsKHR(gpu, &domainCount, domains.data()) != VK_SUCCESS)
    {
        m_requirementsPassed = false;
        return;
    }

    bool hasDeviceDomain = false;
    for (gp::UInt32 i = 0u; i < domainCount; ++i)
    {
        if (domains[i] == VK_TIME_DOMAIN_DEVICE_KHR)
        {
            hasDeviceDomain = true;
            break;
        }
    }

    m_requirementsPassed = hasDeviceDomain;
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRCalibratedTimestamps = 1u;
    }
}

VulkanEXTCalibratedTimestampsExtension::VulkanEXTCalibratedTimestampsExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME, kVulkanExtensionEnabled)
{}

void VulkanEXTCalibratedTimestampsExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    if (extensionFlags.hasKHRCalibratedTimestamps != 0u)
    {
        m_requirementsPassed = false;
        m_supported = false;
        return;
    }

    if (m_device == nullptr || m_device->getPhysicalHandle() == VK_NULL_HANDLE ||
        dynamic_api::vkGetPhysicalDeviceCalibrateableTimeDomainsEXT == nullptr)
    {
        extensionFlags.hasEXTCalibratedTimestamps = 1u;
        return;
    }

    gp::UInt32 domainCount = 0u;
    const VkPhysicalDevice gpu = m_device->getPhysicalHandle();
    if (vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(gpu, &domainCount, nullptr) != VK_SUCCESS || domainCount == 0u)
    {
        m_requirementsPassed = false;
        return;
    }

    gp::Vector<VkTimeDomainEXT> domains(domainCount);
    if (vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(gpu, &domainCount, domains.data()) != VK_SUCCESS)
    {
        m_requirementsPassed = false;
        return;
    }

    bool hasDeviceDomain = false;
    for (gp::UInt32 i = 0u; i < domainCount; ++i)
    {
        if (domains[i] == VK_TIME_DOMAIN_DEVICE_EXT)
        {
            hasDeviceDomain = true;
            break;
        }
    }

    m_requirementsPassed = hasDeviceDomain;
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTCalibratedTimestamps = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_16bit_storage & VK_KHR_shader_float16_int8
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHR16BitStorageExtension::VulkanKHR16BitStorageExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_16BIT_STORAGE_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_1)
    , m_features(
          zeroVulkanStruct<VkPhysicalDevice16BitStorageFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES
          )
      )
{
    m_enabledInCode = m_enabledInCode && (g_cvarAllow16BitOps.getValueOnAnyThread(true) != 0);
}

void VulkanKHR16BitStorageExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDevice16BitStorageFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHR16BitStorageExtension::postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
{
    m_requirementsPassed = (m_features.storageBuffer16BitAccess == VK_TRUE) &&
                           (m_features.uniformAndStorageBuffer16BitAccess == VK_TRUE) &&
                           (m_features.storagePushConstant16 == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHR16bitStorage = 1u;
    }
}

void VulkanKHR16BitStorageExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHR16BitStorageExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features11 != nullptr) &&
                           (coreFeaturesAndProperties.features11->storageBuffer16BitAccess == VK_TRUE) &&
                           (coreFeaturesAndProperties.features11->uniformAndStorageBuffer16BitAccess == VK_TRUE) &&
                           (coreFeaturesAndProperties.features11->storagePushConstant16 == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHR16bitStorage = 1u;
    }
}

VulkanKHRShaderFloat16Int8Extension::VulkanKHRShaderFloat16Int8Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_2
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceShaderFloat16Int8Features>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES
          )
      )
{
    m_enabledInCode = m_enabledInCode && (g_cvarAllow16BitOps.getValueOnAnyThread(true) != 0);
}

void VulkanKHRShaderFloat16Int8Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceShaderFloat16Int8Features>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRShaderFloat16Int8Extension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.shaderFloat16 == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRShaderFloat16 = 1u;
    }
}

void VulkanKHRShaderFloat16Int8Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRShaderFloat16Int8Extension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features12 != nullptr) &&
                           (coreFeaturesAndProperties.features12->shaderFloat16 == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHRShaderFloat16 = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_depth_stencil_resolve
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRDepthStencilResolveExtension::VulkanKHRDepthStencilResolveExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_2
      )
{}

void VulkanKHRDepthStencilResolveExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    extensionFlags.hasKHRDepthStencilResolve = 1u;
}

void VulkanKHRDepthStencilResolveExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(coreFeaturesAndProperties);
    extensionFlags.hasKHRDepthStencilResolve = 1u;
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_image_robustness
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTImageRobustnessExtension::VulkanEXTImageRobustnessExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_EXT_IMAGE_ROBUSTNESS_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_3
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceImageRobustnessFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES
          )
      )
{
    m_enabledInCode = m_enabledInCode && (readForeignConsoleInt("r.Vulkan.AllowImageRobustness", 1) != 0);
}

void VulkanEXTImageRobustnessExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceImageRobustnessFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTImageRobustnessExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.robustImageAccess == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTImageRobustness = 1u;
    }
}

void VulkanEXTImageRobustnessExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanEXTImageRobustnessExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features13 != nullptr) &&
                           (coreFeaturesAndProperties.features13->robustImageAccess == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasEXTImageRobustness = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_pipeline_creation_cache_control
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTPipelineCreationCacheControlExtension::VulkanEXTPipelineCreationCacheControlExtension(
    VulkanDevice* const inDevice
) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_3
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDevicePipelineCreationCacheControlFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES
          )
      )
{}

void VulkanEXTPipelineCreationCacheControlExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDevicePipelineCreationCacheControlFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTPipelineCreationCacheControlExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.pipelineCreationCacheControl == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTPipelineCreationCacheControl = 1u;
    }
}

void VulkanEXTPipelineCreationCacheControlExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanEXTPipelineCreationCacheControlExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features13 != nullptr) &&
                           (coreFeaturesAndProperties.features13->pipelineCreationCacheControl == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasEXTPipelineCreationCacheControl = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_sampler_ycbcr_conversion
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRSamplerYcbcrConversionExtension::VulkanKHRSamplerYcbcrConversionExtension(
    VulkanDevice* const inDevice
) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_1
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceSamplerYcbcrConversionFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES
          )
      )
{}

void VulkanKHRSamplerYcbcrConversionExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceSamplerYcbcrConversionFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRSamplerYcbcrConversionExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.samplerYcbcrConversion == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRSamplerYcbcrConversion = 1u;
    }
}

void VulkanKHRSamplerYcbcrConversionExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRSamplerYcbcrConversionExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features11 != nullptr) &&
                           (coreFeaturesAndProperties.features11->samplerYcbcrConversion == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHRSamplerYcbcrConversion = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_timeline_semaphore
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRTimelineSemaphoreExtension::VulkanKHRTimelineSemaphoreExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_2
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceTimelineSemaphoreFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES
          )
      )
{}

void VulkanKHRTimelineSemaphoreExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceTimelineSemaphoreFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRTimelineSemaphoreExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.timelineSemaphore == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRTimelineSemaphore = 1u;
    }
}

void VulkanKHRTimelineSemaphoreExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRTimelineSemaphoreExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features12 != nullptr) &&
                           (coreFeaturesAndProperties.features12->timelineSemaphore == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHRTimelineSemaphore = 1u;
        bindCoreFunctionPointer(m_device, dynamic_api::vkGetSemaphoreCounterValueKHR, "vkGetSemaphoreCounterValue");
        bindCoreFunctionPointer(m_device, dynamic_api::vkWaitSemaphoresKHR, "vkWaitSemaphores");
        bindCoreFunctionPointer(m_device, dynamic_api::vkSignalSemaphoreKHR, "vkSignalSemaphore");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_mesh_shader
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTMeshShaderExtension::VulkanEXTMeshShaderExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_EXT_MESH_SHADER_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceMeshShaderFeaturesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
          )
      )
    , m_fallbackProperties(
          zeroVulkanStruct<VkPhysicalDeviceMeshShaderPropertiesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT
          )
      )
{}

VkPhysicalDeviceMeshShaderPropertiesEXT& VulkanEXTMeshShaderExtension::resolveProperties() noexcept
{
    if (m_device != nullptr)
    {
        return m_device->getOptionalExtensionPropertiesMutable().meshShaderProperties;
    }
    return m_fallbackProperties;
}

void VulkanEXTMeshShaderExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceMeshShaderFeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTMeshShaderExtension::postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
{
    m_requirementsPassed = (m_features.meshShader == VK_TRUE) && (m_features.multiviewMeshShader == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTMeshShader = 1u;
    }
}

void VulkanEXTMeshShaderExtension::prePhysicalDeviceProperties(
    VkPhysicalDeviceProperties2& physicalDeviceProperties2
) noexcept
{
    if (m_requirementsPassed)
    {
        VkPhysicalDeviceMeshShaderPropertiesEXT& props = resolveProperties();
        props = zeroVulkanStruct<VkPhysicalDeviceMeshShaderPropertiesEXT>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT
        );
        addToPNext(physicalDeviceProperties2, props);
    }
}

void VulkanEXTMeshShaderExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (!m_requirementsPassed)
    {
        return;
    }

    const bool hasPrimitiveFsr =
        (m_device != nullptr) && (m_device->getOptionalExtensions().hasKHRFragmentShadingRate != 0u) &&
        (m_device->getOptionalExtensionProperties().fragmentShadingRateFeatures.primitiveFragmentShadingRate ==
         VK_TRUE);
    if (!hasPrimitiveFsr)
    {
        m_features.primitiveFragmentShadingRateMeshShader = VK_FALSE;
    }
    m_features.meshShaderQueries = VK_FALSE;

    addToPNext(deviceCreateInfo, m_features);
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_dynamic_rendering
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRDynamicRenderingExtension::VulkanKHRDynamicRenderingExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_3
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceDynamicRenderingFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES
          )
      )
{}

void VulkanKHRDynamicRenderingExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceDynamicRenderingFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRDynamicRenderingExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.dynamicRendering == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasKHRDynamicRendering = 1u;
    }
}

void VulkanKHRDynamicRenderingExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRDynamicRenderingExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features13 != nullptr) &&
                           (coreFeaturesAndProperties.features13->dynamicRendering == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasKHRDynamicRendering = 1u;
        bindCoreFunctionPointer(m_device, dynamic_api::vkCmdBeginRenderingKHR, "vkCmdBeginRendering");
        bindCoreFunctionPointer(m_device, dynamic_api::vkCmdEndRenderingKHR, "vkCmdEndRendering");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_extended_dynamic_state (1, 2, 3) & VK_EXT_vertex_input_dynamic_state
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTExtendedDynamicStateExtension::VulkanEXTExtendedDynamicStateExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_3
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT
          )
      )
{
    m_enabledInCode = m_enabledInCode && ((g_cvarAllowDynamicStates.getValueOnAnyThread(true) & 1) != 0);
}

void VulkanEXTExtendedDynamicStateExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTExtendedDynamicStateExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.extendedDynamicState == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTExtendedDynamicState1 = 1u;
    }
}

void VulkanEXTExtendedDynamicStateExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanEXTExtendedDynamicStateExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(coreFeaturesAndProperties);
    m_requirementsPassed = true;
    extensionFlags.hasEXTExtendedDynamicState1 = 1u;

    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdBindVertexBuffers2EXT, "vkCmdBindVertexBuffers2");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetCullModeEXT, "vkCmdSetCullMode");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetFrontFaceEXT, "vkCmdSetFrontFace");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetPrimitiveTopologyEXT, "vkCmdSetPrimitiveTopology");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetViewportWithCountEXT, "vkCmdSetViewportWithCount");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetScissorWithCountEXT, "vkCmdSetScissorWithCount");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetDepthTestEnableEXT, "vkCmdSetDepthTestEnable");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetDepthWriteEnableEXT, "vkCmdSetDepthWriteEnable");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetDepthCompareOpEXT, "vkCmdSetDepthCompareOp");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetDepthBoundsTestEnableEXT, "vkCmdSetDepthBoundsTestEnable");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetStencilTestEnableEXT, "vkCmdSetStencilTestEnable");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetStencilOpEXT, "vkCmdSetStencilOp");
}

VulkanEXTExtendedDynamicState2Extension::VulkanEXTExtendedDynamicState2Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_3
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT
          )
      )
{
    m_enabledInCode = m_enabledInCode && ((g_cvarAllowDynamicStates.getValueOnAnyThread(true) & 2) != 0);
}

void VulkanEXTExtendedDynamicState2Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTExtendedDynamicState2Extension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.extendedDynamicState2 == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTExtendedDynamicState2 = 1u;
    }
}

void VulkanEXTExtendedDynamicState2Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanEXTExtendedDynamicState2Extension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    static_cast<void>(coreFeaturesAndProperties);
    m_requirementsPassed = true;
    extensionFlags.hasEXTExtendedDynamicState2 = 1u;

    bindCoreFunctionPointer(
        m_device, dynamic_api::vkCmdSetRasterizerDiscardEnableEXT, "vkCmdSetRasterizerDiscardEnable"
    );
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetDepthBiasEnableEXT, "vkCmdSetDepthBiasEnable");
    bindCoreFunctionPointer(m_device, dynamic_api::vkCmdSetPrimitiveRestartEnableEXT, "vkCmdSetPrimitiveRestartEnable");
}

VulkanEXTExtendedDynamicState3Extension::VulkanEXTExtendedDynamicState3Extension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceExtendedDynamicState3FeaturesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
          )
      )
{
    m_enabledInCode = m_enabledInCode && ((g_cvarAllowDynamicStates.getValueOnAnyThread(true) & 4) != 0);
}

void VulkanEXTExtendedDynamicState3Extension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceExtendedDynamicState3FeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTExtendedDynamicState3Extension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.extendedDynamicState3PolygonMode == VK_TRUE) &&
                           (m_features.extendedDynamicState3ColorBlendEnable == VK_TRUE) &&
                           (m_features.extendedDynamicState3ColorBlendEquation == VK_TRUE) &&
                           (m_features.extendedDynamicState3ColorWriteMask == VK_TRUE) &&
                           (m_features.extendedDynamicState3RasterizationSamples == VK_TRUE) &&
                           (m_features.extendedDynamicState3AlphaToCoverageEnable == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTExtendedDynamicState3 = 1u;
    }
}

void VulkanEXTExtendedDynamicState3Extension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (!m_requirementsPassed)
    {
        return;
    }

    // Retain only standalone core-compatible EDS3 features so subfeatures tied to unenabled vendor extensions
    // (such as NV shading rate image, viewport W-scaling, or coverage reduction) are never requested without them.
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT sanitized =
        zeroVulkanStruct<VkPhysicalDeviceExtendedDynamicState3FeaturesEXT>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
        );
    sanitized.extendedDynamicState3TessellationDomainOrigin = m_features.extendedDynamicState3TessellationDomainOrigin;
    sanitized.extendedDynamicState3DepthClampEnable = m_features.extendedDynamicState3DepthClampEnable;
    sanitized.extendedDynamicState3PolygonMode = m_features.extendedDynamicState3PolygonMode;
    sanitized.extendedDynamicState3RasterizationSamples = m_features.extendedDynamicState3RasterizationSamples;
    sanitized.extendedDynamicState3SampleMask = m_features.extendedDynamicState3SampleMask;
    sanitized.extendedDynamicState3AlphaToCoverageEnable = m_features.extendedDynamicState3AlphaToCoverageEnable;
    sanitized.extendedDynamicState3AlphaToOneEnable = m_features.extendedDynamicState3AlphaToOneEnable;
    sanitized.extendedDynamicState3LogicOpEnable = m_features.extendedDynamicState3LogicOpEnable;
    sanitized.extendedDynamicState3ColorBlendEnable = m_features.extendedDynamicState3ColorBlendEnable;
    sanitized.extendedDynamicState3ColorBlendEquation = m_features.extendedDynamicState3ColorBlendEquation;
    sanitized.extendedDynamicState3ColorWriteMask = m_features.extendedDynamicState3ColorWriteMask;
    m_features = sanitized;

    addToPNext(deviceCreateInfo, m_features);
}

VulkanEXTVertexInputDynamicStateExtension::VulkanEXTVertexInputDynamicStateExtension(
    VulkanDevice* const inDevice
) noexcept
    : VulkanDeviceExtension(inDevice, VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT
          )
      )
{
    m_enabledInCode = m_enabledInCode && ((g_cvarAllowDynamicStates.getValueOnAnyThread(true) & 8) != 0);
}

void VulkanEXTVertexInputDynamicStateExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTVertexInputDynamicStateExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.vertexInputDynamicState == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTVertexInputDynamicState = 1u;
    }
}

void VulkanEXTVertexInputDynamicStateExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_EXT_texture_compression_astc_hdr
// ---------------------------------------------------------------------------------------------------------------------

VulkanEXTTextureCompressionASTCHDRExtension::VulkanEXTTextureCompressionASTCHDRExtension(
    VulkanDevice* const inDevice
) noexcept
    : VulkanDeviceExtension(
          inDevice,
          VK_EXT_TEXTURE_COMPRESSION_ASTC_HDR_EXTENSION_NAME,
          (GP_VULKAN_SUPPORTS_TEXTURE_COMPRESSION_ASTC_HDR && VulkanPlatform::supportsASTCTextureFormats())
              ? kVulkanExtensionEnabled
              : kVulkanExtensionDisabled,
          VK_API_VERSION_1_3
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceTextureCompressionASTCHDRFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES
          )
      )
{}

void VulkanEXTTextureCompressionASTCHDRExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceTextureCompressionASTCHDRFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TEXTURE_COMPRESSION_ASTC_HDR_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanEXTTextureCompressionASTCHDRExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.textureCompressionASTC_HDR == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasEXTTextureCompressionASTCHDR = 1u;
    }
}

void VulkanEXTTextureCompressionASTCHDRExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanEXTTextureCompressionASTCHDRExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features13 != nullptr) &&
                           (coreFeaturesAndProperties.features13->textureCompressionASTC_HDR == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasEXTTextureCompressionASTCHDR = 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_buffer_device_address
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRBufferDeviceAddressExtension::VulkanKHRBufferDeviceAddressExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(
          inDevice, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, kVulkanExtensionEnabled, VK_API_VERSION_1_2
      )
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceBufferDeviceAddressFeatures>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES
          )
      )
{}

void VulkanKHRBufferDeviceAddressExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceBufferDeviceAddressFeatures>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRBufferDeviceAddressExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.bufferDeviceAddress == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasBufferDeviceAddress = 1u;
    }
}

void VulkanKHRBufferDeviceAddressExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

void VulkanKHRBufferDeviceAddressExtension::handleCoreFeaturesAndProperties(
    const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties, OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    const bool supported = (coreFeaturesAndProperties.features12 != nullptr) &&
                           (coreFeaturesAndProperties.features12->bufferDeviceAddress == VK_TRUE);
    m_requirementsPassed = supported;
    if (supported)
    {
        extensionFlags.hasBufferDeviceAddress = 1u;
        bindCoreFunctionPointer(m_device, dynamic_api::vkGetBufferDeviceAddressKHR, "vkGetBufferDeviceAddress");
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_KHR_acceleration_structure, VK_KHR_ray_tracing_pipeline, VK_KHR_ray_query
// ---------------------------------------------------------------------------------------------------------------------

VulkanKHRAccelerationStructureExtension::VulkanKHRAccelerationStructureExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
          )
      )
    , m_fallbackProperties(
          zeroVulkanStruct<VkPhysicalDeviceAccelerationStructurePropertiesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
          )
      )
{
    m_enabledInCode = m_enabledInCode && isRayTracingAllowed();
}

VkPhysicalDeviceAccelerationStructurePropertiesKHR&
    VulkanKHRAccelerationStructureExtension::resolveProperties() noexcept
{
    if (m_device != nullptr)
    {
        return m_device->getOptionalExtensionPropertiesMutable().accelerationStructureProps;
    }
    return m_fallbackProperties;
}

void VulkanKHRAccelerationStructureExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRAccelerationStructureExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.accelerationStructure == VK_TRUE) &&
                           (m_features.descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasAccelerationStructure = 1u;
    }
}

void VulkanKHRAccelerationStructureExtension::prePhysicalDeviceProperties(
    VkPhysicalDeviceProperties2& physicalDeviceProperties2
) noexcept
{
    if (m_requirementsPassed)
    {
        VkPhysicalDeviceAccelerationStructurePropertiesKHR& props = resolveProperties();
        props = zeroVulkanStruct<VkPhysicalDeviceAccelerationStructurePropertiesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
        );
        addToPNext(physicalDeviceProperties2, props);
    }
}

void VulkanKHRAccelerationStructureExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (!m_requirementsPassed)
    {
        return;
    }

    // Clear subfeatures requiring optional host-command or motion-blur extensions that are not enabled.
    m_features.accelerationStructureCaptureReplay = VK_FALSE;
    m_features.accelerationStructureIndirectBuild = VK_FALSE;
    m_features.accelerationStructureHostCommands = VK_FALSE;
    addToPNext(deviceCreateInfo, m_features);
}

VulkanKHRRayTracingPipelineExtension::VulkanKHRRayTracingPipelineExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
          )
      )
    , m_fallbackProperties(
          zeroVulkanStruct<VkPhysicalDeviceRayTracingPipelinePropertiesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
          )
      )
{
    m_enabledInCode = m_enabledInCode && isRayTracingAllowed();
}

VkPhysicalDeviceRayTracingPipelinePropertiesKHR& VulkanKHRRayTracingPipelineExtension::resolveProperties() noexcept
{
    if (m_device != nullptr)
    {
        return m_device->getOptionalExtensionPropertiesMutable().rayTracingPipelineProps;
    }
    return m_fallbackProperties;
}

void VulkanKHRRayTracingPipelineExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRRayTracingPipelineExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed =
        (m_features.rayTracingPipeline == VK_TRUE) && (m_features.rayTraversalPrimitiveCulling == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasRayTracingPipeline = 1u;
    }
}

void VulkanKHRRayTracingPipelineExtension::prePhysicalDeviceProperties(
    VkPhysicalDeviceProperties2& physicalDeviceProperties2
) noexcept
{
    if (m_requirementsPassed)
    {
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR& props = resolveProperties();
        props = zeroVulkanStruct<VkPhysicalDeviceRayTracingPipelinePropertiesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
        );
        addToPNext(physicalDeviceProperties2, props);
    }
}

void VulkanKHRRayTracingPipelineExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (!m_requirementsPassed)
    {
        return;
    }

    m_features.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE;
    m_features.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE;
    addToPNext(deviceCreateInfo, m_features);
}

VulkanKHRRayQueryExtension::VulkanKHRRayQueryExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_KHR_RAY_QUERY_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceRayQueryFeaturesKHR>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR
          )
      )
{
    m_enabledInCode = m_enabledInCode && isRayTracingAllowed();
}

void VulkanKHRRayQueryExtension::prePhysicalDeviceFeatures(VkPhysicalDeviceFeatures2& physicalDeviceFeatures2) noexcept
{
    m_features =
        zeroVulkanStruct<VkPhysicalDeviceRayQueryFeaturesKHR>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR);
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanKHRRayQueryExtension::postPhysicalDeviceFeatures(OptionalVulkanDeviceExtensions& extensionFlags) noexcept
{
    m_requirementsPassed = (m_features.rayQuery == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasRayQuery = 1u;
    }
}

void VulkanKHRRayQueryExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (m_requirementsPassed)
    {
        addToPNext(deviceCreateInfo, m_features);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// VK_NV_device_diagnostics_config
// ---------------------------------------------------------------------------------------------------------------------

VulkanNVDeviceDiagnosticConfigExtension::VulkanNVDeviceDiagnosticConfigExtension(VulkanDevice* const inDevice) noexcept
    : VulkanDeviceExtension(inDevice, VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME, kVulkanExtensionEnabled)
    , m_features(
          zeroVulkanStruct<VkPhysicalDeviceDiagnosticsConfigFeaturesNV>(
              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV
          )
      )
    , m_configInfo(
          zeroVulkanStruct<VkDeviceDiagnosticsConfigCreateInfoNV>(
              VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV
          )
      )
{}

void VulkanNVDeviceDiagnosticConfigExtension::prePhysicalDeviceFeatures(
    VkPhysicalDeviceFeatures2& physicalDeviceFeatures2
) noexcept
{
    m_features = zeroVulkanStruct<VkPhysicalDeviceDiagnosticsConfigFeaturesNV>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV
    );
    addToPNext(physicalDeviceFeatures2, m_features);
}

void VulkanNVDeviceDiagnosticConfigExtension::postPhysicalDeviceFeatures(
    OptionalVulkanDeviceExtensions& extensionFlags
) noexcept
{
    m_requirementsPassed = (m_features.diagnosticsConfig == VK_TRUE);
    if (m_requirementsPassed)
    {
        extensionFlags.hasNVDeviceDiagnosticConfig = 1u;
    }
}

void VulkanNVDeviceDiagnosticConfigExtension::preCreateDevice(VkDeviceCreateInfo& deviceCreateInfo) noexcept
{
    if (!m_requirementsPassed)
    {
        return;
    }

    m_configInfo = zeroVulkanStruct<VkDeviceDiagnosticsConfigCreateInfoNV>(
        VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV
    );
    m_configInfo.flags = VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |
                         VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |
                         VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV;

    addToPNext(deviceCreateInfo, m_features);
    if (gp::CommandLine::isInitialized() && gp::CommandLine::hasArgument("gpucrashdebugging"))
    {
        addToPNext(deviceCreateInfo, m_configInfo);
    }
}

}   // namespace gp::rhi::vulkan
