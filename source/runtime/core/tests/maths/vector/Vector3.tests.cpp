// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the header for the Vector classes
#include "maths/vector/Vector2.hpp"
#include "maths/vector/Vector3.hpp"
#include "maths/vector/Vector4.hpp"

namespace gp::math::tests
{

// Define a test fixture template
template <typename T>
class Vector3Test : public ::testing::Test
{
protected:
    // Helper constants to ensure correct types during assertions
    const T zero = T{ 0 };
    const T one = T{ 1 };
};

// Specify the types you want to test (must satisfy concepts::IsFloatingPoint)
using FloatingPointTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(Vector3Test, FloatingPointTypes);

TYPED_TEST(Vector3Test, DefaultConstructor)
{
    Vector3<TypeParam> vec;

    EXPECT_EQ(vec.x, this->zero);
    EXPECT_EQ(vec.y, this->zero);
    EXPECT_EQ(vec.z, this->zero);
}

TYPED_TEST(Vector3Test, ComponentConstructor)
{
    TypeParam x = TypeParam{ 1.5 };
    TypeParam y = TypeParam{ 2.5 };
    TypeParam z = TypeParam{ 3.5 };

    Vector3<TypeParam> vec(x, y, z);

    EXPECT_EQ(vec.x, x);
    EXPECT_EQ(vec.y, y);
    EXPECT_EQ(vec.z, z);
}

TYPED_TEST(Vector3Test, ScalarConstructor)
{
    TypeParam scalar = TypeParam{ 7.25 };

    Vector3<TypeParam> vec(scalar);

    EXPECT_EQ(vec.x, scalar);
    EXPECT_EQ(vec.y, scalar);
    EXPECT_EQ(vec.z, scalar);
}

TYPED_TEST(Vector3Test, PointerConstructor)
{
    TypeParam arr[3] = { TypeParam{ 1.1 }, TypeParam{ 2.2 }, TypeParam{ 3.3 } };

    Vector3<TypeParam> vec(arr);

    EXPECT_EQ(vec.x, arr[0]);
    EXPECT_EQ(vec.y, arr[1]);
    EXPECT_EQ(vec.z, arr[2]);
}

TYPED_TEST(Vector3Test, ConversionConstructor)
{
    // Create a vector of a specific type (float) to test conversion to TypeParam (float or double)
    Vector3<float> floatVec(1.0f, 2.0f, 3.0f);

    Vector3<TypeParam> vec(floatVec);

    EXPECT_EQ(vec.x, static_cast<TypeParam>(floatVec.x));
    EXPECT_EQ(vec.y, static_cast<TypeParam>(floatVec.y));
    EXPECT_EQ(vec.z, static_cast<TypeParam>(floatVec.z));
}

TYPED_TEST(Vector3Test, SingleVector2Constructor)
{
    Vector2<TypeParam> vec2(TypeParam{ 1.5 }, TypeParam{ 2.5 });

    // Test with default Z
    Vector3<TypeParam> vecDefault(vec2);
    EXPECT_EQ(vecDefault.x, vec2.x);
    EXPECT_EQ(vecDefault.y, vec2.y);
    EXPECT_EQ(vecDefault.z, this->zero);   // Default is 0

    // Test with explicit Z
    double customZ = 3.5;
    Vector3<TypeParam> vecCustom(vec2, customZ);
    EXPECT_EQ(vecCustom.x, vec2.x);
    EXPECT_EQ(vecCustom.y, vec2.y);
    EXPECT_EQ(vecCustom.z, static_cast<TypeParam>(customZ));
}

TYPED_TEST(Vector3Test, Vector4Constructor)
{
    Vector4<TypeParam> vec4(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });

    Vector3<TypeParam> vecDefaultW(vec4);
    EXPECT_EQ(vecDefaultW.x, vec4.x);
    EXPECT_EQ(vecDefaultW.y, vec4.y);
    EXPECT_EQ(vecDefaultW.z, vec4.z);
}

}   // namespace gp::math::tests
