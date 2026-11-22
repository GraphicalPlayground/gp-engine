// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#pragma once

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "maths/color/Color.hpp"
#include "maths/color/ColorForward.hpp"   // IWYU pragma: keep
#include "maths/color/LinearColor.hpp"

namespace gp::math
{

/// @brief Zero-allocation catalog of 99 standard named 8-bit sRGB colors with case-insensitive name and index lookup.
/// @details Mirrors Unreal Engine's `FColorList` (`GColorList`) while replacing heap-allocated `TMap<FString, ...>`
///          with an immutable `constexpr` table in `.rdata` so queries work in `constexpr` contexts and before or
///          without calling `createColorMap()`.
/// @note Preserves the exact 99-entry index assignment (`0..98`) of Unreal Engine's `FColorList::CreateColorMap()`.
class ColorList final
{
public:
    /// @brief Total number of named colors in the catalog (`99`, indices `0..98`, with `"black"` at index `0`).
    static constexpr gp::Int32 kColorsCount = 99;

    /// @brief Sentinel name returned when an out-of-range index is passed to `getColorNameByIndex()`.
    static constexpr gp::StringView kBadIndexName = "BadIndex";

public:
    /// @section Static Named Colors

    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Magenta;
    static const Color Cyan;
    static const Color Yellow;
    static const Color Black;
    static const Color Aquamarine;
    static const Color BakerChocolate;
    static const Color BlueViolet;
    static const Color Brass;
    static const Color BrightGold;
    static const Color Brown;
    static const Color Bronze;
    static const Color BronzeII;
    static const Color CadetBlue;
    static const Color CoolCopper;
    static const Color Copper;
    static const Color Coral;
    static const Color CornFlowerBlue;
    static const Color DarkBrown;
    static const Color DarkGreen;
    static const Color DarkGreenCopper;
    static const Color DarkOliveGreen;
    static const Color DarkOrchid;
    static const Color DarkPurple;
    static const Color DarkSlateBlue;
    static const Color DarkSlateGrey;
    static const Color DarkTan;
    static const Color DarkTurquoise;
    static const Color DarkWood;
    static const Color DimGrey;
    static const Color DustyRose;
    static const Color Feldspar;
    static const Color Firebrick;
    static const Color ForestGreen;
    static const Color Gold;
    static const Color Goldenrod;
    static const Color Grey;
    static const Color GreenCopper;
    static const Color GreenYellow;
    static const Color HunterGreen;
    static const Color IndianRed;
    static const Color Khaki;
    static const Color LightBlue;
    static const Color LightGrey;
    static const Color LightSteelBlue;
    static const Color LightWood;
    static const Color LimeGreen;
    static const Color MandarianOrange;
    static const Color Maroon;
    static const Color MediumAquamarine;
    static const Color MediumBlue;
    static const Color MediumForestGreen;
    static const Color MediumGoldenrod;
    static const Color MediumOrchid;
    static const Color MediumSeaGreen;
    static const Color MediumSlateBlue;
    static const Color MediumSpringGreen;
    static const Color MediumTurquoise;
    static const Color MediumVioletRed;
    static const Color MediumWood;
    static const Color MidnightBlue;
    static const Color NavyBlue;
    static const Color NeonBlue;
    static const Color NeonPink;
    static const Color NewMidnightBlue;
    static const Color NewTan;
    static const Color OldGold;
    static const Color Orange;
    static const Color OrangeRed;
    static const Color Orchid;
    static const Color PaleGreen;
    static const Color Pink;
    static const Color Plum;
    static const Color Quartz;
    static const Color RichBlue;
    static const Color Salmon;
    static const Color Scarlet;
    static const Color SeaGreen;
    static const Color SemiSweetChocolate;
    static const Color Sienna;
    static const Color Silver;
    static const Color SkyBlue;
    static const Color SlateBlue;
    static const Color SpicyPink;
    static const Color SpringGreen;
    static const Color SteelBlue;
    static const Color SummerSky;
    static const Color Tan;
    static const Color Thistle;
    static const Color Turquoise;
    static const Color VeryDarkBrown;
    static const Color VeryLightGrey;
    static const Color Violet;
    static const Color VioletRed;
    static const Color Wheat;
    static const Color YellowGreen;

public:
    /// @section Lifecycle & Queries

    /// @brief Default constructor. The static catalog is pre-populated and ready immediately.
    [[nodiscard]] constexpr ColorList() noexcept = default;

    /// @brief Initializes the color map (retained for Unreal `FColorList::CreateColorMap()` parity; idempotent).
    GP_CORE_API void createColorMap() noexcept;

