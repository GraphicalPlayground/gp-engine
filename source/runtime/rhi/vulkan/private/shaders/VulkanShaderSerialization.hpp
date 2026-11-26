// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "shaders/ShaderForward.hpp"         // IWYU pragma: keep
#include "shaders/VulkanShaderForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Reads a `VulkanShaderHeader`.
/// @details Besides bounds checks, rejects out-of-range enumerators, uniform buffer tables larger than the binding
///          table, more bound uniform buffers than uniform buffer entries, and attachment or sampler entries that
///          reference a missing binding.
/// @param[in,out] reader Reader positioned at the header.
/// @param[out] outHeader Receives the header; contents are unspecified on failure.
/// @return True on success; on failure the reader is latched into its failed state.
GP_RHI_VULKAN_API bool readVulkanShaderHeader(VulkanShaderCodeReader& reader, VulkanShaderHeader& outHeader) noexcept;

/// @brief Writes a `VulkanShaderHeader` in the layout `readVulkanShaderHeader` expects.
/// @param[in,out] writer Destination writer.
/// @param[in] header Header to serialize.
GP_RHI_VULKAN_API void
    writeVulkanShaderHeader(VulkanShaderCodeWriter& writer, const VulkanShaderHeader& header) noexcept;

/// @brief Reads a `shader::ShaderResourceTable`.
/// @param[in,out] reader Reader positioned at the table.
/// @param[out] outTable Receives the table.
/// @return True on success.
GP_RHI_VULKAN_API bool
    readShaderResourceTable(VulkanShaderCodeReader& reader, shader::ShaderResourceTable& outTable) noexcept;

/// @brief Writes a `shader::ShaderResourceTable`.
/// @param[in,out] writer Destination writer.
/// @param[in] table Table to serialize.
GP_RHI_VULKAN_API void
    writeShaderResourceTable(VulkanShaderCodeWriter& writer, const shader::ShaderResourceTable& table) noexcept;

/// @brief Decodes a complete serialized shader blob, as passed to the `create*Shader(code, hash)` entry points.
/// @details Validates the magic number and version, every section, the SPIR-V magic word of each module, and that
///          no trailing bytes remain. Logs the reason of any rejection.
/// @param[in] bytes Serialized blob.
/// @param[out] outCode Receives the decoded content; unspecified on failure.
/// @return True when the blob is well formed.
GP_RHI_VULKAN_API bool parseVulkanShaderCode(gp::ConstVectorView<gp::UInt8> bytes, VulkanShaderCode& outCode) noexcept;

/// @brief Serializes a shader blob in the layout `parseVulkanShaderCode` expects.
/// @param[in] code Content to serialize; SPIR-V modules of ray hit groups are only written when the header says so.
/// @return Serialized blob.
[[nodiscard]] GP_RHI_VULKAN_API gp::Vector<gp::UInt8> serializeVulkanShaderCode(const VulkanShaderCode& code) noexcept;

}   // namespace gp::rhi::vulkan
