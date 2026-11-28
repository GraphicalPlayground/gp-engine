// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "commands/CommandEnums.hpp"
#include "commands/CommandTypes.hpp"
#include "commands/ICommandContext.hpp"
#include "commands/IComputeContext.hpp"
#include "commands/RenderPassInfo.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/GPUMask.hpp"
#include "core/ResourceForward.hpp"
#include "core/ResourceTypes.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/DiagnosticsForward.hpp"
#include "maths/color/LinearColor.hpp"
#include "maths/vector/Vector4.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineForward.hpp"
#include "RHIBase.hpp"
#include "RHIDefinitions.hpp"
#include "shaders/ShaderForward.hpp"
#include "shaders/ShaderParameters.hpp"
#include "sync/SyncEnums.hpp"
#include "sync/SyncForward.hpp"
#include "sync/TransitionDesc.hpp"
#include "validation/ValidationForward.hpp"   // IWYU pragma: keep
#include "validation/ValidationResource.hpp"
#include "validation/ValidationTypes.hpp"
#include "views/ViewForward.hpp"

namespace gp::rhi
{

/// @brief Validation proxy wrapping an `IComputeContext` to audit compute PSOs, shader bindings, barriers, and UAVs.
class GP_RHI_API ValidationComputeContext final : public command::IComputeContext
{
    friend class ValidationRHI;

public:
    /// @brief Constructs a compute validation wrapper around `innerContext` owned by `validationRHI`.
    /// @param[in] validationRHI Owning `ValidationRHI` device wrapper.
    /// @param[in] innerContext Non-null concrete backend `IComputeContext`.
    ValidationComputeContext(ValidationRHI& validationRHI, command::IComputeContext* innerContext) noexcept;

    /// @copydoc command::IComputeContext::~IComputeContext()
    virtual ~ValidationComputeContext() noexcept override;

public:
    /// @copydoc command::IComputeContext::getPipeline()
    [[nodiscard]] virtual sync::PipelineStage getPipeline() const noexcept override
    {
        return m_innerContext->getPipeline();
    }

    /// @copydoc command::IComputeContext::getLowestLevelContext()
    [[nodiscard]] virtual command::IComputeContext& getLowestLevelContext() noexcept override
    {
        return m_innerContext->getLowestLevelContext();
    }

    /// @copydoc command::IComputeContext::setExecutingCommandList()
    virtual void setExecutingCommandList(command::CommandListBase* cmdList) noexcept override;

    /// @copydoc command::IComputeContext::setComputePipelineState()
    virtual void setComputePipelineState(pipeline::ComputePipelineState* computePipelineState) noexcept override;

    /// @copydoc command::IComputeContext::dispatchComputeShader()
    virtual void dispatchComputeShader(
        gp::UInt32 threadGroupCountX, gp::UInt32 threadGroupCountY, gp::UInt32 threadGroupCountZ
    ) noexcept override;

