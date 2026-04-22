// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "math/LinearAlgebra.hpp"
#include <catch2/catch_test_macros.hpp>

namespace gp::math::tests
{

TEST_CASE("min - two values (int)", "[math][LinearAlgebra][min]")
{
    REQUIRE(min(3, 5) == 3);
    REQUIRE(min(-1, 1) == -1);
    REQUIRE(min(0, 0) == 0);
}

TEST_CASE("min - three values (int)", "[math][LinearAlgebra][min]")
{
    REQUIRE(min(3, 5, 2) == 2);
    REQUIRE(min(-1, 1, -2) == -2);
    REQUIRE(min(0, 0, 0) == 0);
}

TEST_CASE("max - two values (int)", "[math][LinearAlgebra][max]")
{
    REQUIRE(max(3, 5) == 5);
    REQUIRE(max(-1, 1) == 1);
    REQUIRE(max(0, 0) == 0);
}

TEST_CASE("max - three values (int)", "[math][LinearAlgebra][max]")
{
    REQUIRE(max(3, 5, 2) == 5);
    REQUIRE(max(-1, 1, -2) == 1);
    REQUIRE(max(0, 0, 0) == 0);
}

TEST_CASE("clamp (int)", "[math][LinearAlgebra][clamp]")
{
    REQUIRE(clamp(5, 1, 10) == 5);     // Within range
    REQUIRE(clamp(0, 1, 10) == 1);     // Below range
    REQUIRE(clamp(15, 1, 10) == 10);   // Above range
}

TEST_CASE("min - two values (float)", "[math][LinearAlgebra][min]")
{
    REQUIRE(min(3.f, 5.f) == 3.f);
    REQUIRE(min(-1.f, 1.f) == -1.f);
    REQUIRE(min(0.f, 0.f) == 0.f);
}

TEST_CASE("min - three values (float)", "[math][LinearAlgebra][min]")
{
    REQUIRE(min(3.f, 5.f, 2.f) == 2.f);
    REQUIRE(min(-1.f, 1.f, -2.f) == -2.f);
    REQUIRE(min(0.f, 0.f, 0.f) == 0.f);
}

TEST_CASE("max - two values (float)", "[math][LinearAlgebra][max]")
{
    REQUIRE(max(3.f, 5.f) == 5.f);
    REQUIRE(max(-1.f, 1.f) == 1.f);
    REQUIRE(max(0.f, 0.f) == 0.f);
}

TEST_CASE("max - three values (float)", "[math][LinearAlgebra][max]")
{
    REQUIRE(max(3.f, 5.f, 2.f) == 5.f);
    REQUIRE(max(-1.f, 1.f, -2.f) == 1.f);
    REQUIRE(max(0.f, 0.f, 0.f) == 0.f);
}

TEST_CASE("clamp (float)", "[math][LinearAlgebra][clamp]")
{
    REQUIRE(clamp(5.f, 1.f, 10.f) == 5.f);     // Within range
    REQUIRE(clamp(0.f, 1.f, 10.f) == 1.f);     // Below range
    REQUIRE(clamp(15.f, 1.f, 10.f) == 10.f);   // Above range
}

}   // namespace gp::math::tests
