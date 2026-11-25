// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"    // IWYU pragma: keep
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "memory/Memory.hpp"
#include "memory/pointers/SharedPtr.hpp"
#include "templates/NumericLimits.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Sentinel indicating an extension has not been promoted to a core Vulkan specification version.
inline constexpr gp::UInt32 kVulkanExtensionNotPromoted = gp::NumericLimits<gp::UInt32>::max();

/// @brief Compile-time enable constant passed to extension constructors.
inline constexpr gp::Int32 kVulkanExtensionEnabled = 1;

/// @brief Compile-time disable constant passed to extension constructors.
inline constexpr gp::Int32 kVulkanExtensionDisabled = 0;

/// @brief Controls whether a supported extension is enabled automatically or waits for explicit activation.
enum class VulkanExtensionActivation : gp::UInt8
{
    /// @brief Enabled automatically when enabled in code and supported by the driver.
    AutoActivate = 0u,

    /// @brief Remains inactive until `setActivated()` is called (for debug layers or external plugins).
    ManuallyActivate
};

/// @brief Preference policy selecting between Fragment Shading Rate and Fragment Density Map VRS paths.
enum class VulkanVariableRateShadingPreference : gp::UInt8
{
    /// @brief Prefer Fragment Shading Rate when both FSR and FDM are available.
    PreferFSR = 0u,
    /// @brief Use Fragment Shading Rate only if available; log a message otherwise.
    UseFSROnlyIfAvailable,
    /// @brief Require Fragment Shading Rate and report an error if unavailable.
    RequireFSR,
    /// @brief Prefer Fragment Density Map when both FDM and FSR are available.
    PreferFDM,
    /// @brief Use Fragment Density Map only if available; log a message otherwise.
    UseFDMOnlyIfAvailable,
    /// @brief Require Fragment Density Map and report an error if unavailable.
    RequireFDM
};

/// @brief Active debugging or capture extension selected during instance layer setup.
enum class ActiveDebugLayerExtension : gp::UInt8
{
    None = 0u,
    GfxReconstructLayer,
    VkTraceLayer,
    DebugUtilsExtension
};

/// @brief Compact bitfield tracking optional Vulkan instance extensions enabled on the active `VkInstance`.
struct OptionalVulkanInstanceExtensions final
{
    gp::UInt32 hasKHRGetSurfaceCapabilities2 : 1;

    /// @brief Zero-initializes all optional instance extension flags.
    OptionalVulkanInstanceExtensions() noexcept
    {
        gp::memory::zeroMemory(this, sizeof(*this));
    }
};

/// @brief Compact bitfield tracking optional, vendor, and promoted Vulkan device extensions enabled on a `VkDevice`.
struct OptionalVulkanDeviceExtensions final
{
    // Optional extensions
    gp::UInt64 hasEXTValidationCache : 1;
    gp::UInt64 hasMemoryPriority : 1;
    gp::UInt64 hasMemoryBudget : 1;
    gp::UInt64 hasEXTASTCDecodeMode : 1;
    gp::UInt64 hasEXTFragmentDensityMap : 1;
    gp::UInt64 hasEXTFragmentDensityMap2 : 1;
    gp::UInt64 hasEXTFragmentDensityMapOffset : 1;
    gp::UInt64 hasKHRFragmentShadingRate : 1;
    gp::UInt64 hasKHRFragmentShaderBarycentric : 1;
    gp::UInt64 hasEXTFullscreenExclusive : 1;
    gp::UInt64 hasImageAtomicInt64 : 1;
    gp::UInt64 hasAccelerationStructure : 1;
    gp::UInt64 hasRayTracingPipeline : 1;
    gp::UInt64 hasRayQuery : 1;
    gp::UInt64 hasKHRPipelineLibrary : 1;
    gp::UInt64 hasDeferredHostOperations : 1;
    gp::UInt64 hasEXTCalibratedTimestamps : 1;
    gp::UInt64 hasKHRCalibratedTimestamps : 1;
    gp::UInt64 hasEXTDescriptorBuffer : 1;
    gp::UInt64 hasEXTDescriptorHeap : 1;
    gp::UInt64 hasEXTDeviceFault : 1;
    gp::UInt64 hasEXTMeshShader : 1;
    gp::UInt64 hasEXTToolingInfo : 1;
    gp::UInt64 hasEXTImageCompressionControl : 1;
    gp::UInt64 hasEXTMutableDescriptorType : 1;
    gp::UInt64 hasKHRMaintenance7 : 1;
    gp::UInt64 hasKHRMaintenance9 : 1;
    gp::UInt64 hasEXTShaderObject : 1;
    gp::UInt64 hasEXTGraphicsPipelineLibrary : 1;
    gp::UInt64 hasEXTDeviceGeneratedCommands : 1;
    gp::UInt64 hasEXTDeviceAddressBindingReport : 1;
    gp::UInt64 hasKHRPipelineBinary : 1;
    gp::UInt64 hasKHRPresentId2 : 1;
    gp::UInt64 hasEXTPresentTiming : 1;
    gp::UInt64 hasEXTPageableDeviceLocalMemory : 1;

