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
    const T minusOne = T{ -1 };
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

TYPED_TEST(Vector3Test, Constants)
{
    Vector3<TypeParam> zeroVec = Vector3<TypeParam>::zero();
    EXPECT_EQ(zeroVec.x, this->zero);
    EXPECT_EQ(zeroVec.y, this->zero);
    EXPECT_EQ(zeroVec.z, this->zero);

    Vector3<TypeParam> oneVec = Vector3<TypeParam>::one();
    EXPECT_EQ(oneVec.x, this->one);
    EXPECT_EQ(oneVec.y, this->one);
    EXPECT_EQ(oneVec.z, this->one);

    Vector3<TypeParam> unitX = Vector3<TypeParam>::unitX();
    EXPECT_EQ(unitX.x, this->one);
    EXPECT_EQ(unitX.y, this->zero);
    EXPECT_EQ(unitX.z, this->zero);

    Vector3<TypeParam> unitY = Vector3<TypeParam>::unitY();
    EXPECT_EQ(unitY.x, this->zero);
    EXPECT_EQ(unitY.y, this->one);
    EXPECT_EQ(unitY.z, this->zero);

    Vector3<TypeParam> unitZ = Vector3<TypeParam>::unitZ();
    EXPECT_EQ(unitZ.x, this->zero);
    EXPECT_EQ(unitZ.y, this->zero);
    EXPECT_EQ(unitZ.z, this->one);

    Vector3<TypeParam> forward = Vector3<TypeParam>::forward();
    EXPECT_EQ(forward.x, this->zero);
    EXPECT_EQ(forward.y, this->zero);
    EXPECT_EQ(forward.z, this->one);

    Vector3<TypeParam> backward = Vector3<TypeParam>::backward();
    EXPECT_EQ(backward.x, this->zero);
    EXPECT_EQ(backward.y, this->zero);
    EXPECT_EQ(backward.z, this->minusOne);

    Vector3<TypeParam> left = Vector3<TypeParam>::left();
    EXPECT_EQ(left.x, this->minusOne);
    EXPECT_EQ(left.y, this->zero);
    EXPECT_EQ(left.z, this->zero);

    Vector3<TypeParam> right = Vector3<TypeParam>::right();
    EXPECT_EQ(right.x, this->one);
    EXPECT_EQ(right.y, this->zero);
    EXPECT_EQ(right.z, this->zero);

    Vector3<TypeParam> up = Vector3<TypeParam>::up();
    EXPECT_EQ(up.x, this->zero);
    EXPECT_EQ(up.y, this->one);
    EXPECT_EQ(up.z, this->zero);

    Vector3<TypeParam> down = Vector3<TypeParam>::down();
    EXPECT_EQ(down.x, this->zero);
    EXPECT_EQ(down.y, this->minusOne);
    EXPECT_EQ(down.z, this->zero);
}

TYPED_TEST(Vector3Test, CrossProductOperator)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> vecB(TypeParam{ 0.0 }, TypeParam{ 1.0 }, TypeParam{ 0.0 });

    Vector3<TypeParam> cross = vecA ^ vecB;

    EXPECT_EQ(cross.x, this->zero);
    EXPECT_EQ(cross.y, this->zero);
    EXPECT_EQ(cross.z, this->one);
}

TYPED_TEST(Vector3Test, DotProductOperator)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });

    TypeParam dot = vecA | vecB;

    EXPECT_EQ(dot, TypeParam{ 32.0 });   // 1*4 + 2*5 + 3*6 = 32
}

TYPED_TEST(Vector3Test, NegationOperator)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ -2.0 }, TypeParam{ 3.0 });

    Vector3<TypeParam> negated = -vec;

    EXPECT_EQ(negated.x, -vec.x);
    EXPECT_EQ(negated.y, -vec.y);
    EXPECT_EQ(negated.z, -vec.z);
}

