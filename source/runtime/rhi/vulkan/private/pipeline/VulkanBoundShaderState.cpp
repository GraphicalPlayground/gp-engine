// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanBoundShaderState.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "memory/pointers/PointerTypes.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

VulkanBoundShaderState::VulkanBoundShaderState(
    pipeline::VertexDeclaration* vertexDeclaration,
    shader::VertexShader* vertexShader,
    shader::PixelShader* pixelShader,
    shader::GeometryShader* geometryShader
) noexcept
    : m_cacheLink(vertexDeclaration, vertexShader, pixelShader, geometryShader, this)
{
    m_cacheLink.addToCache();
}

VulkanBoundShaderState::VulkanBoundShaderState(
    shader::MeshShader* meshShader, shader::AmplificationShader* amplificationShader, shader::PixelShader* pixelShader
) noexcept
    : m_cacheLink(meshShader, amplificationShader, pixelShader, this)
{
    m_cacheLink.addToCache();
}

VulkanBoundShaderState::~VulkanBoundShaderState() noexcept
{
    m_cacheLink.removeFromCache();
}

pipeline::BoundShaderStateRef createVulkanBoundShaderState(
    pipeline::VertexDeclaration* vertexDeclaration,
    shader::VertexShader* vertexShader,
    shader::PixelShader* pixelShader,
    shader::GeometryShader* geometryShader
) noexcept
{
    pipeline::BoundShaderStateRef cached =
        findCachedVulkanBoundShaderState(vertexDeclaration, vertexShader, pixelShader, geometryShader);
    if (cached.isValid())
    {
        return cached;
    }
    // Two threads may both miss and both create: each state is valid and cached, and the later lookups return
    // whichever comes first. No lock is held across creation.
    return pipeline::BoundShaderStateRef(
        new VulkanBoundShaderState(vertexDeclaration, vertexShader, pixelShader, geometryShader), gp::kRetainRef
    );
}

pipeline::BoundShaderStateRef createVulkanBoundShaderState(
    shader::AmplificationShader* amplificationShader, shader::MeshShader* meshShader, shader::PixelShader* pixelShader
) noexcept
{
    pipeline::BoundShaderStateRef cached =
        findCachedVulkanBoundShaderState(nullptr, nullptr, pixelShader, nullptr, meshShader, amplificationShader);
    if (cached.isValid())
    {
        return cached;
    }
    return pipeline::BoundShaderStateRef(
        new VulkanBoundShaderState(meshShader, amplificationShader, pixelShader), gp::kRetainRef
    );
}

}   // namespace gp::rhi::vulkan
