// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>
#include <sstream>

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

TEST(StringViewTests, NullPtrConstructor)
{
    StringView sv(nullptr);
    EXPECT_TRUE(sv.isEmpty());
    EXPECT_EQ(sv.size(), 0);
    EXPECT_EQ(sv.data(), nullptr);
}

TEST(StringViewTests, CopyConstructor)
{
    StringView original("hello");
    StringView copy(original);
    EXPECT_EQ(copy.size(), 5);
    EXPECT_EQ(copy.data(), original.data());
    EXPECT_EQ(copy, original);
}

TEST(StringViewTests, CopyAssignment)
{
    StringView sv1("hello");
    StringView sv2("world");
    sv2 = sv1;
    EXPECT_EQ(sv2, "hello");
    EXPECT_EQ(sv2.data(), sv1.data());
}

TEST(StringViewTests, LengthAlias)
{
    StringView sv("hello");
    EXPECT_EQ(sv.length(), sv.size());
    EXPECT_EQ(sv.length(), 5);
    StringView empty;
    EXPECT_EQ(empty.length(), 0);
}

TEST(StringViewTests, MaxSize)
{
    StringView sv;
    EXPECT_GT(sv.maxSize(), 0u);
}

TEST(StringViewTests, ConstIterators)
{
    StringView sv("abc");
    EXPECT_EQ(*sv.cbegin(), 'a');
    EXPECT_EQ(*(sv.cend() - 1), 'c');
    EXPECT_EQ(*sv.crbegin(), 'c');
    EXPECT_EQ(*(sv.crend() - 1), 'a');
}

TEST(StringViewTests, RangeBasedFor)
{
    StringView sv("abc");
    std::string result;
    for (char c: sv)
    {
        result += c;
    }
    EXPECT_EQ(result, "abc");
}

TEST(StringViewTests, ModifiersNoOp)
{
    StringView sv("hello");
    sv.removePrefix(0);
    EXPECT_EQ(sv, "hello");
    sv.removeSuffix(0);
    EXPECT_EQ(sv, "hello");
}

TEST(StringViewTests, ModifiersRemoveAll)
{
    StringView sv("hello");
    sv.removePrefix(5);
    EXPECT_TRUE(sv.isEmpty());
    EXPECT_EQ(sv.size(), 0);
}

TEST(StringViewTests, SwapWithEmpty)
{
    StringView sv1("hello");
    StringView sv2;
    sv1.swap(sv2);
    EXPECT_TRUE(sv1.isEmpty());
    EXPECT_EQ(sv2, "hello");
}

TEST(StringViewTests, CopyTruncation)
{
    StringView sv("hello");
    char buffer[10] = { 0 };
    auto copied = sv.copy(buffer, 100);
    EXPECT_EQ(copied, 5u);
    EXPECT_STREQ(buffer, "hello");
}

TEST(StringViewTests, CopyFromMiddle)
{
    StringView sv("hello world");
    char buffer[10] = { 0 };
    auto copied = sv.copy(buffer, 5, 6);
    EXPECT_EQ(copied, 5u);
    EXPECT_STREQ(buffer, "world");
}

TEST(StringViewTests, SubstrAtEnd)
{
    StringView sv("hello");
    EXPECT_EQ(sv.substr(5), "");
    EXPECT_TRUE(sv.substr(5).isEmpty());
}

TEST(StringViewTests, SubstrExcessCount)
{
    StringView sv("hello");
    EXPECT_EQ(sv.substr(2, 1000), "llo");
}

TEST(StringViewTests, SubstrFull)
{
    StringView sv("hello");
    EXPECT_EQ(sv.substr(), "hello");
    EXPECT_EQ(sv.substr(0, StringView::npos), "hello");
}

TEST(StringViewTests, ThreeWayComparison)
{
    StringView sv1("abc");
    StringView sv2("abd");
    StringView sv3("abc");
    StringView sv4("ab");

    EXPECT_TRUE(sv1 < sv2);
    EXPECT_TRUE(sv2 > sv1);
    EXPECT_TRUE(sv1 <= sv3);
    EXPECT_TRUE(sv1 >= sv3);
    EXPECT_TRUE(sv4 < sv1);
    EXPECT_TRUE(sv1 > sv4);
    EXPECT_FALSE(sv1 < sv4);
    EXPECT_FALSE(sv4 > sv1);
}

TEST(StringViewTests, CompareSubstringOverloads)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.compare(6, 5, "world"), 0);
    EXPECT_EQ(sv.compare(6, 5, StringView("world!"), 0, 5), 0);
    EXPECT_EQ(sv.compare("hello world"), 0);
    EXPECT_EQ(sv.compare(0, 5, "hello"), 0);
    EXPECT_LT(sv.compare(0, 5, "world"), 0);
}

