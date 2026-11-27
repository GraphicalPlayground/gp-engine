// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanCommandListContext.hpp"
#include "barriers/VulkanBarrierContext.hpp"
#include "barriers/VulkanBarrierSettings.hpp"
#include "barriers/VulkanBarrierTypes.hpp"
#include "barriers/VulkanCommandBufferBarrierRecorder.hpp"
#include "barriers/VulkanPipelineBarrier.hpp"
#include "barriers/VulkanTransitionProcessor.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "commands/VulkanCommandEnums.hpp"
#include "commands/VulkanSubmission.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "contexts/VulkanCopyCommands.hpp"
#include "contexts/VulkanDrawUtils.hpp"
#include "contexts/VulkanResourceTable.hpp"
#include "CoreMinimal.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "maths/base/Arithmetic.hpp"
#include "memory/VulkanAllocation.hpp"
#include "memory/VulkanTempBlockAllocator.hpp"
#include "memory/VulkanUniformBufferMemory.hpp"
#include "pendingstate/VulkanCommonPipelineDescriptorState.hpp"
#include "pendingstate/VulkanComputePipelineDescriptorState.hpp"
#include "pendingstate/VulkanDescriptorPoolSetAcquirer.hpp"
#include "pendingstate/VulkanGraphicsPipelineDescriptorState.hpp"
#include "pendingstate/VulkanPendingComputeState.hpp"
#include "pendingstate/VulkanPendingGfxState.hpp"
#include "pipeline/VulkanComputePipelineState.hpp"
#include "pipeline/VulkanGraphicsPipelineState.hpp"
#include "pipeline/VulkanWrappedPipelineStates.hpp"
#include "queries/VulkanQueryRecording.hpp"
#include "queries/VulkanRenderQuery.hpp"
#include "renderpass/VulkanFramebuffer.hpp"
#include "renderpass/VulkanRenderPassManager.hpp"
#include "renderpass/VulkanRenderTargetLayout.hpp"
#include "resources/VulkanBuffer.hpp"
#include "resources/VulkanCommandBufferTransferRecorder.hpp"
#include "resources/VulkanReadbackStagingBuffer.hpp"
#include "resources/VulkanTexture.hpp"
#include "resources/VulkanTextureTypes.hpp"
#include "resources/VulkanUniformBuffer.hpp"
#include "shaders/ShaderData.hpp"
#include "shaders/ShaderEnums.hpp"
#include "states/VulkanSamplerState.hpp"
#include "sync/SyncEnums.hpp"
#include "views/ShaderResourceView.hpp"
#include "views/UnorderedAccessView.hpp"
#include "views/VulkanShaderResourceView.hpp"
#include "views/VulkanUnorderedAccessView.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanCommon.hpp"
#include <bit>

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Number of graphics stages the context tracks.
constexpr gp::USize kNumGraphicsStages = static_cast<gp::USize>(shader_stage::Stage::NumGraphicsStages);

/// @brief Frequency of each graphics stage, indexed by `shader_stage::Stage`.
constexpr gp::Array<shader::ShaderFrequency, kNumGraphicsStages> kGraphicsStageFrequencies{
    shader::ShaderFrequency::Vertex,
    shader::ShaderFrequency::Pixel,
    shader::ShaderFrequency::Geometry,
    shader::ShaderFrequency::Mesh,
    shader::ShaderFrequency::Amplification
};

/// @brief Descriptor defaults used when the services provide none: every binding must then be explicit.
const VulkanDescriptorDefaults kEmptyDescriptorDefaults{};

/// @brief Binds the resources of a uniform buffer resource table into one stage of a descriptor state.
/// @details Adapter between the generic resource table walk and the Vulkan descriptor state. Texture layouts are the
///          default read layouts the barrier system leaves sampled images in, so the descriptor always agrees with the
///          transitions the render graph issued.
class VulkanStageDescriptorBinder final
{
public:
    /// @brief Creates a binder writing into `stage` of `descriptorState`.
    /// @param[in,out] descriptorState Destination descriptor state.
    /// @param[in] stage Destination stage.
    /// @param[in] readAccess Access used to pick image layouts (`SRVGraphics` or `SRVCompute`).
    /// @param[in] device Barrier device snapshot.
    /// @param[in] translation Barrier translation inputs.
    VulkanStageDescriptorBinder(
        VulkanCommonPipelineDescriptorState& descriptorState,
        const shader_stage::Stage stage,
        const sync::ResourceAccess readAccess,
        const VulkanBarrierDeviceDesc& device,
        const VulkanBarrierTranslationDesc& translation
    ) noexcept
        : m_state(descriptorState)
        , m_stage(stage)
        , m_readAccess(readAccess)
        , m_device(device)
        , m_translation(translation)
    {}

    /// @brief Binds the default view of `texture`.
    /// @param[in] bindIndex Binding slot.
    /// @param[in] texture Texture; null is ignored.
    void setTexture(const gp::UInt32 bindIndex, resource::Texture* const texture) noexcept
    {
        if (texture == nullptr)
        {
            return;
        }
        const VulkanTexture& vulkanTexture = *static_cast<const VulkanTexture*>(texture);
        const VulkanTextureView view{ vulkanTexture.getDefaultView(), vulkanTexture.getImage(), 0u };
        static_cast<void>(m_state.setTextureView(m_stage, bindIndex, view, getReadLayout(vulkanTexture)));
    }

    /// @brief Binds a shader resource view.
    /// @param[in] bindIndex Binding slot.
    /// @param[in] srv View; null is ignored.
    void setSRV(const gp::UInt32 bindIndex, view::ShaderResourceView* const srv) noexcept
    {
        if (srv == nullptr)
        {
            return;
        }
        VulkanShaderResourceView& vulkanSrv = *static_cast<VulkanShaderResourceView*>(srv);
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (srv->isTexture() && (srv->getTexture() != nullptr))
        {
            layout = getReadLayout(*static_cast<const VulkanTexture*>(srv->getTexture()));
        }
        static_cast<void>(m_state.setShaderResourceView(m_stage, bindIndex, vulkanSrv, layout));
    }

    /// @brief Binds an unordered access view.
    /// @param[in] bindIndex Binding slot.
    /// @param[in] uav View; null is ignored.
    void setUAV(const gp::UInt32 bindIndex, view::UnorderedAccessView* const uav) noexcept
    {
        if (uav == nullptr)
        {
            return;
        }
        static_cast<void>(
            m_state.setUnorderedAccessView(m_stage, bindIndex, *static_cast<VulkanUnorderedAccessView*>(uav))
        );
    }

