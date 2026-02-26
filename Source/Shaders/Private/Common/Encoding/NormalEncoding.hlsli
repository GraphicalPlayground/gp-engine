/// \file    NormalEncoding.hlsli
/// \brief   Normal vector encoding and decoding schemes for the GBuffer and vertex streams.
///          Provides multiple algorithms with explicit precision/performance trade-off
///          documentation to serve as a learning reference.
///
/// \note    Engine default: Octahedral encoding into R16G16_SNORM (two signed shorts).
///          Rationale: best precision/bit ratio, uniform error, no singularities.
///          Alternatives are provided for comparison and legacy support.
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_NORMAL_ENCODING_HLSLI
#define GP_NORMAL_ENCODING_HLSLI

// ─────────────────────────────────────────────────────────────────────────────────────────────
// 1. OCTAHEDRAL ENCODING (Engine Default)
// Reference: Cigolle et al. "Survey of Efficient Representations for Independent Unit Vectors"
//            Meyer et al. "Real-Time Rendering" 4th ed.
//
// Precision: ~0.000015 rad max angular error at 16-bit snorm.
// Storage:   2 x 16-bit snorm  (R16G16_SNORM, 4 bytes)
// Coverage:  Full sphere (hemisphere variant available below).
// Cost:      ~4 ALU encode, ~5 ALU decode.
// ─────────────────────────────────────────────────────────────────────────────────────────────

/// Encode a unit vector to octahedral representation in [-1, 1]^2 (SNORM domain).
/// Works for all points on the unit sphere, including the bottom hemisphere.
Float2 GP_OctEncodeSnorm(Float3 n)
{
    // Project onto L1 unit sphere (octahedron face)
    Float2 p = n.xy / (abs(n.x) + abs(n.y) + abs(n.z));

    // Fold negative-Z hemisphere: flip corner symmetrically
    if (n.z <= 0.0f)
        p = (1.0f - abs(p.yx)) * sign(p);

    return p;
}

/// Decode from octahedral SNORM [-1, 1]^2 to a unit vector.
Float3 GP_OctDecodeSnorm(Float2 e)
{
    Float3 v = Float3(e.x, e.y, 1.0f - abs(e.x) - abs(e.y));

    // Un-fold bottom hemisphere
    if (v.z < 0.0f)
        v.xy = (1.0f - abs(v.yx)) * sign(v.xy);

    return normalize(v);
}

/// Encode to UNORM [0, 1]^2 for 8-bit storage (lower precision, ~0.012 rad max error at 8-bit).
Float2 GP_OctEncodeUnorm(Float3 n)
{
    return GP_OctEncodeSnorm(n) * 0.5f + 0.5f;
}

