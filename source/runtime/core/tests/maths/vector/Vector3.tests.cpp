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

TYPED_TEST(Vector3Test, CompoundAssignmentOperators)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> other(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    vec += other;
    EXPECT_NEAR(vec.x, TypeParam{ 5.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.y, TypeParam{ 7.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.z, TypeParam{ 9.0 }, TypeParam{ 1e-6 });

    vec -= other;
    EXPECT_NEAR(vec.x, TypeParam{ 1.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.y, TypeParam{ 2.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.z, TypeParam{ 3.0 }, TypeParam{ 1e-6 });

    vec *= scalar;
    EXPECT_NEAR(vec.x, TypeParam{ 5.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.y, TypeParam{ 10.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.z, TypeParam{ 15.0 }, TypeParam{ 1e-6 });

    vec /= scalar;
    EXPECT_NEAR(vec.x, TypeParam{ 1.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.y, TypeParam{ 2.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.z, TypeParam{ 3.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, ComponentMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    EXPECT_EQ(vec.component(0), TypeParam{ 1.0 });
    EXPECT_EQ(vec.component(1), TypeParam{ 2.0 });
    EXPECT_EQ(vec.component(2), TypeParam{ 3.0 });

    const Vector3<TypeParam> constVec(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });
    EXPECT_EQ(constVec.component(0), TypeParam{ 4.0 });
    EXPECT_EQ(constVec.component(1), TypeParam{ 5.0 });
    EXPECT_EQ(constVec.component(2), TypeParam{ 6.0 });
}

TYPED_TEST(Vector3Test, CrossMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> vecB(TypeParam{ 0.0 }, TypeParam{ 1.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> cross = vecA.cross(vecB);
    EXPECT_EQ(cross.x, this->zero);
    EXPECT_EQ(cross.y, this->zero);
    EXPECT_EQ(cross.z, this->one);
}

TYPED_TEST(Vector3Test, DotMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });
    EXPECT_EQ(vecA.dot(vecB), TypeParam{ 32.0 });
}

TYPED_TEST(Vector3Test, EqualsMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecC(TypeParam{ 1.1 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });

    EXPECT_TRUE(vecA.equals(vecB));
    EXPECT_FALSE(vecA.equals(vecC));
    EXPECT_TRUE(vecA.equals(vecC, TypeParam{ 0.2 }));
}

TYPED_TEST(Vector3Test, IsAllComponentsEqualMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.1 });

    EXPECT_TRUE(vecA.isAllComponentsEqual());
    EXPECT_FALSE(vecB.isAllComponentsEqual());
    EXPECT_TRUE(vecB.isAllComponentsEqual(TypeParam{ 0.2 }));
}

TYPED_TEST(Vector3Test, IsNearlyZeroMethod)
{
    Vector3<TypeParam> vecA = Vector3<TypeParam>::zero();
    Vector3<TypeParam> vecB(TypeParam{ 0.00001 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });

    EXPECT_TRUE(vecA.isNearlyZero());
    EXPECT_FALSE(vecB.isNearlyZero(TypeParam{ 1e-6 }));
    EXPECT_TRUE(vecB.isNearlyZero(TypeParam{ 1e-4 }));
}

TYPED_TEST(Vector3Test, IsZeroMethod)
{
    Vector3<TypeParam> vecA = Vector3<TypeParam>::zero();
    Vector3<TypeParam> vecB(TypeParam{ 0.00001 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });

    EXPECT_TRUE(vecA.isZero());
    EXPECT_FALSE(vecB.isZero());
}

TYPED_TEST(Vector3Test, IsUnitMethod)
{
    Vector3<TypeParam> vecA = Vector3<TypeParam>::unitX();
    Vector3<TypeParam> vecB(TypeParam{ 0.5 }, TypeParam{ 0.5 }, TypeParam{ 0.5 });

    EXPECT_TRUE(vecA.isUnit());
    EXPECT_FALSE(vecB.isUnit());
}

TYPED_TEST(Vector3Test, IsNormalizedMethod)
{
    Vector3<TypeParam> vecA = Vector3<TypeParam>::unitY();
    Vector3<TypeParam> vecB(TypeParam{ 1.0 }, TypeParam{ 1.0 }, TypeParam{ 0.0 });

    EXPECT_TRUE(vecA.isNormalized());
    EXPECT_FALSE(vecB.isNormalized());
}

TYPED_TEST(Vector3Test, IsUniformMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ 5.0 }, TypeParam{ 5.0 }, TypeParam{ 5.0 });
    Vector3<TypeParam> vecB(TypeParam{ 5.0 }, TypeParam{ 4.0 }, TypeParam{ 5.0 });

    EXPECT_TRUE(vecA.isUniform());
    EXPECT_FALSE(vecB.isUniform());
}

TYPED_TEST(Vector3Test, GetMaxMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    EXPECT_EQ(vec.getMax(), TypeParam{ 5.0 });
}

TYPED_TEST(Vector3Test, GetMinMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ -3.0 });
    EXPECT_EQ(vec.getMin(), TypeParam{ -3.0 });
}

TYPED_TEST(Vector3Test, GetAbsMaxMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ -5.0 }, TypeParam{ 3.0 });
    EXPECT_EQ(vec.getAbsMax(), TypeParam{ 5.0 });
}

TYPED_TEST(Vector3Test, GetAbsMinMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ -5.0 }, TypeParam{ 0.5 });
    EXPECT_EQ(vec.getAbsMin(), TypeParam{ 0.5 });
}

