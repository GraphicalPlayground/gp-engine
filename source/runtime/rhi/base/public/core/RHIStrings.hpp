// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/strings/Name.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "core/ResourceEnums.hpp"
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "resources/BufferEnums.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIBase.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/ShaderEnums.hpp"
#include "sync/SyncEnums.hpp"
#include "views/ViewEnums.hpp"

namespace gp::rhi
{

using shader::getShaderFrequencyString;

/// @brief Converts `getRHIGlobals().gpuInfo.vendorId` into a printable vendor name (or `"Unknown"`).
/// @return Null-terminated static vendor name string.
[[nodiscard]] GP_RHI_API const char* rhiVendorIdToString() noexcept;

/// @brief Converts `vendorId` into a printable vendor name (or `"Unknown"`).
/// @param[in] vendorId PCI vendor identifier.
/// @return Null-terminated static vendor name string.
[[nodiscard]] GP_RHI_API const char* rhiVendorIdToString(GpuVendorId vendorId) noexcept;

/// @brief Converts `interfaceType` into its backend name string.
/// @param[in] interfaceType Graphics API interface enumerator.
/// @return Null-terminated static backend name string.
[[nodiscard]] GP_RHI_API const char* getInterfaceTypeString(InterfaceType interfaceType) noexcept;

/// @brief Converts `featureLevel` into its canonical string representation.
/// @param[in] featureLevel Shader model / feature level tier.
/// @return Canonical feature level string (`"ES3_1"`, `"SM5"`, `"SM6"`, etc.).
[[nodiscard]] GP_RHI_API gp::String featureLevelToString(FeatureLevel featureLevel) noexcept;

/// @brief Writes the canonical name for `featureLevel` into `outName` (`"InvalidFeatureLevel"` if out of range).
/// @param[in] featureLevel Target feature level.
/// @param[out] outName Receives the feature level name string.
GP_RHI_API void getFeatureLevelName(FeatureLevel featureLevel, gp::String& outName) noexcept;

/// @brief Writes the canonical `gp::Name` for `featureLevel` into `outName`.
/// @param[in] featureLevel Target feature level.
/// @param[out] outName Receives the feature level `gp::Name`.
GP_RHI_API void getFeatureLevelName(FeatureLevel featureLevel, gp::Name& outName) noexcept;

/// @brief Resolves a `FeatureLevel` enumerator from its string name.
/// @param[in] name Feature level text (`"ES2"`, `"ES3_1"`, `"SM4_REMOVED"`, `"SM5"`, `"SM6"`).
/// @param[out] outFeatureLevel Receives the parsed `FeatureLevel` (`FeatureLevel::Count` on failure).
/// @return True when `name` matched a known feature level; false otherwise.
[[nodiscard]] GP_RHI_API bool getFeatureLevelFromName(gp::StringView name, FeatureLevel& outFeatureLevel) noexcept;

/// @brief Resolves a `FeatureLevel` enumerator from a `gp::Name`.
/// @param[in] name Feature level `gp::Name`.
/// @param[out] outFeatureLevel Receives the parsed `FeatureLevel` (`FeatureLevel::Count` on failure).
/// @return True when `name` matched a known feature level; false otherwise.
[[nodiscard]] GP_RHI_API bool getFeatureLevelFromName(gp::Name name, FeatureLevel& outFeatureLevel) noexcept;

/// @brief Resolves a `FeatureLevel` enumerator from a null-terminated C-string.
/// @param[in] name Null-terminated feature level string.
/// @param[out] outFeatureLevel Receives the parsed `FeatureLevel` (`FeatureLevel::Count` on failure).
/// @return True when `name` matched a known feature level; false otherwise.
[[nodiscard]] inline bool getFeatureLevelFromName(const char* const name, FeatureLevel& outFeatureLevel) noexcept
{
    return getFeatureLevelFromName(gp::StringView(name != nullptr ? name : ""), outFeatureLevel);
}

/// @brief Resolves a `FeatureLevel` enumerator from an owning `gp::String`.
/// @param[in] name Feature level string.
/// @param[out] outFeatureLevel Receives the parsed `FeatureLevel` (`FeatureLevel::Count` on failure).
/// @return True when `name` matched a known feature level; false otherwise.
[[nodiscard]] inline bool getFeatureLevelFromName(const gp::String& name, FeatureLevel& outFeatureLevel) noexcept
{
    return getFeatureLevelFromName(name.view(), outFeatureLevel);
}

/// @brief Returns the static string literal for `shadingPath`.
/// @param[in] shadingPath Shading pipeline path.
/// @return Null-terminated static string (`"Deferred"`, `"Forward"`, `"Mobile"`, or `"InvalidShadingPath"`).
[[nodiscard]] GP_RHI_API const char* getShadingPathString(ShadingPath shadingPath) noexcept;

/// @brief Writes the canonical string name for `shadingPath` into `outName`.
/// @param[in] shadingPath Target shading path.
/// @param[out] outName Receives the shading path string.
GP_RHI_API void getShadingPathName(ShadingPath shadingPath, gp::String& outName) noexcept;

/// @brief Writes the canonical `gp::Name` for `shadingPath` into `outName`.
/// @param[in] shadingPath Target shading path.
/// @param[out] outName Receives the shading path `gp::Name`.
GP_RHI_API void getShadingPathName(ShadingPath shadingPath, gp::Name& outName) noexcept;

/// @brief Resolves a `ShadingPath` enumerator from its string name.
/// @param[in] name Shading path text (`"Deferred"`, `"Forward"`, `"Mobile"`).
/// @param[out] outShadingPath Receives the parsed `ShadingPath`.
/// @return True when `name` matched a known shading path; false otherwise.
[[nodiscard]] GP_RHI_API bool getShadingPathFromName(gp::StringView name, ShadingPath& outShadingPath) noexcept;

/// @brief Resolves a `ShadingPath` enumerator from a `gp::Name`.
/// @param[in] name Shading path `gp::Name`.
/// @param[out] outShadingPath Receives the parsed `ShadingPath`.
/// @return True when `name` matched a known shading path; false otherwise.
[[nodiscard]] GP_RHI_API bool getShadingPathFromName(gp::Name name, ShadingPath& outShadingPath) noexcept;

/// @brief Resolves a `ShadingPath` enumerator from a null-terminated C-string.
/// @param[in] name Null-terminated shading path string.
/// @param[out] outShadingPath Receives the parsed `ShadingPath`.
/// @return True when `name` matched a known shading path; false otherwise.
[[nodiscard]] inline bool getShadingPathFromName(const char* const name, ShadingPath& outShadingPath) noexcept
{
    return getShadingPathFromName(gp::StringView(name != nullptr ? name : ""), outShadingPath);
}

/// @brief Resolves a `ShadingPath` enumerator from an owning `gp::String`.
/// @param[in] name Shading path string.
/// @param[out] outShadingPath Receives the parsed `ShadingPath`.
/// @return True when `name` matched a known shading path; false otherwise.
[[nodiscard]] inline bool getShadingPathFromName(const gp::String& name, ShadingPath& outShadingPath) noexcept
{
    return getShadingPathFromName(name.view(), outShadingPath);
}

/// @brief Returns the static string literal for `configuration` (or `""` if unknown).
/// @param[in] configuration Bindless configuration tier.
/// @return Null-terminated static string (`"Disabled"`, `"RayTracing"`, `"Minimal"`, `"All"`).
[[nodiscard]] GP_RHI_API const char* getBindlessConfigurationString(view::BindlessConfiguration configuration) noexcept;

/// @brief Parses a `view::BindlessConfiguration` case-insensitively from `inString`.
/// @param[in] inString Candidate configuration string.
/// @param[out] outConfiguration Receives the parsed configuration on success.
/// @return True when `inString` matched a valid bindless configuration; false otherwise.
[[nodiscard]] GP_RHI_API bool
    getBindlessConfigurationFromString(gp::StringView inString, view::BindlessConfiguration& outConfiguration) noexcept;

/// @brief Returns the static string literal for `heapType`.
/// @param[in] heapType Hardware descriptor heap category.
/// @return Null-terminated static string.
[[nodiscard]] GP_RHI_API const char* getDescriptorHeapTypeString(view::DescriptorHeapType heapType) noexcept;

/// @brief Returns the static string literal for `descriptorType`.
/// @param[in] descriptorType Concrete descriptor view type.
/// @return Null-terminated static string.
[[nodiscard]] GP_RHI_API const char* getDescriptorTypeString(view::DescriptorType descriptorType) noexcept;

/// @brief Formats a `|`-delimited list of descriptor types set in `typeMask` (`"<empty>"` when `None`).
/// @param[in] typeMask Descriptor type bitmask.
/// @return Formatted bitmask string.
[[nodiscard]] GP_RHI_API gp::String getDescriptorTypeMaskString(view::DescriptorTypeMask typeMask) noexcept;

/// @brief Returns the static string literal for `resourceType` (`"<unknown>"` when unrecognized).
/// @param[in] resourceType Concrete RHI resource type tag.
/// @return Null-terminated static string.
[[nodiscard]] GP_RHI_API const char* stringFromRHIResourceType(resource::ResourceType resourceType) noexcept;

/// @brief Parses a `resource::ResourceType` case-insensitively from `inString` (`ResourceType::None` if unknown).
/// @param[in] inString Resource type name string.
/// @return Matching `resource::ResourceType`, or `resource::ResourceType::None`.
[[nodiscard]] GP_RHI_API resource::ResourceType rhiResourceTypeFromString(gp::StringView inString) noexcept;

/// @brief Returns the unprefixed name for a single `BufferUsageFlags` bit (or `""` if not a single known bit).
/// @param[in] singleFlag Single buffer usage flag bit.
/// @return Null-terminated static flag name.
[[nodiscard]] GP_RHI_API const char* getBufferUsageFlagString(resource::BufferUsageFlags singleFlag) noexcept;

/// @brief Formats a `|`-delimited list of `BUF_*` flag names present in `bufferUsage` (`"BUF_None"` when empty).
/// @param[in] bufferUsage Buffer usage bitmask.
/// @return Formatted bitmask string.
[[nodiscard]] GP_RHI_API gp::String getBufferUsageFlagsName(resource::BufferUsageFlags bufferUsage) noexcept;

/// @brief Returns the static string literal for `dimension` (`""` if unrecognized).
/// @param[in] dimension Texture dimensionality enumerator.
/// @return Null-terminated static string.
[[nodiscard]] GP_RHI_API const char* getTextureDimensionString(resource::TextureDimension dimension) noexcept;

/// @brief Returns the name for a single `TextureCreateFlags` bit (or `""` if not a single known bit).
/// @param[in] singleFlag Single texture creation flag bit.
/// @return Null-terminated static flag name.
[[nodiscard]] GP_RHI_API const char* getTextureCreateFlagString(resource::TextureCreateFlags singleFlag) noexcept;

/// @brief Formats a `|`-delimited list of flag names present in `textureCreateFlags` (`"None"` when empty).
/// @param[in] textureCreateFlags Texture creation bitmask.
/// @return Formatted bitmask string.
[[nodiscard]] GP_RHI_API gp::String getTextureCreateFlagsName(resource::TextureCreateFlags textureCreateFlags) noexcept;

/// @brief Formats a `|`-delimited list of `ResourceAccess` state bits (`"Unknown"` when `Unknown`).
/// @param[in] access Resource access bitmask.
/// @return Formatted bitmask string.
[[nodiscard]] GP_RHI_API gp::String getRHIAccessName(sync::ResourceAccess access) noexcept;

/// @brief Formats a `|`-delimited list of `ResourceTransitionFlags` bits (`"None"` when `None`).
/// @param[in] flags Resource transition flag bitmask.
/// @return Formatted bitmask string.
[[nodiscard]] GP_RHI_API gp::String getResourceTransitionFlagsName(sync::ResourceTransitionFlags flags) noexcept;

/// @brief Formats a `|`-delimited list of `PipelineStage` bits (`"None"`, `"Graphics"`, `"AsyncCompute"`, etc.).
/// @param[in] pipeline Pipeline stage bitmask.
/// @return Formatted bitmask string.
[[nodiscard]] GP_RHI_API gp::String getRHIPipelineName(sync::PipelineStage pipeline) noexcept;

/// @brief Returns the static string literal for `queryType`.
/// @param[in] queryType Render query metric enumerator.
/// @return `"Undefined"`, `"GPUOcclusion"`, or `"GPUAbsoluteTime"`.
[[nodiscard]] GP_RHI_API const char* getRenderQueryTypeString(sync::RenderQueryType queryType) noexcept;

/// @brief Returns the `gp::Name` identifier for `queryType`.
/// @param[in] queryType Render query metric enumerator.
/// @return `gp::Name` for `queryType`.
[[nodiscard]] GP_RHI_API gp::Name getRenderQueryTypeName(sync::RenderQueryType queryType) noexcept;

/// @brief Returns the static string literal for `elementType` (`"Unknown"` if unrecognized).
/// @param[in] elementType Vertex attribute format enumerator.
/// @return Null-terminated static string.
[[nodiscard]] GP_RHI_API const char* getVertexElementTypeString(pipeline::VertexElementType elementType) noexcept;

/// @brief Returns the static string literal for `primitiveType` (`"Unknown"` if unrecognized).
/// @param[in] primitiveType Draw primitive topology enumerator.
/// @return Null-terminated static string.
[[nodiscard]] GP_RHI_API const char* getPrimitiveTypeString(pipeline::PrimitiveType primitiveType) noexcept;

/// @brief Returns the static string literal for `format`.
/// @param[in] format Pixel format enumerator.
/// @return Null-terminated static format name.
[[nodiscard]] GP_RHI_API const char* getPixelFormatString(PixelFormat format) noexcept;

/// @brief Formats a `|`-delimited list of `PixelFormatCapabilities` bits (`"None"` when `None`).
/// @param[in] capabilities Pixel format capability bitmask.
/// @return Formatted bitmask string.
[[nodiscard]] GP_RHI_API gp::String getPixelFormatCapabilitiesName(PixelFormatCapabilities capabilities) noexcept;

/// @brief Returns the `UBMT_*` static string literal for `baseType`.
/// @param[in] baseType Uniform buffer reflected member type.
/// @return Null-terminated static string.
[[nodiscard]] GP_RHI_API const char* getUniformBufferBaseTypeString(shader::UniformBufferBaseType baseType) noexcept;

/// @brief Returns the static string literal for `bindingType`.
/// @param[in] bindingType Shader code reflected resource binding type.
/// @return Null-terminated static string.
[[nodiscard]] GP_RHI_API const char*
    getShaderCodeResourceBindingTypeName(shader::ShaderCodeResourceBindingType bindingType) noexcept;

}   // namespace gp::rhi
