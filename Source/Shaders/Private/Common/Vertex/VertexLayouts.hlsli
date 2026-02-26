/// \file    VertexLayouts.hlsli
/// \brief   Standard vertex input assembly structs and VS output structs.
///          These mirror the CPU-side vertex buffer layouts declared in GPMesh.hpp.
///
/// \details Vertex layout philosophy in this engine:
///          - Static mesh data is stored in tight, interleaved per-stream buffers.
///          - Skinned meshes have a separate stream for bone weights/indices.
///          - All attributes beyond position are fetched manually from bindless buffers
///            in the vertex shader (VertexFetch.hlsli) for bindless/mesh shader pipelines.
///          - These structs are used for the traditional IA (Input Assembler) path only.
///
///          Stream binding convention (matches GPInputLayoutManager.cpp):
///            Stream 0 : Position (POSITION, R32G32B32_FLOAT)
///            Stream 1 : Normal + Tangent (R10G10B10A2_SNORM packed)
///            Stream 2 : TexCoord0 + TexCoord1 (R16G16_FLOAT x2)
///            Stream 3 : Color (R8G8B8A8_UNORM)
///            Stream 4 : BlendWeights + BlendIndices (skinned meshes only)
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_VERTEX_LAYOUTS_HLSLI
#define GP_VERTEX_LAYOUTS_HLSLI

// ─────────────────────────────────────────────────────────────────────────────────────────────
// INPUT ASSEMBLY STRUCTS (IA → VS)
// ─────────────────────────────────────────────────────────────────────────────────────────────

/// Minimal position-only vertex. Used in shadow depth passes and depth prepass
/// where no material attributes are needed.
struct FVertexPos
{
    Float3 position     : POSITION;
};

/// Standard static mesh vertex: position, normal, tangent, UV0.
/// Matches the binary layout of the FStaticMeshVertex struct in GPMesh.hpp.
struct FVertexStandard
{
    Float3 position     : POSITION;
    UInt32 normalPacked : NORMAL;       ///< Oct-encoded normal, packed R10G10B10A2
    UInt32 tangentPacked: TANGENT;      ///< Oct-encoded tangent XY + bitangent sign in .w
    Float2 uv0          : TEXCOORD0;
    Float2 uv1          : TEXCOORD1;    ///< Lightmap UV
};

/// Full vertex with vertex color (foliage, particles, spline meshes).
struct FVertexColored
{
    Float3 position     : POSITION;
    UInt32 normalPacked : NORMAL;
    UInt32 tangentPacked: TANGENT;
    Float2 uv0          : TEXCOORD0;
    Float2 uv1          : TEXCOORD1;
    Float4 color        : COLOR0;       ///< Vertex color (sRGB, hardware gamma-decoded on sample)
};

/// Skinned mesh vertex (stream 0-3 + stream 4 for skin data).
struct FVertexSkinned
{
    Float3 position     : POSITION;
    UInt32 normalPacked : NORMAL;
    UInt32 tangentPacked: TANGENT;
    Float2 uv0          : TEXCOORD0;
    Float2 uv1          : TEXCOORD1;
    Float4 blendWeights : BLENDWEIGHT;  ///< 4 bone weights, must sum to 1.0
    UInt4  blendIndices : BLENDINDICES; ///< 4 bone indices into the bone matrix palette
};

/// UI vertex: 2D position + UV + tinted color.
struct FVertexUI
{
    Float2 position     : POSITION;     ///< Screen-space pixel position (not NDC)
    Float2 uv           : TEXCOORD0;
    Float4 color        : COLOR0;       ///< Premultiplied RGBA tint
};

