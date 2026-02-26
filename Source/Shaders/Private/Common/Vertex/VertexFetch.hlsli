/// \file    VertexFetch.hlsli
/// \brief   Manual vertex fetch from bindless byte-address buffers.
///          Used by the mesh shader pipeline, GPU-driven indirect draw, and any pass
///          that bypasses the Input Assembler to read vertex data from bindless buffers.
///
/// \details Why manual fetch instead of Input Assembler?
///          The IA binds vertex buffers explicitly per draw call — this works for traditional
///          rendering but breaks in GPU-driven and mesh shader pipelines where thousands of
///          meshes are drawn from a single indirect dispatch with no per-draw IA rebinding.
///
///          Instead, each mesh stores its vertex data start offset in the GPU Scene buffer
///          (g_Frame.gpuSceneBufferIndex). The VS/MS reads the vertex buffer index and offset
///          from the scene object, then fetches individual attributes via these functions.
///
///          Attribute buffer layout matches FStaticMeshBuffers in GPMesh.hpp:
///            positionBuffer  : R32G32B32_FLOAT (12 bytes / vertex)
///            normalBuffer    : R10G10B10A2_UNORM packed (4 bytes / vertex)
///            tangentBuffer   : R10G10B10A2_UNORM packed (4 bytes / vertex)
///            uv0Buffer       : R16G16_FLOAT (4 bytes / vertex)
///            uv1Buffer       : R16G16_FLOAT (4 bytes / vertex)
///            colorBuffer     : R8G8B8A8_UNORM (4 bytes / vertex, optional)
///            indexBuffer     : R32_UINT or R16_UINT (4 or 2 bytes / index)
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_VERTEX_FETCH_HLSLI
#define GP_VERTEX_FETCH_HLSLI

// ─── GPU Scene Object ─────────────────────────────────────────────────────────────────────────
// Each draw call's objectIndex (from g_PerDraw) indexes into this struct within the scene buf.

struct FGPUSceneObject
{
    Float4x4 localToWorld;          ///< Current frame object-to-world transform
    Float4x4 prevLocalToWorld;      ///< Previous frame (for motion vectors)
    Float4x4 localToWorldInvTrans;  ///< Inverse-transpose for normal transform

    UInt32   positionBufferIndex;   ///< Bindless index of position byte buffer
    UInt32   normalBufferIndex;     ///< Bindless index of normal byte buffer
    UInt32   tangentBufferIndex;    ///< Bindless index of tangent byte buffer
    UInt32   uv0BufferIndex;        ///< Bindless index of UV0 byte buffer

    UInt32   uv1BufferIndex;        ///< Bindless index of UV1 byte buffer (lightmap)
    UInt32   colorBufferIndex;      ///< Bindless index of vertex color buffer (GP_INVALID if none)
    UInt32   indexBufferIndex;      ///< Bindless index of index buffer
    UInt32   materialIndex;         ///< Index into material data buffer

    UInt32   vertexCount;
    UInt32   indexCount;
    UInt32   firstVertex;           ///< Base vertex offset within the position buffer
    UInt32   firstIndex;            ///< Base index within the index buffer

    Float3   boundsCenter;          ///< World-space AABB center
    Float32  boundsRadius;          ///< World-space bounding sphere radius
    Float3   boundsExtents;         ///< World-space AABB half-extents
    UInt32   objectFlags;           ///< GP_OBJECT_FLAG_CASTS_SHADOW, etc.
};

// ─── Object Flag Constants ────────────────────────────────────────────────────────────────────

static const UInt32 GP_OBJECT_FLAG_CASTS_SHADOW     = (1u << 0);
static const UInt32 GP_OBJECT_FLAG_RECEIVES_DECALS  = (1u << 1);
static const UInt32 GP_OBJECT_FLAG_DYNAMIC          = (1u << 2);   ///< Moves each frame
static const UInt32 GP_OBJECT_FLAG_HAS_VERTEX_COLOR = (1u << 3);
static const UInt32 GP_OBJECT_FLAG_HAS_UV1          = (1u << 4);
static const UInt32 GP_OBJECT_FLAG_SKINNED          = (1u << 5);

