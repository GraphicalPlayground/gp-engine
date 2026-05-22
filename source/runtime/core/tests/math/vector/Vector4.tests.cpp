// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the header for the Vector4 class
#include "maths/vector/Vector4.hpp"

namespace gp::math::testing
{

// Define a test fixture template
template <typename T>
class Vector4Test : public ::testing::Test
{
protected:
    // Helper constants to ensure correct types during assertions
    const T zero = T{ 0 };
    const T one = T{ 1 };
};

// Specify the types you want to test (must satisfy concepts::IsFloatingPoint)
using FloatingPointTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(Vector4Test, FloatingPointTypes);

TYPED_TEST(Vector4Test, DefaultConstructor)
{
    Vector4<TypeParam> vec;

    EXPECT_EQ(vec.x, this->zero);
    EXPECT_EQ(vec.y, this->zero);
    EXPECT_EQ(vec.z, this->zero);
    EXPECT_EQ(vec.w, this->one);
}

TYPED_TEST(Vector4Test, ComponentConstructor)
{
    TypeParam x = TypeParam{ 1.5 };
    TypeParam y = TypeParam{ 2.5 };
    TypeParam z = TypeParam{ 3.5 };
    TypeParam w = TypeParam{ 4.5 };

    Vector4<TypeParam> vec(x, y, z, w);

    EXPECT_EQ(vec.x, x);
    EXPECT_EQ(vec.y, y);
    EXPECT_EQ(vec.z, z);
    EXPECT_EQ(vec.w, w);
}

TYPED_TEST(Vector4Test, ScalarConstructor)
{
    TypeParam scalar = TypeParam{ 7.25 };

    Vector4<TypeParam> vec(scalar);

    EXPECT_EQ(vec.x, scalar);
    EXPECT_EQ(vec.y, scalar);
    EXPECT_EQ(vec.z, scalar);
    EXPECT_EQ(vec.w, scalar);
}

TYPED_TEST(Vector4Test, PointerConstructor)
{
    TypeParam arr[4] = { TypeParam{ 1.1 }, TypeParam{ 2.2 }, TypeParam{ 3.3 }, TypeParam{ 4.4 } };

    Vector4<TypeParam> vec(arr);

    EXPECT_EQ(vec.x, arr[0]);
    EXPECT_EQ(vec.y, arr[1]);
    EXPECT_EQ(vec.z, arr[2]);
    EXPECT_EQ(vec.w, arr[3]);
}

TYPED_TEST(Vector4Test, ConversionConstructor)
{
    // Create a vector of a specific type (float) to test conversion to TypeParam (float or double)
    Vector4<float> floatVec(1.0f, 2.0f, 3.0f, 4.0f);

    Vector4<TypeParam> vec(floatVec);

    EXPECT_EQ(vec.x, static_cast<TypeParam>(floatVec.x));
    EXPECT_EQ(vec.y, static_cast<TypeParam>(floatVec.y));
    EXPECT_EQ(vec.z, static_cast<TypeParam>(floatVec.z));
    EXPECT_EQ(vec.w, static_cast<TypeParam>(floatVec.w));
}

// TYPED_TEST(Vector4Test, Vector3Constructor)
// {
//     Vector3<TypeParam> vec3(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });

//     // Test with default W component
//     Vector4<TypeParam> vecDefaultW(vec3);
//     EXPECT_EQ(vecDefaultW.x, vec3.x);
//     EXPECT_EQ(vecDefaultW.y, vec3.y);
//     EXPECT_EQ(vecDefaultW.z, vec3.z);
//     EXPECT_EQ(vecDefaultW.w, this->one);   // Default is 1

//     // Test with explicit W component (using a different type to test the template <U>)
//     float customW = 5.5f;
//     Vector4<TypeParam> vecCustomW(vec3, customW);
//     EXPECT_EQ(vecCustomW.x, vec3.x);
//     EXPECT_EQ(vecCustomW.y, vec3.y);
//     EXPECT_EQ(vecCustomW.z, vec3.z);
//     EXPECT_EQ(vecCustomW.w, static_cast<TypeParam>(customW));
// }

// TYPED_TEST(Vector4Test, SingleVector2Constructor)
// {
//     Vector2<TypeParam> vec2(TypeParam{ 1.5 }, TypeParam{ 2.5 });

//     // Test with default Z and W
//     Vector4<TypeParam> vecDefault(vec2);
//     EXPECT_EQ(vecDefault.x, vec2.x);
//     EXPECT_EQ(vecDefault.y, vec2.y);
//     EXPECT_EQ(vecDefault.z, this->zero);   // Default is 0
//     EXPECT_EQ(vecDefault.w, this->one);    // Default is 1

//     // Test with explicit Z and W
//     double customZ = 3.5;
//     double customW = 4.5;
//     Vector4<TypeParam> vecCustom(vec2, customZ, customW);
//     EXPECT_EQ(vecCustom.x, vec2.x);
//     EXPECT_EQ(vecCustom.y, vec2.y);
//     EXPECT_EQ(vecCustom.z, static_cast<TypeParam>(customZ));
//     EXPECT_EQ(vecCustom.w, static_cast<TypeParam>(customW));
// }

// TYPED_TEST(Vector4Test, DoubleVector2Constructor)
// {
//     Vector2<TypeParam> vecXY(TypeParam{ 1.2 }, TypeParam{ 3.4 });
//     Vector2<TypeParam> vecZW(TypeParam{ 5.6 }, TypeParam{ 7.8 });

//     Vector4<TypeParam> vec(vecXY, vecZW);

//     EXPECT_EQ(vec.x, vecXY.x);
//     EXPECT_EQ(vec.y, vecXY.y);
//     EXPECT_EQ(vec.z, vecZW.x);
//     EXPECT_EQ(vec.w, vecZW.y);
// }

}   // namespace gp::math::testing
