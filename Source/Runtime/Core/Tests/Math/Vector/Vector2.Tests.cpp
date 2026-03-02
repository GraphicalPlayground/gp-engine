// Copyright (c) - Graphical Playground. All rights reserved.

#include "Math/Vector/Vector2.hpp"
#include "Math/Vector/Vector3.hpp"
#include "Math/Vector/Vector4.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

TEST_CASE("Vector2: Constructors", "[Math][Vector2]")
{
    SECTION("Default constructor initializes to zero")
    {
        TVector2<float> v;
        REQUIRE(v.x == 0.0f);
        REQUIRE(v.y == 0.0f);
    }

    SECTION("Constructor with components")
    {
        TVector2<float> v(1.5f, -2.5f);
        REQUIRE(v.x == 1.5f);
        REQUIRE(v.y == -2.5f);
    }

    SECTION("Constructor from pointer to array")
    {
        float* arr = new float[2]{ 3.0f, 4.0f };
        TVector2<float> v(arr);
        REQUIRE(v.x == 3.0f);
        REQUIRE(v.y == 4.0f);
        delete[] arr;
    }

    SECTION("Constructor from c-style array")
    {
        float arr[2] = { 3.0f, 4.0f };
        TVector2<float> v(arr);
        REQUIRE(v.x == 3.0f);
        REQUIRE(v.y == 4.0f);
    }

    SECTION("Explicit constructor from TVector3")
    {
        TVector3<float> vec3(5.0f, 6.0f, 7.0f);
        TVector2<float> v{ vec3 };
        REQUIRE(v.x == 5.0f);
        REQUIRE(v.y == 6.0f);
    }

    SECTION("Explicit constructor from TVector4")
    {
        TVector4<float> vec4(8.0f, 9.0f, 10.0f, 11.0f);
        TVector2<float> v{ vec4 };
        REQUIRE(v.x == 8.0f);
        REQUIRE(v.y == 9.0f);
    }
}

TEST_CASE("Vector2: Equality operators", "[Math][Vector2]")
{
    SECTION("Equality operator with identical vectors")
    {
        TVector2<float> v1(1.0f, 2.0f);
        TVector2<float> v2(1.0f, 2.0f);
        REQUIRE(v1 == v2);
    }

    SECTION("Equality operator with different vectors")
    {
        TVector2<float> v1(1.0f, 2.0f);
        TVector2<float> v2(3.0f, 4.0f);
        REQUIRE_FALSE(v1 == v2);
    }

    SECTION("Inequality operator with identical vectors")
    {
        TVector2<float> v1(1.0f, 2.0f);
        TVector2<float> v2(1.0f, 2.0f);
        REQUIRE_FALSE(v1 != v2);
    }

    SECTION("Inequality operator with different vectors")
    {
        TVector2<float> v1(1.0f, 2.0f);
        TVector2<float> v2(3.0f, 4.0f);
        REQUIRE(v1 != v2);
    }
}
