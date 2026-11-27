// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "memory/pointers/IntrusiveRefCounted.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanCommonPipelineState.hpp"
#include "pipeline/VulkanGraphicsPipelineDesc.hpp"
#include "pipeline/VulkanPipelineKey.hpp"
#include "pipeline/VulkanPipelineTypes.hpp"
#include "renderpass/VulkanRenderPassForward.hpp"   // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "shaders/GraphicsShader.hpp"
#include "shaders/ShaderEnums.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderTypes.hpp"
#include "threading/sync/Mutex.hpp"
#include "VulkanCommon.hpp"
#include "VulkanForward.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Cached graphics pipeline: native description, cache key, `VkPipeline` and LRU links.
/// @details Everything needed to (re)create the `VkPipeline` is kept here: the description, the layout, the render
///          pass (absent with dynamic rendering), the shaders and their specialization constants. That is what lets the
///          LRU destroy the native pipeline under memory pressure and rebuild it transparently on the next use.
///
///          While the creating thread builds the pipeline, `m_isInitializing` is set and `m_initializingMutex` is held.
///          A second thread that asks for the same key finds the entry, takes a reference, and blocks on the mutex
///          instead of compiling the same pipeline twice.
/// @note Created and mutated only by `VulkanPipelineStateCacheManager`; the RHI sees it through
///       `VulkanWrappedGraphicsPipelineState`.
class GP_RHI_VULKAN_API VulkanGraphicsPipelineState final
    : public gp::IntrusiveRefCounted<VulkanGraphicsPipelineState>
    , public VulkanCommonPipelineState
{
public:
    /// @brief Creates a state without a native pipeline.
    /// @param[in] device Logical device that owns the pipeline.
    /// @param[in] deferredDeletionQueue Queue that releases the pipeline; null destroys it immediately.
    /// @param[in] owner Cache manager that tracks the state.
    /// @param[in] desc Native description; consumed.
    /// @param[in] key Cache key of `desc`; consumed.
    VulkanGraphicsPipelineState(
        VulkanDevice& device,
        VulkanDeferredDeletionQueue* deferredDeletionQueue,
        VulkanPipelineStateCacheManager* owner,
        VulkanGraphicsPipelineDesc&& desc,
        VulkanPipelineKey&& key
    ) noexcept;

    /// @brief Releases the native pipeline; the state must have left the LRU.
    ~VulkanGraphicsPipelineState() noexcept;

    /// @brief Records the bind of the pipeline on a command buffer.
    /// @note The caller must make sure the pipeline is resident
    /// (`VulkanPipelineStateCacheManager::touchGraphicsPipeline`)
    ///       when the LRU is enabled.
    /// @param[in] commandBuffer Command buffer in the recording state.
    void bind(VulkanCommandBuffer& commandBuffer) const noexcept;

    /// @brief Returns the RHI shader of one stage.
    /// @param[in] frequency Graphics stage (vertex, mesh, amplification, pixel or geometry).
    /// @return Shader, or null when the stage is unused or `frequency` is not a graphics stage.
    [[nodiscard]] shader::GraphicsShader* getShader(shader::ShaderFrequency frequency) const noexcept;

    /// @brief Returns the backend shader of one pipeline stage.
    /// @details The pending state reads the shader code headers through this accessor to find packed globals, global
    ///          samplers and input attachments without going back through the RHI shader objects.
    /// @param[in] stage Graphics pipeline stage.
    /// @return Shader, or null when the stage is unused or out of range.
    [[nodiscard]] VulkanShader* getVulkanShader(const shader_stage::Stage stage) const noexcept
    {
        const gp::USize index = static_cast<gp::USize>(stage);
        return (index < static_cast<gp::USize>(kNumGraphicsPipelineStages)) ? m_shaders[index] : nullptr;
    }

    /// @brief Returns the native description.
    /// @return Description the pipeline is (re)created from.
    [[nodiscard]] const VulkanGraphicsPipelineDesc& getDesc() const noexcept
    {
        return m_desc;
    }

    /// @brief Returns the cache key.
    /// @return Serialized description with its hash.
    [[nodiscard]] const VulkanPipelineKey& getKey() const noexcept
    {
        return m_key;
    }

    /// @brief Returns the hash precached pipelines are matched by.
    /// @return `computeVulkanPrecachePSOHash` of the creation initializer.
    [[nodiscard]] gp::UInt64 getPrecacheKey() const noexcept
    {
        return m_precacheKey;
    }

    /// @brief Returns the render pass the pipeline is compatible with.
    /// @return Render pass owned by the render pass manager, or null with dynamic rendering.
    [[nodiscard]] const VulkanRenderPass* getRenderPass() const noexcept
    {
        return m_renderPass;
    }

    /// @brief Reports whether a shader of the pipeline reads input attachments.
    /// @return True when the descriptor layout has input attachments.
    [[nodiscard]] bool hasInputAttachments() const noexcept
    {
        return m_hasInputAttachments;
    }

    /// @brief Returns the size the LRU charges for the pipeline.
    /// @return Driver cache blob size in bytes; 0 while the LRU does not track the pipeline.
    [[nodiscard]] gp::UInt32 getPipelineSize() const noexcept
    {
        return m_pipelineSize;
    }

    /// @brief Reports whether the pipeline is linked in the LRU list.
    /// @return True while the LRU tracks the pipeline.
    [[nodiscard]] bool isInLRU() const noexcept
    {
        return m_isInLRU;
    }

    /// @brief Returns the frame the LRU last saw the pipeline in.
    /// @return Frame number.
    [[nodiscard]] gp::UInt64 getLRUFrame() const noexcept
    {
        return m_lruFrame;
    }

private:
    /// @brief Native description.
    VulkanGraphicsPipelineDesc m_desc;

    /// @brief Cache key of `m_desc`.
    VulkanPipelineKey m_key;

    /// @brief Hash of the precache-relevant initializer fields.
    gp::UInt64 m_precacheKey{ 0u };

    /// @brief Vulkan shader per stage, indexed by `shader_stage::Stage`; kept alive by `m_rhiShaders`.
    VulkanGraphicsShaderArray m_shaders{};

    /// @brief RHI shader per stage, indexed by `shader_stage::Stage`; holds a reference.
    gp::Array<shader::GraphicsShaderRef, kNumGraphicsPipelineStages> m_rhiShaders{};

    /// @brief Specialization constants per stage, indexed by `shader_stage::Stage`.
    gp::Array<shader::SpecializationContainer, kNumGraphicsPipelineStages> m_specializationConstants{};

    /// @brief Render pass the pipeline is compatible with; null with dynamic rendering.
    VulkanRenderPass* m_renderPass{ nullptr };

    /// @brief A shader reads input attachments.
    bool m_hasInputAttachments{ false };

    /// @brief Held by the creating thread while the pipeline is built.
    gp::thread::Mutex m_initializingMutex{};

    /// @brief The creating thread has not finished; guarded by the manager's graphics lock.
    bool m_isInitializing{ false };

    /// @brief Size the LRU charges for the pipeline.
    gp::UInt32 m_pipelineSize{ 0u };

    /// @brief Pipeline is linked in the LRU list.
    bool m_isInLRU{ false };

    /// @brief Frame the LRU last saw the pipeline in.
    gp::UInt64 m_lruFrame{ 0u };

    /// @brief Previous (more recently used) pipeline in the LRU list.
    VulkanGraphicsPipelineState* m_lruPrev{ nullptr };

    /// @brief Next (less recently used) pipeline in the LRU list.
    VulkanGraphicsPipelineState* m_lruNext{ nullptr };

    friend class VulkanPipelineStateCacheManager;
};

}   // namespace gp::rhi::vulkan
