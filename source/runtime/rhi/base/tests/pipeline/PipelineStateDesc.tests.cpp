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
    const SamplerStateDesc zeroBiasPosMax(
        SamplerFilter::Point,
        SamplerAddressMode::Wrap,
        SamplerAddressMode::Wrap,
        SamplerAddressMode::Wrap,
        0.0f,
        1,
        0.0f,
        0.0f,
        0u,
        SamplerCompareFunction::Never
    );
    const SamplerStateDesc negZeroBiasNegMax(
        SamplerFilter::Point,
        SamplerAddressMode::Wrap,
        SamplerAddressMode::Wrap,
        SamplerAddressMode::Wrap,
        -0.0f,
        1,
        -0.0f,
        -0.0f,
        0u,
        SamplerCompareFunction::Never
    );
    EXPECT_EQ(zeroBiasPosMax, negZeroBiasNegMax);
    EXPECT_EQ(gp::hashing::hashOf(zeroBiasPosMax), gp::hashing::hashOf(negZeroBiasNegMax));
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

    gp::hashing::Fnv1a64 rastStreamA{};
    gp::hashing::Fnv1a64 rastStreamB{};
    solidCw.hashAppend(rastStreamA);
    solidCwExplicit.hashAppend(rastStreamB);
    EXPECT_EQ(rastStreamA.finalize(), rastStreamB.finalize());
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

    gp::hashing::Fnv1a64 dsStreamA{};
    gp::hashing::Fnv1a64 dsStreamB{};
    stencilMasking.hashAppend(dsStreamA);
    reversedZReadOnly.hashAppend(dsStreamB);
    EXPECT_NE(dsStreamA.finalize(), dsStreamB.finalize());
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

    // Check useAlphaToCoverage mismatch and renderTargets[i] mismatch branches
    const BlendStateDesc singleNoAlphaToCov(alphaBlend, false);
    EXPECT_NE(singleTarget, singleNoAlphaToCov);

    const gp::Array<RenderTargetBlendDesc, 2u> multiTargetDiffSecond{
        { alphaBlend, RenderTargetBlendDesc{} }
    };
    const BlendStateDesc multiDiff(multiTargetDiffSecond, false);
    EXPECT_NE(multiTarget, multiDiff);

    EXPECT_NE(alphaBlend.getHash(), additiveRgb.getHash());
    gp::hashing::Fnv1a64 rtStreamA{};
    gp::hashing::Fnv1a64 rtStreamB{};
    alphaBlend.hashAppend(rtStreamA);
    additiveRgb.hashAppend(rtStreamB);
    EXPECT_NE(rtStreamA.finalize(), rtStreamB.finalize());

    gp::hashing::Fnv1a64 blendStreamA{};
    gp::hashing::Fnv1a64 blendStreamB{};
    singleTarget.hashAppend(blendStreamA);
    fromSingleArray.hashAppend(blendStreamB);
    EXPECT_EQ(blendStreamA.finalize(), blendStreamB.finalize());
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

    gp::hashing::Fnv1a64 veStreamA{};
    gp::hashing::Fnv1a64 veStreamB{};
    position.hashAppend(veStreamA);
    normal.hashAppend(veStreamB);
    EXPECT_NE(veStreamA.finalize(), veStreamB.finalize());
}

