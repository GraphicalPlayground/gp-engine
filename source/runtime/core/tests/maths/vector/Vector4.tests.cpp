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

TYPED_TEST(Vector4Test, Vector3Constructor)
{
    Vector3<TypeParam> vec3(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });

    // Test with default W component
    Vector4<TypeParam> vecDefaultW(vec3);
    EXPECT_EQ(vecDefaultW.x, vec3.x);
    EXPECT_EQ(vecDefaultW.y, vec3.y);
    EXPECT_EQ(vecDefaultW.z, vec3.z);
    EXPECT_EQ(vecDefaultW.w, this->one);   // Default is 1

    // Test with explicit W component (using a different type to test the template <U>)
    float customW = 5.5f;
    Vector4<TypeParam> vecCustomW(vec3, customW);
    EXPECT_EQ(vecCustomW.x, vec3.x);
    EXPECT_EQ(vecCustomW.y, vec3.y);
    EXPECT_EQ(vecCustomW.z, vec3.z);
    EXPECT_EQ(vecCustomW.w, static_cast<TypeParam>(customW));
}

TYPED_TEST(Vector4Test, SingleVector2Constructor)
{
    Vector2<TypeParam> vec2(TypeParam{ 1.5 }, TypeParam{ 2.5 });

    // Test with default Z and W
    Vector4<TypeParam> vecDefault(vec2);
    EXPECT_EQ(vecDefault.x, vec2.x);
    EXPECT_EQ(vecDefault.y, vec2.y);
    EXPECT_EQ(vecDefault.z, this->zero);   // Default is 0
    EXPECT_EQ(vecDefault.w, this->one);    // Default is 1

    // Test with explicit Z and W
    double customZ = 3.5;
    double customW = 4.5;
    Vector4<TypeParam> vecCustom(vec2, customZ, customW);
    EXPECT_EQ(vecCustom.x, vec2.x);
    EXPECT_EQ(vecCustom.y, vec2.y);
    EXPECT_EQ(vecCustom.z, static_cast<TypeParam>(customZ));
    EXPECT_EQ(vecCustom.w, static_cast<TypeParam>(customW));
}

TYPED_TEST(Vector4Test, DoubleVector2Constructor)
{
    Vector2<TypeParam> vecXY(TypeParam{ 1.2 }, TypeParam{ 3.4 });
    Vector2<TypeParam> vecZW(TypeParam{ 5.6 }, TypeParam{ 7.8 });

    Vector4<TypeParam> vec(vecXY, vecZW);

    EXPECT_EQ(vec.x, vecXY.x);
    EXPECT_EQ(vec.y, vecXY.y);
    EXPECT_EQ(vec.z, vecZW.x);
    EXPECT_EQ(vec.w, vecZW.y);
}

TYPED_TEST(Vector4Test, Constants)
{
    EXPECT_EQ(Vector4<TypeParam>::zero(), (Vector4<TypeParam>(0, 0, 0, 0)));
    EXPECT_EQ(Vector4<TypeParam>::one(), (Vector4<TypeParam>(1, 1, 1, 1)));
    EXPECT_EQ(Vector4<TypeParam>::up(), (Vector4<TypeParam>(0, 1, 0, 0)));
    EXPECT_EQ(Vector4<TypeParam>::down(), (Vector4<TypeParam>(0, -1, 0, 0)));
    EXPECT_EQ(Vector4<TypeParam>::left(), (Vector4<TypeParam>(-1, 0, 0, 0)));
    EXPECT_EQ(Vector4<TypeParam>::right(), (Vector4<TypeParam>(1, 0, 0, 0)));
    EXPECT_EQ(Vector4<TypeParam>::forward(), (Vector4<TypeParam>(0, 0, 1, 0)));
    EXPECT_EQ(Vector4<TypeParam>::backward(), (Vector4<TypeParam>(0, 0, -1, 0)));
    EXPECT_EQ(Vector4<TypeParam>::negativeW(), (Vector4<TypeParam>(0, 0, 0, -1)));
    EXPECT_EQ(Vector4<TypeParam>::positiveW(), (Vector4<TypeParam>(0, 0, 0, 1)));
    EXPECT_EQ(Vector4<TypeParam>::unitX(), Vector4<TypeParam>::right());
    EXPECT_EQ(Vector4<TypeParam>::unitY(), Vector4<TypeParam>::up());
    EXPECT_EQ(Vector4<TypeParam>::unitZ(), Vector4<TypeParam>::forward());
    EXPECT_EQ(Vector4<TypeParam>::unitW(), Vector4<TypeParam>::positiveW());
}

