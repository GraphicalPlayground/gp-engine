// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the header for the DualQuaternion class
#include "maths/rotation/DualQuaternion.hpp"

namespace gp::math::tests
{

// Define a test fixture template
template <typename T>
class DualQuaternionTest : public ::testing::Test
{
protected:
    // Helper constants to ensure correct types during assertions
    const T zero = T{ 0 };
    const T one = T{ 1 };
};

// Specify the types you want to test (must satisfy concepts::IsFloatingPoint)
using FloatingPointTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(DualQuaternionTest, FloatingPointTypes);

TYPED_TEST(DualQuaternionTest, DefaultConstructor)
{
    DualQuaternion<TypeParam> dq;

    EXPECT_EQ(dq.real.x, this->zero);
    EXPECT_EQ(dq.real.y, this->zero);
    EXPECT_EQ(dq.real.z, this->zero);
    EXPECT_EQ(dq.real.w, this->one);

    EXPECT_EQ(dq.dual.x, this->zero);
    EXPECT_EQ(dq.dual.y, this->zero);
    EXPECT_EQ(dq.dual.z, this->zero);
    EXPECT_EQ(dq.dual.w, this->zero);
}

TYPED_TEST(DualQuaternionTest, ComponentConstructor)
{
    TypeParam realX = TypeParam{ 1.0 };
    TypeParam realY = TypeParam{ 2.0 };
    TypeParam realZ = TypeParam{ 3.0 };
    TypeParam realW = TypeParam{ 4.0 };

    TypeParam dualX = TypeParam{ 5.0 };
    TypeParam dualY = TypeParam{ 6.0 };
    TypeParam dualZ = TypeParam{ 7.0 };
    TypeParam dualW = TypeParam{ 8.0 };

    DualQuaternion<TypeParam> dq(realX, realY, realZ, realW, dualX, dualY, dualZ, dualW);

    EXPECT_EQ(dq.real.x, realX);
    EXPECT_EQ(dq.real.y, realY);
    EXPECT_EQ(dq.real.z, realZ);
    EXPECT_EQ(dq.real.w, realW);

    EXPECT_EQ(dq.dual.x, dualX);
    EXPECT_EQ(dq.dual.y, dualY);
    EXPECT_EQ(dq.dual.z, dualZ);
    EXPECT_EQ(dq.dual.w, dualW);
}

}   // namespace gp::math::tests
