/// \file    Transform.hlsli
/// \brief   Matrix construction, decomposition, and coordinate space transformation utilities.
///          Covers TRS matrices, view/projection construction, NDC ↔ World, and velocity vectors.
///
/// \details Coordinate system conventions used by this engine:
///          - World space:  Right-handed, Y-up (X=right, Y=up, Z=forward toward camera).
///          - View space:   Right-handed, Y-up, -Z into the screen (camera looks down -Z).
///          - Clip space:   Left-handed, DirectX convention (NDC x∈[-1,1], y∈[-1,1], z∈[0,1]).
///                          With reversed-Z: near maps to z=1, far maps to z=0.
///          - UV space:     (0,0) = top-left, (1,1) = bottom-right.
///          - Depth buffer: Reversed-Z, D32_FLOAT (see DepthEncoding.hlsli).
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_TRANSFORM_HLSLI
#define GP_TRANSFORM_HLSLI

// ─── Matrix Construction ──────────────────────────────────────────────────────────────────────

/// Build a translation matrix.
Float4x4 GP_MatTranslation(Float3 t)
{
    return Float4x4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        t.x, t.y, t.z, 1
    );
}

/// Build a uniform scale matrix.
Float4x4 GP_MatScale(Float32 s)
{
    return Float4x4(
        s, 0, 0, 0,
        0, s, 0, 0,
        0, 0, s, 0,
        0, 0, 0, 1
    );
}

/// Build a non-uniform scale matrix.
Float4x4 GP_MatScaleXYZ(Float3 s)
{
    return Float4x4(
        s.x, 0,   0,   0,
        0,   s.y, 0,   0,
        0,   0,   s.z, 0,
        0,   0,   0,   1
    );
}

/// Build a rotation matrix around the X axis.
Float4x4 GP_MatRotX(Float32 angle)
{
    Float32 s, c;
    sincos(angle, s, c);
    return Float4x4(
        1,  0,  0, 0,
        0,  c, -s, 0,
        0,  s,  c, 0,
        0,  0,  0, 1
    );
}

/// Build a rotation matrix around the Y axis.
Float4x4 GP_MatRotY(Float32 angle)
{
    Float32 s, c;
    sincos(angle, s, c);
    return Float4x4(
         c, 0, s, 0,
         0, 1, 0, 0,
        -s, 0, c, 0,
         0, 0, 0, 1
    );
}

/// Build a rotation matrix around the Z axis.
Float4x4 GP_MatRotZ(Float32 angle)
{
    Float32 s, c;
    sincos(angle, s, c);
    return Float4x4(
        c, -s, 0, 0,
        s,  c, 0, 0,
        0,  0, 1, 0,
        0,  0, 0, 1
    );
}

/// Build a rotation matrix from an axis-angle pair (Rodrigues' formula).
Float4x4 GP_MatFromAxisAngle(Float3 axis, Float32 angle)
{
    Float32 s, c;
    sincos(angle, s, c);
    Float32 t   = 1.0f - c;
    Float3  a   = normalize(axis);

    return Float4x4(
        t*a.x*a.x + c,     t*a.x*a.y - s*a.z, t*a.x*a.z + s*a.y, 0,
        t*a.x*a.y + s*a.z, t*a.y*a.y + c,     t*a.y*a.z - s*a.x, 0,
        t*a.x*a.z - s*a.y, t*a.y*a.z + s*a.x, t*a.z*a.z + c,     0,
        0,                  0,                  0,                  1
    );
}

/// Build a TRS (translate-rotate-scale) matrix from components.
/// Apply order: Scale first, then Rotate, then Translate (standard SRT order).
Float4x4 GP_MatTRS(Float3 translation, Float4 rotationQuat, Float3 scale)
{
    Float3x3 rot = GP_QuatToMatrix3x3(rotationQuat);

    return Float4x4(
        Float4(rot[0] * scale.x, 0),
        Float4(rot[1] * scale.y, 0),
        Float4(rot[2] * scale.z, 0),
        Float4(translation, 1)
    );
}

/// Inverse of a TRS matrix. Exploits the factored form to avoid a full 4x4 inversion.
Float4x4 GP_MatInverseTRS(Float3 translation, Float4 rotQuat, Float3 scale)
{
    Float3   invScale = 1.0f / scale;
    Float4   invRot   = GP_QuatConjugate(rotQuat);
    Float3   invT     = GP_QuatRotate(invRot, -translation * invScale);
    return GP_MatTRS(invT, invRot, invScale);
}

