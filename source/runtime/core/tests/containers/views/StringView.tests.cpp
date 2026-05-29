// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the StringView header to test its functionality.
#include "containers/views/StringView.hpp"

namespace gp::testing
{

TEST(StringViewTests, DefaultConstructor)
{
    StringView sv;
    EXPECT_TRUE(sv.isEmpty());
    EXPECT_EQ(sv.size(), 0);
    EXPECT_EQ(sv.data(), nullptr);
}

TEST(StringViewTests, CStringConstructor)
{
    StringView sv("hello");
    EXPECT_FALSE(sv.isEmpty());
    EXPECT_EQ(sv.size(), 5);
    EXPECT_STREQ(sv.data(), "hello");
}

TEST(StringViewTests, PointerLengthConstructor)
{
    const char* str = "hello world";
    StringView sv(str, 5);
    EXPECT_EQ(sv.size(), 5);
    EXPECT_EQ(sv.front(), 'h');
    EXPECT_EQ(sv.back(), 'o');
}

TEST(StringViewTests, ElementAccess)
{
    StringView sv("abc");
    EXPECT_EQ(sv[0], 'a');
    EXPECT_EQ(sv[1], 'b');
    EXPECT_EQ(sv[2], 'c');
    EXPECT_EQ(sv.at(0), 'a');
    EXPECT_EQ(sv.at(2), 'c');
    EXPECT_EQ(sv.front(), 'a');
    EXPECT_EQ(sv.back(), 'c');
}

TEST(StringViewTests, Iterators)
{
    StringView sv("123");
    EXPECT_EQ(*sv.begin(), '1');
    EXPECT_EQ(*(sv.end() - 1), '3');
    EXPECT_EQ(*sv.rbegin(), '3');
    EXPECT_EQ(*(sv.rend() - 1), '1');
}

TEST(StringViewTests, Modifiers)
{
    StringView sv("hello world");
    sv.removePrefix(6);
    EXPECT_EQ(sv, "world");
    sv.removeSuffix(2);
    EXPECT_EQ(sv, "wor");
}

TEST(StringViewTests, Swap)
{
    StringView sv1("abc");
    StringView sv2("def");
    sv1.swap(sv2);
    EXPECT_EQ(sv1, "def");
    EXPECT_EQ(sv2, "abc");
}

TEST(StringViewTests, Copy)
{
    StringView sv("test");
    char buffer[10] = { 0 };
    sv.copy(buffer, 2, 1);
    EXPECT_STREQ(buffer, "es");
}

TEST(StringViewTests, Substr)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.substr(6, 5), "world");
    EXPECT_EQ(sv.substr(6), "world");
    EXPECT_EQ(sv.substr(0, 5), "hello");
}

TEST(StringViewTests, Compare)
{
    StringView sv1("abc");
    StringView sv2("abd");
    StringView sv3("abc");
    StringView sv4("abcd");

    EXPECT_LT(sv1.compare(sv2), 0);
    EXPECT_EQ(sv1.compare(sv3), 0);
    EXPECT_GT(sv2.compare(sv1), 0);
    EXPECT_LT(sv1.compare(sv4), 0);

    EXPECT_TRUE(sv1 == sv3);
    EXPECT_TRUE(sv1 != sv2);
    EXPECT_TRUE(sv1 < sv2);
    EXPECT_TRUE(sv4 > sv1);
}

TEST(StringViewTests, StartsEndsWith)
{
    StringView sv("hello world");
    EXPECT_TRUE(sv.startsWith("hello"));
    EXPECT_TRUE(sv.startsWith('h'));
    EXPECT_TRUE(sv.endsWith("world"));
    EXPECT_TRUE(sv.endsWith('d'));
    EXPECT_FALSE(sv.startsWith("world"));
    EXPECT_FALSE(sv.endsWith("hello"));
}

TEST(StringViewTests, Contains)
{
    StringView sv("hello world");
    EXPECT_TRUE(sv.contains("lo wo"));
    EXPECT_TRUE(sv.contains('w'));
    EXPECT_FALSE(sv.contains("xyz"));
    EXPECT_FALSE(sv.contains('z'));
}

TEST(StringViewTests, Find)
{
    StringView sv("hello world world");
    EXPECT_EQ(sv.find("world"), 6);
    EXPECT_EQ(sv.find("world", 7), 12);
    EXPECT_EQ(sv.find('o'), 4);
    EXPECT_EQ(sv.find('o', 5), 7);
    EXPECT_EQ(sv.find("xyz"), StringView::npos);
}

TEST(StringViewTests, RFind)
{
    StringView sv("hello world world");
    EXPECT_EQ(sv.rfind("world"), 12);
    EXPECT_EQ(sv.rfind("world", 10), 6);
    EXPECT_EQ(sv.rfind('o'), 13);
    EXPECT_EQ(sv.rfind('o', 10), 7);
    EXPECT_EQ(sv.rfind("xyz"), StringView::npos);
}

TEST(StringViewTests, FindFirstOf)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.findFirstOf("aeiou"), 1);
    EXPECT_EQ(sv.findFirstOf("aeiou", 2), 4);
    EXPECT_EQ(sv.findFirstOf("xyz"), StringView::npos);
}

TEST(StringViewTests, FindLastOf)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.findLastOf("aeiou"), 7);
    EXPECT_EQ(sv.findLastOf("aeiou", 6), 4);
    EXPECT_EQ(sv.findLastOf("xyz"), StringView::npos);
}

TEST(StringViewTests, FindFirstNotOf)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.findFirstNotOf("helo "), 6);
    EXPECT_EQ(sv.findFirstNotOf('h'), 1);
}

TEST(StringViewTests, FindLastNotOf)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.findLastNotOf("dlrow "), 1);
    EXPECT_EQ(sv.findLastNotOf('d'), 9);
}

TEST(StringViewTests, ConstexprSupport)
{
    constexpr StringView sv("constexpr string");
    static_assert(sv.size() == 16);
    static_assert(sv.front() == 'c');
    static_assert(sv.back() == 'g');
    static_assert(sv.contains("string"));
    static_assert(sv.find('y') == StringView::npos);
    static_assert(sv.startsWith("const"));
    static_assert(sv.endsWith("string"));
}

}   // namespace gp::testing
