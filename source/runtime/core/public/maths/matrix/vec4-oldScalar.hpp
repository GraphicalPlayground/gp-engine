// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com
#pragma once

namespace gp::math
{

inline constexpr const char* kBackendName = "Scalar";

struct MatrixVec4
{
    alignas(16) float f[4];

    MatrixVec4() : f{0.0f, 0.0f, 0.0f, 0.0f} {}
    MatrixVec4(float x, float y, float z, float w) : f{x, y, z, w} {}

    static MatrixVec4 zero()                { return MatrixVec4(); }
    static MatrixVec4 splat(float s)        { return MatrixVec4(s, s, s, s); }
    static MatrixVec4 load(const float* p)  { return MatrixVec4(p[0], p[1], p[2], p[3]); }
    void store(float* p) const        { p[0] = f[0]; p[1] = f[1]; p[2] = f[2]; p[3] = f[3]; }

    template <int I> MatrixVec4 broadcast() const {
        static_assert(I >= 0 && I < 4, "lane index out of range");
        return splat(f[I]);
    }

    friend MatrixVec4 operator+(const MatrixVec4& a, const MatrixVec4& b) {
        return MatrixVec4(a.f[0] + b.f[0], a.f[1] + b.f[1], a.f[2] + b.f[2], a.f[3] + b.f[3]);
    }
    friend MatrixVec4 operator-(const MatrixVec4& a, const MatrixVec4& b) {
        return MatrixVec4(a.f[0] - b.f[0], a.f[1] - b.f[1], a.f[2] - b.f[2], a.f[3] - b.f[3]);
    }
    friend MatrixVec4 operator*(const MatrixVec4& a, const MatrixVec4& b) {
        return MatrixVec4(a.f[0] * b.f[0], a.f[1] * b.f[1], a.f[2] * b.f[2], a.f[3] * b.f[3]);
    }
    friend MatrixVec4 operator-(const MatrixVec4& a) { return MatrixVec4(-a.f[0], -a.f[1], -a.f[2], -a.f[3]); }
};

} // namespace gp::math