/// \file    Intersection.hlsli
/// \brief   Ray-primitive intersection tests and geometric query utilities.
///          Used by: culling shaders, inline raytracing, SSAO, SSGI, DDGI, and Hi-Z occlusion.
///
/// \details All ray tests use the parametric form: P(t) = ray.origin + t * ray.direction.
///          Returned t values are in world-space units. t < 0 means the intersection is
///          behind the ray origin. Callers must check tMin/tMax validity.
///
///          Primitives covered:
///          - AABB (axis-aligned bounding box)
///          - Sphere
///          - Triangle (Möller-Trumbore)
///          - Plane
///          - Capsule
///          - OBB (oriented bounding box) — via AABB in local space
///          - Cone
///          - Frustum (plane-set test for culling)
///
/// \warning Never include this file directly; always include Common.hlsli.

#ifndef GP_INTERSECTION_HLSLI
#define GP_INTERSECTION_HLSLI

// ─── Ray Structure ────────────────────────────────────────────────────────────────────────────

struct FRay
{
    Float3 origin;
    Float3 direction;       ///< Must be normalized for correct t values.
    Float32 tMin;           ///< Minimum valid t (typically EPSILON to avoid self-intersection).
    Float32 tMax;           ///< Maximum valid t (e.g., FLT_MAX for primary rays, light dist for shadows).
};

/// Construct a ray with default tMin=EPSILON, tMax=FLT_MAX.
FRay GP_MakeRay(Float3 origin, Float3 direction)
{
    FRay r;
    r.origin    = origin;
    r.direction = direction;
    r.tMin      = GP_EPSILON;
    r.tMax      = GP_LARGE_FLOAT;
    return r;
}

/// Evaluate a point on a ray at parameter t.
Float3 GP_RayPoint(FRay r, Float32 t)
{
    return r.origin + t * r.direction;
}

// ─── AABB ─────────────────────────────────────────────────────────────────────────────────────

struct FAABB
{
    Float3 minBound;
    Float3 maxBound;
};

/// Construct an AABB from a center and half-extents.
FAABB GP_AABBFromCenterExtents(Float3 center, Float3 halfExtents)
{
    FAABB box;
    box.minBound = center - halfExtents;
    box.maxBound = center + halfExtents;
    return box;
}

/// Expand an AABB to contain a point.
FAABB GP_AABBExpand(FAABB box, Float3 point)
{
    FAABB result;
    result.minBound = min(box.minBound, point);
    result.maxBound = max(box.maxBound, point);
    return result;
}

/// Merge two AABBs.
FAABB GP_AABBMerge(FAABB a, FAABB b)
{
    FAABB result;
    result.minBound = min(a.minBound, b.minBound);
    result.maxBound = max(a.maxBound, b.maxBound);
    return result;
}

/// Ray-AABB intersection (slab method). Returns true if the ray hits the box.
/// \param ray   Input ray.
/// \param box   AABB to test.
/// \param tNear Output: entry parameter (may be negative if ray origin is inside).
/// \param tFar  Output: exit parameter.
bool GP_RayAABB(FRay ray, FAABB box, out Float32 tNear, out Float32 tFar)
{
    Float3 invDir = 1.0f / ray.direction;
    Float3 t0     = (box.minBound - ray.origin) * invDir;
    Float3 t1     = (box.maxBound - ray.origin) * invDir;

    Float3 tmin3  = min(t0, t1);
    Float3 tmax3  = max(t0, t1);

    tNear = max(max(tmin3.x, tmin3.y), tmin3.z);
    tFar  = min(min(tmax3.x, tmax3.y), tmax3.z);

    return tNear <= tFar && tFar >= ray.tMin && tNear <= ray.tMax;
}

/// Simpler AABB ray test — returns only true/false. Slightly faster.
bool GP_RayAABBFast(Float3 origin, Float3 invDir, FAABB box)
{
    Float3 t0   = (box.minBound - origin) * invDir;
    Float3 t1   = (box.maxBound - origin) * invDir;
    Float3 tmin = min(t0, t1);
    Float3 tmax = max(t0, t1);
    return max(max(tmin.x, tmin.y), tmin.z) <= min(min(tmax.x, tmax.y), tmax.z);
}

