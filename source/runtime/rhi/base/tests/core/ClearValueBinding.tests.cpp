// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/ClearValueBinding.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashOf.hpp"
#include "maths/color/LinearColor.hpp"
#include "templates/NumericLimits.hpp"
#include <bit>
#include <gtest/gtest.h>

namespace gp::rhi::resource::tests
{

static_assert(ClearValueBinding::kNone.colorBinding == ClearBinding::NoneBound, "kNone must use NoneBound.");
static_assert(ClearValueBinding::kBlack.colorBinding == ClearBinding::ColorBound, "kBlack must use ColorBound.");
static_assert(
    ClearValueBinding::kTransparent == ClearValueBinding{}, "Default ClearValueBinding must match kTransparent."
);
static_assert(
    ClearValueBinding::kDepthOne.colorBinding == ClearBinding::DepthStencilBound,
    "kDepthOne must use DepthStencilBound."
);
static_assert(ClearValueBinding::kDepthNear == ClearValueBinding::kDepthOne, "kDepthNear must match kDepthOne.");
static_assert(ClearValueBinding::kDepthFar == ClearValueBinding::kDepthZero, "kDepthFar must match kDepthZero.");
static_assert(ClearValueBinding::kBlack != ClearValueBinding::kWhite, "kBlack and kWhite must differ.");
static_assert(ClearValueBinding::kNone != ClearValueBinding::kTransparent, "NoneBound must not equal ColorBound.");
static_assert(
    ClearValueBinding::kDepthZero != ClearValueBinding::kTransparent, "DepthStencilBound must not equal ColorBound."
);
static_assert(
    gp::hashing::hashOf(ClearValueBinding::kBlack) == ClearValueBinding::kBlack.getHash(),
    "hashOf must dispatch to ClearValueBinding::getHash()."
);

TEST(ClearValueBindingTest, PresetsHaveExpectedValues)
{
    EXPECT_EQ(ClearValueBinding::kNone.colorBinding, ClearBinding::NoneBound);

    const gp::math::LinearColor black = ClearValueBinding::kBlack.getClearColor();
    EXPECT_FLOAT_EQ(black.r, 0.0f);
    EXPECT_FLOAT_EQ(black.g, 0.0f);
    EXPECT_FLOAT_EQ(black.b, 0.0f);
    EXPECT_FLOAT_EQ(black.a, 1.0f);

    const gp::math::LinearColor blackMaxAlpha = ClearValueBinding::kBlackMaxAlpha.getClearColor();
    EXPECT_FLOAT_EQ(blackMaxAlpha.r, 0.0f);
    EXPECT_FLOAT_EQ(blackMaxAlpha.g, 0.0f);
    EXPECT_FLOAT_EQ(blackMaxAlpha.b, 0.0f);
    EXPECT_FLOAT_EQ(blackMaxAlpha.a, gp::NumericLimits<gp::Float32>::max());

    const gp::math::LinearColor white = ClearValueBinding::kWhite.getClearColor();
    EXPECT_FLOAT_EQ(white.r, 1.0f);
    EXPECT_FLOAT_EQ(white.g, 1.0f);
    EXPECT_FLOAT_EQ(white.b, 1.0f);
    EXPECT_FLOAT_EQ(white.a, 1.0f);

    const gp::math::LinearColor green = ClearValueBinding::kGreen.getClearColor();
    EXPECT_FLOAT_EQ(green.r, 0.0f);
    EXPECT_FLOAT_EQ(green.g, 1.0f);
    EXPECT_FLOAT_EQ(green.b, 0.0f);
    EXPECT_FLOAT_EQ(green.a, 1.0f);

    const gp::math::LinearColor normal8 = ClearValueBinding::kDefaultNormal8Bit.getClearColor();
    EXPECT_FLOAT_EQ(normal8.r, 128.0f / 255.0f);
    EXPECT_FLOAT_EQ(normal8.g, 128.0f / 255.0f);
    EXPECT_FLOAT_EQ(normal8.b, 128.0f / 255.0f);
    EXPECT_FLOAT_EQ(normal8.a, 1.0f);

    gp::Float32 depth = -1.0f;
    gp::UInt32 stencil = 99u;
    ClearValueBinding::kDepthOne.getDepthStencil(depth, stencil);
    EXPECT_FLOAT_EQ(depth, 1.0f);
    EXPECT_EQ(stencil, 0u);

    ClearValueBinding::kDepthZero.getDepthStencil(depth, stencil);
    EXPECT_FLOAT_EQ(depth, 0.0f);
    EXPECT_EQ(stencil, 0u);
}

TEST(ClearValueBindingTest, ColorComparisonAndHashUseBitwiseSemantics)
{
    const ClearValueBinding posZero(gp::math::LinearColor(0.0f, 0.0f, 0.0f, 0.0f));
    const ClearValueBinding negZero(gp::math::LinearColor(-0.0f, 0.0f, 0.0f, 0.0f));

    EXPECT_NE(posZero, negZero);
    EXPECT_NE(gp::hashing::hashOf(posZero), gp::hashing::hashOf(negZero));

    const gp::Float32 nanSentinelA = std::bit_cast<gp::Float32>(0xFFFFFFFFu);
    const gp::Float32 nanSentinelB = std::bit_cast<gp::Float32>(0x7FC00001u);

    const ClearValueBinding clearNanA1(gp::math::LinearColor(nanSentinelA, 0.0f, 0.0f, 0.0f));
    const ClearValueBinding clearNanA2(gp::math::LinearColor(nanSentinelA, 0.0f, 0.0f, 0.0f));
    const ClearValueBinding clearNanB(gp::math::LinearColor(nanSentinelB, 0.0f, 0.0f, 0.0f));

    EXPECT_EQ(clearNanA1, clearNanA2);
    EXPECT_EQ(gp::hashing::hashOf(clearNanA1), gp::hashing::hashOf(clearNanA2));
    EXPECT_NE(clearNanA1, clearNanB);
    EXPECT_NE(gp::hashing::hashOf(clearNanA1), gp::hashing::hashOf(clearNanB));

    // Streaming hashAppend for ColorBound and NoneBound
    gp::hashing::Fnv1a64 posStream{};
    gp::hashing::Fnv1a64 negStream{};
    posZero.hashAppend(posStream);
    negZero.hashAppend(negStream);
    EXPECT_NE(posStream.finalize(), negStream.finalize());

    gp::hashing::Fnv1a64 nanStreamA1{};
    gp::hashing::Fnv1a64 nanStreamA2{};
    clearNanA1.hashAppend(nanStreamA1);
    clearNanA2.hashAppend(nanStreamA2);
    EXPECT_EQ(nanStreamA1.finalize(), nanStreamA2.finalize());

    const ClearValueBinding explicitNone(ClearBinding::NoneBound);
    EXPECT_EQ(explicitNone, ClearValueBinding::kNone);
    EXPECT_EQ(explicitNone.getHash(), ClearValueBinding::kNone.getHash());
    gp::hashing::Fnv1a64 noneStreamA{};
    gp::hashing::Fnv1a64 noneStreamB{};
    explicitNone.hashAppend(noneStreamA);
    ClearValueBinding::kNone.hashAppend(noneStreamB);
    EXPECT_EQ(noneStreamA.finalize(), noneStreamB.finalize());
    EXPECT_NE(noneStreamA.finalize(), posStream.finalize());
}

TEST(ClearValueBindingTest, DepthStencilComparisonAndHashUseFloatSemantics)
{
    const ClearValueBinding depthOnlyDefaultStencil(0.25f);
    gp::Float32 defaultStencilDepth = 0.0f;
    gp::UInt32 defaultStencilVal = 99u;
    depthOnlyDefaultStencil.getDepthStencil(defaultStencilDepth, defaultStencilVal);
    EXPECT_FLOAT_EQ(defaultStencilDepth, 0.25f);
    EXPECT_EQ(defaultStencilVal, 0u);
    const ClearValueBinding posZeroDepth(0.0f, 7u);
    const ClearValueBinding negZeroDepth(-0.0f, 7u);
    const ClearValueBinding differentStencil(0.0f, 8u);
    const ClearValueBinding differentDepth(0.5f, 7u);

    EXPECT_EQ(posZeroDepth, negZeroDepth);
    EXPECT_EQ(gp::hashing::hashOf(posZeroDepth), gp::hashing::hashOf(negZeroDepth));

    EXPECT_NE(posZeroDepth, differentStencil);
    EXPECT_NE(posZeroDepth, differentDepth);

    gp::Float32 outDepth = 0.0f;
    gp::UInt32 outStencil = 0u;
    differentDepth.getDepthStencil(outDepth, outStencil);
    EXPECT_FLOAT_EQ(outDepth, 0.5f);
    EXPECT_EQ(outStencil, 7u);

    // Streaming hashAppend must also agree for -0.0f and +0.0f depth values.
    gp::hashing::Fnv1a64 hasherPos;
    posZeroDepth.hashAppend(hasherPos);
    gp::hashing::Fnv1a64 hasherNeg;
    negZeroDepth.hashAppend(hasherNeg);
    EXPECT_EQ(hasherPos.finalize(), hasherNeg.finalize());
}

}   // namespace gp::rhi::resource::tests
