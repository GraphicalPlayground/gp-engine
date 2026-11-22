// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "CoreMinimal.hpp"
#include "maths/base/Arithmetic.hpp"
#include "maths/base/Constants.hpp"
#include "maths/base/FloatingPoint.hpp"   // IWYU pragma: keep
#include "maths/color/ColorEnums.hpp"
#include "maths/color/ColorForward.hpp"   // IWYU pragma: keep
#include "maths/color/LinearColor.hpp"
#include "maths/matrix/Matrix4x4.hpp"
#include "maths/vector/Vector2.hpp"
#include "maths/vector/Vector3.hpp"
#include <type_traits>

namespace gp::math
{

/// @section CIE 1931 xyY / XYZ Helpers & Standard White Points

/// @brief Returns the CIE 1931 2-degree standard observer `(x, y)` chromaticity coordinates for a `WhitePoint`.
/// @param[in] whitePoint Standard illuminant white point enumerator.
/// @return 2D chromaticity coordinates `(x, y)` in double precision.
[[nodiscard]] constexpr Vector2<gp::Float64> getWhitePoint(const WhitePoint whitePoint) noexcept
{
    switch (whitePoint)
    {
    case WhitePoint::CIE1931_D50:
        return Vector2<gp::Float64>(0.34567, 0.35850);
    case WhitePoint::CIE1931_D55:
        return Vector2<gp::Float64>(0.33242, 0.34743);
    case WhitePoint::ACES_D60:
        return Vector2<gp::Float64>(0.32168, 0.33767);
    case WhitePoint::DCI_CalibrationWhite:
        return Vector2<gp::Float64>(0.314, 0.351);
    case WhitePoint::CIE1931_D65:
        return Vector2<gp::Float64>(0.3127, 0.3290);
    case WhitePoint::CIE1931_D75:
        return Vector2<gp::Float64>(0.29902, 0.31485);
    case WhitePoint::CIE1931_E:
        return Vector2<gp::Float64>(1.0 / 3.0, 1.0 / 3.0);
    case WhitePoint::Count:
        // falls through
    default:
        return Vector2<gp::Float64>(0.3127, 0.3290);
    }
}

/// @brief Converts CIE 1931 `(x, y)` chromaticity and luminance `Y` into CIE 1931 `(X, Y, Z)` tristimulus coordinates.
/// @param[in] xy Chromaticity coordinates `(x, y)`.
/// @param[in] luminanceY Relative luminance `Y` (defaults to `1.0`).
/// @return CIE XYZ tristimulus vector `((x * Y) / y, Y, ((1 - x - y) * Y) / y)`, or `(0, 0, 0)` when `|y| <= 1e-12`.
[[nodiscard]] constexpr Vector3<gp::Float64>
    xyYToXYZ(const Vector2<gp::Float64>& xy, const gp::Float64 luminanceY = 1.0) noexcept
{
    if (gp::math::abs(xy.y) <= 1.0e-12)
    {
        return Vector3<gp::Float64>::zero();
    }
    const gp::Float64 scale = luminanceY / xy.y;
    return Vector3<gp::Float64>(xy.x * scale, luminanceY, (1.0 - xy.x - xy.y) * scale);
}

/// @brief Converts CIE 1931 `(X, Y, Z)` tristimulus coordinates into `(x, y, Y)` chromaticity and luminance.
/// @param[in] xyz CIE XYZ tristimulus vector.
/// @return `Vector3<Float64>(x, y, Y)`, or `(0, 0, 0)` when `|X + Y + Z| <= 1e-12`.
[[nodiscard]] constexpr Vector3<gp::Float64> xyzToxyY(const Vector3<gp::Float64>& xyz) noexcept
{
    const gp::Float64 sum = xyz.x + xyz.y + xyz.z;
    if (gp::math::abs(sum) <= 1.0e-12)
    {
        return Vector3<gp::Float64>::zero();
    }
    const gp::Float64 invSum = 1.0 / sum;
    return Vector3<gp::Float64>(xyz.x * invSum, xyz.y * invSum, xyz.y);
}

/// @brief CIE 1931 `(x, y)` chromaticity coordinates for the Red, Green, and Blue primaries plus reference White Point.
struct Chromaticities final
{
public:
    /// @brief Red primary `(x, y)` chromaticity.
    Vector2<gp::Float64> red{ 0.64, 0.33 };

