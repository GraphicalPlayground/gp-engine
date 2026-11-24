// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "maths/matrix/Vec4.hpp"
#include "concepts/Concepts.hpp"

#include <cassert>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace gp::math
{

template <concepts::IsFloatingPoint T, int R, int C>
class Matrix {
    static_assert(R >= 1 && R <= 4 && C >= 1 && C <= 4, "Matrix supports 1..4 rows and columns");
    // lets Matrix<T,R,C2> be filled by Matrix<T,R,C>::operator*
    template <concepts::IsFloatingPoint U, int, int> friend class Matrix;

public:
    using value_type = T;
    using Vec        = Vec4<T>;
    static constexpr int rows = R;
    static constexpr int cols = C;

    Matrix() = default;   // all zeros, including padding

    // Row-major initializer list:
    //   Matrix<float,2,2>{ 1, 2,
    //                      3, 4 }
    Matrix(std::initializer_list<T> rowMajor) {
        assert(static_cast<int>(rowMajor.size()) == R * C);
        int i = 0;
        for (T x : rowMajor) { d_[i % C][i / C] = x; ++i; }
    }

    [[nodiscard]] static Matrix zero() { return Matrix(); }

    [[nodiscard]] static Matrix identity() {
        static_assert(R == C, "identity needs a square matrix");
        Matrix m;
        for (int i = 0; i < R; ++i) m.d_[i][i] = T(1);
        return m;
    }

    // Element access. Only real rows are reachable, so padding can't be corrupted.
    T& operator()(int r, int c)       { assert(r >= 0 && r < R && c >= 0 && c < C); return d_[c][r]; }
    T  operator()(int r, int c) const { assert(r >= 0 && r < R && c >= 0 && c < C); return d_[c][r]; }

    // One column as a SIMD register.
    [[nodiscard]] Vec col(int j) const { return Vec::load(d_[j]); }

    // Raw column-major data: C columns of 4 elements (stride 4).
    [[nodiscard]] const T* data() const { return &d_[0][0]; }

    // Exact comparison of the real elements (use approxEqual for tolerance).
    [[nodiscard]] bool operator==(const Matrix& other) const {
        for (int c = 0; c < C; ++c)
            for (int r = 0; r < R; ++r)
                if (d_[c][r] != other.d_[c][r]) return false;
        return true;
    }

    // ---- arithmetic -------------------------------------------------------

    [[nodiscard]] Matrix operator+(const Matrix& b) const {
        Matrix o;
        for (int j = 0; j < C; ++j) o.setCol(j, col(j) + b.col(j));
        return o;
    }
    [[nodiscard]] Matrix operator-(const Matrix& b) const {
        Matrix o;
        for (int j = 0; j < C; ++j) o.setCol(j, col(j) - b.col(j));
        return o;
    }
    [[nodiscard]] Matrix operator-() const {
        Matrix o;
        for (int j = 0; j < C; ++j) o.setCol(j, -col(j));
        return o;
    }
    [[nodiscard]] Matrix operator*(T s) const {
        const Vec sv = Vec::splat(s);
        Matrix o;
        for (int j = 0; j < C; ++j) o.setCol(j, col(j) * sv);
        return o;
    }
    [[nodiscard]] friend Matrix operator*(T s, const Matrix& m) { return m * s; }

    template <int C2>
    [[nodiscard]] Matrix<T, R, C2> operator*(const Matrix<T, C, C2>& b) const {
        const Vec a0 = colOrZero<0>();
        const Vec a1 = colOrZero<1>();
        const Vec a2 = colOrZero<2>();
        const Vec a3 = colOrZero<3>();

        Matrix<T, R, C2> out;
        for (int j = 0; j < C2; ++j) {
            const Vec bj = b.col(j);
            Vec acc = a0 * bj.template broadcast<0>();
            if constexpr (C > 1) acc = acc + a1 * bj.template broadcast<1>();
            if constexpr (C > 2) acc = acc + a2 * bj.template broadcast<2>();
            if constexpr (C > 3) acc = acc + a3 * bj.template broadcast<3>();
            out.setCol(j, acc);
        }
        return out;
    }

    [[nodiscard]] Matrix<T, C, R> transposed() const {
        Matrix<T, C, R> t;
        for (int c = 0; c < C; ++c)
            for (int r = 0; r < R; ++r)
                t(c, r) = (*this)(r, c);
        return t;
    }

private:
    template <int J>
    Vec colOrZero() const {
        if constexpr (J < C) return col(J);
        else                 return Vec::zero();
    }

    // Private on purpose: the Vec must have zero padding lanes. Operators in this
    // class guarantee that; outside code goes through operator().
    void setCol(int j, const Vec& v) { v.store(d_[j]); }

    alignas(sizeof(T) * 4) T d_[C][4] = {};
};

// Default tolerance scales with the type (float and double differ by ~9 orders of magnitude).
template <concepts::IsFloatingPoint T, int R, int C>
[[nodiscard]] bool approxEqual(const Matrix<T, R, C>& a, const Matrix<T, R, C>& b,
                               T eps = std::numeric_limits<T>::epsilon() * T(64)) {
    for (int c = 0; c < C; ++c)
        for (int r = 0; r < R; ++r)
            if (std::fabs(a(r, c) - b(r, c)) > eps) return false;
    return true;
}

}   // namespace gp::math