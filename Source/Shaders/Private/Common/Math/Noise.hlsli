/// \file    Noise.hlsli
/// \brief   Procedural noise functions for GPU shaders: value noise, Perlin gradient noise,
///          Simplex noise, FBM (fractal Brownian motion), blue noise, and hash functions.
///
/// \note    All functions are stateless and deterministic — output depends only on input
///          coordinates. No texture lookups unless explicitly noted (blue noise uses a UAV).
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_NOISE_HLSLI
#define GP_NOISE_HLSLI

// ─── Hash Functions ───────────────────────────────────────────────────────────────────────────
// Integer hash functions with good avalanche properties, derived from xxHash / PCG.

/// 1D integer hash → UInt32. High-quality, bijective.
UInt32 GP_Hash1D(UInt32 x)
{
    x ^= x >> 16u;
    x *= 0x45D9F3Bu;
    x ^= x >> 16u;
    x *= 0x45D9F3Bu;
    x ^= x >> 16u;
    return x;
}

/// 2D integer hash → UInt32. Based on Murmur-inspired mixing.
UInt32 GP_Hash2D(UInt2 v)
{
    v = v * UInt2(3266489917u, 668265263u);
    v.x += v.y;
    v.y += v.x;
    v.x ^= v.x >> 15u;
    v.x *= 2246822519u;
    v.x ^= v.x >> 13u;
    v.x *= 3266489917u;
    v.x ^= v.x >> 16u;
    return v.x;
}

/// 3D integer hash → UInt32.
UInt32 GP_Hash3D(UInt3 v)
{
    v = v * UInt3(1597334673u, 3812015801u, 2798796415u);
    v.x += v.y + v.z;
    v.y += v.x;
    v.z += v.y;
    return GP_Hash1D(v.x ^ v.y ^ v.z);
}

/// Float hash — maps a UInt32 to a float in [0, 1).
Float32 GP_HashToFloat(UInt32 h)
{
    return GP_UIntToUNorm(h);
}

/// 1D float hash → float in [0, 1).
Float32 GP_HashF1(Float32 x)
{
    return GP_HashToFloat(GP_Hash1D(asuint(x)));
}

/// 2D float hash → float in [0, 1).
Float32 GP_HashF2F1(Float2 v)
{
    return GP_HashToFloat(GP_Hash2D(asuint(v)));
}

/// 2D float hash → float2 in [0, 1)^2.
Float2 GP_HashF2F2(Float2 v)
{
    UInt32 h = GP_Hash2D(asuint(v));
    return Float2(GP_HashToFloat(h), GP_HashToFloat(GP_Hash1D(h)));
}

/// 3D float hash → float in [0, 1).
Float32 GP_HashF3F1(Float3 v)
{
    return GP_HashToFloat(GP_Hash3D(asuint(v)));
}

// ─── Value Noise ──────────────────────────────────────────────────────────────────────────────
// Lattice-based noise. Fast but exhibits visible axis-aligned artifacts at low frequencies.

/// 2D Value noise in [-1, 1].
Float32 GP_ValueNoise2D(Float2 p)
{
    Float2 i = floor(p);
    Float2 f = frac(p);
    Float2 u = f * f * (3.0f - 2.0f * f); // Hermite C1 smoothstep

    Float32 a = GP_HashF2F1(i + Float2(0, 0));
    Float32 b = GP_HashF2F1(i + Float2(1, 0));
    Float32 c = GP_HashF2F1(i + Float2(0, 1));
    Float32 d = GP_HashF2F1(i + Float2(1, 1));

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y) * 2.0f - 1.0f;
}