/// AABB vs frustum test (conservative): returns false only if definitely outside all planes.
/// \param box    AABB to test.
/// \param planes Array of 6 frustum planes (each Float4: xyz=normal, w=distance from origin).
bool GP_AABBFrustumTest(FAABB box, Float4 planes[6])
{
    GP_UNROLL
    for (Int32 i = 0; i < 6; ++i)
    {
        Float3 n    = planes[i].xyz;
        Float32 d   = planes[i].w;

        // Find the positive vertex (farthest in direction of plane normal)
        Float3 p;
        p.x = (n.x >= 0.0f) ? box.maxBound.x : box.minBound.x;
        p.y = (n.y >= 0.0f) ? box.maxBound.y : box.minBound.y;
        p.z = (n.z >= 0.0f) ? box.maxBound.z : box.minBound.z;

        if (dot(n, p) + d < 0.0f)
            return false; // Outside this plane — definitively culled
    }
    return true;
}

/// AABB center and half-extent, useful for conservative sphere-frustum pre-test.
Float3 GP_AABBCenter(FAABB box)     { return (box.minBound + box.maxBound) * 0.5f; }
Float3 GP_AABBExtents(FAABB box)    { return (box.maxBound - box.minBound) * 0.5f; }
Float32 GP_AABBRadius(FAABB box)    { return length(GP_AABBExtents(box)); }

// ─── Sphere ───────────────────────────────────────────────────────────────────────────────────

/// Ray-sphere intersection. Returns the number of intersections (0, 1, or 2).
/// \param ray      Input ray.
/// \param center   Sphere center.
/// \param radius   Sphere radius.
/// \param t0       Output: smaller t (entry). Negative if ray origin is inside sphere.
/// \param t1       Output: larger t (exit).
Int32 GP_RaySphere(FRay ray, Float3 center, Float32 radius, out Float32 t0, out Float32 t1)
{
    Float3  oc    = ray.origin - center;
    Float32 a     = dot(ray.direction, ray.direction); // 1.0 if direction is normalized
    Float32 b     = 2.0f * dot(oc, ray.direction);
    Float32 c     = dot(oc, oc) - radius * radius;
    Float32 disc  = b * b - 4.0f * a * c;

    t0 = t1 = GP_LARGE_FLOAT;

    if (disc < 0.0f)
        return 0; // No intersection

    Float32 sqrtDisc = sqrt(disc);
    t0 = (-b - sqrtDisc) / (2.0f * a);
    t1 = (-b + sqrtDisc) / (2.0f * a);

    return (disc == 0.0f) ? 1 : 2;
}

/// Sphere vs frustum test (conservative, fast).
/// Returns false only if the sphere is definitively outside all 6 planes.
bool GP_SphereFrustumTest(Float3 center, Float32 radius, Float4 planes[6])
{
    GP_UNROLL
    for (Int32 i = 0; i < 6; ++i)
    {
        if (dot(planes[i].xyz, center) + planes[i].w < -radius)
            return false;
    }
    return true;
}

/// Point in sphere test.
bool GP_PointInSphere(Float3 point, Float3 center, Float32 radius)
{
    Float3 d = point - center;
    return dot(d, d) <= radius * radius;
}

/// Sphere vs sphere intersection test.
bool GP_SphereSphere(Float3 c0, Float32 r0, Float3 c1, Float32 r1)
{
    Float3  d   = c1 - c0;
    Float32 rSum = r0 + r1;
    return dot(d, d) <= rSum * rSum;
}

// ─── Triangle (Möller-Trumbore) ───────────────────────────────────────────────────────────────
// Reference: Möller & Trumbore (1997) "Fast, Minimum Storage Ray-Triangle Intersection"

