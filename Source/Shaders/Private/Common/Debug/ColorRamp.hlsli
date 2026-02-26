/// \file    ColorRamp.hlsli
/// \brief   False-color visualization ramps for debug and profiling overlays.
///          Used by: VisualizeGBuffer, VisualizeLightCount, Overdraw, Hi-Z, SDF debug views.
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_COLOR_RAMP_HLSLI
#define GP_COLOR_RAMP_HLSLI

// ─── Turbo Colormap ───────────────────────────────────────────────────────────────────────────
// Google's Turbo colormap: a perceptually uniform rainbow ramp from dark blue to dark red.
// Superior to Jet/HSV: no false detail in the yellow band, better luminance uniformity.
// Reference: https://ai.googleblog.com/2019/08/turbo-improved-rainbow-colormap.html
//
// This is the polynomial approximation — no texture lookup needed.

/// Map a scalar t in [0,1] to a Turbo colormap RGB color.
Float3 GP_TurboColormap(Float32 t)
{
    t = saturate(t);

    static const Float4 kRedVec4   = Float4(0.13572138f,  4.61539260f, -42.66032258f, 132.13108234f);
    static const Float4 kGreenVec4 = Float4(0.09140261f,  2.19418839f,   4.84296658f, -14.18503333f);
    static const Float4 kBlueVec4  = Float4(0.10667330f, 12.64194608f, -60.58204836f, 110.36276771f);
    static const Float2 kRedVec2   = Float2(-152.94239396f,  59.28637943f);
    static const Float2 kGreenVec2 = Float2(  4.27729857f,   2.82956604f);
    static const Float2 kBlueVec2  = Float2(-89.90310912f,  27.34824973f);

    Float4 v4 = Float4(1.0f, t, t*t, t*t*t);
    Float2 v2 = Float2(v4.z * v4.z, v4.w * v4.z);

    return saturate(Float3(
        dot(v4, kRedVec4)   + dot(v2, kRedVec2),
        dot(v4, kGreenVec4) + dot(v2, kGreenVec2),
        dot(v4, kBlueVec4)  + dot(v2, kBlueVec2)
    ));
}

// ─── Viridis Colormap ─────────────────────────────────────────────────────────────────────────
// Perceptually uniform, colorblind-safe, monotonically increasing luminance.
// Preferred for accessibility-sensitive overlays.
// Polynomial approximation by Matías Capeletto.

/// Map a scalar t in [0,1] to a Viridis colormap RGB color.
Float3 GP_ViridisColormap(Float32 t)
{
    t = saturate(t);

    static const Float3 c0 = Float3(0.2777273272234177f,  0.005407344544966578f, 0.3340998053353061f);
    static const Float3 c1 = Float3(0.1050930431085774f,  1.404613529898575f,    1.384590162594685f);
    static const Float3 c2 = Float3(-0.3308618287255563f, 0.214847559468213f,   -0.09500909532357232f);
    static const Float3 c3 = Float3(-4.634230498983486f, -5.799100973351585f,   -19.33244095627987f);
    static const Float3 c4 = Float3(6.228269936347081f,   14.17993336680509f,    56.69055260067819f);
    static const Float3 c5 = Float3(4.776384997670288f,  -13.74514537774601f,   -65.35303263337234f);
    static const Float3 c6 = Float3(-5.435455855934631f,  4.645852612178535f,    26.3124352495832f);

    return saturate(c0 + t*(c1 + t*(c2 + t*(c3 + t*(c4 + t*(c5 + t*c6))))));
}

// ─── Magma Colormap ───────────────────────────────────────────────────────────────────────────
// Dark-to-bright perceptually uniform ramp (black → purple → orange → white).
// Excellent for heat maps, luminance visualization, and HDR range display.

/// Map a scalar t in [0,1] to a Magma colormap RGB color.
Float3 GP_MagmaColormap(Float32 t)
{
    t = saturate(t);

    static const Float3 c0 = Float3(-0.002136485053939582f, -0.000749655052795221f, -0.005386127855323933f);
    static const Float3 c1 = Float3(0.2516605407371642f,    0.6775232436837668f,   2.494026599312351f);
    static const Float3 c2 = Float3(8.353717279216625f,    -3.577719514958484f,    0.3144679030132573f);
    static const Float3 c3 = Float3(-27.66873308576866f,    14.26473078096533f,   -13.64921318813922f);
    static const Float3 c4 = Float3(52.17613981234068f,    -27.94360607168351f,    12.94416944865686f);
    static const Float3 c5 = Float3(-50.76852536473588f,    29.04658282127291f,     4.23415299384598f);
    static const Float3 c6 = Float3(18.65570506591883f,    -11.48977351997711f,   -5.601961508648656f);

    return saturate(c0 + t*(c1 + t*(c2 + t*(c3 + t*(c4 + t*(c5 + t*c6))))));
}

// ─── Jet (Legacy) ─────────────────────────────────────────────────────────────────────────────
// Classic rainbow ramp. Perceptually non-uniform but widely recognized.
// Use Turbo instead for new work. Provided for legacy debug shader compatibility.

