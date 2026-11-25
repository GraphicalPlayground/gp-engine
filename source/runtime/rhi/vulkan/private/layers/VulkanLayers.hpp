// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "extensions/VulkanDeviceExtension.hpp"
#include "extensions/VulkanExtensionBase.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "extensions/VulkanInstanceExtension.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "RHIVulkan.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Standard Khronos Vulkan validation layer name.
inline constexpr const char* kKhronosStandardValidationLayerName = "VK_LAYER_KHRONOS_validation";

/// @brief RenderDoc capture layer name used to detect active frame-capture tooling.
inline constexpr const char* kRenderDocLayerName = "VK_LAYER_RENDERDOC_Capture";

/// @brief LunarG GFXReconstruct capture layer name enabled by `-vktrace`.
inline constexpr const char* kGfxReconstructLayerName = "VK_LAYER_LUNARG_gfxreconstruct";

/// @brief Legacy LunarG VkTrace capture layer name used as a fallback for `-vktrace`.
inline constexpr const char* kVkTraceLayerName = "VK_LAYER_LUNARG_vktrace";

/// @brief LunarG API dump layer name enabled by `-vulkanapidump` or `r.Vulkan.EnableAPIDump`.
inline constexpr const char* kApiDumpLayerName = "VK_LAYER_LUNARG_api_dump";

/// @brief Resolved validation layer settings produced by `setupValidationRequests()`.
struct VulkanValidationConfig final
{
    /// @brief True when `validationLevel > 0` and debugging support is compiled in.
    bool isDebugLayerEnabled{ false };

    /// @brief Active validation severity level (`0` = off, `1`..`5` = error..all).
    gp::Int32 validationLevel{ 0 };

    /// @brief Active GPU-assisted validation level (`0` = off, `1` = GPU-assisted, `2` = +reserve binding slot).
    gp::Int32 gpuValidationLevel{ 0 };
};

/// @brief Result returned by `setupInstanceLayers()`.
struct VulkanInstanceLayerSetupResult final
{
    /// @brief Deduplicated, lexicographically sorted list of instance layer names to enable.
    gp::Vector<const char*> enabledLayers{};

    /// @brief Optional `VkLayerSettingEXT` entries (for example `VK_LAYER_LUNARG_api_dump` file output settings).
    gp::Vector<VkLayerSettingEXT> layerSettings{};

    /// @brief Active debug or trace layer extension selected during instance setup.
    ActiveDebugLayerExtension activeDebugLayerExtension{ ActiveDebugLayerExtension::None };
};

/// @brief Result returned by `setupDeviceLayers()`.
struct VulkanDeviceLayerSetupResult final
{
    /// @brief Deduplicated, lexicographically sorted list of device layer names to enable.
    gp::Vector<const char*> enabledLayers{};

    /// @brief True when the RenderDoc capture layer was detected among available device layers.
    bool renderDocFound{ false };

    /// @brief True when legacy `VK_EXT_debug_marker` was activated because `VK_EXT_debug_utils` is unavailable.
    bool useLegacyDebugMarkerExt{ false };
};

/// @brief Evaluates command-line overrides (`-vulkandebug`, `-vulkanvalidation=N`, `-gpuvalidation`) and resolves
///        validation settings.
/// @return Resolved `VulkanValidationConfig`.
[[nodiscard]] GP_RHI_VULKAN_API VulkanValidationConfig setupValidationRequests() noexcept;

/// @brief Returns the current validation severity level from `r.Vulkan.EnableValidation`.
/// @return Integer validation level (`0`..`5`).
[[nodiscard]] GP_RHI_VULKAN_API gp::Int32 getVulkanValidationLevel() noexcept;

/// @brief Returns the current GPU-assisted validation level from `r.Vulkan.GPUValidation`.
/// @return Integer GPU validation level (`0`..`2`).
[[nodiscard]] GP_RHI_VULKAN_API gp::Int32 getVulkanGPUValidationLevel() noexcept;

/// @brief Enumerates available Vulkan instance layers and the extensions exposed by each layer.
/// @return Vector of `VulkanLayerWithExtensions` entries.
[[nodiscard]] GP_RHI_VULKAN_API gp::Vector<VulkanLayerWithExtensions> enumerateInstanceLayerProperties() noexcept;

/// @brief Enumerates available Vulkan device layers for `gpu` and the extensions exposed by each layer.
/// @param[in] gpu Physical device to query.
/// @return Vector of `VulkanLayerWithExtensions` entries.
[[nodiscard]] GP_RHI_VULKAN_API gp::Vector<VulkanLayerWithExtensions>
    enumerateDeviceLayerProperties(VkPhysicalDevice gpu) noexcept;

/// @brief Searches `layerProperties` for a layer matching `layerName`.
/// @param[in] layerName Null-terminated layer name to locate.
/// @param[in] layerProperties Available layer list.
/// @return Zero-based index if found, or `-1` if absent.
[[nodiscard]] GP_RHI_VULKAN_API gp::Int32 findLayerIndexInList(
    const char* layerName, gp::ConstVectorView<VulkanLayerWithExtensions> layerProperties
) noexcept;