    // Vendor specific
    gp::UInt64 hasAMDBufferMarker : 1;
    gp::UInt64 hasNVDiagnosticCheckpoints : 1;
    gp::UInt64 hasNVDeviceDiagnosticConfig : 1;
    gp::UInt64 hasANDROIDExternalMemoryHardwareBuffer : 1;

    // Promoted to Vulkan 1.1
    gp::UInt64 hasKHRMultiview : 1;
    gp::UInt64 hasKHR16bitStorage : 1;
    gp::UInt64 hasKHRSamplerYcbcrConversion : 1;

    // Promoted to Vulkan 1.2
    gp::UInt64 hasKHRRenderPass2 : 1;
    gp::UInt64 hasKHRImageFormatList : 1;
    gp::UInt64 hasKHRShaderAtomicInt64 : 1;
    gp::UInt64 hasEXTScalarBlockLayout : 1;
    gp::UInt64 hasBufferDeviceAddress : 1;
    gp::UInt64 hasSPIRV_14 : 1;
    gp::UInt64 hasShaderFloatControls : 1;
    gp::UInt64 hasKHRShaderFloat16 : 1;
    gp::UInt64 hasEXTDescriptorIndexing : 1;
    gp::UInt64 hasSeparateDepthStencilLayouts : 1;
    gp::UInt64 hasEXTHostQueryReset : 1;
    gp::UInt64 hasQcomRenderPassShaderResolve : 1;
    gp::UInt64 hasKHRDepthStencilResolve : 1;
    gp::UInt64 hasKHRTimelineSemaphore : 1;

    // Promoted to Vulkan 1.3
    gp::UInt64 hasEXTTextureCompressionASTCHDR : 1;
    gp::UInt64 hasKHRMaintenance4 : 1;
    gp::UInt64 hasKHRSynchronization2 : 1;
    gp::UInt64 hasKHRDynamicRendering : 1;
    gp::UInt64 hasEXTSubgroupSizeControl : 1;
    gp::UInt64 hasEXTPipelineCreationCacheControl : 1;
    gp::UInt64 hasEXTExtendedDynamicState1 : 1;
    gp::UInt64 hasEXTExtendedDynamicState2 : 1;
    gp::UInt64 hasEXTExtendedDynamicState3 : 1;
    gp::UInt64 hasEXTVertexInputDynamicState : 1;
    gp::UInt64 hasEXTImageRobustness : 1;

    // Promoted to Vulkan 1.4
    gp::UInt64 hasKHRMaintenance5 : 1;
    gp::UInt64 hasKHRMaintenance6 : 1;
    gp::UInt64 hasKHRPushDescriptors : 1;
    gp::UInt64 hasEXTLoadStoreOpNone : 1;
    gp::UInt64 hasEXTHostImageCopy : 1;

