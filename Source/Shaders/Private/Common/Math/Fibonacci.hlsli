/// \file    Fibonacci.hlsli
/// \brief   Fibonacci / golden ratio low-discrepancy sequences for importance sampling.
///          Used for hemisphere sampling in IBL prefiltering, SSAO, DDGI probe ray generation,
///          and any offline-style importance sampling pass running inside a compute shader.
///
/// \details The Fibonacci lattice is a quasi-random sequence that distributes N points on a
///          sphere/hemisphere with near-optimal coverage (minimizes discrepancy). It converges
///          faster than purely random sampling, making it ideal for a fixed sample count.
///
///          Key sequences provided:
///          - Spherical Fibonacci (Marques et al. 2013)  — N points on the unit sphere
///          - Hemisphere Fibonacci                        — N points on the upper hemisphere
///          - 2D Fibonacci lattice (square [0,1]^2)
///          - Radical inverse (Van der Corput)            — component for Hammersley
///          - Hammersley sequence                         — 2D low-discrepancy for shaders
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_FIBONACCI_HLSLI
#define GP_FIBONACCI_HLSLI

// ─── Golden Ratio Constant ────────────────────────────────────────────────────────────────────

/// 2π / φ² where φ is the golden ratio. Used as the azimuthal step in Fibonacci lattices.
static const Float32 GP_GOLDEN_ANGLE = 2.39996322972865332f; // radians ≈ 137.5°

// ─── Van der Corput Radical Inverse ──────────────────────────────────────────────────────────
// Reflects an integer's binary representation about the decimal point.
// Maps {0, 1, 2, 3, ...} → {0, 0.5, 0.25, 0.75, ...} — perfectly interleaved.
// This is the 1D base-2 Halton sequence and the X-component of the Hammersley sequence.

/// Compute the Van der Corput radical inverse of an unsigned integer in base 2.
/// \param bits  Sample index (0-based).
/// \return      Float in [0, 1).
Float32 GP_RadicalInverseBase2(UInt32 bits)
{
    bits  = (bits << 16u) | (bits >> 16u);
    bits  = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits  = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits  = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits  = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return (Float32)bits * 2.3283064365386963e-10f; // / 2^32
}

/// Radical inverse in base 3. Used as the Y-component of a base-(2,3) Halton sequence.
Float32 GP_RadicalInverseBase3(UInt32 n)
{
    Float32 result = 0.0f;
    Float32 base   = 1.0f / 3.0f;
    Float32 inv    = base;
    while (n > 0u)
    {
        result += (Float32)(n % 3u) * inv;
        n      /= 3u;
        inv    *= base;
    }
    return result;
}

/// Radical inverse in base 5. Used as the Z-component of a base-(2,3,5) Halton sequence.
Float32 GP_RadicalInverseBase5(UInt32 n)
{
    Float32 result = 0.0f;
    Float32 inv    = 1.0f / 5.0f;
    while (n > 0u)
    {
        result += (Float32)(n % 5u) * inv;
        n      /= 5u;
        inv    /= 5.0f;
    }
    return result;
}

// ─── Hammersley Sequence ──────────────────────────────────────────────────────────────────────
// A 2D low-discrepancy sequence for a fixed total sample count N.
// i/N gives the stratified X, radical inverse gives the Y.
// Optimal for fixed-count loops (IBL LUT baking, BRDF integration, prefiltering).

/// Return the i-th sample of the Hammersley sequence for a total of N samples.
/// \param i  Sample index [0, N).
/// \param N  Total sample count.
/// \return   2D sample in [0, 1)^2.
Float2 GP_Hammersley(UInt32 i, UInt32 N)
{
    return Float2((Float32)i / (Float32)N, GP_RadicalInverseBase2(i));
}

/// Hammersley with a randomized scramble for decorrelated multi-pass use.
/// Adds a per-pass random offset (Cranley-Patterson rotation) to both components.
Float2 GP_HammersleyScrambled(UInt32 i, UInt32 N, Float2 random)
{
    Float2 h = GP_Hammersley(i, N);
    return frac(h + random);
}

// ─── 2D Fibonacci Lattice ─────────────────────────────────────────────────────────────────────
// Maps an index to a 2D point in [0,1]^2 using the golden ratio as the irrational step.
// Superior to Hammersley when N is unknown at compile time (streaming / adaptive sampling).

/// Return the i-th 2D Fibonacci lattice sample in [0,1)^2.
/// \param i  Sample index (0-based).
Float2 GP_FibonacciLattice2D(UInt32 i)
{
    Float32 x = frac((Float32)i * GP_GOLDEN);                  // golden ratio irrational step
    Float32 y = (Float32)i * GP_INV_PI * GP_GOLDEN;            // ~0.618... offset in Y
    return Float2(x, frac(y));
}

// ─── Spherical Fibonacci ──────────────────────────────────────────────────────────────────────
// Distributes N points uniformly on the unit sphere via the golden angle spiral.
// Reference: Marques et al. "Spherical Fibonacci Point Sets for Illumination Integrals" (2013).

/// Return the i-th of N uniformly distributed directions on the unit sphere.
/// Points are distributed via an area-preserving golden-angle spiral.
/// \param i  Sample index [0, N).
/// \param N  Total sample count.
/// \return   Unit direction vector on the sphere.
Float3 GP_SphericalFibonacci(UInt32 i, UInt32 N)
{
    Float32 phi   = GP_GOLDEN_ANGLE * (Float32)i;
    Float32 cosT  = 1.0f - (2.0f * (Float32)i + 1.0f) / (Float32)N; // maps to [-1, 1]
    Float32 sinT  = sqrt(saturate(1.0f - cosT * cosT));

    Float32 sinP, cosP;
    sincos(phi, sinP, cosP);

    return Float3(sinT * cosP, sinT * sinP, cosT);
}

