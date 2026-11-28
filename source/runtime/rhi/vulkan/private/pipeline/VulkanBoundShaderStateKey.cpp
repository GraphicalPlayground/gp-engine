// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanBoundShaderStateKey.hpp"
#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "hashing/XxHash.hpp"
#include "memory/pointers/PointerTypes.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "shaders/AmplificationShader.hpp"
#include "shaders/GeometryShader.hpp"
#include "shaders/MeshShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/VertexShader.hpp"

namespace gp::rhi::vulkan
{

VulkanBoundShaderStateLookupKey::VulkanBoundShaderStateLookupKey(
    const pipeline::VertexDeclaration* vertexDeclaration,
    const shader::VertexShader* vertexShader,
    const shader::PixelShader* pixelShader,
    const shader::GeometryShader* geometryShader
) noexcept
    : m_vertexDeclaration(vertexDeclaration)
    , m_vertexShader(vertexShader)
    , m_pixelShader(pixelShader)
    , m_geometryShader(geometryShader)
{
    computeHash();
}

VulkanBoundShaderStateLookupKey::VulkanBoundShaderStateLookupKey(
    const shader::MeshShader* meshShader,
    const shader::AmplificationShader* amplificationShader,
    const shader::PixelShader* pixelShader
) noexcept
    : m_meshShader(meshShader)
    , m_amplificationShader(amplificationShader)
    , m_pixelShader(pixelShader)
{
    computeHash();
}

bool VulkanBoundShaderStateLookupKey::operator==(const VulkanBoundShaderStateLookupKey& rhs) const noexcept
{
    return m_vertexDeclaration == rhs.m_vertexDeclaration && m_vertexShader == rhs.m_vertexShader &&
           m_meshShader == rhs.m_meshShader && m_amplificationShader == rhs.m_amplificationShader &&
           m_pixelShader == rhs.m_pixelShader && m_geometryShader == rhs.m_geometryShader;
}

void VulkanBoundShaderStateLookupKey::computeHash() noexcept
{
    // The slot of each pointer is part of its identity, so the addresses are hashed as an ordered array.
    const gp::Array<const void*, 6u> pointers{
        m_vertexDeclaration, m_vertexShader, m_meshShader, m_amplificationShader, m_pixelShader, m_geometryShader,
    };
    gp::hashing::XxHash64 hasher(0u);
    hasher.update(pointers.data(), sizeof(const void*) * pointers.size());
    m_hash = hasher.finalize();
}

VulkanBoundShaderStateKey::VulkanBoundShaderStateKey(
    pipeline::VertexDeclaration* vertexDeclaration,
    shader::VertexShader* vertexShader,
    shader::PixelShader* pixelShader,
    shader::GeometryShader* geometryShader
) noexcept
    : m_vertexDeclaration(vertexDeclaration, gp::kRetainRef)
    , m_vertexShader(vertexShader, gp::kRetainRef)
    , m_pixelShader(pixelShader, gp::kRetainRef)
    , m_geometryShader(geometryShader, gp::kRetainRef)
    , m_lookupKey(vertexDeclaration, vertexShader, pixelShader, geometryShader)
{}

VulkanBoundShaderStateKey::VulkanBoundShaderStateKey(
    shader::MeshShader* meshShader, shader::AmplificationShader* amplificationShader, shader::PixelShader* pixelShader
) noexcept
    : m_meshShader(meshShader, gp::kRetainRef)
    , m_amplificationShader(amplificationShader, gp::kRetainRef)
    , m_pixelShader(pixelShader, gp::kRetainRef)
    , m_lookupKey(meshShader, amplificationShader, pixelShader)
{}

VulkanBoundShaderStateKey::~VulkanBoundShaderStateKey() noexcept = default;

pipeline::VertexDeclaration* VulkanBoundShaderStateKey::getVertexDeclaration() const noexcept
{
    return m_vertexDeclaration.get();
}

shader::VertexShader* VulkanBoundShaderStateKey::getVertexShader() const noexcept
{
    return m_vertexShader.get();
}

shader::MeshShader* VulkanBoundShaderStateKey::getMeshShader() const noexcept
{
    return m_meshShader.get();
}

shader::AmplificationShader* VulkanBoundShaderStateKey::getAmplificationShader() const noexcept
{
    return m_amplificationShader.get();
}

shader::PixelShader* VulkanBoundShaderStateKey::getPixelShader() const noexcept
{
    return m_pixelShader.get();
}

shader::GeometryShader* VulkanBoundShaderStateKey::getGeometryShader() const noexcept
{
    return m_geometryShader.get();
}

}   // namespace gp::rhi::vulkan
