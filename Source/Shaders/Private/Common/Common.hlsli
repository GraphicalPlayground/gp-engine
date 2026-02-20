// Copyright (c) - Graphical Playground. All rights reserved.

/// \file    Common.hlsli
/// \brief   Master include for all GP engine shaders. Every .hlsl file in the engine begins
///          with a single line: #include "Common/Common.hlsli"
///
/// \details Include order is strictly defined. Dependencies flow top-to-bottom:
///          1. Platform       — SM guards, type aliases, constants, annotation macros.
///          2. Samplers       — Static sampler declarations (no dependencies).
///          3. BindlessRes    — Descriptor heap accessors (depends on Platform for types,
///                              Samplers for GP_SS_* in safe-sample helpers).
///          4. RootSignature  — Root parameter bindings and per-draw constants
///                              (depends on Platform for types).
///          5. Math           — Pure math utilities (depend on Platform types/constants).
///          6. Encoding       — Higher-level data packing (depends on Math + Packing).
///
/// \note    This file is the ONLY include a shader author should write directly.
///          Internal cross-dependencies between Common/* headers are managed here.
///          Adding a new utility header: place it in the appropriate subdirectory, include it
///          below in the correct dependency order, and document it in this header.
///
/// \warning This file must remain free of any resource declarations (Texture2D, Buffer, etc.)
///          and any function definitions that are not inline/static. Anything declared at
///          file scope here appears in every single compiled shader — keep it minimal.

#ifndef GP_COMMON_HLSLI
#define GP_COMMON_HLSLI

// ─── Layer 1: Platform Abstraction ───────────────────────────────────────────────────────────
// Must be first. Defines type aliases, SM feature macros, and numeric constants that
// everything else depends on.

#include "Platform.hlsli"

// ─── Layer 2: Resource Binding Infrastructure ─────────────────────────────────────────────────
// Sampler declarations before BindlessResources because the safe-sample helpers in
// BindlessResources.hlsli reference GP_SS_* sampler aliases.

#include "Samplers.hlsli"
#include "BindlessResources.hlsli"

// ─── Layer 3: Root Signature and Per-Draw Constants ──────────────────────────────────────────
// Declares g_PerDraw (ConstantBuffer<FPerDrawConstants>) which is accessed in every
// vertex, pixel, and compute shader. Draw flag constants defined here too.

#include "RootSignature.hlsli"

// ─── Layer 4: Math Utilities ──────────────────────────────────────────────────────────────────
// Pure functions — no resource or cbuffer declarations. Safe to include in any order
// after Platform.hlsli. Ordered by dependency: MathCommon has no math dependencies,
// Packing uses UIntToUNorm from MathCommon, ColorSpace uses pow/log from MathCommon.

// TODO: Add mathematical includes here as needed.

// ─── Layer 5: Encoding ────────────────────────────────────────────────────────────────────────
// Higher-level encode/decode built on top of Math layer. NormalEncoding uses
// GP_OctEncode* defined in Packing.hlsli (included above for convenience, canonical
// versions with full documentation live here in Encoding/).

// TODO: Add encoding includes here as needed.

// ─── Layer 6: Vertex Utilities ────────────────────────────────────────────────────────────────
// Vertex fetch and transform helpers. Depend on Math (Transform, MathCommon) and
// BindlessResources (for manual vertex fetch from bindless byte buffers).

// TODO: Add vertex utility includes here as needed.

// ─── Layer 7: Debug ───────────────────────────────────────────────────────────────────────────
// Compiled out in Release builds (GP_BUILD_SHIPPING) by the shader compile flags.
// In Debug/Development, these add ~0 overhead unless explicitly called.

#ifndef GP_BUILD_SHIPPING
// TODO: Add debug includes here as needed.
#endif

// ─────────────────────────────────────────────────────────────────────────────────────────────
// FRAME CONSTANTS
// Declared here (not in RootSignature.hlsli) because they depend on types that are
// defined across multiple headers above (Float4x4, UInt32, etc. from Platform + Interop).
// The struct definition (FFrameConstants) lives in ShaderInterop_Frame.hpp and is
// validated by C++ static_assert for layout. We re-declare the cbuffer binding here.
// ─────────────────────────────────────────────────────────────────────────────────────────────

/// \brief  Per-frame constants updated once per frame by the render thread.
///         Bound at root parameter slot 1 (b1, space0).
///         See ShaderInterop_Frame.hpp for the canonical struct layout.
struct FFrameConstants
{
    Float4x4 viewMatrix;
    Float4x4 projMatrix;
    Float4x4 viewProjMatrix;
    Float4x4 invViewProjMatrix;
    Float4x4 prevViewProjMatrix;
    Float4x4 prevViewMatrix;

    Float4   cameraWorldPos;            //<! .w = unused
    Float4   cameraForward;             //<! .w = unused
    Float4   cameraRight;               //<! .w = unused
    Float4   cameraUp;                  //<! .w = unused
    Float4   frustumPlanes[6];          //<! World-space frustum planes (normal + distance)

    Float2   renderResolution;
    Float2   invRenderResolution;
    Float2   displayResolution;
    Float2   invDisplayResolution;

    Float2   jitterOffset;              //<! TAA jitter: current frame, in [-0.5, 0.5] pixels
    Float2   prevJitterOffset;          //<! TAA jitter: previous frame

    Float32  nearPlane;
    Float32  farPlane;
    Float32  fieldOfView;
    Float32  aspectRatio;

    Float32  time;                      //<! Elapsed time in seconds (wraps at ~100 hours)
    Float32  deltaTime;                 //<! Frame delta in seconds
    UInt32   frameIndex;                //<! Monotonically increasing frame counter
    UInt32   frameFlags;                //<! GP_FRAME_FLAG_TAA_ENABLED, etc.

