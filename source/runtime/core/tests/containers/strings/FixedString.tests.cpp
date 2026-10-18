// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground.com/legal
// mailto:support AT graphical-playground DOT com

#include "containers/strings/FixedString.hpp"
#include <gtest/gtest.h>

namespace gp::tests
{

TEST(FixedStringTests, DefaultConstructor)
{
    FixedString<32> str;
    EXPECT_TRUE(str.isEmpty());
    EXPECT_EQ(str.size(), 0u);
    EXPECT_EQ(str.length(), 0u);
    EXPECT_EQ(str.capacity(), 32u);
    EXPECT_EQ(str.maxSize(), 32u);
    EXPECT_STREQ(str.cString(), "");
}

TEST(FixedStringTests, ConstructFromCString)
{
    FixedString<32> str("Hello");
    EXPECT_FALSE(str.isEmpty());
    EXPECT_EQ(str.size(), 5u);
    EXPECT_STREQ(str.cString(), "Hello");
}

TEST(FixedStringTests, ConstructFromStringView)
{
    gp::StringView view("World");
    FixedString<32> str(view);
    EXPECT_EQ(str.size(), 5u);
    EXPECT_STREQ(str.cString(), "World");
}

TEST(FixedStringTests, ElementAccess)
{
    FixedString<32> str("Hello");
    EXPECT_EQ(str[0], 'H');
    EXPECT_EQ(str[4], 'o');
    EXPECT_EQ(str.at(1), 'e');
    EXPECT_EQ(str.front(), 'H');
    EXPECT_EQ(str.back(), 'o');
}

TEST(FixedStringTests, Iterators)
{
    FixedString<32> str("abc");

    // Normal iterators
    auto it = str.begin();
    EXPECT_EQ(*it, 'a');
    ++it;
    EXPECT_EQ(*it, 'b');
    ++it;
    EXPECT_EQ(*it, 'c');
    ++it;
    EXPECT_EQ(it, str.end());

    // Reverse iterators
    auto rit = str.rbegin();
    EXPECT_EQ(*rit, 'c');
    ++rit;
    EXPECT_EQ(*rit, 'b');
    ++rit;
    EXPECT_EQ(*rit, 'a');
    ++rit;
    EXPECT_EQ(rit, str.rend());
}

TEST(FixedStringTests, Modification)
{
    FixedString<32> str;
    str.pushBack('H');
    EXPECT_EQ(str.size(), 1u);
    EXPECT_STREQ(str.cString(), "H");

    str += 'i';
    EXPECT_EQ(str.size(), 2u);
    EXPECT_STREQ(str.cString(), "Hi");

    str.append(" there");
    EXPECT_EQ(str.size(), 8u);
    EXPECT_STREQ(str.cString(), "Hi there");

    str += "!";
    EXPECT_EQ(str.size(), 9u);
    EXPECT_STREQ(str.cString(), "Hi there!");

    str.popBack();
    EXPECT_EQ(str.size(), 8u);
    EXPECT_STREQ(str.cString(), "Hi there");

    str.clear();
    EXPECT_TRUE(str.isEmpty());
    EXPECT_EQ(str.size(), 0u);
    EXPECT_STREQ(str.cString(), "");
}

TEST(FixedStringTests, Assign)
{
    FixedString<32> str("Initial");
    str.assign("New Value");
    EXPECT_EQ(str.size(), 9u);
    EXPECT_STREQ(str.cString(), "New Value");
}

TEST(FixedStringTests, Comparison)
{
    FixedString<32> str1("abc");
    FixedString<32> str2("abc");
    FixedString<32> str3("abd");
    FixedString<16> str4("abc");

    EXPECT_TRUE(str1 == str2);
    EXPECT_FALSE(str1 == str3);
    EXPECT_TRUE(str1 == str4);

    EXPECT_TRUE(str1 <=> str3 < 0);
    EXPECT_TRUE(str3 <=> str1 > 0);
    EXPECT_TRUE(str1 <=> str2 == 0);

    gp::StringView view("abc");
    EXPECT_TRUE(str1 == view);
}

TEST(FixedStringTests, ConversionToStringView)
{
    FixedString<32> str("Hello");
    gp::StringView view = str;
    EXPECT_EQ(view.size(), 5u);
    EXPECT_EQ(view, "Hello");
}

TEST(FixedStringTests, TypeAliases)
{
    FixedWString<32> wstr(L"Wide");
    EXPECT_EQ(wstr.size(), 4u);

    FixedU8String<32> u8str(u8"UTF-8");
    EXPECT_EQ(u8str.size(), 5u);

    FixedU16String<32> u16str(u"UTF-16");
    EXPECT_EQ(u16str.size(), 6u);

    FixedU32String<32> u32str(U"UTF-32");
    EXPECT_EQ(u32str.size(), 6u);
}

}   // namespace gp::tests
