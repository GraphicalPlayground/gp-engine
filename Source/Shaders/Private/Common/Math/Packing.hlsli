/// \file    Packing.hlsli
/// \brief   Bit packing, quantization, and compact encoding utilities for GPU data layouts.
///          Used extensively in the GBuffer, vertex buffers, and constant buffers to minimize
///          memory bandwidth at the cost of minimal ALU.
///
/// \note    Coverage:
///          - Half float (R16G16, R16G16B16A16)
///          - R11G11B10F (HDR radiance packing)
///          - R10G10B10A2 (normal packing)
///          - Octahedral normal encoding (snorm16x2 / unorm8x2)
///          - RGBE / RGBM HDR encoding
///          - Unsigned and signed fixed-point quantization
///          - 8-bit / 16-bit per-channel interleaving
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_PACKING_HLSLI
#define GP_PACKING_HLSLI

// ─── Half Float (16-bit) ──────────────────────────────────────────────────────────────────────

/// Pack a float2 into a single UInt32 as two 16-bit floats (R16G16F layout).
UInt32 GP_PackFloat2ToR16G16F(Float2 v)
{
    return f32tof16(v.x) | (f32tof16(v.y) << 16u);
}

/// Unpack two 16-bit floats from a single UInt32.
Float2 GP_UnpackR16G16FToFloat2(UInt32 packed)
{
    return Float2(f16tof32(packed & 0xFFFFu), f16tof32(packed >> 16u));
}

/// Pack a float4 into two UInt32s as four 16-bit floats.
UInt2 GP_PackFloat4ToR16G16B16A16F(Float4 v)
{
    return UInt2(GP_PackFloat2ToR16G16F(v.xy), GP_PackFloat2ToR16G16F(v.zw));
}

/// Unpack four 16-bit floats from two UInt32s.
Float4 GP_UnpackR16G16B16A16FToFloat4(UInt2 packed)
{
    return Float4(GP_UnpackR16G16FToFloat2(packed.x), GP_UnpackR16G16FToFloat2(packed.y));
}

// ─── R11G11B10F ───────────────────────────────────────────────────────────────────────────────
// Used for HDR emission, irradiance, and light colors where alpha is not needed.
// R/G have 6 mantissa bits + 5 exponent bits; B has 5 mantissa + 5 exponent.
// Range: [0, 65024]. Alpha channel is discarded.

/// Pack a linear HDR RGB color into R11G11B10F (D3D native format bit layout).
UInt32 GP_PackR11G11B10F(Float3 rgb)
{
    UInt3 v;
    v.x = f32tof16(rgb.r);
    v.y = f32tof16(rgb.g);
    v.z = f32tof16(rgb.b);

    // R11: bits [0..10]  = sign(1) discarded, exp(5) + mantissa(6) from f16
    // G11: bits [11..21] = same
    // B10: bits [22..31] = sign(1) discarded, exp(5) + mantissa(5) from f16 (drop 1 mantissa bit)
    return ((v.x >> 1u) & 0x7FFu)
         | (((v.y >> 1u) & 0x7FFu) << 11u)
         | (((v.z >> 2u) & 0x3FFu) << 22u);
}

/// Unpack R11G11B10F back to a linear HDR float3.
Float3 GP_UnpackR11G11B10F(UInt32 packed)
{
    UInt3 v;
    v.x = (packed         & 0x7FFu) << 1u;
    v.y = ((packed >> 11u) & 0x7FFu) << 1u;
    v.z = ((packed >> 22u) & 0x3FFu) << 2u;
    return Float3(f16tof32(v.x), f16tof32(v.y), f16tof32(v.z));
}

// ─── R10G10B10A2 ──────────────────────────────────────────────────────────────────────────────
// Used for storing unit vectors (normals) at 10-bit precision per component + 2-bit alpha.

/// Pack a float4 with components in [0,1] into R10G10B10A2_UNORM (10 bits each for R/G/B, 2 for A).
UInt32 GP_PackR10G10B10A2Unorm(Float4 v)
{
    v = saturate(v);
    UInt32 r = (UInt32)(v.r * 1023.0f + 0.5f);
    UInt32 g = (UInt32)(v.g * 1023.0f + 0.5f);
    UInt32 b = (UInt32)(v.b * 1023.0f + 0.5f);
    UInt32 a = (UInt32)(v.a * 3.0f    + 0.5f);
    return r | (g << 10u) | (b << 20u) | (a << 30u);
}

/// Unpack R10G10B10A2_UNORM to a float4 in [0,1].
Float4 GP_UnpackR10G10B10A2Unorm(UInt32 packed)
{
    return Float4(
        (Float32)((packed)       & 0x3FFu) / 1023.0f,
        (Float32)((packed >> 10u) & 0x3FFu) / 1023.0f,
        (Float32)((packed >> 20u) & 0x3FFu) / 1023.0f,
        (Float32)((packed >> 30u) & 0x3u)   / 3.0f
    );
}

// ─── Fixed-Point Quantization ─────────────────────────────────────────────────────────────────

/// Quantize a float in [0,1] to an N-bit unsigned integer. N must be in [1, 16].
UInt32 GP_QuantizeUNorm(Float32 x, UInt32 bits)
{
    Float32 maxVal = (Float32)((1u << bits) - 1u);
    return (UInt32)(saturate(x) * maxVal + 0.5f);
}

/// Dequantize an N-bit unsigned integer to a float in [0,1].
Float32 GP_DequantizeUNorm(UInt32 x, UInt32 bits)
{
    Float32 maxVal = (Float32)((1u << bits) - 1u);
    return (Float32)x / maxVal;
}

