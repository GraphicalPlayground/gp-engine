/// \file    Quaternion.hlsli
/// \brief   Quaternion and dual quaternion math for GPU use.
///          Quaternions are used for compact rotation storage in skeletal animation,
///          probe orientation, and camera interpolation.
///          Dual quaternions extend this to handle non-rigid (translation + rotation) transforms
///          for skinning without volume loss (see Skinning_DQS.CS.hlsl).
///
/// \details Storage convention:
///          Float4 q = (x, y, z, w) where w is the scalar part.
///          This matches the GPU-standard layout and DirectXMath convention.
///          A unit quaternion satisfies: dot(q, q) = 1.
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_QUATERNION_HLSLI
#define GP_QUATERNION_HLSLI

// ─── Quaternion Operations ────────────────────────────────────────────────────────────────────

/// Quaternion multiplication (Hamilton product).
/// Combines two rotations: result = a * b applies b first, then a.
/// \param a  Left quaternion (xyzw).
/// \param b  Right quaternion (xyzw).
Float4 GP_QuatMul(Float4 a, Float4 b)
{
    return Float4(
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
    );
}

/// Conjugate of a unit quaternion (inverse rotation, since |q|=1 means q^{-1} = q*).
Float4 GP_QuatConjugate(Float4 q)
{
    return Float4(-q.xyz, q.w);
}

/// Normalize a quaternion to unit length. Always call after arithmetic to prevent drift.
Float4 GP_QuatNormalize(Float4 q)
{
    return q / length(q);
}

/// Rotate a vector by a unit quaternion.
/// Uses the Rodrigues formula: v' = v + 2w(q.xyz × v) + 2(q.xyz × (q.xyz × v))
/// Equivalent to converting to matrix and multiplying, but ~30% fewer ops for a single vector.
Float3 GP_QuatRotate(Float4 q, Float3 v)
{
    Float3 u   = q.xyz;
    Float32 s  = q.w;
    Float3 r   = 2.0f * dot(u, v) * u
               + (2.0f * s * s - 1.0f) * v
               + 2.0f * s * cross(u, v);
    return r;
}

/// Build a quaternion from an axis-angle pair.
/// \param axis   Normalized rotation axis.
/// \param angle  Rotation angle in radians.
Float4 GP_QuatFromAxisAngle(Float3 axis, Float32 angle)
{
    Float32 halfAngle = angle * 0.5f;
    Float32 sinH, cosH;
    sincos(halfAngle, sinH, cosH);
    return Float4(axis * sinH, cosH);
}

/// Extract the rotation axis and angle from a unit quaternion.
/// \param q      Unit quaternion.
/// \param axis   Output: normalized rotation axis.
/// \param angle  Output: rotation angle in radians [0, 2π).
void GP_QuatToAxisAngle(Float4 q, out Float3 axis, out Float32 angle)
{
    angle = 2.0f * acos(clamp(q.w, -1.0f, 1.0f));
    Float32 sinHalfAngle = sqrt(max(0.0f, 1.0f - q.w * q.w));
    axis = (sinHalfAngle > 0.0001f) ? q.xyz / sinHalfAngle : Float3(1, 0, 0);
}

/// Convert a unit quaternion to a 3x3 rotation matrix.
Float3x3 GP_QuatToMatrix3x3(Float4 q)
{
    Float32 x2 = q.x * q.x, y2 = q.y * q.y, z2 = q.z * q.z;
    Float32 xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    Float32 wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    return Float3x3(
        1.0f - 2.0f * (y2 + z2),  2.0f * (xy - wz),          2.0f * (xz + wy),
        2.0f * (xy + wz),          1.0f - 2.0f * (x2 + z2),   2.0f * (yz - wx),
        2.0f * (xz - wy),          2.0f * (yz + wx),           1.0f - 2.0f * (x2 + y2)
    );
}

/// Convert a unit quaternion to a 4x4 rotation matrix (bottom-right = 1, rest = 0).
Float4x4 GP_QuatToMatrix4x4(Float4 q)
{
    Float3x3 m = GP_QuatToMatrix3x3(q);
    return Float4x4(
        Float4(m[0], 0.0f),
        Float4(m[1], 0.0f),
        Float4(m[2], 0.0f),
        Float4(0.0f, 0.0f, 0.0f, 1.0f)
    );
}

/// Build a quaternion from a 3x3 rotation matrix (Shepperd's method).
/// Numerically stable — avoids the division-by-zero edge case in naive implementations.
Float4 GP_QuatFromMatrix3x3(Float3x3 m)
{
    Float32 trace = m[0][0] + m[1][1] + m[2][2];
    Float4 q;

    if (trace > 0.0f)
    {
        Float32 s = 0.5f / sqrt(trace + 1.0f);
        q = Float4((m[2][1] - m[1][2]) * s,
                   (m[0][2] - m[2][0]) * s,
                   (m[1][0] - m[0][1]) * s,
                   0.25f / s);
    }
    else if (m[0][0] > m[1][1] && m[0][0] > m[2][2])
    {
        Float32 s = 2.0f * sqrt(1.0f + m[0][0] - m[1][1] - m[2][2]);
        q = Float4(0.25f * s,
                   (m[0][1] + m[1][0]) / s,
                   (m[0][2] + m[2][0]) / s,
                   (m[2][1] - m[1][2]) / s);
    }
    else if (m[1][1] > m[2][2])
    {
        Float32 s = 2.0f * sqrt(1.0f + m[1][1] - m[0][0] - m[2][2]);
        q = Float4((m[0][1] + m[1][0]) / s,
                   0.25f * s,
                   (m[1][2] + m[2][1]) / s,
                   (m[0][2] - m[2][0]) / s);
    }
    else
    {
        Float32 s = 2.0f * sqrt(1.0f + m[2][2] - m[0][0] - m[1][1]);
        q = Float4((m[0][2] + m[2][0]) / s,
                   (m[1][2] + m[2][1]) / s,
                   0.25f * s,
                   (m[1][0] - m[0][1]) / s);
    }

    return normalize(q);
}

