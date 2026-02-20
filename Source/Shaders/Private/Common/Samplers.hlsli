// Copyright (c) - Graphical Playground. All rights reserved.

/// \file    Samplers.hlsli
/// \brief   Global static sampler declarations. All samplers used by the engine are declared
///          here and bound via the root signature's static sampler table, consuming no
///          descriptor heap slots.
///
/// \note    Register layout:
///          s0  - PointClamp
///          s1  - PointWrap
///          s2  - PointMirror
///          s3  - LinearClamp
///          s4  - LinearWrap
///          s5  - LinearMirror
///          s6  - AnisotropicClamp      (16x, mip bias from camera constants)
///          s7  - AnisotropicWrap       (16x)
///          s8  - AnisotropicMirror     (16x)
///          s9  - ShadowCompareLess     (PCF shadow sampler, linear, clamp to border 1.0)
///          s10 - ShadowCompareLessEq   (PCF, less-or-equal, for reversed-Z compatibility)
///          s11 - DepthMinReduction     (min reduction for Hi-Z chain construction)
///          s12 - DepthMaxReduction     (max reduction for conservative Hi-Z)
///          s13 - LinearClampNoMip      (LOD clamped to 0, for single-mip lookups)
///          s14 - CubemapLinearClamp    (IBL cubemap sampler with trilinear filtering)
///
/// \warning Never include this file directly; always include Common.hlsli.
/// \warning Sampler register assignments must match GPRootSignature.cpp on the CPU side exactly.

#ifndef GP_SAMPLERS_HLSLI
#define GP_SAMPLERS_HLSLI

// ─── Point Samplers ───────────────────────────────────────────────────────────────────────────

/// Point-filtered, clamp on all axes. Used for depth buffer reads, G-Buffer reconstruction,
/// and any pass where sub-pixel filtering would introduce incorrect values.
SamplerState GP_SamplerPointClamp   : register(s0);

/// Point-filtered, wrap on all axes. Used for tiling noise textures and debug patterns.
SamplerState GP_SamplerPointWrap    : register(s1);

/// Point-filtered, mirror on all axes.
SamplerState GP_SamplerPointMirror  : register(s2);

// ─── Linear / Bilinear Samplers ───────────────────────────────────────────────────────────────

/// Bilinear with trilinear mip interpolation, clamp to edge. Standard sampler for most
/// texture lookups that need smooth filtering but controlled edge behavior.
SamplerState GP_SamplerLinearClamp  : register(s3);

/// Bilinear + trilinear mip, wrap. Standard tiling texture sampler (albedo, normal maps, etc.).
SamplerState GP_SamplerLinearWrap   : register(s4);

/// Bilinear + trilinear mip, mirror.
SamplerState GP_SamplerLinearMirror : register(s5);

// ─── Anisotropic Samplers ─────────────────────────────────────────────────────────────────────

/// 16x anisotropic, clamp. For surface textures viewed at extreme angles.
SamplerState GP_SamplerAnisoClamp   : register(s6);

/// 16x anisotropic, wrap. Primary sampler for world-space surface materials.
SamplerState GP_SamplerAnisoWrap    : register(s7);

/// 16x anisotropic, mirror.
SamplerState GP_SamplerAnisoMirror  : register(s8);

// ─── Shadow / Comparison Samplers ────────────────────────────────────────────────────────────

/// PCF shadow sampler. Linear filter across a 2x2 footprint with hardware percentage-closer
/// filtering. Less-than comparison (depth < reference = lit).
/// Use with SampleCmpLevelZero() for hardware PCF.
SamplerComparisonState GP_SamplerShadowLess     : register(s9);

/// PCF shadow sampler with less-or-equal comparison. Used with reversed-Z depth buffers
/// where values are stored as (1 - z/w). Provides correct comparisons after the flip.
SamplerComparisonState GP_SamplerShadowLessEq   : register(s10);

// ─── Reduction Samplers ───────────────────────────────────────────────────────────────────────

/// Min-reduction sampler for Hi-Z mip chain construction. Returns the minimum depth in the
/// 2x2 footprint. Used by HiZ_Build.CS.hlsl to build the occlusion culling pyramid.
SamplerState GP_SamplerDepthMinReduce : register(s11);

/// Max-reduction sampler for conservative Hi-Z (returns furthest depth = smallest coverage).
SamplerState GP_SamplerDepthMaxReduce : register(s12);

// ─── Special Purpose ──────────────────────────────────────────────────────────────────────────

/// Bilinear clamp, LOD forced to 0. Used when sampling from single-mip intermediate targets
/// (shadow maps, GBuffer targets, SSAO, etc.) where mip selection must be exact.
SamplerState GP_SamplerLinearClampL0  : register(s13);

/// Trilinear clamp for IBL cubemap lookups. Used by IBL.hlsli for environment map sampling
/// across the specular mip chain (roughness → mip level mapping).
SamplerState GP_SamplerCubemap        : register(s14);

// ─── Convenience Aliases ──────────────────────────────────────────────────────────────────────
// Short names for the most commonly accessed samplers inside shader code.

#define GP_SS_POINT_CLAMP       GP_SamplerPointClamp
#define GP_SS_POINT_WRAP        GP_SamplerPointWrap
#define GP_SS_LINEAR_CLAMP      GP_SamplerLinearClamp
#define GP_SS_LINEAR_WRAP       GP_SamplerLinearWrap
#define GP_SS_ANISO_WRAP        GP_SamplerAnisoWrap
#define GP_SS_ANISO_CLAMP       GP_SamplerAnisoClamp
#define GP_SS_SHADOW            GP_SamplerShadowLess
#define GP_SS_SHADOW_REV_Z      GP_SamplerShadowLessEq
#define GP_SS_HIZ               GP_SamplerDepthMinReduce
#define GP_SS_CUBEMAP           GP_SamplerCubemap

#endif // GP_SAMPLERS_HLSLI
