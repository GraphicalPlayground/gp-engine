// Copyright (c) - Graphical Playground. All rights reserved.

/// \file    BindlessResources.hlsli
/// \brief   Bindless resource access layer. Provides typed accessor functions over
///          ResourceDescriptorHeap[] (SM6.6) with SM6.5-compatible fallback via unbounded
///          descriptor arrays. All engine shaders access textures and buffers through these
///          accessors rather than through explicit register bindings.
///
/// \details Architecture overview:
///          On SM6.6+ hardware, the ResourceDescriptorHeap[] global array is used directly.
///          On SM6.5, the engine binds large unbounded arrays at fixed spaces (space1 onward).
///          The CPU side records a descriptor index (UInt32) per resource handle, stored in
///          material/instance constant buffers. Shaders call e.g. GP_TEXTURE2D(index) to
///          obtain a Texture2D<float4> bound at that index.
///
///          Register space layout (SM6.5 fallback path):
///            space1 : Texture2D<float4>[]        (color textures)
///            space2 : Texture2D<float>[]          (single-channel: depth, AO, shadow)
///            space3 : Texture2D<float2>[]         (two-channel: motion vectors, normals oct)
///            space4 : TextureCube<float4>[]       (cubemaps: IBL, reflection captures)
///            space5 : Texture3D<float4>[]         (volume textures: LUTs, froxels, SDF)
///            space6 : Texture2DArray<float4>[]    (texture arrays: shadow cascades, faces)
///            space7 : ByteAddressBuffer[]         (raw GPU buffers: mesh data, BVH, etc.)
///            space8 : RWTexture2D<float4>[]       (UAV outputs)
///            space9 : RWByteAddressBuffer[]       (UAV buffers)
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_BINDLESS_RESOURCES_HLSLI
#define GP_BINDLESS_RESOURCES_HLSLI

// ─── SM6.6 Direct Heap Access Path ───────────────────────────────────────────────────────────

#if GP_HAS_SM66_BINDLESS

/// Retrieve a Texture2D<float4> from the descriptor heap by index.
/// \param index  Descriptor heap index stored in material/instance data.
#define GP_TEXTURE2D(index)             ResourceDescriptorHeap[NonUniformResourceIndex(index)]

/// Retrieve a Texture2D<float> (single channel) — depth, AO, shadow maps, etc.
#define GP_TEXTURE2D_FLOAT(index)       ResourceDescriptorHeap[NonUniformResourceIndex(index)]

/// Retrieve a Texture2D<float2> — oct-encoded normals, 2-channel motion vectors, etc.
#define GP_TEXTURE2D_FLOAT2(index)      ResourceDescriptorHeap[NonUniformResourceIndex(index)]

/// Retrieve a TextureCube<float4> — IBL environment maps, reflection captures.
#define GP_TEXTURECUBE(index)           ResourceDescriptorHeap[NonUniformResourceIndex(index)]

/// Retrieve a Texture3D<float4> — color LUTs, volume fog, SDF volumes.
#define GP_TEXTURE3D(index)             ResourceDescriptorHeap[NonUniformResourceIndex(index)]

/// Retrieve a Texture2DArray<float4> — shadow cascades, terrain layers, atlas arrays.
#define GP_TEXTURE2D_ARRAY(index)       ResourceDescriptorHeap[NonUniformResourceIndex(index)]

/// Retrieve a ByteAddressBuffer — mesh vertex/index data, BVH nodes, light lists.
#define GP_BUFFER_RAW(index)            ResourceDescriptorHeap[NonUniformResourceIndex(index)]

/// Retrieve a StructuredBuffer<T> — typed structured data.
#define GP_BUFFER_TYPED(T, index)       ResourceDescriptorHeap[NonUniformResourceIndex(index)]

/// Retrieve a RWTexture2D<float4> UAV output.
#define GP_RW_TEXTURE2D(index)          ResourceDescriptorHeap[NonUniformResourceIndex(index)]

/// Retrieve a RWByteAddressBuffer UAV.
#define GP_RW_BUFFER_RAW(index)         ResourceDescriptorHeap[NonUniformResourceIndex(index)]

/// Retrieve a sampler from the sampler heap.
#define GP_SAMPLER(index)               SamplerDescriptorHeap[NonUniformResourceIndex(index)]

// ─── SM6.5 Unbounded Array Fallback Path ──────────────────────────────────────────────────────

#else // !GP_HAS_SM66_BINDLESS

Texture2D<float4>           g_BindlessTexture2D[]       : register(t0, space1);
Texture2D<float>            g_BindlessTexture2DFloat[]  : register(t0, space2);
Texture2D<float2>           g_BindlessTexture2DFloat2[] : register(t0, space3);
TextureCube<float4>         g_BindlessTextureCube[]     : register(t0, space4);
Texture3D<float4>           g_BindlessTexture3D[]       : register(t0, space5);
Texture2DArray<float4>      g_BindlessTexture2DArray[]  : register(t0, space6);
ByteAddressBuffer           g_BindlessBufferRaw[]       : register(t0, space7);
RWTexture2D<float4>         g_BindlessRWTexture2D[]     : register(u0, space8);
RWByteAddressBuffer         g_BindlessRWBufferRaw[]     : register(u0, space9);