TYPED_TEST(Vector3Test, UnaryPlusOperator)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ -2.0 }, TypeParam{ 3.0 });

    Vector3<TypeParam> result = +vec;

    EXPECT_EQ(result.x, vec.x);
    EXPECT_EQ(result.y, vec.y);
    EXPECT_EQ(result.z, vec.z);
}

TYPED_TEST(Vector3Test, EqualityOperator)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecC(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });

    EXPECT_TRUE(vecA == vecB);
    EXPECT_FALSE(vecA == vecC);
}

TYPED_TEST(Vector3Test, InequalityOperator)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecC(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });

    EXPECT_FALSE(vecA != vecB);
    EXPECT_TRUE(vecA != vecC);
}

TYPED_TEST(Vector3Test, SubscriptOperator)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });

    EXPECT_EQ(vec[0], vec.x);
    EXPECT_EQ(vec[1], vec.y);
    EXPECT_EQ(vec[2], vec.z);

    const Vector3<TypeParam> constVec(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });
    EXPECT_EQ(constVec[0], constVec.x);
    EXPECT_EQ(constVec[1], constVec.y);
    EXPECT_EQ(constVec[2], constVec.z);

    Vector3<TypeParam> modifiableVec(TypeParam{ 7.0 }, TypeParam{ 8.0 }, TypeParam{ 9.0 });
    modifiableVec[0] = TypeParam{ 10.0 };
    modifiableVec[1] = TypeParam{ 11.0 };
    modifiableVec[2] = TypeParam{ 12.0 };
    EXPECT_EQ(modifiableVec.x, TypeParam{ 10.0 });
    EXPECT_EQ(modifiableVec.y, TypeParam{ 11.0 });
    EXPECT_EQ(modifiableVec.z, TypeParam{ 12.0 });
}

TYPED_TEST(Vector3Test, VectorAdditionOperator)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });

    Vector3<TypeParam> sum = vecA + vecB;

    EXPECT_NEAR(sum.x, vecA.x + vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(sum.y, vecA.y + vecB.y, TypeParam{ 1e-6 });
    EXPECT_NEAR(sum.z, vecA.z + vecB.z, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, ScalarAdditionOperator)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    TypeParam bias = TypeParam{ 5.0 };

    Vector3<TypeParam> result = vec + bias;

    EXPECT_NEAR(result.x, vec.x + bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y + bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.z, vec.z + bias, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, VectorSubtractionOperator)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });

    Vector3<TypeParam> difference = vecA - vecB;

    EXPECT_NEAR(difference.x, vecA.x - vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(difference.y, vecA.y - vecB.y, TypeParam{ 1e-6 });
    EXPECT_NEAR(difference.z, vecA.z - vecB.z, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, ScalarSubtractionOperator)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    TypeParam bias = TypeParam{ 5.0 };

    Vector3<TypeParam> result = vec - bias;

    EXPECT_NEAR(result.x, vec.x - bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y - bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.z, vec.z - bias, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, VectorMultiplicationOperator)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });

    Vector3<TypeParam> product = vecA * vecB;

    EXPECT_NEAR(product.x, vecA.x * vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(product.y, vecA.y * vecB.y, TypeParam{ 1e-6 });
    EXPECT_NEAR(product.z, vecA.z * vecB.z, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, ScalarMultiplicationOperator)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    Vector3<TypeParam> result = vec * scalar;

    EXPECT_NEAR(result.x, vec.x * scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y * scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.z, vec.z * scalar, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, VectorDivisionOperator)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });

    Vector3<TypeParam> quotient = vecA / vecB;

    EXPECT_NEAR(quotient.x, vecA.x / vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(quotient.y, vecA.y / vecB.y, TypeParam{ 1e-6 });
    EXPECT_NEAR(quotient.z, vecA.z / vecB.z, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, ScalarDivisionOperator)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    Vector3<TypeParam> result = vec / scalar;

    EXPECT_NEAR(result.x, vec.x / scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y / scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.z, vec.z / scalar, TypeParam{ 1e-6 });
}

}   // namespace gp::math::tests
