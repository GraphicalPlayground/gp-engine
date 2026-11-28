// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "../pipeline/VulkanPipelineTestFakes.hpp"
#include "../queries/VulkanQueryTestFixture.hpp"
#include "containers/arrays/Vector.hpp"        // IWYU pragma: keep
#include "containers/views/VectorView.hpp"     // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "descriptors/VulkanDescriptorPoolsManager.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "memory/pointers/UniquePtr.hpp"
#include "pendingstate/VulkanPendingStateTypes.hpp"
#include "pipeline/BlendState.hpp"   // IWYU pragma: keep
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/ComputePipelineState.hpp"    // IWYU pragma: keep
#include "pipeline/DepthStencilState.hpp"       // IWYU pragma: keep
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineState.hpp"   // IWYU pragma: keep
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/RasterizerState.hpp"   // IWYU pragma: keep
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerState.hpp"      // IWYU pragma: keep
#include "pipeline/VulkanComputePipelineState.hpp"
#include "pipeline/VulkanGraphicsPipelineState.hpp"
#include "pipeline/VulkanPipelineStateCacheManager.hpp"
#include "pipeline/VulkanWrappedPipelineStates.hpp"
#include "shaders/ComputeShader.hpp"   // IWYU pragma: keep
#include "shaders/VertexShader.hpp"    // IWYU pragma: keep
#include "shaders/VulkanBaseShader.hpp"
#include "shaders/VulkanShaderFactory.hpp"
#include "states/VulkanSamplerState.hpp"
#include "states/VulkanStateRHI.hpp"
#include "views/VulkanViewTypes.hpp"
#include "VulkanPendingStateTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

/// @brief Live fixture for the pending states: a device and a graphics queue, real pipelines without descriptor
///        bindings, a pool manager and the fake context services.
/// @details Pipelines come from a pipeline state cache manager. Their shaders declare no binding, so the descriptor
///          states have no set and every flush exercises the pipeline, dynamic state and vertex stream paths.
/// @note Pending states keep pointers to pipelines: tests must destroy them before the pipeline references.
class VulkanPendingStateLiveTest : public VulkanQueryLiveTest
{
protected:
    virtual void SetUp() override
    {
        VulkanQueryLiveTest::SetUp();
        if (IsSkipped() || HasFatalFailure())
        {
            return;
        }

        pipeline::RasterizerStateDesc rasterizerDesc{};
        rasterizerDesc.fillMode = pipeline::RasterizerFillMode::Solid;
        m_blend = createVulkanBlendState(pipeline::BlendStateDesc{});
        m_rasterizer = createVulkanRasterizerState(rasterizerDesc);
        m_depthStencil = createVulkanDepthStencilState(pipeline::DepthStencilStateDesc{});

        const gp::Vector<gp::UInt8> computeBlob =
            makeTestShaderBlob(kSpirvExecutionModelGLCompute, 0x2468ace0u, "PendingCS");
        const gp::Vector<gp::UInt8> vertexBlob =
            makeTestShaderBlob(kSpirvExecutionModelVertex, 0x13579bdfu, "PendingVS");
        m_computeShader = m_factory.createShader<VulkanComputeShader>(
            *m_device, nullptr, gp::ConstVectorView<gp::UInt8>(computeBlob)
        );
        m_vertexShader =
            m_factory.createShader<VulkanVertexShader>(*m_device, nullptr, gp::ConstVectorView<gp::UInt8>(vertexBlob));
        m_pipelineManager = gp::makeUnique<VulkanPipelineStateCacheManager>(*m_device, nullptr, nullptr);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.maxLod = 1.0f;
        m_defaultSampler = pipeline::SamplerStateRef(new VulkanSamplerState(*m_device, samplerInfo, nullptr));
        m_defaultImageView.viewId = 1u;
        m_defaults.defaultSampler = static_cast<const VulkanSamplerState*>(m_defaultSampler.get());
        m_defaults.defaultImageView = &m_defaultImageView;

        m_poolsManager = gp::makeUnique<VulkanDescriptorPoolsManager>();
        m_poolsManager->init(*m_device);
        m_context = gp::makeUnique<FakePendingStateContext>(*m_poolsManager);
    }

