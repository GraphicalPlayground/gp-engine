// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "maths/matrix/Matrix.hpp"
#include "maths/matrix/MatrixOps.hpp"

namespace gp::math::tests
{

namespace
{

// Deterministic generator so any failure is reproducible.
class TestRandom
{
public:
    template <typename T>
    T next()
    {
        state_ = state_ * 1664525u + 1013904223u;
        return static_cast<T>(((state_ >> 8) & 0xFFFFu) / 32768.0 - 1.0);
    }

private:
    unsigned state_ = 12345u;
};

template <typename T, int R, int C>
Matrix<T, R, C> makeRandomMatrix(TestRandom& rng)
{
    Matrix<T, R, C> m;
    for (int c = 0; c < C; ++c)
        for (int r = 0; r < R; ++r)
            m(r, c) = rng.next<T>();
    return m;
}

// The padding lanes (rows R..3 of every column) must always be exactly zero.
template <typename T, int R, int C>
void expectPaddingIsZero(const Matrix<T, R, C>& m)
{
    for (int c = 0; c < C; ++c)
        for (int r = R; r < 4; ++r)
            EXPECT_EQ(m.data()[c * 4 + r], T{ 0 }) << "padding lane (" << r << ", " << c << ") is not zero";
}

// Compares (R x K) * (K x C) against a plain triple loop.
template <typename T, int R, int K, int C>
void expectMultiplyMatchesReference(TestRandom& rng)
{
    const auto a = makeRandomMatrix<T, R, K>(rng);
    const auto b = makeRandomMatrix<T, K, C>(rng);
    const auto product = a * b;

    Matrix<T, R, C> reference;
    for (int i = 0; i < R; ++i)
        for (int j = 0; j < C; ++j)
        {
            T sum = T{ 0 };
            for (int k = 0; k < K; ++k)
                sum += a(i, k) * b(k, j);
            reference(i, j) = sum;
        }

    EXPECT_TRUE(approxEqual(product, reference)) << R << "x" << K << " * " << K << "x" << C;
    expectPaddingIsZero(product);
}

template <typename T, int N>
void expectInverseRoundTrip(TestRandom& rng)
{
    auto m = makeRandomMatrix<T, N, N>(rng);
    for (int i = 0; i < N; ++i)
        m(i, i) += T{ 3 };   // keep it well conditioned

    const T tolerance = std::sqrt(std::numeric_limits<T>::epsilon());
    Matrix<T, N, N> inv;

    ASSERT_TRUE(tryInverse(m, inv)) << "N = " << N;
    EXPECT_TRUE(approxEqual(m * inv, Matrix<T, N, N>::identity(), tolerance)) << "N = " << N;
    EXPECT_TRUE(approxEqual(inv * m, Matrix<T, N, N>::identity(), tolerance)) << "N = " << N;
    expectPaddingIsZero(inv);
}

}   // namespace

// Layout guarantees the GPU boundary relies on.
static_assert(sizeof(Matrix<float, 4, 4>) == 64 && alignof(Matrix<float, 4, 4>) == 16);
static_assert(sizeof(Matrix<float, 3, 3>) == 48);
static_assert(sizeof(Matrix<float, 2, 2>) == 32);
static_assert(sizeof(Matrix<double, 4, 4>) == 128);

// Define a test fixture template
template <typename T>
class MatrixTest : public ::testing::Test
{
protected:
    const T zero = T{ 0 };
    const T one = T{ 1 };
    const T tolerance = std::numeric_limits<T>::epsilon() * T{ 64 };
    const T looseTolerance = std::sqrt(std::numeric_limits<T>::epsilon());
    TestRandom rng;
};

// Specify the types you want to test (must satisfy concepts::IsFloatingPoint)
using FloatingPointTypes = ::testing::Types<float, double>;
TYPED_TEST_SUITE(MatrixTest, FloatingPointTypes);

// ---- construction & layout --------------------------------------------------

TYPED_TEST(MatrixTest, ReportsBackend)
{
    ASSERT_NE(Vec4<TypeParam>::backend, nullptr);
    this->RecordProperty("backend", Vec4<TypeParam>::backend);
    GTEST_LOG_(INFO) << "Vec4 backend: " << Vec4<TypeParam>::backend;
}

TYPED_TEST(MatrixTest, DefaultConstructorIsZero)
{
    Matrix<TypeParam, 3, 3> m;

    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r)
            EXPECT_EQ(m(r, c), this->zero);
    expectPaddingIsZero(m);
}