#define GP_TEXTURE2D(index)             g_BindlessTexture2D[NonUniformResourceIndex(index)]
#define GP_TEXTURE2D_FLOAT(index)       g_BindlessTexture2DFloat[NonUniformResourceIndex(index)]
#define GP_TEXTURE2D_FLOAT2(index)      g_BindlessTexture2DFloat2[NonUniformResourceIndex(index)]
#define GP_TEXTURECUBE(index)           g_BindlessTextureCube[NonUniformResourceIndex(index)]
#define GP_TEXTURE3D(index)             g_BindlessTexture3D[NonUniformResourceIndex(index)]
#define GP_TEXTURE2D_ARRAY(index)       g_BindlessTexture2DArray[NonUniformResourceIndex(index)]
#define GP_BUFFER_RAW(index)            g_BindlessBufferRaw[NonUniformResourceIndex(index)]
#define GP_BUFFER_TYPED(T, index)       g_BindlessBufferRaw[NonUniformResourceIndex(index)]
#define GP_RW_TEXTURE2D(index)          g_BindlessRWTexture2D[NonUniformResourceIndex(index)]
#define GP_RW_BUFFER_RAW(index)         g_BindlessRWBufferRaw[NonUniformResourceIndex(index)]

#endif // GP_HAS_SM66_BINDLESS

// ─── Typed Buffer Helpers ─────────────────────────────────────────────────────────────────────
// Convenience functions that load typed data from raw ByteAddressBuffer bindless slots.
// GPU mesh data, lights, and instances are stored as tightly-packed structs in raw buffers.

/// Load a float from a raw bindless buffer at a byte offset.
/// \param bufferIndex  Descriptor index of the ByteAddressBuffer.
/// \param byteOffset   Byte offset from the start of the buffer (must be 4-byte aligned).
Float32 GP_LoadFloat(UInt32 bufferIndex, UInt32 byteOffset)
{
    return asfloat(GP_BUFFER_RAW(bufferIndex).Load(byteOffset));
}

/// Load a float2 from a raw bindless buffer at a byte offset.
Float2 GP_LoadFloat2(UInt32 bufferIndex, UInt32 byteOffset)
{
    return asfloat(GP_BUFFER_RAW(bufferIndex).Load2(byteOffset));
}

/// Load a float3 from a raw bindless buffer at a byte offset.
Float3 GP_LoadFloat3(UInt32 bufferIndex, UInt32 byteOffset)
{
    return asfloat(GP_BUFFER_RAW(bufferIndex).Load3(byteOffset));
}

/// Load a float4 from a raw bindless buffer at a byte offset.
Float4 GP_LoadFloat4(UInt32 bufferIndex, UInt32 byteOffset)
{
    return asfloat(GP_BUFFER_RAW(bufferIndex).Load4(byteOffset));
}

/// Load a UInt32 from a raw bindless buffer at a byte offset.
UInt32 GP_LoadUInt(UInt32 bufferIndex, UInt32 byteOffset)
{
    return GP_BUFFER_RAW(bufferIndex).Load(byteOffset);
}

// ─── Sentinel Index ───────────────────────────────────────────────────────────────────────────

/// Descriptor index value meaning "no texture bound". Shaders must check before sampling.
static const UInt32 GP_INVALID_DESCRIPTOR_INDEX = 0xFFFFFFFFu;

/// Returns true if a descriptor index represents a valid binding.
bool GP_IsValidDescriptor(UInt32 index)
{
    return index != GP_INVALID_DESCRIPTOR_INDEX;
}

// ─── Safe Sampling Helpers ────────────────────────────────────────────────────────────────────

/// Sample a bindless Texture2D, returning a fallback value if the index is invalid.
/// \param index     Descriptor index.
/// \param uv        Texture coordinates.
/// \param sampler   Sampler to use (typically GP_SS_ANISO_WRAP or GP_SS_LINEAR_CLAMP).
/// \param fallback  Value returned when index == GP_INVALID_DESCRIPTOR_INDEX.
Float4 GP_SampleTexture2D(UInt32 index, Float2 uv, SamplerState s, Float4 fallback = (Float4)0)
{
    if (!GP_IsValidDescriptor(index))
        return fallback;
    return GP_TEXTURE2D(index).Sample(s, uv);
}

/// Sample a bindless Texture2D at a specific mip level (no derivatives needed — good for CS).
Float4 GP_SampleTexture2DLevel(UInt32 index, Float2 uv, Float32 mip, Float4 fallback = (Float4)0)
{
    if (!GP_IsValidDescriptor(index))
        return fallback;
    return GP_TEXTURE2D(index).SampleLevel(GP_SS_LINEAR_CLAMP, uv, mip);
}

/// Sample a bindless TextureCube, returning a fallback if invalid.
Float4 GP_SampleCubemap(UInt32 index, Float3 dir, Float32 mip = 0.0f, Float4 fallback = (Float4)0)
{
    if (!GP_IsValidDescriptor(index))
        return fallback;
    return GP_TEXTURECUBE(index).SampleLevel(GP_SS_CUBEMAP, dir, mip);
}

#endif // GP_BINDLESS_RESOURCES_HLSLI