/// Decode from UNORM [0, 1]^2.
Float3 GP_OctDecodeUnorm(Float2 e)
{
    return GP_OctDecodeSnorm(e * 2.0f - 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// 2. OCTAHEDRAL HEMISPHERE ENCODING
// Variant for normals guaranteed to be in the upper hemisphere (N·up >= 0).
// Saves the fold computation; gains a tiny amount of precision.
// Used for: terrain normals, skylight probes, cloth fibers pointing upward.
// Storage: 2 x 16-bit unorm (R16G16_UNORM, 4 bytes)
// ─────────────────────────────────────────────────────────────────────────────────────────────

/// Encode a hemisphere normal (Z >= 0) to UNORM octahedral [0,1]^2.
Float2 GP_OctEncodeHemisphere(Float3 n)
{
    GP_BRANCH
    if (n.z < 0.0f)
        n.z = 0.0f; // Clamp to hemisphere boundary

    Float2 p = n.xy / (abs(n.x) + abs(n.y) + abs(n.z));
    return p * 0.5f + 0.5f;
}

/// Decode hemisphere normal from UNORM octahedral.
Float3 GP_OctDecodeHemisphere(Float2 e)
{
    Float2 p = e * 2.0f - 1.0f;
    Float3 n = Float3(p.x, p.y, 1.0f - abs(p.x) - abs(p.y));
    return normalize(Float3(n.xy, max(n.z, 0.0f)));
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// 3. SPHEREMAP TRANSFORM (Lambert Azimuthal Equal-Area)
// Reference: Aras Pranckevičius "Compact Normal Storage for small G-Buffers" (2009)
//
// Precision: Non-uniform — best near the top, degrades near horizon.
// Storage:   2 x 16-bit unorm (R16G16_UNORM, 4 bytes)
// Cost:      ~5 ALU encode, ~5 ALU decode.
// Note:      Only encodes the upper hemisphere. If full sphere needed, use Octahedral instead.
// ─────────────────────────────────────────────────────────────────────────────────────────────

/// Encode a view-space or world-space upper-hemisphere normal using the Spheremap transform.
Float2 GP_SpheremapEncode(Float3 n)
{
    // Lambert azimuthal equal-area projection
    Float32 f = sqrt(8.0f * n.z + 8.0f);
    return n.xy / f + 0.5f;
}

/// Decode Spheremap-encoded normal.
Float3 GP_SpheremapDecode(Float2 e)
{
    Float2 fenc = e * 4.0f - 2.0f;
    Float32 f   = dot(fenc, fenc);
    Float32 g   = sqrt(1.0f - f / 4.0f);
    return Float3(fenc * g, 1.0f - f / 2.0f);
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// 4. STEREOGRAPHIC PROJECTION
// Projects the sphere to a plane through the South Pole tangent point.
//
// Precision: Non-uniform — highest near the North Pole (like Spheremap).
// Storage:   2 x 16-bit snorm
// Cost:      ~4 ALU encode, ~5 ALU decode.
// ─────────────────────────────────────────────────────────────────────────────────────────────

/// Encode via stereographic projection. Input normal must be a unit vector.
Float2 GP_StereographicEncode(Float3 n)
{
    return n.xy / (1.0f + n.z);
}

/// Decode from stereographic projection.
Float3 GP_StereographicDecode(Float2 e)
{
    Float32 lenSq = dot(e, e);
    return Float3(2.0f * e / (1.0f + lenSq), (1.0f - lenSq) / (1.0f + lenSq));
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// 5. R10G10B10 DIRECT SNORM
// Store XYZ components directly, discarding 2 bits for alpha.
//
// Precision: ~0.001 rad uniform.
// Storage:   R10G10B10A2_SNORM (4 bytes)
// Cost:      Minimal — pack/unpack is a hardware format.
// Downside:  Decoding requires renormalize; no guarantee of unit length.
// ─────────────────────────────────────────────────────────────────────────────────────────────

/// Pack a normal into R10G10B10_SNORM (biased to [0,1] for UNORM storage with 10 bits).
UInt32 GP_PackNormalR10G10B10(Float3 n)
{
    n = saturate(n * 0.5f + 0.5f);
    UInt32 x = (UInt32)(n.r * 1023.0f + 0.5f);
    UInt32 y = (UInt32)(n.g * 1023.0f + 0.5f);
    UInt32 z = (UInt32)(n.b * 1023.0f + 0.5f);
    return x | (y << 10u) | (z << 20u);
}

/// Unpack a normal from R10G10B10 biased UNORM.
Float3 GP_UnpackNormalR10G10B10(UInt32 packed)
{
    Float3 n;
    n.x = (Float32)((packed)       & 0x3FFu) / 1023.0f;
    n.y = (Float32)((packed >> 10u) & 0x3FFu) / 1023.0f;
    n.z = (Float32)((packed >> 20u) & 0x3FFu) / 1023.0f;
    return normalize(n * 2.0f - 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// 6. DERIVATIVE MAP NORMALS (DXT5nm / BC5 Tangent-Space Normals)
// Store only XY of a tangent-space normal; reconstruct Z = sqrt(1 - x^2 - y^2).
//
// Precision: Lower — Z precision degrades near tangent horizon (Z ≈ 0).
// Storage:   BC5 (2 x 8-bit channels, GPU-compressed). VRAM-efficient for surface normals.
// Cost:      ~2 ALU decode (one sqrt).
// Use:       Surface normal maps read from BC5-compressed textures.
// ─────────────────────────────────────────────────────────────────────────────────────────────

/// Decode a BC5 / DXT5nm normal map sample to a unit tangent-space normal.
/// Assumes the texture stores [X, Y] in a [0,1] or [-1,1] SNORM format.
/// For BC5_SNORM (DX format), X and Y come in as [-1,1] directly.
Float3 GP_DecodeNormalMapBC5(Float2 xy)
{
    Float3 n;
    n.xy = xy;  // Already in [-1, 1] if hardware decoded SNORM
    n.z  = sqrt(max(0.0f, 1.0f - dot(n.xy, n.xy)));
    return normalize(n);
}

/// Decode from an 8-bit UNorm texture (swizzle required: RG = XY stored in [0,1]).
Float3 GP_DecodeNormalMapUnorm(Float2 uv)
{
    Float3 n;
    n.xy = uv * 2.0f - 1.0f;
    n.z  = sqrt(max(0.0f, 1.0f - dot(n.xy, n.xy)));
    return normalize(n);
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// PRECISION COMPARISON TABLE (angular error at various bit depths)
//
// Method               | 8-bit     | 16-bit    | Notes
// ─────────────────────┼───────────┼───────────┼──────────────────────────────────────────
// Octahedral SNORM     | 0.70°     | 0.0013°   | Best uniform precision, full sphere
// Octahedral UNORM     | 0.70°     | 0.0013°   | Same, slightly simpler for unorm targets
// Spheremap (Lambert)  | ~1.2°     | ~0.003°   | Non-uniform, hemisphere only
// Stereographic        | ~0.9°     | ~0.002°   | Non-uniform, hemisphere only
// R10G10B10 direct     | N/A       | ~0.056°   | Uniform, requires renormalize
// BC5 derivative       | ~2.3°     | N/A       | Horizon artifacts, VRAM-efficient
//
// Recommendation: Use Octahedral SNORM (R16G16_SNORM) for GBuffer normals.
//                 Use BC5 + GP_DecodeNormalMapBC5 for surface normal maps.
// ─────────────────────────────────────────────────────────────────────────────────────────────

// ─── Engine-Default Aliases ───────────────────────────────────────────────────────────────────
// The GBuffer uses Octahedral SNORM throughout. These aliases make GBuffer shaders
// self-documenting without repeating the algorithm name everywhere.

#define GP_EncodeGBufferNormal(n)   GP_OctEncodeSnorm(n)
#define GP_DecodeGBufferNormal(e)   GP_OctDecodeSnorm(e)

#endif // GP_NORMAL_ENCODING_HLSLI