/// Ray-triangle intersection. Returns true if the ray hits the triangle.
/// \param ray   Input ray.
/// \param v0    Triangle vertex 0.
/// \param v1    Triangle vertex 1.
/// \param v2    Triangle vertex 2.
/// \param t     Output: ray parameter at intersection.
/// \param bary  Output: barycentric coordinates (u, v) where w = 1 - u - v.
bool GP_RayTriangle(FRay ray, Float3 v0, Float3 v1, Float3 v2,
                    out Float32 t, out Float2 bary)
{
    t    = GP_LARGE_FLOAT;
    bary = (Float2)0;

    Float3 e1 = v1 - v0;
    Float3 e2 = v2 - v0;
    Float3 h  = cross(ray.direction, e2);
    Float32 a = dot(e1, h);

    // Backface: a < 0. Use abs(a) for double-sided test.
    if (abs(a) < GP_EPSILON)
        return false;

    Float32 f  = 1.0f / a;
    Float3  s  = ray.origin - v0;
    Float32 u  = f * dot(s, h);

    if (u < 0.0f || u > 1.0f)
        return false;

    Float3  q  = cross(s, e1);
    Float32 v  = f * dot(ray.direction, q);

    if (v < 0.0f || u + v > 1.0f)
        return false;

    t = f * dot(e2, q);

    if (t < ray.tMin || t > ray.tMax)
        return false;

    bary = Float2(u, v);
    return true;
}

/// Compute the normal of a triangle (not normalized).
Float3 GP_TriangleNormal(Float3 v0, Float3 v1, Float3 v2)
{
    return cross(v1 - v0, v2 - v0);
}

/// Compute the area of a triangle.
Float32 GP_TriangleArea(Float3 v0, Float3 v1, Float3 v2)
{
    return length(GP_TriangleNormal(v0, v1, v2)) * 0.5f;
}

/// Interpolate a Float3 attribute over a triangle using barycentric coordinates.
Float3 GP_BaryInterp(Float3 a0, Float3 a1, Float3 a2, Float2 bary)
{
    return a0 * (1.0f - bary.x - bary.y) + a1 * bary.x + a2 * bary.y;
}

Float2 GP_BaryInterp(Float2 a0, Float2 a1, Float2 a2, Float2 bary)
{
    return a0 * (1.0f - bary.x - bary.y) + a1 * bary.x + a2 * bary.y;
}

// ─── Plane ────────────────────────────────────────────────────────────────────────────────────

/// Ray-plane intersection. The plane is defined by (normal, distance): dot(n, p) = d.
/// \param ray    Input ray.
/// \param plane  Float4(normal.xyz, d).
/// \param t      Output: intersection parameter. Negative if behind the ray.
bool GP_RayPlane(FRay ray, Float4 plane, out Float32 t)
{
    Float32 denom = dot(plane.xyz, ray.direction);
    t = GP_LARGE_FLOAT;

    if (abs(denom) < GP_EPSILON)
        return false; // Ray is parallel to plane

    t = -(dot(plane.xyz, ray.origin) + plane.w) / denom;
    return t >= ray.tMin && t <= ray.tMax;
}

/// Signed distance from a point to a plane. Positive = in front of (same side as normal).
Float32 GP_SignedDistToPlane(Float3 p, Float4 plane)
{
    return dot(plane.xyz, p) + plane.w;
}

// ─── Capsule ──────────────────────────────────────────────────────────────────────────────────

