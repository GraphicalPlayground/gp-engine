// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once
#include <arm_neon.h>

namespace gp::math {

template <>
struct Vec4<float> {
    static constexpr const char* backend = "NEON";

    float32x4_t v;

    Vec4() : v(vdupq_n_f32(0.0f)) {}
    explicit Vec4(float32x4_t r) : v(r) {}
    Vec4(float x, float y, float z, float w) {
        const float t[4] = {x, y, z, w};
        v = vld1q_f32(t);
    }

    static Vec4 zero()                  { return Vec4(vdupq_n_f32(0.0f)); }
    static Vec4 splat(float s)          { return Vec4(vdupq_n_f32(s)); }
    static Vec4 load(const float* p)    { return Vec4(vld1q_f32(p)); }
    void store(float* p) const          { vst1q_f32(p, v); }

    // Copies lane I into all four lanes (portable to ARMv7 and AArch64).
    template <int I> Vec4 broadcast() const {
        static_assert(I >= 0 && I < 4, "lane index out of range");
        return Vec4(vdupq_n_f32(vgetq_lane_f32(v, I)));
    }

    friend Vec4 operator+(Vec4 a, Vec4 b) { return Vec4(vaddq_f32(a.v, b.v)); }
    friend Vec4 operator-(Vec4 a, Vec4 b) { return Vec4(vsubq_f32(a.v, b.v)); }
    friend Vec4 operator*(Vec4 a, Vec4 b) { return Vec4(vmulq_f32(a.v, b.v)); }
    friend Vec4 operator-(Vec4 a)         { return Vec4(vnegq_f32(a.v)); }
};

}   // namespace gp::math
