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

TYPED_TEST(Vector2Test, Constants)
{
    EXPECT_EQ(Vector2<TypeParam>::zero(), (Vector2<TypeParam>(this->zero, this->zero)));
    EXPECT_EQ(Vector2<TypeParam>::one(), (Vector2<TypeParam>(this->one, this->one)));
    EXPECT_EQ(Vector2<TypeParam>::up(), (Vector2<TypeParam>(this->zero, this->one)));
    EXPECT_EQ(Vector2<TypeParam>::down(), (Vector2<TypeParam>(this->zero, -this->one)));
    EXPECT_EQ(Vector2<TypeParam>::left(), (Vector2<TypeParam>(-this->one, this->zero)));
    EXPECT_EQ(Vector2<TypeParam>::right(), (Vector2<TypeParam>(this->one, this->zero)));
    EXPECT_EQ(Vector2<TypeParam>::unitX(), Vector2<TypeParam>::right());
    EXPECT_EQ(Vector2<TypeParam>::unitY(), Vector2<TypeParam>::up());
}

TYPED_TEST(Vector2Test, CrossProductOperator)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 0.0 });
    Vector2<TypeParam> vecB(TypeParam{ 0.0 }, TypeParam{ 1.0 });

    TypeParam cross = vecA ^ vecB;

    EXPECT_EQ(cross, this->one);
}

TYPED_TEST(Vector2Test, DotProductOperator)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 3.0 }, TypeParam{ 4.0 });

    TypeParam dot = vecA | vecB;

    EXPECT_EQ(dot, TypeParam{ 11.0 });
}

TYPED_TEST(Vector2Test, NegationOperator)
{
    Vector2<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ -2.0 });

    Vector2<TypeParam> negated = -vec;

    EXPECT_EQ(negated.x, -vec.x);
    EXPECT_EQ(negated.y, -vec.y);
}

TYPED_TEST(Vector2Test, UnaryPlusOperator)
{
    Vector2<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ -2.0 });

    Vector2<TypeParam> result = +vec;

    EXPECT_EQ(result.x, vec.x);
    EXPECT_EQ(result.y, vec.y);
}

TYPED_TEST(Vector2Test, EqualityOperator)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecC(TypeParam{ 3.0 }, TypeParam{ 4.0 });

    EXPECT_TRUE(vecA == vecB);
    EXPECT_FALSE(vecA == vecC);
}

TYPED_TEST(Vector2Test, InequalityOperator)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecC(TypeParam{ 3.0 }, TypeParam{ 4.0 });

    EXPECT_FALSE(vecA != vecB);
    EXPECT_TRUE(vecA != vecC);
}

TYPED_TEST(Vector2Test, SubscriptOperator)
{
    Vector2<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 });

    EXPECT_EQ(vec[0], vec.x);
    EXPECT_EQ(vec[1], vec.y);

    const Vector2<TypeParam> constVec(TypeParam{ 3.0 }, TypeParam{ 4.0 });
    EXPECT_EQ(constVec[0], constVec.x);
    EXPECT_EQ(constVec[1], constVec.y);

    Vector2<TypeParam> modifiableVec(TypeParam{ 5.0 }, TypeParam{ 6.0 });
    modifiableVec[0] = TypeParam{ 7.0 };
    modifiableVec[1] = TypeParam{ 8.0 };
    EXPECT_EQ(modifiableVec.x, TypeParam{ 7.0 });
    EXPECT_EQ(modifiableVec.y, TypeParam{ 8.0 });
}

