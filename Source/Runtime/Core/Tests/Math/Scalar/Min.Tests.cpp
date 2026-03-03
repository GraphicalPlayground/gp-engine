// Copyright (c) - Graphical Playground. All rights reserved.

#include "Math/Scalar/Min.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

TEST_CASE("Min: Scalar (two arguments)", "[Math][Min]")
{
    SECTION("Int: second is smaller") { REQUIRE(Min(5, 3) == 3); }

    SECTION("Int: first is smaller") { REQUIRE(Min(3, 5) == 3); }

    SECTION("Int: equal values") { REQUIRE(Min(4, 4) == 4); }

    SECTION("Int: negative values") { REQUIRE(Min(-2, -7) == -7); }

    SECTION("Int: mixed sign, negative wins") { REQUIRE(Min(-1, 1) == -1); }

    SECTION("Int: zero and positive") { REQUIRE(Min(0, 10) == 0); }

    SECTION("Int: zero and negative") { REQUIRE(Min(0, -10) == -10); }

    SECTION("UInt: smaller first") { REQUIRE(Min(1u, 5u) == 1u); }

    SECTION("UInt: equal values") { REQUIRE(Min(7u, 7u) == 7u); }

    SECTION("Float: second is smaller") { REQUIRE(Min(3.5f, 1.2f) == 1.2f); }

    SECTION("Float: first is smaller") { REQUIRE(Min(1.2f, 3.5f) == 1.2f); }

    SECTION("Float: equal values") { REQUIRE(Min(2.0f, 2.0f) == 2.0f); }

    SECTION("Float: negative values") { REQUIRE(Min(-1.0f, -4.0f) == -4.0f); }

    SECTION("Float: mixed sign") { REQUIRE(Min(-0.5f, 0.5f) == -0.5f); }

    SECTION("Float: zero and positive") { REQUIRE(Min(0.0f, 1.0f) == 0.0f); }

    SECTION("Double: smaller second") { REQUIRE(Min(10.0, 5.0) == 5.0); }

    SECTION("Double: equal values") { REQUIRE(Min(3.14, 3.14) == 3.14); }

    SECTION("Double: negative values") { REQUIRE(Min(-100.0, -200.0) == -200.0); }
}

TEST_CASE("Min: Scalar (three arguments)", "[Math][Min]")
{
    SECTION("Int: first is smallest") { REQUIRE(Min(1, 2, 3) == 1); }

    SECTION("Int: second is smallest") { REQUIRE(Min(3, 1, 2) == 1); }

    SECTION("Int: third is smallest") { REQUIRE(Min(3, 2, 1) == 1); }

    SECTION("Int: all equal") { REQUIRE(Min(5, 5, 5) == 5); }

    SECTION("Int: two equal, third smaller") { REQUIRE(Min(4, 4, -1) == -1); }

    SECTION("Int: all negative, middle smallest") { REQUIRE(Min(-1, -9, -3) == -9); }

    SECTION("Int: mixed sign, negative smallest") { REQUIRE(Min(5, -3, 7) == -3); }

    SECTION("Float: first is smallest") { REQUIRE(Min(0.1f, 0.5f, 1.0f) == 0.1f); }

    SECTION("Float: third is smallest") { REQUIRE(Min(3.0f, 2.0f, 1.0f) == 1.0f); }

    SECTION("Float: all equal") { REQUIRE(Min(7.7f, 7.7f, 7.7f) == 7.7f); }

    SECTION("Double: second is smallest") { REQUIRE(Min(10.0, 0.001, 5.0) == 0.001); }
}

TEST_CASE("Min: Vector2", "[Math][Min]")
{
    SECTION("First vector has all smaller components")
    {
        REQUIRE(Min(TVector2<float>{ 1.0f, 2.0f }, TVector2<float>{ 3.0f, 4.0f }) == TVector2<float>{ 1.0f, 2.0f });
    }

    SECTION("Second vector has all smaller components")
    {
        REQUIRE(Min(TVector2<float>{ 3.0f, 4.0f }, TVector2<float>{ 1.0f, 2.0f }) == TVector2<float>{ 1.0f, 2.0f });
    }

    SECTION("Mixed: first x smaller, second y smaller")
    {
        REQUIRE(Min(TVector2<float>{ 1.0f, 5.0f }, TVector2<float>{ 3.0f, 2.0f }) == TVector2<float>{ 1.0f, 2.0f });
    }

    SECTION("Mixed: second x smaller, first y smaller")
    {
        REQUIRE(Min(TVector2<float>{ 3.0f, 2.0f }, TVector2<float>{ 1.0f, 5.0f }) == TVector2<float>{ 1.0f, 2.0f });
    }

    SECTION("Equal vectors")
    {
        REQUIRE(Min(TVector2<float>{ 2.0f, 2.0f }, TVector2<float>{ 2.0f, 2.0f }) == TVector2<float>{ 2.0f, 2.0f });
    }

    SECTION("Negative components")
    {
        REQUIRE(
            Min(TVector2<float>{ -1.0f, -4.0f }, TVector2<float>{ -3.0f, -2.0f }) == TVector2<float>{ -3.0f, -4.0f }
        );
    }

    SECTION("Mixed sign components")
    {
        REQUIRE(Min(TVector2<float>{ -1.0f, 2.0f }, TVector2<float>{ 1.0f, -2.0f }) == TVector2<float>{ -1.0f, -2.0f });
    }

    SECTION("Zero vectors")
    {
        REQUIRE(Min(TVector2<float>{ 0.0f, 0.0f }, TVector2<float>{ 0.0f, 0.0f }) == TVector2<float>{ 0.0f, 0.0f });
    }

    SECTION("Double precision")
    {
        REQUIRE(Min(TVector2<double>{ 1.0, 3.0 }, TVector2<double>{ 2.0, 0.5 }) == TVector2<double>{ 1.0, 0.5 });
    }
}