/// @brief Adds `layerName` to `outLayers` if present in `layerProperties` and marks its exposed extensions as
///        supported in `engineExtensions`.
/// @tparam InExtensionType `VulkanInstanceExtension` or `VulkanDeviceExtension`.
/// @param[in] layerName Null-terminated layer name to request.
/// @param[in] layerProperties Available layer descriptors with their extension lists.
/// @param[in,out] engineExtensions Registered engine extensions to update when the layer supplies them.
/// @param[in,out] outLayers Destination list receiving `layerName` on success.
/// @return True if the layer was found and added; false otherwise.
template <typename InExtensionType>
[[nodiscard]] inline bool addRequestedLayer(
    const char* const layerName,
    const gp::ConstVectorView<VulkanLayerWithExtensions> layerProperties,
    gp::Vector<gp::UniquePtr<InExtensionType>>& engineExtensions,
    gp::Vector<const char*>& outLayers
) noexcept
{
    const gp::Int32 layerIndex = findLayerIndexInList(layerName, layerProperties);
    if (layerIndex < 0)
    {
        return false;
    }

    outLayers.pushBack(layerName);

    const gp::Vector<VkExtensionProperties>& layerExts =
        layerProperties[static_cast<gp::USize>(layerIndex)].extensionProperties;
    for (const VkExtensionProperties& extProp: layerExts)
    {
        const gp::Int32 extIndex = VulkanExtensionBase::findExtension(engineExtensions, extProp.extensionName);
        if (extIndex >= 0)
        {
            engineExtensions[static_cast<gp::USize>(extIndex)]->setSupported();
        }
    }

    return true;
}

/// @brief Discovers and selects Vulkan instance layers, debug extensions, and layer settings.
/// @param[in,out] extensions Engine instance extension list updated with layer-provided support and activation flags.
/// @return Selected instance layers, layer settings, and active debug layer extension.
[[nodiscard]] GP_RHI_VULKAN_API VulkanInstanceLayerSetupResult
    setupInstanceLayers(VulkanInstanceExtensionArray& extensions) noexcept;

/// @brief Selects Vulkan instance layers against a pre-enumerated `availableLayers` list (deterministic test entry
///        point).
/// @param[in] availableLayers Pre-enumerated instance layers and their extensions.
/// @param[in,out] extensions Engine instance extension list updated with layer-provided support and activation flags.
/// @param[in] validationConfig Resolved validation configuration controlling validation and feature activation.
/// @return Selected instance layers, layer settings, and active debug layer extension.
[[nodiscard]] GP_RHI_VULKAN_API VulkanInstanceLayerSetupResult setupInstanceLayers(
    gp::ConstVectorView<VulkanLayerWithExtensions> availableLayers,
    VulkanInstanceExtensionArray& extensions,
    const VulkanValidationConfig& validationConfig
) noexcept;

/// @brief Discovers and selects Vulkan device layers for `gpu`.
/// @param[in] gpu Target `VkPhysicalDevice`.
/// @param[in] supportsDebugUtilsExt True when instance-level `VK_EXT_debug_utils` is active.
/// @param[in,out] extensions Engine device extension list updated with layer-provided support and activation flags.
/// @return Selected device layers, RenderDoc presence flag, and legacy debug marker fallback flag.
[[nodiscard]] GP_RHI_VULKAN_API VulkanDeviceLayerSetupResult setupDeviceLayers(
    VkPhysicalDevice gpu, bool supportsDebugUtilsExt, VulkanDeviceExtensionArray& extensions
) noexcept;

/// @brief Selects Vulkan device layers against a pre-enumerated `availableLayers` list (deterministic test entry
///        point).
/// @param[in] availableLayers Pre-enumerated device layers and their extensions.
/// @param[in] supportsDebugUtilsExt True when instance-level `VK_EXT_debug_utils` is active.
/// @param[in,out] extensions Engine device extension list updated with layer-provided support and activation flags.
/// @return Selected device layers, RenderDoc presence flag, and legacy debug marker fallback flag.
[[nodiscard]] GP_RHI_VULKAN_API VulkanDeviceLayerSetupResult setupDeviceLayers(
    gp::ConstVectorView<VulkanLayerWithExtensions> availableLayers,
    bool supportsDebugUtilsExt,
    VulkanDeviceExtensionArray& extensions
) noexcept;

/// @brief Registers externally requested instance layer names before instance creation.
/// @param[in] layers View of null-terminated layer names.
GP_RHI_VULKAN_API void addExternalInstanceLayers(gp::ConstVectorView<const char*> layers) noexcept;

/// @brief Clears all externally registered instance layer names.
GP_RHI_VULKAN_API void clearExternalInstanceLayers() noexcept;

/// @brief Registers externally requested device layer names before device creation.
/// @param[in] layers View of null-terminated layer names.
GP_RHI_VULKAN_API void addExternalDeviceLayers(gp::ConstVectorView<const char*> layers) noexcept;

/// @brief Clears all externally registered device layer names.
GP_RHI_VULKAN_API void clearExternalDeviceLayers() noexcept;

}   // namespace gp::rhi::vulkan
