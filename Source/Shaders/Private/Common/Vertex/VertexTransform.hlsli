/// \file    VertexTransform.hlsli
/// \brief   Vertex shader clip-space transform helpers: world→clip, normal transform,
///          velocity computation, and common VS boilerplate reduction.
///
/// \details These functions centralize the repetitive matrix multiply chain
///          that every vertex shader performs. Rather than duplicating:
///             float4 clip = mul(g_Frame.viewProjMatrix, float4(worldPos, 1));
///          across 40 vertex shaders and risking one using the wrong matrix, all VS
///          entry points call GP_TransformToClip() and let this file be the single
///          source of truth for how the transform chain works.
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_VERTEX_TRANSFORM_HLSLI
#define GP_VERTEX_TRANSFORM_HLSLI

// ─── Core Transform Chain ─────────────────────────────────────────────────────────────────────

/// Transform a local-space position to world space.
Float3 GP_LocalToWorld(Float3 localPos, Float4x4 localToWorld)
{
    return mul(localToWorld, Float4(localPos, 1.0f)).xyz;
}

/// Transform a world-space position to clip space using the frame view-projection matrix.
Float4 GP_WorldToClipPos(Float3 worldPos)
{
    return mul(GP_VIEW_PROJ_MATRIX, Float4(worldPos, 1.0f));
}

/// Full local → clip transform in one call.
Float4 GP_LocalToClipPos(Float3 localPos, Float4x4 localToWorld)
{
    Float3 worldPos = GP_LocalToWorld(localPos, localToWorld);
    return GP_WorldToClipPos(worldPos);
}

/// Transform for shadow depth pass: use the pass-specific light VP from the pass cbuffer.
/// The pass constants CBV must contain a Float4x4 at offset 0 (lightViewProj).
Float4 GP_LocalToShadowClip(Float3 localPos, Float4x4 localToWorld, Float4x4 lightViewProj)
{
    Float3 worldPos = GP_LocalToWorld(localPos, localToWorld);
    return mul(lightViewProj, Float4(worldPos, 1.0f));
}

// ─── Normal Transform Chain ───────────────────────────────────────────────────────────────────

/// Transform a local-space normal to world space using the inverse-transpose matrix.
/// Call only when non-uniform scale is possible. For uniform scale, use GP_TransformNormalFast.
Float3 GP_LocalNormalToWorld(Float3 localNormal, Float4x4 localToWorldInvTrans)
{
    return normalize(mul((Float3x3)localToWorldInvTrans, localNormal));
}

/// Transform a local-space tangent to world space (tangents transform by M, not M^{-T}).
Float3 GP_LocalTangentToWorld(Float3 localTangent, Float4x4 localToWorld)
{
    return normalize(mul((Float3x3)localToWorld, localTangent));
}

/// Build the full TBN frame in world space from local-space normal and tangent.
/// \param localNormal     Local-space geometric normal (unit length).
/// \param localTangent    Local-space tangent (unit length).
/// \param bitangentSign   +1 or -1 (from vertex data .w component of packed tangent).
/// \param localToWorld    Object-to-world matrix.
/// \param invTransM       Inverse-transpose upper 3x3 (for correct normal transform).
/// \param worldN          Output: world-space normal.
/// \param worldT          Output: world-space tangent.
/// \param worldB          Output: world-space bitangent.
void GP_BuildWorldTBN(Float3 localNormal, Float3 localTangent, Float32 bitangentSign,
                      Float4x4 localToWorld, Float4x4 invTransM,
                      out Float3 worldN, out Float3 worldT, out Float3 worldB)
{
    worldN = normalize(mul((Float3x3)invTransM, localNormal));
    worldT = normalize(mul((Float3x3)localToWorld, localTangent));
    worldB = cross(worldN, worldT) * bitangentSign;
}

// ─── Velocity / Motion Vector ─────────────────────────────────────────────────────────────────

/// Compute the clip-space positions for the current and previous frame.
/// These are written to interpolants and used in the PS to compute the motion vector.
/// \param localPos      Current frame local-space position (post-skinning if applicable).
/// \param prevLocalPos  Previous frame local-space position (post-skinning, prev bones).
/// \param localToWorld     Current frame object-to-world.
/// \param prevLocalToWorld Previous frame object-to-world.
/// \param currClip      Output: current clip-space position.
/// \param prevClip      Output: previous clip-space position.
void GP_ComputeVelocityClipPositions(Float3 localPos, Float3 prevLocalPos,
                                     Float4x4 localToWorld, Float4x4 prevLocalToWorld,
                                     out Float4 currClip, out Float4 prevClip)
{
    Float3 worldPos     = GP_LocalToWorld(localPos, localToWorld);
    Float3 prevWorldPos = GP_LocalToWorld(prevLocalPos, prevLocalToWorld);

    currClip = mul(GP_VIEW_PROJ_MATRIX,  Float4(worldPos,     1.0f));
    prevClip = mul(GP_PREV_VIEW_PROJ,    Float4(prevWorldPos, 1.0f));
}