TEST_CASE("Min: Vector3", "[Math][Min]")
{
    SECTION("First vector has all smaller components")
    {
        REQUIRE(
            Min(TVector3<float>{ 1.0f, 2.0f, 3.0f }, TVector3<float>{ 4.0f, 5.0f, 6.0f }) ==
            TVector3<float>{ 1.0f, 2.0f, 3.0f }
        );
    }

    SECTION("Second vector has all smaller components")
    {
        REQUIRE(
            Min(TVector3<float>{ 4.0f, 5.0f, 6.0f }, TVector3<float>{ 1.0f, 2.0f, 3.0f }) ==
            TVector3<float>{ 1.0f, 2.0f, 3.0f }
        );
    }

    SECTION("Mixed components")
    {
        REQUIRE(
            Min(TVector3<float>{ 1.0f, 5.0f, 3.0f }, TVector3<float>{ 4.0f, 2.0f, 6.0f }) ==
            TVector3<float>{ 1.0f, 2.0f, 3.0f }
        );
    }

    SECTION("Equal vectors")
    {
        REQUIRE(
            Min(TVector3<float>{ 3.0f, 3.0f, 3.0f }, TVector3<float>{ 3.0f, 3.0f, 3.0f }) ==
            TVector3<float>{ 3.0f, 3.0f, 3.0f }
        );
    }

    SECTION("Negative components")
    {
        REQUIRE(
            Min(TVector3<float>{ -1.0f, -4.0f, -2.0f }, TVector3<float>{ -3.0f, -2.0f, -5.0f }) ==
            TVector3<float>{ -3.0f, -4.0f, -5.0f }
        );
    }

    SECTION("Mixed sign components")
    {
        REQUIRE(
            Min(TVector3<float>{ -1.0f, 2.0f, -3.0f }, TVector3<float>{ 1.0f, -2.0f, 3.0f }) ==
            TVector3<float>{ -1.0f, -2.0f, -3.0f }
        );
    }

    SECTION("Zero vectors")
    {
        REQUIRE(
            Min(TVector3<float>{ 0.0f, 0.0f, 0.0f }, TVector3<float>{ 0.0f, 0.0f, 0.0f }) ==
            TVector3<float>{ 0.0f, 0.0f, 0.0f }
        );
    }

    SECTION("Double precision")
    {
        REQUIRE(
            Min(TVector3<double>{ 1.0, 3.0, 5.0 }, TVector3<double>{ 2.0, 0.5, 4.0 }) ==
            TVector3<double>{ 1.0, 0.5, 4.0 }
        );
    }
}

TEST_CASE("Min: Vector4", "[Math][Min]")
{
    SECTION("First vector has all smaller components")
    {
        REQUIRE(
            Min(TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f }, TVector4<float>{ 5.0f, 6.0f, 7.0f, 8.0f }) ==
            TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f }
        );
    }

    SECTION("Second vector has all smaller components")
    {
        REQUIRE(
            Min(TVector4<float>{ 5.0f, 6.0f, 7.0f, 8.0f }, TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f }) ==
            TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f }
        );
    }

    SECTION("Mixed components")
    {
        REQUIRE(
            Min(TVector4<float>{ 1.0f, 6.0f, 3.0f, 8.0f }, TVector4<float>{ 5.0f, 2.0f, 7.0f, 4.0f }) ==
            TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f }
        );
    }

    SECTION("Equal vectors")
    {
        REQUIRE(
            Min(TVector4<float>{ 4.0f, 4.0f, 4.0f, 4.0f }, TVector4<float>{ 4.0f, 4.0f, 4.0f, 4.0f }) ==
            TVector4<float>{ 4.0f, 4.0f, 4.0f, 4.0f }
        );
    }

    SECTION("Negative components")
    {
        REQUIRE(
            Min(TVector4<float>{ -1.0f, -4.0f, -2.0f, -7.0f }, TVector4<float>{ -3.0f, -2.0f, -5.0f, -6.0f }) ==
            TVector4<float>{ -3.0f, -4.0f, -5.0f, -7.0f }
        );
    }

    SECTION("Mixed sign components")
    {
        REQUIRE(
            Min(TVector4<float>{ -1.0f, 2.0f, -3.0f, 4.0f }, TVector4<float>{ 1.0f, -2.0f, 3.0f, -4.0f }) ==
            TVector4<float>{ -1.0f, -2.0f, -3.0f, -4.0f }
        );
    }

    SECTION("Zero vectors")
    {
        REQUIRE(
            Min(TVector4<float>{ 0.0f, 0.0f, 0.0f, 0.0f }, TVector4<float>{ 0.0f, 0.0f, 0.0f, 0.0f }) ==
            TVector4<float>{ 0.0f, 0.0f, 0.0f, 0.0f }
        );
    }

    SECTION("Double precision")
    {
        REQUIRE(
            Min(TVector4<double>{ 1.0, 3.0, 5.0, 7.0 }, TVector4<double>{ 2.0, 0.5, 4.0, 9.0 }) ==
            TVector4<double>{ 1.0, 0.5, 4.0, 7.0 }
        );
    }
}
