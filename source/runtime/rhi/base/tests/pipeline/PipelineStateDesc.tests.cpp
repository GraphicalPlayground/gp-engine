// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/arrays/Array.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashOf.hpp"
#include "pipeline/BlendStateDesc.hpp"
#include "pipeline/DepthStencilStateDesc.hpp"
#include "pipeline/RasterizerStateDesc.hpp"
#include "pipeline/SamplerStateDesc.hpp"
#include "pipeline/VertexDeclarationDesc.hpp"
#include "RHIDefinitions.hpp"
#include "templates/NumericLimits.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::pipeline::tests
{

static_assert(SamplerStateDesc{}.filter == SamplerFilter::Point, "Default sampler filter must be Point.");
static_assert(SamplerStateDesc{}.addressU == SamplerAddressMode::Wrap, "Default sampler addressU must be Wrap.");
static_assert(
    SamplerStateDesc{}.maxMipLevel == gp::NumericLimits<gp::Float32>::max(),
    "Default sampler maxMipLevel must be FLT_MAX."
);
static_assert(RasterizerStateDesc{}.fillMode == RasterizerFillMode::Point, "Default fill mode must be Point.");
static_assert(RasterizerStateDesc{}.cullMode == RasterizerCullMode::None, "Default cull mode must be None.");
static_assert(
    RasterizerStateDesc{}.depthClipMode == RasterizerDepthClipMode::DepthClip,
    "Default depth clip mode must be DepthClip."
);
static_assert(DepthStencilStateDesc{}.enableDepthWrite, "Default DepthStencilStateDesc must enable depth write.");
static_assert(
    DepthStencilStateDesc{}.depthTest == CompareFunction::LessEqual,
    "Default DepthStencilStateDesc depth test must be LessEqual."
);
static_assert(
    DepthStencilStateDesc{}.stencilReadMask == 0xFFu && DepthStencilStateDesc{}.stencilWriteMask == 0xFFu,
    "Default stencil masks must be 0xFF."
);
static_assert(
    RenderTargetBlendDesc{}.colorWriteMask == ColorWriteMask::RGBA,
    "Default RenderTargetBlendDesc write mask must be RGBA."
);
static_assert(!BlendStateDesc{}.useIndependentRenderTargetBlendStates, "Default BlendStateDesc must share target 0.");
static_assert(!VertexElement{}.isInstanced(), "Default VertexElement must advance per vertex.");
static_assert(
    VertexElement(1u, 16u, VertexElementType::Float4, 2u, 64u, true).isInstanced(),
    "Instanced VertexElement must report isInstanced."
);

TEST(PipelineStateDescTest, SamplerStateDescEqualityAndFloatHashConsistency)
{
    const SamplerStateDesc anisotropic(
        SamplerFilter::AnisotropicLinear,
        SamplerAddressMode::Clamp,
        SamplerAddressMode::Mirror,
        SamplerAddressMode::Border,
        -0.5f,
        16,
        0.0f,
        12.0f,
        0xFF0000FFu,
        SamplerCompareFunction::Less
    );
    const SamplerStateDesc anisotropicNegZero(
        SamplerFilter::AnisotropicLinear,
        SamplerAddressMode::Clamp,
        SamplerAddressMode::Mirror,
        SamplerAddressMode::Border,
        -0.5f,
        16,
        -0.0f,
        12.0f,
        0xFF0000FFu,
        SamplerCompareFunction::Less
    );
    const SamplerStateDesc differentAnisotropy(
        SamplerFilter::AnisotropicLinear,
        SamplerAddressMode::Clamp,
        SamplerAddressMode::Mirror,
        SamplerAddressMode::Border,
        -0.5f,
        8,
        0.0f,
        12.0f,
        0xFF0000FFu,
        SamplerCompareFunction::Less
    );

    EXPECT_EQ(anisotropic, anisotropicNegZero);
    EXPECT_EQ(gp::hashing::hashOf(anisotropic), gp::hashing::hashOf(anisotropicNegZero));
    EXPECT_NE(anisotropic, differentAnisotropy);
    EXPECT_NE(gp::hashing::hashOf(anisotropic), gp::hashing::hashOf(differentAnisotropy));

    gp::hashing::Fnv1a64 streamA;
    anisotropic.hashAppend(streamA);
    gp::hashing::Fnv1a64 streamB;
    anisotropicNegZero.hashAppend(streamB);
    EXPECT_EQ(streamA.finalize(), streamB.finalize());
}

TEST(PipelineStateDescTest, RasterizerStateDescConstructorsEqualityAndHashing)
{
    const RasterizerStateDesc solidCw(RasterizerFillMode::Solid, RasterizerCullMode::CW, true);
    const RasterizerStateDesc solidCwExplicit(
        RasterizerFillMode::Solid, RasterizerCullMode::CW, -0.0f, -0.0f, RasterizerDepthClipMode::DepthClip, true
    );
    const RasterizerStateDesc shadowPass(
        RasterizerFillMode::Solid, RasterizerCullMode::CCW, 0.001f, 2.0f, RasterizerDepthClipMode::DepthClamp, false
    );

    EXPECT_EQ(solidCw, solidCwExplicit);
    EXPECT_EQ(gp::hashing::hashOf(solidCw), gp::hashing::hashOf(solidCwExplicit));
    EXPECT_NE(solidCw, shadowPass);
    EXPECT_NE(gp::hashing::hashOf(solidCw), gp::hashing::hashOf(shadowPass));
}

TEST(PipelineStateDescTest, DepthStencilStateDescEqualityAndHashing)
{
    const DepthStencilStateDesc defaultDesc{};
    const DepthStencilStateDesc reversedZReadOnly(false, CompareFunction::DepthNearOrEqual);
    const DepthStencilStateDesc stencilMasking(
        true,
        CompareFunction::GreaterEqual,
        true,
        CompareFunction::NotEqual,
        StencilOp::Keep,
        StencilOp::Zero,
        StencilOp::Replace,
        true,
        CompareFunction::Equal,
        StencilOp::Decrement,
        StencilOp::Increment,
        StencilOp::Invert,
        0x0Fu,
        0xF0u
    );

    EXPECT_NE(defaultDesc, reversedZReadOnly);
    EXPECT_NE(defaultDesc, stencilMasking);
    EXPECT_NE(gp::hashing::hashOf(defaultDesc), gp::hashing::hashOf(reversedZReadOnly));
    EXPECT_NE(gp::hashing::hashOf(defaultDesc), gp::hashing::hashOf(stencilMasking));
    EXPECT_EQ(stencilMasking.stencilReadMask, 0x0Fu);
    EXPECT_EQ(stencilMasking.stencilWriteMask, 0xF0u);
}

TEST(PipelineStateDescTest, BlendStateDescSingleAndMultiTargetConstruction)
{
    const RenderTargetBlendDesc alphaBlend(
        BlendOperation::Add,
        BlendFactor::SourceAlpha,
        BlendFactor::InverseSourceAlpha,
        BlendOperation::Add,
        BlendFactor::One,
        BlendFactor::InverseSourceAlpha,
        ColorWriteMask::RGBA
    );
    const RenderTargetBlendDesc additiveRgb(
        BlendOperation::Add,
        BlendFactor::One,
        BlendFactor::One,
        BlendOperation::Add,
        BlendFactor::Zero,
        BlendFactor::One,
        ColorWriteMask::RGB
    );

    const BlendStateDesc singleTarget(alphaBlend, true);
    EXPECT_FALSE(singleTarget.useIndependentRenderTargetBlendStates);
    EXPECT_TRUE(singleTarget.useAlphaToCoverage);
    EXPECT_EQ(singleTarget.renderTargets[0], alphaBlend);
    EXPECT_EQ(singleTarget.renderTargets[1], RenderTargetBlendDesc{});

    const gp::Array<RenderTargetBlendDesc, 1u> oneElementArray{ { alphaBlend } };
    const BlendStateDesc fromSingleArray(oneElementArray, true);
    EXPECT_FALSE(fromSingleArray.useIndependentRenderTargetBlendStates);
    EXPECT_EQ(fromSingleArray, singleTarget);
    EXPECT_EQ(gp::hashing::hashOf(fromSingleArray), gp::hashing::hashOf(singleTarget));

    const gp::Array<RenderTargetBlendDesc, 2u> multiTargetArray{
        { alphaBlend, additiveRgb }
    };
    const BlendStateDesc multiTarget(multiTargetArray, false);
    EXPECT_TRUE(multiTarget.useIndependentRenderTargetBlendStates);
    EXPECT_FALSE(multiTarget.useAlphaToCoverage);
    EXPECT_EQ(multiTarget.renderTargets[0], alphaBlend);
    EXPECT_EQ(multiTarget.renderTargets[1], additiveRgb);
    EXPECT_EQ(multiTarget.renderTargets[2], RenderTargetBlendDesc{});
    EXPECT_NE(multiTarget, singleTarget);
    EXPECT_NE(gp::hashing::hashOf(multiTarget), gp::hashing::hashOf(singleTarget));
}

TEST(PipelineStateDescTest, VertexElementAndFixedElementList)
{
    static_assert(
        VertexDeclarationElementList::kInlineElementCapacity == kMaxVertexElementCount,
        "VertexDeclarationElementList inline capacity must match kMaxVertexElementCount."
    );

    VertexDeclarationElementList elements{};
    EXPECT_TRUE(elements.isEmpty());

    const VertexElement position(0u, 0u, VertexElementType::Float3, 0u, 32u, false);
    const VertexElement normal(0u, 12u, VertexElementType::PackedNormal, 1u, 32u, false);
    const VertexElement instanceMatrixRow(1u, 0u, VertexElementType::Float4, 4u, 64u, true);

    elements.emplaceBack(position);
    elements.emplaceBack(normal);
    elements.emplaceBack(instanceMatrixRow);

    EXPECT_EQ(elements.capacity(), kMaxVertexElementCount);
    ASSERT_EQ(elements.size(), 3u);
    EXPECT_EQ(elements[0], position);
    EXPECT_FALSE(elements[0].isInstanced());
    EXPECT_TRUE(elements[2].isInstanced());
    EXPECT_NE(position, normal);
    EXPECT_NE(gp::hashing::hashOf(position), gp::hashing::hashOf(normal));
}

}   // namespace gp::rhi::pipeline::tests