    /// @brief Green primary `(x, y)` chromaticity.
    Vector2<gp::Float64> green{ 0.30, 0.60 };

    /// @brief Blue primary `(x, y)` chromaticity.
    Vector2<gp::Float64> blue{ 0.15, 0.06 };

    /// @brief Reference white point `(x, y)` chromaticity.
    Vector2<gp::Float64> white{ 0.3127, 0.3290 };

public:
    /// @brief Default-constructs sRGB / Rec.709 chromaticities.
    [[nodiscard]] constexpr Chromaticities() noexcept = default;

    /// @brief Constructs chromaticities from explicit Red, Green, Blue, and White `(x, y)` coordinates.
    /// @param[in] inRed Red primary `(x, y)`.
    /// @param[in] inGreen Green primary `(x, y)`.
    /// @param[in] inBlue Blue primary `(x, y)`.
    /// @param[in] inWhite Reference white point `(x, y)`.
    [[nodiscard]] constexpr Chromaticities(
        const Vector2<gp::Float64>& inRed,
        const Vector2<gp::Float64>& inGreen,
        const Vector2<gp::Float64>& inBlue,
        const Vector2<gp::Float64>& inWhite
    ) noexcept
        : red(inRed)
        , green(inGreen)
        , blue(inBlue)
        , white(inWhite)
    {}

    /// @brief Constructs the standard CIE 1931 chromaticities for a catalog `ColorSpaceType`.
    /// @param[in] spaceType Standard color space enumerator.
    [[nodiscard]] GP_CORE_API explicit Chromaticities(ColorSpaceType spaceType) noexcept;

    /// @brief Exact equality comparison across all four `(x, y)` pairs.
    /// @param[in] other Chromaticities to compare against.
    /// @return True if all four coordinate pairs match exactly.
    [[nodiscard]] constexpr bool operator==(const Chromaticities& other) const noexcept
    {
        return red == other.red && green == other.green && blue == other.blue && white == other.white;
    }

    /// @brief Exact inequality comparison.
    /// @param[in] other Chromaticities to compare against.
    /// @return True if any coordinate pair differs.
    [[nodiscard]] constexpr bool operator!=(const Chromaticities& other) const noexcept
    {
        return !(*this == other);
    }

    /// @brief Tolerance-based comparison across all four chromaticity coordinate pairs.
    /// @param[in] other Chromaticities to compare against.
    /// @param[in] tolerance Maximum absolute deviation per coordinate.
    /// @return True if all primaries and the white point match within @p tolerance.
    [[nodiscard]] constexpr bool equals(
        const Chromaticities& other, const gp::Float64 tolerance = Constants<gp::Float64>::kindaSmallNumber
    ) const noexcept
    {
        return red.equals(other.red, tolerance) && green.equals(other.green, tolerance) &&
               blue.equals(other.blue, tolerance) && white.equals(other.white, tolerance);
    }
};

/// @brief CIE 1931 RGB Color Space defined by 3 primary chromaticities, a reference white point, and cached `4x4`
///        RGB <-> XYZ conversion matrices.
/// @details Mirrors Unreal Engine's `UE::Color::FColorSpace`. Maintains a global working color space (defaulting to
///          `sRGB`) queried by `LinearColor::getLuminance()`.
class GP_CORE_API ColorSpace final
{
public:
    /// @section Global Working Color Space

    /// @brief Returns the active engine working color space (defaults to `ColorSpaceType::sRGB`).
    /// @return Const reference to the global working `ColorSpace`.
    [[nodiscard]] static const ColorSpace& getWorking() noexcept;

