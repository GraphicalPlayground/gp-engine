// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/VulkanCommandBufferDynamicState.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/arrays/Vector.hpp"                 // IWYU pragma: keep
#include "CoreMinimal.hpp"                              // IWYU pragma: keep
#include "memory/pointers/UniquePtr.hpp"
#include "pendingstate/VulkanPendingStateForward.hpp"   // IWYU pragma: keep
#include "pendingstate/VulkanPendingStateTypes.hpp"
#include "pipeline/VulkanPipelineForward.hpp"           // IWYU pragma: keep
#include "pipeline/VulkanPipelineTypes.hpp"
#include "RHIDefinitions.hpp"
#include "RHIVulkan.hpp"
#include "VulkanForward.hpp"      // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep

namespace gp::rhi::vulkan
{

/// @brief Graphics state of a command context, flushed into the command buffer before every draw.
/// @details The setters only store values. `prepareForDraw()` then records, in order:
///          1. the input attachments of the active subpass, then the packed globals and the descriptor sets;
///          2. the dynamic states: viewports, scissors, stencil reference, blend constants, depth bounds, then every
///             extended dynamic state the pipeline enables (topology, cull mode, depth/stencil tests, depth bias,
///             blend, polygon mode, sample count, vertex input);
///          3. `vkCmdBindDescriptorSets`;
///          4. `vkCmdBindVertexBuffers`, once per run of bound streams.
///
///          Dynamic states are compared against what the command buffer last received
///          (`VulkanCommandBufferDynamicState`), not against the previous draw of this state, so two states sharing a
///          command buffer, or a state moving to a new command buffer, never skip a required command.
///
///          Viewports are flipped (negative height, origin at the bottom edge) so clip space keeps +Y up, as on the
///          other graphics APIs.
/// @note Not thread-safe; owned by one command context on the recording thread.
class GP_RHI_VULKAN_API VulkanPendingGfxState
{
public:
    /// @brief Creates a state with nothing bound.
    /// @param[in] device Owning device; must outlive the state. Its extensions decide which dynamic states exist.
    /// @param[in] context Command context services; must outlive the state.
    /// @param[in] defaults Default and global samplers and views given to every descriptor state.
    VulkanPendingGfxState(
        VulkanDevice& device, IVulkanPendingStateContext& context, const VulkanDescriptorDefaults& defaults
    ) noexcept;

    /// @brief Destroys every descriptor state.
    ~VulkanPendingGfxState() noexcept;

    VulkanPendingGfxState(const VulkanPendingGfxState&) = delete;
    VulkanPendingGfxState& operator=(const VulkanPendingGfxState&) = delete;
    VulkanPendingGfxState(VulkanPendingGfxState&&) = delete;
    VulkanPendingGfxState& operator=(VulkanPendingGfxState&&) = delete;

    /// @brief Clears viewports, scissors, streams and the current pipeline.
    void reset() noexcept;

    /// @brief Sets a single viewport and a scissor rectangle covering it.
    /// @details Vulkan rejects an empty depth range, so `minZ == maxZ` becomes `[minZ, minZ + 1]`.
    /// @param[in] minX Left edge in pixels.
    /// @param[in] minY Top edge in pixels.
    /// @param[in] minZ Near depth.
    /// @param[in] maxX Right edge in pixels.
    /// @param[in] maxY Bottom edge in pixels.
    /// @param[in] maxZ Far depth.
    void setViewport(
        gp::Float32 minX, gp::Float32 minY, gp::Float32 minZ, gp::Float32 maxX, gp::Float32 maxY, gp::Float32 maxZ
    ) noexcept;

    /// @brief Sets several viewports, each with a scissor rectangle covering it.
    /// @param[in] viewports Viewports in top-left origin convention.
    /// @param[in] count Number of viewports; clamped to `kVulkanMaxPendingViewports`.
    void setMultiViewport(const VkViewport* viewports, gp::UInt32 count) noexcept;

    /// @brief Sets the scissor rectangle, or resets it to the first viewport when `enable` is false.
    /// @param[in] enable True to clip to the rectangle.
    /// @param[in] minX Left edge in pixels.
    /// @param[in] minY Top edge in pixels.
    /// @param[in] maxX Right edge in pixels.
    /// @param[in] maxY Bottom edge in pixels.
    void setScissor(bool enable, gp::UInt32 minX, gp::UInt32 minY, gp::UInt32 maxX, gp::UInt32 maxY) noexcept;

    /// @brief Sets a single scissor rectangle.
    /// @param[in] x Left edge in pixels.
    /// @param[in] y Top edge in pixels.
    /// @param[in] width Width in pixels.
    /// @param[in] height Height in pixels.
    void setScissorRect(gp::Int32 x, gp::Int32 y, gp::UInt32 width, gp::UInt32 height) noexcept;