    /// @brief Reports whether the color catalog is initialized and ready for queries.
    /// @return Always `true` (backed by static `.rdata` table).
    [[nodiscard]] constexpr bool isInitialized() const noexcept
    {
        return m_isInitialized;
    }

    /// @brief Returns the number of colors in the catalog (`99`).
    /// @return `kColorsCount` (`99`).
    [[nodiscard]] constexpr gp::Int32 getColorsNum() const noexcept
    {
        return kColorsCount;
    }

    /// @brief Checks whether `colorIndex` is within `[0, getColorsNum())`.
    /// @param[in] colorIndex Zero-based index to test.
    /// @return True if `0 <= colorIndex < kColorsCount`.
    [[nodiscard]] constexpr bool isValidColorIndex(const gp::Int32 colorIndex) const noexcept
    {
        return colorIndex >= 0 && colorIndex < kColorsCount;
    }

    /// @brief Looks up an 8-bit `Color` by its case-insensitive name (e.g., `"aquamarine"`).
    /// @param[in] colorName Case-insensitive ASCII color name.
    /// @return Const reference to the matching `Color`, or `ColorList::White` if not found.
    [[nodiscard]] GP_CORE_API const Color& getColorByName(gp::StringView colorName) const noexcept;

    /// @brief Alias for `getColorByName()` matching Unreal `GetFColorByName()`.
    /// @param[in] colorName Case-insensitive ASCII color name.
    /// @return Const reference to the matching `Color`, or `ColorList::White` if not found.
    [[nodiscard]] const Color& getFColorByName(const gp::StringView colorName) const noexcept
    {
        return getColorByName(colorName);
    }

    /// @brief Looks up a `LinearColor` (sRGB-decoded) by its case-insensitive name.
    /// @param[in] colorName Case-insensitive ASCII color name.
    /// @return Decoded `LinearColor`, or `LinearColor::kWhite` if not found.
    [[nodiscard]] GP_CORE_API LinearColor getLinearColorByName(gp::StringView colorName) const noexcept;

    /// @brief Alias for `getLinearColorByName()` matching Unreal `GetFLinearColorByName()`.
    /// @param[in] colorName Case-insensitive ASCII color name.
    /// @return Decoded `LinearColor`, or `LinearColor::kWhite` if not found.
    [[nodiscard]] LinearColor getFLinearColorByName(const gp::StringView colorName) const noexcept
    {
        return getLinearColorByName(colorName);
    }

    /// @brief Checks whether `colorName` is a valid color name in the catalog (case-insensitive).
    /// @param[in] colorName Candidate color name.
    /// @return True if found in the catalog, false otherwise.
    [[nodiscard]] GP_CORE_API bool isValidColorName(gp::StringView colorName) const noexcept;

    /// @brief Returns the zero-based catalog index of `colorName` (case-insensitive), or `0` if not found.
    /// @note Index `0` is `"black"` and indices `1..98` are `"aquamarine"` through `"yellowgreen"`. Unknown names
    ///       return `0` matching Unreal's `FColorList::GetColorIndex()`.
    /// @param[in] colorName Candidate color name.
    /// @return Zero-based index in `[0, 98]`, or `0` if not found.
    [[nodiscard]] GP_CORE_API gp::Int32 getColorIndex(gp::StringView colorName) const noexcept;

    /// @brief Looks up an 8-bit `Color` by its zero-based index in `[0, 98]`.
    /// @param[in] colorIndex Zero-based catalog index.
    /// @return Const reference to the color at `colorIndex`, or `ColorList::White` if out of bounds.
    [[nodiscard]] GP_CORE_API const Color& getColorByIndex(gp::Int32 colorIndex) const noexcept;

    /// @brief Alias for `getColorByIndex()` matching Unreal `GetFColorByIndex()`.
    /// @param[in] colorIndex Zero-based catalog index.
    /// @return Const reference to the color at `colorIndex`, or `ColorList::White` if out of bounds.
    [[nodiscard]] const Color& getFColorByIndex(const gp::Int32 colorIndex) const noexcept
    {
        return getColorByIndex(colorIndex);
    }

    /// @brief Returns the lowercase canonical name of the color at `colorIndex`.
    /// @param[in] colorIndex Zero-based catalog index in `[0, 98]`.
    /// @return Canonical color name view, or `"BadIndex"` if `colorIndex` is out of bounds.
    [[nodiscard]] GP_CORE_API gp::StringView getColorNameByIndex(gp::Int32 colorIndex) const noexcept;