/// Extract translation from a 4x4 TRS matrix (last row, XYZ).
Float3 GP_MatGetTranslation(Float4x4 m)
{
    return m[3].xyz;
}

/// Extract uniform scale from a 4x4 TRS matrix (length of each column).
Float3 GP_MatGetScale(Float4x4 m)
{
    return Float3(length(m[0].xyz), length(m[1].xyz), length(m[2].xyz));
}

// ─── View Matrix Construction ─────────────────────────────────────────────────────────────────

/// Build a right-handed look-at view matrix (camera looks toward target).
/// \param eye     Camera world-space position.
/// \param target  World-space point to look at.
/// \param up      World-space up vector (typically (0,1,0)).
Float4x4 GP_MatLookAt(Float3 eye, Float3 target, Float3 up)
{
    Float3 f = normalize(target - eye);     // forward (-Z in view space for RH)
    Float3 r = normalize(cross(f, up));     // right
    Float3 u = cross(r, f);                 // recomputed up (orthogonalized)

    return Float4x4(
         r.x,           u.x,          -f.x,          0,
         r.y,           u.y,          -f.y,          0,
         r.z,           u.z,          -f.z,          0,
        -dot(r, eye),  -dot(u, eye),   dot(f, eye),  1
    );
}

// ─── Projection Matrix Construction ──────────────────────────────────────────────────────────

/// Build a reversed-Z infinite perspective projection matrix (DX clip space).
/// Reversed-Z: near maps to 1.0, infinity maps to 0.0 — maximizes float depth precision.
/// Infinite far plane: eliminates far-clipping artifacts for large worlds.
/// \param fovY   Vertical field-of-view in radians.
/// \param aspect Aspect ratio (width / height).
/// \param near   Near plane distance (positive).
Float4x4 GP_MatPerspectiveReversedZ(Float32 fovY, Float32 aspect, Float32 near)
{
    Float32 f = 1.0f / tan(fovY * 0.5f);
    return Float4x4(
        f / aspect,  0,  0,     0,
        0,           f,  0,     0,
        0,           0,  0,    -1,
        0,           0,  near,  0
    );
}

/// Build a reversed-Z finite perspective projection matrix.
/// Use when a finite far plane is needed (e.g., shadow cameras, reflection captures).
Float4x4 GP_MatPerspectiveReversedZFinite(Float32 fovY, Float32 aspect, Float32 near, Float32 far)
{
    Float32 f  = 1.0f / tan(fovY * 0.5f);
    Float32 q  = near / (far - near);
    return Float4x4(
        f / aspect,  0,  0,              0,
        0,           f,  0,              0,
        0,           0,  q,             -1,
        0,           0,  q * far,        0
    );
}

/// Build a standard orthographic projection matrix (DX clip space).
/// \param width   Total width of the orthographic volume.
/// \param height  Total height.
/// \param near    Near plane.
/// \param far     Far plane.
Float4x4 GP_MatOrtho(Float32 width, Float32 height, Float32 near, Float32 far)
{
    Float32 invDepth = 1.0f / (far - near);
    return Float4x4(
        2.0f / width,  0,             0,                  0,
        0,             2.0f / height, 0,                  0,
        0,             0,            -invDepth,            0,
        0,             0,            -near * invDepth,     1
    );
}

/// Build an orthographic projection from explicit left/right/bottom/top/near/far bounds.
/// Used for shadow cascade cameras and editor viewport tiles.
Float4x4 GP_MatOrthoOffCenter(Float32 l, Float32 r, Float32 b, Float32 t, Float32 n, Float32 f)
{
    Float32 rml = r - l, tmb = t - b, fmn = f - n;
    return Float4x4(
        2.0f / rml,       0,              0,              0,
        0,                2.0f / tmb,     0,              0,
        0,                0,             -1.0f / fmn,     0,
       -(r+l) / rml,    -(t+b) / tmb,   -n / fmn,        1
    );
}

// ─── Coordinate Space Transforms ──────────────────────────────────────────────────────────────

/// Transform a world-space position to clip space.
Float4 GP_WorldToClip(Float3 worldPos, Float4x4 viewProj)
{
    return mul(viewProj, Float4(worldPos, 1.0f));
}

/// Transform a world-space position to NDC [-1,1]^2, [0,1] (DX reversed-Z).
Float3 GP_WorldToNDC(Float3 worldPos, Float4x4 viewProj)
{
    Float4 clip = GP_WorldToClip(worldPos, viewProj);
    return clip.xyz / clip.w;
}