    virtual void TearDown() override
    {
        // Pipelines hold shaders and layouts; shaders unregister from the factory: release in dependency order.
        m_context.reset();
        m_poolsManager.reset();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        m_pipelineManager.reset();
        m_computeShader.reset();
        m_vertexShader.reset();
        m_blend.reset();
        m_rasterizer.reset();
        m_depthStencil.reset();
        m_defaultSampler = pipeline::SamplerStateRef();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
        VulkanQueryLiveTest::TearDown();
    }

    /// @brief Creates the vertex-only graphics pipeline.
    /// @return Pipeline reference; invalid when the device renders through render passes.
    [[nodiscard]] pipeline::GraphicsPipelineStateRef createGraphicsPipeline() noexcept
    {
        if (!m_pipelineManager->usesDynamicRendering())
        {
            return pipeline::GraphicsPipelineStateRef();
        }
        pipeline::GraphicsPipelineStateInitializer initializer{};
        initializer.boundShaderState = pipeline::BoundShaderStateInput(nullptr, m_vertexShader.get(), nullptr);
        initializer.blendState = m_blend.get();
        initializer.rasterizerState = m_rasterizer.get();
        initializer.depthStencilState = m_depthStencil.get();
        initializer.primitiveType = pipeline::PrimitiveType::TriangleList;
        initializer.numSamples = 1u;
        return m_pipelineManager->createGraphicsPipelineState(initializer);
    }

    /// @brief Creates the compute pipeline.
    /// @return Pipeline reference.
    [[nodiscard]] pipeline::ComputePipelineStateRef createComputePipeline() noexcept
    {
        pipeline::ComputePipelineStateInitializer initializer{};
        initializer.computeShader = m_computeShader.get();
        return m_pipelineManager->createComputePipelineState(initializer);
    }

    /// @brief Returns the inner state behind a graphics wrapper.
    /// @param[in] ref Graphics pipeline reference.
    /// @return Inner state.
    [[nodiscard]] static VulkanGraphicsPipelineState*
        getInnerState(const pipeline::GraphicsPipelineStateRef& ref) noexcept
    {
        return static_cast<VulkanWrappedGraphicsPipelineState*>(ref.get())->getState();
    }

    /// @brief Returns the inner state behind a compute wrapper.
    /// @param[in] ref Compute pipeline reference.
    /// @return Inner state.
    [[nodiscard]] static VulkanComputePipelineState*
        getInnerState(const pipeline::ComputePipelineStateRef& ref) noexcept
    {
        return static_cast<VulkanWrappedComputePipelineState*>(ref.get())->getState();
    }

    /// @brief Shader factory.
    VulkanShaderFactory m_factory{};

    /// @brief Compute stage without bindings.
    gp::RefCountedPtr<VulkanComputeShader> m_computeShader{};

    /// @brief Vertex stage without bindings.
    gp::RefCountedPtr<VulkanVertexShader> m_vertexShader{};

    /// @brief Blend state.
    pipeline::BlendStateRef m_blend{};

    /// @brief Rasterizer state.
    pipeline::RasterizerStateRef m_rasterizer{};

    /// @brief Depth-stencil state.
    pipeline::DepthStencilStateRef m_depthStencil{};

    /// @brief Pipeline factory.
    gp::UniquePtr<VulkanPipelineStateCacheManager> m_pipelineManager{};

    /// @brief Default sampler.
    pipeline::SamplerStateRef m_defaultSampler{};

    /// @brief Default image view.
    VulkanTextureView m_defaultImageView{};

    /// @brief Defaults handed to the pending states.
    VulkanDescriptorDefaults m_defaults{};

    /// @brief Pool manager behind the context; destroyed before the device.
    gp::UniquePtr<VulkanDescriptorPoolsManager> m_poolsManager{};

    /// @brief Command context services.
    gp::UniquePtr<FakePendingStateContext> m_context{};
};

}   // namespace gp::rhi::vulkan::tests
