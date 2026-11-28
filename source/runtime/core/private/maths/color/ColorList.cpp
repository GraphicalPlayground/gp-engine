// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/color/ColorList.hpp"
#include "containers/arrays/Array.hpp"
#include "containers/strings/String.hpp"        // IWYU pragma: keep
#include "containers/strings/StringEnums.hpp"   // IWYU pragma: keep
#include "logging/LogCategory.hpp"
#include "logging/LogMacros.hpp"

namespace gp::math
{

GP_DEFINE_LOG_CATEGORY_INLINE(LogColorList, gp::log::LogLevel::Info, gp::log::LogLevel::Trace);

namespace
{

struct ColorCatalogEntry final
{
    gp::StringView name{};
    const Color* colorPtr{ nullptr };
};

// clang-format off
/// @brief Immutable `constexpr` lookup table matching the exact 99-entry order of Unreal's `FColorList::CreateColorMap`.
/// @details Entry `0` is `"black"`, and entries `1..98` are strictly sorted in ASCII lowercase order.
constexpr gp::Array<ColorCatalogEntry, static_cast<gp::USize>(ColorList::kColorsCount)> kColorCatalogTable{
    ColorCatalogEntry{ "black",              &ColorList::Black },
    ColorCatalogEntry{ "aquamarine",         &ColorList::Aquamarine },
    ColorCatalogEntry{ "bakerchocolate",     &ColorList::BakerChocolate },
    ColorCatalogEntry{ "blue",               &ColorList::Blue },
    ColorCatalogEntry{ "blueviolet",         &ColorList::BlueViolet },
    ColorCatalogEntry{ "brass",              &ColorList::Brass },
    ColorCatalogEntry{ "brightgold",         &ColorList::BrightGold },
    ColorCatalogEntry{ "brown",              &ColorList::Brown },
    ColorCatalogEntry{ "bronze",             &ColorList::Bronze },
    ColorCatalogEntry{ "bronzeii",           &ColorList::BronzeII },
    ColorCatalogEntry{ "cadetblue",          &ColorList::CadetBlue },
    ColorCatalogEntry{ "coolcopper",         &ColorList::CoolCopper },
    ColorCatalogEntry{ "copper",             &ColorList::Copper },
    ColorCatalogEntry{ "coral",              &ColorList::Coral },
    ColorCatalogEntry{ "cornflowerblue",     &ColorList::CornFlowerBlue },
    ColorCatalogEntry{ "cyan",               &ColorList::Cyan },
    ColorCatalogEntry{ "darkbrown",          &ColorList::DarkBrown },
    ColorCatalogEntry{ "darkgreen",          &ColorList::DarkGreen },
    ColorCatalogEntry{ "darkgreencopper",    &ColorList::DarkGreenCopper },
    ColorCatalogEntry{ "darkolivegreen",     &ColorList::DarkOliveGreen },
    ColorCatalogEntry{ "darkorchid",         &ColorList::DarkOrchid },
    ColorCatalogEntry{ "darkpurple",         &ColorList::DarkPurple },
    ColorCatalogEntry{ "darkslateblue",      &ColorList::DarkSlateBlue },
    ColorCatalogEntry{ "darkslategrey",      &ColorList::DarkSlateGrey },
    ColorCatalogEntry{ "darktan",            &ColorList::DarkTan },
    ColorCatalogEntry{ "darkturquoise",      &ColorList::DarkTurquoise },
    ColorCatalogEntry{ "darkwood",           &ColorList::DarkWood },
    ColorCatalogEntry{ "dimgrey",            &ColorList::DimGrey },
    ColorCatalogEntry{ "dustyrose",          &ColorList::DustyRose },
    ColorCatalogEntry{ "feldspar",           &ColorList::Feldspar },
    ColorCatalogEntry{ "firebrick",          &ColorList::Firebrick },
    ColorCatalogEntry{ "forestgreen",        &ColorList::ForestGreen },
    ColorCatalogEntry{ "gold",               &ColorList::Gold },
    ColorCatalogEntry{ "goldenrod",          &ColorList::Goldenrod },
    ColorCatalogEntry{ "green",              &ColorList::Green },
    ColorCatalogEntry{ "greencopper",        &ColorList::GreenCopper },
    ColorCatalogEntry{ "greenyellow",        &ColorList::GreenYellow },
    ColorCatalogEntry{ "grey",               &ColorList::Grey },
    ColorCatalogEntry{ "huntergreen",        &ColorList::HunterGreen },
    ColorCatalogEntry{ "indianred",          &ColorList::IndianRed },
    ColorCatalogEntry{ "khaki",              &ColorList::Khaki },
    ColorCatalogEntry{ "lightblue",          &ColorList::LightBlue },
    ColorCatalogEntry{ "lightgrey",          &ColorList::LightGrey },
    ColorCatalogEntry{ "lightsteelblue",     &ColorList::LightSteelBlue },
    ColorCatalogEntry{ "lightwood",          &ColorList::LightWood },
    ColorCatalogEntry{ "limegreen",          &ColorList::LimeGreen },
    ColorCatalogEntry{ "magenta",            &ColorList::Magenta },
    ColorCatalogEntry{ "mandarianorange",    &ColorList::MandarianOrange },
    ColorCatalogEntry{ "maroon",             &ColorList::Maroon },
    ColorCatalogEntry{ "mediumaquamarine",   &ColorList::MediumAquamarine },
    ColorCatalogEntry{ "mediumblue",         &ColorList::MediumBlue },
    ColorCatalogEntry{ "mediumforestgreen",  &ColorList::MediumForestGreen },
    ColorCatalogEntry{ "mediumgoldenrod",    &ColorList::MediumGoldenrod },
    ColorCatalogEntry{ "mediumorchid",       &ColorList::MediumOrchid },
    ColorCatalogEntry{ "mediumseagreen",     &ColorList::MediumSeaGreen },
    ColorCatalogEntry{ "mediumslateblue",    &ColorList::MediumSlateBlue },
    ColorCatalogEntry{ "mediumspringgreen",  &ColorList::MediumSpringGreen },
    ColorCatalogEntry{ "mediumturquoise",    &ColorList::MediumTurquoise },
    ColorCatalogEntry{ "mediumvioletred",    &ColorList::MediumVioletRed },
    ColorCatalogEntry{ "mediumwood",         &ColorList::MediumWood },
    ColorCatalogEntry{ "midnightblue",       &ColorList::MidnightBlue },
    ColorCatalogEntry{ "navyblue",           &ColorList::NavyBlue },
    ColorCatalogEntry{ "neonblue",           &ColorList::NeonBlue },
    ColorCatalogEntry{ "neonpink",           &ColorList::NeonPink },
    ColorCatalogEntry{ "newmidnightblue",    &ColorList::NewMidnightBlue },
    ColorCatalogEntry{ "newtan",             &ColorList::NewTan },
    ColorCatalogEntry{ "oldgold",            &ColorList::OldGold },
    ColorCatalogEntry{ "orange",             &ColorList::Orange },
    ColorCatalogEntry{ "orangered",          &ColorList::OrangeRed },
    ColorCatalogEntry{ "orchid",             &ColorList::Orchid },
    ColorCatalogEntry{ "palegreen",          &ColorList::PaleGreen },
    ColorCatalogEntry{ "pink",               &ColorList::Pink },
    ColorCatalogEntry{ "plum",               &ColorList::Plum },
    ColorCatalogEntry{ "quartz",             &ColorList::Quartz },
    ColorCatalogEntry{ "red",                &ColorList::Red },
    ColorCatalogEntry{ "richblue",           &ColorList::RichBlue },
    ColorCatalogEntry{ "salmon",             &ColorList::Salmon },
    ColorCatalogEntry{ "scarlet",            &ColorList::Scarlet },
    ColorCatalogEntry{ "seagreen",           &ColorList::SeaGreen },
    ColorCatalogEntry{ "semisweetchocolate", &ColorList::SemiSweetChocolate },
    ColorCatalogEntry{ "sienna",             &ColorList::Sienna },
    ColorCatalogEntry{ "silver",             &ColorList::Silver },
    ColorCatalogEntry{ "skyblue",            &ColorList::SkyBlue },
    ColorCatalogEntry{ "slateblue",          &ColorList::SlateBlue },
    ColorCatalogEntry{ "spicypink",          &ColorList::SpicyPink },
    ColorCatalogEntry{ "springgreen",        &ColorList::SpringGreen },
    ColorCatalogEntry{ "steelblue",          &ColorList::SteelBlue },
    ColorCatalogEntry{ "summersky",          &ColorList::SummerSky },
    ColorCatalogEntry{ "tan",                &ColorList::Tan },
    ColorCatalogEntry{ "thistle",            &ColorList::Thistle },
    ColorCatalogEntry{ "turquoise",          &ColorList::Turquoise },
    ColorCatalogEntry{ "verydarkbrown",      &ColorList::VeryDarkBrown },
    ColorCatalogEntry{ "verylightgrey",      &ColorList::VeryLightGrey },
    ColorCatalogEntry{ "violet",             &ColorList::Violet },
    ColorCatalogEntry{ "violetred",          &ColorList::VioletRed },
    ColorCatalogEntry{ "wheat",              &ColorList::Wheat },
    ColorCatalogEntry{ "white",              &ColorList::White },
    ColorCatalogEntry{ "yellow",             &ColorList::Yellow },
    ColorCatalogEntry{ "yellowgreen",        &ColorList::YellowGreen }
};
// clang-format on

/// @brief Compile-time verification that `kColorCatalogTable` has 99 entries, no null pointers, no empty names,
///        `"black"` at index `0`, and no duplicate names.
consteval bool validateColorCatalogTable() noexcept
{
    if (kColorCatalogTable.size() != static_cast<gp::USize>(ColorList::kColorsCount))
    {
        return false;
    }

    if (kColorCatalogTable[0u].name != gp::StringView("black") || kColorCatalogTable[0u].colorPtr == nullptr)
    {
        return false;
    }

    for (gp::USize i = 0u; i < kColorCatalogTable.size(); ++i)
    {
        if (kColorCatalogTable[i].name.isEmpty() || kColorCatalogTable[i].colorPtr == nullptr)
        {
            return false;
        }

        for (gp::USize j = i + 1u; j < kColorCatalogTable.size(); ++j)
        {
            if (kColorCatalogTable[i].name == kColorCatalogTable[j].name)
            {
                return false;
            }
        }
    }

    return true;
}

static_assert(
    validateColorCatalogTable(), "kColorCatalogTable must have 99 unique valid entries with 'black' at index 0."
);

/// @brief Looks up a catalog index by case-insensitive ASCII name across the 99-entry static table.
/// @param[in] colorName Case-insensitive ASCII color name.
/// @return Index in `[0, 98]`, or `-1` if not found.
[[nodiscard]] constexpr gp::Int32 findCatalogIndexByName(const gp::StringView colorName) noexcept
{
    if (colorName.isEmpty())
    {
        return -1;
    }

    for (gp::USize i = 0u; i < kColorCatalogTable.size(); ++i)
    {
        if (kColorCatalogTable[i].name.equalsIgnoreCase(colorName))
        {
            return static_cast<gp::Int32>(i);
        }
    }

    return -1;
}

}   // namespace

ColorList g_colorList{};

void ColorList::createColorMap() noexcept
{
    m_isInitialized = true;
}

const Color& ColorList::getColorByName(const gp::StringView colorName) const noexcept
{
    const gp::Int32 index = findCatalogIndexByName(colorName);
    if (index >= 0)
    {
        return *kColorCatalogTable[static_cast<gp::USize>(index)].colorPtr;
    }
    return White;
}

LinearColor ColorList::getLinearColorByName(const gp::StringView colorName) const noexcept
{
    const gp::Int32 index = findCatalogIndexByName(colorName);
    if (index >= 0)
    {
        return LinearColor(*kColorCatalogTable[static_cast<gp::USize>(index)].colorPtr);
    }
    return LinearColor::kWhite;
}

bool ColorList::isValidColorName(const gp::StringView colorName) const noexcept
{
    return findCatalogIndexByName(colorName) >= 0;
}

gp::Int32 ColorList::getColorIndex(const gp::StringView colorName) const noexcept
{
    const gp::Int32 index = findCatalogIndexByName(colorName);
    return (index >= 0) ? index : 0;
}

const Color& ColorList::getColorByIndex(const gp::Int32 colorIndex) const noexcept
{
    if (isValidColorIndex(colorIndex))
    {
        return *kColorCatalogTable[static_cast<gp::USize>(colorIndex)].colorPtr;
    }
    return White;
}

gp::StringView ColorList::getColorNameByIndex(const gp::Int32 colorIndex) const noexcept
{
    if (isValidColorIndex(colorIndex))
    {
        return kColorCatalogTable[static_cast<gp::USize>(colorIndex)].name;
    }
    return kBadIndexName;
}

void ColorList::logColors() const noexcept
{
    for (gp::USize i = 0u; i < kColorCatalogTable.size(); ++i)
    {
        const ColorCatalogEntry& entry = kColorCatalogTable[i];
        const gp::String colorStr = entry.colorPtr->toString();
        GP_LOG_INFO(LogColorList, "{:3d} - {} -> {}", static_cast<gp::Int32>(i), entry.name, colorStr.view());
    }
}

}   // namespace gp::math