TYPED_TEST(Vector3Test, GetComponentMinMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> minVec = vecA.getComponentMin(vecB);
    EXPECT_EQ(minVec.x, TypeParam{ 1.0 });
    EXPECT_EQ(minVec.y, TypeParam{ 4.0 });
    EXPECT_EQ(minVec.z, TypeParam{ 3.0 });
}

TYPED_TEST(Vector3Test, GetComponentMaxMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> maxVec = vecA.getComponentMax(vecB);
    EXPECT_EQ(maxVec.x, TypeParam{ 2.0 });
    EXPECT_EQ(maxVec.y, TypeParam{ 5.0 });
    EXPECT_EQ(maxVec.z, TypeParam{ 6.0 });
}

TYPED_TEST(Vector3Test, GetComponentAbsMinMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ -1.0 }, TypeParam{ 5.0 }, TypeParam{ -3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ -4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> minVec = vecA.getComponentAbsMin(vecB);
    EXPECT_EQ(minVec.x, TypeParam{ 1.0 });
    EXPECT_EQ(minVec.y, TypeParam{ 4.0 });
    EXPECT_EQ(minVec.z, TypeParam{ 3.0 });
}

TYPED_TEST(Vector3Test, GetComponentAbsMaxMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ -1.0 }, TypeParam{ 5.0 }, TypeParam{ -3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ -4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> maxVec = vecA.getComponentAbsMax(vecB);
    EXPECT_EQ(maxVec.x, TypeParam{ 2.0 });
    EXPECT_EQ(maxVec.y, TypeParam{ 5.0 });
    EXPECT_EQ(maxVec.z, TypeParam{ 6.0 });
}

TYPED_TEST(Vector3Test, GetAbsoluteMethod)
{
    Vector3<TypeParam> vec(TypeParam{ -1.0 }, TypeParam{ 2.0 }, TypeParam{ -3.0 });
    Vector3<TypeParam> absVec = vec.getAbsolute();
    EXPECT_EQ(absVec.x, TypeParam{ 1.0 });
    EXPECT_EQ(absVec.y, TypeParam{ 2.0 });
    EXPECT_EQ(absVec.z, TypeParam{ 3.0 });
}

