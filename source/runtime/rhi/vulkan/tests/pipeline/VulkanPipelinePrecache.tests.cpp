// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanPipelinePrecache.hpp"
#include "containers/arrays/Vector.hpp"      // IWYU pragma: keep
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "core/ResourceDeletionQueue.hpp"
#include "CoreMinimal.hpp"                   // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/VulkanDevice.hpp"
#include "hashing/HashingTypes.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/BlendState.hpp"             // IWYU pragma: keep
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/DepthStencilState.hpp"   // IWYU pragma: keep
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/RasterizerState.hpp"     // IWYU pragma: keep
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/VertexDeclaration.hpp"   // IWYU pragma: keep
#include "pipeline/VertexDeclarationDesc.hpp"
#include "resources/TextureEnums.hpp"
#include "shaders/VertexShader.hpp"   // IWYU pragma: keep
#include "shaders/VulkanBaseShader.hpp"
#include "shaders/VulkanShaderFactory.hpp"
#include "states/VulkanStateRHI.hpp"
#include "states/VulkanVertexDeclaration.hpp"
#include "VulkanPipelineTestFakes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Single-stream position declaration.
/// @details The precache hash of a declaration ignores strides, so tests vary the element format instead.
/// @param[in] isWide True for a 4-component position, false for a 3-component one.
/// @return Element list.
[[nodiscard]] pipeline::VertexDeclarationElementList makeElements(const bool isWide) noexcept
{
    const pipeline::VertexElementType type =
        isWide ? pipeline::VertexElementType::Float4 : pipeline::VertexElementType::Float3;
    const gp::UInt16 stride = isWide ? 16u : 12u;
    pipeline::VertexDeclarationElementList elements;
    elements.emplaceBack(pipeline::VertexElement(0u, 0u, type, 0u, stride));
    return elements;
}

/// @brief Fixture owning one set of fixed-function states and an initializer that points at them.
class VulkanPipelinePrecacheTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        VulkanVertexDeclaration::emptyCache();
        m_blend = createVulkanBlendState(pipeline::BlendStateDesc{});
        m_rasterizer = createVulkanRasterizerState(pipeline::RasterizerStateDesc{});
        m_depthStencil = createVulkanDepthStencilState(pipeline::DepthStencilStateDesc{});
        m_initializer.blendState = m_blend.get();
        m_initializer.rasterizerState = m_rasterizer.get();
        m_initializer.depthStencilState = m_depthStencil.get();
    }

    virtual void TearDown() override
    {
        m_blend.reset();
        m_rasterizer.reset();
        m_depthStencil.reset();
        VulkanVertexDeclaration::emptyCache();
        resource::ResourceDeletionQueue::flushPendingDeletes(true);
    }

    /// @brief Shared blend state.
    pipeline::BlendStateRef m_blend{};

    /// @brief Shared rasterizer state.
    pipeline::RasterizerStateRef m_rasterizer{};

    /// @brief Shared depth-stencil state.
    pipeline::DepthStencilStateRef m_depthStencil{};

    /// @brief Initializer referencing the states above.
    pipeline::GraphicsPipelineStateInitializer m_initializer{};
};

}   // namespace

TEST_F(VulkanPipelinePrecacheTest, HashesAreDeterministic)
{
    const pipeline::GraphicsPipelineStateInitializer copy = m_initializer;

    EXPECT_EQ(
        computeVulkanStatePrecachePSOHash(m_initializer, GpuVendorId::Nvidia),
        computeVulkanStatePrecachePSOHash(copy, GpuVendorId::Nvidia)
    );
    EXPECT_EQ(
        computeVulkanPrecachePSOHash(m_initializer, GpuVendorId::Nvidia),
        computeVulkanPrecachePSOHash(copy, GpuVendorId::Nvidia)
    );
}

