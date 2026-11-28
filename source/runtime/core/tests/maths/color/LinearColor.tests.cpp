// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/color/LinearColor.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/IConsoleManager.hpp"
#include "containers/arrays/Vector.hpp"
#include "containers/strings/String.hpp"     // IWYU pragma: keep
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "hashing/HashOf.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/ColorSpace.hpp"
#include "maths/vector/Vector3.hpp"
#include "maths/vector/Vector4.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace gp::math::tests
{

namespace
{

constexpr bool testConstexprLinearColorOperations() noexcept
{
    const LinearColor zero{};
    if (zero.r != 0.0f || zero.g != 0.0f || zero.b != 0.0f || zero.a != 0.0f)
    {
        return false;
    }

    const LinearColor c1(0.25f, 0.5f, 0.75f, 1.0f);
    const LinearColor c2(0.25f, 0.25f, 0.25f, 0.5f);
    const LinearColor sum = c1 + c2;
    if (sum.r != 0.5f || sum.g != 0.75f || sum.b != 1.0f || sum.a != 1.5f)
    {
        return false;
    }

    const LinearColor clamped = sum.getClamped(0.0f, 1.0f);
    if (clamped.a != 1.0f || clamped.getMax() != 1.0f || clamped.getMin() != 0.5f)
    {
        return false;
    }

    const LinearColor fromSrgb(Color(0u, 255u, 0u, 255u));
    if (fromSrgb.r != 0.0f || fromSrgb.g != 1.0f || fromSrgb.b != 0.0f || fromSrgb.a != 1.0f)
    {
        return false;
    }

    return true;
}

static_assert(testConstexprLinearColorOperations(), "Constexpr LinearColor evaluation failed.");

}   // namespace

TEST(LinearColorTest, ConstructorsAndVectorConversions)
{
    const LinearColor def{};
    EXPECT_FLOAT_EQ(def.r, 0.0f);
    EXPECT_FLOAT_EQ(def.g, 0.0f);
    EXPECT_FLOAT_EQ(def.b, 0.0f);
    EXPECT_FLOAT_EQ(def.a, 0.0f);

    const LinearColor rgbOnly(0.1f, 0.2f, 0.3f);
    EXPECT_FLOAT_EQ(rgbOnly.r, 0.1f);
    EXPECT_FLOAT_EQ(rgbOnly.g, 0.2f);
    EXPECT_FLOAT_EQ(rgbOnly.b, 0.3f);
    EXPECT_FLOAT_EQ(rgbOnly.a, 1.0f);

    const Vector3<gp::Float32> v3f(0.4f, 0.5f, 0.6f);
    const LinearColor fromV3f(v3f);
    EXPECT_EQ(fromV3f, LinearColor(0.4f, 0.5f, 0.6f, 1.0f));
    EXPECT_EQ(fromV3f.toVector3(), v3f);

    const Vector3<gp::Float64> v3d(0.7, 0.8, 0.9);
    const LinearColor fromV3d(v3d);
    EXPECT_TRUE(fromV3d.equals(LinearColor(0.7f, 0.8f, 0.9f, 1.0f)));

    const Vector4<gp::Float32> v4f(0.1f, 0.2f, 0.3f, 0.4f);
    const LinearColor fromV4f(v4f);
    EXPECT_EQ(fromV4f, LinearColor(0.1f, 0.2f, 0.3f, 0.4f));
    EXPECT_EQ(fromV4f.toVector4(), v4f);

    const Vector4<gp::Float64> v4d(0.5, 0.6, 0.7, 0.8);
    const LinearColor fromV4d(v4d);
    EXPECT_TRUE(fromV4d.equals(LinearColor(0.5f, 0.6f, 0.7f, 0.8f)));
}

TEST(LinearColorTest, ComponentAccessAndArithmeticOperators)
{
    LinearColor c(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(c.component(0), 1.0f);
    EXPECT_FLOAT_EQ(c.component(1), 2.0f);
    EXPECT_FLOAT_EQ(c.component(2), 3.0f);
    EXPECT_FLOAT_EQ(c.component(3), 4.0f);

    c[0] = 2.0f;
    c[1] = 4.0f;
    c[2] = 6.0f;
    c[3] = 8.0f;
    const LinearColor& constRef = c;
    EXPECT_FLOAT_EQ(constRef[0], 2.0f);
    EXPECT_FLOAT_EQ(constRef[1], 4.0f);
    EXPECT_FLOAT_EQ(constRef[2], 6.0f);
    EXPECT_FLOAT_EQ(constRef[3], 8.0f);

    const LinearColor b(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_EQ(c + b, LinearColor(3.0f, 6.0f, 9.0f, 12.0f));
    EXPECT_EQ(c - b, LinearColor(1.0f, 2.0f, 3.0f, 4.0f));
    EXPECT_EQ(c * b, LinearColor(2.0f, 8.0f, 18.0f, 32.0f));
    EXPECT_EQ(c / b, LinearColor(2.0f, 2.0f, 2.0f, 2.0f));
    EXPECT_EQ(c * 0.5f, b);
    EXPECT_EQ(0.5f * c, b);
    EXPECT_EQ(c / 2.0f, b);

    LinearColor mut = b;
    mut += b;
    EXPECT_EQ(mut, c);
    mut -= b;
    EXPECT_EQ(mut, b);
    mut *= 2.0f;
    EXPECT_EQ(mut, c);
    mut /= 2.0f;
    EXPECT_EQ(mut, b);
    mut *= b;
    EXPECT_EQ(mut, LinearColor(1.0f, 4.0f, 9.0f, 16.0f));
    mut /= b;
    EXPECT_EQ(mut, b);
}

TEST(LinearColorTest, ClampingQueriesAndDistance)
{
    const LinearColor hdr(-0.5f, 0.25f, 1.5f, 2.0f);
    EXPECT_EQ(hdr.getClamped(), LinearColor(0.0f, 0.25f, 1.0f, 1.0f));
    EXPECT_EQ(hdr.getClamped(-1.0f, 1.25f), LinearColor(-0.5f, 0.25f, 1.25f, 1.25f));
    EXPECT_FLOAT_EQ(hdr.getMin(), -0.5f);
    EXPECT_FLOAT_EQ(hdr.getMax(), 2.0f);

    const LinearColor withOpacity = hdr.copyWithNewOpacity(0.125f);
    EXPECT_FLOAT_EQ(withOpacity.a, 0.125f);
    EXPECT_FLOAT_EQ(withOpacity.r, hdr.r);

    EXPECT_TRUE(LinearColor(0.001f, 0.001f, 0.001f, 1.0f).isAlmostBlack());
    EXPECT_FALSE(LinearColor(0.01f, 0.0f, 0.0f, 1.0f).isAlmostBlack());

    const LinearColor p1(1.0f, 2.0f, 3.0f, 4.0f);
    const LinearColor p2(2.0f, 4.0f, 5.0f, 8.0f);
    // dr=1, dg=2, db=2, da=4 -> sqrt(1 + 4 + 4 + 16) = sqrt(25) = 5
    EXPECT_FLOAT_EQ(LinearColor::dist(p1, p2), 5.0f);

    const LinearColor mid = gp::math::lerp(p1, p2, 0.5f);
    EXPECT_TRUE(mid.equals(LinearColor(1.5f, 3.0f, 4.0f, 6.0f)));
}

TEST(LinearColorTest, ExhaustiveSRGBAndPow22RoundTripAndEdgeCases)
{
    // Every single 8-bit Color value [0, 255] MUST round-trip identically through sRGB -> LinearColor -> toColorSRGB!
    for (gp::Int32 i = 0; i <= 255; ++i)
    {
        const gp::UInt8 u = static_cast<gp::UInt8>(i);
        const Color orig(u, static_cast<gp::UInt8>(255 - i), static_cast<gp::UInt8>((i * 7) & 0xFF), u);
        const LinearColor lin = LinearColor::fromSRGBColor(orig);
        const Color roundTripped = lin.toColorSRGB();
        EXPECT_EQ(roundTripped, orig) << "Failed sRGB round-trip at index " << i;
        EXPECT_EQ(lin.toFColorSRGB(), orig);
        EXPECT_EQ(lin.toColor(true), orig);
        EXPECT_EQ(lin.toFColor(true), orig);
    }

    // Batch conversion test
    LinearColor srcBatch[4] = { LinearColor::kBlack, LinearColor::kRed, LinearColor::kGreen, LinearColor::kWhite };
    Color dstBatch[4]{};
    convertLinearColorsToColorSRGB(srcBatch, dstBatch, 4);
    EXPECT_EQ(dstBatch[0], Color::kBlack);
    EXPECT_EQ(dstBatch[1], Color::kRed);
    EXPECT_EQ(dstBatch[2], Color::kGreen);
    EXPECT_EQ(dstBatch[3], Color::kWhite);

    // Null / zero count safety
    convertLinearColorsToColorSRGB(nullptr, dstBatch, 4);
    convertLinearColorsToColorSRGB(srcBatch, nullptr, 4);
    convertLinearColorsToColorSRGB(srcBatch, dstBatch, 0);

    // NaNs, negatives, and > 1.0f values in toColorSRGB, quantizeRound, and quantizeFloor
    const gp::Float32 nanVal = std::numeric_limits<gp::Float32>::quiet_NaN();
    const LinearColor weird(nanVal, -10.0f, 5.0f, nanVal);
    EXPECT_EQ(weird.toColorSRGB(), Color(0u, 0u, 255u, 0u));
    EXPECT_EQ(weird.toColor(false), Color(0u, 0u, 255u, 0u));
    EXPECT_EQ(weird.quantizeFloor(), Color(0u, 0u, 255u, 0u));

    // Pow22 lookup table check
    const LinearColor pow22Zero = LinearColor::fromPow22Color(Color(0u, 0u, 0u, 0u));
    EXPECT_EQ(pow22Zero, LinearColor::kTransparent);
    const LinearColor pow22White = LinearColor::fromPow22Color(Color(255u, 255u, 255u, 255u));
    EXPECT_EQ(pow22White, LinearColor::kWhite);
    const LinearColor pow22Mid = LinearColor::fromPow22Color(Color(128u, 128u, 128u, 255u));
    EXPECT_NEAR(pow22Mid.r, 0.2195197f, 1.0e-5f);
}

TEST(LinearColorTest, RGBEConversionAndEdgeCases)
{
    // Very small / zero / negative values encode to (0, 0, 0, 0)
    const LinearColor black(0.0f, 0.0f, 0.0f, 1.0f);
    const Color blackRgbe = black.toRGBE();
    EXPECT_EQ(blackRgbe, Color(0u, 0u, 0u, 0u));
    EXPECT_EQ(blackRgbe.fromRGBE(), LinearColor::kBlack);

    const LinearColor negative(-5.0f, -1.0f, -2.0f, 1.0f);
    EXPECT_EQ(negative.toRGBE(), Color(0u, 0u, 0u, 0u));

    // NaN and Inf guard in toRGBE
    const gp::Float32 nanVal = std::numeric_limits<gp::Float32>::quiet_NaN();
    const gp::Float32 infVal = std::numeric_limits<gp::Float32>::infinity();
    EXPECT_EQ(LinearColor(nanVal, 1.0f, 1.0f, 1.0f).toRGBE(), Color(0u, 0u, 0u, 0u));
    EXPECT_EQ(LinearColor(infVal, 1.0f, 1.0f, 1.0f).toRGBE(), Color(0u, 0u, 0u, 0u));
    EXPECT_EQ(LinearColor(1.0e-35f, 1.0e-35f, 1.0e-35f, 1.0f).toRGBE(), Color(0u, 0u, 0u, 0u));

    // HDR color round-trip through RGBE (8-bit mantissa precision ~ 1-2%)
    const LinearColor hdrSun(12.5f, 6.25f, 1.5f, 1.0f);
    const Color encoded = hdrSun.toRGBE();
    EXPECT_GT(encoded.a, 128u);
    const LinearColor decoded = encoded.fromRGBE();
    EXPECT_NEAR(decoded.r, hdrSun.r, 0.15f);
    EXPECT_NEAR(decoded.g, hdrSun.g, 0.15f);
    EXPECT_NEAR(decoded.b, hdrSun.b, 0.15f);
    EXPECT_FLOAT_EQ(decoded.a, 1.0f);

    // Negative channel alongside positive HDR channel clamps negative channel to 0
    const LinearColor mixed(-2.0f, 4.0f, 2.0f, 1.0f);
    const LinearColor mixedDecoded = mixed.toRGBE().fromRGBE();
    EXPECT_LT(mixedDecoded.r, 0.05f);
    EXPECT_NEAR(mixedDecoded.g, 4.0f, 0.05f);
    EXPECT_NEAR(mixedDecoded.b, 2.0f, 0.05f);
}

TEST(LinearColorTest, HSVConversionsAndLerpUsingHSVShortestPath)
{
    // Pure primary and secondary colors
    EXPECT_TRUE(LinearColor::kRed.linearRGBToHSV().equals(LinearColor(0.0f, 1.0f, 1.0f, 1.0f)));
    EXPECT_TRUE(LinearColor::kYellow.linearRGBToHSV().equals(LinearColor(60.0f, 1.0f, 1.0f, 1.0f)));
    EXPECT_TRUE(LinearColor::kGreen.linearRGBToHSV().equals(LinearColor(120.0f, 1.0f, 1.0f, 1.0f)));
    EXPECT_TRUE(LinearColor(0.0f, 1.0f, 1.0f, 1.0f).linearRGBToHSV().equals(LinearColor(180.0f, 1.0f, 1.0f, 1.0f)));
    EXPECT_TRUE(LinearColor::kBlue.linearRGBToHSV().equals(LinearColor(240.0f, 1.0f, 1.0f, 1.0f)));
    EXPECT_TRUE(LinearColor(1.0f, 0.0f, 1.0f, 1.0f).linearRGBToHSV().equals(LinearColor(300.0f, 1.0f, 1.0f, 1.0f)));
    EXPECT_TRUE(LinearColor::kBlack.linearRGBToHSV().equals(LinearColor(0.0f, 0.0f, 0.0f, 1.0f)));
    EXPECT_TRUE(LinearColor::kWhite.linearRGBToHSV().equals(LinearColor(0.0f, 0.0f, 1.0f, 1.0f)));

    // Round-trip across all 6 hue sextants
    for (gp::Int32 h = 0; h < 360; h += 15)
    {
        const LinearColor hsv(static_cast<gp::Float32>(h), 0.75f, 0.85f, 0.5f);
        const LinearColor rgb = hsv.hsvToLinearRGB();
        const LinearColor backHsv = rgb.linearRGBToHSV();
        EXPECT_TRUE(backHsv.equals(hsv, 1.0e-3f));
    }

    // Negative hue wrapping in hsvToLinearRGB
    const LinearColor negHue(-60.0f, 1.0f, 1.0f, 1.0f);
    EXPECT_TRUE(negHue.hsvToLinearRGB().equals(LinearColor(1.0f, 0.0f, 1.0f, 1.0f), 1.0e-4f));

    // makeFromHSV8
    const LinearColor hsv8Red = LinearColor::makeFromHSV8(0u, 255u, 255u);
    EXPECT_TRUE(hsv8Red.equals(LinearColor::kRed, 1.0e-4f));

    // lerpUsingHSV shortest angular path across 0/360 seam (350 deg -> 10 deg midpoint should be 0 deg = Red)
    const LinearColor c350 = LinearColor(350.0f, 1.0f, 1.0f, 0.2f).hsvToLinearRGB();
    const LinearColor c010 = LinearColor(10.0f, 1.0f, 1.0f, 0.8f).hsvToLinearRGB();
    const LinearColor mid1 = LinearColor::lerpUsingHSV(c350, c010, 0.5f);
    EXPECT_TRUE(mid1.equals(LinearColor(1.0f, 0.0f, 0.0f, 0.5f), 1.0e-3f));

    const LinearColor mid2 = LinearColor::lerpUsingHSV(c010, c350, 0.5f);
    EXPECT_TRUE(mid2.equals(LinearColor(1.0f, 0.0f, 0.0f, 0.5f), 1.0e-3f));
}

TEST(LinearColorTest, LuminanceAndDesaturateAndCVarOverride)
{
    // Default working color space is sRGB (Rec.709 luminance weights: 0.212639, 0.7151687, 0.0721923)
    ColorSpace::setWorking(ColorSpace(ColorSpaceType::sRGB));
    const LinearColor sample(1.0f, 1.0f, 1.0f, 1.0f);
    EXPECT_NEAR(sample.getLuminance(), 1.0f, 1.0e-5f);
    EXPECT_NEAR(LinearColor::kGreen.getLuminance(), 0.7151687f, 1.0e-4f);

    // Custom luminance weights overload
    EXPECT_FLOAT_EQ(sample.getLuminance(LinearColor(0.3f, 0.59f, 0.11f)), 1.0f);

    // Desaturate: 0 -> original, 1 -> (lum, lum, lum, 0)
    const LinearColor redDesat0 = LinearColor::kRed.desaturate(0.0f);
    EXPECT_TRUE(redDesat0.equals(LinearColor::kRed));
    const LinearColor redDesat1 = LinearColor::kRed.desaturate(1.0f);
    const gp::Float32 redLum = LinearColor::kRed.getLuminance();
    EXPECT_TRUE(redDesat1.equals(LinearColor(redLum, redLum, redLum, 0.0f), 1.0e-5f));

    // Legacy CVar override test (`r.LegacyLuminanceFactors`)
    (void)gp::console::IConsoleManager::get().initialize();
    if (gp::console::IConsoleManager::isGlobalInitialized())
    {
        gp::console::AutoConsoleVariable<gp::Int32> legacyCvar(
            "r.LegacyLuminanceFactors", 1, "Use legacy (0.3, 0.59, 0.11) luminance weights."
        );
        EXPECT_NEAR(LinearColor::kGreen.getLuminance(), 0.59f, 1.0e-5f);
        (void)legacyCvar.set(0);
        EXPECT_NEAR(LinearColor::kGreen.getLuminance(), 0.7151687f, 1.0e-4f);
    }
}

TEST(LinearColorTest, EvaluateBezierAndComputeFixedColorAndIntensity)
{
    const LinearColor controlPoints[4] = { LinearColor(0.0f, 0.0f, 0.0f, 0.0f),
                                           LinearColor(1.0f, 0.0f, 0.0f, 0.0f),
                                           LinearColor(2.0f, 0.0f, 0.0f, 0.0f),
                                           LinearColor(3.0f, 0.0f, 0.0f, 0.0f) };

    gp::Vector<LinearColor> samples{};
    // Degenerate / invalid calls
    EXPECT_FLOAT_EQ(LinearColor::evaluateBezier(nullptr, 4, samples), 0.0f);
    EXPECT_FLOAT_EQ(LinearColor::evaluateBezier(controlPoints, 1, samples), 0.0f);
    EXPECT_TRUE(samples.isEmpty());

    // Collinear cubic Bezier from (0,0,0,0) to (3,0,0,0) has exact path length 3.0f
    const gp::Float32 length = LinearColor::evaluateBezier(controlPoints, 4, samples);
    ASSERT_EQ(samples.size(), 4u);
    EXPECT_NEAR(length, 3.0f, 1.0e-4f);
    EXPECT_TRUE(samples[0].equals(controlPoints[0]));
    EXPECT_TRUE(samples[3].equals(controlPoints[3]));

    // computeAndFixedColorAndIntensity
    Color fixedColor{};
    gp::Float32 intensity = 0.0f;
    computeAndFixedColorAndIntensity(LinearColor(4.0f, 2.0f, 0.0f, 4.0f), fixedColor, intensity);
    EXPECT_FLOAT_EQ(intensity, 4.0f);
    EXPECT_EQ(fixedColor.r, 255u);
    EXPECT_EQ(fixedColor.b, 0u);
    EXPECT_EQ(fixedColor.a, 255u);

    // Zero / negative color floors intensity at Constants<Float32>::delta
    computeAndFixedColorAndIntensity(LinearColor(0.0f, -1.0f, 0.0f, 1.0f), fixedColor, intensity);
    EXPECT_FLOAT_EQ(intensity, Constants<gp::Float32>::delta);
    EXPECT_EQ(fixedColor.r, 0u);
    EXPECT_EQ(fixedColor.g, 0u);
    EXPECT_EQ(fixedColor.b, 0u);
}

TEST(LinearColorTest, StringSerializationParsingAndDistinctColors)
{
    const LinearColor c(0.125f, 0.25f, 0.5f, 0.75f);
    const gp::String str = c.toString();
    EXPECT_EQ(str.view(), gp::StringView("(R=0.125000,G=0.250000,B=0.500000,A=0.750000)"));

    LinearColor parsed{};
    EXPECT_TRUE(parsed.initFromString(str.view()));
    EXPECT_TRUE(parsed.equals(c, 1.0e-5f));

    // Optional A= defaults to 1.0f
    LinearColor noAlpha{};
    EXPECT_TRUE(noAlpha.initFromString("R=0.5, G=0.25, B=0.75"));
    EXPECT_TRUE(noAlpha.equals(LinearColor(0.5f, 0.25f, 0.75f, 1.0f)));

    // Invalid string returns false
    LinearColor failed{};
    EXPECT_FALSE(failed.initFromString("(R=0.5,G=0.25)"));
    EXPECT_FALSE(failed.initFromString("invalid"));

    // intToDistinctColor & hashing
    const LinearColor d0 = LinearColor::intToDistinctColor(0);
    const LinearColor d1 = LinearColor::intToDistinctColor(1);
    const LinearColor d2 = LinearColor::intToDistinctColor(2);
    EXPECT_NE(d0, d1);
    EXPECT_NE(d1, d2);
    EXPECT_NE(gp::hashing::hashOf(d0), gp::hashing::hashOf(d1));
}

}   // namespace gp::math::tests
