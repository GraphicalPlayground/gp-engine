// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the header for the Rotator2D class
#include "maths/rotation/Rotator2D.hpp"

namespace gp::math::tests
{

// Define a test fixture template
template <typename T>
class Rotator2DTest : public ::testing::Test
{
protected:
    // Helper constants to ensure correct types during assertions
    const T zero = T{ 0 };
    const T one = T{ 1 };
};

// Specify the types you want to test (must satisfy concepts::IsFloatingPoint)
using FloatingPointTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(Rotator2DTest, FloatingPointTypes);

TYPED_TEST(Rotator2DTest, DefaultConstructor)
{
    Rotator2D<TypeParam> rot;

    EXPECT_EQ(rot.angle, this->zero);
}

TYPED_TEST(Rotator2DTest, AngleConstructor)
{
    TypeParam angle = TypeParam{ 1.5 };

    Rotator2D<TypeParam> rot(angle);

    EXPECT_EQ(rot.angle, angle);
}

}   // namespace gp::math::tests