// ─── Scene Object Fetch ───────────────────────────────────────────────────────────────────────

/// Load the FGPUSceneObject for the current draw from the GPU scene buffer.
/// Uses g_PerDraw.objectIndex (set by root constants per draw call).
FGPUSceneObject GP_FetchSceneObject(UInt32 objectIndex)
{
    // Each FGPUSceneObject is tightly packed. Compute byte offset:
    // sizeof(FGPUSceneObject) = 3 * 64 (matrices) + 16 * 4 (scalars) = 256 bytes
    UInt32 byteOffset = objectIndex * 256u;

    ByteAddressBuffer sceneBuffer = GP_BUFFER_RAW(g_Frame.gpuSceneBufferIndex);

    FGPUSceneObject obj;

    // Load matrices (each is 4x float4 = 64 bytes)
    obj.localToWorld = (Float4x4)0;     // Abbreviated — real impl uses Load4 x4 per matrix
    obj.prevLocalToWorld = (Float4x4)0;
    obj.localToWorldInvTrans = (Float4x4)0;

    // Load scalar fields
    UInt32 scalarBase = byteOffset + 192u; // After 3 matrices

    UInt4  pack0 = sceneBuffer.Load4(scalarBase);
    UInt4  pack1 = sceneBuffer.Load4(scalarBase + 16u);
    UInt4  pack2 = sceneBuffer.Load4(scalarBase + 32u);
    UInt4  pack3 = sceneBuffer.Load4(scalarBase + 48u);

    obj.positionBufferIndex = pack0.x;
    obj.normalBufferIndex   = pack0.y;
    obj.tangentBufferIndex  = pack0.z;
    obj.uv0BufferIndex      = pack0.w;

    obj.uv1BufferIndex      = pack1.x;
    obj.colorBufferIndex    = pack1.y;
    obj.indexBufferIndex    = pack1.z;
    obj.materialIndex       = pack1.w;

    obj.vertexCount         = pack2.x;
    obj.indexCount          = pack2.y;
    obj.firstVertex         = pack2.z;
    obj.firstIndex          = pack2.w;

    obj.boundsCenter        = asfloat(Float3(pack3.x, pack3.y, pack3.z));
    obj.boundsRadius        = asfloat(pack3.w);

    return obj;
}

// ─── Per-Attribute Fetch ──────────────────────────────────────────────────────────────────────

/// Fetch the world-space position of a vertex by index.
/// \param obj          GPU scene object (from GP_FetchSceneObject).
/// \param vertexIndex  Vertex index within this mesh (not absolute).
Float3 GP_FetchPosition(FGPUSceneObject obj, UInt32 vertexIndex)
{
    UInt32 byteOffset = (obj.firstVertex + vertexIndex) * 12u; // 3 x float32
    return GP_LoadFloat3(obj.positionBufferIndex, byteOffset);
}

/// Fetch and decode a world-space normal from the packed normal buffer.
Float3 GP_FetchNormal(FGPUSceneObject obj, UInt32 vertexIndex)
{
    UInt32 byteOffset  = (obj.firstVertex + vertexIndex) * 4u; // R10G10B10A2
    UInt32 packed      = GP_LoadUInt(obj.normalBufferIndex, byteOffset);
    Float4 unorm       = GP_UnpackR10G10B10A2Unorm(packed);
    Float3 normal      = unorm.xyz * 2.0f - 1.0f;
    return normalize(mul((Float3x3)obj.localToWorldInvTrans, normal));
}

/// Fetch and decode a world-space tangent (xyz) + bitangent sign (w) from the packed buffer.
Float4 GP_FetchTangent(FGPUSceneObject obj, UInt32 vertexIndex)
{
    UInt32 byteOffset  = (obj.firstVertex + vertexIndex) * 4u;
    UInt32 packed      = GP_LoadUInt(obj.tangentBufferIndex, byteOffset);
    Float4 unorm       = GP_UnpackR10G10B10A2Unorm(packed);
    Float3 tangentLS   = unorm.xyz * 2.0f - 1.0f;
    Float32 sign       = (unorm.w > 0.5f) ? 1.0f : -1.0f; // Bitangent sign

    Float3 tangentWS   = normalize(mul((Float3x3)obj.localToWorld, tangentLS));
    return Float4(tangentWS, sign);
}