/// 3D Value noise in [-1, 1].
Float32 GP_ValueNoise3D(Float3 p)
{
    Float3 i = floor(p);
    Float3 f = frac(p);
    Float3 u = f * f * (3.0f - 2.0f * f);

    // 8 corners of the unit cube
    Float32 c000 = GP_HashF3F1(i + Float3(0,0,0));
    Float32 c100 = GP_HashF3F1(i + Float3(1,0,0));
    Float32 c010 = GP_HashF3F1(i + Float3(0,1,0));
    Float32 c110 = GP_HashF3F1(i + Float3(1,1,0));
    Float32 c001 = GP_HashF3F1(i + Float3(0,0,1));
    Float32 c101 = GP_HashF3F1(i + Float3(1,0,1));
    Float32 c011 = GP_HashF3F1(i + Float3(0,1,1));
    Float32 c111 = GP_HashF3F1(i + Float3(1,1,1));

    return lerp(
        lerp(lerp(c000, c100, u.x), lerp(c010, c110, u.x), u.y),
        lerp(lerp(c001, c101, u.x), lerp(c011, c111, u.x), u.y),
        u.z
    ) * 2.0f - 1.0f;
}

// ─── Gradient Noise (Perlin) ──────────────────────────────────────────────────────────────────
// Gradient vectors are chosen from a fixed table via hashing.

/// Compute gradient contribution for 2D Perlin noise.
Float32 GP_GradDot2D(UInt32 hash, Float2 delta)
{
    // 8 gradient directions on the unit circle (classic Perlin)
    Float32 u = ((hash & 1u) != 0u) ? delta.x : delta.y;
    Float32 v = ((hash & 2u) != 0u) ?-delta.x : delta.y;
    return (((hash & 4u) != 0u) ? -u : u) + (((hash & 8u) != 0u) ? -v : v);
}

/// 2D Perlin gradient noise in approximately [-1, 1].
Float32 GP_PerlinNoise2D(Float2 p)
{
    Float2 i = floor(p);
    Float2 f = frac(p);
    Float2 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f); // Quintic C2 interpolant

    UInt32 h00 = GP_Hash2D(UInt2(i + Float2(0,0)));
    UInt32 h10 = GP_Hash2D(UInt2(i + Float2(1,0)));
    UInt32 h01 = GP_Hash2D(UInt2(i + Float2(0,1)));
    UInt32 h11 = GP_Hash2D(UInt2(i + Float2(1,1)));

    return lerp(
        lerp(GP_GradDot2D(h00, f - Float2(0,0)), GP_GradDot2D(h10, f - Float2(1,0)), u.x),
        lerp(GP_GradDot2D(h01, f - Float2(0,1)), GP_GradDot2D(h11, f - Float2(1,1)), u.x),
        u.y
    );
}

/// Compute gradient contribution for 3D Perlin noise (16 gradient directions on unit cube edges).
Float32 GP_GradDot3D(UInt32 hash, Float3 delta)
{
    UInt32 h = hash & 15u;
    Float32 u = (h < 8u) ? delta.x : delta.y;
    Float32 v = (h < 4u) ? delta.y : ((h == 12u || h == 14u) ? delta.x : delta.z);
    return (((h & 1u) == 0u) ? u : -u) + (((h & 2u) == 0u) ? v : -v);
}

/// 3D Perlin gradient noise in approximately [-1, 1].
Float32 GP_PerlinNoise3D(Float3 p)
{
    Float3 i = floor(p);
    Float3 f = frac(p);
    Float3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    Float32 c000 = GP_GradDot3D(GP_Hash3D(UInt3(i + Float3(0,0,0))), f - Float3(0,0,0));
    Float32 c100 = GP_GradDot3D(GP_Hash3D(UInt3(i + Float3(1,0,0))), f - Float3(1,0,0));
    Float32 c010 = GP_GradDot3D(GP_Hash3D(UInt3(i + Float3(0,1,0))), f - Float3(0,1,0));
    Float32 c110 = GP_GradDot3D(GP_Hash3D(UInt3(i + Float3(1,1,0))), f - Float3(1,1,0));
    Float32 c001 = GP_GradDot3D(GP_Hash3D(UInt3(i + Float3(0,0,1))), f - Float3(0,0,1));
    Float32 c101 = GP_GradDot3D(GP_Hash3D(UInt3(i + Float3(1,0,1))), f - Float3(1,0,1));
    Float32 c011 = GP_GradDot3D(GP_Hash3D(UInt3(i + Float3(0,1,1))), f - Float3(0,1,1));
    Float32 c111 = GP_GradDot3D(GP_Hash3D(UInt3(i + Float3(1,1,1))), f - Float3(1,1,1));

    return lerp(
        lerp(lerp(c000, c100, u.x), lerp(c010, c110, u.x), u.y),
        lerp(lerp(c001, c101, u.x), lerp(c011, c111, u.x), u.y),
        u.z
    );
}