TYPED_TEST(Vector2Test, VectorAdditionOperator)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 3.0 }, TypeParam{ 4.0 });

    Vector2<TypeParam> sum = vecA + vecB;

    EXPECT_NEAR(sum.x, vecA.x + vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(sum.y, vecA.y + vecB.y, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, ScalarAdditionOperator)
{
    Vector2<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    TypeParam bias = TypeParam{ 5.0 };

    Vector2<TypeParam> result = vec + bias;

    EXPECT_NEAR(result.x, vec.x + bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y + bias, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, ScalarAdditionOperatorCommutative)
{
    Vector2<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    TypeParam bias = TypeParam{ 5.0 };

    Vector2<TypeParam> result = bias + vec;

    EXPECT_NEAR(result.x, vec.x + bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y + bias, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, VectorSubtractionOperator)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 3.0 }, TypeParam{ 4.0 });

    Vector2<TypeParam> difference = vecA - vecB;

    EXPECT_NEAR(difference.x, vecA.x - vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(difference.y, vecA.y - vecB.y, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, ScalarSubtractionOperator)
{
    Vector2<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    TypeParam bias = TypeParam{ 5.0 };

    Vector2<TypeParam> result = vec - bias;

    EXPECT_NEAR(result.x, vec.x - bias, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y - bias, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, VectorMultiplicationOperator)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 3.0 }, TypeParam{ 4.0 });

    Vector2<TypeParam> product = vecA * vecB;

    EXPECT_NEAR(product.x, vecA.x * vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(product.y, vecA.y * vecB.y, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, ScalarMultiplicationOperator)
{
    Vector2<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    Vector2<TypeParam> result = vec * scalar;

    EXPECT_NEAR(result.x, vec.x * scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y * scalar, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, ScalarMultiplicationOperatorCommutative)
{
    Vector2<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    Vector2<TypeParam> result = scalar * vec;

    EXPECT_NEAR(result.x, vec.x * scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y * scalar, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, VectorDivisionOperator)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 4.0 }, TypeParam{ 5.0 });

    Vector2<TypeParam> quotient = vecA / vecB;

    EXPECT_NEAR(quotient.x, vecA.x / vecB.x, TypeParam{ 1e-6 });
    EXPECT_NEAR(quotient.y, vecA.y / vecB.y, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, ScalarDivisionOperator)
{
    Vector2<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    Vector2<TypeParam> result = vec / scalar;

    EXPECT_NEAR(result.x, vec.x / scalar, TypeParam{ 1e-6 });
    EXPECT_NEAR(result.y, vec.y / scalar, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, CompoundAssignmentOperators)
{
    Vector2<TypeParam> value(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    const Vector2<TypeParam> other(TypeParam{ 4.0 }, TypeParam{ 5.0 });
    TypeParam scalar = TypeParam{ 5.0 };

    value += other;
    EXPECT_NEAR(value.x, TypeParam{ 5.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.y, TypeParam{ 7.0 }, TypeParam{ 1e-6 });

    value -= other;
    EXPECT_NEAR(value.x, TypeParam{ 1.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.y, TypeParam{ 2.0 }, TypeParam{ 1e-6 });

    value *= scalar;
    EXPECT_NEAR(value.x, TypeParam{ 5.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.y, TypeParam{ 10.0 }, TypeParam{ 1e-6 });

    value /= scalar;
    EXPECT_NEAR(value.x, TypeParam{ 1.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(value.y, TypeParam{ 2.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, ComponentMethod)
{
    Vector2<TypeParam> vec(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    EXPECT_EQ(vec.component(0), TypeParam{ 1.0 });
    EXPECT_EQ(vec.component(1), TypeParam{ 2.0 });

    const Vector2<TypeParam> constVec(TypeParam{ 3.0 }, TypeParam{ 4.0 });
    EXPECT_EQ(constVec.component(0), TypeParam{ 3.0 });
    EXPECT_EQ(constVec.component(1), TypeParam{ 4.0 });
}

TYPED_TEST(Vector2Test, CrossMethod)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 1.0 });

    EXPECT_EQ(vecA.cross(vecB), TypeParam{ -3.0 });
}

TYPED_TEST(Vector2Test, DotMethod)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 1.0 });

    EXPECT_EQ(vecA.dot(vecB), TypeParam{ 4.0 });
}

TYPED_TEST(Vector2Test, EqualsMethod)
{
    Vector2<TypeParam> vecA(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 1.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecC(TypeParam{ 1.1 }, TypeParam{ 2.0 });

    EXPECT_TRUE(vecA.equals(vecB));
    EXPECT_FALSE(vecA.equals(vecC));
    EXPECT_TRUE(vecA.equals(vecC, TypeParam{ 0.2 }));
}

TYPED_TEST(Vector2Test, IsAllComponentsEqualMethod)
{
    Vector2<TypeParam> vecA(TypeParam{ 2.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 2.1 });

    EXPECT_TRUE(vecA.isAllComponentsEqual());
    EXPECT_FALSE(vecB.isAllComponentsEqual());
    EXPECT_TRUE(vecB.isAllComponentsEqual(TypeParam{ 0.2 }));
}

TYPED_TEST(Vector2Test, IsNearlyZeroMethod)
{
    Vector2<TypeParam> vecA = Vector2<TypeParam>::zero();
    Vector2<TypeParam> vecB(TypeParam{ 0.00001 }, TypeParam{ 0.0 });

    EXPECT_TRUE(vecA.isNearlyZero());
    EXPECT_FALSE(vecB.isNearlyZero(TypeParam{ 1e-6 }));
    EXPECT_TRUE(vecB.isNearlyZero(TypeParam{ 1e-4 }));
}

TYPED_TEST(Vector2Test, IsZeroMethod)
{
    Vector2<TypeParam> vecA = Vector2<TypeParam>::zero();
    Vector2<TypeParam> vecB(TypeParam{ 0.00001 }, TypeParam{ 0.0 });

    EXPECT_TRUE(vecA.isZero());
    EXPECT_FALSE(vecB.isZero());
}

TYPED_TEST(Vector2Test, IsUnitMethod)
{
    Vector2<TypeParam> vecA = Vector2<TypeParam>::unitX();
    Vector2<TypeParam> vecB(TypeParam{ 0.5 }, TypeParam{ 0.5 });

    EXPECT_TRUE(vecA.isUnit());
    EXPECT_FALSE(vecB.isUnit());
}

TYPED_TEST(Vector2Test, IsNormalizedMethod)
{
    EXPECT_TRUE(Vector2<TypeParam>::unitX().isNormalized());
}

TYPED_TEST(Vector2Test, IsUniformMethod)
{
    Vector2<TypeParam> vecA(TypeParam{ 2.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> vecB(TypeParam{ 2.0 }, TypeParam{ 2.1 });

    EXPECT_TRUE(vecA.isUniform());
    EXPECT_FALSE(vecB.isUniform());
}

TYPED_TEST(Vector2Test, GetMaxMethod)
{
    EXPECT_EQ(Vector2<TypeParam>(TypeParam{ -2.0 }, TypeParam{ 4.0 }).getMax(), TypeParam{ 4.0 });
}

TYPED_TEST(Vector2Test, GetMinMethod)
{
    EXPECT_EQ(Vector2<TypeParam>(TypeParam{ -2.0 }, TypeParam{ 4.0 }).getMin(), TypeParam{ -2.0 });
}

TYPED_TEST(Vector2Test, GetAbsMaxMethod)
{
    EXPECT_EQ(Vector2<TypeParam>(TypeParam{ -2.0 }, TypeParam{ 4.0 }).getAbsMax(), TypeParam{ 4.0 });
}

TYPED_TEST(Vector2Test, GetAbsMinMethod)
{
    EXPECT_EQ(Vector2<TypeParam>(TypeParam{ -2.0 }, TypeParam{ 4.0 }).getAbsMin(), TypeParam{ 2.0 });
}

TYPED_TEST(Vector2Test, GetComponentMinMethod)
{
    EXPECT_EQ(Vector2<TypeParam>(TypeParam{ -2.0 }, TypeParam{ 4.0 })
                  .getComponentMin(Vector2<TypeParam>(TypeParam{ -3.0 }, TypeParam{ 5.0 })),
              (Vector2<TypeParam>(TypeParam{ -3.0 }, TypeParam{ 4.0 })));
}

TYPED_TEST(Vector2Test, GetComponentMaxMethod)
{
    EXPECT_EQ(Vector2<TypeParam>(TypeParam{ -2.0 }, TypeParam{ 4.0 })
                  .getComponentMax(Vector2<TypeParam>(TypeParam{ -3.0 }, TypeParam{ 5.0 })),
              (Vector2<TypeParam>(TypeParam{ -2.0 }, TypeParam{ 5.0 })));
}

TYPED_TEST(Vector2Test, GetComponentAbsMinMethod)
{
    EXPECT_EQ(Vector2<TypeParam>(TypeParam{ -2.0 }, TypeParam{ 4.0 })
                  .getComponentAbsMin(Vector2<TypeParam>(TypeParam{ 3.0 }, TypeParam{ 2.0 })),
              (Vector2<TypeParam>(TypeParam{ 2.0 }, TypeParam{ 2.0 })));
}

TYPED_TEST(Vector2Test, GetComponentAbsMaxMethod)
{
    EXPECT_EQ(Vector2<TypeParam>(TypeParam{ -2.0 }, TypeParam{ 4.0 })
                  .getComponentAbsMax(Vector2<TypeParam>(TypeParam{ 3.0 }, TypeParam{ 2.0 })),
              (Vector2<TypeParam>(TypeParam{ 3.0 }, TypeParam{ 4.0 })));
}

TYPED_TEST(Vector2Test, GetAbsoluteMethod)
{
    EXPECT_EQ(Vector2<TypeParam>(TypeParam{ -2.0 }, TypeParam{ 4.0 }).getAbsolute(),
              (Vector2<TypeParam>(TypeParam{ 2.0 }, TypeParam{ 4.0 })));
}

TYPED_TEST(Vector2Test, LengthMethod)
{
    Vector2<TypeParam> vec(TypeParam{ 3.0 }, TypeParam{ 4.0 });
    EXPECT_NEAR(vec.length(), TypeParam{ 5.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, LengthSquaredMethod)
{
    Vector2<TypeParam> vec(TypeParam{ 3.0 }, TypeParam{ 4.0 });
    EXPECT_NEAR(vec.lengthSquared(), TypeParam{ 25.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, NormalizeMethod)
{
    Vector2<TypeParam> vec(TypeParam{ 3.0 }, TypeParam{ 4.0 });
    EXPECT_TRUE(vec.normalize());
    EXPECT_NEAR(vec.x, TypeParam{ 0.6 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(vec.y, TypeParam{ 0.8 }, TypeParam{ 1e-6 });

    Vector2<TypeParam> zeroVec = Vector2<TypeParam>::zero();
    EXPECT_FALSE(zeroVec.normalize());
}

TYPED_TEST(Vector2Test, GetUnsafeNormalMethod)
{
    Vector2<TypeParam> vec(TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector2<TypeParam> norm = vec.getUnsafeNormal();
    EXPECT_NEAR(norm.x, TypeParam{ 0.6 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(norm.y, TypeParam{ 0.8 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, GetSafeNormalMethod)
{
    Vector2<TypeParam> vec(TypeParam{ 3.0 }, TypeParam{ 4.0 });
    Vector2<TypeParam> norm = vec.getSafeNormal();
    EXPECT_NEAR(norm.x, TypeParam{ 0.6 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(norm.y, TypeParam{ 0.8 }, TypeParam{ 1e-6 });

    Vector2<TypeParam> zeroVec = Vector2<TypeParam>::zero();
    EXPECT_EQ(zeroVec.getSafeNormal(), Vector2<TypeParam>::zero());
}

TYPED_TEST(Vector2Test, GetSignVectorMethod)
{
    Vector2<TypeParam> vec(TypeParam{ -5.0 }, TypeParam{ 0.0 });
    Vector2<TypeParam> signVec = vec.getSignVector();
    EXPECT_EQ(signVec.x, TypeParam{ -1.0 });
    EXPECT_EQ(signVec.y, TypeParam{ 0.0 });
}

TYPED_TEST(Vector2Test, GetReciprocalMethod)
{
    Vector2<TypeParam> vec(TypeParam{ 2.0 }, TypeParam{ 4.0 });
    Vector2<TypeParam> reciprocal = vec.getReciprocal();
    EXPECT_NEAR(reciprocal.x, TypeParam{ 0.5 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(reciprocal.y, TypeParam{ 0.25 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, ProjectOnToMethod)
{
    Vector2<TypeParam> vec(TypeParam{ 2.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> onto(TypeParam{ 1.0 }, TypeParam{ 0.0 });
    Vector2<TypeParam> projection = vec.projectOnTo(onto);
    EXPECT_NEAR(projection.x, TypeParam{ 2.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(projection.y, TypeParam{ 0.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, ProjectOnToNormalMethod)
{
    Vector2<TypeParam> vec(TypeParam{ 2.0 }, TypeParam{ 2.0 });
    Vector2<TypeParam> normal(TypeParam{ 1.0 }, TypeParam{ 0.0 });
    Vector2<TypeParam> projection = vec.projectOnToNormal(normal);
    EXPECT_NEAR(projection.x, TypeParam{ 2.0 }, TypeParam{ 1e-6 });
    EXPECT_NEAR(projection.y, TypeParam{ 0.0 }, TypeParam{ 1e-6 });
}

TYPED_TEST(Vector2Test, StaticCrossMethod)
{
    EXPECT_EQ(Vector2<TypeParam>::cross(Vector2<TypeParam>(1, 5), Vector2<TypeParam>(2, 4)), TypeParam{ -6 });
}

TYPED_TEST(Vector2Test, StaticDotMethod)
{
    EXPECT_EQ(Vector2<TypeParam>::dot(Vector2<TypeParam>(1, 5), Vector2<TypeParam>(2, 4)), TypeParam{ 22 });
}

TYPED_TEST(Vector2Test, StaticMin2Method)
{
    EXPECT_EQ(Vector2<TypeParam>::min(Vector2<TypeParam>(1, 5), Vector2<TypeParam>(2, 4)),
              (Vector2<TypeParam>(1, 4)));
}

TYPED_TEST(Vector2Test, StaticMax2Method)
{
    EXPECT_EQ(Vector2<TypeParam>::max(Vector2<TypeParam>(1, 5), Vector2<TypeParam>(2, 4)),
              (Vector2<TypeParam>(2, 5)));
}

TYPED_TEST(Vector2Test, StaticMin3Method)
{
    EXPECT_EQ(Vector2<TypeParam>::min(Vector2<TypeParam>(1, 5), Vector2<TypeParam>(2, 4), Vector2<TypeParam>(0, 6)),
              (Vector2<TypeParam>(0, 4)));
}

TYPED_TEST(Vector2Test, StaticMax3Method)
{
    EXPECT_EQ(Vector2<TypeParam>::max(Vector2<TypeParam>(1, 5), Vector2<TypeParam>(2, 4), Vector2<TypeParam>(0, 6)),
              (Vector2<TypeParam>(2, 6)));
}

TYPED_TEST(Vector2Test, StaticClampMethod)
{
    EXPECT_EQ(Vector2<TypeParam>::clamp(Vector2<TypeParam>(-1, 7), Vector2<TypeParam>::zero(),
                                        Vector2<TypeParam>(2, 6)),
              (Vector2<TypeParam>(0, 6)));
}

TYPED_TEST(Vector2Test, GlobalMin2Function)
{
    EXPECT_EQ(gp::math::min(Vector2<TypeParam>(1, 5), Vector2<TypeParam>(2, 4)),
              (Vector2<TypeParam>(1, 4)));
}

TYPED_TEST(Vector2Test, GlobalMax2Function)
{
    EXPECT_EQ(gp::math::max(Vector2<TypeParam>(1, 5), Vector2<TypeParam>(2, 4)),
              (Vector2<TypeParam>(2, 5)));
}

TYPED_TEST(Vector2Test, GlobalMin3Function)
{
    EXPECT_EQ(gp::math::min(Vector2<TypeParam>(1, 5), Vector2<TypeParam>(2, 4), Vector2<TypeParam>(0, 6)),
              (Vector2<TypeParam>(0, 4)));
}

TYPED_TEST(Vector2Test, GlobalMax3Function)
{
    EXPECT_EQ(gp::math::max(Vector2<TypeParam>(1, 5), Vector2<TypeParam>(2, 4), Vector2<TypeParam>(0, 6)),
              (Vector2<TypeParam>(2, 6)));
}

TYPED_TEST(Vector2Test, GlobalClampFunction)
{
    EXPECT_EQ(gp::math::clamp(Vector2<TypeParam>(-1, 7), Vector2<TypeParam>::zero(),
                              Vector2<TypeParam>(2, 6)),
              (Vector2<TypeParam>(0, 6)));
}

}   // namespace gp::math::tests
