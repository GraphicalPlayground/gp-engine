// Copyright (c) - Graphical Playground. All rights reserved.

/// \file    Platform.hlsli
/// \brief   Shader model version guards, compiler abstraction macros, and platform-specific
///          feature detection. Every shader in the engine includes this via Common.hlsli.
///
/// \note    Shader Model capability matrix targeted by this engine:
///          SM6.0 - Wave intrinsics, 64-bit integers
///          SM6.1 - SV_ViewID, barycentric semantics (DXC)
///          SM6.2 - Float16 native, Denorm mode control
///          SM6.5 - Mesh/Amplification shaders, DXR 1.1 inline raytracing, feedback textures
///          SM6.6 - ResourceDescriptorHeap[], SamplerDescriptorHeap[], atomics on typed resources
///          SM6.7 - Advanced raytracing, Quad any/all, helper lane detection
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_PLATFORM_HLSLI
#define GP_PLATFORM_HLSLI

// ─── Shader Model Detection ────────────────────────────────────────────────────────────────────

#if defined(__SHADER_TARGET_MAJOR) && defined(__SHADER_TARGET_MINOR)
    #define GP_SM_MAJOR __SHADER_TARGET_MAJOR
    #define GP_SM_MINOR __SHADER_TARGET_MINOR
#else
    // Fallback — assume SM6.0
    #define GP_SM_MAJOR 6
    #define GP_SM_MINOR 0
#endif

#define GP_SM_VERSION (GP_SM_MAJOR * 10 + GP_SM_MINOR)

#define GP_SM_AT_LEAST(major, minor) (GP_SM_VERSION >= ((major) * 10 + (minor)))

// ─── Feature Capability Flags ─────────────────────────────────────────────────────────────────

/// Wave / subgroup intrinsics (WaveGetLaneIndex, WaveActiveBallot, etc.)
#define GP_HAS_WAVE_INTRINSICS          GP_SM_AT_LEAST(6, 0)

/// Native 16-bit float (float16_t, min16float is a hint, not guaranteed)
#define GP_HAS_FLOAT16                  GP_SM_AT_LEAST(6, 2)

/// Mesh and Amplification shader stages
#define GP_HAS_MESH_SHADERS             GP_SM_AT_LEAST(6, 5)

/// Inline raytracing (RayQuery<> object)
#define GP_HAS_INLINE_RAYTRACING        GP_SM_AT_LEAST(6, 5)

/// ResourceDescriptorHeap[] / SamplerDescriptorHeap[] bindless access
#define GP_HAS_SM66_BINDLESS            GP_SM_AT_LEAST(6, 6)

/// Typed UAV atomic operations (InterlockedAdd on RWTexture2D<float>)
#define GP_HAS_TYPED_UAV_ATOMICS        GP_SM_AT_LEAST(6, 6)

/// Helper lane queries (IsHelperLane())
#define GP_HAS_HELPER_LANE_QUERY        GP_SM_AT_LEAST(6, 7)

/// SV_Barycentrics (per-pixel barycentric coordinates without GS)
#define GP_HAS_BARYCENTRICS             GP_SM_AT_LEAST(6, 1)

// ─── Compiler Detection ───────────────────────────────────────────────────────────────────────

#if defined(__DXC__)
    #define GP_COMPILER_DXC 1
    #define GP_COMPILER_FXC 0
#else
    #define GP_COMPILER_DXC 0
    #define GP_COMPILER_FXC 1
#endif

// ─── Primitive Type Aliases ───────────────────────────────────────────────────────────────────
// Mirror GP:: CPU-side names so ShaderInterop headers compile cleanly on both sides.

typedef float       Float32;
typedef float2      Float2;
typedef float3      Float3;
typedef float4      Float4;
typedef float4x4    Float4x4;
typedef float3x3    Float3x3;
typedef float3x4    Float3x4;
typedef float4x3    Float4x3;

typedef double      Float64;
typedef double2     Double2;
typedef double3     Double3;
typedef double4     Double4;

