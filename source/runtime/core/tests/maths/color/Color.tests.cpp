// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/color/Color.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "containers/views/StringView.hpp"
#include "hashing/HashOf.hpp"
#include "maths/color/ColorEnums.hpp"
#include "maths/color/LinearColor.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace gp::math::tests
{

namespace
{

constexpr bool testConstexprColorOperations() noexcept
{
    const Color defaultColor{};
    if (defaultColor.r != 0u || defaultColor.g != 0u || defaultColor.b != 0u || defaultColor.a != 0u)
    {
        return false;
    }

    const Color rgba(10u, 20u, 30u, 40u);
    if (rgba.r != 10u || rgba.g != 20u || rgba.b != 30u || rgba.a != 40u)
    {
        return false;
    }

    const gp::UInt32 packed = rgba.dwColor();
    const Color unpacked(packed);
    if (unpacked != rgba)
    {
        return false;
    }

    const Color withNewA = rgba.withAlpha(128u);
    if (withNewA.r != 10u || withNewA.g != 20u || withNewA.b != 30u || withNewA.a != 128u)
    {
        return false;
    }

    Color accum(200u, 100u, 50u, 250u);
    accum += Color(100u, 200u, 25u, 50u);
    if (accum.r != 255u || accum.g != 255u || accum.b != 75u || accum.a != 255u)
    {
        return false;
    }

    if (Color::quantizeUNormFloatTo8(0.0f) != 0u || Color::quantizeUNormFloatTo8(1.0f) != 255u)
    {
        return false;
    }

    if (Color::quantizeUNormFloatTo16(0.0f) != 0u || Color::quantizeUNormFloatTo16(1.0f) != 65535u)
    {
        return false;
    }

    if (Color::requantize10to8(0) != 0u || Color::requantize10to8(1023) != 255u)
    {
        return false;
    }

    if (Color::requantize16to8(0) != 0u || Color::requantize16to8(65535) != 255u)
    {
        return false;
    }

    return true;
}

static_assert(testConstexprColorOperations(), "Constexpr Color operations evaluation failed.");
static_assert(toString(GammaSpace::Linear) == gp::StringView("Linear"), "GammaSpace::Linear toString failed.");
static_assert(toString(GammaSpace::Pow22) == gp::StringView("Pow22"), "GammaSpace::Pow22 toString failed.");
static_assert(toString(GammaSpace::sRGB) == gp::StringView("sRGB"), "GammaSpace::sRGB toString failed.");
static_assert(toString(GammaSpace::Invalid) == gp::StringView("Invalid"), "GammaSpace::Invalid toString failed.");

}   // namespace

TEST(ColorTest, ConstructorsAndMemoryLayout)
{
    const Color zero{};
    EXPECT_EQ(zero.r, 0u);
    EXPECT_EQ(zero.g, 0u);
    EXPECT_EQ(zero.b, 0u);
    EXPECT_EQ(zero.a, 0u);
    EXPECT_EQ(zero.dwColor(), 0u);

    const Color opaqueRgb(12u, 34u, 56u);
    EXPECT_EQ(opaqueRgb.r, 12u);
    EXPECT_EQ(opaqueRgb.g, 34u);
    EXPECT_EQ(opaqueRgb.b, 56u);
    EXPECT_EQ(opaqueRgb.a, 255u);

    // Verify BGRA byte order on little-endian targets: byte[0]=B, byte[1]=G, byte[2]=R, byte[3]=A
    const Color bgraCheck(0x11u, 0x22u, 0x33u, 0x44u);
    const gp::UInt8* const rawBytes = reinterpret_cast<const gp::UInt8*>(&bgraCheck);
    EXPECT_EQ(rawBytes[0], 0x33u);
    EXPECT_EQ(rawBytes[1], 0x22u);
    EXPECT_EQ(rawBytes[2], 0x11u);
    EXPECT_EQ(rawBytes[3], 0x44u);
    EXPECT_EQ(bgraCheck.dwColor(), 0x44112233u);

    Color mutated{};
    mutated.setDwColor(0x44112233u);
    EXPECT_EQ(mutated, bgraCheck);
}

TEST(ColorTest, PackedFormatsAndWithAlpha)
{
    const Color c(0x12u, 0x34u, 0x56u, 0x78u);

    EXPECT_EQ(c.toPackedARGB(), 0x78123456u);
    EXPECT_EQ(c.toPackedABGR(), 0x78563412u);
    EXPECT_EQ(c.toPackedRGBA(), 0x12345678u);
    EXPECT_EQ(c.toPackedBGRA(), 0x56341278u);

    const Color alphaMod = c.withAlpha(0x9Au);
    EXPECT_EQ(alphaMod.r, 0x12u);
    EXPECT_EQ(alphaMod.g, 0x34u);
    EXPECT_EQ(alphaMod.b, 0x56u);
    EXPECT_EQ(alphaMod.a, 0x9Au);
}

TEST(ColorTest, SaturatingAdditionAndEquality)
{
    Color a(100u, 200u, 255u, 0u);
    const Color b(50u, 100u, 10u, 255u);

    EXPECT_NE(a, b);
    a += b;
    EXPECT_EQ(a.r, 150u);
    EXPECT_EQ(a.g, 255u);
    EXPECT_EQ(a.b, 255u);
    EXPECT_EQ(a.a, 255u);
    EXPECT_EQ(a, Color(150u, 255u, 255u, 255u));
}

TEST(ColorTest, UNormQuantizationAndDequantizationExactness)
{
    const gp::Float32 nanVal = std::numeric_limits<gp::Float32>::quiet_NaN();
    const gp::Float32 posInf = std::numeric_limits<gp::Float32>::infinity();
    const gp::Float32 negInf = -std::numeric_limits<gp::Float32>::infinity();

    EXPECT_FLOAT_EQ(Color::clamp01NansTo0(nanVal), 0.0f);
    EXPECT_FLOAT_EQ(Color::clamp01NansTo0(-5.0f), 0.0f);
    EXPECT_FLOAT_EQ(Color::clamp01NansTo0(negInf), 0.0f);
    EXPECT_FLOAT_EQ(Color::clamp01NansTo0(0.25f), 0.25f);
    EXPECT_FLOAT_EQ(Color::clamp01NansTo0(2.5f), 1.0f);
    EXPECT_FLOAT_EQ(Color::clamp01NansTo0(posInf), 1.0f);

    EXPECT_EQ(Color::quantizeUNormFloatTo8(nanVal), 0u);
    EXPECT_EQ(Color::quantizeUNormFloatTo8(-1.0f), 0u);
    EXPECT_EQ(Color::quantizeUNormFloatTo8(2.0f), 255u);
    EXPECT_EQ(Color::quantizeUNormFloatTo16(nanVal), 0u);
    EXPECT_EQ(Color::quantizeUNormFloatTo16(-1.0f), 0u);
    EXPECT_EQ(Color::quantizeUNormFloatTo16(2.0f), 65535u);

    // Exhaustive round-trip verification for all 256 8-bit values
    for (gp::Int32 i = 0; i <= 255; ++i)
    {
        const gp::Float32 f = Color::dequantizeUNorm8ToFloat(i);
        EXPECT_EQ(Color::quantizeUNormFloatTo8(f), static_cast<gp::UInt8>(i));
    }

    // Spot-check and boundary verification for 16-bit values
    EXPECT_FLOAT_EQ(Color::dequantizeUNorm16ToFloat(0), 0.0f);
    EXPECT_FLOAT_EQ(Color::dequantizeUNorm16ToFloat(65535), 1.0f);
    for (gp::Int32 i = 0; i <= 65535; i += 257)
    {
        const gp::Float32 f = Color::dequantizeUNorm16ToFloat(i);
        EXPECT_EQ(Color::quantizeUNormFloatTo16(f), static_cast<gp::UInt16>(i));
    }
}

TEST(ColorTest, Requantize10to8And16to8ExhaustiveExactMatchWithFloatFormula)
{
    // Exhaustively test all 1024 10-bit values against the reference floating-point formula
    for (gp::Int32 v10 = 0; v10 <= 1023; ++v10)
    {
        const gp::UInt8 expected = static_cast<gp::UInt8>((static_cast<gp::Float32>(v10) / 1023.0f) * 255.0f + 0.5f);
        EXPECT_EQ(Color::requantize10to8(v10), expected);
    }

    // Exhaustively test all 65536 16-bit values against the reference floating-point formula
    for (gp::Int32 v16 = 0; v16 <= 65535; ++v16)
    {
        const gp::UInt8 expected = static_cast<gp::UInt8>((static_cast<gp::Float32>(v16) / 65535.0f) * 255.0f + 0.5f);
        EXPECT_EQ(Color::requantize16to8(v16), expected);
    }

    const Color c0 = Color::makeRequantizeFrom1010102(0, 511, 1023, 0);
    EXPECT_EQ(c0.r, 0u);
    EXPECT_EQ(c0.g, 127u);
    EXPECT_EQ(c0.b, 255u);
    EXPECT_EQ(c0.a, 0x00u);

    const Color c1 = Color::makeRequantizeFrom1010102(1023, 0, 512, 1);
    EXPECT_EQ(c1.a, 0x55u);
    const Color c2 = Color::makeRequantizeFrom1010102(1023, 0, 512, 2);
    EXPECT_EQ(c2.a, 0xAAu);
    const Color c3 = Color::makeRequantizeFrom1010102(1023, 0, 512, 3);
    EXPECT_EQ(c3.a, 0xFFu);
}

TEST(ColorTest, ReinterpretAsLinearRoundTripsWithQuantizeRound)
{
    for (gp::Int32 i = 0; i <= 255; ++i)
    {
        const gp::UInt8 u = static_cast<gp::UInt8>(i);
        const Color orig(u, static_cast<gp::UInt8>(255u - u), static_cast<gp::UInt8>(u / 2u), u);
        const LinearColor lin = orig.reinterpretAsLinear();
        const Color back = lin.quantizeRound();
        EXPECT_EQ(back, orig);
    }
}

TEST(ColorTest, HexFormattingAndParsingAllFormatsAndEdgeCases)
{
    const Color sample(0x1Au, 0x2Bu, 0x3Cu, 0x4Du);
    const gp::String hexStr = sample.toHex();
    EXPECT_EQ(hexStr.view(), gp::StringView("1A2B3C4D"));
    EXPECT_EQ(Color::fromHex(hexStr.view()), sample);

    // #RRGGBBAA and lowercase rrggbbaa
    EXPECT_EQ(Color::fromHex("#1A2B3C4D"), sample);
    EXPECT_EQ(Color::fromHex("1a2b3c4d"), sample);
    EXPECT_EQ(Color::fromHex("#1a2b3c4d"), sample);

    // RRGGBB and #RRGGBB (alpha defaults to 255)
    EXPECT_EQ(Color::fromHex("1A2B3C"), Color(0x1Au, 0x2Bu, 0x3Cu, 255u));
    EXPECT_EQ(Color::fromHex("#ff8000"), Color(255u, 128u, 0u, 255u));

    // RGB and #RGB shorthand (nibble replication: F -> FF, 8 -> 88, 0 -> 00)
    EXPECT_EQ(Color::fromHex("F80"), Color(0xFFu, 0x88u, 0x00u, 255u));
    EXPECT_EQ(Color::fromHex("#abc"), Color(0xAAu, 0xBBu, 0xCCu, 255u));

    // Invalid hex strings return (0, 0, 0, 0)
    const Color zero(0u, 0u, 0u, 0u);
    EXPECT_EQ(Color::fromHex(""), zero);
    EXPECT_EQ(Color::fromHex("#"), zero);
    EXPECT_EQ(Color::fromHex("12"), zero);
    EXPECT_EQ(Color::fromHex("#1234"), zero);
    EXPECT_EQ(Color::fromHex("12345"), zero);
    EXPECT_EQ(Color::fromHex("1234567"), zero);
    EXPECT_EQ(Color::fromHex("123456789"), zero);
    EXPECT_EQ(Color::fromHex("GG0000"), zero);
    EXPECT_EQ(Color::fromHex("#12G"), zero);
    EXPECT_EQ(Color::fromHex("#112233ZZ"), zero);
}

TEST(ColorTest, StringSerializationAndParsingEdgeCases)
{
    const Color c(15u, 128u, 240u, 200u);
    const gp::String str = c.toString();
    EXPECT_EQ(str.view(), gp::StringView("(R=15,G=128,B=240,A=200)"));

    Color parsed{};
    EXPECT_TRUE(parsed.initFromString(str.view()));
    EXPECT_EQ(parsed, c);

    // Missing optional A= defaults to 255
    Color noAlpha{};
    EXPECT_TRUE(noAlpha.initFromString("(R=10, G=20, B=30)"));
    EXPECT_EQ(noAlpha, Color(10u, 20u, 30u, 255u));

    // Reordered keys, semicolon delimiters, and lowercase keys
    Color reordered{};
    EXPECT_TRUE(reordered.initFromString("b=90; a=100; r=30; g=60"));
    EXPECT_EQ(reordered, Color(30u, 60u, 90u, 100u));

    // Whitespace after '=' before value
    Color withSpaces{};
    EXPECT_TRUE(withSpaces.initFromString("(R=  45 , G=\t90 , B= 135 , A= 180 )"));
    EXPECT_EQ(withSpaces, Color(45u, 90u, 135u, 180u));

    // Invalid optional A= leaves default alpha = 255 intact
    Color badAlpha{};
    EXPECT_TRUE(badAlpha.initFromString("(R=10,G=20,B=30,A=notanumber)"));
    EXPECT_EQ(badAlpha, Color(10u, 20u, 30u, 255u));

    // Out-of-bounds integer channels clamp to [0, 255]
    Color clamped{};
    EXPECT_TRUE(clamped.initFromString("(R=-50,G=500,B=128,A=999)"));
    EXPECT_EQ(clamped, Color(0u, 255u, 128u, 255u));

    // Missing required R, G, or B or empty key value fails and returns false
    Color invalid{};
    EXPECT_FALSE(invalid.initFromString(""));
    EXPECT_FALSE(invalid.initFromString("(R=,G=20,B=30)"));
    EXPECT_FALSE(invalid.initFromString("(R=10,G=20,B=)"));
    EXPECT_FALSE(invalid.initFromString("(R=10,G=20)"));
    EXPECT_FALSE(invalid.initFromString("(R=10,B=30)"));
    EXPECT_FALSE(invalid.initFromString("(G=20,B=30)"));
    EXPECT_FALSE(invalid.initFromString("(R=abc,G=20,B=30)"));
    EXPECT_FALSE(invalid.initFromString("(R=99999999999999999999,G=20,B=30)"));
}

TEST(ColorTest, GammaSpaceEnumToStringOutOfRange)
{
    EXPECT_EQ(toString(static_cast<GammaSpace>(255u)), gp::StringView("Invalid"));
}

TEST(ColorTest, MakeRedToGreenColorFromScalarRamp)
{
    // Scalar <= 0.0 -> pure Red (255, 0, 0, 255)
    EXPECT_EQ(Color::makeRedToGreenColorFromScalar(-1.0f), Color(255u, 0u, 0u, 255u));
    EXPECT_EQ(Color::makeRedToGreenColorFromScalar(0.0f), Color(255u, 0u, 0u, 255u));

    // Scalar == 0.5 -> pure Yellow (255, 255, 0, 255)
    EXPECT_EQ(Color::makeRedToGreenColorFromScalar(0.5f), Color(255u, 255u, 0u, 255u));

    // Scalar >= 1.0 -> pure Green (0, 255, 0, 255)
    EXPECT_EQ(Color::makeRedToGreenColorFromScalar(1.0f), Color(0u, 255u, 0u, 255u));
    EXPECT_EQ(Color::makeRedToGreenColorFromScalar(2.0f), Color(0u, 255u, 0u, 255u));
}

TEST(ColorTest, RandomAndTemperatureFactoriesAndHashing)
{
    const Color seeded1 = Color::makeRandomSeededColor(42);
    const Color seeded2 = Color::makeRandomSeededColor(42);
    const Color seeded3 = Color::makeRandomSeededColor(9999);
    EXPECT_EQ(seeded1, seeded2);
    EXPECT_NE(seeded1, seeded3);
    EXPECT_EQ(seeded1.a, 255u);

    const Color rnd1 = Color::makeRandomColor();
    const Color rnd2 = Color::makeRandomColor();
    EXPECT_NE(rnd1, rnd2);
    EXPECT_EQ(rnd1.a, 255u);

    const Color warm = Color::makeFromColorTemperature(2000.0f);
    const Color cool = Color::makeFromColorTemperature(10000.0f);
    EXPECT_GT(warm.r, warm.b);
    EXPECT_GT(cool.b, cool.r);

    // Hashing integration
    EXPECT_EQ(Color::kOrange.getHash(), Color::kOrange.dwColor());
    EXPECT_NE(gp::hashing::hashOf(Color::kRed), gp::hashing::hashOf(Color::kBlue));
    EXPECT_EQ(gp::hashing::hashOf(Color::kEmerald), gp::hashing::hashOf(Color(46u, 204u, 113u, 255u)));
}

TEST(ColorTest, PredefinedStaticColors)
{
    EXPECT_EQ(Color::kWhite, Color(255u, 255u, 255u, 255u));
    EXPECT_EQ(Color::kBlack, Color(0u, 0u, 0u, 255u));
    EXPECT_EQ(Color::kTransparent, Color(0u, 0u, 0u, 0u));
    EXPECT_EQ(Color::kRed, Color(255u, 0u, 0u, 255u));
    EXPECT_EQ(Color::kGreen, Color(0u, 255u, 0u, 255u));
    EXPECT_EQ(Color::kBlue, Color(0u, 0u, 255u, 255u));
    EXPECT_EQ(Color::kYellow, Color(255u, 255u, 0u, 255u));
    EXPECT_EQ(Color::kCyan, Color(0u, 255u, 255u, 255u));
    EXPECT_EQ(Color::kMagenta, Color(255u, 0u, 255u, 255u));
    EXPECT_EQ(Color::kOrange, Color(243u, 156u, 18u, 255u));
    EXPECT_EQ(Color::kPurple, Color(169u, 7u, 228u, 255u));
    EXPECT_EQ(Color::kTurquoise, Color(26u, 188u, 156u, 255u));
    EXPECT_EQ(Color::kSilver, Color(189u, 195u, 199u, 255u));
    EXPECT_EQ(Color::kEmerald, Color(46u, 204u, 113u, 255u));
}

}   // namespace gp::math::tests