// ─── Simplex Noise ────────────────────────────────────────────────────────────────────────────
// Better isotropy than Perlin, fewer artifacts, O(n) complexity (vs O(2^n) for grid noise).

/// 2D Simplex noise in [-1, 1]. Based on Stefan Gustavson's implementation.
Float32 GP_SimplexNoise2D(Float2 p)
{
    static const Float32 K1 = 0.366025403784f; // (sqrt(3)-1)/2
    static const Float32 K2 = 0.211324865405f; // (3-sqrt(3))/6

    Float2 i = floor(p + (p.x + p.y) * K1);
    Float2 a = p - (i - (i.x + i.y) * K2);
    Float2 o = (a.x > a.y) ? Float2(1, 0) : Float2(0, 1);
    Float2 b = a - o + K2;
    Float2 c = a - 1.0f + 2.0f * K2;

    Float3 h = max(0.5f - Float3(dot(a,a), dot(b,b), dot(c,c)), 0.0f);
    Float3 n = h * h * h * h * Float3(
        dot(a, frac(sin(Float2(i.x,          i.y         ) * Float2(127.1f, 311.7f)) * 43758.5453f) * 2.0f - 1.0f),
        dot(b, frac(sin(Float2(i.x + o.x,    i.y + o.y   ) * Float2(127.1f, 311.7f)) * 43758.5453f) * 2.0f - 1.0f),
        dot(c, frac(sin(Float2(i.x + 1.0f,   i.y + 1.0f  ) * Float2(127.1f, 311.7f)) * 43758.5453f) * 2.0f - 1.0f)
    );

    return 70.0f * (n.x + n.y + n.z);
}

// ─── Fractal Brownian Motion ──────────────────────────────────────────────────────────────────
// Sums multiple octaves of noise, each halved in amplitude and doubled in frequency.

/// FBM over 2D Perlin noise.
/// \param p          Input coordinate.
/// \param octaves    Number of octaves (typically 4–8).
/// \param lacunarity Frequency multiplier per octave (typically 2.0).
/// \param gain       Amplitude multiplier per octave (typically 0.5, = Hurst exponent 0.5).
Float32 GP_FBM2D(Float2 p, Int32 octaves = 5, Float32 lacunarity = 2.0f, Float32 gain = 0.5f)
{
    Float32 value     = 0.0f;
    Float32 amplitude = 0.5f;
    Float32 frequency = 1.0f;

    GP_UNROLL
    for (Int32 i = 0; i < octaves; ++i)
    {
        value     += amplitude * GP_PerlinNoise2D(p * frequency);
        frequency *= lacunarity;
        amplitude *= gain;
    }
    return value;
}

/// FBM over 3D Perlin noise.
Float32 GP_FBM3D(Float3 p, Int32 octaves = 5, Float32 lacunarity = 2.0f, Float32 gain = 0.5f)
{
    Float32 value     = 0.0f;
    Float32 amplitude = 0.5f;
    Float32 frequency = 1.0f;

    GP_UNROLL
    for (Int32 i = 0; i < octaves; ++i)
    {
        value     += amplitude * GP_PerlinNoise3D(p * frequency);
        frequency *= lacunarity;
        amplitude *= gain;
    }
    return value;
}