    /// @brief Binds a sampler.
    /// @param[in] bindIndex Binding slot.
    /// @param[in] sampler Sampler; null is ignored.
    void setSampler(const gp::UInt32 bindIndex, pipeline::SamplerState* const sampler) noexcept
    {
        if (sampler == nullptr)
        {
            return;
        }
        static_cast<void>(m_state.setSampler(m_stage, bindIndex, *static_cast<const VulkanSamplerState*>(sampler)));
    }

private:
    /// @brief Returns the layout a sampled read of `texture` happens in.
    /// @param[in] texture Texture.
    /// @return Default read layout.
    [[nodiscard]] VkImageLayout getReadLayout(const VulkanTexture& texture) const noexcept
    {
        return VulkanPipelineBarrier::getDefaultLayout(
            VulkanBarrierResourceResolver::resolveTexture(texture),
            m_readAccess,
            m_device.supportsParallelRendering,
            m_translation.shadingRateImageType
        );
    }

private:
    VulkanCommonPipelineDescriptorState& m_state;
    shader_stage::Stage m_stage;
    sync::ResourceAccess m_readAccess;
    const VulkanBarrierDeviceDesc& m_device;
    const VulkanBarrierTranslationDesc& m_translation;
};

/// @brief Returns the access used to pick read layouts for a pipeline family.
/// @param[in] isCompute True for compute.
/// @return `SRVCompute` or `SRVGraphics`.
[[nodiscard]] sync::ResourceAccess getReadAccess(const bool isCompute) noexcept
{
    return isCompute ? sync::ResourceAccess::SRVCompute : sync::ResourceAccess::SRVGraphics;
}

/// @brief Builds the subresource range a texture UAV covers.
/// @param[in] uav Texture UAV.
/// @param[in] texture Viewed texture.
/// @return Range of the view; zero counts mean "the rest of the resource".
[[nodiscard]] VkImageSubresourceRange
    getUAVSubresourceRange(const view::UnorderedAccessView& uav, const VulkanTexture& texture) noexcept
{
    const view::ViewDesc& desc = uav.getDesc();
    VkImageSubresourceRange range{};
    range.aspectMask = texture.getFullAspectMask();
    range.baseMipLevel = desc.texture.uav.mipRange.first;
    range.levelCount = (desc.texture.uav.mipRange.num == 0u) ? VK_REMAINING_MIP_LEVELS
                                                             : static_cast<gp::UInt32>(desc.texture.uav.mipRange.num);
    range.baseArrayLayer = desc.texture.uav.arrayRange.first;
    range.layerCount = (desc.texture.uav.arrayRange.num == 0u)
                           ? VK_REMAINING_ARRAY_LAYERS
                           : static_cast<gp::UInt32>(desc.texture.uav.arrayRange.num);
    return range;
}

}   // namespace

// --- Lifecycle -------------------------------------------------------------------------------------------------------

VulkanCommandListContext::VulkanCommandListContext(const VulkanContextServices& services) noexcept
    : VulkanContextCommon(services, VulkanCommandBufferType::Primary)
    , m_barrierDevice(makeVulkanBarrierDeviceDesc(*services.device))
    , m_barrierTranslation(makeVulkanBarrierTranslationDesc(*services.device))
    , m_descriptorDefaults(
          (services.descriptorDefaults != nullptr) ? *services.descriptorDefaults : kEmptyDescriptorDefaults
      )
{
    if (services.descriptorPoolsManager != nullptr)
    {
        m_poolSetAcquirer = gp::makeUnique<VulkanDescriptorPoolSetAcquirer>(*services.descriptorPoolsManager);
    }
    m_gfxState = gp::makeUnique<VulkanPendingGfxState>(*services.device, *this, m_descriptorDefaults);
    m_computeState = gp::makeUnique<VulkanPendingComputeState>(*services.device, *this, m_descriptorDefaults);
    for (VulkanStageUniformBuffers& stage: m_graphicsUniformBuffers)
    {
        stage.reset();
    }
    m_computeUniformBuffers.reset();
}

VulkanCommandListContext::~VulkanCommandListContext() noexcept
{
    m_transferRecorder.reset();
    m_transferRecorderOwner = nullptr;
    if (m_poolSetAcquirer)
    {
        m_poolSetAcquirer->release();
    }
}

void VulkanCommandListContext::endActiveRenderPass(VulkanCommandBuffer& commandBuffer) noexcept
{
    if (m_isDynamicRenderingActive)
    {
        static_cast<void>(commandBuffer.endDynamicRendering());
        m_isDynamicRenderingActive = false;
    }
    else
    {
        VulkanContextCommon::endActiveRenderPass(commandBuffer);
    }
    m_isInsideRenderPass = false;
    m_currentFramebuffer = nullptr;
    m_currentRenderPass = nullptr;
}

void VulkanCommandListContext::onCommandBufferBegun(VulkanCommandBuffer& commandBuffer) noexcept
{
    static_cast<void>(commandBuffer);
    if (m_gfxState->getCurrentState() != nullptr)
    {
        m_gfxState->getCurrentState()->markDirty();
    }
    if (m_computeState->getCurrentState() != nullptr)
    {
        m_computeState->getCurrentState()->markDirty();
    }
}

void VulkanCommandListContext::onFinalize(VulkanPayload& signalPayload) noexcept
{
    m_transferRecorder.reset();
    m_transferRecorderOwner = nullptr;

    if (m_poolSetAcquirer && m_poolSetAcquirer->hasPoolSetContainer())
    {
        // The sets were written into command buffers of this batch; the pools may only be recycled once the GPU
        // has executed them, which the signal payload knows.
        signalPayload.descriptorPoolSetContainers.pushBack(m_poolSetAcquirer->detachPoolSetContainer());
        signalPayload.descriptorPoolsManager = getServices().descriptorPoolsManager;
    }

    // Sets allocated so far now belong to the payload: force fresh ones on the next draw.
    if (m_gfxState->getCurrentState() != nullptr)
    {
        m_gfxState->getCurrentState()->markDirty();
    }
    if (m_computeState->getCurrentState() != nullptr)
    {
        m_computeState->getCurrentState()->markDirty();
    }
}

IVulkanTransferCommandRecorder* VulkanCommandListContext::getTransferRecorder() noexcept
{
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if (commandBuffer == nullptr)
    {
        return nullptr;
    }
    if (!m_transferRecorder || (m_transferRecorderOwner != commandBuffer))
    {
        m_transferRecorder = gp::makeUnique<VulkanCommandBufferTransferRecorder>(*commandBuffer, getContextSyncPoint());
        m_transferRecorderOwner = commandBuffer;
    }
    return m_transferRecorder.get();
}

