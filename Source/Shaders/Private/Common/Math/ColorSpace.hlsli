/// \file    ColorSpace.hlsli
/// \brief   Color space conversion utilities. All engine rendering is done in linear RGB
///          (Rec. 709 primaries unless otherwise noted). This file provides transforms
///          to and from display-referred spaces, perceptual models, and HDR formats.
///
/// \note    Workflow assumption:
///          - Input textures are stored in sRGB (GPU hardware gamma-decodes on sample).
///          - All lighting math is in scene-linear Rec. 709.
///          - Output to display goes through tone mapping → display gamma (see ToneMapping/).
///          - HDR displays receive linear output in the Rec. 2020 / ST.2084 (PQ) domain.
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_COLOR_SPACE_HLSLI
#define GP_COLOR_SPACE_HLSLI

// ─── sRGB ↔ Linear ────────────────────────────────────────────────────────────────────────────
// Used when textures are sampled without hardware decode (e.g., from UAVs, unorm formats).

/// Convert a single channel from sRGB (gamma-encoded) to linear.
Float32 GP_sRGBToLinear(Float32 srgb)
{
    return (srgb <= 0.04045f)
        ? srgb / 12.92f
        : pow((srgb + 0.055f) / 1.055f, 2.4f);
}

/// Convert a float3 from sRGB to linear.
Float3 GP_sRGBToLinear(Float3 srgb)
{
    return Float3(GP_sRGBToLinear(srgb.r),
                  GP_sRGBToLinear(srgb.g),
                  GP_sRGBToLinear(srgb.b));
}

/// Convert a single channel from linear to sRGB (gamma encode).
Float32 GP_LinearToSRGB(Float32 lin)
{
    return (lin <= 0.0031308f)
        ? lin * 12.92f
        : 1.055f * pow(lin, 1.0f / 2.4f) - 0.055f;
}

/// Convert a float3 from linear to sRGB.
Float3 GP_LinearToSRGB(Float3 lin)
{
    return Float3(GP_LinearToSRGB(lin.r),
                  GP_LinearToSRGB(lin.g),
                  GP_LinearToSRGB(lin.b));
}

/// Fast approximation: pow(x, 2.2) — use when perceptual accuracy is not critical.
Float3 GP_sRGBToLinearFast(Float3 srgb) { return pow(srgb, 2.2f); }
Float3 GP_LinearToSRGBFast(Float3 lin)  { return pow(lin, 1.0f / 2.2f); }

// ─── Rec. 2020 / Wide Gamut ───────────────────────────────────────────────────────────────────

/// Transform Rec. 709 (sRGB primaries) to Rec. 2020 primaries.
/// Matrix from ITU-R BT.2087.
Float3 GP_Rec709ToRec2020(Float3 rgb709)
{
    static const Float3x3 M =
    {
         0.6274040f, 0.3292820f, 0.0433136f,
         0.0690970f, 0.9195400f, 0.0113612f,
         0.0163916f, 0.0880132f, 0.8955950f
    };
    return mul(M, rgb709);
}

/// Transform Rec. 2020 back to Rec. 709.
Float3 GP_Rec2020ToRec709(Float3 rgb2020)
{
    static const Float3x3 M =
    {
         1.6604910f, -0.5876411f, -0.0728499f,
        -0.1245505f,  1.1328999f, -0.0083494f,
        -0.0181508f, -0.1005789f,  1.1187297f
    };
    return mul(M, rgb2020);
}

// ─── ST.2084 (PQ — Perceptual Quantizer) ─────────────────────────────────────────────────────
// HDR10 output format. Encodes luminance in [0, 10000] nits into a normalized [0, 1] signal.

static const Float32 GP_PQ_M1 = 0.1593017578125f;
static const Float32 GP_PQ_M2 = 78.84375f;
static const Float32 GP_PQ_C1 = 0.8359375f;
static const Float32 GP_PQ_C2 = 18.8515625f;
static const Float32 GP_PQ_C3 = 18.6875f;

