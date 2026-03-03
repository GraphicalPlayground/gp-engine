// Copyright (c) - Graphical Playground. All rights reserved.

#include "Math/Scalar/Max.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

TEST_CASE("Max: Scalar (two arguments)", "[Math][Max]")
{
    SECTION("Int: second is larger") { REQUIRE(Max(3, 5) == 5); }

    SECTION("Int: first is larger") { REQUIRE(Max(5, 3) == 5); }

    SECTION("Int: equal values") { REQUIRE(Max(4, 4) == 4); }

    SECTION("Int: negative values") { REQUIRE(Max(-2, -7) == -2); }

    SECTION("Int: mixed sign, positive wins") { REQUIRE(Max(-1, 1) == 1); }

    SECTION("Int: zero and positive") { REQUIRE(Max(0, 10) == 10); }

    SECTION("Int: zero and negative") { REQUIRE(Max(0, -10) == 0); }

    SECTION("UInt: larger second") { REQUIRE(Max(1u, 5u) == 5u); }

    SECTION("UInt: equal values") { REQUIRE(Max(7u, 7u) == 7u); }

    SECTION("Float: second is larger") { REQUIRE(Max(1.2f, 3.5f) == 3.5f); }

    SECTION("Float: first is larger") { REQUIRE(Max(3.5f, 1.2f) == 3.5f); }

    SECTION("Float: equal values") { REQUIRE(Max(2.0f, 2.0f) == 2.0f); }

    SECTION("Float: negative values") { REQUIRE(Max(-1.0f, -4.0f) == -1.0f); }

    SECTION("Float: mixed sign") { REQUIRE(Max(-0.5f, 0.5f) == 0.5f); }

    SECTION("Float: zero and positive") { REQUIRE(Max(0.0f, 1.0f) == 1.0f); }

    SECTION("Double: larger second") { REQUIRE(Max(5.0, 10.0) == 10.0); }

    SECTION("Double: equal values") { REQUIRE(Max(3.14, 3.14) == 3.14); }

    SECTION("Double: negative values") { REQUIRE(Max(-100.0, -200.0) == -100.0); }
}

TEST_CASE("Max: Scalar (three arguments)", "[Math][Max]")
{
    SECTION("Int: third is largest") { REQUIRE(Max(1, 2, 3) == 3); }

    SECTION("Int: first is largest") { REQUIRE(Max(3, 1, 2) == 3); }

    SECTION("Int: second is largest") { REQUIRE(Max(1, 3, 2) == 3); }

    SECTION("Int: all equal") { REQUIRE(Max(5, 5, 5) == 5); }

    SECTION("Int: two equal, third larger") { REQUIRE(Max(4, 4, 9) == 9); }

    SECTION("Int: all negative, middle largest") { REQUIRE(Max(-1, -9, -3) == -1); }

    SECTION("Int: mixed sign, positive largest") { REQUIRE(Max(-5, 3, -7) == 3); }

    SECTION("Float: third is largest") { REQUIRE(Max(0.1f, 0.5f, 1.0f) == 1.0f); }

    SECTION("Float: first is largest") { REQUIRE(Max(3.0f, 2.0f, 1.0f) == 3.0f); }

    SECTION("Float: all equal") { REQUIRE(Max(7.7f, 7.7f, 7.7f) == 7.7f); }

    SECTION("Double: second is largest") { REQUIRE(Max(0.001, 10.0, 5.0) == 10.0); }
}

TEST_CASE("Max: Vector2", "[Math][Max]")
{
    SECTION("First vector has all larger components")
    {
        REQUIRE(Max(TVector2<float>{ 3.0f, 4.0f }, TVector2<float>{ 1.0f, 2.0f }) == TVector2<float>{ 3.0f, 4.0f });
    }

    SECTION("Second vector has all larger components")
    {
        REQUIRE(Max(TVector2<float>{ 1.0f, 2.0f }, TVector2<float>{ 3.0f, 4.0f }) == TVector2<float>{ 3.0f, 4.0f });
    }

    SECTION("Mixed: first x larger, second y larger")
    {
        REQUIRE(Max(TVector2<float>{ 3.0f, 2.0f }, TVector2<float>{ 1.0f, 5.0f }) == TVector2<float>{ 3.0f, 5.0f });
    }

    SECTION("Mixed: second x larger, first y larger")
    {
        REQUIRE(Max(TVector2<float>{ 1.0f, 5.0f }, TVector2<float>{ 3.0f, 2.0f }) == TVector2<float>{ 3.0f, 5.0f });
    }

    SECTION("Equal vectors")
    {
        REQUIRE(Max(TVector2<float>{ 2.0f, 2.0f }, TVector2<float>{ 2.0f, 2.0f }) == TVector2<float>{ 2.0f, 2.0f });
    }

    SECTION("Negative components")
    {
        REQUIRE(
            Max(TVector2<float>{ -1.0f, -4.0f }, TVector2<float>{ -3.0f, -2.0f }) == TVector2<float>{ -1.0f, -2.0f }
        );
    }

    SECTION("Mixed sign components")
    {
        REQUIRE(Max(TVector2<float>{ -1.0f, 2.0f }, TVector2<float>{ 1.0f, -2.0f }) == TVector2<float>{ 1.0f, 2.0f });
    }

    SECTION("Zero vectors")
    {
        REQUIRE(Max(TVector2<float>{ 0.0f, 0.0f }, TVector2<float>{ 0.0f, 0.0f }) == TVector2<float>{ 0.0f, 0.0f });
    }

    SECTION("Double precision")
    {
        REQUIRE(Max(TVector2<double>{ 1.0, 3.0 }, TVector2<double>{ 2.0, 0.5 }) == TVector2<double>{ 2.0, 3.0 });
    }
}