TEST(StringViewTests, CompareSelf)
{
    StringView sv("abc");
    EXPECT_EQ(sv.compare(sv), 0);
}

TEST(StringViewTests, StartsWithEmptyAndSelf)
{
    StringView sv("hello");
    EXPECT_TRUE(sv.startsWith(""));
    EXPECT_TRUE(sv.startsWith(StringView("")));
    EXPECT_TRUE(sv.startsWith(sv));
}

TEST(StringViewTests, EndsWithEmptyAndSelf)
{
    StringView sv("hello");
    EXPECT_TRUE(sv.endsWith(""));
    EXPECT_TRUE(sv.endsWith(StringView("")));
    EXPECT_TRUE(sv.endsWith(sv));
}

TEST(StringViewTests, StartsEndsWithOnEmptyView)
{
    StringView sv;
    EXPECT_TRUE(sv.startsWith(""));
    EXPECT_FALSE(sv.startsWith('h'));
    EXPECT_TRUE(sv.endsWith(""));
    EXPECT_FALSE(sv.endsWith('d'));
}

TEST(StringViewTests, ContainsEmpty)
{
    StringView sv("hello");
    EXPECT_TRUE(sv.contains(""));
    EXPECT_TRUE(sv.contains(StringView("")));
}

TEST(StringViewTests, ContainsSelf)
{
    StringView sv("hello");
    EXPECT_TRUE(sv.contains(sv));
}

TEST(StringViewTests, FindEmptySubstring)
{
    StringView sv("hello");
    EXPECT_EQ(sv.find(""), 0u);
    EXPECT_EQ(sv.find("", 3), 3u);
    EXPECT_EQ(sv.find(StringView("")), 0u);
}

TEST(StringViewTests, FindBeyondEnd)
{
    StringView sv("hello");
    EXPECT_EQ(sv.find('l', 10), StringView::npos);
    EXPECT_EQ(sv.find("lo", StringView::npos), StringView::npos);
}

TEST(StringViewTests, FindWithExplicitCount)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.find("world", 0, 5), 6u);
    EXPECT_EQ(sv.find("world!", 0, 5), 6u);
}

TEST(StringViewTests, FindOnEmptyView)
{
    StringView sv;
    EXPECT_EQ(sv.find("hello"), StringView::npos);
    EXPECT_EQ(sv.find('h'), StringView::npos);
}

TEST(StringViewTests, RFindEmptySubstring)
{
    StringView sv("hello");
    EXPECT_EQ(sv.rfind(""), sv.size());
    EXPECT_EQ(sv.rfind("", 2), 2u);
    EXPECT_EQ(sv.rfind(StringView("")), sv.size());
}

TEST(StringViewTests, RFindAtStart)
{
    StringView sv("hello world hello");
    EXPECT_EQ(sv.rfind("hello"), 12u);
    EXPECT_EQ(sv.rfind("hello", 5), 0u);
}

TEST(StringViewTests, RFindWithExplicitCount)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.rfind("world", StringView::npos, 5), 6u);
    EXPECT_EQ(sv.rfind("world!", StringView::npos, 5), 6u);
}

TEST(StringViewTests, RFindOnEmptyView)
{
    StringView sv;
    EXPECT_EQ(sv.rfind("hello"), StringView::npos);
    EXPECT_EQ(sv.rfind('h'), StringView::npos);
}

TEST(StringViewTests, FindFirstOfSingleChar)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.findFirstOf('l'), 2u);
    EXPECT_EQ(sv.findFirstOf('l', 3), 3u);
    EXPECT_EQ(sv.findFirstOf('z'), StringView::npos);
}

TEST(StringViewTests, FindFirstOfExplicitCount)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.findFirstOf("aeiou", 0, 5), 1u);
}

TEST(StringViewTests, FindFirstOfEmptySet)
{
    StringView sv("hello");
    EXPECT_EQ(sv.findFirstOf(""), StringView::npos);
    EXPECT_EQ(sv.findFirstOf(StringView("")), StringView::npos);
}

TEST(StringViewTests, FindFirstOfOnEmptyView)
{
    StringView sv;
    EXPECT_EQ(sv.findFirstOf("aeiou"), StringView::npos);
    EXPECT_EQ(sv.findFirstOf('a'), StringView::npos);
}

TEST(StringViewTests, FindLastOfSingleChar)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.findLastOf('l'), 9u);
    EXPECT_EQ(sv.findLastOf('l', 8), 3u);
    EXPECT_EQ(sv.findLastOf('z'), StringView::npos);
}

TEST(StringViewTests, FindLastOfExplicitCount)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.findLastOf("dlrow", StringView::npos, 5), 10u);
}