TYPED_TEST(MatrixTest, InitializerListIsRowMajor)
{
    Matrix<TypeParam, 2, 3> m{ 1, 2, 3, 4, 5, 6 };

    EXPECT_EQ(m(0, 0), TypeParam{ 1 });
    EXPECT_EQ(m(0, 1), TypeParam{ 2 });
    EXPECT_EQ(m(0, 2), TypeParam{ 3 });
    EXPECT_EQ(m(1, 0), TypeParam{ 4 });
    EXPECT_EQ(m(1, 1), TypeParam{ 5 });
    EXPECT_EQ(m(1, 2), TypeParam{ 6 });
}

TYPED_TEST(MatrixTest, StorageIsColumnMajorAndPadded)
{
    Matrix<TypeParam, 2, 2> m{ 1, 2, 3, 4 };
    const TypeParam* d = m.data();

    // column 0 = (1, 3), column 1 = (2, 4), each padded to 4 elements
    EXPECT_EQ(d[0], TypeParam{ 1 });
    EXPECT_EQ(d[1], TypeParam{ 3 });
    EXPECT_EQ(d[2], this->zero);
    EXPECT_EQ(d[3], this->zero);
    EXPECT_EQ(d[4], TypeParam{ 2 });
    EXPECT_EQ(d[5], TypeParam{ 4 });
    EXPECT_EQ(d[6], this->zero);
    EXPECT_EQ(d[7], this->zero);
}

TYPED_TEST(MatrixTest, ZeroFactory)
{
    EXPECT_EQ((Matrix<TypeParam, 4, 4>::zero()), (Matrix<TypeParam, 4, 4>{}));
}

TYPED_TEST(MatrixTest, IdentityFactory)
{
    const auto id = Matrix<TypeParam, 4, 4>::identity();

    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            EXPECT_EQ(id(r, c), r == c ? this->one : this->zero);
}

TYPED_TEST(MatrixTest, ElementAccessIsWritable)
{
    Matrix<TypeParam, 3, 3> m;
    m(1, 2) = TypeParam{ 9 };

    EXPECT_EQ(m(1, 2), TypeParam{ 9 });
    EXPECT_EQ(m.data()[2 * 4 + 1], TypeParam{ 9 });   // column 2, row 1
    expectPaddingIsZero(m);
}

TYPED_TEST(MatrixTest, ColumnAccessLoadsOneColumn)
{
    Matrix<TypeParam, 3, 3> m{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    TypeParam out[4];

    m.col(1).store(out);

    EXPECT_EQ(out[0], TypeParam{ 2 });
    EXPECT_EQ(out[1], TypeParam{ 5 });
    EXPECT_EQ(out[2], TypeParam{ 8 });
    EXPECT_EQ(out[3], this->zero);
}

TYPED_TEST(MatrixTest, EqualityOperator)
{
    Matrix<TypeParam, 2, 2> a{ 1, 2, 3, 4 };
    Matrix<TypeParam, 2, 2> b{ 1, 2, 3, 4 };
    Matrix<TypeParam, 2, 2> c{ 1, 2, 3, 5 };

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);
}

TYPED_TEST(MatrixTest, ApproxEqual)
{
    Matrix<TypeParam, 2, 2> a{ 1, 2, 3, 4 };
    Matrix<TypeParam, 2, 2> b{ 1, 2, 3, TypeParam{ 4.001 } };

    EXPECT_FALSE(approxEqual(a, b));
    EXPECT_TRUE(approxEqual(a, b, TypeParam{ 0.01 }));
}

// ---- element-wise arithmetic ------------------------------------------------

TYPED_TEST(MatrixTest, AdditionOperator)
{
    Matrix<TypeParam, 3, 4> a = makeRandomMatrix<TypeParam, 3, 4>(this->rng);
    Matrix<TypeParam, 3, 4> b = makeRandomMatrix<TypeParam, 3, 4>(this->rng);

    const auto sum = a + b;

    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            EXPECT_NEAR(sum(r, c), a(r, c) + b(r, c), this->tolerance);
    expectPaddingIsZero(sum);
}

TYPED_TEST(MatrixTest, SubtractionOperator)
{
    Matrix<TypeParam, 3, 4> a = makeRandomMatrix<TypeParam, 3, 4>(this->rng);
    Matrix<TypeParam, 3, 4> b = makeRandomMatrix<TypeParam, 3, 4>(this->rng);

    const auto difference = a - b;

    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            EXPECT_NEAR(difference(r, c), a(r, c) - b(r, c), this->tolerance);
    expectPaddingIsZero(difference);
}

