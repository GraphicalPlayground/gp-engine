// Copyright (c) - Graphical Playground. All rights reserved.

#include "Crypto/Hash/NoiseHash.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("NoiseHash Hashing", "[GP][Core][Crypto][Hash][NoiseHash]")
{
    using namespace GP::Crypto;

    SECTION("Squirrel3 - Adjacent Positions Produce Different Values")
    {
        auto h0 = NoiseHash::Squirrel3(0);
        auto h1 = NoiseHash::Squirrel3(1);
        auto h2 = NoiseHash::Squirrel3(2);
        auto h3 = NoiseHash::Squirrel3(3);

        REQUIRE(h0 != h1);
        REQUIRE(h1 != h2);
        REQUIRE(h2 != h3);
        REQUIRE(h0 != h3);
    }

    SECTION("Squirrel3 - Different Seeds Produce Different Values")
    {
        auto h1 = NoiseHash::Squirrel3(42, 0);
        auto h2 = NoiseHash::Squirrel3(42, 1);
        auto h3 = NoiseHash::Squirrel3(42, 0xDEADBEEFU);

        REQUIRE(h1 != h2);
        REQUIRE(h1 != h3);
        REQUIRE(h2 != h3);
    }

    SECTION("Squirrel3 - Deterministic Output")
    {
        auto h1 = NoiseHash::Squirrel3(100, 7);
        auto h2 = NoiseHash::Squirrel3(100, 7);
        REQUIRE(h1 == h2);
    }

    SECTION("Squirrel5 - Adjacent Positions Produce Different Values")
    {
        auto h0 = NoiseHash::Squirrel5(0);
        auto h1 = NoiseHash::Squirrel5(1);
        auto h2 = NoiseHash::Squirrel5(2);
        auto h3 = NoiseHash::Squirrel5(3);

        REQUIRE(h0 != h1);
        REQUIRE(h1 != h2);
        REQUIRE(h2 != h3);
        REQUIRE(h0 != h3);
    }

    SECTION("Squirrel5 - Different Seeds Produce Different Values")
    {
        auto h1 = NoiseHash::Squirrel5(42, 0);
        auto h2 = NoiseHash::Squirrel5(42, 1);
        auto h3 = NoiseHash::Squirrel5(42, 0xDEADBEEFU);

        REQUIRE(h1 != h2);
        REQUIRE(h1 != h3);
        REQUIRE(h2 != h3);
    }

    SECTION("Squirrel5 - Deterministic Output")
    {
        auto h1 = NoiseHash::Squirrel5(100, 7);
        auto h2 = NoiseHash::Squirrel5(100, 7);
        REQUIRE(h1 == h2);
    }

    SECTION("Squirrel3 and Squirrel5 Produce Different Values")
    {
        auto h3 = NoiseHash::Squirrel3(42, 0);
        auto h5 = NoiseHash::Squirrel5(42, 0);
        REQUIRE(h3 != h5);
    }

    SECTION("WangHash32 - Adjacent Keys Produce Different Values")
    {
        auto h0 = NoiseHash::WangHash32(0);
        auto h1 = NoiseHash::WangHash32(1);
        auto h2 = NoiseHash::WangHash32(2);
        auto h3 = NoiseHash::WangHash32(3);

        REQUIRE(h0 != h1);
        REQUIRE(h1 != h2);
        REQUIRE(h2 != h3);
    }

    SECTION("WangHash32 - Deterministic Output")
    {
        auto h1 = NoiseHash::WangHash32(12345);
        auto h2 = NoiseHash::WangHash32(12345);
        REQUIRE(h1 == h2);
    }

    SECTION("WangHash64 - Adjacent Keys Produce Different Values")
    {
        auto h0 = NoiseHash::WangHash64(0);
        auto h1 = NoiseHash::WangHash64(1);
        auto h2 = NoiseHash::WangHash64(2);
        auto h3 = NoiseHash::WangHash64(3);

        REQUIRE(h0 != h1);
        REQUIRE(h1 != h2);
        REQUIRE(h2 != h3);
    }

    SECTION("WangHash64 - Deterministic Output")
    {
        auto h1 = NoiseHash::WangHash64(12345ULL);
        auto h2 = NoiseHash::WangHash64(12345ULL);
        REQUIRE(h1 == h2);
    }

    SECTION("PCGHash32 - Adjacent Inputs Produce Different Values")
    {
        auto h0 = NoiseHash::PCGHash32(0);
        auto h1 = NoiseHash::PCGHash32(1);
        auto h2 = NoiseHash::PCGHash32(2);
        auto h3 = NoiseHash::PCGHash32(3);

        REQUIRE(h0 != h1);
        REQUIRE(h1 != h2);
        REQUIRE(h2 != h3);
    }

    SECTION("PCGHash32 - Deterministic Output")
    {
        auto h1 = NoiseHash::PCGHash32(12345);
        auto h2 = NoiseHash::PCGHash32(12345);
        REQUIRE(h1 == h2);
    }

    SECTION("ToFloat01 Range")
    {
        auto f0 = NoiseHash::ToFloat01(0U);
        REQUIRE(f0 >= 0.0f);
        REQUIRE(f0 < 1.0f);

        auto fMax = NoiseHash::ToFloat01(0xFFFFFFFFU);
        REQUIRE(fMax >= 0.0f);
        REQUIRE(fMax < 1.0f);

        auto fMid = NoiseHash::ToFloat01(0x80000000U);
        REQUIRE(fMid >= 0.0f);
        REQUIRE(fMid < 1.0f);

        // Verify range across many hashed values
        for (GP::Int32 i = 0; i < 1000; ++i)
        {
            auto val = NoiseHash::ToFloat01(NoiseHash::Squirrel5(i));
            REQUIRE(val >= 0.0f);
            REQUIRE(val < 1.0f);
        }
    }

    SECTION("ToFloatNeg11 Range")
    {
        auto f0 = NoiseHash::ToFloatNeg11(0U);
        REQUIRE(f0 >= -1.0f);
        REQUIRE(f0 < 1.0f);

        auto fMax = NoiseHash::ToFloatNeg11(0xFFFFFFFFU);
        REQUIRE(fMax >= -1.0f);
        REQUIRE(fMax < 1.0f);

        auto fMid = NoiseHash::ToFloatNeg11(0x80000000U);
        REQUIRE(fMid >= -1.0f);
        REQUIRE(fMid < 1.0f);

        // Verify range across many hashed values
        for (GP::Int32 i = 0; i < 1000; ++i)
        {
            auto val = NoiseHash::ToFloatNeg11(NoiseHash::Squirrel5(i));
            REQUIRE(val >= -1.0f);
            REQUIRE(val < 1.0f);
        }
    }

    SECTION("Noise2D - Deterministic Output")
    {
        auto h1 = NoiseHash::Noise2D(10, 20, 0);
        auto h2 = NoiseHash::Noise2D(10, 20, 0);
        REQUIRE(h1 == h2);

        auto h3 = NoiseHash::Noise2D(10, 20, 1);
        REQUIRE(h1 != h3);
    }

    SECTION("Noise2D - Different Coordinates Produce Different Values")
    {
        auto h00 = NoiseHash::Noise2D(0, 0);
        auto h10 = NoiseHash::Noise2D(1, 0);
        auto h01 = NoiseHash::Noise2D(0, 1);
        auto h11 = NoiseHash::Noise2D(1, 1);

        REQUIRE(h00 != h10);
        REQUIRE(h00 != h01);
        REQUIRE(h00 != h11);
        REQUIRE(h10 != h01);
    }

    SECTION("Noise3D - Deterministic Output")
    {
        auto h1 = NoiseHash::Noise3D(5, 10, 15, 0);
        auto h2 = NoiseHash::Noise3D(5, 10, 15, 0);
        REQUIRE(h1 == h2);

        auto h3 = NoiseHash::Noise3D(5, 10, 15, 1);
        REQUIRE(h1 != h3);
    }

    SECTION("Noise3D - Different Coordinates Produce Different Values")
    {
        auto h000 = NoiseHash::Noise3D(0, 0, 0);
        auto h100 = NoiseHash::Noise3D(1, 0, 0);
        auto h010 = NoiseHash::Noise3D(0, 1, 0);
        auto h001 = NoiseHash::Noise3D(0, 0, 1);

        REQUIRE(h000 != h100);
        REQUIRE(h000 != h010);
        REQUIRE(h000 != h001);
    }

    SECTION("Noise4D - Deterministic Output")
    {
        auto h1 = NoiseHash::Noise4D(1, 2, 3, 4, 0);
        auto h2 = NoiseHash::Noise4D(1, 2, 3, 4, 0);
        REQUIRE(h1 == h2);

        auto h3 = NoiseHash::Noise4D(1, 2, 3, 4, 1);
        REQUIRE(h1 != h3);
    }

    SECTION("Noise4D - Different Coordinates Produce Different Values")
    {
        auto h0000 = NoiseHash::Noise4D(0, 0, 0, 0);
        auto h1000 = NoiseHash::Noise4D(1, 0, 0, 0);
        auto h0100 = NoiseHash::Noise4D(0, 1, 0, 0);
        auto h0010 = NoiseHash::Noise4D(0, 0, 1, 0);
        auto h0001 = NoiseHash::Noise4D(0, 0, 0, 1);

        REQUIRE(h0000 != h1000);
        REQUIRE(h0000 != h0100);
        REQUIRE(h0000 != h0010);
        REQUIRE(h0000 != h0001);
    }

    SECTION("Compile-time Evaluation")
    {
        constexpr auto h1 = NoiseHash::Squirrel3(42, 7);
        REQUIRE(h1 == NoiseHash::Squirrel3(42, 7));

        constexpr auto h2 = NoiseHash::Squirrel5(42, 7);
        REQUIRE(h2 == NoiseHash::Squirrel5(42, 7));

        constexpr auto h3 = NoiseHash::WangHash32(42);
        REQUIRE(h3 == NoiseHash::WangHash32(42));

        constexpr auto h4 = NoiseHash::WangHash64(42ULL);
        REQUIRE(h4 == NoiseHash::WangHash64(42ULL));

        constexpr auto h5 = NoiseHash::PCGHash32(42);
        REQUIRE(h5 == NoiseHash::PCGHash32(42));

        constexpr auto h6 = NoiseHash::Noise2D(1, 2, 3);
        REQUIRE(h6 == NoiseHash::Noise2D(1, 2, 3));

        constexpr auto h7 = NoiseHash::Noise3D(1, 2, 3, 4);
        REQUIRE(h7 == NoiseHash::Noise3D(1, 2, 3, 4));

        constexpr auto h8 = NoiseHash::Noise4D(1, 2, 3, 4, 5);
        REQUIRE(h8 == NoiseHash::Noise4D(1, 2, 3, 4, 5));
    }

    SECTION("Compile-time ToFloat01 and ToFloatNeg11")
    {
        constexpr auto f0 = NoiseHash::ToFloat01(0U);
        constexpr auto fMax = NoiseHash::ToFloat01(0xFFFFFFFFU);
        constexpr auto fn0 = NoiseHash::ToFloatNeg11(0U);
        constexpr auto fnMax = NoiseHash::ToFloatNeg11(0xFFFFFFFFU);

        REQUIRE(f0 >= 0.0f);
        REQUIRE(f0 < 1.0f);
        REQUIRE(fMax >= 0.0f);
        REQUIRE(fMax < 1.0f);
        REQUIRE(fn0 >= -1.0f);
        REQUIRE(fn0 < 1.0f);
        REQUIRE(fnMax >= -1.0f);
        REQUIRE(fnMax < 1.0f);
    }

    SECTION("WangHash32 - Zero and Max Input")
    {
        auto h0 = NoiseHash::WangHash32(0U);
        auto hMax = NoiseHash::WangHash32(0xFFFFFFFFU);
        REQUIRE(h0 != hMax);
        // Both must still produce valid-looking (non-trivial) values.
        REQUIRE(h0 != 0U);
        REQUIRE(hMax != 0U);
    }

    SECTION("WangHash64 - Zero and Max Input")
    {
        auto h0 = NoiseHash::WangHash64(0ULL);
        auto hMax = NoiseHash::WangHash64(0xFFFFFFFFFFFFFFFFULL);
        REQUIRE(h0 != hMax);
        REQUIRE(h0 != 0ULL);
        REQUIRE(hMax != 0ULL);
    }

    SECTION("PCGHash32 - Zero Input")
    {
        auto h = NoiseHash::PCGHash32(0U);
        // PCG with input 0 should still avalanche to a non-trivial value.
        REQUIRE(h != 0U);
        REQUIRE(h == NoiseHash::PCGHash32(0U));
    }

    SECTION("ToFloat01 - Maps Uniformly Across Full Range")
    {
        // Verify monotonicity is NOT guaranteed (it's a hash, not a sorted map),
        // but that the extremes and midpoint are all inside [0, 1).
        REQUIRE(NoiseHash::ToFloat01(0U) >= 0.0f);
        REQUIRE(NoiseHash::ToFloat01(0x7FFFFFFFU) >= 0.0f);
        REQUIRE(NoiseHash::ToFloat01(0x80000000U) >= 0.0f);
        REQUIRE(NoiseHash::ToFloat01(0xFFFFFFFFU) < 1.0f);
    }

    SECTION("ToFloatNeg11 - Symmetric Around Zero")
    {
        // 0x80000000 maps to the midpoint of the uint32 range, so the
        // result should be close to but not exactly 0 (due to bit-shift truncation).
        auto mid = NoiseHash::ToFloatNeg11(0x80000000U);
        REQUIRE(mid >= -1.0f);
        REQUIRE(mid < 1.0f);

        auto lowest = NoiseHash::ToFloatNeg11(0U);
        REQUIRE(lowest >= -1.0f);
        REQUIRE(lowest < 0.0f);
    }

    SECTION("Noise4D - Symmetry: Swapped Coordinates Differ")
    {
        auto hABCD = NoiseHash::Noise4D(1, 2, 3, 4);
        auto hDCBA = NoiseHash::Noise4D(4, 3, 2, 1);
        REQUIRE(hABCD != hDCBA);
    }
}