const VulkanStageUniformBuffers&
    VulkanCommandListContext::getGraphicsStageUniformBuffers(const shader_stage::Stage stage) const noexcept
{
    const gp::USize index = static_cast<gp::USize>(stage);
    GP_CHECK(index < kNumGraphicsStages);
    return m_graphicsUniformBuffers[index];
}

VulkanStageUniformBuffers&
    VulkanCommandListContext::getStageUniformBuffers(const bool isCompute, const shader_stage::Stage stage) noexcept
{
    if (isCompute)
    {
        return m_computeUniformBuffers;
    }
    const gp::USize index = static_cast<gp::USize>(stage);
    GP_CHECK(index < kNumGraphicsStages);
    return m_graphicsUniformBuffers[index];
}

// --- Shader parameters -----------------------------------------------------------------------------------------------

void VulkanCommandListContext::bindUniformBuffer(
    VulkanCommonPipelineDescriptorState& descriptorState,
    VulkanStageUniformBuffers& stageBuffers,
    const shader_stage::Stage stage,
    const gp::UInt32 bufferIndex,
    resource::UniformBuffer* const uniformBuffer
) noexcept
{
    if (bufferIndex >= kVulkanMaxBoundUniformBuffersPerStage)
    {
        GP_LOG_ERROR(LogVulkanRHI, "Uniform buffer index {} exceeds the per-stage limit.", bufferIndex);
        return;
    }

    stageBuffers.buffers[bufferIndex] = uniformBuffer;
    if (uniformBuffer == nullptr)
    {
        stageBuffers.dirtyBits &= ~(1u << bufferIndex);
        return;
    }
    stageBuffers.dirtyBits |= (1u << bufferIndex);

    // Uniform buffers occupy the first bindings of a stage, so the shader-local index is the binding index.
    const VulkanUniformBuffer& vulkanBuffer = *static_cast<const VulkanUniformBuffer*>(uniformBuffer);
    const gp::UInt32 size = uniformBuffer->getLayout().getConstantBufferSize();
    if ((size > 0u) && (vulkanBuffer.getBufferHandle() != VK_NULL_HANDLE))
    {
        static_cast<void>(descriptorState.setUniformBuffer(
            stage,
            bufferIndex,
            vulkanBuffer.getBufferHandle(),
            vulkanBuffer.getAllocation().handleId,
            vulkanBuffer.getOffset(),
            size
        ));
    }
}

void VulkanCommandListContext::applyStaticUniformBuffers(
    VulkanCommonPipelineDescriptorState& descriptorState,
    VulkanStageUniformBuffers& stageBuffers,
    const shader_stage::Stage stage,
    const shader::ShaderData* const shader
) noexcept
{
    if (shader == nullptr)
    {
        return;
    }
    static_cast<void>(applyVulkanStaticUniformBuffers(
        *shader,
        m_staticUniformBuffers,
        [this, &descriptorState, &stageBuffers, stage](
            const gp::UInt32 bufferIndex, resource::UniformBuffer* const uniformBuffer
        ) -> void
    {
        bindUniformBuffer(descriptorState, stageBuffers, stage, bufferIndex, uniformBuffer);
    }
    ));
}

void VulkanCommandListContext::applyShaderParameters(
    VulkanCommonPipelineDescriptorState& descriptorState,
    VulkanStageUniformBuffers& stageBuffers,
    const shader_stage::Stage stage,
    const bool isCompute,
    const gp::ConstVectorView<gp::UInt8> parametersData,
    const gp::ConstVectorView<shader::ShaderParameter> parameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> resourceParameters
) noexcept
{
    // Loose parameters live in buffer 0, the packed globals buffer uploaded at draw time.
    for (const shader::ShaderParameter& parameter: parameters)
    {
        const gp::USize end = static_cast<gp::USize>(parameter.byteOffset) + parameter.byteSize;
        if ((parameter.bufferIndex != 0u) || (end > parametersData.size()))
        {
            continue;
        }
        descriptorState.setPackedGlobalParameter(
            stage, parameter.baseIndex, parameter.byteSize, parametersData.data() + parameter.byteOffset
        );
    }

    VulkanStageDescriptorBinder binder(
        descriptorState, stage, getReadAccess(isCompute), m_barrierDevice, m_barrierTranslation
    );
    for (const shader::ShaderParameterResource& parameter: resourceParameters)
    {
        const gp::UInt32 index = parameter.index;
        switch (parameter.type)
        {
        case shader::ShaderParameterResource::Type::Texture:
            binder.setTexture(index, static_cast<resource::Texture*>(parameter.resource));
            break;
        case shader::ShaderParameterResource::Type::ResourceView:
            binder.setSRV(index, static_cast<view::ShaderResourceView*>(parameter.resource));
            break;
        case shader::ShaderParameterResource::Type::UnorderedAccessView:
            binder.setUAV(index, static_cast<view::UnorderedAccessView*>(parameter.resource));
            break;
        case shader::ShaderParameterResource::Type::Sampler:
            binder.setSampler(index, static_cast<pipeline::SamplerState*>(parameter.resource));
            break;
        case shader::ShaderParameterResource::Type::UniformBuffer:
            bindUniformBuffer(
                descriptorState, stageBuffers, stage, index, static_cast<resource::UniformBuffer*>(parameter.resource)
            );
            break;
        case shader::ShaderParameterResource::Type::ResourceCollection:
        case shader::ShaderParameterResource::Type::DescriptorRange:
            // Bindless resources are not supported by this backend yet.
            break;
        default:
            break;
        }
    }
}

void VulkanCommandListContext::setShaderParameters(
    shader::ComputeShader* const shader,
    const gp::ConstVectorView<gp::UInt8> inParametersData,
    const gp::ConstVectorView<shader::ShaderParameter> inParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
) noexcept
{
    static_cast<void>(inBindlessParameters);
    VulkanComputePipelineDescriptorState* const state = m_computeState->getCurrentState();
    if (state == nullptr)
    {
        GP_LOG_WARNING(LogVulkanRHI, "setShaderParameters(compute) called without a compute pipeline bound.");
        return;
    }
    if (shader != nullptr)
    {
        m_computeUniformBuffers.shader = shader;
    }
    applyShaderParameters(
        *state,
        m_computeUniformBuffers,
        shader_stage::Stage::Compute,
        true,
        inParametersData,
        inParameters,
        inResourceParameters
    );
}