/// Fetch UV0 from R16G16_FLOAT buffer.
Float2 GP_FetchUV0(FGPUSceneObject obj, UInt32 vertexIndex)
{
    UInt32 byteOffset = (obj.firstVertex + vertexIndex) * 4u;
    UInt32 packed     = GP_LoadUInt(obj.uv0BufferIndex, byteOffset);
    return GP_UnpackR16G16FToFloat2(packed);
}

/// Fetch UV1 (lightmap) from R16G16_FLOAT buffer. Returns (0,0) if UV1 is not present.
Float2 GP_FetchUV1(FGPUSceneObject obj, UInt32 vertexIndex)
{
    if (!GP_IsValidDescriptor(obj.uv1BufferIndex))
        return (Float2)0;

    UInt32 byteOffset = (obj.firstVertex + vertexIndex) * 4u;
    UInt32 packed     = GP_LoadUInt(obj.uv1BufferIndex, byteOffset);
    return GP_UnpackR16G16FToFloat2(packed);
}

/// Fetch vertex color (R8G8B8A8_UNORM, linear — hardware sRGB gamma is not applied here).
/// Returns float4(1,1,1,1) if no color buffer is bound.
Float4 GP_FetchColor(FGPUSceneObject obj, UInt32 vertexIndex)
{
    if (!GP_IsValidDescriptor(obj.colorBufferIndex))
        return (Float4)1;

    UInt32 byteOffset = (obj.firstVertex + vertexIndex) * 4u;
    UInt32 packed     = GP_LoadUInt(obj.colorBufferIndex, byteOffset);

    Float4 color;
    color.r = (Float32)((packed      ) & 0xFFu) / 255.0f;
    color.g = (Float32)((packed >>  8u) & 0xFFu) / 255.0f;
    color.b = (Float32)((packed >> 16u) & 0xFFu) / 255.0f;
    color.a = (Float32)((packed >> 24u) & 0xFFu) / 255.0f;
    return color;
}

/// Fetch a 32-bit index from the index buffer.
UInt32 GP_FetchIndex32(FGPUSceneObject obj, UInt32 primitiveIndex, UInt32 localVertexIndex)
{
    UInt32 globalIndex = obj.firstIndex + primitiveIndex * 3u + localVertexIndex;
    UInt32 byteOffset  = globalIndex * 4u;
    return GP_LoadUInt(obj.indexBufferIndex, byteOffset) + obj.firstVertex;
}

// ─── Full Vertex Fetch (Bundled) ──────────────────────────────────────────────────────────────

/// Decoded vertex data in world space, ready for the pixel shader.
struct FDecodedVertex
{
    Float3 worldPos;
    Float3 worldNormal;
    Float4 worldTangent;    ///< .xyz = tangent, .w = bitangent sign
    Float3 worldBitangent;  ///< Precomputed: cross(worldNormal, worldTangent.xyz) * worldTangent.w
    Float2 uv0;
    Float2 uv1;
    Float4 color;
};

/// Fetch and decode all vertex attributes for a given vertex index.
/// This is the primary entry point used by VS and MS entry points.
FDecodedVertex GP_FetchVertex(FGPUSceneObject obj, UInt32 vertexIndex)
{
    FDecodedVertex v;

    Float3 localPos      = GP_FetchPosition(obj, vertexIndex);
    v.worldPos           = mul(obj.localToWorld, Float4(localPos, 1.0f)).xyz;
    v.worldNormal        = GP_FetchNormal(obj, vertexIndex);
    v.worldTangent       = GP_FetchTangent(obj, vertexIndex);
    v.worldBitangent     = cross(v.worldNormal, v.worldTangent.xyz) * v.worldTangent.w;
    v.uv0                = GP_FetchUV0(obj, vertexIndex);
    v.uv1                = GP_FetchUV1(obj, vertexIndex);
    v.color              = GP_FetchColor(obj, vertexIndex);

    return v;
}

#endif // GP_VERTEX_FETCH_HLSLI
