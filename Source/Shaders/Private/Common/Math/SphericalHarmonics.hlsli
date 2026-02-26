/// \file    SphericalHarmonics.hlsli
/// \brief   Spherical Harmonics (SH) evaluation, projection, and irradiance reconstruction.
///          L1 (4 coefficients) and L2 (9 coefficients) bands are supported.
///
/// \details Spherical Harmonics compress low-frequency spherical functions into a small set
///          of coefficients. In rendering they are used for:
///          - Diffuse irradiance (3-band / 9 coefficients per color channel)
///          - Ambient occlusion visibility terms
///          - DDGI probe radiance representation
///          - Precomputed Radiance Transfer (PRT)
///
///          SH basis convention: real-valued, orthonormal, Condon-Shortley phase excluded.
///          Matching the convention in Sloan (2008) "Stupid Spherical Harmonics Tricks"
///          and Ramamoorthi & Hanrahan (2001) irradiance environment maps.
///
///          Coefficient storage layout (L0 + L1 + L2 = 9 coefficients):
///            Index  Band  m   Name
///            0      0     0   Y_0^0   (constant)
///            1      1    -1   Y_1^{-1}
///            2      1     0   Y_1^0
///            3      1     1   Y_1^1
///            4      2    -2   Y_2^{-2}
///            5      2    -1   Y_2^{-1}
///            6      2     0   Y_2^0
///            7      2     1   Y_2^1
///            8      2     2   Y_2^2
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_SPHERICAL_HARMONICS_HLSLI
#define GP_SPHERICAL_HARMONICS_HLSLI

// ─── SH Storage Structs ───────────────────────────────────────────────────────────────────────

/// L1 SH coefficients (4 values). One per color channel for RGB irradiance.
struct FSH_L1
{
    Float4 c[4];    ///< c[0..3] = SH bands Y_0^0 through Y_1^1.
                    ///< c[i].xyz = RGB coefficients, c[i].w = padding.
};

/// L2 SH coefficients (9 values). One per color channel for full RGB irradiance.
/// Most common representation for diffuse GI (DDGI probes, IBL irradiance).
struct FSH_L2
{
    Float4 c[9];    ///< c[0..8] = SH bands Y_0^0 through Y_2^2.
                    ///< c[i].xyz = RGB coefficients, c[i].w = padding.
};

/// Compact L2 representation: stores 9x float3 packed into 7x float4 for VRAM efficiency.
/// Layout: c0.xyz=Y00, c0.w=Y1-1.x | c1.xyz=Y1-1.yz+Y10.x, etc.
/// The full pack/unpack is done in SH_Pack7 / SH_Unpack7 below.
struct FSH_L2_Packed
{
    Float4 c[7];
};

// ─── SH Basis Functions ───────────────────────────────────────────────────────────────────────
// Evaluate the real spherical harmonic basis for a given direction.
// Direction n must be a unit vector.

/// Y_0^0 — constant band 0.
Float32 GP_SH_Y00 (Float3 n) { return  0.282094792f; }

/// Y_1^{-1} — band 1, dipole in Y.
Float32 GP_SH_Y1n1(Float3 n) { return  0.488602512f * n.y; }

/// Y_1^0   — band 1, dipole in Z.
Float32 GP_SH_Y10 (Float3 n) { return  0.488602512f * n.z; }

/// Y_1^1   — band 1, dipole in X.
Float32 GP_SH_Y11 (Float3 n) { return  0.488602512f * n.x; }

/// Y_2^{-2} — band 2.
Float32 GP_SH_Y2n2(Float3 n) { return  1.092548431f * n.x * n.y; }

/// Y_2^{-1} — band 2.
Float32 GP_SH_Y2n1(Float3 n) { return  1.092548431f * n.y * n.z; }

/// Y_2^0   — band 2 (zonal).
Float32 GP_SH_Y20 (Float3 n) { return  0.315391565f * (3.0f * n.z * n.z - 1.0f); }

/// Y_2^1   — band 2.
Float32 GP_SH_Y21 (Float3 n) { return  1.092548431f * n.x * n.z; }

/// Y_2^2   — band 2.
Float32 GP_SH_Y22 (Float3 n) { return  0.546274215f * (n.x * n.x - n.y * n.y); }

/// Evaluate all 9 L2 basis values for a direction and store them in a float array.
/// \param n    Unit direction vector.
/// \param y    Output: 9 basis values [Y00, Y1-1, Y10, Y11, Y2-2, Y2-1, Y20, Y21, Y22].
void GP_SH_EvalL2(Float3 n, out Float32 y[9])
{
    y[0] = GP_SH_Y00(n);
    y[1] = GP_SH_Y1n1(n);
    y[2] = GP_SH_Y10(n);
    y[3] = GP_SH_Y11(n);
    y[4] = GP_SH_Y2n2(n);
    y[5] = GP_SH_Y2n1(n);
    y[6] = GP_SH_Y20(n);
    y[7] = GP_SH_Y21(n);
    y[8] = GP_SH_Y22(n);
}