/// Spherical linear interpolation (SLERP) between two unit quaternions.
/// \param a  Start rotation.
/// \param b  End rotation.
/// \param t  Blend factor [0, 1].
Float4 GP_QuatSlerp(Float4 a, Float4 b, Float32 t)
{
    Float32 cosHalfAngle = dot(a, b);

    // Flip b if quaternions point in opposite hemispheres (take the shorter arc)
    if (cosHalfAngle < 0.0f)
    {
        b             = -b;
        cosHalfAngle  = -cosHalfAngle;
    }

    // Fallback to NLERP for nearly identical quaternions (avoids division by zero)
    if (cosHalfAngle > 0.9995f)
        return normalize(lerp(a, b, t));

    Float32 halfAngle = acos(clamp(cosHalfAngle, -1.0f, 1.0f));
    Float32 sinHalf   = sin(halfAngle);

    return (a * sin((1.0f - t) * halfAngle) + b * sin(t * halfAngle)) / sinHalf;
}

/// Normalized linear interpolation (NLERP). Faster than SLERP, slightly non-constant speed.
Float4 GP_QuatNlerp(Float4 a, Float4 b, Float32 t)
{
    if (dot(a, b) < 0.0f) b = -b;
    return normalize(lerp(a, b, t));
}

/// Build a quaternion that rotates from direction `from` to direction `to`.
Float4 GP_QuatFromToRotation(Float3 from, Float3 to)
{
    Float3 axis = cross(from, to);
    Float32 sinA = length(axis);
    Float32 cosA = dot(from, to);

    if (sinA < 0.0001f)
    {
        // Vectors are parallel or anti-parallel
        if (cosA > 0.0f)
            return Float4(0, 0, 0, 1); // Identity — same direction
        else
        {
            // 180° rotation — pick any perpendicular axis
            Float3 perp = abs(from.x) < 0.9f ? Float3(1, 0, 0) : Float3(0, 1, 0);
            axis = normalize(cross(from, perp));
            return Float4(axis, 0);
        }
    }

    axis /= sinA;
    Float32 halfAngle = atan2(sinA, cosA) * 0.5f;
    Float32 sinH, cosH;
    sincos(halfAngle, sinH, cosH);
    return Float4(axis * sinH, cosH);
}

// ─── Dual Quaternions ─────────────────────────────────────────────────────────────────────────
// A dual quaternion encodes a rigid transform (rotation + translation) as two quaternions:
//   real part q_r = rotation quaternion
//   dual part q_d = 0.5 * translation_quat * q_r (where translation_quat = (t, 0))
//
// Dual quaternion skinning avoids the "candy wrapper" artifact of Linear Blend Skinning
// because DQ interpolation is a proper rigid body blend (no volume loss at joints).
// Reference: Kavan et al. "Skinning with Dual Quaternions" (2007).

struct FDualQuat
{
    Float4 real;    ///< Rotation part (unit quaternion: xyzw).
    Float4 dual;    ///< Translation encoding: 0.5 * (0, t) * real.
};

/// Create a dual quaternion from a rotation quaternion and a translation vector.
FDualQuat GP_DualQuatFromRT(Float4 rotation, Float3 translation)
{
    FDualQuat dq;
    dq.real = rotation;
    dq.dual = GP_QuatMul(Float4(translation * 0.5f, 0.0f), rotation);
    return dq;
}

/// Create a dual quaternion from a 4x4 TRS matrix.
FDualQuat GP_DualQuatFromMatrix(Float4x4 m)
{
    Float3x3 rot3 = (Float3x3)m;
    Float4   r    = GP_QuatFromMatrix3x3(rot3);
    Float3   t    = m[3].xyz;
    return GP_DualQuatFromRT(r, t);
}

/// Add two dual quaternions (used to accumulate weighted blend).
FDualQuat GP_DualQuatAdd(FDualQuat a, FDualQuat b)
{
    FDualQuat result;
    result.real = a.real + b.real;
    result.dual = a.dual + b.dual;
    return result;
}

/// Scale a dual quaternion (used with blend weights in skinning).
FDualQuat GP_DualQuatScale(FDualQuat dq, Float32 w)
{
    FDualQuat result;
    result.real = dq.real * w;
    result.dual = dq.dual * w;
    return result;
}

/// Normalize a dual quaternion: divide by the magnitude of the real part.
FDualQuat GP_DualQuatNormalize(FDualQuat dq)
{
    Float32 mag = length(dq.real);
    FDualQuat result;
    result.real = dq.real / mag;
    result.dual = dq.dual / mag;
    return result;
}

/// Transform a position by a unit dual quaternion.
Float3 GP_DualQuatTransformPoint(FDualQuat dq, Float3 p)
{
    // Extract rotation and translation from dual quaternion
    Float3 t = 2.0f * (dq.real.w * dq.dual.xyz - dq.dual.w * dq.real.xyz
             + cross(dq.real.xyz, dq.dual.xyz));

    return GP_QuatRotate(dq.real, p) + t;
}

/// Transform a direction (no translation) by a unit dual quaternion.
Float3 GP_DualQuatTransformDir(FDualQuat dq, Float3 v)
{
    return GP_QuatRotate(dq.real, v);
}

#endif // GP_QUATERNION_HLSLI