void VulkanCommandListContext::setShaderParameters(
    shader::GraphicsShader* const shader,
    const gp::ConstVectorView<gp::UInt8> inParametersData,
    const gp::ConstVectorView<shader::ShaderParameter> inParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inResourceParameters,
    const gp::ConstVectorView<shader::ShaderParameterResource> inBindlessParameters
) noexcept
{
    static_cast<void>(inBindlessParameters);
    VulkanGraphicsPipelineDescriptorState* const state = m_gfxState->getCurrentState();
    if ((state == nullptr) || (shader == nullptr))
    {
        GP_LOG_WARNING(LogVulkanRHI, "setShaderParameters(graphics) called without a graphics pipeline or shader.");
        return;
    }
    const shader_stage::Stage stage = shader_stage::getStageForFrequency(shader->getFrequency());
    if (static_cast<gp::USize>(stage) >= kNumGraphicsStages)
    {
        GP_LOG_ERROR(LogVulkanRHI, "setShaderParameters(graphics) received a non-graphics shader.");
        return;
    }
    VulkanStageUniformBuffers& stageBuffers = getStageUniformBuffers(false, stage);
    stageBuffers.shader = shader;
    applyShaderParameters(*state, stageBuffers, stage, false, inParametersData, inParameters, inResourceParameters);
}

void VulkanCommandListContext::setShaderUnbinds(
    shader::ComputeShader* const shader, const gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
) noexcept
{
    static_cast<void>(shader);
    static_cast<void>(inUnbinds);
}

void VulkanCommandListContext::setShaderUnbinds(
    shader::GraphicsShader* const shader, const gp::ConstVectorView<shader::ShaderParameterUnbind> inUnbinds
) noexcept
{
    static_cast<void>(shader);
    static_cast<void>(inUnbinds);
}

void VulkanCommandListContext::setStaticUniformBuffers(
    const shader::UniformBufferStaticBindings& uniformBuffers
) noexcept
{
    uniformBuffers.bind(m_staticUniformBuffers);
}

void VulkanCommandListContext::setUniformBlockOffset(
    view::ShaderResourceView* const srv, const gp::UInt32 offset
) noexcept
{
    static_cast<void>(srv);
    m_uniformBlockOffset = offset;
}

void VulkanCommandListContext::commitGraphicsResourceTables() noexcept
{
    VulkanGraphicsPipelineDescriptorState* const state = m_gfxState->getCurrentState();
    if (state == nullptr)
    {
        return;
    }
    for (gp::USize index = 0u; index < kNumGraphicsStages; ++index)
    {
        VulkanStageUniformBuffers& stageBuffers = m_graphicsUniformBuffers[index];
        if ((stageBuffers.shader == nullptr) || (stageBuffers.dirtyBits == 0u))
        {
            continue;
        }
        const shader_stage::Stage stage = static_cast<shader_stage::Stage>(index);
        VulkanStageDescriptorBinder binder(*state, stage, getReadAccess(false), m_barrierDevice, m_barrierTranslation);
        static_cast<void>(setVulkanUniformBufferResourcesFromTables(
            binder, *stageBuffers.shader, stageBuffers.buffers, stageBuffers.dirtyBits
        ));
    }
}

void VulkanCommandListContext::commitComputeResourceTables() noexcept
{
    VulkanComputePipelineDescriptorState* const state = m_computeState->getCurrentState();
    if ((state == nullptr) || (m_computeUniformBuffers.shader == nullptr) || (m_computeUniformBuffers.dirtyBits == 0u))
    {
        return;
    }
    VulkanStageDescriptorBinder binder(
        *state, shader_stage::Stage::Compute, getReadAccess(true), m_barrierDevice, m_barrierTranslation
    );
    static_cast<void>(setVulkanUniformBufferResourcesFromTables(
        binder, *m_computeUniformBuffers.shader, m_computeUniformBuffers.buffers, m_computeUniformBuffers.dirtyBits
    ));
}

// --- Compute ---------------------------------------------------------------------------------------------------------

void VulkanCommandListContext::setComputePipelineState(
    pipeline::ComputePipelineState* const computePipelineState
) noexcept
{
    if (computePipelineState == nullptr)
    {
        return;
    }
    const VulkanComputePipelineState* const pipeline =
        static_cast<VulkanWrappedComputePipelineState*>(computePipelineState)->getState();
    if (pipeline == nullptr)
    {
        return;
    }

    m_computeState->setComputePipeline(*pipeline);
    m_computeUniformBuffers.reset();
    m_computeUniformBuffers.shader = pipeline->getComputeShader();

    VulkanComputePipelineDescriptorState* const state = m_computeState->getCurrentState();
    if (state != nullptr)
    {
        applyStaticUniformBuffers(
            *state, m_computeUniformBuffers, shader_stage::Stage::Compute, m_computeUniformBuffers.shader
        );
    }
}

void VulkanCommandListContext::dispatchComputeShader(
    const gp::UInt32 threadGroupCountX, const gp::UInt32 threadGroupCountY, const gp::UInt32 threadGroupCountZ
) noexcept
{
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if (commandBuffer == nullptr)
    {
        return;
    }
    commitComputeResourceTables();
    if (!m_computeState->prepareForDispatch(*commandBuffer))
    {
        return;
    }
    vkCmdDispatch(commandBuffer->getHandle(), threadGroupCountX, threadGroupCountY, threadGroupCountZ);
    recordDispatchStats();
}

void VulkanCommandListContext::dispatchIndirectComputeShader(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if ((commandBuffer == nullptr) || (argumentBuffer == nullptr))
    {
        return;
    }
    commitComputeResourceTables();
    if (!m_computeState->prepareForDispatch(*commandBuffer))
    {
        return;
    }
    const VulkanBuffer& buffer = *static_cast<const VulkanBuffer*>(argumentBuffer);
    vkCmdDispatchIndirect(
        commandBuffer->getHandle(),
        buffer.getHandle(),
        static_cast<VkDeviceSize>(buffer.getOffset()) + static_cast<VkDeviceSize>(argumentOffset)
    );
    recordDispatchStats();
}

// --- Transitions -----------------------------------------------------------------------------------------------------

void VulkanCommandListContext::beginTransitions(const gp::ConstVectorView<const sync::Transition*> transitions) noexcept
{
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if ((commandBuffer == nullptr) || transitions.isEmpty())
    {
        return;
    }
    VulkanCommandBufferBarrierRecorder recorder(*commandBuffer);
    const VulkanBarrierContext context(
        *this, m_resolver, recorder, getPipeline(), m_barrierDevice, m_barrierTranslation, getVulkanBarrierSettings()
    );
    beginVulkanTransitions(context, transitions);

    for (const VulkanSemaphoreRef& semaphore: recorder.getSignalSemaphores())
    {
        addSignalSemaphore(semaphore);
    }
}