TEST(PipelineStateDescTest, PipelineEnumsConstantsAndBitwiseMasks)
{
    EXPECT_EQ(ColorWriteMask::Red | ColorWriteMask::Green, ColorWriteMask::RG);
    EXPECT_EQ(ColorWriteMask::Blue | ColorWriteMask::Alpha, ColorWriteMask::BA);
    EXPECT_EQ(ColorWriteMask::RG | ColorWriteMask::BA, ColorWriteMask::RGBA);
    EXPECT_EQ(ColorWriteMask::RGBA & ColorWriteMask::RGB, ColorWriteMask::RGB);

    EXPECT_EQ(CompareFunction::DepthNearOrEqual, CompareFunction::GreaterEqual);
    EXPECT_EQ(CompareFunction::DepthNear, CompareFunction::Greater);
    EXPECT_EQ(CompareFunction::DepthFartherOrEqual, CompareFunction::LessEqual);
    EXPECT_EQ(CompareFunction::DepthFarther, CompareFunction::Less);

    EXPECT_EQ(VRSShadingRate::Last, VRSShadingRate::Rate4x4);
    EXPECT_EQ(kSamplerFilterCount, 5u);
    EXPECT_EQ(kSamplerAddressModeCount, 4u);
    EXPECT_EQ(kRasterizerFillModeCount, 3u);
    EXPECT_EQ(kRasterizerCullModeCount, 3u);
    EXPECT_EQ(kRasterizerDepthClipModeCount, 2u);
    EXPECT_EQ(kColorWriteMaskNumBits, 4u);
    EXPECT_EQ(kCompareFunctionCount, 8u);
    EXPECT_EQ(kStencilMaskCount, 10u);
    EXPECT_EQ(kStencilOpCount, 8u);
    EXPECT_EQ(kBlendOperationCount, 5u);
    EXPECT_EQ(kBlendFactorCount, 16u);
    EXPECT_EQ(kVertexElementTypeCount, 22u);
    EXPECT_EQ(kPrimitiveTopologyTypeCount, 4u);
    EXPECT_EQ(kPrimitiveTypeCount, 6u);
}