    Float32  exposureValue;             //<! Camera EV100 (log2 scale)
    Float32  exposureScale;             //<! Linear exposure multiplier (pow2(EV100 - 3))
    Float32  prevExposureScale;         //<! Previous frame (for exposure-compensated reprojection)
    Float32  skyIntensity;              //<! Sky light multiplier

    UInt32   blueNoiseTextureIndex;     //<! Bindless index of the 128x128 blue noise texture
    UInt32   ltcLUT1TextureIndex;       //<! LTC magnitude LUT (area light BRDF)
    UInt32   ltcLUT2TextureIndex;       //<! LTC transform LUT
    UInt32   brdfLUTTextureIndex;       //<! Split-sum BRDF LUT (IBL specular)

    UInt32   irradianceCubemapIndex;    //<! Bindless index of the diffuse irradiance cubemap
    UInt32   radianceCubemapIndex;      //<! Bindless index of the specular radiance cubemap
    UInt32   radianceCubemapMipCount;   //<! Mip count of the radiance cubemap
    UInt32   shadowAtlasTextureIndex;   //<! Bindless index of the shadow map atlas

    UInt32   gpuSceneBufferIndex;       //<! Bindless index of the GPU scene instance buffer
    UInt32   lightListBufferIndex;      //<! Bindless index of the light list buffer
    UInt32   lightGridBufferIndex;      //<! Bindless index of the tiled/clustered light grid
    UInt32   directionalLightCount;     //<! Number of active directional lights this frame

    Float4   directionalLightDir;       //<! Primary directional light direction (world space)
    Float4   directionalLightColor;     //<! Primary directional light color (linear, pre-exposed)
};

ConstantBuffer<FFrameConstants> g_Frame : register(b1, space0);

// ─────────────────────────────────────────────────────────────────────────────────────────────
// CONVENIENCE MACROS
// Short-hand access to frame constants used in nearly every shader.
// Avoids the verbose g_Frame.xxx prefix in shader code.
// ─────────────────────────────────────────────────────────────────────────────────────────────

#define GP_VIEW_MATRIX          g_Frame.viewMatrix
#define GP_PROJ_MATRIX          g_Frame.projMatrix
#define GP_VIEW_PROJ_MATRIX     g_Frame.viewProjMatrix
#define GP_INV_VIEW_PROJ        g_Frame.invViewProjMatrix
#define GP_PREV_VIEW_PROJ       g_Frame.prevViewProjMatrix

#define GP_CAMERA_POS           g_Frame.cameraWorldPos.xyz
#define GP_NEAR                 g_Frame.nearPlane
#define GP_FAR                  g_Frame.farPlane
#define GP_RENDER_RES           g_Frame.renderResolution
#define GP_INV_RENDER_RES       g_Frame.invRenderResolution
#define GP_DISPLAY_RES          g_Frame.displayResolution

#define GP_TIME                 g_Frame.time
#define GP_DELTA_TIME           g_Frame.deltaTime
#define GP_FRAME_INDEX          g_Frame.frameIndex

#define GP_EXPOSURE             g_Frame.exposureScale
#define GP_PREV_EXPOSURE        g_Frame.prevExposureScale

#define GP_OBJECT_INDEX         g_PerDraw.objectIndex
#define GP_MATERIAL_INDEX       g_PerDraw.materialIndex
#define GP_DRAW_FLAGS           g_PerDraw.drawFlags
#define GP_USER_PAYLOAD         g_PerDraw.userPayload

// ─────────────────────────────────────────────────────────────────────────────────────────────
// SCREEN UV HELPERS
// Common pixel-shader and compute-shader helpers for screen-space work.
// ─────────────────────────────────────────────────────────────────────────────────────────────

/// Convert a pixel coordinate (integer, top-left origin) to screen UV [0, 1].
Float2 GP_PixelToUV(UInt2 pixelCoord)
{
    return ((Float2)pixelCoord + 0.5f) * GP_INV_RENDER_RES;
}

/// Convert screen UV to NDC [-1, 1] (DX convention: Y flipped).
Float2 GP_UVToNDC(Float2 uv)
{
    return Float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
}

/// Convert NDC to screen UV.
Float2 GP_NDCToUV(Float2 ndc)
{
    return Float2(ndc.x * 0.5f + 0.5f, 0.5f - ndc.y * 0.5f);
}

/// Reconstruct world-space position from screen UV and depth, using frame constants.
Float3 GP_ScreenUVToWorldPos(Float2 uv, Float32 hwDepth)
{
    return GP_ReconstructWorldPos(uv, hwDepth, GP_INV_VIEW_PROJ);
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// FRAME FLAGS
// ─────────────────────────────────────────────────────────────────────────────────────────────

static const UInt32 GP_FRAME_FLAG_TAA_ENABLED           = (1u << 0);
static const UInt32 GP_FRAME_FLAG_REVERSE_Z             = (1u << 1);
static const UInt32 GP_FRAME_FLAG_ORTHO_CAMERA          = (1u << 2);
static const UInt32 GP_FRAME_FLAG_VOLUMETRIC_ENABLED    = (1u << 3);
static const UInt32 GP_FRAME_FLAG_RAYTRACING_ENABLED    = (1u << 4);
static const UInt32 GP_FRAME_FLAG_DDGI_ENABLED          = (1u << 5);
static const UInt32 GP_FRAME_FLAG_FIRST_FRAME           = (1u << 6);   // No valid history
static const UInt32 GP_FRAME_FLAG_CAMERA_CUT            = (1u << 7);   // TAA must discard history

#endif // GP_COMMON_HLSLI
