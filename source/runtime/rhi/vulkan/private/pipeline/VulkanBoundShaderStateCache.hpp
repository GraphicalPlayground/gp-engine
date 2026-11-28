// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                      // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"    // IWYU pragma: keep
#include "pipeline/PipelineForward.hpp"         // IWYU pragma: keep
#include "pipeline/VulkanBoundShaderStateKey.hpp"
#include "pipeline/VulkanPipelineForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "shaders/ShaderForward.hpp"            // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief A bound shader state's entry in the process-wide bound shader state cache.
/// @details Each `VulkanBoundShaderState` owns one link: it registers the link when it is constructed and removes it
///          when it is destroyed. The link keeps an owning key (so the stage pointers stay unique while cached) and a
///          non-owning pointer back to the state: the state's own lifetime decides how long the entry exists.
/// @note `addToCache()` and `removeFromCache()` are thread-safe; they serialize on the cache mutex.
class GP_RHI_VULKAN_API VulkanCachedBoundShaderStateLink
{
public:
    /// @brief Creates the link of a vertex pipeline; it is not cached until `addToCache()`.
    /// @param[in] vertexDeclaration Vertex declaration, may be null.
    /// @param[in] vertexShader Vertex shader.
    /// @param[in] pixelShader Pixel shader, may be null.
    /// @param[in] geometryShader Geometry shader, may be null.
    /// @param[in] boundShaderState State that owns this link.
    VulkanCachedBoundShaderStateLink(
        pipeline::VertexDeclaration* vertexDeclaration,
        shader::VertexShader* vertexShader,
        shader::PixelShader* pixelShader,
        shader::GeometryShader* geometryShader,
        pipeline::BoundShaderState* boundShaderState
    ) noexcept;

    /// @brief Creates the link of a mesh pipeline; it is not cached until `addToCache()`.
    /// @param[in] meshShader Mesh shader.
    /// @param[in] amplificationShader Amplification shader, may be null.
    /// @param[in] pixelShader Pixel shader, may be null.
    /// @param[in] boundShaderState State that owns this link.
    VulkanCachedBoundShaderStateLink(
        shader::MeshShader* meshShader,
        shader::AmplificationShader* amplificationShader,
        shader::PixelShader* pixelShader,
        pipeline::BoundShaderState* boundShaderState
    ) noexcept;

    VulkanCachedBoundShaderStateLink(const VulkanCachedBoundShaderStateLink&) = delete;
    VulkanCachedBoundShaderStateLink& operator=(const VulkanCachedBoundShaderStateLink&) = delete;
    VulkanCachedBoundShaderStateLink(VulkanCachedBoundShaderStateLink&&) = delete;
    VulkanCachedBoundShaderStateLink& operator=(VulkanCachedBoundShaderStateLink&&) = delete;

    /// @brief Registers the link in the cache.
    void addToCache() noexcept;

    /// @brief Removes this exact link from the cache (no-op when it is not cached).
    /// @details Removal is by link address, not by key: a replacement state with the same key may already be cached
    ///          while this one waits in the deferred deletion queue, and it must stay cached.
    void removeFromCache() noexcept;

    /// @brief Returns the owning key.
    /// @return Key.
    [[nodiscard]] const VulkanBoundShaderStateKey& getKey() const noexcept
    {
        return m_key;
    }

    /// @brief Returns the state that owns this link.
    /// @return Bound shader state.
    [[nodiscard]] pipeline::BoundShaderState* getBoundShaderState() const noexcept
    {
        return m_boundShaderState;
    }

private:
    /// @brief Owning key.
    VulkanBoundShaderStateKey m_key;

    /// @brief Owner of the link; not reference counted, the owner removes the link before it dies.
    pipeline::BoundShaderState* m_boundShaderState{ nullptr };
};

/// @brief Looks up a live cached bound shader state.
/// @details The mesh key is used when `meshShader` is not null, the vertex key otherwise. A state found in the cache
///          is returned only if a reference could be taken while it is live: one that already reached a zero count
///          and waits for deferred deletion is skipped, so the caller creates a replacement.
/// @param[in] vertexDeclaration Vertex declaration, may be null.
/// @param[in] vertexShader Vertex shader, may be null.
/// @param[in] pixelShader Pixel shader, may be null.
/// @param[in] geometryShader Geometry shader, may be null.
/// @param[in] meshShader Mesh shader, may be null.
/// @param[in] amplificationShader Amplification shader, may be null.
/// @return Retained state, or null when none is cached.
[[nodiscard]] GP_RHI_VULKAN_API pipeline::BoundShaderStateRef findCachedVulkanBoundShaderState(
    const pipeline::VertexDeclaration* vertexDeclaration,
    const shader::VertexShader* vertexShader,
    const shader::PixelShader* pixelShader,
    const shader::GeometryShader* geometryShader = nullptr,
    const shader::MeshShader* meshShader = nullptr,
    const shader::AmplificationShader* amplificationShader = nullptr
) noexcept;

/// @brief Forgets every cached link without touching the states that own them.
/// @note The states stay alive; their later `removeFromCache()` finds nothing to remove.
GP_RHI_VULKAN_API void emptyCachedVulkanBoundShaderStates() noexcept;

/// @brief Returns how many links are cached.
/// @return Number of cached links.
[[nodiscard]] GP_RHI_VULKAN_API gp::USize getCachedVulkanBoundShaderStateCount() noexcept;

}   // namespace gp::rhi::vulkan
