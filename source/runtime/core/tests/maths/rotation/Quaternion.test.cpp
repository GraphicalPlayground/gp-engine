// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the header for the Quaternion class
#include "maths/rotation/Quaternion.hpp"

namespace gp::math::tests
{

// Define a test fixture template
template <typename T>
class QuaternionTest : public ::testing::Test
{
protected:
    // Helper constants to ensure correct types during assertions
    const T zero = T{ 0 };
    const T one = T{ 1 };
};

// Specify the types you want to test (must satisfy concepts::IsFloatingPoint)
using FloatingPointTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(QuaternionTest, FloatingPointTypes);

TYPED_TEST(QuaternionTest, DefaultConstructor)
{
    Quaternion<TypeParam> quat;

    EXPECT_EQ(quat.x, this->zero);
    EXPECT_EQ(quat.y, this->zero);
    EXPECT_EQ(quat.z, this->zero);
    EXPECT_EQ(quat.w, this->one);
}

TYPED_TEST(QuaternionTest, ComponentConstructor)
{
    TypeParam x = TypeParam{ 1.0 };
    TypeParam y = TypeParam{ 2.0 };
    TypeParam z = TypeParam{ 3.0 };
    TypeParam w = TypeParam{ 4.0 };

    Quaternion<TypeParam> quat(x, y, z, w);

    EXPECT_EQ(quat.x, x);
    EXPECT_EQ(quat.y, y);
    EXPECT_EQ(quat.z, z);
    EXPECT_EQ(quat.w, w);
}

}   // namespace gp::math::tests