/// Transform a world-space position to screen UV [0,1]^2 + depth.
Float3 GP_WorldToScreenUV(Float3 worldPos, Float4x4 viewProj)
{
    Float3 ndc = GP_WorldToNDC(worldPos, viewProj);
    return Float3(GP_NDCToUV(ndc.xy), ndc.z);
}

/// Transform a world-space direction to view space (no translation applied).
Float3 GP_WorldDirToView(Float3 dir, Float4x4 viewMatrix)
{
    return mul((Float3x3)viewMatrix, dir);
}

/// Reconstruct view-space position from NDC + depth using the projection matrix.
Float3 GP_NDCToViewSpace(Float3 ndc, Float4x4 invProj)
{
    Float4 viewPos = mul(invProj, Float4(ndc, 1.0f));
    return viewPos.xyz / viewPos.w;
}

// ─── Velocity / Motion Vectors ────────────────────────────────────────────────────────────────
// Motion vectors encode per-pixel displacement from the previous frame's screen position
// to the current frame's screen position, in NDC space (not UV space).
// TAA and motion blur read these from the velocity buffer (R16G16_FLOAT).

/// Compute the screen-space motion vector for a surface point.
/// \param worldPos      World-space position of the surface point (current frame).
/// \param prevWorldPos  World-space position in the previous frame (after skinning/vertex anim).
/// \param currVP        Current frame's view-projection matrix.
/// \param prevVP        Previous frame's view-projection matrix.
/// \return              Motion vector in NDC space (current - previous).
Float2 GP_ComputeMotionVector(Float3 worldPos, Float3 prevWorldPos, Float4x4 currVP, Float4x4 prevVP)
{
    Float4 currClip = mul(currVP, Float4(worldPos,     1.0f));
    Float4 prevClip = mul(prevVP, Float4(prevWorldPos, 1.0f));

    Float2 currNDC  = currClip.xy / currClip.w;
    Float2 prevNDC  = prevClip.xy / prevClip.w;

    // Remove TAA jitter from both positions to get the "true" motion
    currNDC -= g_Frame.jitterOffset     * 2.0f * GP_INV_RENDER_RES;
    prevNDC -= g_Frame.prevJitterOffset * 2.0f * GP_INV_RENDER_RES;

    return currNDC - prevNDC;
}

/// Convert a motion vector from NDC space to screen UV space.
Float2 GP_MotionVectorNDCToUV(Float2 mvNDC)
{
    return mvNDC * Float2(0.5f, -0.5f); // NDC Y is flipped relative to UV Y
}

// ─── Normal Transform ─────────────────────────────────────────────────────────────────────────

/// Transform a normal vector by the inverse-transpose of a model matrix.
/// Required for non-uniform scale: normals transform by (M^{-1})^T, not M.
/// \param n          Object-space normal.
/// \param invTransM  Inverse-transpose of the model matrix (upper 3x3 is sufficient).
Float3 GP_TransformNormal(Float3 n, Float3x3 invTransM)
{
    return normalize(mul(invTransM, n));
}

/// Cheap approximation: transform normal by the model matrix and renormalize.
/// Correct only for uniform scale, or when the error is acceptable (fast path).
Float3 GP_TransformNormalFast(Float3 n, Float3x3 modelMatrix3x3)
{
    return normalize(mul(modelMatrix3x3, n));
}

// ─── Billboard ────────────────────────────────────────────────────────────────────────────────

/// Build a world-space billboard matrix that always faces the camera.
/// \param position  World-space center of the billboard.
/// \param cameraPos World-space camera position.
/// \param up        Up vector (typically world Y).
Float4x4 GP_MatBillboard(Float3 position, Float3 cameraPos, Float3 up)
{
    Float3 look  = normalize(cameraPos - position);
    Float3 right = normalize(cross(up, look));
    Float3 localUp = cross(look, right);

    return Float4x4(
        Float4(right,   0),
        Float4(localUp, 0),
        Float4(look,    0),
        Float4(position, 1)
    );
}

/// Build a billboard matrix aligned to the Y axis only (cylindrical billboard).
/// Useful for tree impostors and particles that should not tilt forward/back.
Float4x4 GP_MatBillboardAxisY(Float3 position, Float3 cameraPos)
{
    Float3 toCamera = cameraPos - position;
    toCamera.y      = 0;
    Float3 look     = normalize(toCamera);
    Float3 right    = cross(Float3(0,1,0), look);

    return Float4x4(
        Float4(right,        0),
        Float4(0, 1, 0,      0),
        Float4(look,         0),
        Float4(position,     1)
    );
}

#endif // GP_TRANSFORM_HLSLI