TEST(StringViewTests, FindLastOfEmptySet)
{
    StringView sv("hello");
    EXPECT_EQ(sv.findLastOf(""), StringView::npos);
    EXPECT_EQ(sv.findLastOf(StringView("")), StringView::npos);
}

TEST(StringViewTests, FindLastOfOnEmptyView)
{
    StringView sv;
    EXPECT_EQ(sv.findLastOf("aeiou"), StringView::npos);
    EXPECT_EQ(sv.findLastOf('a'), StringView::npos);
}

TEST(StringViewTests, FindFirstNotOfEmptySet)
{
    StringView sv("hello");
    EXPECT_EQ(sv.findFirstNotOf(""), 0u);
    EXPECT_EQ(sv.findFirstNotOf(StringView("")), 0u);
    EXPECT_EQ(sv.findFirstNotOf("", 3), 3u);
}

TEST(StringViewTests, FindFirstNotOfExplicitCount)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.findFirstNotOf("helo ", 0, 5), 6u);
}

TEST(StringViewTests, FindFirstNotOfAllMatch)
{
    StringView sv("aaaa");
    EXPECT_EQ(sv.findFirstNotOf('a'), StringView::npos);
    EXPECT_EQ(sv.findFirstNotOf("a"), StringView::npos);
}

TEST(StringViewTests, FindFirstNotOfOnEmptyView)
{
    StringView sv;
    EXPECT_EQ(sv.findFirstNotOf("abc"), StringView::npos);
    EXPECT_EQ(sv.findFirstNotOf('a'), StringView::npos);
}

TEST(StringViewTests, FindLastNotOfEmptySet)
{
    StringView sv("hello");
    EXPECT_EQ(sv.findLastNotOf(""), 4u);
    EXPECT_EQ(sv.findLastNotOf(StringView("")), 4u);
    EXPECT_EQ(sv.findLastNotOf("", 2), 2u);
}

TEST(StringViewTests, FindLastNotOfExplicitCount)
{
    StringView sv("hello world");
    EXPECT_EQ(sv.findLastNotOf("dlrow ", StringView::npos, 6), 1u);
}

TEST(StringViewTests, FindLastNotOfAllMatch)
{
    StringView sv("aaaa");
    EXPECT_EQ(sv.findLastNotOf('a'), StringView::npos);
    EXPECT_EQ(sv.findLastNotOf("a"), StringView::npos);
}

TEST(StringViewTests, FindLastNotOfOnEmptyView)
{
    StringView sv;
    EXPECT_EQ(sv.findLastNotOf("abc"), StringView::npos);
    EXPECT_EQ(sv.findLastNotOf('a'), StringView::npos);
}

TEST(StringViewTests, StreamOutput)
{
    std::ostringstream oss;
    StringView sv("hello");
    oss << sv;
    EXPECT_EQ(oss.str(), "hello");
}

TEST(StringViewTests, StreamOutputNonNullTerminated)
{
    // A view over "hello" carved out of a longer null-terminated buffer.
    // operator<< must write exactly sv.size() bytes, not the whole buffer.
    std::ostringstream oss;
    const char* buf = "hello world";
    StringView sv(buf, 5);
    oss << sv;
    EXPECT_EQ(oss.str(), "hello");
}

TEST(StringViewTests, StreamOutputEmpty)
{
    std::ostringstream oss;
    StringView sv;
    oss << sv;
    EXPECT_EQ(oss.str(), "");
}

TEST(StringViewTests, WideStringView)
{
    gp::WStringView wsv(L"hello");
    EXPECT_EQ(wsv.size(), 5u);
    EXPECT_EQ(wsv.front(), L'h');
    EXPECT_EQ(wsv.back(), L'o');
    EXPECT_TRUE(wsv.startsWith(L'h'));
    EXPECT_TRUE(wsv.endsWith(L'o'));
    EXPECT_TRUE(wsv.contains(L'l'));
    EXPECT_EQ(wsv.find(L'l'), 2u);
}

TEST(StringViewTests, ConstexprIterators)
{
    constexpr StringView sv("abc");
    static_assert(*sv.cbegin() == 'a');
    static_assert(sv.cbegin() + 3 == sv.cend());
    static_assert(*sv.crbegin() == 'c');
}

TEST(StringViewTests, ConstexprCompare)
{
    constexpr StringView sv1("abc");
    constexpr StringView sv2("abc");
    constexpr StringView sv3("abd");
    static_assert(sv1.compare(sv2) == 0);
    static_assert(sv1.compare(sv3) < 0);
    static_assert(sv1 == sv2);
    static_assert(sv1 < sv3);
}

}   // namespace gp::testing