/// Domain-warped FBM. Feeds FBM output back as input displacement — creates organic swirling.
/// \note  Significantly more expensive than plain FBM. Use sparingly in pixel shaders.
Float32 GP_WarpedFBM2D(Float2 p, Int32 octaves = 4)
{
    Float2 q = Float2(GP_FBM2D(p,               octaves),
                      GP_FBM2D(p + Float2(5.2f, 1.3f), octaves));

    Float2 r = Float2(GP_FBM2D(p + 4.0f * q + Float2(1.7f, 9.2f),  octaves),
                      GP_FBM2D(p + 4.0f * q + Float2(8.3f, 2.8f),  octaves));

    return GP_FBM2D(p + 4.0f * r, octaves);
}

// ─── Voronoi / Worley ─────────────────────────────────────────────────────────────────────────
// Cell noise — returns distance to nearest feature point.

struct FVoronoiResult
{
    Float32 minDist;    ///< Distance to nearest cell center.
    Float32 minDist2;   ///< Distance to second-nearest cell center.
    Float2  cellID;     ///< Integer cell coordinates of nearest cell.
};

/// 2D Voronoi (Worley) noise. Returns distances and cell ID.
FVoronoiResult GP_Voronoi2D(Float2 p)
{
    Float2 i = floor(p);
    Float2 f = frac(p);

    FVoronoiResult result;
    result.minDist  = GP_LARGE_FLOAT;
    result.minDist2 = GP_LARGE_FLOAT;
    result.cellID   = (Float2)0;

    GP_UNROLL
    for (Int32 dy = -1; dy <= 1; ++dy)
    {
        GP_UNROLL
        for (Int32 dx = -1; dx <= 1; ++dx)
        {
            Float2 neighbor = Float2(dx, dy);
            Float2 point    = GP_HashF2F2(i + neighbor);
            Float2 diff     = neighbor + point - f;
            Float32 d       = dot(diff, diff); // squared distance

            if (d < result.minDist)
            {
                result.minDist2 = result.minDist;
                result.minDist  = d;
                result.cellID   = i + neighbor;
            }
            else if (d < result.minDist2)
            {
                result.minDist2 = d;
            }
        }
    }

    result.minDist  = sqrt(result.minDist);
    result.minDist2 = sqrt(result.minDist2);
    return result;
}

// ─── Blue Noise ───────────────────────────────────────────────────────────────────────────────
// Spatially anti-correlated noise. Preferred for dithering, ray jitter, and sample generation
// because it concentrates error at high frequencies (invisible to the human visual system).
//
// The engine pre-generates a 128x128 LDR blue noise texture (stored in the bindless heap at
// a well-known index, set in FrameConstants.blueNoiseTextureIndex).
//
// For temporal uses, the noise is shifted per-frame using a Cranley-Patterson rotation so
// samples decorrelate over time without repeating patterns.

/// Sample the blue noise texture at a given pixel coordinate, optionally temporally jittered.
/// \param pixelCoord     Integer pixel coordinate (wraps at 128x128).
/// \param frameIndex     Current frame index for temporal offset.
/// \param channel        Which channel to read (0=R, 1=G, 2=B, 3=A). Use different channels
///                       for independent 1D samples in the same pixel.
Float32 GP_BlueNoiseSample(UInt32 blueNoiseTexIndex, UInt2 pixelCoord, UInt32 frameIndex, UInt32 channel = 0u)
{
    // Cranley-Patterson rotation: add a per-frame hash offset and wrap
    UInt2 coord = (pixelCoord + UInt2(frameIndex * 1013u, frameIndex * 7919u)) & 127u;
    Float4 bn   = GP_TEXTURE2D(blueNoiseTexIndex).Load(Int3(coord, 0));

    Float32 channels[4] = { bn.r, bn.g, bn.b, bn.a };
    return channels[channel & 3u];
}

/// 2D blue noise sample returned as float2 (R and G channels = two independent 1D samples).
Float2 GP_BlueNoiseSample2D(UInt32 blueNoiseTexIndex, UInt2 pixelCoord, UInt32 frameIndex)
{
    UInt2 coord = (pixelCoord + UInt2(frameIndex * 1013u, frameIndex * 7919u)) & 127u;
    return GP_TEXTURE2D(blueNoiseTexIndex).Load(Int3(coord, 0)).rg;
}

#endif // GP_NOISE_HLSLI