TYPED_TEST(MatrixTest, NegationOperator)
{
    Matrix<TypeParam, 3, 3> m = makeRandomMatrix<TypeParam, 3, 3>(this->rng);

    const auto negated = -m;

    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            EXPECT_EQ(negated(r, c), -m(r, c));
    expectPaddingIsZero(negated);
}

TYPED_TEST(MatrixTest, ScalarMultiplicationOperator)
{
    Matrix<TypeParam, 2, 3> m = makeRandomMatrix<TypeParam, 2, 3>(this->rng);
    const TypeParam scalar = TypeParam{ 2.5 };

    const auto result = m * scalar;

    for (int r = 0; r < 2; ++r)
        for (int c = 0; c < 3; ++c)
            EXPECT_NEAR(result(r, c), m(r, c) * scalar, this->tolerance);
    expectPaddingIsZero(result);
}

TYPED_TEST(MatrixTest, ScalarMultiplicationOperatorCommutative)
{
    Matrix<TypeParam, 2, 3> m = makeRandomMatrix<TypeParam, 2, 3>(this->rng);
    const TypeParam scalar = TypeParam{ 2.5 };

    EXPECT_EQ(scalar * m, m * scalar);
}

TYPED_TEST(MatrixTest, TransposeSwapsDimensionsAndElements)
{
    Matrix<TypeParam, 2, 3> m{ 1, 2, 3, 4, 5, 6 };

    const Matrix<TypeParam, 3, 2> t = m.transposed();

    EXPECT_EQ(t, (Matrix<TypeParam, 3, 2>{ 1, 4, 2, 5, 3, 6 }));
    expectPaddingIsZero(t);
    EXPECT_EQ(t.transposed(), m);
}

// ---- matrix multiplication --------------------------------------------------

TYPED_TEST(MatrixTest, MultiplyKnownValues2x2)
{
    Matrix<TypeParam, 2, 2> a{ 1, 2, 3, 4 };
    Matrix<TypeParam, 2, 2> b{ 5, 6, 7, 8 };

    EXPECT_EQ(a * b, (Matrix<TypeParam, 2, 2>{ 19, 22, 43, 50 }));
}

TYPED_TEST(MatrixTest, MultiplyKnownValuesNonSquare)
{
    Matrix<TypeParam, 2, 3> a{ 1, 2, 3, 4, 5, 6 };
    Matrix<TypeParam, 3, 2> b{ 7, 8, 9, 10, 11, 12 };

    const Matrix<TypeParam, 2, 2> product = a * b;

    EXPECT_EQ(product, (Matrix<TypeParam, 2, 2>{ 58, 64, 139, 154 }));
}