    /// @brief Sets the active engine working color space.
    /// @param[in] workingColorSpace New working color space.
    static void setWorking(const ColorSpace& workingColorSpace) noexcept;

public:
    /// @section Constructors

    /// @brief Default constructor. Initializes to `ColorSpaceType::sRGB`.
    [[nodiscard]] ColorSpace() noexcept;

    /// @brief Constructs a standard color space from a `ColorSpaceType` catalog enumerator.
    /// @param[in] spaceType Standard color space identifier.
    [[nodiscard]] explicit ColorSpace(ColorSpaceType spaceType) noexcept;

    /// @brief Constructs a custom color space from explicit Red, Green, Blue, and White `(x, y)` chromaticities.
    /// @param[in] red Red primary `(x, y)`.
    /// @param[in] green Green primary `(x, y)`.
    /// @param[in] blue Blue primary `(x, y)`.
    /// @param[in] white Reference white point `(x, y)`.
    [[nodiscard]] ColorSpace(
        const Vector2<gp::Float64>& red,
        const Vector2<gp::Float64>& green,
        const Vector2<gp::Float64>& blue,
        const Vector2<gp::Float64>& white
    ) noexcept;

    /// @brief Constructs a color space from a `Chromaticities` descriptor.
    /// @param[in] chromaticities Primary and white point coordinates.
    [[nodiscard]] explicit ColorSpace(const Chromaticities& chromaticities) noexcept;

public:
    /// @section Queries & Accessors

    /// @brief Returns the `Chromaticities` (Red, Green, Blue, White `(x, y)`) of this color space.
    /// @return Const reference to `m_chromaticities`.
    [[nodiscard]] const Chromaticities& getChromaticities() const noexcept
    {
        return m_chromaticities;
    }

    /// @brief Outputs the four `(x, y)` chromaticity vectors (`outRed`, `outGreen`, `outBlue`, `outWhite`).
    /// @param[out] outRed Receives the Red primary `(x, y)`.
    /// @param[out] outGreen Receives the Green primary `(x, y)`.
    /// @param[out] outBlue Receives the Blue primary `(x, y)`.
    /// @param[out] outWhite Receives the White point `(x, y)`.
    void getChromaticities(
        Vector2<gp::Float64>& outRed,
        Vector2<gp::Float64>& outGreen,
        Vector2<gp::Float64>& outBlue,
        Vector2<gp::Float64>& outWhite
    ) const noexcept
    {
        outRed = m_chromaticities.red;
        outGreen = m_chromaticities.green;
        outBlue = m_chromaticities.blue;
        outWhite = m_chromaticities.white;
    }

    /// @brief Returns the `4x4` RGB-to-XYZ transformation matrix (column-vector convention: `XYZ = M * RGB`).
    /// @return Const reference to the `4x4` double-precision RGB-to-XYZ matrix.
    [[nodiscard]] const Matrix4x4<gp::Float64>& getRgbToXyz() const noexcept
    {
        return m_rgbToXyz;
    }

    /// @brief Returns the `4x4` XYZ-to-RGB inverse transformation matrix (column-vector convention: `RGB = M * XYZ`).
    /// @return Const reference to the `4x4` double-precision XYZ-to-RGB matrix.
    [[nodiscard]] const Matrix4x4<gp::Float64>& getXyzToRgb() const noexcept
    {
        return m_xyzToRgb;
    }

    /// @brief Returns the CIE `Y` relative luminance weights `(m10, m11, m12, 1.0f)` for this color space.
    /// @return `LinearColor` holding the red, green, and blue luminance factors.
    [[nodiscard]] LinearColor getLuminanceFactors() const noexcept
    {
        return LinearColor(
            static_cast<gp::Float32>(m_rgbToXyz.m10),
            static_cast<gp::Float32>(m_rgbToXyz.m11),
            static_cast<gp::Float32>(m_rgbToXyz.m12),
            1.0f
        );
    }

