// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "shaders/VulkanShaderRHI.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"
#include "shaders/ShaderTypes.hpp"           // IWYU pragma: keep
#include "shaders/VulkanBaseShader.hpp"
#include "shaders/VulkanShaderFactory.hpp"

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Shared body of every `createVulkan*Shader` entry point.
/// @tparam InShaderType Concrete Vulkan shader type.
/// @param[in,out] factory Shader registry.
/// @param[in] device Logical device.
/// @param[in] deferredDeletionQueue Module deletion queue; may be null.
/// @param[in] code Serialized blob.
/// @param[in] hash RHI hash.
/// @return Shader, or null.
template <typename InShaderType>
[[nodiscard]] gp::RefCountedPtr<InShaderType> createVulkanShaderOfType(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept
{
    gp::RefCountedPtr<InShaderType> created = factory.createShader<InShaderType>(device, deferredDeletionQueue, code);
    if (created.isValid())
    {
        created->setHash(hash);
    }
    return created;
}

}   // namespace

shader::VertexShaderRef createVulkanVertexShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept
{
    return shader::VertexShaderRef(
        createVulkanShaderOfType<VulkanVertexShader>(factory, device, deferredDeletionQueue, code, hash)
    );
}

shader::PixelShaderRef createVulkanPixelShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept
{
    return shader::PixelShaderRef(
        createVulkanShaderOfType<VulkanPixelShader>(factory, device, deferredDeletionQueue, code, hash)
    );
}

shader::GeometryShaderRef createVulkanGeometryShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept
{
    return shader::GeometryShaderRef(
        createVulkanShaderOfType<VulkanGeometryShader>(factory, device, deferredDeletionQueue, code, hash)
    );
}

shader::ComputeShaderRef createVulkanComputeShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept
{
    return shader::ComputeShaderRef(
        createVulkanShaderOfType<VulkanComputeShader>(factory, device, deferredDeletionQueue, code, hash)
    );
}

shader::MeshShaderRef createVulkanMeshShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept
{
    return shader::MeshShaderRef(
        createVulkanShaderOfType<VulkanMeshShader>(factory, device, deferredDeletionQueue, code, hash)
    );
}

shader::AmplificationShaderRef createVulkanAmplificationShader(
    VulkanShaderFactory& factory,
    VulkanDevice& device,
    VulkanDeferredDeletionQueue* const deferredDeletionQueue,
    const gp::ConstVectorView<gp::UInt8> code,
    const shader::ShaderHash& hash
) noexcept
{
    return shader::AmplificationShaderRef(
        createVulkanShaderOfType<VulkanTaskShader>(factory, device, deferredDeletionQueue, code, hash)
    );
}

}   // namespace gp::rhi::vulkan