    /// @brief Zero-initializes all optional device extension flags.
    OptionalVulkanDeviceExtensions() noexcept
    {
        gp::memory::zeroMemory(this, sizeof(*this));
    }

    /// @brief Reports whether GPU breadcrumb crash-dump extensions (AMD buffer markers or NV checkpoints) are enabled.
    /// @return True when either AMD buffer markers or NVIDIA diagnostic checkpoints are active.
    [[nodiscard]] bool hasGPUCrashDumpExtensions() const noexcept
    {
        return (hasAMDBufferMarker != 0u) || (hasNVDiagnosticCheckpoints != 0u);
    }

    /// @brief Reports whether the full set of extensions required for hardware ray tracing is enabled.
    /// @return True when acceleration structures, ray tracing pipelines or ray query, descriptor indexing,
    ///         buffer device address, deferred host operations, SPIR-V 1.4, and float controls are all active.
    [[nodiscard]] bool hasRaytracingExtensions() const noexcept
    {
        return (hasAccelerationStructure != 0u) &&
               (((hasRayTracingPipeline != 0u) && (hasKHRPipelineLibrary != 0u)) || (hasRayQuery != 0u)) &&
               (hasEXTDescriptorIndexing != 0u) && (hasBufferDeviceAddress != 0u) &&
               (hasDeferredHostOperations != 0u) && (hasSPIRV_14 != 0u) && (hasShaderFloatControls != 0u);
    }

    /// @brief Reports whether any extended dynamic state or dynamic vertex input extension is active.
    /// @return True when extended dynamic state 1, 2, 3, or vertex input dynamic state is enabled.
    [[nodiscard]] bool hasAnyExtendedDynamicState() const noexcept
    {
        return (hasEXTExtendedDynamicState1 != 0u) || (hasEXTExtendedDynamicState2 != 0u) ||
               (hasEXTExtendedDynamicState3 != 0u) || (hasEXTVertexInputDynamicState != 0u);
    }
};

/// @brief Aggregated physical device extension properties retained after device discovery.
struct OptionalVulkanDeviceExtensionProperties final
{
    /// @brief Zero-initializes all retained extension property structures and sets `driverId` to
    /// `VK_DRIVER_ID_MAX_ENUM`.
    OptionalVulkanDeviceExtensionProperties() noexcept
    {
        gp::memory::zeroMemory(this, sizeof(*this));
        driverId = VK_DRIVER_ID_MAX_ENUM;
    }

    VkDriverId driverId{ VK_DRIVER_ID_MAX_ENUM };
    VkPhysicalDeviceSubgroupSizeControlPropertiesEXT subgroupSizeControlProperties{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProps{};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProps{};
    VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatures{};
    VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragmentDensityMapFeatures{};
    VkPhysicalDeviceFragmentDensityMap2FeaturesEXT fragmentDensityMap2Features{};
    VkPhysicalDeviceMeshShaderPropertiesEXT meshShaderProperties{};
};

/// @brief Pointers to core Vulkan 1.1 through 1.4 feature and property structs passed to promoted extension handlers.
/// @details Pointers are `nullptr` when the active Vulkan API version is below the struct's introducing version.
struct VulkanCoreFeaturesAndProperties final
{
    const VkPhysicalDeviceVulkan11Features* features11{ nullptr };
    const VkPhysicalDeviceVulkan12Features* features12{ nullptr };
    const VkPhysicalDeviceVulkan13Features* features13{ nullptr };
    const VkPhysicalDeviceVulkan14Features* features14{ nullptr };

