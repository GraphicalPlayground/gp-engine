// Copyright (c) - Graphical Playground. All rights reserved.

#include "Math/Vector/Vector2.hpp"
#include "Math/Vector/Vector3.hpp"
#include "Math/Vector/Vector4.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

TEST_CASE("Vector4: Constructors", "[Math][Vector4]")
{
    SECTION("Default constructor initializes to zero")
    {
        TVector4<float> v;
        REQUIRE(v.x == 0.0f);
        REQUIRE(v.y == 0.0f);
        REQUIRE(v.z == 0.0f);
        REQUIRE(v.w == 0.0f);
    }

    SECTION("Constructor with components")
    {
        TVector4<float> v(1.5f, -2.5f, 3.0f, 0.0f);
        REQUIRE(v.x == 1.5f);
        REQUIRE(v.y == -2.5f);
        REQUIRE(v.z == 3.0f);
        REQUIRE(v.w == 0.0f);
    }

    SECTION("Constructor from pointer to array")
    {
        float* arr = new float[4]{ 3.0f, 4.0f, 5.0f, 6.0f };
        TVector4<float> v(arr);
        REQUIRE(v.x == 3.0f);
        REQUIRE(v.y == 4.0f);
        REQUIRE(v.z == 5.0f);
        REQUIRE(v.w == 6.0f);
        delete[] arr;
    }

    SECTION("Constructor from c-style array")
    {
        float arr[4] = { 3.0f, 4.0f, 5.0f, 6.0f };
        TVector4<float> v(arr);
        REQUIRE(v.x == 3.0f);
        REQUIRE(v.y == 4.0f);
        REQUIRE(v.z == 5.0f);
        REQUIRE(v.w == 6.0f);
    }

    SECTION("Explicit constructor from TVector2")
    {
        TVector2<float> vec2(5.0f, 6.0f);
        TVector4<float> v{ vec2, 1.0f, 2.0f };
        REQUIRE(v.x == 5.0f);
        REQUIRE(v.y == 6.0f);
        REQUIRE(v.z == 1.0f);
        REQUIRE(v.w == 2.0f);
    }

    SECTION("Explicit constructor from TVector3")
    {
        TVector3<float> vec3(8.0f, 9.0f, 10.0f);
        TVector4<float> v{ vec3, 12.0f };
        REQUIRE(v.x == 8.0f);
        REQUIRE(v.y == 9.0f);
        REQUIRE(v.z == 10.0f);
        REQUIRE(v.w == 12.0f);
    }
}