// ─── SH Projection ────────────────────────────────────────────────────────────────────────────
// Add a radiance sample to an SH coefficient set during Monte Carlo integration.
// Call once per sampled direction to build up a probe or irradiance estimate.

/// Project a single RGB radiance sample in direction n into L2 SH coefficients.
/// Accumulate the result — call this in a loop over many samples, then normalize by N.
/// \param n        Sample direction (unit vector, from surface to light).
/// \param radiance Linear RGB radiance arriving from that direction.
/// \param sh       SH coefficient struct to accumulate into.
/// \param weight   Monte Carlo weight = 1 / (sampleCount * pdf). For uniform hemisphere: 2π/N.
void GP_SH_AddSample(Float3 n, Float3 radiance, inout FSH_L2 sh, Float32 weight)
{
    Float32 y[9];
    GP_SH_EvalL2(n, y);

    GP_UNROLL
    for (Int32 i = 0; i < 9; ++i)
    {
        sh.c[i].xyz += radiance * y[i] * weight;
    }
}

// ─── SH Reconstruction ────────────────────────────────────────────────────────────────────────
// Evaluate the stored SH function at a query direction.

/// Evaluate L2 SH at a given direction. Returns the RGB value stored in the SH.
/// Used to look up irradiance in a direction from a probe.
Float3 GP_SH_EvalL2RGB(FSH_L2 sh, Float3 n)
{
    Float32 y[9];
    GP_SH_EvalL2(n, y);

    Float3 result = (Float3)0;
    GP_UNROLL
    for (Int32 i = 0; i < 9; ++i)
    {
        result += sh.c[i].xyz * y[i];
    }
    return max(result, (Float3)0);
}

/// Evaluate L1 SH at a given direction. Faster but lower quality.
Float3 GP_SH_EvalL1RGB(FSH_L1 sh, Float3 n)
{
    return max(sh.c[0].xyz * GP_SH_Y00(n)
             + sh.c[1].xyz * GP_SH_Y1n1(n)
             + sh.c[2].xyz * GP_SH_Y10(n)
             + sh.c[3].xyz * GP_SH_Y11(n), (Float3)0);
}

// ─── Irradiance Reconstruction ────────────────────────────────────────────────────────────────
// Ramamoorthi & Hanrahan (2001): the irradiance integral E(n) over the hemisphere can be
// approximated by convolving the radiance SH with the clamped-cosine ZH kernel.
// This folded the integration into the SH coefficients, making real-time irradiance evaluation
// a simple dot-product of 9 terms.
//
// Convolution constants (Λ_l for l = 0,1,2):
//   Λ_0 = π
//   Λ_1 = 2π/3
//   Λ_2 = π/4
//
// The irradiance SH = SH_radiance * Λ_l (per band).
// After this convolution, evaluating irradiance at a normal direction costs ~27 MAD ops.

/// Pre-convolve L2 radiance SH with the cosine lobe to get irradiance SH.
/// Call once when building a probe. The output can be directly evaluated with GP_SH_EvalL2RGB.
FSH_L2 GP_SH_ConvolveWithCosine(FSH_L2 sh)
{
    FSH_L2 result;

    // Band 0: Λ_0 = π
    result.c[0] = sh.c[0] * GP_PI;

    // Band 1: Λ_1 = 2π/3
    Float32 lambda1 = GP_TWO_PI / 3.0f;
    result.c[1] = sh.c[1] * lambda1;
    result.c[2] = sh.c[2] * lambda1;
    result.c[3] = sh.c[3] * lambda1;

    // Band 2: Λ_2 = π/4
    Float32 lambda2 = GP_PI / 4.0f;
    result.c[4] = sh.c[4] * lambda2;
    result.c[5] = sh.c[5] * lambda2;
    result.c[6] = sh.c[6] * lambda2;
    result.c[7] = sh.c[7] * lambda2;
    result.c[8] = sh.c[8] * lambda2;

    return result;
}

/// Evaluate irradiance at normal n from a pre-convolved L2 irradiance SH.
/// This is the function called at runtime in every pixel that samples probe irradiance.
/// ~27 MAD operations — suitable for use in deferred or forward lighting passes.
Float3 GP_SH_Irradiance(FSH_L2 irradianceSH, Float3 n)
{
    return GP_SH_EvalL2RGB(irradianceSH, n);
}

