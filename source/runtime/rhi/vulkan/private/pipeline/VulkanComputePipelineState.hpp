// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "memory/pointers/IntrusiveRefCounted.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanCommonPipelineState.hpp"
#include "RHIVulkan.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderTypes.hpp"
#include "shaders/VulkanShaderForward.hpp"   // IWYU pragma: keep
#include "VulkanForward.hpp"                 // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Cached compute pipeline: shader, specialization constants and `VkPipeline`.
/// @details Compute pipelines are keyed by the shader key and their specialization constants only; there is no
///          fixed-function state. They are never evicted by the LRU.
/// @note Created and mutated only by `VulkanPipelineStateCacheManager`; the RHI sees it through
///       `VulkanWrappedComputePipelineState`.
class GP_RHI_VULKAN_API VulkanComputePipelineState final
    : public gp::IntrusiveRefCounted<VulkanComputePipelineState>
    , public VulkanCommonPipelineState
{
public:
    /// @brief Creates a state without a native pipeline.
    /// @param[in] device Logical device that owns the pipeline.
    /// @param[in] deferredDeletionQueue Queue that releases the pipeline; null destroys it immediately.
    /// @param[in] owner Cache manager that tracks the state.
    /// @param[in] computeShader RHI compute shader; a reference is held.
    /// @param[in] specializationConstants Specialization constants of the compute stage.
    /// @param[in] key Cache key (shader key and constants).
    VulkanComputePipelineState(
        VulkanDevice& device,
        VulkanDeferredDeletionQueue* deferredDeletionQueue,
        VulkanPipelineStateCacheManager* owner,
        shader::ComputeShader* computeShader,
        const shader::SpecializationContainer& specializationConstants,
        gp::UInt64 key
    ) noexcept;

    /// @brief Releases the native pipeline.
    ~VulkanComputePipelineState() noexcept;

    /// @brief Records the bind of the pipeline on a command buffer.
    /// @param[in] commandBuffer Command buffer in the recording state.
    void bind(VulkanCommandBuffer& commandBuffer) const noexcept;

    /// @brief Returns the RHI compute shader.
    /// @return Shader.
    [[nodiscard]] shader::ComputeShader* getComputeShader() const noexcept;

    /// @brief Returns the Vulkan half of the compute shader.
    /// @return Shader.
    [[nodiscard]] VulkanShader* getVulkanShader() const noexcept
    {
        return m_vulkanShader;
    }

    /// @brief Returns the specialization constants of the compute stage.
    /// @return Constants.
    [[nodiscard]] const shader::SpecializationContainer& getSpecializationConstants() const noexcept
    {
        return m_specializationConstants;
    }

    /// @brief Returns the cache key.
    /// @return XXH64 of the constants, seeded with the shader key.
    [[nodiscard]] gp::UInt64 getKey() const noexcept
    {
        return m_key;
    }

    /// @brief Reports whether the native pipeline was created.
    /// @return False when creation failed or was skipped.
    [[nodiscard]] bool isValid() const noexcept
    {
        return m_isValid;
    }

private:
    /// @brief RHI compute shader; holds a reference.
    shader::ComputeShaderRef m_computeShader{};

    /// @brief Vulkan half of `m_computeShader`.
    VulkanShader* m_vulkanShader{ nullptr };

    /// @brief Specialization constants of the compute stage.
    shader::SpecializationContainer m_specializationConstants{};

    /// @brief Cache key.
    gp::UInt64 m_key{ 0u };

    /// @brief The native pipeline was created.
    bool m_isValid{ false };

    friend class VulkanPipelineStateCacheManager;
};

/// @brief Computes the cache key of a compute pipeline.
/// @details XXH64 of the specialization constant words, seeded with the shader key: the same shader with different
///          constants gets a different pipeline, and the same constants on different shaders never collide by design.
/// @param[in] shaderKey Deduplication key of the compute shader.
/// @param[in] specializationConstants Constants of the compute stage.
/// @return Cache key.
[[nodiscard]] GP_RHI_VULKAN_API gp::UInt64 computeVulkanComputePipelineKey(
    gp::UInt64 shaderKey, const shader::SpecializationContainer& specializationConstants
) noexcept;

}   // namespace gp::rhi::vulkan
