// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "maths/color/ColorList.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "maths/color/Color.hpp"
#include "maths/color/LinearColor.hpp"
#include <gtest/gtest.h>

namespace gp::math::tests
{

TEST(ColorListTest, CatalogSizeAndIndexBoundaries)
{
    g_colorList.createColorMap();
    EXPECT_TRUE(g_colorList.isInitialized());
    EXPECT_EQ(g_colorList.getColorsNum(), 99);

    // Index 0 is "black", Index 98 is "yellowgreen"
    EXPECT_TRUE(g_colorList.isValidColorIndex(0));
    EXPECT_TRUE(g_colorList.isValidColorIndex(98));
    EXPECT_FALSE(g_colorList.isValidColorIndex(-1));
    EXPECT_FALSE(g_colorList.isValidColorIndex(99));
    EXPECT_FALSE(g_colorList.isValidColorIndex(100));

    EXPECT_EQ(g_colorList.getColorNameByIndex(0), gp::StringView("black"));
    EXPECT_EQ(g_colorList.getColorByIndex(0), ColorList::Black);
    EXPECT_EQ(g_colorList.getFColorByIndex(0), ColorList::Black);

    EXPECT_EQ(g_colorList.getColorNameByIndex(98), gp::StringView("yellowgreen"));
    EXPECT_EQ(g_colorList.getColorByIndex(98), ColorList::YellowGreen);

    // Out-of-range indices fall back to White and "BadIndex"
    EXPECT_EQ(g_colorList.getColorByIndex(-5), ColorList::White);
    EXPECT_EQ(g_colorList.getColorByIndex(99), ColorList::White);
    EXPECT_EQ(g_colorList.getColorByIndex(100), ColorList::White);
    EXPECT_EQ(g_colorList.getColorNameByIndex(-1), ColorList::kBadIndexName);
    EXPECT_EQ(g_colorList.getColorNameByIndex(99), ColorList::kBadIndexName);
    EXPECT_EQ(g_colorList.getColorNameByIndex(200), ColorList::kBadIndexName);
}

TEST(ColorListTest, EveryCatalogEntryRoundTripsByNameAndIndex)
{
    for (gp::Int32 idx = 0; idx < g_colorList.getColorsNum(); ++idx)
    {
        const gp::StringView name = g_colorList.getColorNameByIndex(idx);
        ASSERT_FALSE(name.isEmpty());
        ASSERT_NE(name, ColorList::kBadIndexName);

        EXPECT_TRUE(g_colorList.isValidColorName(name));
        EXPECT_EQ(g_colorList.getColorIndex(name), idx);

        const Color& byIndex = g_colorList.getColorByIndex(idx);
        const Color& byName = g_colorList.getColorByName(name);
        EXPECT_EQ(byIndex, byName);
        EXPECT_EQ(g_colorList.getFColorByName(name), byIndex);

        const LinearColor linByName = g_colorList.getLinearColorByName(name);
        EXPECT_EQ(linByName, LinearColor(byIndex));
        EXPECT_EQ(g_colorList.getFLinearColorByName(name), LinearColor(byIndex));
    }
}

TEST(ColorListTest, CaseInsensitiveLookupAndInvalidNames)
{
    EXPECT_TRUE(g_colorList.isValidColorName("Aquamarine"));
    EXPECT_TRUE(g_colorList.isValidColorName("AQUAMARINE"));
    EXPECT_TRUE(g_colorList.isValidColorName("aquamarine"));
    EXPECT_EQ(g_colorList.getColorByName("AQUAMARINE"), ColorList::Aquamarine);
    EXPECT_EQ(g_colorList.getColorByName("MandarianOrange"), ColorList::MandarianOrange);
    EXPECT_EQ(g_colorList.getColorByName("SemiSweetChocolate"), ColorList::SemiSweetChocolate);

    // Invalid or empty names
    EXPECT_FALSE(g_colorList.isValidColorName(""));
    EXPECT_FALSE(g_colorList.isValidColorName("NotARealColorName"));
    EXPECT_EQ(g_colorList.getColorByName(""), ColorList::White);
    EXPECT_EQ(g_colorList.getColorByName("NotARealColorName"), ColorList::White);
    EXPECT_EQ(g_colorList.getLinearColorByName("NotARealColorName"), LinearColor::kWhite);
    EXPECT_EQ(g_colorList.getColorIndex("NotARealColorName"), 0);

    // Exercise logColors() to verify zero crashes or formatting errors
    g_colorList.logColors();
}

}   // namespace gp::math::tests