    /// @brief Binds a vertex buffer to a stream.
    /// @param[in] streamIndex Stream slot; below `kMaxVertexElementCount`.
    /// @param[in] buffer Native buffer, or `VK_NULL_HANDLE` to unbind.
    /// @param[in] offset Byte offset of the first vertex.
    void setStreamSource(gp::UInt32 streamIndex, VkBuffer buffer, VkDeviceSize offset) noexcept;

    /// @brief Sets the stencil reference of both faces.
    /// @param[in] stencilRef Reference value.
    void setStencilRef(gp::UInt32 stencilRef) noexcept;

    /// @brief Sets the blend constants.
    /// @param[in] blendFactor RGBA constants.
    void setBlendFactor(const gp::Array<gp::Float32, 4u>& blendFactor) noexcept;

    /// @brief Sets the depth bounds test range.
    /// @param[in] minDepthBounds Lower bound.
    /// @param[in] maxDepthBounds Upper bound.
    void setDepthBounds(gp::Float32 minDepthBounds, gp::Float32 maxDepthBounds) noexcept;

    /// @brief Makes `pipeline` current, creating its descriptor state on first use.
    /// @param[in] pipeline Graphics pipeline with a layout; must outlive its use by this state.
    /// @param[in] forceReset True to reset the descriptor state even when the pipeline is already current.
    /// @return True when the pipeline changed and must be bound again.
    bool setGfxPipeline(const VulkanGraphicsPipelineState& pipeline, bool forceReset) noexcept;

    /// @brief Binds the current pipeline into the command buffer.
    /// @param[in,out] commandBuffer Command buffer in the recording state.
    void bind(VulkanCommandBuffer& commandBuffer) noexcept;

    /// @brief Records everything the current pipeline needs before a draw.
    /// @param[in,out] commandBuffer Command buffer recording a render pass, with the current pipeline bound.
    /// @return False when no pipeline is current or the descriptor sets could not be updated.
    [[nodiscard]] bool prepareForDraw(VulkanCommandBuffer& commandBuffer) noexcept;

    /// @brief Drops the descriptor state of a pipeline that is being destroyed.
    /// @param[in] pipeline Pipeline being destroyed.
    void notifyDeletedPipeline(const VulkanGraphicsPipelineState& pipeline) noexcept;

    /// @brief Returns the current pipeline.
    /// @return Pipeline, or null.
    [[nodiscard]] const VulkanGraphicsPipelineState* getCurrentPipeline() const noexcept
    {
        return m_currentPipeline;
    }

    /// @brief Returns the descriptor state of the current pipeline; shader parameters are set through it.
    /// @return Descriptor state, or null when no pipeline is current.
    [[nodiscard]] VulkanGraphicsPipelineDescriptorState* getCurrentState() const noexcept
    {
        return m_currentState;
    }

    /// @brief Returns the number of pipelines with a descriptor state.
    /// @return Descriptor state count.
    [[nodiscard]] gp::USize getNumPipelineStates() const noexcept
    {
        return static_cast<gp::USize>(m_pipelineStates.size());
    }

    /// @brief Returns the pending viewports.
    /// @return Viewports; only the first `getNumViewports()` are meaningful.
    [[nodiscard]] const gp::Array<VkViewport, kVulkanMaxPendingViewports>& getViewports() const noexcept
    {
        return m_viewports;
    }

    /// @brief Returns the number of pending viewports.
    /// @return Viewport count.
    [[nodiscard]] gp::UInt32 getNumViewports() const noexcept
    {
        return m_numViewports;
    }

    /// @brief Returns the pending scissor rectangles.
    /// @return Rectangles; only the first `getNumScissors()` are meaningful.
    [[nodiscard]] const gp::Array<VkRect2D, kVulkanMaxPendingViewports>& getScissors() const noexcept
    {
        return m_scissors;
    }

    /// @brief Returns the number of pending scissor rectangles.
    /// @return Scissor count.
    [[nodiscard]] gp::UInt32 getNumScissors() const noexcept
    {
        return m_numScissors;
    }

    /// @brief Reports whether the last `setScissor()` enabled clipping.
    /// @return True when the scissor is not the viewport.
    [[nodiscard]] bool isScissorEnabled() const noexcept
    {
        return m_isScissorEnabled;
    }

    /// @brief Returns the pending stencil reference.
    /// @return Reference value.
    [[nodiscard]] gp::UInt32 getStencilRef() const noexcept
    {
        return m_stencilRef;
    }

    /// @brief Returns the pending blend constants.
    /// @return RGBA constants.
    [[nodiscard]] const gp::Array<gp::Float32, 4u>& getBlendFactor() const noexcept
    {
        return m_blendFactor;
    }

    /// @brief Returns the pending lower depth bound.
    /// @return Lower bound.
    [[nodiscard]] gp::Float32 getMinDepthBounds() const noexcept
    {
        return m_minDepthBounds;
    }

