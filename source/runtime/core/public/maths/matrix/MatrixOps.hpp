// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "Matrix.hpp"

namespace gp::math
{

// The (N-1)x(N-1) matrix left after removing row skipR and column skipC.
template <concepts::IsFloatingPoint T, int N>
[[nodiscard]] Matrix<T, N - 1, N - 1> minorMatrix(const Matrix<T, N, N>& m, int skipR, int skipC) {
    static_assert(N >= 2, "no minor for a 1x1 matrix");
    Matrix<T, N - 1, N - 1> out;
    for (int c = 0, oc = 0; c < N; ++c) {
        if (c == skipC) continue;
        for (int r = 0, orow = 0; r < N; ++r) {
            if (r == skipR) continue;
            out(orow++, oc) = m(r, c);
        }
        ++oc;
    }
    return out;
}

template <concepts::IsFloatingPoint T, int N>
[[nodiscard]] T determinant(const Matrix<T, N, N>& m) {
    if constexpr (N == 1) {
        return m(0, 0);
    } else if constexpr (N == 2) {
        return m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0);
    } else {
        T sum = T(0);                              // expand along column 0
        for (int r = 0; r < N; ++r) {
            const T sign = (r & 1) ? T(-1) : T(1);
            sum += sign * m(r, 0) * determinant(minorMatrix(m, r, 0));
        }
        return sum;
    }
}

// Returns false (and leaves `out` untouched) when |det| <= eps.
// The default only rejects (near-)exact zeros; pass a larger eps for scale-aware checks.
template <concepts::IsFloatingPoint T, int N>
[[nodiscard]] bool tryInverse(const Matrix<T, N, N>& m, Matrix<T, N, N>& out,
                              T eps = std::numeric_limits<T>::min()) {
    const T det = determinant(m);
    if (std::fabs(det) <= eps) return false;
    const T invDet = T(1) / det;

    if constexpr (N == 1) {
        out(0, 0) = invDet;
    } else {
        Matrix<T, N, N> r;
        for (int row = 0; row < N; ++row)
            for (int c = 0; c < N; ++c) {
                const T sign = ((row + c) & 1) ? T(-1) : T(1);
                // inverse = adjugate / det, and adjugate is the transposed cofactor matrix
                r(c, row) = sign * determinant(minorMatrix(m, row, c)) * invDet;
            }
        out = r;
    }
    return true;
}

// Convenience: returns the zero matrix if singular. Use tryInverse when you need to know.
template <concepts::IsFloatingPoint T, int N>
[[nodiscard]] Matrix<T, N, N> inverse(const Matrix<T, N, N>& m) {
    Matrix<T, N, N> out;
    (void)tryInverse(m, out);
    return out;
}

// ---- Matrix<T,3,4> as an affine transform ---------------------------------
// Columns 0..2 are the linear part, column 3 is the translation (implicit last row 0 0 0 1).
// Transform a point:      Matrix<T,3,1> p = m * Matrix<T,4,1>{ x, y, z, 1 };
// Transform a direction:  Matrix<T,3,1> d = m * Matrix<T,4,1>{ x, y, z, 0 };

// Cheap inverse: invert the 3x3 part, translation becomes -(inv * t).
template <concepts::IsFloatingPoint T>
[[nodiscard]] bool tryInverseAffine(const Matrix<T, 3, 4>& m, Matrix<T, 3, 4>& out) {
    Matrix<T, 3, 3> lin;
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r) lin(r, c) = m(r, c);

    Matrix<T, 3, 3> inv;
    if (!tryInverse(lin, inv)) return false;

    const Matrix<T, 3, 1> t{ m(0, 3), m(1, 3), m(2, 3) };
    const Matrix<T, 3, 1> nt = inv * t;

    Matrix<T, 3, 4> r;
    for (int c = 0; c < 3; ++c)
        for (int row = 0; row < 3; ++row) r(row, c) = inv(row, c);
    for (int row = 0; row < 3; ++row) r(row, 3) = -nt(row, 0);
    out = r;
    return true;
}

// a * b as if both were 4x4 with last row 0 0 0 1.
template <concepts::IsFloatingPoint T>
[[nodiscard]] Matrix<T, 3, 4> compose(const Matrix<T, 3, 4>& a, const Matrix<T, 3, 4>& b) {
    Matrix<T, 3, 4> out;
    for (int c = 0; c < 4; ++c) {
        const T w = (c == 3) ? T(1) : T(0);        // translation column picks up a's translation
        const Matrix<T, 3, 1> col = a * Matrix<T, 4, 1>{ b(0, c), b(1, c), b(2, c), w };
        for (int r = 0; r < 3; ++r) out(r, c) = col(r, 0);
    }
    return out;
}

}   // namespace gp::math