/// Ray-capsule intersection (capsule = cylinder + two hemisphere end caps).
/// \param ray    Input ray.
/// \param a      One end of the capsule axis.
/// \param b      Other end of the capsule axis.
/// \param radius Capsule radius.
/// \param t      Output: nearest intersection parameter.
bool GP_RayCapsule(FRay ray, Float3 a, Float3 b, Float32 radius, out Float32 t)
{
    Float3 ab   = b - a;
    Float3 ao   = ray.origin - a;
    Float3 rd   = ray.direction;

    Float32 abLenSq  = dot(ab, ab);
    Float32 abDotRd  = dot(ab, rd);
    Float32 abDotAo  = dot(ab, ao);

    Float32 A = abLenSq - abDotRd * abDotRd;
    Float32 B = 2.0f * (abLenSq * dot(ao, rd) - abDotAo * abDotRd);
    Float32 C = abLenSq * dot(ao, ao) - abDotAo * abDotAo - radius * radius * abLenSq;

    Float32 disc = B * B - 4.0f * A * C;
    t = GP_LARGE_FLOAT;

    if (disc < 0.0f)
        return false;

    Float32 sqrtDisc = sqrt(disc);
    Float32 t0 = (-B - sqrtDisc) / (2.0f * A);
    Float32 t1 = (-B + sqrtDisc) / (2.0f * A);

    // Check if the infinite cylinder hit is within the finite segment
    Float32 y0 = abDotAo + t0 * abDotRd;
    Float32 y1 = abDotAo + t1 * abDotRd;

    if (y0 > 0.0f && y0 < abLenSq)
    {
        t = t0;
        return t >= ray.tMin && t <= ray.tMax;
    }

    // Test end caps (sphere intersection at each end)
    Float32 ts, te;
    FRay capRay = ray;
    Float3 capCenter = (y0 <= 0.0f) ? a : b;
    Int32 hits = GP_RaySphere(capRay, capCenter, radius, ts, te);
    if (hits > 0 && ts >= ray.tMin && ts <= ray.tMax)
    {
        t = ts;
        return true;
    }

    return false;
}

// ─── Cone ─────────────────────────────────────────────────────────────────────────────────────

/// Ray-cone intersection (infinite cone). Returns true and sets t to the nearest hit.
/// The cone is defined by apex, axis direction, and half-angle.
bool GP_RayCone(FRay ray, Float3 apex, Float3 axis, Float32 halfAngle, out Float32 t)
{
    Float3  co     = ray.origin - apex;
    Float32 cosA   = cos(halfAngle);
    Float32 cos2A  = cosA * cosA;
    Float3  d      = ray.direction;
    Float3  v      = axis; // normalized cone axis

    Float32 dv = dot(d, v);
    Float32 cv = dot(co, v);

    Float32 A = dv * dv - cos2A;
    Float32 B = 2.0f * (dv * cv - dot(d, co) * cos2A);
    Float32 C = cv * cv - dot(co, co) * cos2A;

    t = GP_LARGE_FLOAT;

    Float32 disc = B * B - 4.0f * A * C;
    if (disc < 0.0f)
        return false;

    Float32 sqrtDisc = sqrt(disc);
    Float32 t0 = (-B - sqrtDisc) / (2.0f * A);
    Float32 t1 = (-B + sqrtDisc) / (2.0f * A);

    // Accept only the intersection on the correct half of the double cone
    GP_BRANCH
    if (t0 >= ray.tMin)
    {
        Float3 pt = GP_RayPoint(ray, t0);
        if (dot(pt - apex, v) >= 0.0f)
        {
            t = t0;
            return t <= ray.tMax;
        }
    }

    if (t1 >= ray.tMin)
    {
        Float3 pt = GP_RayPoint(ray, t1);
        if (dot(pt - apex, v) >= 0.0f)
        {
            t = t1;
            return t <= ray.tMax;
        }
    }

    return false;
}

// ─── Screen-Space Closest Point ───────────────────────────────────────────────────────────────

/// Find the closest point on a line segment to a given point (3D).
Float3 GP_ClosestPointOnSegment(Float3 p, Float3 a, Float3 b)
{
    Float3  ab = b - a;
    Float32 t  = dot(p - a, ab) / dot(ab, ab);
    return a + ab * saturate(t);
}

/// Find the closest point on an AABB surface to a point p.
Float3 GP_ClosestPointOnAABB(Float3 p, FAABB box)
{
    return clamp(p, box.minBound, box.maxBound);
}

/// Squared distance from a point to an AABB.
Float32 GP_DistSqPointAABB(Float3 p, FAABB box)
{
    Float3 d = max(Float3(0,0,0), max(box.minBound - p, p - box.maxBound));
    return dot(d, d);
}

#endif // GP_INTERSECTION_HLSLI
