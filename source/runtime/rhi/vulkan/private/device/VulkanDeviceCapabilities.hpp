// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "device/RHIGlobals.hpp"
#include "device/VulkanPhysicalDeviceFeatures.hpp"
#include "device/VulkanPhysicalDeviceProperties.hpp"
#include "device/VulkanQueueFamilySelection.hpp"
#include "extensions/VulkanExtensionTypes.hpp"
#include "RHIPixelFormat.hpp"
#include "RHIVulkan.hpp"
#include "templates/Function.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Per-`PixelFormat` Vulkan format mapping, component swizzle, and capability table built during device setup.
struct VulkanFormatSupportTable final
{
    /// @brief Selected image `VkFormat` for each engine `PixelFormat` (`VK_FORMAT_UNDEFINED` if unsupported).
    gp::Array<VkFormat, kPixelFormatCount> platformFormats{};

    /// @brief Selected texel-buffer `VkFormat` for each engine `PixelFormat` (`VK_FORMAT_UNDEFINED` if unsupported).
    gp::Array<VkFormat, kPixelFormatCount> bufferFormats{};

    /// @brief Hardware sRGB variant of `platformFormats[i]` when supported, otherwise `platformFormats[i]`.
    gp::Array<VkFormat, kPixelFormatCount> srgbFormats{};

    /// @brief Default `VkComponentMapping` swizzle applied when creating image views for each format.
    gp::Array<VkComponentMapping, kPixelFormatCount> componentMappings{};

    /// @brief True when the format supports either image or buffer usage on the active GPU.
    gp::Array<bool, kPixelFormatCount> supported{};

    /// @brief Resolved `PixelFormatCapabilities` bitmask for each format.
    gp::Array<PixelFormatCapabilities, kPixelFormatCount> capabilities{};

    /// @brief Zero-initializes all entries to `VK_FORMAT_UNDEFINED` and `PixelFormatCapabilities::None`.
    VulkanFormatSupportTable() noexcept;
};

/// @brief Callback signature returning `VkFormatProperties` for a given `VkFormat`.
using VulkanFormatPropertiesQueryFn = gp::UniqueFunction<VkFormatProperties(VkFormat)>;

/// @brief Callback signature reporting whether a `VkFormat` supports mipmapped 2D optimal sampled images.
using VulkanMipMapSupportQueryFn = gp::UniqueFunction<bool(VkFormat)>;

/// @brief Resolves the active Variable Rate Shading method (FSR vs FDM) and disables the non-selected path.
/// @param[in,out] extFlags Optional device extension flags (may clear FDM flags when FSR is chosen).
/// @param[in,out] extProps Optional device extension properties (may clear FSR/FDM feature bits).
/// @param[in] preference Policy selecting between Fragment Shading Rate and Fragment Density Map.
/// @param[out] outVrsCaps Populated `VariableRateShadingCapabilities` block for `RHIGlobals`.
GP_RHI_VULKAN_API void chooseVariableRateShadingMethod(
    OptionalVulkanDeviceExtensions& extFlags,
    OptionalVulkanDeviceExtensionProperties& extProps,
    VulkanVariableRateShadingPreference preference,
    gp::rhi::VariableRateShadingCapabilities& outVrsCaps
) noexcept;

/// @brief Populates `outSupport` by checking `VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT` for every `VertexElementType`.
/// @param[out] outSupport Destination vertex attribute support table.
/// @param[in] queryFormatProps Callback returning `VkFormatProperties` for each vertex `VkFormat`.
GP_RHI_VULKAN_API void populateVertexElementTypeSupport(
    gp::rhi::VertexElementTypeSupportInfo& outSupport, const VulkanFormatPropertiesQueryFn& queryFormatProps
) noexcept;

/// @brief Probes every engine `PixelFormat` (and its fallback list) to populate `outTable`.
/// @param[out] outTable Destination Vulkan format support and capability table.
/// @param[in] features Active physical device features (checked for `shaderImageGatherExtended`).
/// @param[in] renderDocFound True when RenderDoc is active (relaxes `R64_UInt` atomic requirement).
/// @param[in] queryFormatProps Callback returning `VkFormatProperties` for a `VkFormat`.
/// @param[in] queryMipMapSupport Callback reporting whether a `VkFormat` supports mipmapped 2D optimal sampled images.
GP_RHI_VULKAN_API void populateFormatSupportTable(
    VulkanFormatSupportTable& outTable,
    const VulkanPhysicalDeviceFeatures& features,
    bool renderDocFound,
    const VulkanFormatPropertiesQueryFn& queryFormatProps,
    const VulkanMipMapSupportQueryFn& queryMipMapSupport
) noexcept;

/// @brief Populates `outGlobals` limits, wave sizes, feature flags, vertex element support, and pixel format
///        capabilities from a configured Vulkan device state.
/// @param[in] features Negotiated physical device features.
/// @param[in] properties Queried physical device properties (core 1.0..1.4 + subgroup).
/// @param[in] extFlags Active optional device extension bitfield.
/// @param[in] extProps Retained extension properties (subgroup size control, RT, VRS, mesh shader).
/// @param[in] queueIndices Selected queue family indices.
/// @param[in] queueFamilyProps Physical device queue family properties.
/// @param[in] formatTable Populated Vulkan format support table.
/// @param[in] vertexSupport Populated vertex attribute support table.
/// @param[in] vrsCaps Resolved VRS capability block.
/// @param[in,out] outGlobals Destination `RHIGlobals` struct to update.
GP_RHI_VULKAN_API void populateRHIGlobalsFromDevice(
    const VulkanPhysicalDeviceFeatures& features,
    const VulkanPhysicalDeviceProperties& properties,
    const OptionalVulkanDeviceExtensions& extFlags,
    const OptionalVulkanDeviceExtensionProperties& extProps,
    const VulkanQueueFamilyIndices& queueIndices,
    gp::ConstVectorView<VkQueueFamilyProperties> queueFamilyProps,
    const VulkanFormatSupportTable& formatTable,
    const gp::rhi::VertexElementTypeSupportInfo& vertexSupport,
    const gp::rhi::VariableRateShadingCapabilities& vrsCaps,
    gp::rhi::RHIGlobals& outGlobals
) noexcept;

}   // namespace gp::rhi::vulkan
