// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/PipelineForward.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanBoundShaderStateCache.hpp"
#include "RHIVulkan.hpp"
#include "shaders/ShaderForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Vulkan bound shader state: a cached combination of graphics stages and a vertex declaration.
/// @details Vulkan links stages only when a full pipeline is created, so this object carries no native handle. It
///          exists so the same stage combination maps to one RHI object, which the renderer can compare by address,
///          and registers itself in the bound shader state cache for its whole lifetime.
class GP_RHI_VULKAN_API VulkanBoundShaderState final : public pipeline::BoundShaderState
{
public:
    /// @brief Creates and caches the state of a vertex pipeline.
    /// @param[in] vertexDeclaration Vertex declaration, may be null.
    /// @param[in] vertexShader Vertex shader.
    /// @param[in] pixelShader Pixel shader, may be null.
    /// @param[in] geometryShader Geometry shader, may be null.
    VulkanBoundShaderState(
        pipeline::VertexDeclaration* vertexDeclaration,
        shader::VertexShader* vertexShader,
        shader::PixelShader* pixelShader,
        shader::GeometryShader* geometryShader
    ) noexcept;

    /// @brief Creates and caches the state of a mesh pipeline.
    /// @param[in] meshShader Mesh shader.
    /// @param[in] amplificationShader Amplification shader, may be null.
    /// @param[in] pixelShader Pixel shader, may be null.
    VulkanBoundShaderState(
        shader::MeshShader* meshShader,
        shader::AmplificationShader* amplificationShader,
        shader::PixelShader* pixelShader
    ) noexcept;

    /// @brief Returns the cache link (stages and declaration of the state).
    /// @return Link.
    [[nodiscard]] const VulkanCachedBoundShaderStateLink& getCacheLink() const noexcept
    {
        return m_cacheLink;
    }

protected:
    /// @copydoc pipeline::BoundShaderState::~BoundShaderState()
    /// @note Removes the state from the bound shader state cache.
    virtual ~VulkanBoundShaderState() noexcept override;

private:
    /// @brief Cache entry owned by the state.
    VulkanCachedBoundShaderStateLink m_cacheLink;
};

/// @brief Returns the cached bound shader state of a vertex pipeline, creating it on a miss.
/// @param[in] vertexDeclaration Vertex declaration, may be null.
/// @param[in] vertexShader Vertex shader.
/// @param[in] pixelShader Pixel shader, may be null.
/// @param[in] geometryShader Geometry shader, may be null.
/// @return Retained state; never null.
[[nodiscard]] GP_RHI_VULKAN_API pipeline::BoundShaderStateRef createVulkanBoundShaderState(
    pipeline::VertexDeclaration* vertexDeclaration,
    shader::VertexShader* vertexShader,
    shader::PixelShader* pixelShader,
    shader::GeometryShader* geometryShader
) noexcept;

/// @brief Returns the cached bound shader state of a mesh pipeline, creating it on a miss.
/// @param[in] amplificationShader Amplification shader, may be null.
/// @param[in] meshShader Mesh shader.
/// @param[in] pixelShader Pixel shader, may be null.
/// @return Retained state; never null.
[[nodiscard]] GP_RHI_VULKAN_API pipeline::BoundShaderStateRef createVulkanBoundShaderState(
    shader::AmplificationShader* amplificationShader, shader::MeshShader* meshShader, shader::PixelShader* pixelShader
) noexcept;

}   // namespace gp::rhi::vulkan