TYPED_TEST(Vector4Test, CrossProductOperator)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });
    Vector4<TypeParam> vecB(TypeParam{ 0.0 }, TypeParam{ 1.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });

    Vector4<TypeParam> cross = vecA ^ vecB;

    EXPECT_EQ(cross.x, this->zero);
    EXPECT_EQ(cross.y, this->zero);
    EXPECT_EQ(cross.z, this->one);
    EXPECT_EQ(cross.w, this->zero);
}

TYPED_TEST(Vector4Test, DotProductOperator)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecB(TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 }, TypeParam{ 8.0 });

    TypeParam dot = vecA | vecB;

    EXPECT_EQ(dot, TypeParam{ 70.0 });
}

TYPED_TEST(Vector4Test, NegationOperator)
{
    Vector4<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ -2.0 }, TypeParam{ 3.0 }, TypeParam{ -4.0 });

    Vector4<TypeParam> negated = -vec;

    EXPECT_EQ(negated.x, -vec.x);
    EXPECT_EQ(negated.y, -vec.y);
    EXPECT_EQ(negated.z, -vec.z);
    EXPECT_EQ(negated.w, -vec.w);
}

TYPED_TEST(Vector4Test, UnaryPlusOperator)
{
    Vector4<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ -2.0 }, TypeParam{ 3.0 }, TypeParam{ -4.0 });

    Vector4<TypeParam> result = +vec;

    EXPECT_EQ(result.x, vec.x);
    EXPECT_EQ(result.y, vec.y);
    EXPECT_EQ(result.z, vec.z);
    EXPECT_EQ(result.w, vec.w);
}

TYPED_TEST(Vector4Test, EqualityOperator)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecB(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecC(TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 }, TypeParam{ 8.0 });

    EXPECT_TRUE(vecA == vecB);
    EXPECT_FALSE(vecA == vecC);
}

TYPED_TEST(Vector4Test, InequalityOperator)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecB(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecC(TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 }, TypeParam{ 8.0 });

    EXPECT_FALSE(vecA != vecB);
    EXPECT_TRUE(vecA != vecC);
}

TYPED_TEST(Vector4Test, SubscriptOperator)
{
    Vector4<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });

    EXPECT_EQ(vec[0], vec.x);
    EXPECT_EQ(vec[1], vec.y);
    EXPECT_EQ(vec[2], vec.z);
    EXPECT_EQ(vec[3], vec.w);

    const Vector4<TypeParam> constVec(TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 }, TypeParam{ 8.0 });
    EXPECT_EQ(constVec[0], constVec.x);
    EXPECT_EQ(constVec[1], constVec.y);
    EXPECT_EQ(constVec[2], constVec.z);
    EXPECT_EQ(constVec[3], constVec.w);

    Vector4<TypeParam> modifiableVec(TypeParam{ 9.0 }, TypeParam{ 10.0 }, TypeParam{ 11.0 }, TypeParam{ 12.0 });
    modifiableVec[0] = TypeParam{ 13.0 };
    modifiableVec[1] = TypeParam{ 14.0 };
    modifiableVec[2] = TypeParam{ 15.0 };
    modifiableVec[3] = TypeParam{ 16.0 };
    EXPECT_EQ(modifiableVec.x, TypeParam{ 13.0 });
    EXPECT_EQ(modifiableVec.y, TypeParam{ 14.0 });
    EXPECT_EQ(modifiableVec.z, TypeParam{ 15.0 });
    EXPECT_EQ(modifiableVec.w, TypeParam{ 16.0 });
}

