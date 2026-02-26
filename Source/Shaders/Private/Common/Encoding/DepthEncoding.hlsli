/// \file    DepthEncoding.hlsli
/// \brief   Depth buffer encoding, decoding, and linearization utilities.
///          Covers standard-Z, reversed-Z, logarithmic depth, and Hi-Z pyramid operations.
///
/// \details Depth precision in games is notoriously problematic. This file centralizes all
///          depth transforms and makes the precision trade-offs explicit:
///
///          Standard-Z (near=0, far=1):
///            - Precision clusters near the near plane (exponential, good for close objects).
///            - Loses ~50% of integer bits to the first 1% of view range.
///            - Causes severe Z-fighting for distant objects.
///
///          Reversed-Z (near=1, far=0):
///            - Precision clusters near the far plane — counteracts the float exponent bias.
///            - Combined with a float depth buffer (D32F), gives nearly uniform precision.
///            - Engine default. All depth comparisons must flip their operator.
///
///          Logarithmic depth:
///            - Truly uniform precision in clip space.
///            - Requires vertex shader modification; breaks perspective interpolation.
///            - Used for special-purpose passes (atmospheric, ocean, planetary scale).
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_DEPTH_ENCODING_HLSLI
#define GP_DEPTH_ENCODING_HLSLI

// ─── Linearization ────────────────────────────────────────────────────────────────────────────

/// Convert a hardware depth buffer value to linear view-space depth (in world units).
/// \param depth  Raw depth from the depth buffer: reversed-Z means near=1.0, far=0.0.
/// \param near   Camera near plane distance (positive, in world units).
/// \param far    Camera far plane distance (positive, in world units).
///
/// For reversed-Z:     linearDepth = (near * far) / (depth * (far - near) + near)
/// For standard-Z:     linearDepth = (near * far) / (far - depth * (far - near))
Float32 GP_DepthToLinear(Float32 depth, Float32 near, Float32 far)
{
    // Reversed-Z formula (engine default). For standard-Z, negate depth - 1.
    return (near * far) / (depth * (far - near) + near);
}

/// Overload accepting a float2 projection matrix entry: (proj[2][2], proj[3][2]).
/// More efficient when called repeatedly in a CS — avoids recomputing the formula.
/// Derive projParams: projParams.x = far / (far - near), projParams.y = near * far / (near - far)
/// For reversed-Z projection: projParams.x = -far/(far-near) + 1, adjusted accordingly.
Float32 GP_DepthToLinearProj(Float32 depth, Float2 projParams)
{
    // projParams.x = proj[2][2], projParams.y = proj[3][2] (from camera constants)
    return projParams.y / (depth - projParams.x);
}

/// Convert a linear view-space depth back to a hardware depth buffer value (reversed-Z).
Float32 GP_LinearToDepth(Float32 linearDepth, Float32 near, Float32 far)
{
    return (near * far) / (linearDepth * (far - near)) - near / (far - near);
}

/// Convert linear depth to [0,1] normalized (useful for visualization and hi-Z storage).
Float32 GP_LinearDepthNormalize(Float32 linearDepth, Float32 near, Float32 far)
{
    return (linearDepth - near) / (far - near);
}

// ─── Reversed-Z Comparison Helpers ───────────────────────────────────────────────────────────

/// Returns true if depthA is closer to the camera than depthB (reversed-Z: larger = closer).
bool GP_DepthIsCloser(Float32 depthA, Float32 depthB)
{
    return depthA > depthB;
}

/// Returns the closer (in camera terms) of two reversed-Z depth values.
Float32 GP_DepthCloser(Float32 a, Float32 b)
{
    return max(a, b); // Reversed-Z: larger value = closer to camera
}

/// Returns the farther (in camera terms) of two reversed-Z depth values.
Float32 GP_DepthFarther(Float32 a, Float32 b)
{
    return min(a, b); // Reversed-Z: smaller value = farther from camera
}

/// Hi-Z pyramid texel value for a conservative near bound (reversed-Z).
/// The Hi-Z stores the MAXIMUM depth in a tile (= closest geometry, reversed-Z).
Float32 GP_HiZCombine(Float32 a, Float32 b)
{
    return max(a, b);
}

// ─── Logarithmic Depth ────────────────────────────────────────────────────────────────────────
// Provides uniform precision across the entire view range at the cost of VS complexity.
// Used for: atmospheric shaders, ocean rendering, planetary-scale terrain.
//
// Clip-space logarithmic depth: z_log = log(C * w + 1) / log(C * far + 1) * w
// where C is a constant controlling precision distribution (typically 1.0).
// w is the clip-space W component. Multiply result by w to restore perspective divide.

static const Float32 GP_LOG_DEPTH_C = 1.0f;

/// Encode a clip-space W value to a logarithmic depth for writing to SV_Depth.
/// Call from vertex or domain shader.
Float32 GP_EncodeLogDepth(Float32 clipW, Float32 far)
{
    return log(GP_LOG_DEPTH_C * clipW + 1.0f) / log(GP_LOG_DEPTH_C * far + 1.0f) * clipW;
}

