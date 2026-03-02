// Copyright (c) - Graphical Playground. All rights reserved.

#include "Math/Scalar/Abs.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

TEST_CASE("Abs: Scalar", "[Math][Abs]")
{
    SECTION("Absolute value of positive integer") { REQUIRE(Abs(5) == 5); }

    SECTION("Absolute value of negative integer") { REQUIRE(Abs(-5) == 5); }

    SECTION("Absolute value of zero") { REQUIRE(Abs(0) == 0); }

    SECTION("Absolute value of positive float") { REQUIRE(Abs(3.14f) == 3.14f); }

    SECTION("Absolute value of negative float") { REQUIRE(Abs(-3.14f) == 3.14f); }
}

TEST_CASE("Abs: Vector2", "[Math][Abs]")
{
    SECTION("Absolute value of positive vector")
    {
        REQUIRE(Abs(TVector2<float>{ 1.0f, 2.0f }) == TVector2<float>{ 1.0f, 2.0f });
    }

    SECTION("Absolute value of negative vector")
    {
        REQUIRE(Abs(TVector2<float>{ -1.0f, -2.0f }) == TVector2<float>{ 1.0f, 2.0f });
    }

    SECTION("Absolute value of mixed vector")
    {
        REQUIRE(Abs(TVector2<float>{ -1.0f, 2.0f }) == TVector2<float>{ 1.0f, 2.0f });
    }

    SECTION("Absolute value of zero vector")
    {
        REQUIRE(Abs(TVector2<float>{ 0.0f, 0.0f }) == TVector2<float>{ 0.0f, 0.0f });
    }
}

TEST_CASE("Abs: Vector3", "[Math][Abs]")
{
    SECTION("Absolute value of positive vector")
    {
        REQUIRE(Abs(TVector3<float>{ 1.0f, 2.0f, 3.0f }) == TVector3<float>{ 1.0f, 2.0f, 3.0f });
    }

    SECTION("Absolute value of negative vector")
    {
        REQUIRE(Abs(TVector3<float>{ -1.0f, -2.0f, -3.0f }) == TVector3<float>{ 1.0f, 2.0f, 3.0f });
    }

    SECTION("Absolute value of mixed vector")
    {
        REQUIRE(Abs(TVector3<float>{ -1.0f, 2.0f, -3.0f }) == TVector3<float>{ 1.0f, 2.0f, 3.0f });
    }

    SECTION("Absolute value of zero vector")
    {
        REQUIRE(Abs(TVector3<float>{ 0.0f, 0.0f, 0.0f }) == TVector3<float>{ 0.0f, 0.0f, 0.0f });
    }
}

TEST_CASE("Abs: Vector4", "[Math][Abs]")
{
    SECTION("Absolute value of positive vector")
    {
        REQUIRE(Abs(TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f }) == TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f });
    }

    SECTION("Absolute value of negative vector")
    {
        REQUIRE(Abs(TVector4<float>{ -1.0f, -2.0f, -3.0f, -4.0f }) == TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f });
    }

    SECTION("Absolute value of mixed vector")
    {
        REQUIRE(Abs(TVector4<float>{ -1.0f, 2.0f, -3.0f, 4.0f }) == TVector4<float>{ 1.0f, 2.0f, 3.0f, 4.0f });
    }

    SECTION("Absolute value of zero vector")
    {
        REQUIRE(Abs(TVector4<float>{ 0.0f, 0.0f, 0.0f, 0.0f }) == TVector4<float>{ 0.0f, 0.0f, 0.0f, 0.0f });
    }
}