TYPED_TEST(Vector4Test, VectorAdditionOperator)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecB(TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 }, TypeParam{ 8.0 });

    Vector4<TypeParam> sum = vecA + vecB;

    EXPECT_NEAR(sum.x, vecA.x + vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(sum.y, vecA.y + vecB.y, TypeParam{ 1e-6 });
    EXPECT_NEAR(sum.z, vecA.z + vecB.z, TypeParam{ 1e-6 });
    EXPECT_NEAR(sum.w, vecA.w + vecB.w, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, ScalarAdditionOperator)
{
    Vector4<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    TypeParam bias = TypeParam{ 5.0 };

    Vector4<TypeParam> result = vec + bias;

    EXPECT_NEAR(result.x, vec.x + bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y + bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.z, vec.z + bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.w, vec.w + bias, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, ScalarAdditionOperatorCommutative)
{
    Vector4<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    TypeParam bias = TypeParam{ 5.0 };

    Vector4<TypeParam> result = bias + vec;

    EXPECT_NEAR(result.x, vec.x + bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y + bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.z, vec.z + bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.w, vec.w + bias, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, VectorSubtractionOperator)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecB(TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 }, TypeParam{ 8.0 });

    Vector4<TypeParam> difference = vecA - vecB;

    EXPECT_NEAR(difference.x, vecA.x - vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(difference.y, vecA.y - vecB.y, TypeParam{ 1e-6 });
    EXPECT_NEAR(difference.z, vecA.z - vecB.z, TypeParam{ 1e-6 });
    EXPECT_NEAR(difference.w, vecA.w - vecB.w, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, ScalarSubtractionOperator)
{
    Vector4<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    TypeParam bias = TypeParam{ 5.0 };

    Vector4<TypeParam> result = vec - bias;

    EXPECT_NEAR(result.x, vec.x - bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y - bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.z, vec.z - bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.w, vec.w - bias, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, VectorMultiplicationOperator)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecB(TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 }, TypeParam{ 8.0 });

    Vector4<TypeParam> product = vecA * vecB;

    EXPECT_NEAR(product.x, vecA.x * vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(product.y, vecA.y * vecB.y, TypeParam{ 1e-6 });
    EXPECT_NEAR(product.z, vecA.z * vecB.z, TypeParam{ 1e-6 });
    EXPECT_NEAR(product.w, vecA.w * vecB.w, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, ScalarMultiplicationOperator)
{
    Vector4<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    Vector4<TypeParam> result = vec * scalar;

    EXPECT_NEAR(result.x, vec.x * scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y * scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.z, vec.z * scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.w, vec.w * scalar, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, ScalarMultiplicationOperatorCommutative)
{
    Vector4<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    Vector4<TypeParam> result = scalar * vec;

    EXPECT_NEAR(result.x, vec.x * scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y * scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.z, vec.z * scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.w, vec.w * scalar, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, VectorDivisionOperator)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecB(TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 }, TypeParam{ 8.0 });

    Vector4<TypeParam> quotient = vecA / vecB;

    EXPECT_NEAR(quotient.x, vecA.x / vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(quotient.y, vecA.y / vecB.y, TypeParam{ 1e-6 });
    EXPECT_NEAR(quotient.z, vecA.z / vecB.z, TypeParam{ 1e-6 });
    EXPECT_NEAR(quotient.w, vecA.w / vecB.w, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, ScalarDivisionOperator)
{
    Vector4<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    Vector4<TypeParam> result = vec / scalar;

    EXPECT_NEAR(result.x, vec.x / scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y / scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.z, vec.z / scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.w, vec.w / scalar, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, CompoundAssignmentOperators)
{
    Vector4<TypeParam> value(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    const Vector4<TypeParam> other(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    value += other;
    EXPECT_NEAR(value.x, TypeParam{ 5.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.y, TypeParam{ 7.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.z, TypeParam{ 9.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.w, TypeParam{ 11.0 }, TypeParam{ 1e-6 });

    value -= other;
    EXPECT_NEAR(value.x, TypeParam{ 1.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.y, TypeParam{ 2.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.z, TypeParam{ 3.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.w, TypeParam{ 4.0 }, TypeParam{ 1e-6 });

    value *= scalar;
    EXPECT_NEAR(value.x, TypeParam{ 5.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.y, TypeParam{ 10.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.z, TypeParam{ 15.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.w, TypeParam{ 20.0 }, TypeParam{ 1e-6 });

    value /= scalar;
    EXPECT_NEAR(value.x, TypeParam{ 1.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.y, TypeParam{ 2.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.z, TypeParam{ 3.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.w, TypeParam{ 4.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, ComponentMethod)
{
    Vector4<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    EXPECT_EQ(vec.component(0), TypeParam{ 1.0 });
    EXPECT_EQ(vec.component(1), TypeParam{ 2.0 });
    EXPECT_EQ(vec.component(2), TypeParam{ 3.0 });
    EXPECT_EQ(vec.component(3), TypeParam{ 4.0 });

    const Vector4<TypeParam> constVec(TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 }, TypeParam{ 8.0 });
    EXPECT_EQ(constVec.component(0), TypeParam{ 5.0 });
    EXPECT_EQ(constVec.component(1), TypeParam{ 6.0 });
    EXPECT_EQ(constVec.component(2), TypeParam{ 7.0 });
    EXPECT_EQ(constVec.component(3), TypeParam{ 8.0 });
}

TYPED_TEST(Vector4Test, CrossMethod)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });
    Vector4<TypeParam> vecB(TypeParam{ 0.0 }, TypeParam{ 1.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });
    Vector4<TypeParam> cross = vecA.cross(vecB);

    EXPECT_EQ(cross.x, this->zero);
    EXPECT_EQ(cross.y, this->zero);
    EXPECT_EQ(cross.z, this->one);
    EXPECT_EQ(cross.w, this->zero);
}

TYPED_TEST(Vector4Test, DotMethod)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecB(TypeParam{ 5.0 }, TypeParam{ 6.0 }, TypeParam{ 7.0 }, TypeParam{ 8.0 });

    EXPECT_EQ(vecA.dot(vecB), TypeParam{ 70.0 });
}

TYPED_TEST(Vector4Test, EqualsMethod)
{
    Vector4<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecB(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector4<TypeParam> vecC(TypeParam{ 1.1 }, TypeParam{ 2.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });

    EXPECT_TRUE(vecA.equals(vecB));
    EXPECT_FALSE(vecA.equals(vecC));
    EXPECT_TRUE(vecA.equals(vecC, TypeParam{ 0.2 }));
}

TYPED_TEST(Vector4Test, IsAllComponentsEqualMethod)
{
    Vector4<TypeParam> vecA(TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.0 });
    Vector4<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.1 });

    EXPECT_TRUE(vecA.isAllComponentsEqual());
    EXPECT_FALSE(vecB.isAllComponentsEqual());
    EXPECT_TRUE(vecB.isAllComponentsEqual(TypeParam{ 0.2 }));
}

TYPED_TEST(Vector4Test, IsNearlyZeroMethod)
{
    Vector4<TypeParam> vecA = Vector4<TypeParam>::zero();
    Vector4<TypeParam> vecB(TypeParam{ 0.00001 }, TypeParam{ 0.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });

    EXPECT_TRUE(vecA.isNearlyZero());
    EXPECT_FALSE(vecB.isNearlyZero(TypeParam{ 1e-6 }));
    EXPECT_TRUE(vecB.isNearlyZero(TypeParam{ 1e-4 }));
}

TYPED_TEST(Vector4Test, IsZeroMethod)
{
    Vector4<TypeParam> vecA = Vector4<TypeParam>::zero();
    Vector4<TypeParam> vecB(TypeParam{ 0.00001 }, TypeParam{ 0.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });

    EXPECT_TRUE(vecA.isZero());
    EXPECT_FALSE(vecB.isZero());
}

TYPED_TEST(Vector4Test, IsUnitMethod)
{
    Vector4<TypeParam> vecA = Vector4<TypeParam>::unitX();
    Vector4<TypeParam> vecB(TypeParam{ 0.4 }, TypeParam{ 0.4 }, TypeParam{ 0.4 }, TypeParam{ 0.4 });

    EXPECT_TRUE(vecA.isUnit());
    EXPECT_FALSE(vecB.isUnit());
}

TYPED_TEST(Vector4Test, IsNormalizedMethod)
{
    EXPECT_TRUE(Vector4<TypeParam>::unitX().isNormalized());
}

TYPED_TEST(Vector4Test, IsUniformMethod)
{
    Vector4<TypeParam> vecA(TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.0 });
    Vector4<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.1 });

    EXPECT_TRUE(vecA.isUniform());
    EXPECT_FALSE(vecB.isUniform());
}

TYPED_TEST(Vector4Test, GetMaxMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(-2, 4, -6, 8).getMax(), TypeParam{ 8 });
}

TYPED_TEST(Vector4Test, GetMinMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(-2, 4, -6, 8).getMin(), TypeParam{ -6 });
}

TYPED_TEST(Vector4Test, GetAbsMaxMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(-2, 4, -6, 8).getAbsMax(), TypeParam{ 8 });
}

TYPED_TEST(Vector4Test, GetAbsMinMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(-2, 4, -6, 8).getAbsMin(), TypeParam{ 2 });
}

TYPED_TEST(Vector4Test, GetComponentWiseMinMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(-2, 4, -6, 8).getComponentWiseMin(Vector4<TypeParam>(-3, 5, -5, 7)),
              (Vector4<TypeParam>(-3, 4, -6, 7)));
}

TYPED_TEST(Vector4Test, GetComponentWiseMaxMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(-2, 4, -6, 8).getComponentWiseMax(Vector4<TypeParam>(-3, 5, -5, 7)),
              (Vector4<TypeParam>(-2, 5, -5, 8)));
}

TYPED_TEST(Vector4Test, GetComponentWiseAbsMinMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(-2, 4, -6, 8).getComponentWiseAbsMin(Vector4<TypeParam>(3, 5, -5, 7)),
              (Vector4<TypeParam>(2, 4, 5, 7)));
}

TYPED_TEST(Vector4Test, GetComponentWiseAbsMaxMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(-2, 4, -6, 8).getComponentWiseAbsMax(Vector4<TypeParam>(3, 5, -5, 7)),
              (Vector4<TypeParam>(3, 5, 6, 8)));
}

TYPED_TEST(Vector4Test, GetAbsMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(-2, 4, -6, 8).getAbs(), (Vector4<TypeParam>(2, 4, 6, 8)));
}

TYPED_TEST(Vector4Test, LengthMethod)
{
    EXPECT_NEAR(Vector4<TypeParam>(0, 0, 3, 4).length(), TypeParam{ 5 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, LengthSquaredMethod)
{
    EXPECT_NEAR(Vector4<TypeParam>(0, 0, 3, 4).lengthSquared(), TypeParam{ 25 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, NormalizeMethod)
{
    Vector4<TypeParam> value(0, 0, 3, 4);
    EXPECT_TRUE(value.normalize());
    EXPECT_NEAR(value.z, TypeParam{ 0.6 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.w, TypeParam{ 0.8 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, GetNormalizedUnsafeMethod)
{
    EXPECT_NEAR(Vector4<TypeParam>(0, 0, 3, 4).getNormalizedUnsafe().w, TypeParam{ 0.8 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector4Test, GetSafeNormalMethod)
{
    EXPECT_EQ(Vector4<TypeParam>::zero().getSafeNormal(), Vector4<TypeParam>::zero());
}

TYPED_TEST(Vector4Test, GetSignMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(-2, 0, 3, 0).getSign(), (Vector4<TypeParam>(-1, 0, 1, 0)));
}

TYPED_TEST(Vector4Test, GetProjectedToXYMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(2, 4, 5, 10).getProjectedToXY(), (Vector4<TypeParam>(0.4, 0.8, 1, 10)));
}

TYPED_TEST(Vector4Test, GetReciprocalMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(2, 4, 5, 10).getReciprocal(), (Vector4<TypeParam>(0.5, 0.25, 0.2, 0.1)));
}

TYPED_TEST(Vector4Test, ProjectOnToNormalMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(2, 4, 0, 1).projectOnToNormal(Vector4<TypeParam>::unitX()),
              (Vector4<TypeParam>(2, 0, 0, 0)));
}

TYPED_TEST(Vector4Test, MirrorByMethod)
{
    EXPECT_EQ(Vector4<TypeParam>(1, -1, 0, 0).mirrorBy(Vector4<TypeParam>::unitY()),
              (Vector4<TypeParam>(1, 1, 0, 0)));
}

TYPED_TEST(Vector4Test, StaticCrossMethod)
{
    const Vector4<TypeParam> a(1, 5, 3, 7);
    const Vector4<TypeParam> b(2, 4, 6, 8);
    EXPECT_EQ(Vector4<TypeParam>::cross(a, b), (a ^ b));
}

TYPED_TEST(Vector4Test, StaticDotMethod)
{
    EXPECT_EQ(Vector4<TypeParam>::dot(Vector4<TypeParam>(1, 5, 3, 7), Vector4<TypeParam>(2, 4, 6, 8)),
              TypeParam{ 96 });
}

TYPED_TEST(Vector4Test, StaticComponentWiseMinMethod)
{
    EXPECT_EQ(Vector4<TypeParam>::getComponentWiseMin(Vector4<TypeParam>(1, 5, 3, 7),
                                                       Vector4<TypeParam>(2, 4, 6, 8)),
              (Vector4<TypeParam>(1, 4, 3, 7)));
}

TYPED_TEST(Vector4Test, StaticComponentWiseMaxMethod)
{
    EXPECT_EQ(Vector4<TypeParam>::getComponentWiseMax(Vector4<TypeParam>(1, 5, 3, 7),
                                                       Vector4<TypeParam>(2, 4, 6, 8)),
              (Vector4<TypeParam>(2, 5, 6, 8)));
}

TYPED_TEST(Vector4Test, StaticClampMethod)
{
    EXPECT_EQ(Vector4<TypeParam>::clamp(Vector4<TypeParam>(-1, 5, 10, 4), Vector4<TypeParam>::zero(),
                                        Vector4<TypeParam>(2, 4, 6, 8)),
              (Vector4<TypeParam>(0, 4, 6, 4)));
}

TYPED_TEST(Vector4Test, GlobalMin2Function)
{
    EXPECT_EQ(gp::math::min(Vector4<TypeParam>(1, 5, 3, 7), Vector4<TypeParam>(2, 4, 6, 8)),
              (Vector4<TypeParam>(1, 4, 3, 7)));
}

TYPED_TEST(Vector4Test, GlobalMax2Function)
{
    EXPECT_EQ(gp::math::max(Vector4<TypeParam>(1, 5, 3, 7), Vector4<TypeParam>(2, 4, 6, 8)),
              (Vector4<TypeParam>(2, 5, 6, 8)));
}

TYPED_TEST(Vector4Test, GlobalMin3Function)
{
    EXPECT_EQ(gp::math::min(Vector4<TypeParam>(1, 5, 3, 7), Vector4<TypeParam>(2, 4, 6, 8),
                            Vector4<TypeParam>(0, 7, 4, 6)),
              (Vector4<TypeParam>(0, 4, 3, 6)));
}

TYPED_TEST(Vector4Test, GlobalMax3Function)
{
    EXPECT_EQ(gp::math::max(Vector4<TypeParam>(1, 5, 3, 7), Vector4<TypeParam>(2, 4, 6, 8),
                            Vector4<TypeParam>(0, 7, 4, 6)),
              (Vector4<TypeParam>(2, 7, 6, 8)));
}

TYPED_TEST(Vector4Test, GlobalClampFunction)
{
    EXPECT_EQ(gp::math::clamp(Vector4<TypeParam>(-1, 5, 10, 4), Vector4<TypeParam>::zero(),
                              Vector4<TypeParam>(2, 4, 6, 8)),
              (Vector4<TypeParam>(0, 4, 6, 4)));
}

}   // namespace gp::math::tests