/// Decode a logarithmic depth value back to linear view depth.
Float32 GP_DecodeLogDepth(Float32 logDepth, Float32 far)
{
    Float32 denom = log(GP_LOG_DEPTH_C * far + 1.0f);
    return (exp(logDepth * denom) - 1.0f) / GP_LOG_DEPTH_C;
}

// ─── Hi-Z Mip Chain ──────────────────────────────────────────────────────────────────────────

/// Compute the Hi-Z mip level for a given world-space AABB footprint size.
/// Returns the mip at which a single texel covers the AABB's projected footprint.
/// \param screenExtent   Projected screen-space extent of the AABB (in pixels).
/// \param hiZMipCount    Total number of mip levels in the Hi-Z pyramid.
Float32 GP_HiZMipLevel(Float2 screenExtent, UInt32 hiZMipCount)
{
    Float32 maxExtent = max(screenExtent.x, screenExtent.y);
    return clamp(ceil(log2(max(maxExtent, 1.0f))), 0.0f, (Float32)(hiZMipCount - 1u));
}

/// Sample the Hi-Z pyramid at the correct mip for an AABB footprint.
/// Returns the conservative (farthest in reversed-Z = smallest value) depth for occlusion.
/// \param hiZIndex     Bindless descriptor index of the Hi-Z texture.
/// \param uv           Screen-space UV center of the AABB footprint.
/// \param mip          Mip level (from GP_HiZMipLevel).
Float32 GP_SampleHiZ(UInt32 hiZIndex, Float2 uv, Float32 mip)
{
    return GP_TEXTURE2D_FLOAT(hiZIndex).SampleLevel(GP_SS_HIZ, uv, mip).r;
}

// ─── MSAA Depth Resolve ───────────────────────────────────────────────────────────────────────

/// Resolve an MSAA depth buffer to single-sampled by taking the closest sample (reversed-Z: max).
/// \param msaaDepthIndex  Bindless index of the MSAA Texture2DMS<float>.
/// \param coord           Integer pixel coordinate.
/// \param sampleCount     MSAA sample count (2, 4, or 8).
Float32 GP_ResolveMSAADepth(UInt32 msaaDepthIndex, Int2 coord, UInt32 sampleCount)
{
    // Cannot use generic bindless for MS textures — must be explicitly typed.
    // In practice, MSAA resolve is done in ResolveDepth.CS.hlsl using a
    // Texture2DMS<float, 4> (or 2, 8) declared at a fixed register in that shader.
    // This helper documents the pattern for reference.
    return 0.0f; // Placeholder — see ResolveDepth.CS.hlsl for the real implementation.
}

// ─── Depth Bias ───────────────────────────────────────────────────────────────────────────────

/// Compute a world-space depth bias for shadow maps, accounting for surface slope.
/// Based on Kilgard's slope-scale depth bias formulation.
/// \param nDotL        Cosine of the angle between surface normal and light direction.
/// \param baseBias     Minimum bias in depth buffer units (typically 0.0001 for D32F).
/// \param slopeBias    Slope-scale factor (typically 1.0–3.0).
Float32 GP_ShadowDepthBias(Float32 nDotL, Float32 baseBias, Float32 slopeBias)
{
    Float32 sinTheta = sqrt(max(0.0f, 1.0f - nDotL * nDotL)); // sin from cos via Pythagoras
    Float32 tanTheta = sinTheta / max(nDotL, GP_EPSILON);
    return baseBias + slopeBias * tanTheta;
}

/// Normal-offset shadow bias: shift the shadow receiver position along its normal before
/// computing shadow map UV. Eliminates self-shadowing without adding receiver bias.
/// \param worldPos    World-space receiver position.
/// \param worldNormal World-space surface normal.
/// \param nDotL       N·L for the light direction (smaller = more bias needed).
/// \param biasScale   World-space offset scale (typically 0.005–0.02).
Float3 GP_NormalOffsetBias(Float3 worldPos, Float3 worldNormal, Float32 nDotL, Float32 biasScale)
{
    return worldPos + worldNormal * (1.0f - saturate(nDotL)) * biasScale;
}

// ─── Depth Buffer Visualization ───────────────────────────────────────────────────────────────

/// Remap a raw (reversed-Z) hardware depth to [0,1] for display.
/// Applies a log scale to make the full range visible — raw reversed-Z values
/// cluster near 1.0 and are nearly invisible when displayed directly.
Float32 GP_VisualizeDepth(Float32 hwDepth, Float32 near, Float32 far)
{
    Float32 linear = GP_DepthToLinear(hwDepth, near, far);
    return log(1.0f + linear) / log(1.0f + far); // Log scale: near details visible
}

#endif // GP_DEPTH_ENCODING_HLSLI