/// Encode linear scene luminance (in nits, relative to 10000 nit peak) to ST.2084 PQ.
Float32 GP_LinearToPQ(Float32 linearNits)
{
    Float32 y  = linearNits / 10000.0f;
    Float32 yp = pow(max(y, 0.0f), GP_PQ_M1);
    return pow((GP_PQ_C1 + GP_PQ_C2 * yp) / (1.0f + GP_PQ_C3 * yp), GP_PQ_M2);
}

Float3 GP_LinearToPQ(Float3 linearNits)
{
    return Float3(GP_LinearToPQ(linearNits.r),
                  GP_LinearToPQ(linearNits.g),
                  GP_LinearToPQ(linearNits.b));
}

/// Decode ST.2084 PQ to linear scene luminance (nits).
Float32 GP_PQToLinear(Float32 pq)
{
    Float32 yp = pow(max(pq, 0.0f), 1.0f / GP_PQ_M2);
    return 10000.0f * pow(max(yp - GP_PQ_C1, 0.0f) / (GP_PQ_C2 - GP_PQ_C3 * yp), 1.0f / GP_PQ_M1);
}

// ─── HLG (Hybrid Log-Gamma) ───────────────────────────────────────────────────────────────────
// BBC/NHK HDR format. Backward-compatible with SDR displays.

/// Encode linear luminance to HLG.
Float32 GP_LinearToHLG(Float32 lin)
{
    static const Float32 a = 0.17883277f;
    static const Float32 b = 0.28466892f;
    static const Float32 c = 0.55991073f;
    return (lin <= 1.0f / 12.0f)
        ? sqrt(3.0f * lin)
        : a * log(12.0f * lin - b) + c;
}

Float3 GP_LinearToHLG(Float3 lin)
{
    return Float3(GP_LinearToHLG(lin.r), GP_LinearToHLG(lin.g), GP_LinearToHLG(lin.b));
}

// ─── OKLab ────────────────────────────────────────────────────────────────────────────────────
// Perceptually uniform color space by Björn Ottosson (2020).
// Used for perceptual color interpolation, color grading, and chroma operations.

/// Convert linear sRGB to OKLab (perceptually uniform Lab space).
Float3 GP_LinearSRGBToOKLab(Float3 rgb)
{
    Float3 l = Float3(
        0.4122214708f * rgb.r + 0.5363325363f * rgb.g + 0.0514459929f * rgb.b,
        0.2119034982f * rgb.r + 0.6806995451f * rgb.g + 0.1073969566f * rgb.b,
        0.0883024619f * rgb.r + 0.2817188376f * rgb.g + 0.6299787005f * rgb.b
    );

    Float3 cbrt_l = sign(l) * pow(abs(l), 1.0f / 3.0f);

    return Float3(
        0.2104542553f * cbrt_l.x + 0.7936177850f * cbrt_l.y - 0.0040720468f * cbrt_l.z,
        1.9779984951f * cbrt_l.x - 2.4285922050f * cbrt_l.y + 0.4505937099f * cbrt_l.z,
        0.0259040371f * cbrt_l.x + 0.7827717662f * cbrt_l.y - 0.8086757660f * cbrt_l.z
    );
}

/// Convert OKLab back to linear sRGB.
Float3 GP_OKLabToLinearSRGB(Float3 lab)
{
    Float3 lms_ = Float3(
        lab.x + 0.3963377774f * lab.y + 0.2158037573f * lab.z,
        lab.x - 0.1055613458f * lab.y - 0.0638541728f * lab.z,
        lab.x - 0.0894841775f * lab.y - 1.2914855480f * lab.z
    );
    Float3 lms = lms_ * lms_ * lms_;

    return Float3(
         4.0767416621f * lms.x - 3.3077115913f * lms.y + 0.2309699292f * lms.z,
        -1.2684380046f * lms.x + 2.6097574011f * lms.y - 0.3413193965f * lms.z,
        -0.0041960863f * lms.x - 0.7034186147f * lms.y + 1.7076147010f * lms.z
    );
}

