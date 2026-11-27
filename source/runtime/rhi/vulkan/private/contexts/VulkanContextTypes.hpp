// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"
#include "resources/UniformBuffer.hpp"   // IWYU pragma: keep
#include "shaders/ShaderData.hpp"        // IWYU pragma: keep
#include "VulkanThirdParty.hpp"          // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Default timeout used when a context blocks on its own completion sync point (five seconds).
inline constexpr gp::UInt64 kVulkanContextFlushTimeoutNs = 5'000'000'000ULL;

/// @brief Render pass and framebuffer that parallel (secondary) command buffers inherit on begin.
/// @details A parallel context records a slice of a render pass started by another context. Its command buffers
///          must begin with `VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT` and the parent's pass and framebuffer.
struct VulkanParallelRenderPassInfo
{
    /// @brief Render pass the parent context began; `VK_NULL_HANDLE` under dynamic rendering.
    VkRenderPass renderPass{ VK_NULL_HANDLE };

    /// @brief Framebuffer the parent context began; `VK_NULL_HANDLE` under dynamic rendering.
    VkFramebuffer framebuffer{ VK_NULL_HANDLE };

    /// @brief Subpass index the secondary command buffers continue.
    gp::UInt32 subpass{ 0u };
};

/// @brief Maximum uniform buffers a context tracks per shader stage (one dirty bit each).
inline constexpr gp::UInt32 kVulkanMaxBoundUniformBuffersPerStage = 32u;

/// @brief Uniform buffers bound to one shader stage, with the shader whose resource tables read them.
/// @details Binding a uniform buffer writes its descriptor right away, but the resources it carries (textures,
///          views, samplers) are only bound at draw/dispatch time. The dirty mask records which buffer indices still
///          need that resource-table walk, so a run of draws with unchanged bindings walks nothing.
struct VulkanStageUniformBuffers
{
    /// @brief Bound uniform buffers by shader-local index; null when unbound.
    gp::Array<resource::UniformBuffer*, kVulkanMaxBoundUniformBuffersPerStage> buffers{};

    /// @brief Shader bound to the stage by the last pipeline or parameter call; null when none.
    const shader::ShaderData* shader{ nullptr };

    /// @brief Bit `i` set when `buffers[i]` changed since its resource table was last walked.
    gp::UInt32 dirtyBits{ 0u };

    /// @brief Forgets every binding and the shader.
    void reset() noexcept
    {
        buffers.fill(nullptr);
        shader = nullptr;
        dirtyBits = 0u;
    }
};

}   // namespace gp::rhi::vulkan
