/// \file    MathCommon.hlsli
/// \brief   Core scalar and vector math utilities. All functions are pure (no side effects),
///          branchless where possible, and annotated with their numerical domains.
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_MATH_COMMON_HLSLI
#define GP_MATH_COMMON_HLSLI

// ─── Scalar Utilities ─────────────────────────────────────────────────────────────────────────

/// Square of x.
Float32 GP_Sq(Float32 x)    { return x * x; }
Float2  GP_Sq(Float2 x)     { return x * x; }
Float3  GP_Sq(Float3 x)     { return x * x; }
Float4  GP_Sq(Float4 x)     { return x * x; }

/// Cube of x.
Float32 GP_Cb(Float32 x)    { return x * x * x; }

/// Remap x from [inMin, inMax] to [outMin, outMax]. Does not clamp.
Float32 GP_Remap(Float32 x, Float32 inMin, Float32 inMax, Float32 outMin, Float32 outMax)
{
    return outMin + (x - inMin) / (inMax - inMin) * (outMax - outMin);
}

/// Remap and clamp to output range.
Float32 GP_RemapClamped(Float32 x, Float32 inMin, Float32 inMax, Float32 outMin, Float32 outMax)
{
    return clamp(GP_Remap(x, inMin, inMax, outMin, outMax), outMin, outMax);
}

