// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once
#include "concepts/Concepts.hpp"

namespace gp::math
{

template <concepts::IsFloatingPoint T>
struct Vec4
{
    static constexpr const char* backend = "Scalar";

    alignas(sizeof(T) * 4) T f[4];

    Vec4() : f{T(0), T(0), T(0), T(0)} {}
    Vec4(T x, T y, T z, T w) : f{x, y, z, w} {}

    static Vec4 zero()               { return Vec4(); }
    static Vec4 splat(T s)           { return Vec4(s, s, s, s); }
    static Vec4 load(const T* p)     { return Vec4(p[0], p[1], p[2], p[3]); }
    void store(T* p) const           { p[0] = f[0]; p[1] = f[1]; p[2] = f[2]; p[3] = f[3]; }

    // Copies lane I into all four lanes.
    template <int I> Vec4 broadcast() const {
        static_assert(I >= 0 && I < 4, "lane index out of range");
        return splat(f[I]);
    }

    friend Vec4 operator+(const Vec4& a, const Vec4& b)
    {
        return Vec4(a.f[0] + b.f[0], a.f[1] + b.f[1], a.f[2] + b.f[2], a.f[3] + b.f[3]);
    }
    
    friend Vec4 operator-(const Vec4& a, const Vec4& b)
    {
        return Vec4(a.f[0] - b.f[0], a.f[1] - b.f[1], a.f[2] - b.f[2], a.f[3] - b.f[3]);
    }
    
    friend Vec4 operator*(const Vec4& a, const Vec4& b)
    {
        return Vec4(a.f[0] * b.f[0], a.f[1] * b.f[1], a.f[2] * b.f[2], a.f[3] * b.f[3]);
    }
    
    friend Vec4 operator-(const Vec4& a) { return Vec4(-a.f[0], -a.f[1], -a.f[2], -a.f[3]); }
};

} // namespace gp::math