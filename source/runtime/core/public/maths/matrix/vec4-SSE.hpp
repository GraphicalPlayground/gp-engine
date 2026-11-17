// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once
#include <xmmintrin.h>

namespace gp::math
{

template <>
struct Vec4<float>
{
    static constexpr const char* backend = "SSE";

    __m128 v;

    Vec4() : v(_mm_setzero_ps()) {}
    explicit Vec4(__m128 r) : v(r) {}
    Vec4(float x, float y, float z, float w) : v(_mm_set_ps(w, z, y, x)) {}

    static Vec4 zero()                  { return Vec4(_mm_setzero_ps()); }
    static Vec4 splat(float s)          { return Vec4(_mm_set1_ps(s)); }
    static Vec4 load(const float* p)    { return Vec4(_mm_loadu_ps(p)); }
    void store(float* p) const          { _mm_storeu_ps(p, v); }

    template <int I> Vec4 broadcast() const
    {
        static_assert(I >= 0 && I < 4, "lane index out of range");
        return Vec4(_mm_shuffle_ps(v, v, _MM_SHUFFLE(I, I, I, I)));
    }

    friend Vec4 operator+(Vec4 a, Vec4 b) { return Vec4(_mm_add_ps(a.v, b.v)); }
    friend Vec4 operator-(Vec4 a, Vec4 b) { return Vec4(_mm_sub_ps(a.v, b.v)); }
    friend Vec4 operator*(Vec4 a, Vec4 b) { return Vec4(_mm_mul_ps(a.v, b.v)); }
    friend Vec4 operator-(Vec4 a)         { return Vec4(_mm_xor_ps(a.v, _mm_set1_ps(-0.0f))); }
};

} // namespace gp::math