/// Smooth step without branching. Equivalent to GLSL smoothstep but explicit.
/// Returns 0 when x <= edge0, 1 when x >= edge1, smooth Hermite in between.
Float32 GP_SmoothStep(Float32 edge0, Float32 edge1, Float32 x)
{
    Float32 t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

/// Quintic smooth step (C2 continuity — zero first and second derivative at edges).
/// Slower than GP_SmoothStep but avoids tangent discontinuity for noise.
Float32 GP_SmootherStep(Float32 edge0, Float32 edge1, Float32 x)
{
    Float32 t = clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

/// Power of 2 check (UInt32). Useful for mip dimension validation.
bool GP_IsPow2(UInt32 x) { return x > 0u && (x & (x - 1u)) == 0u; }

/// Integer log2 (floor). Returns the position of the highest set bit.
UInt32 GP_Log2Floor(UInt32 x)
{
    return firstbithigh(x);
}

/// Safe normalize — returns (0,0,0) instead of NaN when length is 0.
Float3 GP_SafeNormalize(Float3 v)
{
    Float32 lenSq = dot(v, v);
    return lenSq > GP_EPSILON ? v * rsqrt(lenSq) : (Float3)0;
}

Float2 GP_SafeNormalize(Float2 v)
{
    Float32 lenSq = dot(v, v);
    return lenSq > GP_EPSILON ? v * rsqrt(lenSq) : (Float2)0;
}

/// Luminance of a linear RGB color (Rec. 709 coefficients).
Float32 GP_Luminance(Float3 linearRGB)
{
    return dot(linearRGB, Float3(0.2126f, 0.7152f, 0.0722f));
}

/// Luminance using Rec. 2020 primaries (HDR / wide gamut).
Float32 GP_Luminance2020(Float3 linearRGB)
{
    return dot(linearRGB, Float3(0.2627f, 0.6780f, 0.0593f));
}

/// Max component of a float3.
Float32 GP_MaxComponent(Float3 v) { return max(v.x, max(v.y, v.z)); }
Float32 GP_MaxComponent(Float4 v) { return max(max(v.x, v.y), max(v.z, v.w)); }

/// Min component of a float3.
Float32 GP_MinComponent(Float3 v) { return min(v.x, min(v.y, v.z)); }

/// Linear interpolation (alias for lerp, explicit for readability).
Float32 GP_Lerp(Float32 a, Float32 b, Float32 t) { return lerp(a, b, t); }
Float3  GP_Lerp(Float3 a, Float3 b, Float32 t)  { return lerp(a, b, t); }
Float4  GP_Lerp(Float4 a, Float4 b, Float32 t)  { return lerp(a, b, t); }

/// Bilinear interpolation. s and t are [0,1] blend weights.
Float4 GP_BiLerp(Float4 a, Float4 b, Float4 c, Float4 d, Float32 s, Float32 t)
{
    return lerp(lerp(a, b, s), lerp(c, d, s), t);
}

/// Positive modulo (GLSL mod semantics — result is always >= 0).
Float32 GP_Mod(Float32 x, Float32 y)
{
    return x - y * floor(x / y);
}

Float2 GP_Mod(Float2 x, Float2 y)
{
    return x - y * floor(x / y);
}

/// Wrap a value into [0, 1) — equivalent to frac() but with correct negative behavior.
Float32 GP_Frac01(Float32 x) { return GP_Mod(x, 1.0f); }

// ─── Angle Utilities ──────────────────────────────────────────────────────────────────────────

Float32 GP_DegToRad(Float32 deg) { return deg * (GP_PI / 180.0f); }
Float32 GP_RadToDeg(Float32 rad) { return rad * (180.0f / GP_PI); }

/// Wrap angle to [-PI, PI].
Float32 GP_WrapAngle(Float32 a)
{
    return a - GP_TWO_PI * floor((a + GP_PI) / GP_TWO_PI);
}

// ─── Vector Construction ──────────────────────────────────────────────────────────────────────

/// Build an orthonormal basis (tangent frame) from a single normal vector.
/// Based on Frisvad (2012) "Building an Orthonormal Basis from a 3D Unit Vector".
/// Numerically stable variant from Duff et al. (2017).
void GP_BuildTangentFrame(Float3 n, out Float3 tangent, out Float3 bitangent)
{
    Float32 sign = n.z >= 0.0f ? 1.0f : -1.0f;
    Float32 a    = -1.0f / (sign + n.z);
    Float32 b    = n.x * n.y * a;

    tangent   = Float3(1.0f + sign * n.x * n.x * a, sign * b, -sign * n.x);
    bitangent = Float3(b, sign + n.y * n.y * a, -n.y);
}

/// Transform a direction from tangent space to world space.
/// \param n  World-space normal (unit length).
/// \param t  World-space tangent (unit length, from mesh data).
/// \param b  World-space bitangent (unit length). Use cross(n, t) * tangentSign if not stored.
/// \param v  Tangent-space vector to transform.
Float3 GP_TangentToWorld(Float3 n, Float3 t, Float3 b, Float3 v)
{
    return normalize(t * v.x + b * v.y + n * v.z);
}

/// Transform a direction from world space to tangent space.
Float3 GP_WorldToTangent(Float3 n, Float3 t, Float3 b, Float3 v)
{
    return Float3(dot(v, t), dot(v, b), dot(v, n));
}

// ─── Sphere / Hemisphere Utilities ───────────────────────────────────────────────────────────

/// Convert spherical coordinates (theta = polar, phi = azimuthal) to a Cartesian unit vector.
/// \param theta  Polar angle from the +Z axis [0, PI].
/// \param phi    Azimuthal angle in the XY plane [0, 2*PI].
Float3 GP_SphericalToCartesian(Float32 theta, Float32 phi)
{
    Float32 sinTheta, cosTheta, sinPhi, cosPhi;
    sincos(theta, sinTheta, cosTheta);
    sincos(phi,   sinPhi,   cosPhi);
    return Float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

/// Convert a Cartesian unit vector to spherical coordinates.
void GP_CartesianToSpherical(Float3 v, out Float32 theta, out Float32 phi)
{
    theta = acos(clamp(v.z, -1.0f, 1.0f));
    phi   = atan2(v.y, v.x);
    if (phi < 0.0f)
        phi += GP_TWO_PI;
}

/// Project a point on the unit sphere to a 2D panoramic UV [0,1].
Float2 GP_DirectionToEquirectUV(Float3 dir)
{
    Float32 phi, theta;
    GP_CartesianToSpherical(dir, theta, phi);
    return Float2(phi * GP_INV_TWO_PI, theta * GP_INV_PI);
}

/// Reconstruct a direction from equirectangular UV [0,1].
Float3 GP_EquirectUVToDirection(Float2 uv)
{
    Float32 phi   = uv.x * GP_TWO_PI;
    Float32 theta = uv.y * GP_PI;
    return GP_SphericalToCartesian(theta, phi);
}

// ─── Frustum / Depth Utilities ────────────────────────────────────────────────────────────────

/// Reconstruct world-space position from a depth buffer sample and inverse view-projection.
/// \param uv         Screen UV in [0, 1].
/// \param depth      Depth buffer value (0 = near, 1 = far for standard-Z; reversed for reversed-Z).
/// \param invViewProj Inverse view-projection matrix.
Float3 GP_ReconstructWorldPos(Float2 uv, Float32 depth, Float4x4 invViewProj)
{
    Float4 clipPos  = Float4(uv * 2.0f - 1.0f, depth, 1.0f);
    clipPos.y       = -clipPos.y;                           // DX clip space: Y is flipped vs UV
    Float4 worldPos = mul(invViewProj, clipPos);
    return worldPos.xyz / worldPos.w;
}

/// Reconstruct view-space position from UV + linear depth.
/// \param uv          Screen UV in [0, 1].
/// \param linearDepth View-space depth (positive, in world units).
/// \param invProj     Inverse projection matrix.
Float3 GP_ReconstructViewPos(Float2 uv, Float32 linearDepth, Float4x4 invProj)
{
    Float4 ndcPos    = Float4(uv * 2.0f - 1.0f, 0.0f, 1.0f);
    ndcPos.y         = -ndcPos.y;
    Float4 viewPos   = mul(invProj, ndcPos);
    viewPos.xy       /= viewPos.w;
    return Float3(viewPos.xy * linearDepth, linearDepth);
}

// ─── Piecewise Pow Approximations ────────────────────────────────────────────────────────────
// Fast approximations used in time-critical pixel shader paths.

/// Fast pow(x, 2.2) approximation for gamma encode. Max error ~0.3%.
Float3 GP_GammaPowApprox(Float3 x)
{
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

/// Fast exp2(x) — single-instruction on most GPU hardware.
Float32 GP_FastExp2(Float32 x) { return exp2(x); }

/// Fast log2(x) — single-instruction on most GPU hardware.
Float32 GP_FastLog2(Float32 x) { return log2(x); }

/// Fast pow via exp2/log2 — avoids the pow() intrinsic which some drivers mis-handle for x=0.
Float32 GP_FastPow(Float32 base, Float32 exp)
{
    return exp2(log2(abs(base)) * exp);
}

#endif // GP_MATH_COMMON_HLSLI