void VulkanCommandListContext::endTransitions(const gp::ConstVectorView<const sync::Transition*> transitions) noexcept
{
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if ((commandBuffer == nullptr) || transitions.isEmpty())
    {
        return;
    }
    VulkanCommandBufferBarrierRecorder recorder(*commandBuffer);
    const VulkanBarrierContext context(
        *this, m_resolver, recorder, getPipeline(), m_barrierDevice, m_barrierTranslation, getVulkanBarrierSettings()
    );
    endVulkanTransitions(context, transitions);

    const gp::ConstVectorView<VulkanSemaphoreRef> waits = recorder.getWaitSemaphores();
    const gp::ConstVectorView<VkPipelineStageFlags> waitMasks = recorder.getWaitStageMasks();
    for (gp::USize index = 0u; (index < waits.size()) && (index < waitMasks.size()); ++index)
    {
        addWaitSemaphore(waitMasks[index], waits[index]);
    }
    for (const VulkanSemaphoreRef& semaphore: recorder.getSignalSemaphores())
    {
        addSignalSemaphore(semaphore);
    }

    const gp::ConstVectorView<VulkanCommitReservedResourceDesc> commits = recorder.getReservedResourceCommits();
    if (!commits.isEmpty())
    {
        VulkanPayload& payload = getPayload(VulkanContextPhase::UpdateReservedResources);
        for (const VulkanCommitReservedResourceDesc& commit: commits)
        {
            payload.reservedResourceCommits.pushBack(commit);
        }
    }
}

// --- Clears and copies -----------------------------------------------------------------------------------------------

void VulkanCommandListContext::clearUAV(
    view::UnorderedAccessView* const unorderedAccessView,
    const VkClearColorValue& color,
    const gp::Array<gp::UInt32, 4u>& words
) noexcept
{
    if (unorderedAccessView == nullptr)
    {
        return;
    }
    if (m_isInsideRenderPass)
    {
        GP_LOG_ERROR(LogVulkanRHI, "UAV clears must be recorded outside of a render pass.");
        return;
    }
    IVulkanTransferCommandRecorder* const recorder = getTransferRecorder();
    if (recorder == nullptr)
    {
        return;
    }

    if (unorderedAccessView->isTexture())
    {
        const VulkanTexture* const texture = static_cast<const VulkanTexture*>(unorderedAccessView->getTexture());
        if ((texture == nullptr) || (texture->getImage() == VK_NULL_HANDLE))
        {
            return;
        }
        // Storage images are kept in GENERAL while they are UAVs.
        recorder->clearColorImage(
            texture->getImage(), VK_IMAGE_LAYOUT_GENERAL, color, getUAVSubresourceRange(*unorderedAccessView, *texture)
        );
        return;
    }

    const VulkanBuffer* const buffer = static_cast<const VulkanBuffer*>(unorderedAccessView->getBuffer());
    if ((buffer == nullptr) || (buffer->getHandle() == VK_NULL_HANDLE))
    {
        return;
    }
    // vkCmdFillBuffer repeats one 32-bit word; multi-word patterns would need a compute clear.
    if ((words[1] != words[0]) || (words[2] != words[0]) || (words[3] != words[0]))
    {
        GP_LOG_ERROR(LogVulkanRHI, "Buffer UAV clears only support a single 32-bit pattern on every component.");
        return;
    }
    recorder->fillBuffer(
        buffer->getHandle(),
        static_cast<VkDeviceSize>(buffer->getOffset()),
        static_cast<VkDeviceSize>(buffer->getCurrentSize()) & ~static_cast<VkDeviceSize>(3u),
        words[0]
    );
}

void VulkanCommandListContext::clearUAVFloat(
    view::UnorderedAccessView* const unorderedAccessView, const gp::math::Vector4<gp::Float32>& values
) noexcept
{
    VkClearColorValue color{};
    color.float32[0] = values.x;
    color.float32[1] = values.y;
    color.float32[2] = values.z;
    color.float32[3] = values.w;
    const gp::Array<gp::UInt32, 4u> words{ std::bit_cast<gp::UInt32>(values.x),
                                           std::bit_cast<gp::UInt32>(values.y),
                                           std::bit_cast<gp::UInt32>(values.z),
                                           std::bit_cast<gp::UInt32>(values.w) };
    clearUAV(unorderedAccessView, color, words);
}

void VulkanCommandListContext::clearUAVUint(
    view::UnorderedAccessView* const unorderedAccessView, const resource::UIntVector4& values
) noexcept
{
    VkClearColorValue color{};
    color.uint32[0] = values.x;
    color.uint32[1] = values.y;
    color.uint32[2] = values.z;
    color.uint32[3] = values.w;
    const gp::Array<gp::UInt32, 4u> words{ values.x, values.y, values.z, values.w };
    clearUAV(unorderedAccessView, color, words);
}

void VulkanCommandListContext::copyToStagingBuffer(
    resource::Buffer* const sourceBuffer,
    resource::StagingBuffer* const destinationStagingBuffer,
    const gp::UInt32 offset,
    const gp::UInt32 numBytes
) noexcept
{
    if ((sourceBuffer == nullptr) || (destinationStagingBuffer == nullptr) || (numBytes == 0u))
    {
        return;
    }
    IVulkanTransferCommandRecorder* const recorder = getTransferRecorder();
    if (recorder == nullptr)
    {
        return;
    }
    static_cast<VulkanReadbackStagingBuffer*>(destinationStagingBuffer)
        ->copyFromBuffer(*recorder, *static_cast<const VulkanBuffer*>(sourceBuffer), offset, numBytes);
}

void VulkanCommandListContext::writeGPUFence(sync::GPUFence* const fence) noexcept
{
    static_cast<void>(fence);
    GP_LOG_ERROR(LogVulkanRHI, "GPU fences are written top of pipe; writeGPUFence() must not reach the context.");
}

void VulkanCommandListContext::copyTexture(
    resource::Texture* const sourceTexture,
    resource::Texture* const destTexture,
    const command::CopyTextureInfo& copyInfo
) noexcept
{
    if ((sourceTexture == nullptr) || (destTexture == nullptr))
    {
        return;
    }
    if (m_isInsideRenderPass)
    {
        GP_LOG_ERROR(LogVulkanRHI, "copyTexture() must be recorded outside of a render pass.");
        return;
    }
    IVulkanTransferCommandRecorder* const recorder = getTransferRecorder();
    if (recorder == nullptr)
    {
        return;
    }

    const VulkanTexture& source = *static_cast<const VulkanTexture*>(sourceTexture);
    const VulkanTexture& dest = *static_cast<const VulkanTexture*>(destTexture);
    const resource::TextureDesc& sourceDesc = source.getDesc();
    const resource::IntVector sourceExtent{ sourceDesc.extent.x,
                                            sourceDesc.extent.y,
                                            static_cast<gp::Int32>(sourceDesc.depth) };

    const VulkanCpuReadbackBuffer* const readback = dest.getCpuReadbackBuffer();
    if (readback != nullptr)
    {
        gp::Vector<VkBufferImageCopy> regions;
        buildVulkanReadbackCopyRegions(copyInfo, sourceExtent, source.getFullAspectMask(), *readback, regions);
        recordVulkanReadbackCopy(*recorder, source.getImage(), readback->buffer, regions);
        return;
    }

    gp::Vector<VkImageCopy> regions;
    buildVulkanImageCopyRegions(copyInfo, sourceExtent, source.getFullAspectMask(), dest.getFullAspectMask(), regions);
    recordVulkanImageCopy(*recorder, source.getImage(), dest.getImage(), regions);
}

