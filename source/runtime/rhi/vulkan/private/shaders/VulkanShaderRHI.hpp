// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/ContainerForward.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "shaders/ShaderForward.hpp"         // IWYU pragma: keep
#include "shaders/VulkanShaderForward.hpp"   // IWYU pragma: keep
#include "VulkanForward.hpp"                 // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Backend of `DynamicRHI::createVertexShader(code, hash)`.
/// @details Every `createVulkan*Shader` entry point decodes `code` as a serialized Vulkan shader blob (descriptor
///          header, resource table, then SPIR-V; see `VulkanShaderCode`), deduplicates it through `factory`, and
///          stamps the RHI hash on the result.
/// @param[in,out] factory Shader registry.
/// @param[in] device Logical device that creates the modules.
/// @param[in] deferredDeletionQueue Queue used to release module handles; may be null.
/// @param[in] code Serialized shader blob.
/// @param[in] hash RHI hash of the shader.
/// @return Shader, or a null reference when the blob is malformed.
[[nodiscard]] GP_RHI_VULKAN_API shader::VertexShaderRef createVulkanVertexShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* deferredDeletionQueue,
    gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept;

/// @brief Backend of `DynamicRHI::createPixelShader(code, hash)`.
/// @param[in,out] factory Shader registry.
/// @param[in] device Logical device that creates the modules.
/// @param[in] deferredDeletionQueue Queue used to release module handles; may be null.
/// @param[in] code Serialized shader blob.
/// @param[in] hash RHI hash of the shader.
/// @return Shader, or a null reference when the blob is malformed.
[[nodiscard]] GP_RHI_VULKAN_API shader::PixelShaderRef createVulkanPixelShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* deferredDeletionQueue,
    gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept;

/// @brief Backend of `DynamicRHI::createGeometryShader(code, hash)`.
/// @param[in,out] factory Shader registry.
/// @param[in] device Logical device that creates the modules.
/// @param[in] deferredDeletionQueue Queue used to release module handles; may be null.
/// @param[in] code Serialized shader blob.
/// @param[in] hash RHI hash of the shader.
/// @return Shader, or a null reference when the blob is malformed.
[[nodiscard]] GP_RHI_VULKAN_API shader::GeometryShaderRef createVulkanGeometryShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* deferredDeletionQueue,
    gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept;

/// @brief Backend of `DynamicRHI::createComputeShader(code, hash)`.
/// @param[in,out] factory Shader registry.
/// @param[in] device Logical device that creates the modules.
/// @param[in] deferredDeletionQueue Queue used to release module handles; may be null.
/// @param[in] code Serialized shader blob.
/// @param[in] hash RHI hash of the shader.
/// @return Shader, or a null reference when the blob is malformed.
[[nodiscard]] GP_RHI_VULKAN_API shader::ComputeShaderRef createVulkanComputeShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* deferredDeletionQueue,
    gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept;

/// @brief Backend of `DynamicRHI::createMeshShader(code, hash)`.
/// @param[in,out] factory Shader registry.
/// @param[in] device Logical device that creates the modules.
/// @param[in] deferredDeletionQueue Queue used to release module handles; may be null.
/// @param[in] code Serialized shader blob.
/// @param[in] hash RHI hash of the shader.
/// @return Shader, or a null reference when the blob is malformed.
[[nodiscard]] GP_RHI_VULKAN_API shader::MeshShaderRef createVulkanMeshShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* deferredDeletionQueue,
    gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept;

/// @brief Backend of `DynamicRHI::createAmplificationShader(code, hash)`.
/// @param[in,out] factory Shader registry.
/// @param[in] device Logical device that creates the modules.
/// @param[in] deferredDeletionQueue Queue used to release module handles; may be null.
/// @param[in] code Serialized shader blob.
/// @param[in] hash RHI hash of the shader.
/// @return Shader, or a null reference when the blob is malformed.
[[nodiscard]] GP_RHI_VULKAN_API shader::AmplificationShaderRef createVulkanAmplificationShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* deferredDeletionQueue,
    gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept;

}   // namespace gp::rhi::vulkan