TYPED_TEST(Vector3Test, LengthMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 0.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    EXPECT_NEAR(vec.length(), TypeParam{ 5.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, LengthSquaredMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    EXPECT_NEAR(vec.lengthSquared(), TypeParam{ 14.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, NormalizeMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 0.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    EXPECT_TRUE(vec.normalize());
    EXPECT_NEAR(vec.x, TypeParam{ 0.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.y, TypeParam{ 0.6 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.z, TypeParam{ 0.8 }, TypeParam{ 1e-6 });

    Vector3<TypeParam> zeroVec = Vector3<TypeParam>::zero();
    EXPECT_FALSE(zeroVec.normalize());
}

TYPED_TEST(Vector3Test, GetUnsafeNormalMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 0.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector3<TypeParam> norm = vec.getUnsafeNormal();
    EXPECT_NEAR(norm.x, TypeParam{ 0.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(norm.y, TypeParam{ 0.6 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(norm.z, TypeParam{ 0.8 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, GetSafeNormalMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 0.0 }, TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector3<TypeParam> norm = vec.getSafeNormal();
    EXPECT_NEAR(norm.x, TypeParam{ 0.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(norm.y, TypeParam{ 0.6 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(norm.z, TypeParam{ 0.8 }, TypeParam{ 1e-6 });

    Vector3<TypeParam> zeroVec = Vector3<TypeParam>::zero();
    Vector3<TypeParam> zeroNorm = zeroVec.getSafeNormal();
    EXPECT_EQ(zeroNorm.x, TypeParam{ 0.0 });
    EXPECT_EQ(zeroNorm.y, TypeParam{ 0.0 });
    EXPECT_EQ(zeroNorm.z, TypeParam{ 0.0 });
}

TYPED_TEST(Vector3Test, GetSignVectorMethod)
{
    Vector3<TypeParam> vec(TypeParam{ -5.0 }, TypeParam{ 0.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> signVec = vec.getSignVector();
    EXPECT_EQ(signVec.x, TypeParam{ -1.0 });
    EXPECT_EQ(signVec.y, TypeParam{ 0.0 });
    EXPECT_EQ(signVec.z, TypeParam{ 1.0 });
}

TYPED_TEST(Vector3Test, GetProjectionMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 2.0 });
    Vector3<TypeParam> proj = vec.getProjection();
    EXPECT_NEAR(proj.x, TypeParam{ 1.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(proj.y, TypeParam{ 2.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(proj.z, TypeParam{ 1.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, GetReciprocalMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 5.0 });
    Vector3<TypeParam> rec = vec.getReciprocal();
    EXPECT_NEAR(rec.x, TypeParam{ 0.5 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(rec.y, TypeParam{ 0.25 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(rec.z, TypeParam{ 0.2 }, TypeParam{ 1e-6 });

    Vector3<TypeParam> vecZero = Vector3<TypeParam>::zero();
    Vector3<TypeParam> recZero = vecZero.getReciprocal();
    EXPECT_EQ(recZero.x, constants<TypeParam>::bigNumber);
    EXPECT_EQ(recZero.y, constants<TypeParam>::bigNumber);
    EXPECT_EQ(recZero.z, constants<TypeParam>::bigNumber);
}

TYPED_TEST(Vector3Test, ProjectOnToMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> onto(TypeParam{ 1.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> proj = vec.projectOnTo(onto);
    EXPECT_NEAR(proj.x, TypeParam{ 2.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(proj.y, TypeParam{ 0.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(proj.z, TypeParam{ 0.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, ProjectOnToNormalMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> normal(TypeParam{ 1.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> proj = vec.projectOnToNormal(normal);
    EXPECT_NEAR(proj.x, TypeParam{ 2.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(proj.y, TypeParam{ 0.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(proj.z, TypeParam{ 0.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, MirrorByVectorMethod)
{
    Vector3<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ -1.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> normal(TypeParam{ 0.0 }, TypeParam{ 1.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> mirrored = vec.mirrorByVector(normal);
    EXPECT_NEAR(mirrored.x, TypeParam{ 1.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(mirrored.y, TypeParam{ 1.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(mirrored.z, TypeParam{ 0.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector3Test, StaticCrossMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> vecB(TypeParam{ 0.0 }, TypeParam{ 1.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> cross = Vector3<TypeParam>::cross(vecA, vecB);
    EXPECT_EQ(cross.x, this->zero);
    EXPECT_EQ(cross.y, this->zero);
    EXPECT_EQ(cross.z, this->one);
}

TYPED_TEST(Vector3Test, StaticDotMethod)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 4.0 }, TypeParam{ 5.0 }, TypeParam{ 6.0 });
    EXPECT_EQ(Vector3<TypeParam>::dot(vecA, vecB), TypeParam{ 32.0 });
}

TYPED_TEST(Vector3Test, StaticMin2Method)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> minVec = Vector3<TypeParam>::min(vecA, vecB);
    EXPECT_EQ(minVec.x, TypeParam{ 1.0 });
    EXPECT_EQ(minVec.y, TypeParam{ 4.0 });
    EXPECT_EQ(minVec.z, TypeParam{ 3.0 });
}

TYPED_TEST(Vector3Test, StaticMax2Method)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> maxVec = Vector3<TypeParam>::max(vecA, vecB);
    EXPECT_EQ(maxVec.x, TypeParam{ 2.0 });
    EXPECT_EQ(maxVec.y, TypeParam{ 5.0 });
    EXPECT_EQ(maxVec.z, TypeParam{ 6.0 });
}

TYPED_TEST(Vector3Test, StaticMin3Method)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> vecC(TypeParam{ 0.0 }, TypeParam{ 7.0 }, TypeParam{ 4.0 });
    Vector3<TypeParam> minVec = Vector3<TypeParam>::min(vecA, vecB, vecC);
    EXPECT_EQ(minVec.x, TypeParam{ 0.0 });
    EXPECT_EQ(minVec.y, TypeParam{ 4.0 });
    EXPECT_EQ(minVec.z, TypeParam{ 3.0 });
}

TYPED_TEST(Vector3Test, StaticMax3Method)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> vecC(TypeParam{ 0.0 }, TypeParam{ 7.0 }, TypeParam{ 4.0 });
    Vector3<TypeParam> maxVec = Vector3<TypeParam>::max(vecA, vecB, vecC);
    EXPECT_EQ(maxVec.x, TypeParam{ 2.0 });
    EXPECT_EQ(maxVec.y, TypeParam{ 7.0 });
    EXPECT_EQ(maxVec.z, TypeParam{ 6.0 });
}

TYPED_TEST(Vector3Test, StaticClampMethod)
{
    Vector3<TypeParam> vec(TypeParam{ -1.0 }, TypeParam{ 5.0 }, TypeParam{ 10.0 });
    Vector3<TypeParam> minVec(TypeParam{ 0.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> maxVec(TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.0 });
    Vector3<TypeParam> clamped = Vector3<TypeParam>::clamp(vec, minVec, maxVec);
    EXPECT_EQ(clamped.x, TypeParam{ 0.0 });
    EXPECT_EQ(clamped.y, TypeParam{ 2.0 });
    EXPECT_EQ(clamped.z, TypeParam{ 2.0 });
}

TYPED_TEST(Vector3Test, GlobalMin2Function)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> minVec = gp::math::min(vecA, vecB);
    EXPECT_EQ(minVec.x, TypeParam{ 1.0 });
    EXPECT_EQ(minVec.y, TypeParam{ 4.0 });
    EXPECT_EQ(minVec.z, TypeParam{ 3.0 });
}

TYPED_TEST(Vector3Test, GlobalMax2Function)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> maxVec = gp::math::max(vecA, vecB);
    EXPECT_EQ(maxVec.x, TypeParam{ 2.0 });
    EXPECT_EQ(maxVec.y, TypeParam{ 5.0 });
    EXPECT_EQ(maxVec.z, TypeParam{ 6.0 });
}

TYPED_TEST(Vector3Test, GlobalMin3Function)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> vecC(TypeParam{ 0.0 }, TypeParam{ 7.0 }, TypeParam{ 4.0 });
    Vector3<TypeParam> minVec = gp::math::min(vecA, vecB, vecC);
    EXPECT_EQ(minVec.x, TypeParam{ 0.0 });
    EXPECT_EQ(minVec.y, TypeParam{ 4.0 });
    EXPECT_EQ(minVec.z, TypeParam{ 3.0 });
}

TYPED_TEST(Vector3Test, GlobalMax3Function)
{
    Vector3<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 5.0 }, TypeParam{ 3.0 });
    Vector3<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 4.0 }, TypeParam{ 6.0 });
    Vector3<TypeParam> vecC(TypeParam{ 0.0 }, TypeParam{ 7.0 }, TypeParam{ 4.0 });
    Vector3<TypeParam> maxVec = gp::math::max(vecA, vecB, vecC);
    EXPECT_EQ(maxVec.x, TypeParam{ 2.0 });
    EXPECT_EQ(maxVec.y, TypeParam{ 7.0 });
    EXPECT_EQ(maxVec.z, TypeParam{ 6.0 });
}

TYPED_TEST(Vector3Test, GlobalClampFunction)
{
    Vector3<TypeParam> vec(TypeParam{ -1.0 }, TypeParam{ 5.0 }, TypeParam{ 10.0 });
    Vector3<TypeParam> minVec(TypeParam{ 0.0 }, TypeParam{ 0.0 }, TypeParam{ 0.0 });
    Vector3<TypeParam> maxVec(TypeParam{ 2.0 }, TypeParam{ 2.0 }, TypeParam{ 2.0 });
    Vector3<TypeParam> clamped = gp::math::clamp(vec, minVec, maxVec);
    EXPECT_EQ(clamped.x, TypeParam{ 0.0 });
    EXPECT_EQ(clamped.y, TypeParam{ 2.0 });
    EXPECT_EQ(clamped.z, TypeParam{ 2.0 });
}

}   // namespace gp::math::tests