void VulkanCommandListContext::copyBufferRegion(
    resource::Buffer* const destBuffer,
    const gp::UInt64 dstOffset,
    resource::Buffer* const sourceBuffer,
    const gp::UInt64 srcOffset,
    const gp::UInt64 numBytes
) noexcept
{
    if ((destBuffer == nullptr) || (sourceBuffer == nullptr))
    {
        return;
    }
    if (m_isInsideRenderPass)
    {
        GP_LOG_ERROR(LogVulkanRHI, "copyBufferRegion() must be recorded outside of a render pass.");
        return;
    }
    const VulkanBuffer& source = *static_cast<const VulkanBuffer*>(sourceBuffer);
    const VulkanBuffer& dest = *static_cast<const VulkanBuffer*>(destBuffer);
    VkBufferCopy region{};
    if (!buildVulkanBufferCopyRegion(source.getOffset(), srcOffset, dest.getOffset(), dstOffset, numBytes, region))
    {
        return;
    }
    IVulkanTransferCommandRecorder* const recorder = getTransferRecorder();
    if (recorder == nullptr)
    {
        return;
    }
    recordVulkanBufferCopy(*recorder, source.getHandle(), dest.getHandle(), region);
}

// --- Graphics state --------------------------------------------------------------------------------------------------

void VulkanCommandListContext::setGraphicsPipelineState(
    pipeline::GraphicsPipelineState* const graphicsState, const gp::UInt32 stencilRef, const bool applyAdditionalState
) noexcept
{
    static_cast<void>(applyAdditionalState);
    if (graphicsState == nullptr)
    {
        return;
    }
    const VulkanGraphicsPipelineState* const pipeline =
        static_cast<VulkanWrappedGraphicsPipelineState*>(graphicsState)->getState();
    if (pipeline == nullptr)
    {
        return;
    }

    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    const bool forceReset =
        (commandBuffer != nullptr) && (commandBuffer->getCurrentGraphicsPipeline() == VK_NULL_HANDLE);
    static_cast<void>(m_gfxState->setGfxPipeline(*pipeline, forceReset));
    m_currentTopology = static_cast<VkPrimitiveTopology>(pipeline->getDesc().topology);

    VulkanGraphicsPipelineDescriptorState* const state = m_gfxState->getCurrentState();
    for (gp::USize index = 0u; index < kNumGraphicsStages; ++index)
    {
        VulkanStageUniformBuffers& stageBuffers = m_graphicsUniformBuffers[index];
        stageBuffers.reset();
        stageBuffers.shader = pipeline->getShader(kGraphicsStageFrequencies[index]);
        if (state != nullptr)
        {
            applyStaticUniformBuffers(
                *state, stageBuffers, static_cast<shader_stage::Stage>(index), stageBuffers.shader
            );
        }
    }
    m_gfxState->setStencilRef(stencilRef);
}

void VulkanCommandListContext::setStreamSource(
    const gp::UInt32 streamIndex, resource::Buffer* const vertexBuffer, const gp::UInt32 offset
) noexcept
{
    if (vertexBuffer == nullptr)
    {
        m_gfxState->setStreamSource(streamIndex, VK_NULL_HANDLE, 0u);
        return;
    }
    const VulkanBuffer& buffer = *static_cast<const VulkanBuffer*>(vertexBuffer);
    m_gfxState->setStreamSource(
        streamIndex,
        buffer.getHandle(),
        static_cast<VkDeviceSize>(buffer.getOffset()) + static_cast<VkDeviceSize>(offset)
    );
}

void VulkanCommandListContext::setViewport(
    const gp::Float32 minX,
    const gp::Float32 minY,
    const gp::Float32 minZ,
    const gp::Float32 maxX,
    const gp::Float32 maxY,
    const gp::Float32 maxZ
) noexcept
{
    m_gfxState->setViewport(minX, minY, minZ, maxX, maxY, maxZ);
}

void VulkanCommandListContext::setMultipleViewports(
    const gp::UInt32 count, const command::ViewportBounds* const data
) noexcept
{
    if ((count == 0u) || (data == nullptr))
    {
        return;
    }
    const gp::UInt32 clamped = gp::math::min<gp::UInt32>(count, kVulkanMaxPendingViewports);
    gp::Array<VkViewport, kVulkanMaxPendingViewports> viewports{};
    for (gp::UInt32 index = 0u; index < clamped; ++index)
    {
        const command::ViewportBounds& bounds = data[index];
        viewports[index] = VkViewport{ bounds.topLeftX, bounds.topLeftY, bounds.width,
                                       bounds.height,   bounds.minDepth, bounds.maxDepth };
    }
    m_gfxState->setMultiViewport(viewports.data(), clamped);
}

void VulkanCommandListContext::setScissorRect(
    const bool enable, const gp::UInt32 minX, const gp::UInt32 minY, const gp::UInt32 maxX, const gp::UInt32 maxY
) noexcept
{
    m_gfxState->setScissor(enable, minX, minY, maxX, maxY);
}

void VulkanCommandListContext::setStencilRef(const gp::UInt32 stencilRef) noexcept
{
    m_gfxState->setStencilRef(stencilRef);
}

void VulkanCommandListContext::setBlendFactor(const gp::math::LinearColor& blendFactor) noexcept
{
    const gp::Array<gp::Float32, 4u> factor{ blendFactor.r, blendFactor.g, blendFactor.b, blendFactor.a };
    m_gfxState->setBlendFactor(factor);
}

void VulkanCommandListContext::setDepthBounds(const gp::Float32 minDepth, const gp::Float32 maxDepth) noexcept
{
    m_gfxState->setDepthBounds(minDepth, maxDepth);
}

// --- Draws -----------------------------------------------------------------------------------------------------------