TEST_F(VulkanPipelinePrecacheTest, StateHashFollowsFixedFunctionFields)
{
    const gp::UInt64 baseHash = computeVulkanStatePrecachePSOHash(m_initializer, GpuVendorId::Nvidia);

    pipeline::GraphicsPipelineStateInitializer lines = m_initializer;
    lines.primitiveType = pipeline::PrimitiveType::LineList;
    pipeline::GraphicsPipelineStateInitializer depthBounds = m_initializer;
    depthBounds.depthBounds = true;
    const pipeline::BlendStateRef otherBlend = createVulkanBlendState(
        pipeline::BlendStateDesc(pipeline::RenderTargetBlendDesc(pipeline::BlendOperation::Subtract), true)
    );
    pipeline::GraphicsPipelineStateInitializer blend = m_initializer;
    blend.blendState = otherBlend.get();

    EXPECT_NE(baseHash, computeVulkanStatePrecachePSOHash(lines, GpuVendorId::Nvidia));
    EXPECT_NE(baseHash, computeVulkanStatePrecachePSOHash(depthBounds, GpuVendorId::Nvidia));
    EXPECT_NE(baseHash, computeVulkanStatePrecachePSOHash(blend, GpuVendorId::Nvidia));
}

TEST_F(VulkanPipelinePrecacheTest, EqualStateContentHashesEqually)
{
    const pipeline::BlendStateRef sameBlend = createVulkanBlendState(pipeline::BlendStateDesc{});
    pipeline::GraphicsPipelineStateInitializer other = m_initializer;
    other.blendState = sameBlend.get();

    ASSERT_NE(sameBlend.get(), m_blend.get());
    EXPECT_EQ(
        computeVulkanStatePrecachePSOHash(m_initializer, GpuVendorId::Nvidia),
        computeVulkanStatePrecachePSOHash(other, GpuVendorId::Nvidia)
    );
}

TEST_F(VulkanPipelinePrecacheTest, ArmIgnoresTheVertexDeclaration)
{
    const pipeline::VertexDeclarationRef narrow = createVulkanVertexDeclaration(makeElements(false));
    const pipeline::VertexDeclarationRef wide = createVulkanVertexDeclaration(makeElements(true));
    ASSERT_NE(narrow.get(), wide.get());

    pipeline::GraphicsPipelineStateInitializer first = m_initializer;
    first.boundShaderState.setVertexDeclaration(narrow.get());
    pipeline::GraphicsPipelineStateInitializer second = m_initializer;
    second.boundShaderState.setVertexDeclaration(wide.get());

    EXPECT_NE(
        computeVulkanStatePrecachePSOHash(first, GpuVendorId::Nvidia),
        computeVulkanStatePrecachePSOHash(second, GpuVendorId::Nvidia)
    );
    EXPECT_EQ(
        computeVulkanStatePrecachePSOHash(first, GpuVendorId::Arm),
        computeVulkanStatePrecachePSOHash(second, GpuVendorId::Arm)
    );
}

TEST_F(VulkanPipelinePrecacheTest, ProvidedStateHashIsReused)
{
    pipeline::GraphicsPipelineStateInitializer first = m_initializer;
    first.statePrecachePSOHash = 0xABCDu;
    pipeline::GraphicsPipelineStateInitializer second = first;
    second.primitiveType = pipeline::PrimitiveType::LineList;

    EXPECT_EQ(
        computeVulkanPrecachePSOHash(first, GpuVendorId::Nvidia),
        computeVulkanPrecachePSOHash(second, GpuVendorId::Nvidia)
    );
    EXPECT_NE(
        computeVulkanPrecachePSOHash(first, GpuVendorId::Nvidia),
        computeVulkanPrecachePSOHash(m_initializer, GpuVendorId::Nvidia)
    );
}

TEST_F(VulkanPipelinePrecacheTest, FullHashFollowsRenderTargetsButMasksIrrelevantFlags)
{
    const gp::UInt64 baseHash = computeVulkanPrecachePSOHash(m_initializer, GpuVendorId::Nvidia);

    pipeline::GraphicsPipelineStateInitializer irrelevant = m_initializer;
    irrelevant.renderTargetFlags[0] = resource::TextureCreateFlags::RenderTargetable;
    pipeline::GraphicsPipelineStateInitializer srgb = m_initializer;
    srgb.renderTargetFlags[0] = resource::TextureCreateFlags::SRGB;
    pipeline::GraphicsPipelineStateInitializer samples = m_initializer;
    samples.numSamples = 4u;
    pipeline::GraphicsPipelineStateInitializer subpass = m_initializer;
    subpass.subpassIndex = 1u;

    EXPECT_EQ(baseHash, computeVulkanPrecachePSOHash(irrelevant, GpuVendorId::Nvidia));
    EXPECT_NE(baseHash, computeVulkanPrecachePSOHash(srgb, GpuVendorId::Nvidia));
    EXPECT_NE(baseHash, computeVulkanPrecachePSOHash(samples, GpuVendorId::Nvidia));
    EXPECT_NE(baseHash, computeVulkanPrecachePSOHash(subpass, GpuVendorId::Nvidia));
}

