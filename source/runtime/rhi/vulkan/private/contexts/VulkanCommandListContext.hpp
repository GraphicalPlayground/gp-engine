// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierResourceResolver.hpp"
#include "commands/CommandForward.hpp"   // IWYU pragma: keep
#include "commands/CommandTypes.hpp"
#include "commands/ICommandContext.hpp"
#include "commands/RenderPassInfo.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "contexts/VulkanContextCommon.hpp"
#include "contexts/VulkanContextDesc.hpp"
#include "contexts/VulkanContextTypes.hpp"
#include "core/ResourceForward.hpp"   // IWYU pragma: keep
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "maths/color/LinearColor.hpp"
#include "maths/vector/Vector4.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include "pendingstate/IVulkanPendingStateContext.hpp"
#include "pendingstate/VulkanPendingStateForward.hpp"   // IWYU pragma: keep
#include "pendingstate/VulkanPendingStateTypes.hpp"
#include "pipeline/PipelineForward.hpp"                 // IWYU pragma: keep
#include "renderpass/VulkanDynamicRendering.hpp"
#include "renderpass/VulkanRenderPassForward.hpp"       // IWYU pragma: keep
#include "resources/VulkanResourceForward.hpp"          // IWYU pragma: keep
#include "RHIVulkan.hpp"
#include "shaders/ShaderForward.hpp"                    // IWYU pragma: keep
#include "shaders/ShaderParameters.hpp"
#include "sync/SyncForward.hpp"                         // IWYU pragma: keep
#include "views/ViewForward.hpp"                        // IWYU pragma: keep
#include "VulkanCommon.hpp"
#include "VulkanForward.hpp"                            // IWYU pragma: keep
#include "VulkanThirdParty.hpp"                         // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Graphics and compute command context of the Vulkan backend.
/// @details The context is the translation target of a command list: each RHI command (bind, draw, copy, barrier)
///          lands here and is turned into `vkCmd*` calls on the command buffer `VulkanContextCommon` manages.
///
///          State is deferred the way production backends do it. Pipeline, viewport, vertex stream and descriptor
///          writes go to the pending graphics/compute states, which only touch the command buffer at
///          `prepareForDraw()` / `prepareForDispatch()`. That keeps redundant state changes free and lets descriptor
///          sets be allocated once per draw from the context's descriptor pool set.
///
///          Uniform buffers are two-phase: the buffer's own descriptor is written when bound, while the resources its
///          resource table carries are walked lazily, just before the draw, for the buffers marked dirty.
///
///          The context also implements `IVulkanPendingStateContext`, the seam the pending states use to allocate
///          descriptor sets and per-draw uniform memory. Descriptor pool set containers acquired this way are handed to
///          the signal payload on `finalize()`, so they are recycled only once the GPU has consumed the sets.
/// @note Not thread-safe: one context is driven by one translation thread at a time.
class GP_RHI_VULKAN_API VulkanCommandListContext final
    : public command::ICommandContext
    , public VulkanContextCommon
    , public IVulkanPendingStateContext
{
public:
    /// @brief Creates a context recording primary command buffers on `services.queue`.
    /// @details `services.device` and `services.queue` are mandatory. Missing optional services disable their
    ///          feature: no render pass manager means no render passes, no descriptor pools means no descriptor sets,
    ///          no temp block allocator means no packed globals, no query pool manager means no render queries.
    /// @param[in] services Collaborators of the context; pointers must outlive it.
    explicit VulkanCommandListContext(const VulkanContextServices& services) noexcept;

    /// @brief Releases any descriptor pool set still held and discards unfinished payloads.
    virtual ~VulkanCommandListContext() noexcept override;

    VulkanCommandListContext(const VulkanCommandListContext&) = delete;
    VulkanCommandListContext& operator=(const VulkanCommandListContext&) = delete;
    VulkanCommandListContext(VulkanCommandListContext&&) = delete;
    VulkanCommandListContext& operator=(VulkanCommandListContext&&) = delete;

public:
    /// @copydoc command::IComputeContext::setComputePipelineState()
    /// @note Applies the current static uniform buffers to the compute shader.
    virtual void setComputePipelineState(pipeline::ComputePipelineState* computePipelineState) noexcept override;

    /// @copydoc command::IComputeContext::dispatchComputeShader()
    virtual void dispatchComputeShader(
        gp::UInt32 threadGroupCountX, gp::UInt32 threadGroupCountY, gp::UInt32 threadGroupCountZ
    ) noexcept override;

    /// @copydoc command::IComputeContext::dispatchIndirectComputeShader()
    virtual void
        dispatchIndirectComputeShader(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept override;

    /// @copydoc command::IComputeContext::beginTransitions()
    virtual void beginTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept override;

    /// @copydoc command::IComputeContext::endTransitions()
    /// @note Cross-queue waits and reserved commits the transitions produce are attached to this context's payloads.
    virtual void endTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept override;

    /// @copydoc command::IComputeContext::clearUAVFloat()
    /// @note Texture UAVs are cleared with `vkCmdClearColorImage` in `GENERAL` layout. Buffer UAVs are filled with
    ///       `vkCmdFillBuffer`, which only supports one 32-bit pattern: differing components are rejected.
    virtual void clearUAVFloat(
        view::UnorderedAccessView* unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
    ) noexcept override;

    /// @copydoc command::IComputeContext::clearUAVUint()
    /// @note Same restrictions as `clearUAVFloat()`.
    virtual void clearUAVUint(
        view::UnorderedAccessView* unorderedAccessView, const resource::UIntVector4& values
    ) noexcept override;

    /// @copydoc command::IComputeContext::setShaderParameters()
    /// @note Bindless parameters are not supported yet and are ignored.
    virtual void setShaderParameters(
        shader::ComputeShader* shader,
        gp::ConstVectorView<gp::UInt8> inParametersData,
        gp::ConstVectorView<shader::ShaderParameter> inParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
    ) noexcept override;

    /// @copydoc command::IComputeContext::setShaderUnbinds()
    /// @note No-op: Vulkan descriptors carry no read/write hazard of their own, barriers do.
    virtual void setShaderUnbinds(
        shader::ComputeShader* shader, gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept override;

    /// @copydoc command::IComputeContext::setStaticUniformBuffers()
    /// @note Takes effect on the next pipeline bind, like every static binding.
    virtual void setStaticUniformBuffers(const shader::UniformBufferStaticBindings& uniformBuffers) noexcept override;

    /// @copydoc command::IComputeContext::setUniformBlockOffset()
    /// @note Uniform block descriptors are not supported yet: the offset is recorded but not applied.
    virtual void setUniformBlockOffset(view::ShaderResourceView* srv, gp::UInt32 offset) noexcept override;

    /// @copydoc command::IComputeContext::copyToStagingBuffer()
    virtual void copyToStagingBuffer(
        resource::Buffer* sourceBuffer,
        resource::StagingBuffer* destinationStagingBuffer,
        gp::UInt32 offset,
        gp::UInt32 numBytes
    ) noexcept override;

    /// @copydoc command::IComputeContext::writeGPUFence()
    /// @note GPU fences are written top of pipe by `writeVulkanGPUFenceTopOfPipe()`; reaching this is a misuse that
    ///       is reported and ignored.
    virtual void writeGPUFence(sync::GPUFence* fence) noexcept override;

    /// @copydoc command::ICommandContext::beginRenderPass()
    /// @note Uses dynamic rendering when the render pass manager does, `VkRenderPass` objects otherwise. Starts the
    ///       occlusion query batch and resets viewport and scissor to the render area.
    virtual void beginRenderPass(const command::RenderPassInfo& inInfo, const char* name) noexcept override;

    /// @copydoc command::ICommandContext::endRenderPass()
    virtual void endRenderPass() noexcept override;

    /// @copydoc command::ICommandContext::nextSubpass()
    /// @note With dynamic rendering, subpasses become a by-region barrier making color writes visible to input
    ///       attachment reads.
    virtual void nextSubpass() noexcept override;

    /// @copydoc command::ICommandContext::setGraphicsPipelineState()
    /// @note Applies the current static uniform buffers to every bound graphics shader.
    virtual void setGraphicsPipelineState(
        pipeline::GraphicsPipelineState* graphicsState, gp::UInt32 stencilRef, bool applyAdditionalState
    ) noexcept override;

    /// @copydoc command::ICommandContext::setStreamSource()
    virtual void
        setStreamSource(gp::UInt32 streamIndex, resource::Buffer* vertexBuffer, gp::UInt32 offset) noexcept override;

    /// @copydoc command::ICommandContext::setViewport()
    virtual void setViewport(
        gp::Float32 minX, gp::Float32 minY, gp::Float32 minZ, gp::Float32 maxX, gp::Float32 maxY, gp::Float32 maxZ
    ) noexcept override;

    /// @copydoc command::ICommandContext::setMultipleViewports()
    /// @note Clamped to `kVulkanMaxPendingViewports`.
    virtual void setMultipleViewports(gp::UInt32 count, const command::ViewportBounds* data) noexcept override;

    /// @copydoc command::ICommandContext::setScissorRect()
    virtual void setScissorRect(
        bool enable, gp::UInt32 minX, gp::UInt32 minY, gp::UInt32 maxX, gp::UInt32 maxY
    ) noexcept override;

    /// @copydoc command::ICommandContext::setStencilRef()
    virtual void setStencilRef(gp::UInt32 stencilRef) noexcept override;

    /// @copydoc command::ICommandContext::setBlendFactor()
    virtual void setBlendFactor(const gp::math::LinearColor& blendFactor) noexcept override;

    /// @copydoc command::ICommandContext::setDepthBounds()
    virtual void setDepthBounds(gp::Float32 minDepth, gp::Float32 maxDepth) noexcept override;

    /// @copydoc command::ICommandContext::setShaderParameters()
    /// @note Bindless parameters are not supported yet and are ignored.
    virtual void setShaderParameters(
        shader::GraphicsShader* shader,
        gp::ConstVectorView<gp::UInt8> inParametersData,
        gp::ConstVectorView<shader::ShaderParameter> inParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
    ) noexcept override;

    /// @copydoc command::ICommandContext::setShaderUnbinds()
    /// @note No-op, see the compute overload.
    virtual void setShaderUnbinds(
        shader::GraphicsShader* shader, gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept override;

    /// @copydoc command::ICommandContext::drawPrimitive()
    virtual void
        drawPrimitive(gp::UInt32 baseVertexIndex, gp::UInt32 numPrimitives, gp::UInt32 numInstances) noexcept override;

    /// @copydoc command::ICommandContext::drawPrimitiveIndirect()
    virtual void drawPrimitiveIndirect(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept override;

    /// @copydoc command::ICommandContext::drawIndexedPrimitive()
    virtual void drawIndexedPrimitive(
        resource::Buffer* indexBuffer,
        gp::Int32 baseVertexIndex,
        gp::UInt32 firstInstance,
        gp::UInt32 numVertices,
        gp::UInt32 startIndex,
        gp::UInt32 numPrimitives,
        gp::UInt32 numInstances
    ) noexcept override;

    /// @copydoc command::ICommandContext::drawIndexedPrimitiveIndirect()
    virtual void drawIndexedPrimitiveIndirect(
        resource::Buffer* indexBuffer, resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset
    ) noexcept override;

    /// @copydoc command::ICommandContext::beginRenderQuery()
    virtual void beginRenderQuery(sync::RenderQuery* renderQuery) noexcept override;

    /// @copydoc command::ICommandContext::endRenderQuery()
    virtual void endRenderQuery(sync::RenderQuery* renderQuery) noexcept override;

    /// @copydoc command::ICommandContext::copyTexture()
    /// @note Both textures must already be in copy layouts. A destination with a CPU readback buffer receives a
    ///       buffer copy made visible to host reads.
    virtual void copyTexture(
        resource::Texture* sourceTexture, resource::Texture* destTexture, const command::CopyTextureInfo& copyInfo
    ) noexcept override;

    /// @copydoc command::ICommandContext::copyBufferRegion()
    virtual void copyBufferRegion(
        resource::Buffer* destBuffer,
        gp::UInt64 dstOffset,
        resource::Buffer* sourceBuffer,
        gp::UInt64 srcOffset,
        gp::UInt64 numBytes
    ) noexcept override;

    /// @copydoc IVulkanPendingStateContext::acquirePoolSetAndDescriptorsIfNeeded()
    [[nodiscard]] virtual bool acquirePoolSetAndDescriptorsIfNeeded(
        const VulkanDescriptorSetsLayout& layout, bool needDescriptors, VkDescriptorSet* outSets
    ) noexcept override;

    /// @copydoc IVulkanPendingStateContext::allocateUniformMemory()
    /// @note Sub-allocates from the temp block allocator, retired with this context's sync point.
    [[nodiscard]] virtual bool allocateUniformMemory(gp::UInt32 size, VulkanUniformUpload& outUpload) noexcept override;

    /// @copydoc IVulkanPendingStateContext::getDescriptorSetCache()
    [[nodiscard]] virtual VulkanDescriptorSetCache* getDescriptorSetCache() noexcept override;

    /// @copydoc IVulkanPendingStateContext::getCurrentFrame()
    [[nodiscard]] virtual gp::UInt64 getCurrentFrame() const noexcept override;

    /// @copydoc IVulkanPendingStateContext::getInputAttachmentView()
    /// @note Only valid inside a render pass; depth reads use the read-only depth layout, colors `GENERAL`.
    [[nodiscard]] virtual bool getInputAttachmentView(
        VulkanShaderAttachmentType type, VulkanTextureView& outView, VkImageLayout& outLayout
    ) const noexcept override;

    /// @brief Sets the frame number descriptor pool sets are stamped with.
    /// @param[in] frame Frame counter of the RHI.
    void setCurrentFrame(gp::UInt64 frame) noexcept
    {
        m_currentFrame = frame;
    }

    /// @brief Returns the transfer recorder bound to the active command buffer, creating one if needed.
    /// @return Recorder, or `nullptr` when no command buffer could be acquired.
    [[nodiscard]] IVulkanTransferCommandRecorder* getTransferRecorder() noexcept;

    /// @brief Returns the pending graphics state.
    /// @return Pending graphics state.
    [[nodiscard]] VulkanPendingGfxState& getPendingGfxState() noexcept
    {
        return *m_gfxState;
    }

    /// @brief Returns the pending compute state.
    /// @return Pending compute state.
    [[nodiscard]] VulkanPendingComputeState& getPendingComputeState() noexcept
    {
        return *m_computeState;
    }

    /// @brief Tells whether a render pass (classic or dynamic) is open on this context.
    /// @return True between `beginRenderPass()` and `endRenderPass()`.
    [[nodiscard]] bool isInsideRenderPass() const noexcept
    {
        return m_isInsideRenderPass;
    }

    /// @brief Tells whether the open render pass uses dynamic rendering.
    /// @return True when `vkCmdBeginRendering` started the current pass.
    [[nodiscard]] bool isDynamicRenderingActive() const noexcept
    {
        return m_isDynamicRenderingActive;
    }

    /// @brief Returns the uniform buffers bound to a graphics stage.
    /// @param[in] stage Graphics stage.
    /// @return Stage bindings.
    [[nodiscard]] const VulkanStageUniformBuffers&
        getGraphicsStageUniformBuffers(shader_stage::Stage stage) const noexcept;

    /// @brief Returns the uniform buffers bound to the compute stage.
    /// @return Compute bindings.
    [[nodiscard]] const VulkanStageUniformBuffers& getComputeStageUniformBuffers() const noexcept
    {
        return m_computeUniformBuffers;
    }

    /// @brief Returns the static uniform buffers, indexed by static slot.
    /// @return Static bindings.
    [[nodiscard]] gp::ConstVectorView<resource::UniformBuffer*> getStaticUniformBuffers() const noexcept
    {
        return m_staticUniformBuffers;
    }

    /// @brief Returns the last offset given to `setUniformBlockOffset()`.
    /// @return Recorded offset.
    [[nodiscard]] gp::UInt32 getUniformBlockOffset() const noexcept
    {
        return m_uniformBlockOffset;
    }

protected:
    /// @copydoc VulkanContextCommon::endActiveRenderPass()
    /// @note Closes a dynamic rendering scope with `vkCmdEndRendering`.
    virtual void endActiveRenderPass(VulkanCommandBuffer& commandBuffer) noexcept override;

    /// @copydoc VulkanContextCommon::onCommandBufferBegun()
    /// @note Marks pending descriptor states dirty: a new command buffer has nothing bound.
    virtual void onCommandBufferBegun(VulkanCommandBuffer& commandBuffer) noexcept override;

    /// @copydoc VulkanContextCommon::onFinalize()
    /// @note Hands the descriptor pool set container to `signalPayload` so it is recycled on GPU completion.
    virtual void onFinalize(VulkanPayload& signalPayload) noexcept override;

private:
    /// @brief Returns the bindings tracked for `stage` of the given pipeline family.
    /// @param[in] isCompute True for the compute stage.
    /// @param[in] stage Shader stage.
    /// @return Stage bindings.
    [[nodiscard]] VulkanStageUniformBuffers& getStageUniformBuffers(bool isCompute, shader_stage::Stage stage) noexcept;

    /// @brief Applies loose and resource parameters to one stage of `descriptorState`.
    /// @param[in,out] descriptorState Descriptor state of the bound pipeline.
    /// @param[in,out] stageBuffers Uniform buffer tracking of the stage.
    /// @param[in] stage Shader stage.
    /// @param[in] isCompute True for compute dispatches (picks compute SRV access for layouts).
    /// @param[in] parametersData Loose parameter bytes.
    /// @param[in] parameters Loose parameters.
    /// @param[in] resourceParameters Resource parameters.
    void applyShaderParameters(
        VulkanCommonPipelineDescriptorState& descriptorState,
        VulkanStageUniformBuffers& stageBuffers,
        shader_stage::Stage stage,
        bool isCompute,
        gp::ConstVectorView<gp::UInt8> parametersData,
        gp::ConstVectorView<shader::ShaderParameter> parameters,
        gp::ConstVectorView<shader::ShaderParameterResource> resourceParameters
    ) noexcept;

    /// @brief Binds `uniformBuffer` at `bufferIndex` and marks its resource table dirty.
    /// @param[in,out] descriptorState Descriptor state of the bound pipeline.
    /// @param[in,out] stageBuffers Uniform buffer tracking of the stage.
    /// @param[in] stage Shader stage.
    /// @param[in] bufferIndex Shader-local uniform buffer index.
    /// @param[in] uniformBuffer Uniform buffer; null unbinds the tracking slot only.
    void bindUniformBuffer(
        VulkanCommonPipelineDescriptorState& descriptorState,
        VulkanStageUniformBuffers& stageBuffers,
        shader_stage::Stage stage,
        gp::UInt32 bufferIndex,
        resource::UniformBuffer* uniformBuffer
    ) noexcept;

    /// @brief Binds the static uniform buffers a shader reads.
    /// @param[in,out] descriptorState Descriptor state of the bound pipeline.
    /// @param[in,out] stageBuffers Uniform buffer tracking of the stage.
    /// @param[in] stage Shader stage.
    /// @param[in] shader Shader bound to the stage; null does nothing.
    void applyStaticUniformBuffers(
        VulkanCommonPipelineDescriptorState& descriptorState,
        VulkanStageUniformBuffers& stageBuffers,
        shader_stage::Stage stage,
        const shader::ShaderData* shader
    ) noexcept;

    /// @brief Walks the resource tables of the dirty uniform buffers of every graphics stage.
    void commitGraphicsResourceTables() noexcept;

    /// @brief Walks the resource tables of the dirty compute uniform buffers.
    void commitComputeResourceTables() noexcept;

    /// @brief Records a UAV clear with a 32-bit pattern per component.
    /// @param[in] unorderedAccessView UAV to clear.
    /// @param[in] color Clear value for texture UAVs.
    /// @param[in] words Component patterns for buffer UAVs.
    void clearUAV(
        view::UnorderedAccessView* unorderedAccessView,
        const VkClearColorValue& color,
        const gp::Array<gp::UInt32, 4u>& words
    ) noexcept;

    /// @brief Resets viewport and scissor to cover `renderArea`.
    /// @param[in] renderArea Render area of the pass just begun.
    void resetViewportToRenderArea(const VkRect2D& renderArea) noexcept;

private:
    /// @brief Resolves RHI resources to barrier descriptions (also used for texture layouts).
    VulkanBarrierResourceResolver m_resolver{};

    /// @brief Queue family snapshot used by transitions.
    VulkanBarrierDeviceDesc m_barrierDevice{};

    /// @brief Access translation inputs used by transitions and layout queries.
    VulkanBarrierTranslationDesc m_barrierTranslation{};

    /// @brief Descriptor defaults the pending states fall back to.
    VulkanDescriptorDefaults m_descriptorDefaults{};

    /// @brief Descriptor pool set acquirer; null when no pools manager was provided.
    gp::UniquePtr<VulkanDescriptorPoolSetAcquirer> m_poolSetAcquirer{};

    /// @brief Deferred graphics state.
    gp::UniquePtr<VulkanPendingGfxState> m_gfxState{};

    /// @brief Deferred compute state.
    gp::UniquePtr<VulkanPendingComputeState> m_computeState{};

    /// @brief Transfer recorder bound to `m_transferRecorderOwner`.
    gp::UniquePtr<VulkanCommandBufferTransferRecorder> m_transferRecorder{};

    /// @brief Command buffer `m_transferRecorder` records into.
    VulkanCommandBuffer* m_transferRecorderOwner{ nullptr };

    /// @brief Dynamic rendering description of the open pass (kept alive while the pass is open).
    VulkanDynamicRenderingInfo m_dynamicRenderingInfo{};

    /// @brief Framebuffer of the open classic render pass; null otherwise.
    VulkanFramebuffer* m_currentFramebuffer{ nullptr };

    /// @brief Classic render pass currently open; null otherwise.
    VulkanRenderPass* m_currentRenderPass{ nullptr };

    /// @brief Uniform buffers bound to each graphics stage.
    gp::Array<VulkanStageUniformBuffers, static_cast<gp::USize>(shader_stage::Stage::NumGraphicsStages)>
        m_graphicsUniformBuffers{};

    /// @brief Uniform buffers bound to the compute stage.
    VulkanStageUniformBuffers m_computeUniformBuffers{};

    /// @brief Static uniform buffers indexed by static slot.
    gp::Vector<resource::UniformBuffer*> m_staticUniformBuffers{};

    /// @brief Topology of the bound graphics pipeline, for primitive-to-vertex conversion.
    VkPrimitiveTopology m_currentTopology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };

    /// @brief Frame counter stamped on descriptor pool sets.
    gp::UInt64 m_currentFrame{ 0u };

    /// @brief Last offset passed to `setUniformBlockOffset()`.
    gp::UInt32 m_uniformBlockOffset{ 0u };

    /// @brief True between `beginRenderPass()` and `endRenderPass()`.
    bool m_isInsideRenderPass{ false };

    /// @brief True while a dynamic rendering scope is open.
    bool m_isDynamicRenderingActive{ false };
};

}   // namespace gp::rhi::vulkan