    const VkPhysicalDeviceVulkan11Properties* properties11{ nullptr };
    const VkPhysicalDeviceVulkan12Properties* properties12{ nullptr };
    const VkPhysicalDeviceVulkan13Properties* properties13{ nullptr };
    const VkPhysicalDeviceVulkan14Properties* properties14{ nullptr };
};

/// @brief Base class allowing external subsystems or plugins to register custom Vulkan device extensions.
/// @details Subclasses override `getFeaturesStruct()` / `getPropertiesStruct()` to attach extension structs to
///          `VkPhysicalDeviceFeatures2`, `VkPhysicalDeviceProperties2`, and `VkDeviceCreateInfo` `pNext` chains.
class VulkanExternalDeviceExtensionBase : public gp::SharedFromThis<VulkanExternalDeviceExtensionBase>
{
public:
    /// @brief Constructs an external device extension descriptor.
    /// @param[in] inExtensionName Null-terminated Vulkan extension name string.
    /// @param[in] inPromotedVersion Vulkan version promoting this extension to core, or `kVulkanExtensionNotPromoted`.
    explicit VulkanExternalDeviceExtensionBase(
        const char* const inExtensionName, const gp::UInt32 inPromotedVersion = kVulkanExtensionNotPromoted
    ) noexcept
        : m_extensionName((inExtensionName != nullptr) ? inExtensionName : "")
        , m_promotedVersion(inPromotedVersion)
    {}

    /// @brief Virtual destructor.
    virtual ~VulkanExternalDeviceExtensionBase() noexcept = default;

    /// @brief Returns a pointer to the extension feature struct chained into physical-device feature queries.
    /// @return Pointer to a `VkBaseOutStructure` with `sType` initialized, or `nullptr` if none.
    [[nodiscard]] virtual VkBaseOutStructure* getFeaturesStruct() noexcept
    {
        return nullptr;
    }

    /// @brief Invoked after physical device features have been populated into `getFeaturesStruct()`.
    /// @return True if feature requirements are satisfied and the extension should be enabled; false otherwise.
    [[nodiscard]] virtual bool featuresCallback() noexcept
    {
        return true;
    }

    /// @brief Returns a pointer to the extension property struct chained into physical-device property queries.
    /// @return Pointer to a `VkBaseOutStructure` with `sType` initialized, or `nullptr` if none.
    [[nodiscard]] virtual VkBaseOutStructure* getPropertiesStruct() noexcept
    {
        return nullptr;
    }

    /// @brief Invoked after physical device properties have been populated into `getPropertiesStruct()`.
    /// @return True if property requirements are satisfied and the extension should be enabled; false otherwise.
    [[nodiscard]] virtual bool propertiesCallback() noexcept
    {
        return true;
    }

    /// @brief Invoked when another subsystem or the core engine already registered this extension.
    virtual void duplicateCallback() noexcept
    {}

    /// @brief Invoked when the extension is promoted to core Vulkan and `r.Vulkan.UseCoreStructs` is enabled.
    /// @param[in] coreFeaturesAndProperties Core feature and property structs populated for the active API version.
    virtual void coreCallback(const VulkanCoreFeaturesAndProperties& coreFeaturesAndProperties) noexcept
    {
        static_cast<void>(coreFeaturesAndProperties);
    }

    /// @brief Returns the null-terminated extension name.
    /// @return Extension name string.
    [[nodiscard]] const gp::String& getExtensionName() const noexcept
    {
        return m_extensionName;
    }

    /// @brief Returns the Vulkan version that promoted this extension to core, or `kVulkanExtensionNotPromoted`.
    /// @return Core promotion version.
    [[nodiscard]] gp::UInt32 getPromotedVersion() const noexcept
    {
        return m_promotedVersion;
    }

private:
    gp::String m_extensionName{};
    gp::UInt32 m_promotedVersion{ kVulkanExtensionNotPromoted };
};

/// @brief Pairs a discovered `VkLayerProperties` descriptor with the extensions exposed by that layer.
struct VulkanLayerWithExtensions final
{
    VkLayerProperties layerProperties{};
    gp::Vector<VkExtensionProperties> extensionProperties{};
};

}   // namespace gp::rhi::vulkan
