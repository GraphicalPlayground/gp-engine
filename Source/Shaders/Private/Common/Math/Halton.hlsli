/// \file    Halton.hlsli
/// \brief   Halton low-discrepancy sequence for TAA sub-pixel jitter and temporal sampling.
///
/// \details The Halton sequence is a multi-dimensional low-discrepancy sequence constructed
///          from radical inverses in different prime bases. For 2D jitter, base (2, 3) is
///          standard. For 3D+, add base 5, 7, 11, ...
///
///          Why Halton over random noise for TAA jitter?
///          - Low discrepancy: samples cover the pixel footprint more evenly.
///          - Deterministic: same frame index always gives same offset (reproducible renders).
///          - No pattern repetition for long sequences: the period before revisiting a pixel
///            sub-region is far longer than any practical frame count.
///
///          TAA jitter convention in this engine:
///          - Jitter is applied as a sub-pixel offset to the projection matrix before rendering.
///          - The offset is stored in g_Frame.jitterOffset (current) and prevJitterOffset.
///          - Range: [-0.5, 0.5] pixels in screen space.
///          - The TAA resolve shader must account for this offset during reprojection.
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_HALTON_HLSLI
#define GP_HALTON_HLSLI

// ─── Core Halton ─────────────────────────────────────────────────────────────────────────────

/// Compute the i-th element of the Halton sequence in an arbitrary integer base.
/// \param i     Sample index (1-based is conventional; 0 maps to 0.0).
/// \param base  Prime base (2, 3, 5, 7, 11, ...).
/// \return      Float in [0, 1).
Float32 GP_Halton(UInt32 i, UInt32 base)
{
    Float32 f      = 1.0f;
    Float32 result = 0.0f;
    UInt32  index  = i;

    while (index > 0u)
    {
        f      /= (Float32)base;
        result += f * (Float32)(index % base);
        index  /= base;
    }

    return result;
}

/// Halton sequence 2D sample (base-2, base-3). The standard choice for TAA pixel jitter.
/// \param i  Frame index (0-based). Wraps every lcm(2^k, 3^k) frames automatically.
/// \return   2D sample in [0, 1)^2.
Float2 GP_Halton23(UInt32 i)
{
    return Float2(GP_Halton(i + 1u, 2u), GP_Halton(i + 1u, 3u));
}

/// Halton 3D sample (base 2, 3, 5). Used for 3D jitter (volumetric froxels, probe rays).
Float3 GP_Halton235(UInt32 i)
{
    return Float3(GP_Halton(i + 1u, 2u),
                  GP_Halton(i + 1u, 3u),
                  GP_Halton(i + 1u, 5u));
}

// ─── Fast Halton (Precomputed Tables) ────────────────────────────────────────────────────────
// For frame counts up to 128, precomputed Halton values avoid the division loop entirely.
// The CPU writes the current jitter into g_Frame.jitterOffset, so shaders typically just
// read that directly. These functions are for passes that need the sequence directly on GPU.

/// Precomputed Halton base-2 (Van der Corput) for indices 0–15.
/// These are the X-components of a 16-frame TAA sequence.
static const Float32 GP_HALTON2_16[16] =
{
    0.500000f, 0.250000f, 0.750000f, 0.125000f,
    0.625000f, 0.375000f, 0.875000f, 0.062500f,
    0.562500f, 0.312500f, 0.812500f, 0.187500f,
    0.687500f, 0.437500f, 0.937500f, 0.031250f
};

/// Precomputed Halton base-3 for indices 0–15.
/// These are the Y-components of a 16-frame TAA sequence.
static const Float32 GP_HALTON3_16[16] =
{
    0.333333f, 0.666667f, 0.111111f, 0.444444f,
    0.777778f, 0.222222f, 0.555556f, 0.888889f,
    0.037037f, 0.370370f, 0.703704f, 0.148148f,
    0.481481f, 0.814815f, 0.259259f, 0.592593f
};

