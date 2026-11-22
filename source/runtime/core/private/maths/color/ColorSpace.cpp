// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/color/ColorSpace.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Constants.hpp"       // IWYU pragma: keep
#include "maths/base/FloatingPoint.hpp"   // IWYU pragma: keep
#include "threading/sync/Mutex.hpp"
#include <cmath>

namespace gp::math
{

namespace
{

/// @brief Returns the `4x4` identity matrix.
/// @return Identity `Matrix4x4<Float64>`.
[[nodiscard]] constexpr Matrix4x4<gp::Float64> makeIdentity4x4() noexcept
{
    Matrix4x4<gp::Float64> m{};
    m.m00 = 1.0;
    m.m11 = 1.0;
    m.m22 = 1.0;
    m.m33 = 1.0;
    return m;
}

/// @brief Multiplies two `4x4` matrices (`result = a * b`).
/// @param[in] a Left matrix.
/// @param[in] b Right matrix.
/// @return Matrix product `a * b`.
[[nodiscard]] constexpr Matrix4x4<gp::Float64>
    multiply4x4(const Matrix4x4<gp::Float64>& a, const Matrix4x4<gp::Float64>& b) noexcept
{
    Matrix4x4<gp::Float64> out{};

    out.m00 = a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20 + a.m03 * b.m30;
    out.m01 = a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21 + a.m03 * b.m31;
    out.m02 = a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22 + a.m03 * b.m32;
    out.m03 = a.m00 * b.m03 + a.m01 * b.m13 + a.m02 * b.m23 + a.m03 * b.m33;

    out.m10 = a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20 + a.m13 * b.m30;
    out.m11 = a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21 + a.m13 * b.m31;
    out.m12 = a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22 + a.m13 * b.m32;
    out.m13 = a.m10 * b.m03 + a.m11 * b.m13 + a.m12 * b.m23 + a.m13 * b.m33;

    out.m20 = a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20 + a.m23 * b.m30;
    out.m21 = a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21 + a.m23 * b.m31;
    out.m22 = a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22 + a.m23 * b.m32;
    out.m23 = a.m20 * b.m03 + a.m21 * b.m13 + a.m22 * b.m23 + a.m23 * b.m33;

    out.m30 = a.m30 * b.m00 + a.m31 * b.m10 + a.m32 * b.m20 + a.m33 * b.m30;
    out.m31 = a.m30 * b.m01 + a.m31 * b.m11 + a.m32 * b.m21 + a.m33 * b.m31;
    out.m32 = a.m30 * b.m02 + a.m31 * b.m12 + a.m32 * b.m22 + a.m33 * b.m32;
    out.m33 = a.m30 * b.m03 + a.m31 * b.m13 + a.m32 * b.m23 + a.m33 * b.m33;

    return out;
}

/// @brief Multiplies the upper-left `3x3` block of a `4x4` matrix by a 3D column vector (`M * v`).
/// @param[in] m `4x4` matrix.
/// @param[in] v 3D column vector.
/// @return Transformed 3D vector.
[[nodiscard]] constexpr Vector3<gp::Float64>
    transform3x3(const Matrix4x4<gp::Float64>& m, const Vector3<gp::Float64>& v) noexcept
{
    return Vector3<gp::Float64>(
        m.m00 * v.x + m.m01 * v.y + m.m02 * v.z,
        m.m10 * v.x + m.m11 * v.y + m.m12 * v.z,
        m.m20 * v.x + m.m21 * v.y + m.m22 * v.z
    );
}

/// @brief Inverts the upper-left `3x3` block of a `4x4` matrix (`m33 = 1.0`).
/// @param[in] m Source `4x4` matrix with zero translation/perspective rows.
/// @return Inverted `4x4` matrix, or identity if singular (`|det| <= 1e-18`).
[[nodiscard]] constexpr Matrix4x4<gp::Float64> invert3x3Block(const Matrix4x4<gp::Float64>& m) noexcept
{
    const gp::Float64 c00 = m.m11 * m.m22 - m.m12 * m.m21;
    const gp::Float64 c01 = m.m02 * m.m21 - m.m01 * m.m22;
    const gp::Float64 c02 = m.m01 * m.m12 - m.m02 * m.m11;

    const gp::Float64 c10 = m.m12 * m.m20 - m.m10 * m.m22;
    const gp::Float64 c11 = m.m00 * m.m22 - m.m02 * m.m20;
    const gp::Float64 c12 = m.m02 * m.m10 - m.m00 * m.m12;

    const gp::Float64 c20 = m.m10 * m.m21 - m.m11 * m.m20;
    const gp::Float64 c21 = m.m01 * m.m20 - m.m00 * m.m21;
    const gp::Float64 c22 = m.m00 * m.m11 - m.m01 * m.m10;

    const gp::Float64 det = m.m00 * c00 + m.m01 * c10 + m.m02 * c20;
    if (gp::math::abs(det) <= 1.0e-18)
    {
        return makeIdentity4x4();
    }

    const gp::Float64 invDet = 1.0 / det;
    Matrix4x4<gp::Float64> inv{};
    inv.m00 = c00 * invDet;
    inv.m01 = c01 * invDet;
    inv.m02 = c02 * invDet;

    inv.m10 = c10 * invDet;
    inv.m11 = c11 * invDet;
    inv.m12 = c12 * invDet;

    inv.m20 = c20 * invDet;
    inv.m21 = c21 * invDet;
    inv.m22 = c22 * invDet;

    inv.m33 = 1.0;
    return inv;
}

/// @brief Global working color space mutex and instance.
gp::thread::Mutex g_workingColorSpaceMutex{};

/// @brief Returns the mutable singleton storage for the working `ColorSpace`.
[[nodiscard]] ColorSpace& getMutableWorkingColorSpace() noexcept
{
    static ColorSpace s_working(ColorSpaceType::sRGB);
    return s_working;
}

}   // namespace

Chromaticities::Chromaticities(const ColorSpaceType spaceType) noexcept
{
    switch (spaceType)
    {
    case ColorSpaceType::sRGB:
        red = Vector2<gp::Float64>(0.64, 0.33);
        green = Vector2<gp::Float64>(0.30, 0.60);
        blue = Vector2<gp::Float64>(0.15, 0.06);
        white = getWhitePoint(WhitePoint::CIE1931_D65);
        break;

    case ColorSpaceType::Rec2020:
        red = Vector2<gp::Float64>(0.708, 0.292);
        green = Vector2<gp::Float64>(0.170, 0.797);
        blue = Vector2<gp::Float64>(0.131, 0.046);
        white = getWhitePoint(WhitePoint::CIE1931_D65);
        break;

    case ColorSpaceType::ACESAP0:
        red = Vector2<gp::Float64>(0.7347, 0.2653);
        green = Vector2<gp::Float64>(0.0000, 1.0000);
        blue = Vector2<gp::Float64>(0.0001, -0.0770);
        white = getWhitePoint(WhitePoint::ACES_D60);
        break;

    case ColorSpaceType::ACESAP1:
        red = Vector2<gp::Float64>(0.713, 0.293);
        green = Vector2<gp::Float64>(0.165, 0.830);
        blue = Vector2<gp::Float64>(0.128, 0.044);
        white = getWhitePoint(WhitePoint::ACES_D60);
        break;

    case ColorSpaceType::P3DCI:
        red = Vector2<gp::Float64>(0.680, 0.320);
        green = Vector2<gp::Float64>(0.265, 0.690);
        blue = Vector2<gp::Float64>(0.150, 0.060);
        white = getWhitePoint(WhitePoint::DCI_CalibrationWhite);
        break;

    case ColorSpaceType::P3D65:
        red = Vector2<gp::Float64>(0.680, 0.320);
        green = Vector2<gp::Float64>(0.265, 0.690);
        blue = Vector2<gp::Float64>(0.150, 0.060);
        white = getWhitePoint(WhitePoint::CIE1931_D65);
        break;

    case ColorSpaceType::REDWideGamut:
        red = Vector2<gp::Float64>(0.780308, 0.304253);
        green = Vector2<gp::Float64>(0.121595, 1.493994);
        blue = Vector2<gp::Float64>(0.095612, -0.084589);
        white = getWhitePoint(WhitePoint::CIE1931_D65);
        break;

    case ColorSpaceType::SonySGamut3:
        red = Vector2<gp::Float64>(0.730, 0.280);
        green = Vector2<gp::Float64>(0.140, 0.855);
        blue = Vector2<gp::Float64>(0.100, -0.050);
        white = getWhitePoint(WhitePoint::CIE1931_D65);
        break;

    case ColorSpaceType::SonySGamut3Cine:
        red = Vector2<gp::Float64>(0.766, 0.275);
        green = Vector2<gp::Float64>(0.225, 0.800);
        blue = Vector2<gp::Float64>(0.089, -0.087);
        white = getWhitePoint(WhitePoint::CIE1931_D65);
        break;

    case ColorSpaceType::AlexaWideGamut:
        red = Vector2<gp::Float64>(0.684, 0.313);
        green = Vector2<gp::Float64>(0.221, 0.848);
        blue = Vector2<gp::Float64>(0.0861, -0.1020);
        white = getWhitePoint(WhitePoint::CIE1931_D65);
        break;

    case ColorSpaceType::CanonCinemaGamut:
        red = Vector2<gp::Float64>(0.740, 0.270);
        green = Vector2<gp::Float64>(0.170, 1.140);
        blue = Vector2<gp::Float64>(0.080, -0.100);
        white = getWhitePoint(WhitePoint::CIE1931_D65);
        break;

    case ColorSpaceType::GoProProtuneNative:
        red = Vector2<gp::Float64>(0.698448, 0.193026);
        green = Vector2<gp::Float64>(0.329555, 1.024597);
        blue = Vector2<gp::Float64>(0.108443, -0.034679);
        white = getWhitePoint(WhitePoint::CIE1931_D65);
        break;

    case ColorSpaceType::PanasonicVGamut:
        red = Vector2<gp::Float64>(0.730, 0.280);
        green = Vector2<gp::Float64>(0.165, 0.840);
        blue = Vector2<gp::Float64>(0.100, -0.030);
        white = getWhitePoint(WhitePoint::CIE1931_D65);
        break;

    case ColorSpaceType::PLASA_E1_54:
        red = Vector2<gp::Float64>(0.7347, 0.2653);
        green = Vector2<gp::Float64>(0.1596, 0.8404);
        blue = Vector2<gp::Float64>(0.0366, 0.0001);
        white = Vector2<gp::Float64>(0.4254, 0.4044);
        break;

    case ColorSpaceType::None:
    case ColorSpaceType::Count:
        // falls through
    default:
        red = Vector2<gp::Float64>::zero();
        green = Vector2<gp::Float64>::zero();
        blue = Vector2<gp::Float64>::zero();
        white = Vector2<gp::Float64>::zero();
        break;
    }
}

const ColorSpace& ColorSpace::getWorking() noexcept
{
    return getMutableWorkingColorSpace();
}

void ColorSpace::setWorking(const ColorSpace& workingColorSpace) noexcept
{
    g_workingColorSpaceMutex.lock();
    getMutableWorkingColorSpace() = workingColorSpace;
    g_workingColorSpaceMutex.unlock();
}

ColorSpace::ColorSpace() noexcept
    : ColorSpace(ColorSpaceType::sRGB)
{}

ColorSpace::ColorSpace(const ColorSpaceType spaceType) noexcept
    : m_chromaticities(spaceType)
    , m_type(spaceType)
    , m_isSRGB(spaceType == ColorSpaceType::sRGB)
{
    initializeMatrices();
}

ColorSpace::ColorSpace(
    const Vector2<gp::Float64>& red,
    const Vector2<gp::Float64>& green,
    const Vector2<gp::Float64>& blue,
    const Vector2<gp::Float64>& white
) noexcept
    : m_chromaticities(red, green, blue, white)
    , m_type(ColorSpaceType::None)
    , m_isSRGB(false)
{
    const Chromaticities srgbChroma(ColorSpaceType::sRGB);
    m_isSRGB = m_chromaticities.equals(srgbChroma, 1.0e-7);
    if (m_isSRGB)
    {
        m_type = ColorSpaceType::sRGB;
    }
    initializeMatrices();
}

ColorSpace::ColorSpace(const Chromaticities& chromaticities) noexcept
    : ColorSpace(chromaticities.red, chromaticities.green, chromaticities.blue, chromaticities.white)
{}

bool ColorSpace::equals(const ColorSpace& other, const gp::Float64 tolerance) const noexcept
{
    return m_chromaticities.equals(other.m_chromaticities, tolerance);
}

bool ColorSpace::equals(const Chromaticities& chromaticities, const gp::Float64 tolerance) const noexcept
{
    return m_chromaticities.equals(chromaticities, tolerance);
}

Vector2<gp::Float64>
    ColorSpace::makeChromaticitiesFromTemperature(const gp::Float32 temp, const gp::Float32 tint) noexcept
{
    const gp::Float64 clampedTemp = static_cast<gp::Float64>(gp::math::clamp(temp, 1000.0f, 15000.0f));
    const gp::Float64 clampedTint = static_cast<gp::Float64>(tint);

    // Krystek (1985) rational approximation of Planckian locus in CIE 1960 UCS (u, v)
    const gp::Float64 t2 = clampedTemp * clampedTemp;
    const gp::Float64 u0 = (0.860117757 + 1.54118254e-4 * clampedTemp + 1.28641212e-7 * t2) /
                           (1.0 + 8.42420235e-4 * clampedTemp + 7.08145163e-7 * t2);
    const gp::Float64 v0 = (0.317398726 + 4.22806245e-5 * clampedTemp + 4.20481691e-8 * t2) /
                           (1.0 - 2.89741816e-5 * clampedTemp + 1.61456053e-7 * t2);

    gp::Float64 u = u0;
    gp::Float64 v = v0;

    if (gp::math::abs(clampedTint) > 1.0e-9)
    {
        // Derivative along the Planckian locus via central finite difference for perpendicular isotherm offset
        constexpr gp::Float64 kDeltaT = 10.0;
        const gp::Float64 tp = clampedTemp + kDeltaT;
        const gp::Float64 tm = clampedTemp - kDeltaT;

        const gp::Float64 up = (0.860117757 + 1.54118254e-4 * tp + 1.28641212e-7 * tp * tp) /
                               (1.0 + 8.42420235e-4 * tp + 7.08145163e-7 * tp * tp);
        const gp::Float64 vp = (0.317398726 + 4.22806245e-5 * tp + 4.20481691e-8 * tp * tp) /
                               (1.0 - 2.89741816e-5 * tp + 1.61456053e-7 * tp * tp);

        const gp::Float64 um = (0.860117757 + 1.54118254e-4 * tm + 1.28641212e-7 * tm * tm) /
                               (1.0 + 8.42420235e-4 * tm + 7.08145163e-7 * tm * tm);
        const gp::Float64 vm = (0.317398726 + 4.22806245e-5 * tm + 4.20481691e-8 * tm * tm) /
                               (1.0 - 2.89741816e-5 * tm + 1.61456053e-7 * tm * tm);

        const gp::Float64 du = up - um;
        const gp::Float64 dv = vp - vm;
        const gp::Float64 len = std::sqrt(du * du + dv * dv);
        if (len > 1.0e-12)
        {
            // Standard Delta_uv scale (0.05 max excursion for tint in [-1, 1])
            constexpr gp::Float64 kTintScale = 0.05;
            u += (-dv / len) * clampedTint * kTintScale;
            v += (du / len) * clampedTint * kTintScale;
        }
    }

    const gp::Float64 denom = 2.0 * u - 8.0 * v + 4.0;
    if (gp::math::abs(denom) <= 1.0e-12)
    {
        return getWhitePoint(WhitePoint::CIE1931_D65);
    }

    const gp::Float64 x = (3.0 * u) / denom;
    const gp::Float64 y = (2.0 * v) / denom;
    return Vector2<gp::Float64>(x, y);
}

void ColorSpace::initializeMatrices() noexcept
{
    const Vector3<gp::Float64> rXyz = xyYToXYZ(m_chromaticities.red, 1.0);
    const Vector3<gp::Float64> gXyz = xyYToXYZ(m_chromaticities.green, 1.0);
    const Vector3<gp::Float64> bXyz = xyYToXYZ(m_chromaticities.blue, 1.0);
    const Vector3<gp::Float64> wXyz = xyYToXYZ(m_chromaticities.white, 1.0);

    if (wXyz.isZero())
    {
        m_rgbToXyz = makeIdentity4x4();
        m_xyzToRgb = makeIdentity4x4();
        return;
    }

    // Build unscaled primary matrix P where columns 0, 1, 2 are R_xyz, G_xyz, B_xyz
    Matrix4x4<gp::Float64> primaries{};
    primaries.m00 = rXyz.x;
    primaries.m10 = rXyz.y;
    primaries.m20 = rXyz.z;

    primaries.m01 = gXyz.x;
    primaries.m11 = gXyz.y;
    primaries.m21 = gXyz.z;

    primaries.m02 = bXyz.x;
    primaries.m12 = bXyz.y;
    primaries.m22 = bXyz.z;

    primaries.m33 = 1.0;

    // Solve for primary scale factors S = P^-1 * W_xyz
    const Matrix4x4<gp::Float64> invPrimaries = invert3x3Block(primaries);
    const Vector3<gp::Float64> scale = transform3x3(invPrimaries, wXyz);

    m_rgbToXyz = Matrix4x4<gp::Float64>{};
    m_rgbToXyz.m00 = primaries.m00 * scale.x;
    m_rgbToXyz.m10 = primaries.m10 * scale.x;
    m_rgbToXyz.m20 = primaries.m20 * scale.x;

    m_rgbToXyz.m01 = primaries.m01 * scale.y;
    m_rgbToXyz.m11 = primaries.m11 * scale.y;
    m_rgbToXyz.m21 = primaries.m21 * scale.y;

    m_rgbToXyz.m02 = primaries.m02 * scale.z;
    m_rgbToXyz.m12 = primaries.m12 * scale.z;
    m_rgbToXyz.m22 = primaries.m22 * scale.z;

    m_rgbToXyz.m33 = 1.0;

    m_xyzToRgb = invert3x3Block(m_rgbToXyz);
}

Matrix4x4<gp::Float64> ColorSpaceTransform::calcChromaticAdaptationMatrix(
    const Vector3<gp::Float64>& sourceXyz, const Vector3<gp::Float64>& destXyz, const ChromaticAdaptationMethod method
) noexcept
{
    if (sourceXyz.equals(destXyz, 1.0e-7) || sourceXyz.isZero() || destXyz.isZero())
    {
        return makeIdentity4x4();
    }

    if (method == ChromaticAdaptationMethod::None)
    {
        Matrix4x4<gp::Float64> scaleMat = makeIdentity4x4();
        scaleMat.m00 = (gp::math::abs(sourceXyz.x) > 1.0e-12) ? (destXyz.x / sourceXyz.x) : 1.0;
        scaleMat.m11 = (gp::math::abs(sourceXyz.y) > 1.0e-12) ? (destXyz.y / sourceXyz.y) : 1.0;
        scaleMat.m22 = (gp::math::abs(sourceXyz.z) > 1.0e-12) ? (destXyz.z / sourceXyz.z) : 1.0;
        return scaleMat;
    }

    Matrix4x4<gp::Float64> coneMatrix = makeIdentity4x4();
    if (method == ChromaticAdaptationMethod::Bradford)
    {
        coneMatrix.m00 = 0.8951;
        coneMatrix.m01 = 0.2664;
        coneMatrix.m02 = -0.1614;

        coneMatrix.m10 = -0.7502;
        coneMatrix.m11 = 1.7135;
        coneMatrix.m12 = 0.0367;

        coneMatrix.m20 = 0.0389;
        coneMatrix.m21 = -0.0685;
        coneMatrix.m22 = 1.0296;
    }
    else if (method == ChromaticAdaptationMethod::CAT02)
    {
        coneMatrix.m00 = 0.7328;
        coneMatrix.m01 = 0.4296;
        coneMatrix.m02 = -0.1624;

        coneMatrix.m10 = -0.7036;
        coneMatrix.m11 = 1.6975;
        coneMatrix.m12 = 0.0061;

        coneMatrix.m20 = 0.0030;
        coneMatrix.m21 = 0.0136;
        coneMatrix.m22 = 0.9834;
    }
    else
    {
        return makeIdentity4x4();
    }

    const Vector3<gp::Float64> srcLms = transform3x3(coneMatrix, sourceXyz);
    const Vector3<gp::Float64> dstLms = transform3x3(coneMatrix, destXyz);

    Matrix4x4<gp::Float64> lmsScale = makeIdentity4x4();
    lmsScale.m00 = (gp::math::abs(srcLms.x) > 1.0e-12) ? (dstLms.x / srcLms.x) : 1.0;
    lmsScale.m11 = (gp::math::abs(srcLms.y) > 1.0e-12) ? (dstLms.y / srcLms.y) : 1.0;
    lmsScale.m22 = (gp::math::abs(srcLms.z) > 1.0e-12) ? (dstLms.z / srcLms.z) : 1.0;

    const Matrix4x4<gp::Float64> invConeMatrix = invert3x3Block(coneMatrix);
    return multiply4x4(invConeMatrix, multiply4x4(lmsScale, coneMatrix));
}

ColorSpaceTransform::ColorSpaceTransform(
    const ColorSpace& source, const ColorSpace& destination, const ChromaticAdaptationMethod method
) noexcept
{
    if (source.equals(destination, 1.0e-7))
    {
        m_matrix = makeIdentity4x4();
        return;
    }

    const Vector3<gp::Float64> srcWhiteXyz = xyYToXYZ(source.getChromaticities().white, 1.0);
    const Vector3<gp::Float64> dstWhiteXyz = xyYToXYZ(destination.getChromaticities().white, 1.0);
    const Matrix4x4<gp::Float64> cat = calcChromaticAdaptationMatrix(srcWhiteXyz, dstWhiteXyz, method);

    // M = Dst_XyzToRgb * CAT * Src_RgbToXyz
    m_matrix = multiply4x4(destination.getXyzToRgb(), multiply4x4(cat, source.getRgbToXyz()));
}

ColorSpaceTransform::ColorSpaceTransform(const Matrix4x4<gp::Float64>& matrix) noexcept
    : m_matrix(matrix)
{}

LinearColor ColorSpaceTransform::apply(const LinearColor& color) const noexcept
{
    const gp::Float64 r = static_cast<gp::Float64>(color.r);
    const gp::Float64 g = static_cast<gp::Float64>(color.g);
    const gp::Float64 b = static_cast<gp::Float64>(color.b);

    const gp::Float32 outR =
        static_cast<gp::Float32>(m_matrix.m00 * r + m_matrix.m01 * g + m_matrix.m02 * b + m_matrix.m03);
    const gp::Float32 outG =
        static_cast<gp::Float32>(m_matrix.m10 * r + m_matrix.m11 * g + m_matrix.m12 * b + m_matrix.m13);
    const gp::Float32 outB =
        static_cast<gp::Float32>(m_matrix.m20 * r + m_matrix.m21 * g + m_matrix.m22 * b + m_matrix.m23);

    return LinearColor(outR, outG, outB, color.a);
}

}   // namespace gp::math
