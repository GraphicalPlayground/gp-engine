// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/color/ColorSpace.hpp"
#include "containers/views/StringView.hpp"
#include "maths/color/ColorEnums.hpp"
#include "maths/color/ColorTransfer.hpp"
#include "maths/color/LinearColor.hpp"
#include "maths/vector/Vector2.hpp"
#include "maths/vector/Vector3.hpp"
#include <gtest/gtest.h>

namespace gp::math::tests
{

static_assert(toString(ColorSpaceType::sRGB) == gp::StringView("sRGB"), "ColorSpaceType::sRGB toString failed.");
static_assert(
    toString(ColorSpaceType::ACESAP1) == gp::StringView("ACESAP1"), "ColorSpaceType::ACESAP1 toString failed."
);
static_assert(
    toString(WhitePoint::CIE1931_D65) == gp::StringView("CIE1931_D65"), "WhitePoint::CIE1931_D65 toString failed."
);
static_assert(
    toString(ChromaticAdaptationMethod::Bradford) == gp::StringView("Bradford"),
    "ChromaticAdaptationMethod::Bradford toString failed."
);
static_assert(toString(ColorEncoding::ST2084) == gp::StringView("ST2084"), "ColorEncoding::ST2084 toString failed.");

TEST(ColorSpaceTest, CIE1931xyYAndXYZConversions)
{
    const Vector2<gp::Float64> d65 = getWhitePoint(WhitePoint::CIE1931_D65);
    EXPECT_DOUBLE_EQ(d65.x, 0.3127);
    EXPECT_DOUBLE_EQ(d65.y, 0.3290);

    const Vector3<gp::Float64> d65Xyz = xyYToXYZ(d65, 1.0);
    EXPECT_NEAR(d65Xyz.x, 0.9504559, 1.0e-6);
    EXPECT_DOUBLE_EQ(d65Xyz.y, 1.0);
    EXPECT_NEAR(d65Xyz.z, 1.0890577, 1.0e-6);

    const Vector3<gp::Float64> roundTrip = xyzToxyY(d65Xyz);
    EXPECT_NEAR(roundTrip.x, d65.x, 1.0e-12);
    EXPECT_NEAR(roundTrip.y, d65.y, 1.0e-12);
    EXPECT_NEAR(roundTrip.z, 1.0, 1.0e-12);

    // Zero division guards
    EXPECT_EQ(xyYToXYZ(Vector2<gp::Float64>(0.3, 0.0), 1.0), Vector3<gp::Float64>::zero());
    EXPECT_EQ(xyzToxyY(Vector3<gp::Float64>::zero()), Vector3<gp::Float64>::zero());

    // All standard white points have positive x, y
    for (gp::USize i = 0u; i < kWhitePointCount; ++i)
    {
        const WhitePoint wp = static_cast<WhitePoint>(i);
        const Vector2<gp::Float64> xy = getWhitePoint(wp);
        EXPECT_GT(xy.x, 0.25);
        EXPECT_GT(xy.y, 0.25);
        EXPECT_FALSE(toString(wp).isEmpty());
    }
}

TEST(ColorSpaceTest, AllStandardColorSpacesInvertibilityAndWhitePointInvariant)
{
    for (gp::USize i = 1u; i < kColorSpaceTypeCount; ++i)
    {
        const ColorSpaceType type = static_cast<ColorSpaceType>(i);
        const ColorSpace cs(type);

        EXPECT_EQ(cs.getType(), type);
        EXPECT_EQ(cs.isSRGB(), type == ColorSpaceType::sRGB);

        // In any RGB color space, RGB=(1, 1, 1) must map to the reference white point XYZ (with Y = 1.0)!
        const Vector3<gp::Float64> expectedWhiteXyz = xyYToXYZ(cs.getChromaticities().white, 1.0);
        const Matrix4x4<gp::Float64>& rgbToXyz = cs.getRgbToXyz();
        const Vector3<gp::Float64> actualWhiteXyz(
            rgbToXyz.m00 + rgbToXyz.m01 + rgbToXyz.m02,
            rgbToXyz.m10 + rgbToXyz.m11 + rgbToXyz.m12,
            rgbToXyz.m20 + rgbToXyz.m21 + rgbToXyz.m22
        );
        EXPECT_TRUE(actualWhiteXyz.equals(expectedWhiteXyz, 1.0e-6))
            << "White point XYZ mismatch for ColorSpaceType index " << i;

        // Luminance factors sum to 1.0
        const LinearColor lum = cs.getLuminanceFactors();
        EXPECT_NEAR(lum.r + lum.g + lum.b, 1.0f, 1.0e-5f);

        // XyzToRgb * RgbToXyz must reconstruct any test RGB vector
        const Matrix4x4<gp::Float64>& xyzToRgb = cs.getXyzToRgb();
        const Vector3<gp::Float64> testRgb(0.25, 0.55, 0.85);
        const Vector3<gp::Float64> xyz(
            rgbToXyz.m00 * testRgb.x + rgbToXyz.m01 * testRgb.y + rgbToXyz.m02 * testRgb.z,
            rgbToXyz.m10 * testRgb.x + rgbToXyz.m11 * testRgb.y + rgbToXyz.m12 * testRgb.z,
            rgbToXyz.m20 * testRgb.x + rgbToXyz.m21 * testRgb.y + rgbToXyz.m22 * testRgb.z
        );
        const Vector3<gp::Float64> reconstructed(
            xyzToRgb.m00 * xyz.x + xyzToRgb.m01 * xyz.y + xyzToRgb.m02 * xyz.z,
            xyzToRgb.m10 * xyz.x + xyzToRgb.m11 * xyz.y + xyzToRgb.m12 * xyz.z,
            xyzToRgb.m20 * xyz.x + xyzToRgb.m21 * xyz.y + xyzToRgb.m22 * xyz.z
        );
        EXPECT_TRUE(reconstructed.equals(testRgb, 1.0e-6));
    }
}

TEST(ColorSpaceTest, CustomChromaticitiesAndWorkingColorSpace)
{
    const Chromaticities srgbChroma(ColorSpaceType::sRGB);
    const ColorSpace customSrgb(srgbChroma.red, srgbChroma.green, srgbChroma.blue, srgbChroma.white);
    EXPECT_TRUE(customSrgb.isSRGB());
    EXPECT_EQ(customSrgb.getType(), ColorSpaceType::sRGB);
    EXPECT_TRUE(customSrgb.equals(ColorSpace(ColorSpaceType::sRGB)));
    EXPECT_TRUE(customSrgb.equals(srgbChroma));

    Vector2<gp::Float64> r{};
    Vector2<gp::Float64> g{};
    Vector2<gp::Float64> b{};
    Vector2<gp::Float64> w{};
    customSrgb.getChromaticities(r, g, b, w);
    EXPECT_EQ(r, srgbChroma.red);
    EXPECT_EQ(g, srgbChroma.green);
    EXPECT_EQ(b, srgbChroma.blue);
    EXPECT_EQ(w, srgbChroma.white);

    // Working color space getter/setter
    const ColorSpace acescg(ColorSpaceType::ACESAP1);
    ColorSpace::setWorking(acescg);
    EXPECT_EQ(ColorSpace::getWorking(), acescg);
    EXPECT_FALSE(ColorSpace::getWorking().isSRGB());

    // Restore default sRGB working space
    ColorSpace::setWorking(ColorSpace(ColorSpaceType::sRGB));
    EXPECT_TRUE(ColorSpace::getWorking().isSRGB());
}

TEST(ColorSpaceTest, ColorSpaceTransformRoundTripAndChromaticAdaptation)
{
    const ColorSpace srgb(ColorSpaceType::sRGB);
    const ColorSpace rec2020(ColorSpaceType::Rec2020);
    const ColorSpace acescg(ColorSpaceType::ACESAP1);

    // Identity transform when source == destination
    const ColorSpaceTransform identityTransform(srgb, srgb);
    const LinearColor sample(0.4f, 0.6f, 0.8f, 0.75f);
    EXPECT_TRUE(identityTransform.apply(sample).equals(sample, 1.0e-6f));

    // Same white point (D65): sRGB <-> Rec2020 round-trip
    const ColorSpaceTransform srgbTo2020(srgb, rec2020);
    const ColorSpaceTransform rec2020ToSrgb(rec2020, srgb);
    const LinearColor in2020 = srgbTo2020.apply(sample);
    EXPECT_NE(in2020, sample);
    EXPECT_FLOAT_EQ(in2020.a, sample.a);
    const LinearColor backToSrgb = rec2020ToSrgb.apply(in2020);
    EXPECT_TRUE(backToSrgb.equals(sample, 1.0e-5f));

    // Cross white-point (D65 <-> ACES D60) with Bradford, CAT02, and None chromatic adaptation
    const ChromaticAdaptationMethod methods[3] = { ChromaticAdaptationMethod::None,
                                                   ChromaticAdaptationMethod::Bradford,
                                                   ChromaticAdaptationMethod::CAT02 };

    for (const ChromaticAdaptationMethod method: methods)
    {
        const ColorSpaceTransform forward(srgb, acescg, method);
        const ColorSpaceTransform backward(acescg, srgb, method);

        // Reference white (1, 1, 1) in source must map to reference white (1, 1, 1) in destination!
        const LinearColor adaptedWhite = forward.apply(LinearColor::kWhite);
        EXPECT_TRUE(adaptedWhite.equals(LinearColor::kWhite, 1.0e-4f));

        // Round-trip arbitrary color and 3D vector
        const LinearColor roundTripped = backward.apply(forward.apply(sample));
        EXPECT_TRUE(roundTripped.equals(sample, 1.0e-5f));

        const Vector3<gp::Float64> vecSample(0.3, 0.5, 0.7);
        const Vector3<gp::Float64> vecRoundTripped = backward.apply(forward.apply(vecSample));
        EXPECT_TRUE(vecRoundTripped.equals(vecSample, 1.0e-6));
    }
}

TEST(ColorSpaceTest, MakeChromaticitiesFromTemperatureAndTint)
{
    // 6500K on Planckian locus is close to D65 (~0.313, ~0.324)
    const Vector2<gp::Float64> wp6500 = ColorSpace::makeChromaticitiesFromTemperature(6500.0f, 0.0f);
    EXPECT_NEAR(wp6500.x, 0.3135, 0.01);
    EXPECT_NEAR(wp6500.y, 0.3237, 0.01);

    // Positive and negative tint shift perpendicular to the locus
    const Vector2<gp::Float64> wpTintPos = ColorSpace::makeChromaticitiesFromTemperature(6500.0f, 0.5f);
    const Vector2<gp::Float64> wpTintNeg = ColorSpace::makeChromaticitiesFromTemperature(6500.0f, -0.5f);
    EXPECT_NE(wp6500, wpTintPos);
    EXPECT_NE(wp6500, wpTintNeg);

    // Out-of-bounds temperatures clamp cleanly to [1000K, 15000K]
    EXPECT_EQ(
        ColorSpace::makeChromaticitiesFromTemperature(100.0f, 0.0f),
        ColorSpace::makeChromaticitiesFromTemperature(1000.0f, 0.0f)
    );
    EXPECT_EQ(
        ColorSpace::makeChromaticitiesFromTemperature(50000.0f, 0.0f),
        ColorSpace::makeChromaticitiesFromTemperature(15000.0f, 0.0f)
    );
}

TEST(ColorSpaceTest, DegenerateChromaticitiesAndEnumFallbacks)
{
    // ColorSpaceType::None and out-of-range ColorSpaceType produce zero chromaticities and identity fallback matrices
    const ColorSpace noneSpace(ColorSpaceType::None);
    EXPECT_EQ(noneSpace.getType(), ColorSpaceType::None);
    EXPECT_FALSE(noneSpace.isSRGB());
    EXPECT_DOUBLE_EQ(noneSpace.getRgbToXyz().m00, 1.0);
    EXPECT_DOUBLE_EQ(noneSpace.getXyzToRgb().m00, 1.0);

    // Collinear primaries (singular 3x3 matrix) gracefully fall back without NaN/crash
    const Chromaticities collinear(
        Vector2<gp::Float64>(0.5, 0.5),
        Vector2<gp::Float64>(0.5, 0.5),
        Vector2<gp::Float64>(0.5, 0.5),
        Vector2<gp::Float64>(0.3127, 0.3290)
    );
    const ColorSpace singularSpace(collinear);
    EXPECT_FALSE(singularSpace.isSRGB());
    EXPECT_NE(singularSpace, ColorSpace(ColorSpaceType::sRGB));

    // Direct 4x4 matrix constructor for ColorSpaceTransform
    Matrix4x4<gp::Float64> scale2x{};
    scale2x.m00 = 2.0;
    scale2x.m11 = 3.0;
    scale2x.m22 = 4.0;
    scale2x.m33 = 1.0;
    const ColorSpaceTransform customTransform(scale2x);
    EXPECT_EQ(customTransform.getMatrix().m00, 2.0);
    EXPECT_TRUE(customTransform.apply(LinearColor(1.0f, 1.0f, 1.0f, 0.5f)).equals(LinearColor(2.0f, 3.0f, 4.0f, 0.5f)));

    // Enum toString() coverage for all enumerators and out-of-range sentinels
    for (gp::USize i = 0u; i < kColorSpaceTypeCount; ++i)
    {
        EXPECT_NE(toString(static_cast<ColorSpaceType>(i)), gp::StringView("Unknown"));
    }
    EXPECT_EQ(toString(ColorSpaceType::Count), gp::StringView("Unknown"));
    EXPECT_EQ(toString(static_cast<ColorSpaceType>(255u)), gp::StringView("Unknown"));
    EXPECT_EQ(toString(WhitePoint::Count), gp::StringView("Unknown"));
    EXPECT_EQ(toString(ChromaticAdaptationMethod::Count), gp::StringView("Unknown"));
    EXPECT_EQ(toString(ColorEncoding::Count), gp::StringView("Unknown"));
}

TEST(ColorTransferTest, AllTransferFunctionsRoundTripLinearToEncodedAndBack)
{
    const gp::Float32 testValues[5] = { 0.001f, 0.018f, 0.18f, 0.5f, 0.95f };

    for (gp::USize encIdx = 0u; encIdx < kColorEncodingCount; ++encIdx)
    {
        const ColorEncoding encoding = static_cast<ColorEncoding>(encIdx);
        for (const gp::Float32 val: testValues)
        {
            const gp::Float32 encoded = ColorTransfer::encode(encoding, val);
            const gp::Float32 decoded = ColorTransfer::decode(encoding, encoded);
            EXPECT_NEAR(decoded, val, 1.0e-4f)
                << "Failed scalar round-trip for ColorEncoding " << toString(encoding).data() << " at value " << val;
        }

        const LinearColor linColor(0.05f, 0.18f, 0.75f, 0.42f);
        const LinearColor encColor = ColorTransfer::encode(encoding, linColor);
        EXPECT_FLOAT_EQ(encColor.a, linColor.a);
        const LinearColor decColor = ColorTransfer::decode(encoding, encColor);
        EXPECT_TRUE(decColor.equals(linColor, 1.0e-4f));
    }

    // Verify negative / linear toe branch coverage for log curves that support negative numbers
    EXPECT_NEAR(ColorTransfer::decodeREDLog3G10(ColorTransfer::encodeREDLog3G10(-0.005f)), -0.005f, 1.0e-5f);
    EXPECT_NEAR(ColorTransfer::decodeREDLog3G10(ColorTransfer::encodeREDLog3G10(-0.05f)), -0.05f, 1.0e-5f);
    EXPECT_NEAR(ColorTransfer::decodeSLog2(ColorTransfer::encodeSLog2(-0.02f)), -0.02f, 1.0e-5f);
    EXPECT_NEAR(ColorTransfer::decodeCanonLog(ColorTransfer::encodeCanonLog(-0.05f)), -0.05f, 1.0e-5f);

    // Boundary and fallback coverage (0.0f, negative Gamma22, extreme ST2084, invalid ColorEncoding::Count)
    EXPECT_FLOAT_EQ(ColorTransfer::encodeGamma22(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(ColorTransfer::decodeGamma22(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(ColorTransfer::decodeST2084(10.0f), 0.0f);
    EXPECT_FLOAT_EQ(ColorTransfer::encode(ColorEncoding::Count, 0.42f), 0.42f);
    EXPECT_FLOAT_EQ(ColorTransfer::decode(ColorEncoding::Count, 0.42f), 0.42f);
}

}   // namespace gp::math::tests