    /// @copydoc command::IComputeContext::dispatchIndirectComputeShader()
    virtual void
        dispatchIndirectComputeShader(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept override;

    /// @copydoc command::IComputeContext::setAsyncComputeBudget()
    virtual void setAsyncComputeBudget(command::AsyncComputeBudget budget) noexcept override;

    /// @copydoc command::IComputeContext::setComputeBudget()
    virtual void setComputeBudget(command::SyncComputeBudget budget) noexcept override;

    /// @copydoc command::IComputeContext::setLowLatencyHint()
    virtual void setLowLatencyHint(bool lowLatency) noexcept override;

    /// @copydoc command::IComputeContext::beginTransitions()
    virtual void beginTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept override;

    /// @copydoc command::IComputeContext::endTransitions()
    virtual void endTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept override;

    /// @copydoc command::IComputeContext::clearUAVFloat()
    virtual void clearUAVFloat(
        view::UnorderedAccessView* unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
    ) noexcept override;

    /// @copydoc command::IComputeContext::clearUAVUint()
    virtual void clearUAVUint(
        view::UnorderedAccessView* unorderedAccessView, const resource::UIntVector4& values
    ) noexcept override;

    /// @copydoc command::IComputeContext::setShaderRootConstants()
    virtual void setShaderRootConstants(const resource::UIntVector4& constants) noexcept override;

    /// @copydoc command::IComputeContext::setShaderParameters()
    virtual void setShaderParameters(
        shader::ComputeShader* shader,
        gp::ConstVectorView<gp::UInt8> inParametersData,
        gp::ConstVectorView<shader::ShaderParameter> inParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
    ) noexcept override;

    /// @copydoc command::IComputeContext::setShaderUnbinds()
    virtual void setShaderUnbinds(
        shader::ComputeShader* shader, gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept override;

    /// @copydoc command::IComputeContext::setStaticUniformBuffers()
    virtual void setStaticUniformBuffers(const shader::UniformBufferStaticBindings& uniformBuffers) noexcept override;

    /// @copydoc command::IComputeContext::setStaticUniformBuffer()
    virtual void setStaticUniformBuffer(
        resource::UniformBufferStaticSlot slot, resource::UniformBuffer* uniformBuffer
    ) noexcept override;

    /// @copydoc command::IComputeContext::setUniformBlockOffset()
    virtual void setUniformBlockOffset(view::ShaderResourceView* srv, gp::UInt32 offset) noexcept override;

    /// @copydoc command::IComputeContext::copyToStagingBuffer()
    virtual void copyToStagingBuffer(
        resource::Buffer* sourceBuffer,
        resource::StagingBuffer* destinationStagingBuffer,
        gp::UInt32 offset,
        gp::UInt32 numBytes
    ) noexcept override;

    /// @copydoc command::IComputeContext::writeGPUFence()
    virtual void writeGPUFence(sync::GPUFence* fence) noexcept override;

    /// @copydoc command::IComputeContext::setGPUMask()
    virtual void setGPUMask(GPUMask gpuMask) noexcept override;

    /// @copydoc command::IComputeContext::postExternalCommandsReset()
    virtual void postExternalCommandsReset() noexcept override;

    /// @copydoc command::IComputeContext::setTrackedAccess()
    virtual void setTrackedAccess(const sync::TrackedAccessInfo& info) noexcept override;

    /// @copydoc command::IComputeContext::beginUAVOverlap()
    virtual void beginUAVOverlap() noexcept override;

    /// @copydoc command::IComputeContext::endUAVOverlap()
    virtual void endUAVOverlap() noexcept override;

    /// @copydoc command::IComputeContext::beginUAVOverlap(gp::ConstVectorView<view::UnorderedAccessView*>)
    virtual void beginUAVOverlap(gp::ConstVectorView<view::UnorderedAccessView*> uavs) noexcept override;

    /// @copydoc command::IComputeContext::endUAVOverlap(gp::ConstVectorView<view::UnorderedAccessView*>)
    virtual void endUAVOverlap(gp::ConstVectorView<view::UnorderedAccessView*> uavs) noexcept override;

#if GP_WITH_RHI_BREADCRUMBS
    /// @copydoc command::IComputeContext::beginBreadcrumbGPU()
    virtual void beginBreadcrumbGPU(diagnostics::BreadcrumbNode* node) noexcept override;

    /// @copydoc command::IComputeContext::endBreadcrumbGPU()
    virtual void endBreadcrumbGPU(diagnostics::BreadcrumbNode* node) noexcept override;
#endif

    /// @copydoc command::IComputeContext::shouldEmitBreadcrumbs()
    [[nodiscard]] virtual bool shouldEmitBreadcrumbs() const noexcept override
    {
        return m_innerContext->shouldEmitBreadcrumbs();
    }

    /// @brief Resets transient per-command-list binding state on this validation context.
    void resetContextState() noexcept;

    /// @brief Returns the underlying backend `IComputeContext*`.
    /// @return Non-null backend context pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT command::IComputeContext* getInnerContext() const noexcept
    {
        return m_innerContext;
    }

    /// @brief Returns this context's `validation::Tracker`.
    /// @return Mutable reference to `m_stateTracker`.
    [[nodiscard]] GP_FORCEINLINE_HINT validation::Tracker& getStateTracker() noexcept
    {
        return m_stateTracker;
    }

private:
    ValidationRHI& m_rhi;
    command::IComputeContext* m_innerContext{ nullptr };
    validation::Tracker m_stateTracker;
    validation::StaticUniformBuffers m_staticUniformBuffers{};
    validation::BoundUniformBuffers m_boundUniformBuffers{};
    pipeline::ComputePipelineState* m_boundComputePSO{ nullptr };
};

/// @brief Validation proxy wrapping an `ICommandContext` to audit render passes, graphics/compute PSOs, draws, copies,
///        barriers, and transient aliasing.
class GP_RHI_API ValidationContext final : public command::ICommandContext
{
    friend class ValidationRHI;

public:
    /// @brief Constructs a graphics+compute validation wrapper around `innerContext` owned by `validationRHI`.
    /// @param[in] validationRHI Owning `ValidationRHI` device wrapper.
    /// @param[in] innerContext Non-null concrete backend `ICommandContext`.
    ValidationContext(ValidationRHI& validationRHI, command::ICommandContext* innerContext) noexcept;

    /// @copydoc command::ICommandContext::~ICommandContext()
    virtual ~ValidationContext() noexcept override;

public:
    /// @copydoc command::ICommandContext::getPipeline()
    [[nodiscard]] virtual sync::PipelineStage getPipeline() const noexcept override
    {
        return sync::PipelineStage::Graphics;
    }

    /// @copydoc command::IComputeContext::getLowestLevelContext()
    [[nodiscard]] virtual command::IComputeContext& getLowestLevelContext() noexcept override
    {
        return m_innerContext->getLowestLevelContext();
    }

    /// @copydoc command::IComputeContext::setExecutingCommandList()
    virtual void setExecutingCommandList(command::CommandListBase* cmdList) noexcept override;

    /// @copydoc command::IComputeContext::setComputePipelineState()
    virtual void setComputePipelineState(pipeline::ComputePipelineState* computePipelineState) noexcept override;

    /// @copydoc command::IComputeContext::dispatchComputeShader()
    virtual void dispatchComputeShader(
        gp::UInt32 threadGroupCountX, gp::UInt32 threadGroupCountY, gp::UInt32 threadGroupCountZ
    ) noexcept override;

    /// @copydoc command::IComputeContext::dispatchIndirectComputeShader()
    virtual void
        dispatchIndirectComputeShader(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept override;

    /// @copydoc command::IComputeContext::setAsyncComputeBudget()
    virtual void setAsyncComputeBudget(command::AsyncComputeBudget budget) noexcept override;

    /// @copydoc command::IComputeContext::setComputeBudget()
    virtual void setComputeBudget(command::SyncComputeBudget budget) noexcept override;

    /// @copydoc command::IComputeContext::setLowLatencyHint()
    virtual void setLowLatencyHint(bool lowLatency) noexcept override;

    /// @copydoc command::IComputeContext::beginTransitions()
    virtual void beginTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept override;

    /// @copydoc command::IComputeContext::endTransitions()
    virtual void endTransitions(gp::ConstVectorView<const sync::Transition*> transitions) noexcept override;

    /// @copydoc command::IComputeContext::clearUAVFloat()
    virtual void clearUAVFloat(
        view::UnorderedAccessView* unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
    ) noexcept override;

    /// @copydoc command::IComputeContext::clearUAVUint()
    virtual void clearUAVUint(
        view::UnorderedAccessView* unorderedAccessView, const resource::UIntVector4& values
    ) noexcept override;

    /// @copydoc command::IComputeContext::setShaderRootConstants()
    virtual void setShaderRootConstants(const resource::UIntVector4& constants) noexcept override;

    /// @copydoc command::IComputeContext::setShaderParameters()
    virtual void setShaderParameters(
        shader::ComputeShader* shader,
        gp::ConstVectorView<gp::UInt8> inParametersData,
        gp::ConstVectorView<shader::ShaderParameter> inParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
    ) noexcept override;

    /// @copydoc command::ICommandContext::setShaderParameters()
    virtual void setShaderParameters(
        shader::GraphicsShader* shader,
        gp::ConstVectorView<gp::UInt8> inParametersData,
        gp::ConstVectorView<shader::ShaderParameter> inParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
        gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
    ) noexcept override;

    /// @copydoc command::IComputeContext::setShaderUnbinds()
    virtual void setShaderUnbinds(
        shader::ComputeShader* shader, gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept override;

    /// @copydoc command::ICommandContext::setShaderUnbinds()
    virtual void setShaderUnbinds(
        shader::GraphicsShader* shader, gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
    ) noexcept override;

    /// @copydoc command::IComputeContext::setStaticUniformBuffers()
    virtual void setStaticUniformBuffers(const shader::UniformBufferStaticBindings& uniformBuffers) noexcept override;

    /// @copydoc command::IComputeContext::setStaticUniformBuffer()
    virtual void setStaticUniformBuffer(
        resource::UniformBufferStaticSlot slot, resource::UniformBuffer* uniformBuffer
    ) noexcept override;

    /// @copydoc command::IComputeContext::setUniformBlockOffset()
    virtual void setUniformBlockOffset(view::ShaderResourceView* srv, gp::UInt32 offset) noexcept override;

    /// @copydoc command::IComputeContext::copyToStagingBuffer()
    virtual void copyToStagingBuffer(
        resource::Buffer* sourceBuffer,
        resource::StagingBuffer* destinationStagingBuffer,
        gp::UInt32 offset,
        gp::UInt32 numBytes
    ) noexcept override;

    /// @copydoc command::IComputeContext::writeGPUFence()
    virtual void writeGPUFence(sync::GPUFence* fence) noexcept override;

    /// @copydoc command::IComputeContext::setGPUMask()
    virtual void setGPUMask(GPUMask gpuMask) noexcept override;

    /// @copydoc command::IComputeContext::postExternalCommandsReset()
    virtual void postExternalCommandsReset() noexcept override;

    /// @copydoc command::IComputeContext::setTrackedAccess()
    virtual void setTrackedAccess(const sync::TrackedAccessInfo& info) noexcept override;

    /// @copydoc command::IComputeContext::beginUAVOverlap()
    virtual void beginUAVOverlap() noexcept override;

    /// @copydoc command::IComputeContext::endUAVOverlap()
    virtual void endUAVOverlap() noexcept override;

    /// @copydoc command::IComputeContext::beginUAVOverlap(gp::ConstVectorView<view::UnorderedAccessView*>)
    virtual void beginUAVOverlap(gp::ConstVectorView<view::UnorderedAccessView*> uavs) noexcept override;

    /// @copydoc command::IComputeContext::endUAVOverlap(gp::ConstVectorView<view::UnorderedAccessView*>)
    virtual void endUAVOverlap(gp::ConstVectorView<view::UnorderedAccessView*> uavs) noexcept override;

#if GP_WITH_RHI_BREADCRUMBS
    /// @copydoc command::IComputeContext::beginBreadcrumbGPU()
    virtual void beginBreadcrumbGPU(diagnostics::BreadcrumbNode* node) noexcept override;

    /// @copydoc command::IComputeContext::endBreadcrumbGPU()
    virtual void endBreadcrumbGPU(diagnostics::BreadcrumbNode* node) noexcept override;
#endif

    /// @copydoc command::IComputeContext::shouldEmitBreadcrumbs()
    [[nodiscard]] virtual bool shouldEmitBreadcrumbs() const noexcept override
    {
        return m_innerContext->shouldEmitBreadcrumbs();
    }

    /// @copydoc command::ICommandContext::setMultipleViewports()
    virtual void setMultipleViewports(gp::UInt32 count, const command::ViewportBounds* data) noexcept override;

    /// @copydoc command::ICommandContext::resummarizeHTile()
    virtual void resummarizeHTile(resource::Texture* depthTexture) noexcept override;

    /// @copydoc command::ICommandContext::beginRenderQuery()
    virtual void beginRenderQuery(sync::RenderQuery* renderQuery) noexcept override;

    /// @copydoc command::ICommandContext::endRenderQuery()
    virtual void endRenderQuery(sync::RenderQuery* renderQuery) noexcept override;

    /// @copydoc command::ICommandContext::setStreamSource()
    virtual void
        setStreamSource(gp::UInt32 streamIndex, resource::Buffer* vertexBuffer, gp::UInt32 offset) noexcept override;

    /// @copydoc command::ICommandContext::setViewport()
    virtual void setViewport(
        gp::Float32 minX, gp::Float32 minY, gp::Float32 minZ, gp::Float32 maxX, gp::Float32 maxY, gp::Float32 maxZ
    ) noexcept override;

    /// @copydoc command::ICommandContext::setStereoViewport()
    virtual void setStereoViewport(
        gp::Float32 leftMinX,
        gp::Float32 rightMinX,
        gp::Float32 leftMinY,
        gp::Float32 rightMinY,
        gp::Float32 minZ,
        gp::Float32 leftMaxX,
        gp::Float32 rightMaxX,
        gp::Float32 leftMaxY,
        gp::Float32 rightMaxY,
        gp::Float32 maxZ
    ) noexcept override;

    /// @copydoc command::ICommandContext::setScissorRect()
    virtual void setScissorRect(
        bool enable, gp::UInt32 minX, gp::UInt32 minY, gp::UInt32 maxX, gp::UInt32 maxY
    ) noexcept override;

    /// @copydoc command::ICommandContext::setGraphicsPipelineState()
    virtual void setGraphicsPipelineState(
        pipeline::GraphicsPipelineState* graphicsState, gp::UInt32 stencilRef, bool applyAdditionalState
    ) noexcept override;

    /// @copydoc command::ICommandContext::setStencilRef()
    virtual void setStencilRef(gp::UInt32 stencilRef) noexcept override;

    /// @copydoc command::ICommandContext::setBlendFactor()
    virtual void setBlendFactor(const gp::math::LinearColor& blendFactor) noexcept override;

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

    /// @copydoc command::ICommandContext::multiDrawIndexedPrimitiveIndirect()
    virtual void multiDrawIndexedPrimitiveIndirect(
        resource::Buffer* indexBuffer,
        resource::Buffer* argumentBuffer,
        gp::UInt32 argumentOffset,
        resource::Buffer* countBuffer,
        gp::UInt32 countBufferOffset,
        gp::UInt32 maxDrawArguments,
        gp::UInt32 minMaterialIndex,
        gp::UInt32 maxMaterialIndex
    ) noexcept override;

    /// @copydoc command::ICommandContext::dispatchMeshShader()
    virtual void dispatchMeshShader(
        gp::UInt32 threadGroupCountX, gp::UInt32 threadGroupCountY, gp::UInt32 threadGroupCountZ
    ) noexcept override;

    /// @copydoc command::ICommandContext::dispatchIndirectMeshShader()
    virtual void
        dispatchIndirectMeshShader(resource::Buffer* argumentBuffer, gp::UInt32 argumentOffset) noexcept override;

    /// @copydoc command::ICommandContext::setDepthBounds()
    virtual void setDepthBounds(gp::Float32 minDepth, gp::Float32 maxDepth) noexcept override;

    /// @copydoc command::ICommandContext::setShadingRate()
    virtual void
        setShadingRate(pipeline::VRSShadingRate shadingRate, pipeline::VRSRateCombiner combiner) noexcept override;

    /// @copydoc command::ICommandContext::beginRenderPass()
    virtual void beginRenderPass(const command::RenderPassInfo& inInfo, const char* name) noexcept override;

    /// @copydoc command::ICommandContext::endRenderPass()
    virtual void endRenderPass() noexcept override;

    /// @copydoc command::ICommandContext::nextSubpass()
    virtual void nextSubpass() noexcept override;

    /// @copydoc command::ICommandContext::copyTexture()
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

    /// @brief Resets cached PSO, vertex stream, render pass, and UB bindings at command list boundaries.
    void resetContextState() noexcept;

    /// @brief Returns the wrapped backend `ICommandContext*`.
    /// @return Non-null backend graphics context pointer.
    [[nodiscard]] GP_FORCEINLINE_HINT command::ICommandContext* getInnerContext() const noexcept
    {
        return m_innerContext;
    }

    /// @brief Returns this context's `validation::Tracker`.
    /// @return Mutable reference to `m_stateTracker`.
    [[nodiscard]] GP_FORCEINLINE_HINT validation::Tracker& getStateTracker() noexcept
    {
        return m_stateTracker;
    }

private:
    void validateDrawing() noexcept;

private:
    struct VertexStreamBinding
    {
        resource::Buffer* buffer{ nullptr };
        gp::UInt32 offset{ 0u };
    };

    ValidationRHI& m_rhi;
    command::ICommandContext* m_innerContext{ nullptr };
    validation::Tracker m_stateTracker;
    validation::StaticUniformBuffers m_staticUniformBuffers{};
    validation::BoundUniformBuffers m_boundUniformBuffers{};
    VertexStreamBinding m_vertexStreams[kMaxVertexElementCount]{};
    pipeline::GraphicsPipelineState* m_boundGraphicsPSO{ nullptr };
    pipeline::ComputePipelineState* m_boundComputePSO{ nullptr };
    bool m_insideRenderPass{ false };
};

}   // namespace gp::rhi