    /// @brief Returns the catalog `ColorSpaceType` if constructed from a standard preset (`None` for custom spaces).
    /// @return `ColorSpaceType`.
    [[nodiscard]] ColorSpaceType getType() const noexcept
    {
        return m_type;
    }

    /// @brief Checks whether this color space is standard sRGB / Rec.709.
    /// @return True if `m_isSRGB` holds.
    [[nodiscard]] bool isSRGB() const noexcept
    {
        return m_isSRGB;
    }

    /// @brief Exact equality comparison of chromaticities.
    /// @param[in] other Color space to compare against.
    /// @return True if chromaticities match exactly.
    [[nodiscard]] bool operator==(const ColorSpace& other) const noexcept
    {
        return m_chromaticities == other.m_chromaticities;
    }

    /// @brief Exact inequality comparison.
    /// @param[in] other Color space to compare against.
    /// @return True if chromaticities differ.
    [[nodiscard]] bool operator!=(const ColorSpace& other) const noexcept
    {
        return m_chromaticities != other.m_chromaticities;
    }

    /// @brief Tolerance-based comparison of two color spaces' chromaticities.
    /// @param[in] other Color space to compare against.
    /// @param[in] tolerance Maximum absolute deviation per coordinate (defaults to `1e-7`).
    /// @return True if all primaries and white point coordinates match within @p tolerance.
    [[nodiscard]] bool equals(const ColorSpace& other, gp::Float64 tolerance = 1.0e-7) const noexcept;

    /// @brief Tolerance-based comparison against a `Chromaticities` struct.
    /// @param[in] chromaticities Chromaticities to compare against.
    /// @param[in] tolerance Maximum absolute deviation per coordinate (defaults to `1e-7`).
    /// @return True if all primaries and white point coordinates match within @p tolerance.
    [[nodiscard]] bool equals(const Chromaticities& chromaticities, gp::Float64 tolerance = 1.0e-7) const noexcept;

    /// @brief Converts a correlated color temperature (CCT) in Kelvins and tint offset into CIE 1931 `(x, y)` white
    ///        point coordinates.
    /// @details Uses the Planckian locus / CIE daylight locus approximation with perpendicular isotherm `plankianTint`
    ///          offset in CIE 1960 UCS `(u, v)`.
    /// @param[in] temp Correlated color temperature in Kelvins (`[1000, 15000]`).
    /// @param[in] tint Tint offset along the Planckian isotherm (`0.0f` = on locus, typical range `[-1, 1]`).
    /// @return CIE 1931 `(x, y)` chromaticity coordinates.
    [[nodiscard]] static Vector2<gp::Float64>
        makeChromaticitiesFromTemperature(gp::Float32 temp, gp::Float32 tint = 0.0f) noexcept;

private:
    void initializeMatrices() noexcept;

private:
    Chromaticities m_chromaticities{};
    Matrix4x4<gp::Float64> m_rgbToXyz{};
    Matrix4x4<gp::Float64> m_xyzToRgb{};
    ColorSpaceType m_type{ ColorSpaceType::sRGB };
    bool m_isSRGB{ true };
};

/// @brief Precomputed `4x4` linear RGB transformation between two `ColorSpace` instances with optional chromatic
///        adaptation (Bradford, CAT02, or XYZ scaling).
/// @details Mirrors Unreal Engine's `UE::Color::FColorSpaceTransform`.
class GP_CORE_API ColorSpaceTransform final
{
public:
    /// @brief Computes the `4x4` chromatic adaptation matrix mapping CIE XYZ from @p sourceXyz white to @p destXyz
    ///        white.
    /// @param[in] sourceXyz Source reference white point in CIE XYZ (`Y = 1.0`).
    /// @param[in] destXyz Destination reference white point in CIE XYZ (`Y = 1.0`).
    /// @param[in] method Chromatic adaptation method (`None`, `Bradford`, or `CAT02`).
    /// @return `4x4` double-precision chromatic adaptation matrix in CIE XYZ space.
    [[nodiscard]] static Matrix4x4<gp::Float64> calcChromaticAdaptationMatrix(
        const Vector3<gp::Float64>& sourceXyz,
        const Vector3<gp::Float64>& destXyz,
        ChromaticAdaptationMethod method = ChromaticAdaptationMethod::Bradford
    ) noexcept;

public:
    /// @brief Constructs a color-space transform from @p source to @p destination with chromatic adaptation @p method.
    /// @param[in] source Source RGB color space.
    /// @param[in] destination Destination RGB color space.
    /// @param[in] method Chromatic adaptation transform method (defaults to `ChromaticAdaptationMethod::Bradford`).
    [[nodiscard]] ColorSpaceTransform(
        const ColorSpace& source,
        const ColorSpace& destination,
        ChromaticAdaptationMethod method = ChromaticAdaptationMethod::Bradford
    ) noexcept;

