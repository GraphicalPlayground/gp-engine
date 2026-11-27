// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                      // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"    // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"         // IWYU pragma: keep
#include "pipeline/VulkanPipelineForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "shaders/ShaderForward.hpp"            // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Non-owning identity of a bound shader state: the six stage/declaration pointers it combines.
/// @details This is the type the cache compares during a lookup, so a lookup never touches a reference count. The
///          owning `VulkanBoundShaderStateKey` stored in each cache link keeps the pointed-to objects alive, so a
///          pointer seen here cannot be a recycled address while its link is cached.
class GP_RHI_VULKAN_API VulkanBoundShaderStateLookupKey
{
public:
    /// @brief Creates an empty key (every pointer null).
    VulkanBoundShaderStateLookupKey() noexcept = default;

    /// @brief Creates the key of a vertex pipeline.
    /// @param[in] vertexDeclaration Vertex declaration, may be null.
    /// @param[in] vertexShader Vertex shader.
    /// @param[in] pixelShader Pixel shader, may be null.
    /// @param[in] geometryShader Geometry shader, may be null.
    VulkanBoundShaderStateLookupKey(
        const pipeline::VertexDeclaration* vertexDeclaration,
        const shader::VertexShader* vertexShader,
        const shader::PixelShader* pixelShader,
        const shader::GeometryShader* geometryShader = nullptr
    ) noexcept;

    /// @brief Creates the key of a mesh pipeline.
    /// @param[in] meshShader Mesh shader.
    /// @param[in] amplificationShader Amplification shader, may be null.
    /// @param[in] pixelShader Pixel shader, may be null.
    VulkanBoundShaderStateLookupKey(
        const shader::MeshShader* meshShader,
        const shader::AmplificationShader* amplificationShader,
        const shader::PixelShader* pixelShader
    ) noexcept;

    /// @brief Returns a hash of the six pointers.
    /// @return Pointer hash; equal keys hash equally.
    [[nodiscard]] gp::UInt64 getHash() const noexcept
    {
        return m_hash;
    }

    /// @brief Pointer-wise equality.
    /// @param[in] rhs Key to compare against.
    /// @return True when every pointer matches.
    [[nodiscard]] bool operator==(const VulkanBoundShaderStateLookupKey& rhs) const noexcept;

private:
    /// @brief Recomputes `m_hash` from the pointers.
    void computeHash() noexcept;

private:
    /// @brief Vertex declaration.
    const pipeline::VertexDeclaration* m_vertexDeclaration{ nullptr };

    /// @brief Vertex shader.
    const shader::VertexShader* m_vertexShader{ nullptr };

    /// @brief Mesh shader.
    const shader::MeshShader* m_meshShader{ nullptr };

    /// @brief Amplification shader.
    const shader::AmplificationShader* m_amplificationShader{ nullptr };

    /// @brief Pixel shader.
    const shader::PixelShader* m_pixelShader{ nullptr };

    /// @brief Geometry shader.
    const shader::GeometryShader* m_geometryShader{ nullptr };

    /// @brief Cached pointer hash.
    gp::UInt64 m_hash{ 0u };
};

/// @brief Owning identity of a bound shader state.
/// @details Holds a strong reference to every stage and to the declaration. Without it, a shader recompiled into the
///          same address as a destroyed one would match a stale cache entry, since the cache compares pointers.
/// @note The special members are defined out of line so this header only needs forward declarations of the shader
///       types.
class GP_RHI_VULKAN_API VulkanBoundShaderStateKey
{
public:
    /// @brief Creates the key of a vertex pipeline and retains every non-null object.
    /// @param[in] vertexDeclaration Vertex declaration, may be null.
    /// @param[in] vertexShader Vertex shader.
    /// @param[in] pixelShader Pixel shader, may be null.
    /// @param[in] geometryShader Geometry shader, may be null.
    VulkanBoundShaderStateKey(
        pipeline::VertexDeclaration* vertexDeclaration,
        shader::VertexShader* vertexShader,
        shader::PixelShader* pixelShader,
        shader::GeometryShader* geometryShader = nullptr
    ) noexcept;

    /// @brief Creates the key of a mesh pipeline and retains every non-null shader.
    /// @param[in] meshShader Mesh shader.
    /// @param[in] amplificationShader Amplification shader, may be null.
    /// @param[in] pixelShader Pixel shader, may be null.
    VulkanBoundShaderStateKey(
        shader::MeshShader* meshShader,
        shader::AmplificationShader* amplificationShader,
        shader::PixelShader* pixelShader
    ) noexcept;

    /// @brief Releases every retained object.
    ~VulkanBoundShaderStateKey() noexcept;

    VulkanBoundShaderStateKey(const VulkanBoundShaderStateKey&) = delete;
    VulkanBoundShaderStateKey& operator=(const VulkanBoundShaderStateKey&) = delete;
    VulkanBoundShaderStateKey(VulkanBoundShaderStateKey&&) = delete;
    VulkanBoundShaderStateKey& operator=(VulkanBoundShaderStateKey&&) = delete;

    /// @brief Returns the vertex declaration.
    /// @return Declaration, or null.
    [[nodiscard]] pipeline::VertexDeclaration* getVertexDeclaration() const noexcept;

    /// @brief Returns the vertex shader.
    /// @return Shader, or null for a mesh pipeline.
    [[nodiscard]] shader::VertexShader* getVertexShader() const noexcept;

    /// @brief Returns the mesh shader.
    /// @return Shader, or null for a vertex pipeline.
    [[nodiscard]] shader::MeshShader* getMeshShader() const noexcept;

    /// @brief Returns the amplification shader.
    /// @return Shader, or null.
    [[nodiscard]] shader::AmplificationShader* getAmplificationShader() const noexcept;

    /// @brief Returns the pixel shader.
    /// @return Shader, or null.
    [[nodiscard]] shader::PixelShader* getPixelShader() const noexcept;

    /// @brief Returns the geometry shader.
    /// @return Shader, or null.
    [[nodiscard]] shader::GeometryShader* getGeometryShader() const noexcept;

    /// @brief Returns the non-owning identity used for lookups.
    /// @return Lookup key.
    [[nodiscard]] const VulkanBoundShaderStateLookupKey& getLookupKey() const noexcept
    {
        return m_lookupKey;
    }

private:
    /// @brief Retained vertex declaration.
    pipeline::VertexDeclarationRef m_vertexDeclaration{};

    /// @brief Retained vertex shader.
    shader::VertexShaderRef m_vertexShader{};

    /// @brief Retained mesh shader.
    shader::MeshShaderRef m_meshShader{};

    /// @brief Retained amplification shader.
    shader::AmplificationShaderRef m_amplificationShader{};

    /// @brief Retained pixel shader.
    shader::PixelShaderRef m_pixelShader{};

    /// @brief Retained geometry shader.
    shader::GeometryShaderRef m_geometryShader{};

    /// @brief Non-owning identity of the same pointers.
    VulkanBoundShaderStateLookupKey m_lookupKey{};
};

}   // namespace gp::rhi::vulkan