/// Map a scalar t in [0,1] to a Jet colormap RGB color.
Float3 GP_JetColormap(Float32 t)
{
    t = saturate(t);
    Float3 c;
    c.r = saturate(1.5f - abs(4.0f * t - 3.0f));
    c.g = saturate(1.5f - abs(4.0f * t - 2.0f));
    c.b = saturate(1.5f - abs(4.0f * t - 1.0f));
    return c;
}

// ─── Grayscale / Luminance ────────────────────────────────────────────────────────────────────

/// Map a scalar to a grayscale float3.
Float3 GP_GrayscaleRamp(Float32 t) { return (Float3)saturate(t); }

/// Map a scalar to a black-body radiation color (temperature: 0=black, 0.5=red, 1=white-hot).
Float3 GP_BlackBody(Float32 t)
{
    t = saturate(t);
    return Float3(
        saturate(t * 3.0f),
        saturate(t * 3.0f - 1.0f),
        saturate(t * 3.0f - 2.0f)
    );
}

// ─── Overdraw Heatmap ─────────────────────────────────────────────────────────────────────────
// Specialized ramp for overdraw visualization: 1 draw = green, 4+ = red.

/// Map an overdraw count (0 = untouched, 1 = 1 draw, etc.) to a warning color.
Float3 GP_OverdrawColor(UInt32 count)
{
    // Normalize: 0 draws = black, 1-3 = green→yellow, 4+ = red
    Float32 t = min((Float32)count / 8.0f, 1.0f);
    return GP_JetColormap(t);
}

// ─── Light Count Heatmap ──────────────────────────────────────────────────────────────────────

/// Map a per-tile light count to a heat color. 0 lights = dark, 32+ = hot red.
Float3 GP_LightCountColor(UInt32 lightCount)
{
    Float32 t = min((Float32)lightCount / 32.0f, 1.0f);
    return GP_TurboColormap(t);
}

// ─── Normal Visualization ─────────────────────────────────────────────────────────────────────

/// Visualize a world-space or tangent-space normal as an RGB color.
/// Maps [-1,1] → [0,1] per channel. The classic "normal map blue" result for up-facing normals.
Float3 GP_VisualizeNormal(Float3 normal)
{
    return normal * 0.5f + 0.5f;
}

// ─── Depth Visualization ──────────────────────────────────────────────────────────────────────

/// Visualize a linear depth value with a false-color ramp.
/// Values near the near plane get cool colors, far values get warm colors.
Float3 GP_VisualizeLinearDepth(Float32 linearDepth, Float32 nearPlane, Float32 farPlane)
{
    Float32 t = GP_LinearDepthNormalize(linearDepth, nearPlane, farPlane);
    return GP_TurboColormap(t);
}

// ─── Motion Vector Visualization ─────────────────────────────────────────────────────────────

/// Visualize a 2D motion vector as hue (direction) + saturation (magnitude).
/// Stationary pixels = gray. Magnitude is clamped to 64 pixels for color saturation.
Float3 GP_VisualizeMotionVector(Float2 motionNDC, Float2 renderResolution)
{
    Float2 motionPixels = motionNDC * renderResolution * 0.5f; // NDC → pixels
    Float32 magnitude   = length(motionPixels);
    Float32 angle       = atan2(motionPixels.y, motionPixels.x) * GP_INV_TWO_PI; // [0,1]
    Float32 saturation  = saturate(magnitude / 64.0f);

    // HSV: hue = direction, saturation = magnitude, value = 1
    Float3 hsv = Float3(angle >= 0.0f ? angle : angle + 1.0f, saturation, 1.0f);
    return GP_HSVToRGB(hsv);
}

// ─── Validation Colors ────────────────────────────────────────────────────────────────────────
// Strongly recognizable error/warning colors for NaN detection, out-of-range values, etc.

static const Float3 GP_COLOR_ERROR_NAN      = Float3(1.0f, 0.0f, 1.0f);  // Magenta = NaN
static const Float3 GP_COLOR_ERROR_INF      = Float3(1.0f, 1.0f, 0.0f);  // Yellow  = Inf
static const Float3 GP_COLOR_ERROR_NEGATIVE = Float3(0.0f, 0.0f, 1.0f);  // Blue    = Negative
static const Float3 GP_COLOR_OK             = Float3(0.0f, 1.0f, 0.0f);  // Green   = OK

/// Return an error color if the value contains NaN or Inf, otherwise return the value.
/// Use in the final composite pass to catch rendering errors in debug builds.
Float3 GP_ValidateColor(Float3 color)
{
    if (any(isnan(color))) return GP_COLOR_ERROR_NAN;
    if (any(isinf(color))) return GP_COLOR_ERROR_INF;
    if (any(color < 0.0f)) return GP_COLOR_ERROR_NEGATIVE;
    return color;
}

Float4 GP_ValidateColor(Float4 color)
{
    return Float4(GP_ValidateColor(color.rgb), color.a);
}

#endif // GP_COLOR_RAMP_HLSLI