// ─── Geomerics Lightmap Evaluation ────────────────────────────────────────────────────────────
// Geomerics' approach evaluates clamped (non-negative) SH irradiance without the ringing
// artifacts that appear when L2 SH has negative lobes. Used for lightmaps in games
// (e.g., Frostbite, Enlighten-style baked GI).
// Reference: Geomerics "Enlighten SDK — Spherical Harmonics"

/// Evaluate HL2-style directional lightmap (3 basis directions + ambient).
/// Takes 4 coefficients per channel (L1 SH) and evaluates against the surface normal.
/// \param n      World-space surface normal.
/// \param c0     SH coefficient 0 (ambient) — float3 RGB.
/// \param c1     SH coefficient 1 (Y-axis dipole).
/// \param c2     SH coefficient 2 (Z-axis dipole).
/// \param c3     SH coefficient 3 (X-axis dipole).
Float3 GP_SH_HL2Eval(Float3 n, Float3 c0, Float3 c1, Float3 c2, Float3 c3)
{
    // HL2 basis: clamp each directional component individually to avoid negative lobes
    Float3 result = c0
                  + c1 * n.y
                  + c2 * n.z
                  + c3 * n.x;
    return max(result, (Float3)0);
}

// ─── Windowing (Ringing Suppression) ──────────────────────────────────────────────────────────
// High-frequency spherical signals produce ringing (Gibbs phenomenon) in truncated SH.
// Windowing applies per-band attenuation to reduce this at the cost of some blurring.
// Reference: Sloan "Stupid SH Tricks" — sinc windowing.

/// Apply Hanning windowing to L2 SH coefficients to suppress ringing.
/// \param sh  SH coefficients to window (modified in place).
/// \param w   Window width parameter [0, 1]. 0 = no windowing, 1 = maximum smoothing.
void GP_SH_Window(inout FSH_L2 sh, Float32 w)
{
    // Per-band scale: s_l = sinc(l * π / (L+1)) where L=2
    Float32 s0 = 1.0f;                                     // Band 0
    Float32 s1 = sin(GP_PI / 3.0f) / (GP_PI / 3.0f);      // Band 1
    Float32 s2 = sin(GP_TWO_PI / 3.0f) / (GP_TWO_PI / 3.0f); // Band 2

    // Lerp between no windowing (1.0) and full windowing (s_l)
    s1 = lerp(1.0f, s1, w);
    s2 = lerp(1.0f, s2, w);

    sh.c[1] *= s1; sh.c[2] *= s1; sh.c[3] *= s1;
    sh.c[4] *= s2; sh.c[5] *= s2; sh.c[6] *= s2;
    sh.c[7] *= s2; sh.c[8] *= s2;
}

// ─── Rotation ─────────────────────────────────────────────────────────────────────────────────
// Rotating SH is needed when capturing environment maps at a fixed orientation but
// querying irradiance in world space. Full L2 rotation requires a 9x9 matrix; the
// L1 sub-matrix is 3x3 and can be constructed directly from a rotation matrix.

/// Rotate L1 SH (bands 0 and 1) by a 3x3 rotation matrix.
/// This covers most dynamic probe use cases (sky rotation, time-of-day transitions).
FSH_L1 GP_SH_RotateL1(FSH_L1 sh, Float3x3 rot)
{
    FSH_L1 result;

    // Band 0 is rotationally invariant (constant)
    result.c[0] = sh.c[0];

    // Band 1 transforms via the 3x3 rotation matrix (same as rotating a vector)
    // Note: SH band-1 basis = (Y, Z, X) — map to matrix columns accordingly
    result.c[1].xyz = rot[1] * sh.c[1].x + rot[2] * sh.c[1].y + rot[0] * sh.c[1].z; // Y
    result.c[2].xyz = rot[1] * sh.c[2].x + rot[2] * sh.c[2].y + rot[0] * sh.c[2].z; // Z
    result.c[3].xyz = rot[1] * sh.c[3].x + rot[2] * sh.c[3].y + rot[0] * sh.c[3].z; // X

    result.c[1].w = sh.c[1].w;
    result.c[2].w = sh.c[2].w;
    result.c[3].w = sh.c[3].w;

    return result;
}

// ─── Zero / Identity ─────────────────────────────────────────────────────────────────────────

/// Return a zeroed L2 SH struct.
FSH_L2 GP_SH_Zero()
{
    FSH_L2 sh;
    GP_UNROLL
    for (Int32 i = 0; i < 9; ++i)
        sh.c[i] = (Float4)0;
    return sh;
}

/// Return an L2 SH initialized to a constant ambient color.
FSH_L2 GP_SH_Constant(Float3 color)
{
    FSH_L2 sh = GP_SH_Zero();
    sh.c[0].xyz = color * GP_SH_Y00(Float3(0, 0, 1)); // Y00 = constant 0.282...
    return sh;
}

#endif // GP_SPHERICAL_HARMONICS_HLSLI
