// Copyright (c) - Graphical Playground. All rights reserved.

/// \file    RootSignature.hlsli
/// \brief   Global root signature definition shared by all graphics and compute pipelines.
///          Defined once here and referenced in every shader via [RootSignature(GP_ROOT_SIG)].
///
/// \details Root signature layout:
///
///          Slot  Type               Purpose
///          ────  ─────────────────  ──────────────────────────────────────────────────────
///          0     RootConstants(4)   Per-draw push constants (object index, material index,
///                                   draw call flags, user-defined payload)
///          1     CBV                b0 space0 - Frame constants (time, frame index, etc.)
///          2     CBV                b1 space0 - Camera constants (view, proj, resolution)
///          3     CBV                b2 space0 - Pass-specific constants (varies per pass)
///          4     DescriptorTable    Bindless SRV heap (SM6.5 fallback: unbounded arrays)
///          5     DescriptorTable    Bindless UAV heap (SM6.5 fallback: unbounded RW arrays)
///          6     StaticSamplers     15 static samplers (see Samplers.hlsli)
///
///          The design intentionally keeps the root signature minimal and shared. Pass-specific
///          bindings flow through b2 (the pass constants CBV) and through the bindless heap
///          indices stored within those constants. No per-pass descriptor tables needed.
///
/// \warning The root signature string must exactly match GPRootSignature.cpp on the CPU side.
///          Any mismatch causes silent rendering corruption or a device-lost on D3D12.
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_ROOT_SIGNATURE_HLSLI
#define GP_ROOT_SIGNATURE_HLSLI

// ─── Root Signature String ────────────────────────────────────────────────────────────────────

#define GP_ROOT_SIG \
    "RootFlags(" \
        "ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
        "CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |" \
        "SAMPLER_HEAP_DIRECTLY_INDEXED" \
    ")," \
    \
    /* Slot 0: Per-draw root constants (4x UInt32 = 16 bytes) */ \
    "RootConstants(" \
        "num32BitConstants = 4," \
        "b0," \
        "space0," \
        "visibility = SHADER_VISIBILITY_ALL" \
    ")," \
    \
    /* Slot 1: Frame constants CBV */ \
    "CBV(" \
        "b1," \
        "space0," \
        "visibility = SHADER_VISIBILITY_ALL," \
        "flags = DATA_STATIC_WHILE_SET_AT_EXECUTE" \
    ")," \
    \
    /* Slot 2: Camera constants CBV */ \
    "CBV(" \
        "b2," \
        "space0," \
        "visibility = SHADER_VISIBILITY_ALL," \
        "flags = DATA_STATIC_WHILE_SET_AT_EXECUTE" \
    ")," \
    \
    /* Slot 3: Pass constants CBV */ \
    "CBV(" \
        "b3," \
        "space0," \
        "visibility = SHADER_VISIBILITY_ALL," \
        "flags = DATA_VOLATILE" \
    ")," \
    \
    /* Slot 4: Bindless SRV heap */ \
    "DescriptorTable(" \
        "SRV(t0, space1, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)," \
        "SRV(t0, space2, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)," \
        "SRV(t0, space3, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)," \
        "SRV(t0, space4, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)," \
        "SRV(t0, space5, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)," \
        "SRV(t0, space6, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)," \
        "SRV(t0, space7, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)," \
        "visibility = SHADER_VISIBILITY_ALL" \
    ")," \
    \
    /* Slot 5: Bindless UAV heap */ \
    "DescriptorTable(" \
        "UAV(u0, space8, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)," \
        "UAV(u0, space9, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)," \
        "visibility = SHADER_VISIBILITY_ALL" \
    ")," \
    \
    /* Slot 6: Static samplers (register s0–s14) — see Samplers.hlsli for full table */ \
    "StaticSampler(s0,  filter = FILTER_MIN_MAG_MIP_POINT,       addressU = CLAMP,  addressV = CLAMP,  addressW = CLAMP),"  \
    "StaticSampler(s1,  filter = FILTER_MIN_MAG_MIP_POINT,       addressU = WRAP,   addressV = WRAP,   addressW = WRAP),"   \
    "StaticSampler(s2,  filter = FILTER_MIN_MAG_MIP_POINT,       addressU = MIRROR, addressV = MIRROR, addressW = MIRROR)," \
    "StaticSampler(s3,  filter = FILTER_MIN_MAG_MIP_LINEAR,      addressU = CLAMP,  addressV = CLAMP,  addressW = CLAMP),"  \
    "StaticSampler(s4,  filter = FILTER_MIN_MAG_MIP_LINEAR,      addressU = WRAP,   addressV = WRAP,   addressW = WRAP),"   \
    "StaticSampler(s5,  filter = FILTER_MIN_MAG_MIP_LINEAR,      addressU = MIRROR, addressV = MIRROR, addressW = MIRROR)," \
    "StaticSampler(s6,  filter = FILTER_ANISOTROPIC, maxAnisotropy = 16, addressU = CLAMP,  addressV = CLAMP,  addressW = CLAMP),"  \
    "StaticSampler(s7,  filter = FILTER_ANISOTROPIC, maxAnisotropy = 16, addressU = WRAP,   addressV = WRAP,   addressW = WRAP),"   \
    "StaticSampler(s8,  filter = FILTER_ANISOTROPIC, maxAnisotropy = 16, addressU = MIRROR, addressV = MIRROR, addressW = MIRROR)," \
    "StaticSampler(s9,  filter = FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, comparisonFunc = LESS,          addressU = CLAMP, addressV = CLAMP,  borderColor = OPAQUE_WHITE)," \
    "StaticSampler(s10, filter = FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, comparisonFunc = LESS_EQUAL,    addressU = CLAMP, addressV = CLAMP,  borderColor = OPAQUE_WHITE)," \
    "StaticSampler(s11, filter = FILTER_MINIMUM_MIN_MAG_MIP_LINEAR,          addressU = CLAMP, addressV = CLAMP, addressW = CLAMP)," \
    "StaticSampler(s12, filter = FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR,          addressU = CLAMP, addressV = CLAMP, addressW = CLAMP)," \
    "StaticSampler(s13, filter = FILTER_MIN_MAG_LINEAR_MIP_POINT,            addressU = CLAMP, addressV = CLAMP, addressW = CLAMP, mipLODClamp = 0)," \
    "StaticSampler(s14, filter = FILTER_MIN_MAG_MIP_LINEAR,                  addressU = CLAMP, addressV = CLAMP, addressW = CLAMP)"