TEST_F(VulkanPipelinePrecacheTest, ShaderBytecodeHashReachesTheStateHash)
{
    VulkanDevice offlineDevice(VK_NULL_HANDLE, VK_NULL_HANDLE, VK_API_VERSION_1_3, false);
    VulkanShaderFactory factory{};
    {
        const gp::Vector<gp::UInt8> blob = makeTestShaderBlob(kSpirvExecutionModelVertex, 1u, "Precache");
        const gp::RefCountedPtr<VulkanVertexShader> vertexShader =
            factory.createShader<VulkanVertexShader>(offlineDevice, nullptr, gp::ConstVectorView<gp::UInt8>(blob));
        ASSERT_TRUE(vertexShader.isValid());

        pipeline::GraphicsPipelineStateInitializer initializer = m_initializer;
        initializer.boundShaderState.setVertexShader(vertexShader.get());

        vertexShader->setHash(gp::hashing::Hash128(1u, 2u));
        const gp::UInt64 firstHash = computeVulkanStatePrecachePSOHash(initializer, GpuVendorId::Nvidia);
        vertexShader->setHash(gp::hashing::Hash128(3u, 4u));
        const gp::UInt64 secondHash = computeVulkanStatePrecachePSOHash(initializer, GpuVendorId::Nvidia);

        EXPECT_NE(firstHash, secondHash);
        EXPECT_NE(firstHash, computeVulkanStatePrecachePSOHash(m_initializer, GpuVendorId::Nvidia));
    }
    resource::ResourceDeletionQueue::flushPendingDeletes(true);
}

TEST_F(VulkanPipelinePrecacheTest, MatchAcceptsEqualContent)
{
    const pipeline::BlendStateRef sameBlend = createVulkanBlendState(pipeline::BlendStateDesc{});
    pipeline::GraphicsPipelineStateInitializer other = m_initializer;
    other.blendState = sameBlend.get();
    other.renderTargetFlags[2] = resource::TextureCreateFlags::RenderTargetable;

    EXPECT_TRUE(matchVulkanPrecachePSOInitializers(m_initializer, m_initializer));
    EXPECT_TRUE(matchVulkanPrecachePSOInitializers(m_initializer, other));
}

TEST_F(VulkanPipelinePrecacheTest, MatchRejectsDifferentInitializers)
{
    pipeline::GraphicsPipelineStateInitializer samples = m_initializer;
    samples.numSamples = 2u;
    pipeline::GraphicsPipelineStateInitializer srgb = m_initializer;
    srgb.renderTargetFlags[0] = resource::TextureCreateFlags::SRGB;
    const pipeline::BlendStateRef otherBlend = createVulkanBlendState(
        pipeline::BlendStateDesc(pipeline::RenderTargetBlendDesc(pipeline::BlendOperation::Subtract), true)
    );
    pipeline::GraphicsPipelineStateInitializer blend = m_initializer;
    blend.blendState = otherBlend.get();
    const pipeline::VertexDeclarationRef declaration = createVulkanVertexDeclaration(makeElements(false));
    pipeline::GraphicsPipelineStateInitializer vertexInput = m_initializer;
    vertexInput.boundShaderState.setVertexDeclaration(declaration.get());

    EXPECT_FALSE(matchVulkanPrecachePSOInitializers(m_initializer, samples));
    EXPECT_FALSE(matchVulkanPrecachePSOInitializers(m_initializer, srgb));
    EXPECT_FALSE(matchVulkanPrecachePSOInitializers(m_initializer, blend));
    EXPECT_FALSE(matchVulkanPrecachePSOInitializers(m_initializer, vertexInput));
}

}   // namespace gp::rhi::vulkan::tests