/// Return the TAA jitter offset for a given frame, remapped to [-0.5, 0.5] pixel space.
/// Sequence length is 16 frames, then repeats. For 8-frame sequences, pass frameIndex % 8.
/// \param frameIndex  Current frame index (monotonic, from g_Frame.frameIndex).
/// \return            Sub-pixel jitter offset in pixels: X in [-0.5, 0.5], Y in [-0.5, 0.5].
Float2 GP_TAAJitter(UInt32 frameIndex)
{
    UInt32  i = frameIndex % 16u;
    return Float2(GP_HALTON2_16[i], GP_HALTON3_16[i]) - 0.5f;
}

/// Same as GP_TAAJitter but returns normalized (UV) jitter, divided by render resolution.
/// Use this to shift UV coordinates in the resolve pass rather than pixel coordinates.
Float2 GP_TAAJitterUV(UInt32 frameIndex, Float2 invRenderResolution)
{
    return GP_TAAJitter(frameIndex) * invRenderResolution;
}

// ─── Cranley-Patterson Rotation ───────────────────────────────────────────────────────────────
// Shifts an entire low-discrepancy sequence by a random offset (wrapping in [0,1)).
// Used to decorrelate Halton across spatial samples (pixels) or temporal samples (frames),
// while preserving the sequence's low-discrepancy property.

/// Apply a Cranley-Patterson rotation to a Halton 2D sample.
/// \param sample  Original sample in [0, 1)^2.
/// \param shift   Random shift in [0, 1)^2 (e.g., from a blue noise texture or hash).
/// \return        Rotated sample, still in [0, 1)^2.
Float2 GP_CranleyPatterson(Float2 sample, Float2 shift)
{
    return frac(sample + shift);
}

/// Per-pixel Halton sample with Cranley-Patterson rotation using pixel coordinate as the seed.
/// Gives each pixel a different, well-distributed sequence starting point.
/// \param sampleIndex  Which Halton sample to use (0 = first, 1 = second, etc.).
/// \param pixelCoord   Integer pixel coordinate (determines shift seed).
Float2 GP_PixelHalton23(UInt32 sampleIndex, UInt2 pixelCoord)
{
    Float2 base    = GP_Halton23(sampleIndex);
    Float2 shift   = Float2(GP_HashToFloat(GP_Hash2D(pixelCoord)),
                            GP_HashToFloat(GP_Hash2D(pixelCoord + UInt2(1u, 0u))));
    return GP_CranleyPatterson(base, shift);
}

// ─── Sobol Sequence (2D, first 16 points) ────────────────────────────────────────────────────
// Sobol is an alternative to Halton with better 2D projections. Used in some path tracing
// implementations (notably PBRT v4). Provided here for reference and comparison.
// For production, the engine path tracer uses a full scrambled Sobol table generated offline.

/// First 16 points of the Sobol sequence in 2D, stored as integer direction numbers.
/// Derived from Joe & Kuo's direction numbers table.
static const UInt32 GP_SOBOL_2D_X[16] =
{
    0x00000000u, 0x80000000u, 0x40000000u, 0xC0000000u,
    0x20000000u, 0xA0000000u, 0x60000000u, 0xE0000000u,
    0x10000000u, 0x90000000u, 0x50000000u, 0xD0000000u,
    0x30000000u, 0xB0000000u, 0x70000000u, 0xF0000000u
};

static const UInt32 GP_SOBOL_2D_Y[16] =
{
    0x00000000u, 0x80000000u, 0xC0000000u, 0x40000000u,
    0xE0000000u, 0x60000000u, 0xA0000000u, 0x20000000u,
    0xF0000000u, 0x70000000u, 0xB0000000u, 0x30000000u,
    0xD0000000u, 0x50000000u, 0x90000000u, 0x10000000u
};

/// Return the i-th 2D Sobol point (for i in [0, 15]).
Float2 GP_Sobol2D(UInt32 i)
{
    i = i & 15u; // Wrap to 16-point table
    return Float2(
        (Float32)GP_SOBOL_2D_X[i] * 2.3283064365386963e-10f,
        (Float32)GP_SOBOL_2D_Y[i] * 2.3283064365386963e-10f
    );
}

#endif // GP_HALTON_HLSLI