/// Perceptual color mix in OKLab space. More visually neutral than linear RGB lerp.
Float3 GP_OKLabLerp(Float3 rgbA, Float3 rgbB, Float32 t)
{
    return GP_OKLabToLinearSRGB(lerp(GP_LinearSRGBToOKLab(rgbA), GP_LinearSRGBToOKLab(rgbB), t));
}

// ─── HSV ↔ RGB ───────────────────────────────────────────────────────────────────────────────

/// Convert HSV (H in [0,360], S and V in [0,1]) to linear RGB.
Float3 GP_HSVToRGB(Float3 hsv)
{
    Float4 k = Float4(1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 3.0f);
    Float3 p = abs(frac(Float3(hsv.x, hsv.x, hsv.x) + k.xyz) * 6.0f - k.www);
    return hsv.z * lerp(k.xxx, saturate(p - k.xxx), hsv.y);
}

/// Convert linear RGB to HSV.
Float3 GP_RGBToHSV(Float3 rgb)
{
    Float4 k  = Float4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
    Float4 p  = lerp(Float4(rgb.bg, k.wz), Float4(rgb.gb, k.xy), step(rgb.b, rgb.g));
    Float4 q  = lerp(Float4(p.xyw, rgb.r), Float4(rgb.r, p.yzx), step(p.x, rgb.r));
    Float32 d = q.x - min(q.w, q.y);
    Float32 e = 1e-10f;
    return Float3(abs(q.z + (q.w - q.y) / (6.0f * d + e)), d / (q.x + e), q.x);
}

// ─── Chromaticity ─────────────────────────────────────────────────────────────────────────────

/// Compute chromaticity (xy) from a linear RGB color (Rec. 709).
Float2 GP_RGBToChromaticity(Float3 rgb)
{
    static const Float3x3 M_to_XYZ =
    {
        0.4124564f, 0.3575761f, 0.1804375f,
        0.2126729f, 0.7151522f, 0.0721750f,
        0.0193339f, 0.1191920f, 0.9503041f
    };
    Float3 xyz = mul(M_to_XYZ, rgb);
    Float32 sum = xyz.x + xyz.y + xyz.z;
    return (sum > GP_EPSILON) ? xyz.xy / sum : Float2(0.3127f, 0.3290f); // D65 white
}

// ─── Log Color Spaces ─────────────────────────────────────────────────────────────────────────

/// ACEScc log encoding (used in ACES grading pipeline).
Float32 GP_LinearToACEScc(Float32 lin)
{
    if (lin <= 0.0f)
        return -0.3584474886f;
    else if (lin < exp2(-15.0f))
        return (log2(exp2(-16.0f) + lin * 0.5f) + 9.72f) / 17.52f;
    else
        return (log2(lin) + 9.72f) / 17.52f;
}

/// ACEScc decode back to linear.
Float32 GP_ACESccToLinear(Float32 aces)
{
    if (aces < (9.72f - 15.0f) / 17.52f)
        return exp2(aces * 17.52f - 9.72f) * 2.0f - exp2(-16.0f);
    else if (aces < (log2(GP_LARGE_FLOAT) + 9.72f) / 17.52f)
        return exp2(aces * 17.52f - 9.72f);
    else
        return GP_LARGE_FLOAT;
}

// ─── Display Encoding Helpers ─────────────────────────────────────────────────────────────────

/// Full HDR10 display transform: scene-linear → Rec.2020 → PQ.
/// \param linearRec709  Scene-linear RGB in Rec.709 primaries (after tone mapping).
/// \param peakNits      Display peak luminance in nits (typically 1000–10000).
Float3 GP_ToHDR10(Float3 linearRec709, Float32 peakNits = 1000.0f)
{
    Float3 rec2020 = GP_Rec709ToRec2020(linearRec709);
    Float3 nits    = rec2020 * peakNits;
    return GP_LinearToPQ(nits);
}

/// Standard SDR display transform: scene-linear → sRGB gamma encode.
Float3 GP_ToSDR(Float3 linearRec709)
{
    return GP_LinearToSRGB(saturate(linearRec709));
}

#endif // GP_COLOR_SPACE_HLSLI