// ─── Per-Draw Root Constants ──────────────────────────────────────────────────────────────────
// Bound at root parameter slot 0. Accessed in every shader stage.

/// \brief  Per-draw constants pushed via SetGraphicsRoot32BitConstants / SetComputeRoot32BitConstants.
///         The CPU side fills FPerDrawConstants (from ShaderInterop.hpp) and uploads 4 UInt32s.
struct FPerDrawConstants
{
    UInt32 objectIndex;     //<! Index into the GPUScene object buffer (instance transform, etc.)
    UInt32 materialIndex;   //<! Index into the material data buffer (FMaterialData)
    UInt32 drawFlags;       //<! Bitmask: GP_DRAW_FLAG_DOUBLE_SIDED, GP_DRAW_FLAG_VELOCITY, etc.
    UInt32 userPayload;     //<! Pass-defined payload (cascade index, probe index, LOD level, etc.)
};

ConstantBuffer<FPerDrawConstants> g_PerDraw : register(b0, space0);

// ─── Draw Flag Bit Definitions ────────────────────────────────────────────────────────────────

static const UInt32 GP_DRAW_FLAG_DOUBLE_SIDED       = (1u << 0);  //<! Disable backface culling
static const UInt32 GP_DRAW_FLAG_ALPHA_TEST         = (1u << 1);  //<! Perform alpha discard
static const UInt32 GP_DRAW_FLAG_VELOCITY           = (1u << 2);  //<! Write velocity to MV buffer
static const UInt32 GP_DRAW_FLAG_SKINNED            = (1u << 3);  //<! Skinned mesh (use bone matrices)
static const UInt32 GP_DRAW_FLAG_CAST_SHADOW        = (1u << 4);  //<! Shadow caster pass
static const UInt32 GP_DRAW_FLAG_SELECTED           = (1u << 5);  //<! Editor selection highlight
static const UInt32 GP_DRAW_FLAG_REVERSE_Z          = (1u << 6);  //<! Depth buffer uses reversed-Z
static const UInt32 GP_DRAW_FLAG_ORTHOGRAPHIC       = (1u << 7);  //<! Orthographic projection

#endif // GP_ROOT_SIGNATURE_HLSLI