TEST(PipelineStateDescTest, ToStringAndFromStringRoundTripAndValidation)
{
    // SamplerStateDesc round-trip (including default FLT_MAX maxMipLevel and negative mipBias)
    const SamplerStateDesc defaultSampler{};
    SamplerStateDesc parsedSampler(SamplerFilter::Trilinear);
    ASSERT_TRUE(parsedSampler.fromString(defaultSampler.toString().view()));
    EXPECT_EQ(parsedSampler, defaultSampler);

    const SamplerStateDesc customSampler(
        SamplerFilter::AnisotropicLinear,
        SamplerAddressMode::Clamp,
        SamplerAddressMode::Mirror,
        SamplerAddressMode::Border,
        -1.25f,
        16,
        0.5f,
        11.0f,
        0x80402010u,
        SamplerCompareFunction::Less
    );
    ASSERT_TRUE(parsedSampler.fromString(customSampler.toString().view()));
    EXPECT_EQ(parsedSampler, customSampler);

    // Malformed / out-of-bounds sampler strings must return false and leave parsedSampler intact.
    EXPECT_FALSE(parsedSampler.fromString(""));
    EXPECT_FALSE(parsedSampler.fromString("<0 0 0>"));
    EXPECT_FALSE(parsedSampler.fromString("<99 0 0 0 0 0 100 0 0 0>"));
    EXPECT_FALSE(parsedSampler.fromString("<0 4 0 0 0 0 100 0 0 0>"));
    EXPECT_FALSE(parsedSampler.fromString("<0 0 0 0 0 0 100 -1 0 0>"));
    EXPECT_FALSE(parsedSampler.fromString("<0 0 0 0 0 0 100 0 0 2>"));
    EXPECT_EQ(parsedSampler, customSampler);

    // RasterizerStateDesc round-trip and rejection
    const RasterizerStateDesc shadowRaster(
        RasterizerFillMode::Wireframe,
        RasterizerCullMode::CCW,
        -0.0025f,
        1.75f,
        RasterizerDepthClipMode::DepthClamp,
        true
    );
    RasterizerStateDesc parsedRaster{};
    ASSERT_TRUE(parsedRaster.fromString(shadowRaster.toString().view()));
    EXPECT_EQ(parsedRaster, shadowRaster);
    EXPECT_FALSE(parsedRaster.fromString("<0 0 3 0 0 1>"));
    EXPECT_FALSE(parsedRaster.fromString("<0 0 0 3 0 1>"));
    EXPECT_FALSE(parsedRaster.fromString("<0 0 0 0 2 1>"));
    EXPECT_FALSE(parsedRaster.fromString("<0 0 0 0 0 2>"));
    EXPECT_EQ(parsedRaster, shadowRaster);

    // DepthStencilStateDesc round-trip and rejection
    const DepthStencilStateDesc fullStencil(
        false,
        CompareFunction::GreaterEqual,
        true,
        CompareFunction::NotEqual,
        StencilOp::Keep,
        StencilOp::SaturatedIncrement,
        StencilOp::Replace,
        true,
        CompareFunction::Equal,
        StencilOp::SaturatedDecrement,
        StencilOp::Increment,
        StencilOp::Invert,
        0x3Cu,
        0xC3u
    );
    DepthStencilStateDesc parsedDepth{};
    ASSERT_TRUE(parsedDepth.fromString(fullStencil.toString().view()));
    EXPECT_EQ(parsedDepth, fullStencil);
    EXPECT_FALSE(parsedDepth.fromString("<1 8 0 7 0 0 0 0 7 0 0 0 255 255>"));
    EXPECT_FALSE(parsedDepth.fromString("<1 1 0 7 8 0 0 0 7 0 0 0 255 255>"));
    EXPECT_FALSE(parsedDepth.fromString("<1 1 0 7 0 0 0 0 7 0 0 0 256 255>"));
    EXPECT_EQ(parsedDepth, fullStencil);

    // RenderTargetBlendDesc + BlendStateDesc (58-token current and 57-token legacy format)
    const RenderTargetBlendDesc rtBlend0(
        BlendOperation::Subtract,
        BlendFactor::SourceAlpha,
        BlendFactor::InverseSourceAlpha,
        BlendOperation::Max,
        BlendFactor::One,
        BlendFactor::Zero,
        ColorWriteMask::RGB
    );
    const RenderTargetBlendDesc rtBlend1(
        BlendOperation::ReverseSubtract,
        BlendFactor::Source1Color,
        BlendFactor::InverseSource1Alpha,
        BlendOperation::Min,
        BlendFactor::ConstantBlendFactor,
        BlendFactor::InverseConstantBlendFactor,
        ColorWriteMask::BA
    );
    RenderTargetBlendDesc parsedRt{};
    ASSERT_TRUE(parsedRt.fromString(rtBlend0.toString().view()));
    EXPECT_EQ(parsedRt, rtBlend0);
    EXPECT_FALSE(parsedRt.fromString("0 1 0 0 1 0 16"));
    EXPECT_EQ(parsedRt, rtBlend0);

    const gp::Array<RenderTargetBlendDesc, 2u> blendTargets{
        { rtBlend0, rtBlend1 }
    };
    const BlendStateDesc blendDesc(blendTargets, true);
    BlendStateDesc parsedBlend{};
    ASSERT_TRUE(parsedBlend.fromString(blendDesc.toString().view()));
    EXPECT_EQ(parsedBlend, blendDesc);

    // Legacy 57-token BlendStateDesc omits trailing useAlphaToCoverage and defaults it to false.
    gp::String legacyBlendStr("<");
    for (gp::USize i = 0u; i < kMaxSimultaneousRenderTargets; ++i)
    {
        legacyBlendStr.append(blendDesc.renderTargets[i].toString());
    }
    legacyBlendStr.append("1>");
    BlendStateDesc parsedLegacyBlend(rtBlend0, true);
    ASSERT_TRUE(parsedLegacyBlend.fromString(legacyBlendStr.view()));
    EXPECT_TRUE(parsedLegacyBlend.useIndependentRenderTargetBlendStates);
    EXPECT_FALSE(parsedLegacyBlend.useAlphaToCoverage);
    EXPECT_EQ(parsedLegacyBlend.renderTargets[0], rtBlend0);
    EXPECT_EQ(parsedLegacyBlend.renderTargets[1], rtBlend1);
    EXPECT_FALSE(parsedLegacyBlend.fromString("<0 1 0>"));

    // VertexElement round-trip and bounds validation
    const VertexElement instancedElem(2u, 24u, VertexElementType::URGB10A2N, 5u, 64u, true);
    VertexElement parsedElem{};
    ASSERT_TRUE(parsedElem.fromString(instancedElem.toString().view()));
    EXPECT_EQ(parsedElem, instancedElem);
    EXPECT_FALSE(parsedElem.fromString("<0 0 99 0 16 0>"));
    EXPECT_FALSE(parsedElem.fromString("<256 0 1 0 16 0>"));
    EXPECT_FALSE(parsedElem.fromString("<0 0 1 0 70000 0>"));
    EXPECT_EQ(parsedElem, instancedElem);
}

}   // namespace gp::rhi::pipeline::tests
