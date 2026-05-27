// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the header for the Vector classes
#include "maths/vector/Vector2.hpp"
#include "maths/vector/Vector3.hpp"
#include "maths/vector/Vector4.hpp"

namespace gp::math::testing
{

// Define a test fixture template
template <typename T>
class Vector2Test : public ::testing::Test
{
protected:
    // Helper constants to ensure correct types during assertions
    const T zero = T{ 0 };
    const T one = T{ 1 };
};

// Specify the types you want to test (must satisfy concepts::IsFloatingPoint)
using FloatingPointTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(Vector2Test, FloatingPointTypes);

TYPED_TEST(Vector2Test, DefaultConstructor)
{
    Vector2<TypeParam> vec;

    EXPECT_EQ(vec.x, this->zero);
    EXPECT_EQ(vec.y, this->zero);
}

TYPED_TEST(Vector2Test, ComponentConstructor)
{
    TypeParam x = TypeParam{ 1.5 };
    TypeParam y = TypeParam{ 2.5 };

    Vector2<TypeParam> vec(x, y);

    EXPECT_EQ(vec.x, x);
    EXPECT_EQ(vec.y, y);
}

TYPED_TEST(Vector2Test, ScalarConstructor)
{
    TypeParam scalar = TypeParam{ 7.25 };

    Vector2<TypeParam> vec(scalar);

    EXPECT_EQ(vec.x, scalar);
    EXPECT_EQ(vec.y, scalar);
}

TYPED_TEST(Vector2Test, PointerConstructor)
{
    TypeParam arr[2] = { TypeParam{ 1.1 }, TypeParam{ 2.2 } };

    Vector2<TypeParam> vec(arr);

    EXPECT_EQ(vec.x, arr[0]);
    EXPECT_EQ(vec.y, arr[1]);
}

TYPED_TEST(Vector2Test, ConversionConstructor)
{
    // Create a vector of a specific type (float) to test conversion to TypeParam (float or double)
    Vector2<float> floatVec(1.0f, 2.0f);

    Vector2<TypeParam> vec(floatVec);

    EXPECT_EQ(vec.x, static_cast<TypeParam>(floatVec.x));
    EXPECT_EQ(vec.y, static_cast<TypeParam>(floatVec.y));
}

TYPED_TEST(Vector2Test, Vector3Constructor)
{
    Vector3<TypeParam> vec3(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });

    Vector2<TypeParam> vec(vec3);
    EXPECT_EQ(vec.x, vec3.x);
    EXPECT_EQ(vec.y, vec3.y);
}

TYPED_TEST(Vector2Test, Vector4Constructor)
{
    Vector4<TypeParam> vec4(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });

    Vector2<TypeParam> vec(vec4);
    EXPECT_EQ(vec.x, vec4.x);
    EXPECT_EQ(vec.y, vec4.y);
}

}   // namespace gp::math::testing