// ─── Standard VS Output Fill ──────────────────────────────────────────────────────────────────
// Convenience function that fills a FInterpolantStandard from a decoded vertex.
// Used to avoid repeating this boilerplate in every geometry-pass vertex shader.

/// Fill a FInterpolantStandard from a locally-fetched decoded vertex.
/// \param vert        Decoded vertex in world space (from GP_FetchVertex).
/// \param obj         GPU scene object (contains per-object matrices).
/// \return            Filled interpolant ready for rasterization.
FInterpolantStandard GP_MakeStandardInterpolant(FDecodedVertex vert, FGPUSceneObject obj)
{
    FInterpolantStandard o;

    o.svPosition  = GP_WorldToClipPos(vert.worldPos);
    o.worldPos    = vert.worldPos;
    o.uv0         = vert.uv0;
    o.uv1         = vert.uv1;
    o.worldNormal = vert.worldNormal;
    o.worldTangent= vert.worldTangent;

    // Velocity: for static objects, prevLocalPos == localPos, prev matrix handles the motion
    Float3 localPos  = mul(obj.localToWorld,     Float4(vert.worldPos, 0.0f)).xyz;
    GP_ComputeVelocityClipPositions(
        localPos, localPos,
        obj.localToWorld, obj.prevLocalToWorld,
        o.currClipPos, o.prevClipPos
    );

    return o;
}

// ─── Fullscreen Triangle ──────────────────────────────────────────────────────────────────────
// The engine renders all fullscreen/post-process passes with a single oversized triangle,
// not a screen-filling quad. This avoids the diagonal overdraw line visible in quads on
// some hardware, and saves one vertex worth of work.
//
// Vertex positions (3 verts, no VB needed — computed from SV_VertexID):
//   ID 0: (-1, -1)  — bottom-left (below screen)
//   ID 1: (-1,  3)  — top-left (above screen by 2x)
//   ID 2: ( 3, -1)  — bottom-right (right of screen by 2x)
//
// UV coordinates:
//   ID 0: (0, 1)
//   ID 1: (0, -1)
//   ID 2: (2, 1)
//
// The triangle covers the NDC unit square exactly. The GPU clips the excess geometry.

/// Fill a FInterpolantFullscreen for the post-process fullscreen triangle.
/// Call with SV_VertexID as the parameter. No vertex buffer needed.
FInterpolantFullscreen GP_FullscreenTriangleVS(UInt32 vertexID)
{
    FInterpolantFullscreen o;

    // NDC positions: clip-space Y flipped for DX convention
    Float2 ndc;
    ndc.x = (vertexID == 2u) ?  3.0f : -1.0f;
    ndc.y = (vertexID == 1u) ?  3.0f : -1.0f;

    o.svPosition = Float4(ndc.x, ndc.y, 0.0f, 1.0f);
    o.ndcXY      = ndc;
    o.uv         = GP_NDCToUV(ndc);

    return o;
}

// ─── LOD Screen-Space Error ───────────────────────────────────────────────────────────────────
// Used by culling and tessellation shaders to compute screen-space projected size of an object.

/// Compute the projected screen-space diameter of a world-space sphere.
/// \param center     World-space sphere center.
/// \param radius     Sphere radius.
/// \param proj       Projection matrix.
/// \param renderH    Render height in pixels.
/// \return           Approximate screen-space diameter in pixels.
Float32 GP_ProjectedSphereRadius(Float3 center, Float32 radius, Float4x4 viewProj, Float32 renderH)
{
    Float4 clipCenter = mul(viewProj, Float4(center, 1.0f));
    Float32 d = clipCenter.w;

    if (d <= 0.0f)
        return GP_LARGE_FLOAT; // Behind camera — treat as fully visible

    // Project a point offset by radius in view space
    Float32 projectedRadius = radius * viewProj[1][1] / d; // Uses fovY projection factor
    return projectedRadius * renderH;
}

#endif // GP_VERTEX_TRANSFORM_HLSLI