void VulkanCommandListContext::drawPrimitive(
    const gp::UInt32 baseVertexIndex, const gp::UInt32 numPrimitives, const gp::UInt32 numInstances
) noexcept
{
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if (commandBuffer == nullptr)
    {
        return;
    }
    commitGraphicsResourceTables();
    if (!m_gfxState->prepareForDraw(*commandBuffer))
    {
        return;
    }
    const gp::UInt32 instances = gp::math::max<gp::UInt32>(1u, numInstances);
    const gp::UInt32 numVertices = getVulkanVertexCountForPrimitiveCount(numPrimitives, m_currentTopology);
    vkCmdDraw(commandBuffer->getHandle(), numVertices, instances, baseVertexIndex, 0u);
    recordDrawStats(1u, numPrimitives * instances);
}

void VulkanCommandListContext::drawPrimitiveIndirect(
    resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if ((commandBuffer == nullptr) || (argumentBuffer == nullptr))
    {
        return;
    }
    commitGraphicsResourceTables();
    if (!m_gfxState->prepareForDraw(*commandBuffer))
    {
        return;
    }
    const VulkanBuffer& buffer = *static_cast<const VulkanBuffer*>(argumentBuffer);
    vkCmdDrawIndirect(
        commandBuffer->getHandle(),
        buffer.getHandle(),
        static_cast<VkDeviceSize>(buffer.getOffset()) + static_cast<VkDeviceSize>(argumentOffset),
        1u,
        static_cast<gp::UInt32>(sizeof(VkDrawIndirectCommand))
    );
    recordDrawStats(1u, 0u);
}

void VulkanCommandListContext::drawIndexedPrimitive(
    resource::Buffer* const indexBuffer,
    const gp::Int32 baseVertexIndex,
    const gp::UInt32 firstInstance,
    const gp::UInt32 numVertices,
    const gp::UInt32 startIndex,
    const gp::UInt32 numPrimitives,
    const gp::UInt32 numInstances
) noexcept
{
    static_cast<void>(numVertices);
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if ((commandBuffer == nullptr) || (indexBuffer == nullptr))
    {
        return;
    }
    commitGraphicsResourceTables();
    if (!m_gfxState->prepareForDraw(*commandBuffer))
    {
        return;
    }
    const VulkanBuffer& buffer = *static_cast<const VulkanBuffer*>(indexBuffer);
    const VkCommandBuffer handle = commandBuffer->getHandle();
    vkCmdBindIndexBuffer(
        handle, buffer.getHandle(), static_cast<VkDeviceSize>(buffer.getOffset()), buffer.getIndexType()
    );

    const gp::UInt32 instances = gp::math::max<gp::UInt32>(1u, numInstances);
    const gp::UInt32 numIndices = getVulkanVertexCountForPrimitiveCount(numPrimitives, m_currentTopology);
    vkCmdDrawIndexed(handle, numIndices, instances, startIndex, baseVertexIndex, firstInstance);
    recordDrawStats(1u, numPrimitives * instances);
}

void VulkanCommandListContext::drawIndexedPrimitiveIndirect(
    resource::Buffer* const indexBuffer, resource::Buffer* const argumentBuffer, const gp::UInt32 argumentOffset
) noexcept
{
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if ((commandBuffer == nullptr) || (indexBuffer == nullptr) || (argumentBuffer == nullptr))
    {
        return;
    }
    commitGraphicsResourceTables();
    if (!m_gfxState->prepareForDraw(*commandBuffer))
    {
        return;
    }
    const VulkanBuffer& indices = *static_cast<const VulkanBuffer*>(indexBuffer);
    const VulkanBuffer& arguments = *static_cast<const VulkanBuffer*>(argumentBuffer);
    const VkCommandBuffer handle = commandBuffer->getHandle();
    vkCmdBindIndexBuffer(
        handle, indices.getHandle(), static_cast<VkDeviceSize>(indices.getOffset()), indices.getIndexType()
    );
    vkCmdDrawIndexedIndirect(
        handle,
        arguments.getHandle(),
        static_cast<VkDeviceSize>(arguments.getOffset()) + static_cast<VkDeviceSize>(argumentOffset),
        1u,
        static_cast<gp::UInt32>(sizeof(VkDrawIndexedIndirectCommand))
    );
    recordDrawStats(1u, 0u);
}

// --- Render passes ---------------------------------------------------------------------------------------------------

void VulkanCommandListContext::resetViewportToRenderArea(const VkRect2D& renderArea) noexcept
{
    const gp::Float32 minX = static_cast<gp::Float32>(renderArea.offset.x);
    const gp::Float32 minY = static_cast<gp::Float32>(renderArea.offset.y);
    m_gfxState->setViewport(
        minX,
        minY,
        0.0f,
        minX + static_cast<gp::Float32>(renderArea.extent.width),
        minY + static_cast<gp::Float32>(renderArea.extent.height),
        1.0f
    );
    m_gfxState->setScissor(false, 0u, 0u, 0u, 0u);
}

void VulkanCommandListContext::beginRenderPass(const command::RenderPassInfo& inInfo, const char* const name) noexcept
{
    static_cast<void>(name);
    VulkanRenderPassManager* const manager = getServices().renderPassManager;
    if (manager == nullptr)
    {
        GP_LOG_ERROR(LogVulkanRHI, "beginRenderPass() requires a render pass manager.");
        return;
    }
    if (m_isInsideRenderPass)
    {
        GP_LOG_ERROR(LogVulkanRHI, "beginRenderPass() called while a render pass is already open.");
        return;
    }
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if (commandBuffer == nullptr)
    {
        return;
    }

    m_renderPassInfo = inInfo;
    const VulkanRenderTargetCaps& caps = manager->getCaps();
    VkRect2D renderArea{};

    if (manager->usesDynamicRendering())
    {
        fillVulkanDynamicRenderingInfo(caps, inInfo, m_dynamicRenderingInfo);
        if (!commandBuffer->beginDynamicRendering(m_dynamicRenderingInfo.renderingInfo))
        {
            return;
        }
        m_isDynamicRenderingActive = true;
        renderArea = m_dynamicRenderingInfo.renderingInfo.renderArea;
    }
    else
    {
        VkImageLayout depthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout stencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        extractVulkanDepthStencilLayouts(caps, inInfo, depthLayout, stencilLayout);
        const VulkanRenderTargetLayout layout(caps, inInfo, depthLayout, stencilLayout);

        VulkanRenderPass* const renderPass = manager->getOrCreateRenderPass(layout);
        if (renderPass == nullptr)
        {
            return;
        }
        command::SetRenderTargetsInfo renderTargets{};
        inInfo.convertToRenderTargetsInfo(renderTargets);
        VulkanFramebuffer* const framebuffer = manager->getOrCreateFramebuffer(renderTargets, layout, *renderPass);
        if ((framebuffer == nullptr) || !manager->beginRenderPass(*commandBuffer, inInfo, *renderPass, *framebuffer))
        {
            return;
        }
        m_currentRenderPass = renderPass;
        m_currentFramebuffer = framebuffer;
        renderArea = framebuffer->getRenderArea();
    }
    m_isInsideRenderPass = true;

    if ((inInfo.numOcclusionQueries > 0u) && (getServices().queryPoolManager != nullptr))
    {
        VulkanPayload& payload = getPayload(VulkanContextPhase::Execute);
        payload.queryPoolManager = getServices().queryPoolManager;
        beginVulkanOcclusionQueryBatch(
            *getServices().queryPoolManager,
            payload.queryPools,
            commandBuffer->getHandle(),
            inInfo.numOcclusionQueries,
            inInfo.multiViewCount
        );
    }

    resetViewportToRenderArea(renderArea);
}