TYPED_TEST(MatrixTest, MultiplyMatrixByVector)
{
    Matrix<TypeParam, 3, 3> m{ 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    Matrix<TypeParam, 3, 1> v{ 1, 0, -1 };

    const Matrix<TypeParam, 3, 1> result = m * v;

    EXPECT_EQ(result, (Matrix<TypeParam, 3, 1>{ -2, -2, -2 }));
}

TYPED_TEST(MatrixTest, MultiplyByIdentityIsNoOp)
{
    const auto m = makeRandomMatrix<TypeParam, 3, 3>(this->rng);
    const auto id = Matrix<TypeParam, 3, 3>::identity();

    EXPECT_TRUE(approxEqual(id * m, m));
    EXPECT_TRUE(approxEqual(m * id, m));
}

TYPED_TEST(MatrixTest, MultiplyMatchesReferenceSquare)
{
    expectMultiplyMatchesReference<TypeParam, 1, 1, 1>(this->rng);
    expectMultiplyMatchesReference<TypeParam, 2, 2, 2>(this->rng);
    expectMultiplyMatchesReference<TypeParam, 3, 3, 3>(this->rng);
    expectMultiplyMatchesReference<TypeParam, 4, 4, 4>(this->rng);
}

TYPED_TEST(MatrixTest, MultiplyMatchesReferenceMatrixVector)
{
    expectMultiplyMatchesReference<TypeParam, 2, 2, 1>(this->rng);
    expectMultiplyMatchesReference<TypeParam, 3, 3, 1>(this->rng);
    expectMultiplyMatchesReference<TypeParam, 3, 4, 1>(this->rng);
    expectMultiplyMatchesReference<TypeParam, 4, 4, 1>(this->rng);
}

TYPED_TEST(MatrixTest, MultiplyMatchesReferenceNonSquare)
{
    expectMultiplyMatchesReference<TypeParam, 2, 3, 4>(this->rng);
    expectMultiplyMatchesReference<TypeParam, 4, 1, 4>(this->rng);
    expectMultiplyMatchesReference<TypeParam, 1, 4, 3>(this->rng);
    expectMultiplyMatchesReference<TypeParam, 3, 2, 2>(this->rng);
}

TYPED_TEST(MatrixTest, MultiplyIsAssociative)
{
    const auto a = makeRandomMatrix<TypeParam, 4, 4>(this->rng);
    const auto b = makeRandomMatrix<TypeParam, 4, 4>(this->rng);
    const auto c = makeRandomMatrix<TypeParam, 4, 4>(this->rng);

    EXPECT_TRUE(approxEqual((a * b) * c, a * (b * c), this->looseTolerance));
}

TYPED_TEST(MatrixTest, TransposeOfProduct)
{
    const auto a = makeRandomMatrix<TypeParam, 3, 4>(this->rng);
    const auto b = makeRandomMatrix<TypeParam, 4, 2>(this->rng);

    // (A * B)^T == B^T * A^T
    EXPECT_TRUE(approxEqual((a * b).transposed(), b.transposed() * a.transposed(), this->looseTolerance));
}

// ---- determinant & inverse --------------------------------------------------

TYPED_TEST(MatrixTest, DeterminantKnownValues)
{
    EXPECT_EQ(determinant(Matrix<TypeParam, 1, 1>{ 5 }), TypeParam{ 5 });
    EXPECT_EQ(determinant(Matrix<TypeParam, 2, 2>{ 1, 2, 3, 4 }), TypeParam{ -2 });
    EXPECT_EQ(determinant(Matrix<TypeParam, 3, 3>{ 2, 0, 0, 0, 3, 0, 0, 0, 4 }), TypeParam{ 24 });
    EXPECT_EQ(determinant(Matrix<TypeParam, 3, 3>{ 1, 2, 3, 0, 1, 4, 5, 6, 0 }), TypeParam{ 1 });
    EXPECT_EQ(
        determinant(Matrix<TypeParam, 4, 4>{ 1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4 }), TypeParam{ 24 }
    );
}

TYPED_TEST(MatrixTest, DeterminantOfIdentityIsOne)
{
    EXPECT_EQ(determinant(Matrix<TypeParam, 2, 2>::identity()), this->one);
    EXPECT_EQ(determinant(Matrix<TypeParam, 3, 3>::identity()), this->one);
    EXPECT_EQ(determinant(Matrix<TypeParam, 4, 4>::identity()), this->one);
}

TYPED_TEST(MatrixTest, DeterminantOfSingularIsZero)
{
    // rows 1 and 2 are proportional
    EXPECT_EQ(determinant(Matrix<TypeParam, 3, 3>{ 1, 2, 3, 2, 4, 6, 0, 1, 1 }), this->zero);
}

TYPED_TEST(MatrixTest, DeterminantOfTransposeIsEqual)
{
    const auto m = makeRandomMatrix<TypeParam, 4, 4>(this->rng);

    EXPECT_NEAR(determinant(m), determinant(m.transposed()), this->looseTolerance);
}

TYPED_TEST(MatrixTest, InverseKnownValues3x3)
{
    Matrix<TypeParam, 3, 3> m{ 1, 2, 3, 0, 1, 4, 5, 6, 0 };
    Matrix<TypeParam, 3, 3> inv;

    ASSERT_TRUE(tryInverse(m, inv));

    EXPECT_TRUE(approxEqual(inv, Matrix<TypeParam, 3, 3>{ -24, 18, 5, 20, -15, -4, -5, 4, 1 }, this->looseTolerance));
}

TYPED_TEST(MatrixTest, InverseRoundTrip)
{
    expectInverseRoundTrip<TypeParam, 1>(this->rng);
    expectInverseRoundTrip<TypeParam, 2>(this->rng);
    expectInverseRoundTrip<TypeParam, 3>(this->rng);
    expectInverseRoundTrip<TypeParam, 4>(this->rng);
}

TYPED_TEST(MatrixTest, TryInverseFailsOnSingularAndLeavesOutputUntouched)
{
    Matrix<TypeParam, 3, 3> singular{ 1, 2, 3, 2, 4, 6, 0, 1, 1 };
    Matrix<TypeParam, 3, 3> out = Matrix<TypeParam, 3, 3>::identity();

    EXPECT_FALSE(tryInverse(singular, out));
    EXPECT_EQ(out, (Matrix<TypeParam, 3, 3>::identity()));
}

TYPED_TEST(MatrixTest, InverseOfSingularIsZeroMatrix)
{
    Matrix<TypeParam, 2, 2> singular{ 1, 2, 2, 4 };

    EXPECT_EQ(inverse(singular), (Matrix<TypeParam, 2, 2>::zero()));
}

TYPED_TEST(MatrixTest, InverseOfInverseIsOriginal)
{
    auto m = makeRandomMatrix<TypeParam, 4, 4>(this->rng);
    for (int i = 0; i < 4; ++i)
        m(i, i) += TypeParam{ 3 };

    EXPECT_TRUE(approxEqual(inverse(inverse(m)), m, this->looseTolerance));
}

// ---- Matrix<T,3,4> as an affine transform -----------------------------------

TYPED_TEST(MatrixTest, AffineTransformsPointsAndDirections)
{
    // uniform scale 2 + translation (5, 6, 7)
    Matrix<TypeParam, 3, 4> m{ 2, 0, 0, 5, 0, 2, 0, 6, 0, 0, 2, 7 };

    const Matrix<TypeParam, 3, 1> point = m * Matrix<TypeParam, 4, 1>{ 1, 2, 3, 1 };
    const Matrix<TypeParam, 3, 1> direction = m * Matrix<TypeParam, 4, 1>{ 1, 2, 3, 0 };

    EXPECT_EQ(point, (Matrix<TypeParam, 3, 1>{ 7, 10, 13 }));       // translation applied
    EXPECT_EQ(direction, (Matrix<TypeParam, 3, 1>{ 2, 4, 6 }));     // translation ignored
}

TYPED_TEST(MatrixTest, Matrix4x4TranslationMovesPoints)
{
    Matrix<TypeParam, 4, 4> m{ 1, 0, 0, 5, 0, 1, 0, 6, 0, 0, 1, 7, 0, 0, 0, 1 };

    const Matrix<TypeParam, 4, 1> p = m * Matrix<TypeParam, 4, 1>{ 1, 2, 3, 1 };

    EXPECT_EQ(p, (Matrix<TypeParam, 4, 1>{ 6, 8, 10, 1 }));
}

TYPED_TEST(MatrixTest, InverseAffineRoundTrip)
{
    Matrix<TypeParam, 3, 4> m = makeRandomMatrix<TypeParam, 3, 4>(this->rng);
    for (int i = 0; i < 3; ++i)
        m(i, i) += TypeParam{ 3 };

    Matrix<TypeParam, 3, 4> inv;
    ASSERT_TRUE(tryInverseAffine(m, inv));

    const Matrix<TypeParam, 3, 1> p = m * Matrix<TypeParam, 4, 1>{ TypeParam{ 0.5 }, -1, 2, 1 };
    const Matrix<TypeParam, 3, 1> back = inv * Matrix<TypeParam, 4, 1>{ p(0, 0), p(1, 0), p(2, 0), 1 };

    EXPECT_NEAR(back(0, 0), TypeParam{ 0.5 }, this->looseTolerance);
    EXPECT_NEAR(back(1, 0), TypeParam{ -1 }, this->looseTolerance);
    EXPECT_NEAR(back(2, 0), TypeParam{ 2 }, this->looseTolerance);
    expectPaddingIsZero(inv);
}

TYPED_TEST(MatrixTest, InverseAffineFailsOnSingularLinearPart)
{
    Matrix<TypeParam, 3, 4> m{ 1, 2, 3, 9, 2, 4, 6, 9, 0, 1, 1, 9 };
    Matrix<TypeParam, 3, 4> out;

    EXPECT_FALSE(tryInverseAffine(m, out));
}

TYPED_TEST(MatrixTest, ComposeMatchesPromoted4x4Product)
{
    const auto a = makeRandomMatrix<TypeParam, 3, 4>(this->rng);
    const auto b = makeRandomMatrix<TypeParam, 3, 4>(this->rng);

    // promote to 4x4 by appending the implicit row 0 0 0 1
    const auto promote = [](const Matrix<TypeParam, 3, 4>& x)
    {
        auto result = Matrix<TypeParam, 4, 4>::identity();
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                result(r, c) = x(r, c);
        return result;
    };

    const Matrix<TypeParam, 4, 4> expected = promote(a) * promote(b);
    const Matrix<TypeParam, 3, 4> composed = compose(a, b);

    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 4; ++c)
            EXPECT_NEAR(composed(r, c), expected(r, c), this->looseTolerance);
    expectPaddingIsZero(composed);
}

}   // namespace gp::math::tests