    /// @brief Constructs a color-space transform directly from a precomputed `4x4` matrix.
    /// @param[in] matrix `4x4` transformation matrix (`RGB_dst = matrix * RGB_src`).
    [[nodiscard]] explicit ColorSpaceTransform(const Matrix4x4<gp::Float64>& matrix) noexcept;

public:
    /// @brief Returns the underlying `4x4` double-precision transformation matrix.
    /// @return Const reference to `m_matrix`.
    [[nodiscard]] const Matrix4x4<gp::Float64>& getMatrix() const noexcept
    {
        return m_matrix;
    }

    /// @brief Transforms a `LinearColor` from the source color space into the destination color space, preserving `a`.
    /// @param[in] color Input linear color in the source color space.
    /// @return Transformed `LinearColor` in the destination color space (`a == color.a`).
    [[nodiscard]] LinearColor apply(const LinearColor& color) const noexcept;

    /// @brief Transforms a 3D linear RGB vector from the source color space into the destination color space.
    /// @tparam InFloatT Floating-point component type (`gp::Float32` or `gp::Float64`).
    /// @param[in] rgb Input linear RGB vector.
    /// @return Transformed linear RGB vector.
    template <concepts::IsFloatingPoint InFloatT>
    [[nodiscard]] constexpr Vector3<InFloatT> apply(const Vector3<InFloatT>& rgb) const noexcept
    {
        const gp::Float64 r = static_cast<gp::Float64>(rgb.x);
        const gp::Float64 g = static_cast<gp::Float64>(rgb.y);
        const gp::Float64 b = static_cast<gp::Float64>(rgb.z);

        return Vector3<InFloatT>(
            static_cast<InFloatT>(m_matrix.m00 * r + m_matrix.m01 * g + m_matrix.m02 * b + m_matrix.m03),
            static_cast<InFloatT>(m_matrix.m10 * r + m_matrix.m11 * g + m_matrix.m12 * b + m_matrix.m13),
            static_cast<InFloatT>(m_matrix.m20 * r + m_matrix.m21 * g + m_matrix.m22 * b + m_matrix.m23)
        );
    }

private:
    Matrix4x4<gp::Float64> m_matrix{};
};

static_assert(std::is_trivially_copyable_v<Chromaticities>, "gp::math::Chromaticities must be trivially copyable.");
static_assert(std::is_standard_layout_v<Chromaticities>, "gp::math::Chromaticities must have standard layout.");
static_assert(std::is_trivially_copyable_v<ColorSpace>, "gp::math::ColorSpace must be trivially copyable.");
static_assert(std::is_standard_layout_v<ColorSpace>, "gp::math::ColorSpace must have standard layout.");
static_assert(
    std::is_trivially_copyable_v<ColorSpaceTransform>, "gp::math::ColorSpaceTransform must be trivially copyable."
);
static_assert(
    std::is_standard_layout_v<ColorSpaceTransform>, "gp::math::ColorSpaceTransform must have standard layout."
);

}   // namespace gp::math