TEST_CASE("Max: Vector3", "[Math][Max]")
{
    SECTION("First vector has all larger components")
    {
        REQUIRE(
            Max(TVector3<float>{ 4.0f, 5.0f, 6.0f }, TVector3<float>{ 1.0f, 2.0f, 3.0f }) ==
            TVector3<float>{ 4.0f, 5.0f, 6.0f }
        );
    }

    SECTION("Second vector has all larger components")
    {
        REQUIRE(
            Max(TVector3<float>{ 1.0f, 2.0f, 3.0f }, TVector3<float>{ 4.0f, 5.0f, 6.0f }) ==
            TVector3<float>{ 4.0f, 5.0f, 6.0f }
        );
    }

    SECTION("Mixed components")
    {
        REQUIRE(
            Max(TVector3<float>{ 4.0f, 2.0f, 6.0f }, TVector3<float>{ 1.0f, 5.0f, 3.0f }) ==
            TVector3<float>{ 4.0f, 5.0f, 6.0f }
        );
    }

    SECTION("Equal vectors")
    {
        REQUIRE(
            Max(TVector3<float>{ 3.0f, 3.0f, 3.0f }, TVector3<float>{ 3.0f, 3.0f, 3.0f }) ==
            TVector3<float>{ 3.0f, 3.0f, 3.0f }
        );
    }

    SECTION("Negative components")
    {
        REQUIRE(
            Max(TVector3<float>{ -1.0f, -4.0f, -2.0f }, TVector3<float>{ -3.0f, -2.0f, -5.0f }) ==
            TVector3<float>{ -1.0f, -2.0f, -2.0f }
        );
    }

    SECTION("Mixed sign components")
    {
        REQUIRE(
            Max(TVector3<float>{ -1.0f, 2.0f, -3.0f }, TVector3<float>{ 1.0f, -2.0f, 3.0f }) ==
            TVector3<float>{ 1.0f, 2.0f, 3.0f }
        );
    }

    SECTION("Zero vectors")
    {
        REQUIRE(
            Max(TVector3<float>{ 0.0f, 0.0f, 0.0f }, TVector3<float>{ 0.0f, 0.0f, 0.0f }) ==
            TVector3<float>{ 0.0f, 0.0f, 0.0f }
        );
    }

    SECTION("Double precision")
    {
        REQUIRE(
            Max(TVector3<double>{ 1.0, 3.0, 5.0 }, TVector3<double>{ 2.0, 0.5, 4.0 }) ==
            TVector3<double>{ 2.0, 3.0, 5.0 }
        );
    }
}

TEST_CASE("Max: Vector4", "[Math][Max]")
{
    SECTION("First vector has all larger components")
    {
        REQUIRE(
            Max(TVector4<float>{ 5.0f, 6.0f, 7.0f, 8.0f }, TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f }) ==
            TVector4<float>{ 5.0f, 6.0f, 7.0f, 8.0f }
        );
    }

    SECTION("Second vector has all larger components")
    {
        REQUIRE(
            Max(TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f }, TVector4<float>{ 5.0f, 6.0f, 7.0f, 8.0f }) ==
            TVector4<float>{ 5.0f, 6.0f, 7.0f, 8.0f }
        );
    }

    SECTION("Mixed components")
    {
        REQUIRE(
            Max(TVector4<float>{ 5.0f, 2.0f, 7.0f, 4.0f }, TVector4<float>{ 1.0f, 6.0f, 3.0f, 8.0f }) ==
            TVector4<float>{ 5.0f, 6.0f, 7.0f, 8.0f }
        );
    }

    SECTION("Equal vectors")
    {
        REQUIRE(
            Max(TVector4<float>{ 4.0f, 4.0f, 4.0f, 4.0f }, TVector4<float>{ 4.0f, 4.0f, 4.0f, 4.0f }) ==
            TVector4<float>{ 4.0f, 4.0f, 4.0f, 4.0f }
        );
    }

    SECTION("Negative components")
    {
        REQUIRE(
            Max(TVector4<float>{ -1.0f, -4.0f, -2.0f, -7.0f }, TVector4<float>{ -3.0f, -2.0f, -5.0f, -6.0f }) ==
            TVector4<float>{ -1.0f, -2.0f, -2.0f, -6.0f }
        );
    }

    SECTION("Mixed sign components")
    {
        REQUIRE(
            Max(TVector4<float>{ -1.0f, 2.0f, -3.0f, 4.0f }, TVector4<float>{ 1.0f, -2.0f, 3.0f, -4.0f }) ==
            TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f }
        );
    }

    SECTION("Zero vectors")
    {
        REQUIRE(
            Max(TVector4<float>{ 0.0f, 0.0f, 0.0f, 0.0f }, TVector4<float>{ 0.0f, 0.0f, 0.0f, 0.0f }) ==
            TVector4<float>{ 0.0f, 0.0f, 0.0f, 0.0f }
        );
    }

    SECTION("Double precision")
    {
        REQUIRE(
            Max(TVector4<double>{ 1.0, 3.0, 5.0, 7.0 }, TVector4<double>{ 2.0, 0.5, 4.0, 9.0 }) ==
            TVector4<double>{ 2.0, 3.0, 5.0, 9.0 }
        );
    }
}