/// Terrain vertex: packed height + splat weights.
struct FVertexTerrain
{
    Float2 position2D   : POSITION;     ///< XZ position; Y is read from heightmap in VS
    Float2 uv           : TEXCOORD0;    ///< [0,1] UV across the terrain patch
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// VS → PS INTERPOLANT STRUCTS
// ─────────────────────────────────────────────────────────────────────────────────────────────

/// Standard VS-to-PS interpolant. Used by GBufferWrite, deferred geometry passes.
struct FInterpolantStandard
{
    Float4 svPosition       : SV_Position;
    Float3 worldPos         : TEXCOORD0;    ///< World-space surface position
    Float2 uv0              : TEXCOORD1;    ///< Primary UV
    Float2 uv1              : TEXCOORD2;    ///< Lightmap UV
    Float3 worldNormal      : TEXCOORD3;    ///< World-space geometric normal (interpolated)
    Float4 worldTangent     : TEXCOORD4;    ///< World-space tangent (.xyz) + bitangent sign (.w)
    Float4 currClipPos      : TEXCOORD5;    ///< Current clip-space position (for velocity)
    Float4 prevClipPos      : TEXCOORD6;    ///< Previous clip-space position (for velocity)
};

/// Interpolant with vertex color (foliage, particles, splines).
struct FInterpolantColored
{
    Float4 svPosition       : SV_Position;
    Float3 worldPos         : TEXCOORD0;
    Float2 uv0              : TEXCOORD1;
    Float2 uv1              : TEXCOORD2;
    Float3 worldNormal      : TEXCOORD3;
    Float4 worldTangent     : TEXCOORD4;
    Float4 color            : TEXCOORD5;    ///< Vertex color (linear)
    Float4 currClipPos      : TEXCOORD6;
    Float4 prevClipPos      : TEXCOORD7;
};

/// Minimal interpolant for shadow depth passes (position + UV for alpha test).
struct FInterpolantShadow
{
    Float4 svPosition       : SV_Position;
    Float2 uv0              : TEXCOORD0;    ///< Only needed for alpha-tested shadows
};

/// Interpolant for the fullscreen triangle (post-process passes).
struct FInterpolantFullscreen
{
    Float4 svPosition       : SV_Position;
    Float2 uv               : TEXCOORD0;    ///< [0,1] screen UV
    Float2 ndcXY            : TEXCOORD1;    ///< [-1,1] NDC XY (avoids recomputing in PS)
};

/// UI/sprite interpolant.
struct FInterpolantUI
{
    Float4 svPosition       : SV_Position;
    Float2 uv               : TEXCOORD0;
    Float4 color            : TEXCOORD1;    ///< Premultiplied RGBA tint
};

/// Terrain interpolant with world-space position for multi-layer blending.
struct FInterpolantTerrain
{
    Float4 svPosition       : SV_Position;
    Float3 worldPos         : TEXCOORD0;
    Float2 uv0              : TEXCOORD1;    ///< Detail UV (scaled by layer tiling)
    Float2 uvLightmap       : TEXCOORD2;    ///< Lightmap UV [0,1] across full terrain
    Float3 worldNormal      : TEXCOORD3;
    Float4 worldTangent     : TEXCOORD4;
};

/// Sky / cubemap interpolant. Just needs world-space direction from the camera.
struct FInterpolantSky
{
    Float4 svPosition       : SV_Position;
    Float3 worldDir         : TEXCOORD0;    ///< Direction from camera center to sky pixel
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// GBuffer OUTPUT STRUCT (PS → RenderTargets)
// ─────────────────────────────────────────────────────────────────────────────────────────────
// The GBuffer layout is fixed. Changing it requires updating GBufferDecode.hlsli,
// ShaderInterop_Material.hpp, and the C++ RHI code that creates the render pass.
//
// GBuffer Layout:
//   RT0: RGBA8_UNORM    — Albedo (RGB) + Subsurface mask (A)
//   RT1: R16G16_SNORM   — Packed normal (octahedral)
//   RT2: RGBA8_UNORM    — Metalness (R), Roughness (G), Ambient Occlusion (B), Emissive mask (A)
//   RT3: R16G16_FLOAT   — Motion vector (screen-space, NDC delta XY)
//   Depth: D32_FLOAT    — Reversed-Z depth

struct FGBufferOutput
{
    Float4 rt0  : SV_Target0;   ///< Albedo.RGB + Subsurface.A
    Float4 rt1  : SV_Target1;   ///< Normal.XY (oct-encoded snorm) + Shading model ID (Z) + Flags (W)
    Float4 rt2  : SV_Target2;   ///< Metalness.R + Roughness.G + AO.B + EmissiveMask.A
    Float2 rt3  : SV_Target3;   ///< Motion vector NDC delta (XY = R16G16_FLOAT)
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// MESHLET PAYLOAD (Amplification → Mesh Shader)
// ─────────────────────────────────────────────────────────────────────────────────────────────
// SM6.5 mesh shader pipeline. The amplification shader dispatches mesh shader threadgroups
// and passes per-meshlet data through the payload.

#if GP_HAS_MESH_SHADERS

struct FMeshletPayload
{
    UInt32 meshletIndices[64]; ///< Indices of surviving meshlets after amplification culling.
                               ///< Max 64 per amplification shader threadgroup (AS group limit).
};

/// Mesh shader vertex output — same attributes as FInterpolantStandard but re-exported
/// via the mesh shader's SetMeshOutputCounts / output vertex array.
struct FMeshVertex
{
    Float4 svPosition       : SV_Position;
    Float3 worldPos         : TEXCOORD0;
    Float2 uv0              : TEXCOORD1;
    Float2 uv1              : TEXCOORD2;
    Float3 worldNormal      : TEXCOORD3;
    Float4 worldTangent     : TEXCOORD4;
    Float4 currClipPos      : TEXCOORD5;
    Float4 prevClipPos      : TEXCOORD6;
};

#endif // GP_HAS_MESH_SHADERS

// ─────────────────────────────────────────────────────────────────────────────────────────────
// SHADING MODEL IDs
// Packed into GBuffer RT1.Z to select the lighting branch in the deferred pass.
// ─────────────────────────────────────────────────────────────────────────────────────────────

static const UInt32 GP_SHADING_MODEL_DEFAULT        = 0u; ///< Standard PBR (metallic-roughness)
static const UInt32 GP_SHADING_MODEL_SUBSURFACE      = 1u; ///< Subsurface scattering (skin)
static const UInt32 GP_SHADING_MODEL_CLOTH           = 2u; ///< Cloth (Charlie sheen BRDF)
static const UInt32 GP_SHADING_MODEL_CLEAR_COAT      = 3u; ///< Clear coat (car paint, lacquered wood)
static const UInt32 GP_SHADING_MODEL_HAIR            = 4u; ///< Hair (Marschner lobes)
static const UInt32 GP_SHADING_MODEL_EYE             = 5u; ///< Eye (refraction + iris)
static const UInt32 GP_SHADING_MODEL_UNLIT           = 6u; ///< No lighting (emissive only)
static const UInt32 GP_SHADING_MODEL_IRIDESCENT      = 7u; ///< Thin-film iridescence
static const UInt32 GP_SHADING_MODEL_COUNT           = 8u;

#endif // GP_VERTEX_LAYOUTS_HLSLI