typedef uint        UInt32;
typedef uint2       UInt2;
typedef uint3       UInt3;
typedef uint4       UInt4;

typedef int         Int32;
typedef int2        Int2;
typedef int3        Int3;
typedef int4        Int4;

typedef bool        Bool;
typedef bool2       Bool2;
typedef bool3       Bool3;
typedef bool4       Bool4;

#if GP_HAS_FLOAT16
typedef float16_t   Float16;
typedef float16_t2  Float16x2;
typedef float16_t4  Float16x4;
#endif

// ─── Annotation Macros ────────────────────────────────────────────────────────────────────────

/// Marks a function as preferring inlining. DXC respects [noinline] and [unroll] attributes;
/// [flatten] and [branch] are hints to the driver's shader compiler.
#define GP_FORCEINLINE          [noinline]  // Use [unroll] / [loop] for loops instead
#define GP_NOINLINE             [noinline]

/// Marks a loop as unrolled. Use for known-small trip counts (e.g., SH bands, PCF taps).
#define GP_UNROLL               [unroll]

/// Marks a branch as preferring wave-coherent predication (driver hint).
#define GP_BRANCH               [branch]
#define GP_FLATTEN              [flatten]

/// Forces loop not to be unrolled (when trip count is large and unrolling would bloat ISA).
#define GP_LOOP                 [loop]

/// Early-out depth test in pixel shaders.
#define GP_EARLY_DEPTH_STENCIL  [earlydepthstencil]

/// Marks output as not needing perspective-correct interpolation.
#define GP_NOINTERP             nointerpolation

/// Centroid sampling for MSAA.
#define GP_CENTROID             centroid

/// Sample-frequency pixel shader invocation for MSAA.
#define GP_SAMPLE_FREQ          sample

// ─── Numeric Constants ────────────────────────────────────────────────────────────────────────

static const Float32 GP_PI          = 3.14159265358979323846f;
static const Float32 GP_TWO_PI      = 6.28318530717958647692f;
static const Float32 GP_HALF_PI     = 1.57079632679489661923f;
static const Float32 GP_INV_PI      = 0.31830988618379067154f;
static const Float32 GP_INV_TWO_PI  = 0.15915494309189533577f;
static const Float32 GP_SQRT2       = 1.41421356237309504880f;
static const Float32 GP_INV_SQRT2   = 0.70710678118654752440f;
static const Float32 GP_SQRT3       = 1.73205080756887729353f;
static const Float32 GP_E           = 2.71828182845904523536f;
static const Float32 GP_LOG2_E      = 1.44269504088896340736f;
static const Float32 GP_LN2         = 0.69314718055994530942f;
static const Float32 GP_EPSILON     = 1.192092896e-07f;         // FLT_EPSILON
static const Float32 GP_LARGE_FLOAT = 3.402823466e+38f;         // FLT_MAX
static const Float32 GP_GOLDEN      = 1.61803398874989484820f;  // Golden ratio

static const UInt32  GP_UINT_MAX    = 0xFFFFFFFFu;
static const Int32   GP_INT_MAX     = 0x7FFFFFFF;
static const Float32 GP_NAN         = (Float32)(0.0f / 0.0f);

// ─── Utility Macros ───────────────────────────────────────────────────────────────────────────

/// Safe divide — returns 0 when denominator is 0.
#define GP_SAFE_DIV(a, b)       ((b) != 0.0f ? (a) / (b) : 0.0f)

/// Compile-time static assert (DXC supports _Static_assert via C-style in HLSL 2021).
#if GP_COMPILER_DXC
    #define GP_STATIC_ASSERT(expr, msg)   static_assert(expr, msg)
#else
    #define GP_STATIC_ASSERT(expr, msg)   // FXC has no static_assert
#endif

/// Marks intentionally unreachable code paths (prevents compiler warnings in switch exhaustion).
#define GP_UNREACHABLE()        { return (Float4)0; }

#endif // GP_PLATFORM_HLSLI
