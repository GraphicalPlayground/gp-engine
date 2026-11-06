// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the header for the Rotator class
#include "maths/rotation/Rotator.hpp"

namespace gp::math::tests
{

// Define a test fixture template
template <typename T>
class RotatorTest : public ::testing::Test
{
protected:
    // Helper constants to ensure correct types during assertions
    const T zero = T{ 0 };
    const T one = T{ 1 };
};

// Specify the types you want to test (must satisfy concepts::IsFloatingPoint)
using FloatingPointTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(RotatorTest, FloatingPointTypes);

TYPED_TEST(RotatorTest, DefaultConstructor)
{
    Rotator<TypeParam> rot;

    EXPECT_EQ(rot.pitch, this->zero);
    EXPECT_EQ(rot.yaw, this->zero);
    EXPECT_EQ(rot.roll, this->zero);
}

TYPED_TEST(RotatorTest, ComponentConstructor)
{
    TypeParam pitch = TypeParam{ 1.0 };
    TypeParam yaw = TypeParam{ 2.0 };
    TypeParam roll = TypeParam{ 3.0 };

    Rotator<TypeParam> rot(pitch, yaw, roll);

    EXPECT_EQ(rot.pitch, pitch);
    EXPECT_EQ(rot.yaw, yaw);
    EXPECT_EQ(rot.roll, roll);
}

}   // namespace gp::math::tests