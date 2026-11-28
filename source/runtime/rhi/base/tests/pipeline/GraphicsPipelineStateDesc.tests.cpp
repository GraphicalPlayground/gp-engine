// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/GraphicsPipelineStateDesc.hpp"
#include "commands/CommandEnums.hpp"
#include "core/ExclusiveDepthStencil.hpp"
#include "pipeline/BlendState.hpp"
#include "pipeline/BoundShaderState.hpp"
#include "pipeline/DepthStencilState.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/PipelineStateFallbacks.hpp"
#include "pipeline/RasterizerState.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "resources/TextureEnums.hpp"
#include "RHIPixelFormat.hpp"
#include "shaders/ComputeShader.hpp"
#include "shaders/PixelShader.hpp"
#include "shaders/VertexShader.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::pipeline::tests
{

namespace
{

TEST(GraphicsPipelineStateDescTest, RenderTargetsInfoDefaultValues)
{
    const GraphicsPipelineRenderTargetsInfo info{};
    EXPECT_EQ(info.renderTargetsEnabled, 0u);
    EXPECT_EQ(info.depthStencilTargetFormat, PixelFormat::Unknown);
    EXPECT_EQ(info.depthStencilTargetFlag, resource::TextureCreateFlags::None);
    EXPECT_EQ(info.numSamples, 1u);
    EXPECT_EQ(info.subpassHint, SubpassHint::None);
    EXPECT_EQ(info.subpassIndex, 0u);
    EXPECT_EQ(info.multiViewCount, 0u);
    EXPECT_FALSE(info.hasFragmentDensityAttachment);

    for (gp::UInt32 i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
    {
        EXPECT_EQ(info.renderTargetFormats[i], PixelFormat::Unknown);
        EXPECT_EQ(info.renderTargetFlags[i], resource::TextureCreateFlags::None);
    }
}

TEST(GraphicsPipelineStateDescTest, ApplyRenderTargetsInfoAndCountValidTargets)
{
    GraphicsPipelineRenderTargetsInfo rtInfo{};
    rtInfo.renderTargetsEnabled = 3u;
    rtInfo.renderTargetFormats[0] = PixelFormat::R8G8B8A8_UNorm;
    rtInfo.renderTargetFormats[1] = PixelFormat::R16G16B16A16_Float;
    rtInfo.renderTargetFormats[2] = PixelFormat::Unknown;
    rtInfo.renderTargetFlags[0] = resource::TextureCreateFlags::RenderTargetable | resource::TextureCreateFlags::SRGB;
    rtInfo.depthStencilTargetFormat = PixelFormat::D32_Float_S8_UInt;
    rtInfo.depthStencilTargetFlag = resource::TextureCreateFlags::DepthStencilTargetable;
    rtInfo.depthTargetLoadAction = command::RenderTargetLoadAction::Clear;
    rtInfo.depthTargetStoreAction = command::RenderTargetStoreAction::Store;
    rtInfo.stencilTargetLoadAction = command::RenderTargetLoadAction::Load;
    rtInfo.stencilTargetStoreAction = command::RenderTargetStoreAction::NoAction;
    rtInfo.depthStencilAccess = resource::ExclusiveDepthStencil::kDepthWriteStencilRead;
    rtInfo.numSamples = 4u;
    rtInfo.subpassHint = SubpassHint::DeferredShadingSubpass;
    rtInfo.subpassIndex = 1u;
    rtInfo.multiViewCount = 2u;
    rtInfo.hasFragmentDensityAttachment = true;

    GraphicsPipelineStateInitializer init{};
    init.applyRenderTargetsInfo(rtInfo);

    EXPECT_EQ(init.renderTargetsEnabled, 3u);
    EXPECT_EQ(init.computeNumValidRenderTargets(), 2u);
    EXPECT_EQ(init.renderTargetFormats[0], PixelFormat::R8G8B8A8_UNorm);
    EXPECT_EQ(init.renderTargetFormats[1], PixelFormat::R16G16B16A16_Float);
    EXPECT_EQ(init.depthStencilTargetFormat, PixelFormat::D32_Float_S8_UInt);
    EXPECT_EQ(init.depthTargetLoadAction, command::RenderTargetLoadAction::Clear);
    EXPECT_EQ(init.depthTargetStoreAction, command::RenderTargetStoreAction::Store);
    EXPECT_EQ(init.stencilTargetLoadAction, command::RenderTargetLoadAction::Load);
    EXPECT_EQ(init.stencilTargetStoreAction, command::RenderTargetStoreAction::NoAction);
    EXPECT_EQ(init.depthStencilAccess, resource::ExclusiveDepthStencil::kDepthWriteStencilRead);
    EXPECT_EQ(init.numSamples, 4u);
    EXPECT_EQ(init.subpassHint, SubpassHint::DeferredShadingSubpass);
    EXPECT_EQ(init.subpassIndex, 1u);
    EXPECT_EQ(init.multiViewCount, 2u);
    EXPECT_TRUE(init.hasFragmentDensityAttachment);
}

TEST(GraphicsPipelineStateDescTest, EqualityMasksIrrelevantFlagsAndIgnoresPrecacheHints)
{
    VertexDeclarationRef vertexDecl(new VertexDeclaration());
    shader::VertexShaderRef vs(new shader::VertexShader());
    shader::PixelShaderRef ps(new shader::PixelShader());
    BlendStateRef blend(new BlendState());
    RasterizerStateRef raster(new RasterizerState());
    DepthStencilStateRef depth(new DepthStencilState());

    GraphicsPipelineStateInitializer lhs{};
    lhs.boundShaderState = BoundShaderStateInput(vertexDecl.get(), vs.get(), ps.get());
    lhs.blendState = blend.get();
    lhs.rasterizerState = raster.get();
    lhs.depthStencilState = depth.get();
    lhs.primitiveType = PrimitiveType::TriangleList;
    lhs.renderTargetsEnabled = 1u;
    lhs.renderTargetFormats[0] = PixelFormat::B8G8R8A8_UNorm;
    lhs.renderTargetFlags[0] = resource::TextureCreateFlags::SRGB | resource::TextureCreateFlags::ShaderResource;
    lhs.depthStencilTargetFormat = PixelFormat::D24_UNorm_S8_UInt;
    lhs.depthStencilTargetFlag =
        resource::TextureCreateFlags::DepthStencilTargetable | resource::TextureCreateFlags::ShaderResource;

    GraphicsPipelineStateInitializer rhs = lhs;
    // Toggle flags outside the PSO-relevant masks and modify precache hint bits.
    rhs.renderTargetFlags[0] = resource::TextureCreateFlags::SRGB | resource::TextureCreateFlags::RenderTargetable;
    rhs.depthStencilTargetFlag =
        resource::TextureCreateFlags::DepthStencilTargetable | resource::TextureCreateFlags::Memoryless;
    rhs.setPSOPrecache(true);
    rhs.setFromPSOFileCache(true);
    rhs.setPrecacheCompileType(PSOPrecacheCompileType::PrePass);
    rhs.statePrecachePSOHash = 0x123456789ABCDEF0ull;

    EXPECT_TRUE(rhs.isPSOPrecache());
    EXPECT_TRUE(rhs.isFromPSOFileCache());
    EXPECT_EQ(rhs.getPrecacheCompileType(), PSOPrecacheCompileType::PrePass);
    EXPECT_EQ(lhs, rhs);

    // Changing a relevant flag (SRGB) breaks equality.
    rhs.renderTargetFlags[0] = resource::TextureCreateFlags::RenderTargetable;
    EXPECT_NE(lhs, rhs);

    rhs.renderTargetFlags[0] = lhs.renderTargetFlags[0];
    rhs.depthBounds = true;
    EXPECT_NE(lhs, rhs);
}

TEST(GraphicsPipelineStateDescTest, ComputePipelineStateInitializerEqualityAndHints)
{
    shader::ComputeShaderRef cs1(new shader::ComputeShader());
    shader::ComputeShaderRef cs2(new shader::ComputeShader());

    ComputePipelineStateInitializer lhs(cs1.get());
    ComputePipelineStateInitializer rhs(cs1.get());
    rhs.setPSOPrecache(true);
    rhs.setFromPSOFileCache(true);

    EXPECT_EQ(lhs, rhs);

    rhs.computeShader = cs2.get();
    EXPECT_NE(lhs, rhs);
}

TEST(GraphicsPipelineStateDescTest, PipelineStateFallbacksPreserveInitializerAndShaders)
{
    VertexDeclarationRef vertexDecl(new VertexDeclaration());
    shader::VertexShaderRef vs(new shader::VertexShader());
    shader::PixelShaderRef ps(new shader::PixelShader());
    shader::ComputeShaderRef cs(new shader::ComputeShader());
    BoundShaderStateRef bss(new BoundShaderState());

    GraphicsPipelineStateInitializer graphicsInit{};
    graphicsInit.boundShaderState = BoundShaderStateInput(vertexDecl.get(), vs.get(), ps.get());
    graphicsInit.primitiveType = PrimitiveType::LineList;
    graphicsInit.depthBounds = true;

    GraphicsPipelineStateRef graphicsPSO(new GraphicsPipelineStateFallback(graphicsInit, bss));
    ASSERT_TRUE(graphicsPSO.isValid());
    const GraphicsPipelineStateFallback* const graphicsFallback =
        static_cast<const GraphicsPipelineStateFallback*>(graphicsPSO.get());
    EXPECT_EQ(graphicsFallback->initializer, graphicsInit);
    EXPECT_EQ(graphicsFallback->boundShaderState.get(), bss.get());

    ComputePipelineStateRef computePSO(new ComputePipelineStateFallback(cs.get()));
    ASSERT_TRUE(computePSO.isValid());
    const ComputePipelineStateFallback* const computeFallback =
        static_cast<const ComputePipelineStateFallback*>(computePSO.get());
    EXPECT_EQ(computeFallback->getComputeShader(), cs.get());
}

}   // namespace

}   // namespace gp::rhi::pipeline::tests