/// Quantize a float in [-1,1] to an N-bit signed integer. N must be in [2, 16].
Int32 GP_QuantizeSNorm(Float32 x, UInt32 bits)
{
    Float32 maxVal = (Float32)((1u << (bits - 1u)) - 1u);
    return (Int32)(clamp(x, -1.0f, 1.0f) * maxVal + (x >= 0.0f ? 0.5f : -0.5f));
}

/// Dequantize an N-bit signed integer to a float in [-1,1].
Float32 GP_DequantizeSNorm(Int32 x, UInt32 bits)
{
    Float32 maxVal = (Float32)((1u << (bits - 1u)) - 1u);
    return clamp((Float32)x / maxVal, -1.0f, 1.0f);
}

// ─── RGBE (Radiance HDR) ──────────────────────────────────────────────────────────────────────
// Ward's RGBE format: shared exponent encoding for HDR colors.
// Range: [2^-128, 2^127]. Used in .hdr / .pic panoramas and for compact HDR storage.

/// Encode a linear HDR RGB color to RGBE (4 bytes, shared exponent).
UInt32 GP_EncodeRGBE(Float3 rgb)
{
    Float32 maxComp = max(max(rgb.r, rgb.g), rgb.b);
    if (maxComp < 1e-32f)
        return 0u;

    Float32 exp;
    Float32 scale = frexp(maxComp, exp) * 256.0f / maxComp;

    UInt32 r = (UInt32)(saturate(rgb.r * scale));
    UInt32 g = (UInt32)(saturate(rgb.g * scale));
    UInt32 b = (UInt32)(saturate(rgb.b * scale));
    UInt32 e = (UInt32)(exp + 128.0f);

    return r | (g << 8u) | (b << 16u) | (e << 24u);
}

/// Decode an RGBE-encoded value to linear HDR RGB.
Float3 GP_DecodeRGBE(UInt32 rgbe)
{
    Float32 r = (Float32)((rgbe)       & 0xFFu);
    Float32 g = (Float32)((rgbe >> 8u)  & 0xFFu);
    Float32 b = (Float32)((rgbe >> 16u) & 0xFFu);
    Float32 e = (Float32)((rgbe >> 24u) & 0xFFu);

    Float32 scale = exp2(e - 128.0f - 8.0f);
    return Float3(r, g, b) * scale;
}

// ─── RGBM ─────────────────────────────────────────────────────────────────────────────────────
// Multiplier-based HDR encoding. Better precision than RGBE for a narrower dynamic range.
// Typical maxRange = 6 (covers up to 6x white, i.e. ~1.8 EV above 1.0).

/// Encode a linear HDR RGB to RGBM (RGB + shared multiplier in alpha).
/// \param rgb       Linear HDR color. Values above maxRange will be clipped.
/// \param maxRange  Maximum representable value (typically 6.0).
Float4 GP_EncodeRGBM(Float3 rgb, Float32 maxRange = 6.0f)
{
    Float32 m = GP_MaxComponent(rgb) / maxRange;
    m = ceil(m * 255.0f) / 255.0f;
    m = max(m, 1.0f / 255.0f);
    return Float4(saturate(rgb / (m * maxRange)), m);
}

/// Decode an RGBM value back to linear HDR RGB.
Float3 GP_DecodeRGBM(Float4 rgbm, Float32 maxRange = 6.0f)
{
    return rgbm.rgb * rgbm.a * maxRange;
}

// ─── Octahedral Normal (see NormalEncoding.hlsli for full set) ───────────────────────────────
// Declared here for convenience since GBuffer packing references them.

/// Encode a unit vector to octahedral [−1, 1]^2 (wrap variant for hemisphere use).
Float2 GP_OctEncode(Float3 n)
{
    Float2 p = n.xy / (abs(n.x) + abs(n.y) + abs(n.z));
    return (n.z <= 0.0f) ? ((1.0f - abs(p.yx)) * sign(p)) : p;
}

/// Decode octahedral [−1, 1]^2 back to a unit vector.
Float3 GP_OctDecode(Float2 e)
{
    Float3 v = Float3(e.xy, 1.0f - abs(e.x) - abs(e.y));
    if (v.z < 0.0f)
        v.xy = (1.0f - abs(v.yx)) * sign(v.xy);
    return normalize(v);
}

/// Encode to [0,1]^2 unorm for 8-bit or 16-bit storage.
Float2 GP_OctEncodeUnorm(Float3 n)
{
    return GP_OctEncode(n) * 0.5f + 0.5f;
}

/// Decode from [0,1]^2 unorm.
Float3 GP_OctDecodeUnorm(Float2 e)
{
    return GP_OctDecode(e * 2.0f - 1.0f);
}

// ─── Packed UInt Bit Manipulation ────────────────────────────────────────────────────────────

/// Extract bits [offset, offset+count) from a UInt32.
UInt32 GP_ExtractBits(UInt32 v, UInt32 offset, UInt32 count)
{
    return (v >> offset) & ((1u << count) - 1u);
}

/// Insert `count` bits from `value` into `dest` starting at `offset`.
UInt32 GP_InsertBits(UInt32 dest, UInt32 value, UInt32 offset, UInt32 count)
{
    UInt32 mask = ((1u << count) - 1u) << offset;
    return (dest & ~mask) | ((value << offset) & mask);
}

/// Reverse the bits of a UInt32 (used in Halton / radical inverse sequences).
UInt32 GP_ReverseBits32(UInt32 v)
{
    return reversebits(v);
}

/// Convert a UInt32 to a float in [0, 1) using the full 23-bit mantissa.
Float32 GP_UIntToUNorm(UInt32 v)
{
    return (Float32)(v >> 9u) * (1.0f / (Float32)(1u << 23u));
}

#endif // GP_PACKING_HLSLI