/// Return the i-th of N directions uniformly distributed on the upper hemisphere (Z >= 0).
/// \param i  Sample index [0, N).
/// \param N  Total sample count.
/// \return   Unit direction on the upper hemisphere.
Float3 GP_HemisphereFibonacci(UInt32 i, UInt32 N)
{
    Float32 phi   = GP_GOLDEN_ANGLE * (Float32)i;
    Float32 cosT  = 1.0f - (Float32)i / (Float32)(N - 1u);    // maps to [0, 1]
    Float32 sinT  = sqrt(saturate(1.0f - cosT * cosT));

    Float32 sinP, cosP;
    sincos(phi, sinP, cosP);

    return Float3(sinT * cosP, sinT * sinP, cosT);
}

/// Cosine-weighted hemisphere Fibonacci sample (higher density near the pole).
/// Matches the cosine lobe weighting of Lambertian diffuse — use for diffuse BRDF integration.
/// \param i  Sample index [0, N).
/// \param N  Total sample count.
/// \return   Cosine-weighted direction on the upper hemisphere.
Float3 GP_HemisphereFibonacciCosine(UInt32 i, UInt32 N)
{
    Float2 xi   = GP_Hammersley(i, N);
    Float32 phi = xi.x * GP_TWO_PI;

    // Malley's method: project uniformly distributed disk sample to hemisphere
    Float32 r    = sqrt(xi.y);
    Float32 sinP, cosP;
    sincos(phi, sinP, cosP);

    return Float3(r * cosP, r * sinP, sqrt(max(0.0f, 1.0f - xi.y)));
}

// ─── GGX Importance Sampling ──────────────────────────────────────────────────────────────────
// Warp a uniform 2D sample (xi) to a half-vector H distributed according to the GGX NDF.
// Used in IBL specular prefiltering (IBLSpecular.CS.hlsl) and BRDF LUT generation (BRDFLUT.CS.hlsl).

/// Sample a GGX-distributed half-vector from a 2D uniform sample.
/// \param xi         2D uniform sample in [0,1)^2.
/// \param roughness  Perceptual roughness (α = roughness^2).
/// \return           Half-vector H in tangent space (Z = up).
Float3 GP_SampleGGX(Float2 xi, Float32 roughness)
{
    Float32 alpha     = roughness * roughness;
    Float32 alphaSq   = alpha * alpha;

    Float32 phi       = GP_TWO_PI * xi.x;
    Float32 cosTheta  = sqrt((1.0f - xi.y) / (1.0f + (alphaSq - 1.0f) * xi.y));
    Float32 sinTheta  = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));

    Float32 sinP, cosP;
    sincos(phi, sinP, cosP);

    return Float3(sinTheta * cosP, sinTheta * sinP, cosTheta);
}

/// GGX PDF value for a given half-vector H and surface normal N.
/// Used as the Monte Carlo weight denominator when sampling with GP_SampleGGX.
/// \param NdotH  Dot product of N and H.
/// \param alpha  Roughness squared (α = roughness^2).
Float32 GP_SampleGGX_PDF(Float32 NdotH, Float32 alpha)
{
    Float32 alphaSq   = alpha * alpha;
    Float32 denom     = (NdotH * NdotH * (alphaSq - 1.0f) + 1.0f);
    Float32 D         = alphaSq / (GP_PI * denom * denom);
    return D * NdotH;
}

// ─── Cosine-Weighted Hemisphere Sample ───────────────────────────────────────────────────────
// Used for diffuse BRDF integration with explicit PDF for Monte Carlo estimators.

/// Sample a cosine-weighted direction on the upper hemisphere.
/// \param xi  2D uniform sample in [0,1)^2.
/// \return    Cosine-weighted direction in tangent space (Z = up).
Float3 GP_SampleHemisphereCosine(Float2 xi)
{
    Float32 phi    = GP_TWO_PI * xi.x;
    Float32 sinT   = sqrt(xi.y);
    Float32 cosT   = sqrt(max(0.0f, 1.0f - xi.y));

    Float32 sinP, cosP;
    sincos(phi, sinP, cosP);

    return Float3(sinT * cosP, sinT * sinP, cosT);
}

/// PDF of cosine-weighted hemisphere sampling: p(ω) = cosθ / π.
Float32 GP_SampleHemisphereCosine_PDF(Float32 NdotL)
{
    return max(NdotL, 0.0f) * GP_INV_PI;
}

/// Sample a uniform direction on the upper hemisphere: p(ω) = 1 / (2π).
Float3 GP_SampleHemisphereUniform(Float2 xi)
{
    Float32 phi    = GP_TWO_PI * xi.x;
    Float32 cosT   = xi.y;
    Float32 sinT   = sqrt(max(0.0f, 1.0f - cosT * cosT));

    Float32 sinP, cosP;
    sincos(phi, sinP, cosP);

    return Float3(sinT * cosP, sinT * sinP, cosT);
}

/// PDF of uniform hemisphere sampling.
Float32 GP_SampleHemisphereUniform_PDF()
{
    return GP_INV_TWO_PI;
}

#endif // GP_FIBONACCI_HLSLI