void VulkanCommandListContext::endRenderPass() noexcept
{
    if (!m_isInsideRenderPass)
    {
        return;
    }
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if (commandBuffer == nullptr)
    {
        return;
    }
    if (m_isDynamicRenderingActive)
    {
        static_cast<void>(commandBuffer->endDynamicRendering());
        m_isDynamicRenderingActive = false;
    }
    else if (getServices().renderPassManager != nullptr)
    {
        static_cast<void>(getServices().renderPassManager->endRenderPass(*commandBuffer));
    }
    m_isInsideRenderPass = false;
    m_currentFramebuffer = nullptr;
    m_currentRenderPass = nullptr;
}

void VulkanCommandListContext::nextSubpass() noexcept
{
    if (!m_isInsideRenderPass)
    {
        return;
    }
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if (commandBuffer == nullptr)
    {
        return;
    }
    if (m_isDynamicRenderingActive)
    {
        // Dynamic rendering has no subpasses: a by-region barrier gives the same tile-local visibility guarantee.
        VkMemoryBarrier barrier{};
        zeroVulkanStruct(barrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        vkCmdPipelineBarrier(
            commandBuffer->getHandle(),
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_DEPENDENCY_BY_REGION_BIT,
            1u,
            &barrier,
            0u,
            nullptr,
            0u,
            nullptr
        );
        return;
    }
    vkCmdNextSubpass(commandBuffer->getHandle(), VK_SUBPASS_CONTENTS_INLINE);
}

// --- Queries ---------------------------------------------------------------------------------------------------------

void VulkanCommandListContext::beginRenderQuery(sync::RenderQuery* const renderQuery) noexcept
{
    if (renderQuery == nullptr)
    {
        return;
    }
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if (commandBuffer == nullptr)
    {
        return;
    }
    VulkanPayload& payload = getPayload(VulkanContextPhase::Execute);
    recordVulkanBeginRenderQuery(
        payload.queryPools, commandBuffer->getHandle(), *static_cast<VulkanRenderQuery*>(renderQuery)
    );
}

void VulkanCommandListContext::endRenderQuery(sync::RenderQuery* const renderQuery) noexcept
{
    VulkanQueryPoolManager* const manager = getServices().queryPoolManager;
    if ((renderQuery == nullptr) || (manager == nullptr))
    {
        return;
    }
    VulkanCommandBuffer* const commandBuffer = getCommandBuffer();
    if (commandBuffer == nullptr)
    {
        return;
    }
    VulkanPayload& payload = getPayload(VulkanContextPhase::Execute);
    payload.queryPoolManager = manager;
    recordVulkanEndRenderQuery(
        *manager,
        payload.queryPools,
        commandBuffer->getHandle(),
        *static_cast<VulkanRenderQuery*>(renderQuery),
        m_renderPassInfo.multiViewCount
    );
}

// --- IVulkanPendingStateContext --------------------------------------------------------------------------------------

bool VulkanCommandListContext::acquirePoolSetAndDescriptorsIfNeeded(
    const VulkanDescriptorSetsLayout& layout, const bool needDescriptors, VkDescriptorSet* const outSets
) noexcept
{
    if (!m_poolSetAcquirer)
    {
        return false;
    }
    return m_poolSetAcquirer->acquirePoolSetAndDescriptorsIfNeeded(layout, needDescriptors, m_currentFrame, outSets);
}

bool VulkanCommandListContext::allocateUniformMemory(const gp::UInt32 size, VulkanUniformUpload& outUpload) noexcept
{
    VulkanTempBlockAllocator* const allocator = getServices().tempBlockAllocator;
    if ((allocator == nullptr) || (size == 0u))
    {
        return false;
    }
    VulkanAllocation allocation{};
    gp::UInt8* const mapped =
        allocator->allocate(size, getVulkanUniformBufferAlignment(getDevice()), &getContextSyncPoint(), allocation);
    if (mapped == nullptr)
    {
        return false;
    }
    outUpload.buffer = allocation.getBufferHandle();
    outUpload.handleId = allocation.handleId;
    outUpload.offset = allocation.offset;
    outUpload.mappedData = mapped;
    return true;
}

VulkanDescriptorSetCache* VulkanCommandListContext::getDescriptorSetCache() noexcept
{
    return getServices().descriptorSetCache;
}

gp::UInt64 VulkanCommandListContext::getCurrentFrame() const noexcept
{
    return m_currentFrame;
}

bool VulkanCommandListContext::getInputAttachmentView(
    const VulkanShaderAttachmentType type, VulkanTextureView& outView, VkImageLayout& outLayout
) const noexcept
{
    if (!m_isInsideRenderPass)
    {
        return false;
    }
    if (type == VulkanShaderAttachmentType::Depth)
    {
        const VulkanTexture* const depth =
            static_cast<const VulkanTexture*>(m_renderPassInfo.depthStencilRenderTarget.depthStencilTarget);
        if (depth == nullptr)
        {
            return false;
        }
        outView = VulkanTextureView{ depth->getPartialView(), depth->getImage(), 0u };
        outLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        return outView.view != VK_NULL_HANDLE;
    }

    const gp::UInt32 colorIndex =
        static_cast<gp::UInt32>(type) - static_cast<gp::UInt32>(VulkanShaderAttachmentType::Color0);
    if ((type == VulkanShaderAttachmentType::Count) || (colorIndex >= m_renderPassInfo.colorRenderTargets.size()))
    {
        return false;
    }
    const VulkanTexture* const color =
        static_cast<const VulkanTexture*>(m_renderPassInfo.colorRenderTargets[colorIndex].renderTarget);
    if (color == nullptr)
    {
        return false;
    }
    // The color target is read as an input attachment while it is still bound for writing: GENERAL covers both.
    outView = VulkanTextureView{ color->getDefaultView(), color->getImage(), 0u };
    outLayout = VK_IMAGE_LAYOUT_GENERAL;
    return outView.view != VK_NULL_HANDLE;
}

}   // namespace gp::rhi::vulkan