    /// @brief Logs all 99 catalog colors and their indices via `GP_LOG_INFO`.
    GP_CORE_API void logColors() const noexcept;

private:
    bool m_isInitialized{ true };
};

inline constexpr Color ColorList::White(255u, 255u, 255u, 255u);
inline constexpr Color ColorList::Red(255u, 0u, 0u, 255u);
inline constexpr Color ColorList::Green(0u, 255u, 0u, 255u);
inline constexpr Color ColorList::Blue(0u, 0u, 255u, 255u);
inline constexpr Color ColorList::Magenta(255u, 0u, 255u, 255u);
inline constexpr Color ColorList::Cyan(0u, 255u, 255u, 255u);
inline constexpr Color ColorList::Yellow(255u, 255u, 0u, 255u);
inline constexpr Color ColorList::Black(0u, 0u, 0u, 255u);
inline constexpr Color ColorList::Aquamarine(112u, 219u, 147u, 255u);
inline constexpr Color ColorList::BakerChocolate(92u, 51u, 23u, 255u);
inline constexpr Color ColorList::BlueViolet(159u, 95u, 159u, 255u);
inline constexpr Color ColorList::Brass(181u, 166u, 66u, 255u);
inline constexpr Color ColorList::BrightGold(217u, 217u, 25u, 255u);
inline constexpr Color ColorList::Brown(166u, 42u, 42u, 255u);
inline constexpr Color ColorList::Bronze(140u, 120u, 83u, 255u);
inline constexpr Color ColorList::BronzeII(166u, 125u, 61u, 255u);
inline constexpr Color ColorList::CadetBlue(95u, 159u, 159u, 255u);
inline constexpr Color ColorList::CoolCopper(217u, 135u, 25u, 255u);
inline constexpr Color ColorList::Copper(184u, 115u, 51u, 255u);
inline constexpr Color ColorList::Coral(255u, 127u, 0u, 255u);
inline constexpr Color ColorList::CornFlowerBlue(66u, 66u, 111u, 255u);
inline constexpr Color ColorList::DarkBrown(92u, 64u, 51u, 255u);
inline constexpr Color ColorList::DarkGreen(47u, 79u, 47u, 255u);
inline constexpr Color ColorList::DarkGreenCopper(74u, 118u, 110u, 255u);
inline constexpr Color ColorList::DarkOliveGreen(79u, 79u, 47u, 255u);
inline constexpr Color ColorList::DarkOrchid(153u, 50u, 205u, 255u);
inline constexpr Color ColorList::DarkPurple(135u, 31u, 120u, 255u);
inline constexpr Color ColorList::DarkSlateBlue(107u, 35u, 142u, 255u);
inline constexpr Color ColorList::DarkSlateGrey(47u, 79u, 79u, 255u);
inline constexpr Color ColorList::DarkTan(151u, 105u, 79u, 255u);
inline constexpr Color ColorList::DarkTurquoise(112u, 147u, 219u, 255u);
inline constexpr Color ColorList::DarkWood(133u, 94u, 66u, 255u);
inline constexpr Color ColorList::DimGrey(84u, 84u, 84u, 255u);
inline constexpr Color ColorList::DustyRose(133u, 99u, 99u, 255u);
inline constexpr Color ColorList::Feldspar(209u, 146u, 117u, 255u);
inline constexpr Color ColorList::Firebrick(142u, 35u, 35u, 255u);
inline constexpr Color ColorList::ForestGreen(35u, 142u, 35u, 255u);
inline constexpr Color ColorList::Gold(205u, 127u, 50u, 255u);
inline constexpr Color ColorList::Goldenrod(219u, 219u, 112u, 255u);
inline constexpr Color ColorList::Grey(192u, 192u, 192u, 255u);
inline constexpr Color ColorList::GreenCopper(82u, 127u, 118u, 255u);
inline constexpr Color ColorList::GreenYellow(147u, 219u, 112u, 255u);
inline constexpr Color ColorList::HunterGreen(33u, 94u, 33u, 255u);
inline constexpr Color ColorList::IndianRed(78u, 47u, 47u, 255u);
inline constexpr Color ColorList::Khaki(159u, 159u, 95u, 255u);
inline constexpr Color ColorList::LightBlue(192u, 217u, 217u, 255u);
inline constexpr Color ColorList::LightGrey(168u, 168u, 168u, 255u);
inline constexpr Color ColorList::LightSteelBlue(143u, 143u, 189u, 255u);
inline constexpr Color ColorList::LightWood(233u, 194u, 166u, 255u);
inline constexpr Color ColorList::LimeGreen(50u, 205u, 50u, 255u);
inline constexpr Color ColorList::MandarianOrange(228u, 120u, 51u, 255u);
inline constexpr Color ColorList::Maroon(142u, 35u, 107u, 255u);
inline constexpr Color ColorList::MediumAquamarine(50u, 205u, 153u, 255u);
inline constexpr Color ColorList::MediumBlue(50u, 50u, 205u, 255u);
inline constexpr Color ColorList::MediumForestGreen(107u, 142u, 35u, 255u);
inline constexpr Color ColorList::MediumGoldenrod(234u, 234u, 174u, 255u);
inline constexpr Color ColorList::MediumOrchid(147u, 112u, 219u, 255u);
inline constexpr Color ColorList::MediumSeaGreen(66u, 111u, 66u, 255u);
inline constexpr Color ColorList::MediumSlateBlue(127u, 0u, 255u, 255u);
inline constexpr Color ColorList::MediumSpringGreen(127u, 255u, 0u, 255u);
inline constexpr Color ColorList::MediumTurquoise(112u, 219u, 219u, 255u);
inline constexpr Color ColorList::MediumVioletRed(219u, 112u, 147u, 255u);
inline constexpr Color ColorList::MediumWood(166u, 128u, 100u, 255u);
inline constexpr Color ColorList::MidnightBlue(47u, 47u, 79u, 255u);
inline constexpr Color ColorList::NavyBlue(35u, 35u, 142u, 255u);
inline constexpr Color ColorList::NeonBlue(77u, 77u, 255u, 255u);
inline constexpr Color ColorList::NeonPink(255u, 110u, 199u, 255u);
inline constexpr Color ColorList::NewMidnightBlue(0u, 0u, 156u, 255u);
inline constexpr Color ColorList::NewTan(235u, 199u, 158u, 255u);
inline constexpr Color ColorList::OldGold(207u, 181u, 59u, 255u);
inline constexpr Color ColorList::Orange(255u, 127u, 0u, 255u);
inline constexpr Color ColorList::OrangeRed(255u, 36u, 0u, 255u);
inline constexpr Color ColorList::Orchid(219u, 112u, 219u, 255u);
inline constexpr Color ColorList::PaleGreen(143u, 188u, 143u, 255u);
inline constexpr Color ColorList::Pink(188u, 143u, 143u, 255u);
inline constexpr Color ColorList::Plum(234u, 173u, 234u, 255u);
inline constexpr Color ColorList::Quartz(217u, 217u, 243u, 255u);
inline constexpr Color ColorList::RichBlue(89u, 89u, 171u, 255u);
inline constexpr Color ColorList::Salmon(111u, 66u, 66u, 255u);
inline constexpr Color ColorList::Scarlet(140u, 23u, 23u, 255u);
inline constexpr Color ColorList::SeaGreen(35u, 142u, 104u, 255u);
inline constexpr Color ColorList::SemiSweetChocolate(107u, 66u, 38u, 255u);
inline constexpr Color ColorList::Sienna(142u, 107u, 35u, 255u);
inline constexpr Color ColorList::Silver(230u, 232u, 250u, 255u);
inline constexpr Color ColorList::SkyBlue(50u, 153u, 204u, 255u);
inline constexpr Color ColorList::SlateBlue(0u, 127u, 255u, 255u);
inline constexpr Color ColorList::SpicyPink(255u, 28u, 174u, 255u);
inline constexpr Color ColorList::SpringGreen(0u, 255u, 127u, 255u);
inline constexpr Color ColorList::SteelBlue(35u, 107u, 142u, 255u);
inline constexpr Color ColorList::SummerSky(56u, 176u, 222u, 255u);
inline constexpr Color ColorList::Tan(219u, 147u, 112u, 255u);
inline constexpr Color ColorList::Thistle(216u, 191u, 216u, 255u);
inline constexpr Color ColorList::Turquoise(173u, 234u, 234u, 255u);
inline constexpr Color ColorList::VeryDarkBrown(92u, 64u, 51u, 255u);
inline constexpr Color ColorList::VeryLightGrey(205u, 205u, 205u, 255u);
inline constexpr Color ColorList::Violet(79u, 47u, 79u, 255u);
inline constexpr Color ColorList::VioletRed(204u, 50u, 153u, 255u);
inline constexpr Color ColorList::Wheat(216u, 216u, 191u, 255u);
inline constexpr Color ColorList::YellowGreen(153u, 204u, 50u, 255u);

/// @brief Global instance of the `ColorList` catalog.
extern GP_CORE_API ColorList g_colorList;

}   // namespace gp::math