    /// @brief Returns the pending upper depth bound.
    /// @return Upper bound.
    [[nodiscard]] gp::Float32 getMaxDepthBounds() const noexcept
    {
        return m_maxDepthBounds;
    }

    /// @brief Returns the vertex buffer of a stream.
    /// @param[in] streamIndex Stream slot.
    /// @return Stream binding.
    [[nodiscard]] const VulkanVertexStream& getVertexStream(gp::UInt32 streamIndex) const noexcept;

    /// @brief Reports whether the vertex buffers must be bound again.
    /// @return True after a stream or pipeline change.
    [[nodiscard]] bool hasDirtyVertexStreams() const noexcept
    {
        return m_hasDirtyVertexStreams;
    }

    /// @brief Returns the dynamic states the current pipeline enables.
    /// @return State list; empty when no pipeline is current.
    [[nodiscard]] const VulkanDynamicStateList& getPipelineDynamicStates() const noexcept
    {
        return m_pipelineDynamicStates;
    }

private:
    /// @brief Records viewports, scissors, stencil reference, blend constants and depth bounds when they changed.
    /// @param[in,out] commandBuffer Command buffer and its last recorded dynamic state.
    void updateDynamicStates(VulkanCommandBuffer& commandBuffer) noexcept;

    /// @brief Records the extended dynamic states of the current pipeline that differ from the command buffer's.
    /// @param[in,out] dynamicState Last values recorded into the command buffer.
    /// @param[in] commandBuffer Native command buffer.
    /// @param[in] isFullUpdate True to record every state regardless of the last values.
    void updatePipelineDynamicStates(
        VulkanCommandBufferDynamicState& dynamicState, VkCommandBuffer commandBuffer, bool isFullUpdate
    ) noexcept;

    /// @brief Records `vkCmdBindVertexBuffers` for every pipeline binding, one call per run of bound streams.
    /// @param[in] commandBuffer Native command buffer.
    void bindVertexStreams(VkCommandBuffer commandBuffer) noexcept;

private:
    /// @brief Descriptor state of one pipeline.
    struct Entry
    {
        /// @brief Pipeline the state belongs to.
        const VulkanGraphicsPipelineState* pipeline{ nullptr };

        /// @brief Descriptor state.
        gp::UniquePtr<VulkanGraphicsPipelineDescriptorState> state{};
    };

    /// @brief Owning device.
    VulkanDevice& m_device;

    /// @brief Command context services.
    IVulkanPendingStateContext& m_context;

    /// @brief Default and global samplers and views.
    VulkanDescriptorDefaults m_defaults{};

    /// @brief Extensions that decide which dynamic states pipelines enable.
    VulkanDynamicStateFeatures m_features{};

    /// @brief Descriptor state per pipeline; a linear table.
    gp::Vector<Entry> m_pipelineStates{};

    /// @brief Current pipeline.
    const VulkanGraphicsPipelineState* m_currentPipeline{ nullptr };

    /// @brief Descriptor state of the current pipeline.
    VulkanGraphicsPipelineDescriptorState* m_currentState{ nullptr };

    /// @brief Dynamic states the current pipeline enables.
    VulkanDynamicStateList m_pipelineDynamicStates{};

    /// @brief Pending viewports, top-left origin.
    gp::Array<VkViewport, kVulkanMaxPendingViewports> m_viewports{};

    /// @brief Number of pending viewports.
    gp::UInt32 m_numViewports{ 0u };

    /// @brief Pending scissor rectangles.
    gp::Array<VkRect2D, kVulkanMaxPendingViewports> m_scissors{};

    /// @brief Number of pending scissor rectangles.
    gp::UInt32 m_numScissors{ 0u };

    /// @brief Pending stencil reference.
    gp::UInt32 m_stencilRef{ 0u };

    /// @brief Pending blend constants.
    gp::Array<gp::Float32, 4u> m_blendFactor{ 1.0f, 1.0f, 1.0f, 1.0f };

    /// @brief Pending lower depth bound.
    gp::Float32 m_minDepthBounds{ 0.0f };

    /// @brief Pending upper depth bound.
    gp::Float32 m_maxDepthBounds{ 1.0f };

    /// @brief Vertex buffer per stream.
    gp::Array<VulkanVertexStream, kMaxVertexElementCount> m_pendingStreams{};

    /// @brief True when the last `setScissor()` enabled clipping.
    bool m_isScissorEnabled{ false };

    /// @brief True when the vertex buffers must be bound again.
    bool m_hasDirtyVertexStreams{ true };

    /// @brief True when the pipeline changed since its extended dynamic states were last compared.
    bool m_isPipelineDynamicStateDirty{ true };
};

}   // namespace gp::rhi::vulkan
