// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pendingstate/VulkanPendingGfxState.hpp"
#include "commands/VulkanCommandBuffer.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "descriptors/VulkanDescriptorSettings.hpp"
#include "diagnostics/AssertionMacros.hpp"
#include "loader/VulkanCommandWrappers.hpp"
#include "logging/LogMacros.hpp"
#include "pendingstate/VulkanGraphicsPipelineDescriptorState.hpp"
#include "pipeline/VulkanGraphicsPipelineDesc.hpp"
#include "pipeline/VulkanGraphicsPipelineDescTypes.hpp"
#include "pipeline/VulkanGraphicsPipelineState.hpp"
#include "pipeline/VulkanPipelineDynamicStates.hpp"
#include "shaders/VulkanShader.hpp"
#include "shaders/VulkanShaderHeader.hpp"
#include "VulkanCommon.hpp"
#include <utility>

namespace gp::rhi::vulkan
{

namespace
{

/// @brief Converts a boolean to a Vulkan boolean.
/// @param[in] value Boolean.
/// @return `VK_TRUE` or `VK_FALSE`.
[[nodiscard]] constexpr VkBool32 toVkBool(const bool value) noexcept
{
    return value ? VK_TRUE : VK_FALSE;
}

/// @brief Returns the sample count flag of a pipeline, treating zero as single-sampled.
/// @param[in] rasterizationSamples Sample count stored in the pipeline description.
/// @return Sample count flag.
[[nodiscard]] constexpr VkSampleCountFlagBits toSampleCount(const gp::UInt16 rasterizationSamples) noexcept
{
    return (rasterizationSamples == 0u) ? VK_SAMPLE_COUNT_1_BIT
                                        : static_cast<VkSampleCountFlagBits>(rasterizationSamples);
}

/// @brief Reports whether two stencil faces of a depth stencil description use the same operations.
/// @param[in] lhs First description.
/// @param[in] rhs Second description.
/// @return True when both faces match.
[[nodiscard]] bool hasSameStencilOps(const VulkanDepthStencilDesc& lhs, const VulkanDepthStencilDesc& rhs) noexcept
{
    return (lhs.frontFailOp == rhs.frontFailOp) && (lhs.frontPassOp == rhs.frontPassOp) &&
           (lhs.frontDepthFailOp == rhs.frontDepthFailOp) && (lhs.frontCompareOp == rhs.frontCompareOp) &&
           (lhs.backFailOp == rhs.backFailOp) && (lhs.backPassOp == rhs.backPassOp) &&
           (lhs.backDepthFailOp == rhs.backDepthFailOp) && (lhs.backCompareOp == rhs.backCompareOp);
}

/// @brief Reports whether a rasterizer description enables depth bias.
/// @param[in] rasterizer Rasterizer description.
/// @return True when either bias term is non-zero.
[[nodiscard]] bool isDepthBiasEnabled(const VulkanRasterizerDesc& rasterizer) noexcept
{
    return (rasterizer.depthBiasConstantFactor != 0.0f) || (rasterizer.depthBiasSlopeScale != 0.0f);
}

}   // namespace

VulkanPendingGfxState::VulkanPendingGfxState(
    VulkanDevice& device, IVulkanPendingStateContext& context, const VulkanDescriptorDefaults& defaults
) noexcept
    : m_device(device)
    , m_context(context)
    , m_defaults(defaults)
    , m_features(makeVulkanDynamicStateFeatures(device))
{
    reset();
}

VulkanPendingGfxState::~VulkanPendingGfxState() noexcept = default;

void VulkanPendingGfxState::reset() noexcept
{
    m_viewports = {};
    m_numViewports = 0u;
    m_scissors = {};
    m_numScissors = 0u;
    m_stencilRef = 0u;
    m_isScissorEnabled = false;
    m_pendingStreams = {};
    m_hasDirtyVertexStreams = true;
    m_isPipelineDynamicStateDirty = true;
    m_currentPipeline = nullptr;
    m_currentState = nullptr;
    m_pipelineDynamicStates = VulkanDynamicStateList{};
}

void VulkanPendingGfxState::setViewport(
    const gp::Float32 minX,
    const gp::Float32 minY,
    const gp::Float32 minZ,
    const gp::Float32 maxX,
    const gp::Float32 maxY,
    const gp::Float32 maxZ
) noexcept
{
    m_numViewports = 1u;
    VkViewport& viewport = m_viewports[0];
    viewport.x = minX;
    viewport.y = minY;
    viewport.width = maxX - minX;
    viewport.height = maxY - minY;
    viewport.minDepth = minZ;
    // An empty depth range is rejected by the validation layers; callers use it to clamp depth to a constant.
    viewport.maxDepth = (minZ == maxZ) ? (minZ + 1.0f) : maxZ;

    setScissorRect(
        static_cast<gp::Int32>(minX),
        static_cast<gp::Int32>(minY),
        static_cast<gp::UInt32>(maxX - minX),
        static_cast<gp::UInt32>(maxY - minY)
    );
    m_isScissorEnabled = false;
}

void VulkanPendingGfxState::setMultiViewport(const VkViewport* const viewports, const gp::UInt32 count) noexcept
{
    const gp::UInt32 numViewports = (count < kVulkanMaxPendingViewports) ? count : kVulkanMaxPendingViewports;
    GP_CHECK((viewports != nullptr) || (numViewports == 0u));
    m_numViewports = numViewports;
    m_numScissors = numViewports;
    for (gp::UInt32 index = 0u; index < numViewports; ++index)
    {
        const VkViewport& viewport = viewports[index];
        m_viewports[index] = viewport;
        m_scissors[index].offset.x = static_cast<gp::Int32>(viewport.x);
        m_scissors[index].offset.y = static_cast<gp::Int32>(viewport.y);
        m_scissors[index].extent.width = static_cast<gp::UInt32>(viewport.width);
        m_scissors[index].extent.height = static_cast<gp::UInt32>(viewport.height);
    }
    m_isScissorEnabled = false;
}

void VulkanPendingGfxState::setScissor(
    const bool enable, const gp::UInt32 minX, const gp::UInt32 minY, const gp::UInt32 maxX, const gp::UInt32 maxY
) noexcept
{
    if (enable)
    {
        setScissorRect(static_cast<gp::Int32>(minX), static_cast<gp::Int32>(minY), maxX - minX, maxY - minY);
    }
    else if (m_numViewports != 0u)
    {
        const VkViewport& viewport = m_viewports[0];
        setScissorRect(
            static_cast<gp::Int32>(viewport.x),
            static_cast<gp::Int32>(viewport.y),
            static_cast<gp::UInt32>(viewport.width),
            static_cast<gp::UInt32>(viewport.height)
        );
    }
    m_isScissorEnabled = enable;
}

void VulkanPendingGfxState::setScissorRect(
    const gp::Int32 x, const gp::Int32 y, const gp::UInt32 width, const gp::UInt32 height
) noexcept
{
    m_numScissors = 1u;
    m_scissors[0].offset.x = x;
    m_scissors[0].offset.y = y;
    m_scissors[0].extent.width = width;
    m_scissors[0].extent.height = height;
}

void VulkanPendingGfxState::setStreamSource(
    const gp::UInt32 streamIndex, const VkBuffer buffer, const VkDeviceSize offset
) noexcept
{
    GP_CHECK(streamIndex < kMaxVertexElementCount);
    VulkanVertexStream& stream = m_pendingStreams[streamIndex];
    if ((stream.buffer != buffer) || (stream.offset != offset))
    {
        stream.buffer = buffer;
        stream.offset = offset;
        m_hasDirtyVertexStreams = true;
    }
}

void VulkanPendingGfxState::setStencilRef(const gp::UInt32 stencilRef) noexcept
{
    m_stencilRef = stencilRef;
}

void VulkanPendingGfxState::setBlendFactor(const gp::Array<gp::Float32, 4u>& blendFactor) noexcept
{
    m_blendFactor = blendFactor;
}

void VulkanPendingGfxState::setDepthBounds(const gp::Float32 minDepthBounds, const gp::Float32 maxDepthBounds) noexcept
{
    m_minDepthBounds = minDepthBounds;
    m_maxDepthBounds = maxDepthBounds;
}

bool VulkanPendingGfxState::setGfxPipeline(const VulkanGraphicsPipelineState& pipeline, const bool forceReset) noexcept
{
    const bool hasChanged = m_currentPipeline != &pipeline;
    if (hasChanged)
    {
        VulkanGraphicsPipelineDescriptorState* state = nullptr;
        for (Entry& entry: m_pipelineStates)
        {
            if (entry.pipeline == &pipeline)
            {
                state = entry.state.get();
                break;
            }
        }
        if (state == nullptr)
        {
            Entry entry{};
            entry.pipeline = &pipeline;
            entry.state = gp::makeUnique<VulkanGraphicsPipelineDescriptorState>(
                m_device, pipeline, m_defaults, useVulkanDescriptorCache()
            );
            state = entry.state.get();
            m_pipelineStates.pushBack(std::move(entry));
        }

        m_currentPipeline = &pipeline;
        m_currentState = state;
        m_pipelineDynamicStates = fillVulkanDynamicStates(m_features, pipeline.getDesc());
        m_isPipelineDynamicStateDirty = true;
        // Another vertex layout maps bindings to other streams.
        m_hasDirtyVertexStreams = true;
    }

    if (hasChanged || forceReset)
    {
        m_currentState->reset();
    }
    return hasChanged;
}

void VulkanPendingGfxState::bind(VulkanCommandBuffer& commandBuffer) noexcept
{
    if (m_currentPipeline != nullptr)
    {
        m_currentPipeline->bind(commandBuffer);
    }
}

bool VulkanPendingGfxState::prepareForDraw(VulkanCommandBuffer& commandBuffer) noexcept
{
    if ((m_currentPipeline == nullptr) || (m_currentState == nullptr))
    {
        GP_LOG_WARNING(LogVulkanRHI, "Draw without a graphics pipeline.");
        return false;
    }

    // 1. Descriptors: subpass inputs come from the render pass, so they are refreshed on every draw.
    if (m_currentState->hasInputAttachments())
    {
        (void)m_currentState->updateInputAttachments(m_context);
    }
    const bool hasDescriptorSets = m_currentState->updateDescriptorSets(m_context);
    if (!hasDescriptorSets && (m_currentState->getUsedSetsMask() != 0u))
    {
        return false;
    }

    // 2. Dynamic states.
    updateDynamicStates(commandBuffer);

    // 3. Descriptor sets.
    const VkCommandBuffer handle = commandBuffer.getHandle();
    if (hasDescriptorSets)
    {
        m_currentState->bindDescriptorSets(handle);
    }

    // 4. Vertex buffers.
    if (m_hasDirtyVertexStreams)
    {
        bindVertexStreams(handle);
        m_hasDirtyVertexStreams = false;
    }
    return true;
}

void VulkanPendingGfxState::updateDynamicStates(VulkanCommandBuffer& commandBuffer) noexcept
{
    VulkanCommandBufferDynamicState& dynamicState = commandBuffer.getDynamicState();
    const VkCommandBuffer handle = commandBuffer.getHandle();
    const bool isFullUpdate = dynamicState.needsFullUpdate;
    const bool useWithCount = m_pipelineDynamicStates.contains(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT);

    // Viewports and scissors: the update call stores the new values, so it runs before the full-update test.
    if ((m_numViewports != 0u) && (dynamicState.updateViewports(m_viewports.data(), m_numViewports) || isFullUpdate))
    {
        // Negative heights flip Y so the framebuffer origin matches the other APIs (maintenance1, core in 1.1).
        gp::Array<VkViewport, kVulkanMaxPendingViewports> flippedViewports{};
        for (gp::UInt32 index = 0u; index < m_numViewports; ++index)
        {
            flippedViewports[index] = m_viewports[index];
            flippedViewports[index].y = m_viewports[index].y + m_viewports[index].height;
            flippedViewports[index].height = -m_viewports[index].height;
        }
        if (useWithCount && (dynamic_api::vkCmdSetViewportWithCountEXT != nullptr))
        {
            vkCmdSetViewportWithCountEXT(handle, m_numViewports, flippedViewports.data());
        }
        else if (dynamic_api::vkCmdSetViewport != nullptr)
        {
            vkCmdSetViewport(handle, 0u, m_numViewports, flippedViewports.data());
        }
    }

    if ((m_numScissors != 0u) && (dynamicState.updateScissors(m_scissors.data(), m_numScissors) || isFullUpdate))
    {
        if (useWithCount && (dynamic_api::vkCmdSetScissorWithCountEXT != nullptr))
        {
            vkCmdSetScissorWithCountEXT(handle, m_numScissors, m_scissors.data());
        }
        else if (dynamic_api::vkCmdSetScissor != nullptr)
        {
            vkCmdSetScissor(handle, 0u, m_numScissors, m_scissors.data());
        }
    }

    // The pipeline's dynamic state list gates the remaining commands: depth bounds need a device feature, and a value
    // stored for a pipeline that does not read it would wrongly skip the command for the next pipeline that does.
    if (m_pipelineDynamicStates.contains(VK_DYNAMIC_STATE_STENCIL_REFERENCE) &&
        (dynamicState.updateStencilRef(m_stencilRef) || isFullUpdate) &&
        (dynamic_api::vkCmdSetStencilReference != nullptr))
    {
        vkCmdSetStencilReference(handle, VK_STENCIL_FACE_FRONT_AND_BACK, m_stencilRef);
    }

    if (m_pipelineDynamicStates.contains(VK_DYNAMIC_STATE_BLEND_CONSTANTS) &&
        (dynamicState.updateBlendConstants(m_blendFactor) || isFullUpdate) &&
        (dynamic_api::vkCmdSetBlendConstants != nullptr))
    {
        vkCmdSetBlendConstants(handle, m_blendFactor.data());
    }

    if (m_pipelineDynamicStates.contains(VK_DYNAMIC_STATE_DEPTH_BOUNDS) &&
        (dynamicState.updateDepthBounds(m_minDepthBounds, m_maxDepthBounds) || isFullUpdate) &&
        (dynamic_api::vkCmdSetDepthBounds != nullptr))
    {
        vkCmdSetDepthBounds(handle, m_minDepthBounds, m_maxDepthBounds);
    }

    if (m_isPipelineDynamicStateDirty || isFullUpdate)
    {
        updatePipelineDynamicStates(dynamicState, handle, isFullUpdate);
        m_isPipelineDynamicStateDirty = false;
    }
    dynamicState.needsFullUpdate = false;
}

void VulkanPendingGfxState::updatePipelineDynamicStates(
    VulkanCommandBufferDynamicState& dynamicState, const VkCommandBuffer commandBuffer, const bool isFullUpdate
) noexcept
{
    const VulkanGraphicsPipelineDesc& desc = m_currentPipeline->getDesc();
    VulkanDynamicPipelineStateSnapshot& last = dynamicState.lastPipelineState;
    const VulkanDynamicStateList& states = m_pipelineDynamicStates;

    // Extended dynamic state 1.
    if (states.contains(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT) &&
        (isFullUpdate || (last.topology != desc.topology)) && (dynamic_api::vkCmdSetPrimitiveTopologyEXT != nullptr))
    {
        vkCmdSetPrimitiveTopologyEXT(commandBuffer, static_cast<VkPrimitiveTopology>(desc.topology));
    }
    if (states.contains(VK_DYNAMIC_STATE_CULL_MODE_EXT) &&
        (isFullUpdate || (last.rasterizer.cullMode != desc.rasterizer.cullMode)) &&
        (dynamic_api::vkCmdSetCullModeEXT != nullptr))
    {
        vkCmdSetCullModeEXT(commandBuffer, static_cast<VkCullModeFlags>(desc.rasterizer.cullMode));
    }
    if (states.contains(VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE_EXT) &&
        (isFullUpdate || (last.depthStencil.stencilTestEnable != desc.depthStencil.stencilTestEnable)) &&
        (dynamic_api::vkCmdSetStencilTestEnableEXT != nullptr))
    {
        vkCmdSetStencilTestEnableEXT(commandBuffer, toVkBool(desc.depthStencil.stencilTestEnable));
    }
    if (states.contains(VK_DYNAMIC_STATE_STENCIL_OP_EXT) &&
        (isFullUpdate || !hasSameStencilOps(last.depthStencil, desc.depthStencil)) &&
        (dynamic_api::vkCmdSetStencilOpEXT != nullptr))
    {
        const VulkanDepthStencilDesc& depthStencil = desc.depthStencil;
        vkCmdSetStencilOpEXT(
            commandBuffer,
            VK_STENCIL_FACE_FRONT_BIT,
            static_cast<VkStencilOp>(depthStencil.frontFailOp),
            static_cast<VkStencilOp>(depthStencil.frontPassOp),
            static_cast<VkStencilOp>(depthStencil.frontDepthFailOp),
            static_cast<VkCompareOp>(depthStencil.frontCompareOp)
        );
        vkCmdSetStencilOpEXT(
            commandBuffer,
            VK_STENCIL_FACE_BACK_BIT,
            static_cast<VkStencilOp>(depthStencil.backFailOp),
            static_cast<VkStencilOp>(depthStencil.backPassOp),
            static_cast<VkStencilOp>(depthStencil.backDepthFailOp),
            static_cast<VkCompareOp>(depthStencil.backCompareOp)
        );
    }
    if (states.contains(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP_EXT) &&
        (isFullUpdate || (last.depthStencil.depthCompareOp != desc.depthStencil.depthCompareOp)) &&
        (dynamic_api::vkCmdSetDepthCompareOpEXT != nullptr))
    {
        vkCmdSetDepthCompareOpEXT(commandBuffer, static_cast<VkCompareOp>(desc.depthStencil.depthCompareOp));
    }
    if (states.contains(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE_EXT) &&
        (isFullUpdate || (last.depthStencil.depthTestEnable != desc.depthStencil.depthTestEnable)) &&
        (dynamic_api::vkCmdSetDepthTestEnableEXT != nullptr))
    {
        vkCmdSetDepthTestEnableEXT(commandBuffer, toVkBool(desc.depthStencil.depthTestEnable));
    }
    if (states.contains(VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE_EXT) &&
        (isFullUpdate || (last.depthStencil.depthBoundsTestEnable != desc.depthStencil.depthBoundsTestEnable)) &&
        (dynamic_api::vkCmdSetDepthBoundsTestEnableEXT != nullptr))
    {
        vkCmdSetDepthBoundsTestEnableEXT(commandBuffer, toVkBool(desc.depthStencil.depthBoundsTestEnable));
    }
    if (states.contains(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE_EXT) &&
        (isFullUpdate || (last.depthStencil.depthWriteEnable != desc.depthStencil.depthWriteEnable)) &&
        (dynamic_api::vkCmdSetDepthWriteEnableEXT != nullptr))
    {
        vkCmdSetDepthWriteEnableEXT(commandBuffer, toVkBool(desc.depthStencil.depthWriteEnable));
    }

    // Extended dynamic state 2.
    const bool isBiasEnabled = isDepthBiasEnabled(desc.rasterizer);
    if (states.contains(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE_EXT) &&
        (isFullUpdate || (isDepthBiasEnabled(last.rasterizer) != isBiasEnabled)) &&
        (dynamic_api::vkCmdSetDepthBiasEnableEXT != nullptr))
    {
        vkCmdSetDepthBiasEnableEXT(commandBuffer, toVkBool(isBiasEnabled));
    }
    if (states.contains(VK_DYNAMIC_STATE_DEPTH_BIAS) &&
        (isFullUpdate || (last.rasterizer.depthBiasConstantFactor != desc.rasterizer.depthBiasConstantFactor) ||
         (last.rasterizer.depthBiasSlopeScale != desc.rasterizer.depthBiasSlopeScale)) &&
        (dynamic_api::vkCmdSetDepthBias != nullptr))
    {
        vkCmdSetDepthBias(
            commandBuffer, desc.rasterizer.depthBiasConstantFactor, 0.0f, desc.rasterizer.depthBiasSlopeScale
        );
    }

    // Extended dynamic state 3.
    if (states.contains(VK_DYNAMIC_STATE_POLYGON_MODE_EXT) &&
        (isFullUpdate || (last.rasterizer.polygonMode != desc.rasterizer.polygonMode)) &&
        (dynamic_api::vkCmdSetPolygonModeEXT != nullptr))
    {
        vkCmdSetPolygonModeEXT(commandBuffer, static_cast<VkPolygonMode>(desc.rasterizer.polygonMode));
    }
    if (states.contains(VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT) &&
        (isFullUpdate || (last.rasterizationSamples != desc.rasterizationSamples)) &&
        (dynamic_api::vkCmdSetRasterizationSamplesEXT != nullptr))
    {
        vkCmdSetRasterizationSamplesEXT(commandBuffer, toSampleCount(desc.rasterizationSamples));
    }
    if (states.contains(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT) &&
        (isFullUpdate || (last.useAlphaToCoverage != desc.useAlphaToCoverage)) &&
        (dynamic_api::vkCmdSetAlphaToCoverageEnableEXT != nullptr))
    {
        vkCmdSetAlphaToCoverageEnableEXT(commandBuffer, toVkBool(desc.useAlphaToCoverage != 0u));
    }

    const bool hasColorBlendStates = states.contains(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT) ||
                                     states.contains(VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT) ||
                                     states.contains(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT);
    // The write mask also depends on the pixel shader outputs, which the shader key identifies.
    if (hasColorBlendStates &&
        (isFullUpdate || !last.hasSameColorAttachments(desc) || (last.shaderKeyShared != desc.shaderKeyShared)))
    {
        const VulkanShader* const pixelShader = m_currentPipeline->getVulkanShader(shader_stage::Stage::Pixel);
        const gp::UInt32 pixelOutputMask = (pixelShader != nullptr) ? pixelShader->getCodeHeader().inOutMask : ~0u;
        const gp::UInt32 numAttachments = static_cast<gp::UInt32>(
            (desc.colorAttachmentStates.size() < kMaxSimultaneousRenderTargets) ? desc.colorAttachmentStates.size()
                                                                                : kMaxSimultaneousRenderTargets
        );

        gp::Array<VkBool32, kMaxSimultaneousRenderTargets> blendEnables{};
        gp::Array<VkColorBlendEquationEXT, kMaxSimultaneousRenderTargets> blendEquations{};
        gp::Array<VkColorComponentFlags, kMaxSimultaneousRenderTargets> writeMasks{};
        for (gp::UInt32 index = 0u; index < numAttachments; ++index)
        {
            const VulkanBlendAttachmentDesc& attachment = desc.colorAttachmentStates[index];
            blendEnables[index] = toVkBool(attachment.blend);
            blendEquations[index].srcColorBlendFactor = static_cast<VkBlendFactor>(attachment.srcColorBlendFactor);
            blendEquations[index].dstColorBlendFactor = static_cast<VkBlendFactor>(attachment.dstColorBlendFactor);
            blendEquations[index].colorBlendOp = static_cast<VkBlendOp>(attachment.colorBlendOp);
            blendEquations[index].srcAlphaBlendFactor = static_cast<VkBlendFactor>(attachment.srcAlphaBlendFactor);
            blendEquations[index].dstAlphaBlendFactor = static_cast<VkBlendFactor>(attachment.dstAlphaBlendFactor);
            blendEquations[index].alphaBlendOp = static_cast<VkBlendOp>(attachment.alphaBlendOp);
            // Outputs the pixel shader never writes keep an empty mask, as in the baked pipeline state.
            writeMasks[index] = ((pixelOutputMask & (1u << index)) != 0u)
                                    ? static_cast<VkColorComponentFlags>(attachment.colorWriteMask)
                                    : VkColorComponentFlags{ 0u };
        }

        if ((numAttachments != 0u) && states.contains(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT) &&
            (dynamic_api::vkCmdSetColorBlendEnableEXT != nullptr))
        {
            vkCmdSetColorBlendEnableEXT(commandBuffer, 0u, numAttachments, blendEnables.data());
        }
        if ((numAttachments != 0u) && states.contains(VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT) &&
            (dynamic_api::vkCmdSetColorBlendEquationEXT != nullptr))
        {
            vkCmdSetColorBlendEquationEXT(commandBuffer, 0u, numAttachments, blendEquations.data());
        }
        if ((numAttachments != 0u) && states.contains(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT) &&
            (dynamic_api::vkCmdSetColorWriteMaskEXT != nullptr))
        {
            vkCmdSetColorWriteMaskEXT(commandBuffer, 0u, numAttachments, writeMasks.data());
        }
    }

    // Vertex input.
    if (states.contains(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT) && (isFullUpdate || !last.hasSameVertexInput(desc)) &&
        (dynamic_api::vkCmdSetVertexInputEXT != nullptr))
    {
        const gp::UInt32 numBindings = static_cast<gp::UInt32>(
            (desc.vertexBindings.size() < kMaxVertexElementCount) ? desc.vertexBindings.size() : kMaxVertexElementCount
        );
        const gp::UInt32 numAttributes = static_cast<gp::UInt32>(
            (desc.vertexAttributes.size() < kMaxVertexElementCount) ? desc.vertexAttributes.size()
                                                                    : kMaxVertexElementCount
        );

        gp::Array<VkVertexInputBindingDescription2EXT, kMaxVertexElementCount> bindings{};
        for (gp::UInt32 index = 0u; index < numBindings; ++index)
        {
            const VulkanVertexBindingDesc& binding = desc.vertexBindings[index];
            bindings[index].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
            bindings[index].binding = binding.binding;
            bindings[index].stride = binding.stride;
            bindings[index].inputRate = static_cast<VkVertexInputRate>(binding.inputRate);
            bindings[index].divisor = 1u;
        }
        gp::Array<VkVertexInputAttributeDescription2EXT, kMaxVertexElementCount> attributes{};
        for (gp::UInt32 index = 0u; index < numAttributes; ++index)
        {
            const VulkanVertexAttributeDesc& attribute = desc.vertexAttributes[index];
            attributes[index].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
            attributes[index].location = attribute.location;
            attributes[index].binding = attribute.binding;
            attributes[index].format = static_cast<VkFormat>(attribute.format);
            attributes[index].offset = attribute.offset;
        }
        vkCmdSetVertexInputEXT(commandBuffer, numBindings, bindings.data(), numAttributes, attributes.data());
    }

    // Remember what the command buffer now holds.
    last.topology = desc.topology;
    last.rasterizer = desc.rasterizer;
    last.depthStencil = desc.depthStencil;
    last.rasterizationSamples = desc.rasterizationSamples;
    last.useAlphaToCoverage = desc.useAlphaToCoverage;
    last.shaderKeyShared = desc.shaderKeyShared;
    last.captureColorAttachments(desc);
    last.captureVertexInput(desc);
}

void VulkanPendingGfxState::bindVertexStreams(const VkCommandBuffer commandBuffer) noexcept
{
    const VulkanGraphicsPipelineDesc& desc = m_currentPipeline->getDesc();
    const gp::UInt32 numBindings = static_cast<gp::UInt32>(
        (desc.vertexBindings.size() < kMaxVertexElementCount) ? desc.vertexBindings.size() : kMaxVertexElementCount
    );
    if ((numBindings == 0u) || (commandBuffer == VK_NULL_HANDLE) || (dynamic_api::vkCmdBindVertexBuffers == nullptr))
    {
        return;
    }

    // Bindings are compacted: binding `i` reads the stream of the `i`-th set bit of the stream mask.
    gp::Array<VkBuffer, kMaxVertexElementCount> buffers{};
    gp::Array<VkDeviceSize, kMaxVertexElementCount> offsets{};
    gp::UInt32 firstBinding = 0u;
    gp::UInt32 numInRun = 0u;
    gp::UInt32 remainingStreams = desc.vertexStreamMask;
    for (gp::UInt32 binding = 0u; binding < numBindings; ++binding)
    {
        GP_CHECKF(remainingStreams != 0u, "Vertex binding {} has no stream in the pipeline's stream mask.", binding);
        gp::UInt32 streamIndex = 0u;
        while ((remainingStreams & (1u << streamIndex)) == 0u)
        {
            ++streamIndex;
        }
        remainingStreams &= ~(1u << streamIndex);

        const VulkanVertexStream& stream = m_pendingStreams[streamIndex];
        if (stream.buffer == VK_NULL_HANDLE)
        {
            GP_LOG_WARNING(LogVulkanRHI, "Vertex stream {} is unbound but the pipeline reads it.", streamIndex);
            if (numInRun != 0u)
            {
                vkCmdBindVertexBuffers(commandBuffer, firstBinding, numInRun, buffers.data(), offsets.data());
            }
            firstBinding = binding + 1u;
            numInRun = 0u;
            continue;
        }

        buffers[numInRun] = stream.buffer;
        offsets[numInRun] = stream.offset;
        ++numInRun;
    }
    if (numInRun != 0u)
    {
        vkCmdBindVertexBuffers(commandBuffer, firstBinding, numInRun, buffers.data(), offsets.data());
    }
}

const VulkanVertexStream& VulkanPendingGfxState::getVertexStream(const gp::UInt32 streamIndex) const noexcept
{
    GP_CHECK(streamIndex < kMaxVertexElementCount);
    return m_pendingStreams[streamIndex];
}

void VulkanPendingGfxState::notifyDeletedPipeline(const VulkanGraphicsPipelineState& pipeline) noexcept
{
    if (m_currentPipeline == &pipeline)
    {
        m_currentPipeline = nullptr;
        m_currentState = nullptr;
        m_pipelineDynamicStates = VulkanDynamicStateList{};
    }
    for (gp::USize index = 0u; index < static_cast<gp::USize>(m_pipelineStates.size()); ++index)
    {
        if (m_pipelineStates[index].pipeline == &pipeline)
        {
            m_pipelineStates.removeAt(index);
            return;
        }
    }
}

}   // namespace gp::rhi::vulkan
