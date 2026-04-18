// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "container/BasicStringView.hpp"
#include "container/Forward.hpp"
#include <catch2/catch_test_macros.hpp>

namespace gp::tests
{

TEST_CASE("StringView - default construction", "[container][StringView]")
{
    gp::StringView v;
    REQUIRE(v.data() == nullptr);
    REQUIRE(v.size() == 0);
    REQUIRE(v.empty());
}

TEST_CASE("StringView - construct from C string", "[container][StringView]")
{
    gp::StringView v("hello");
    REQUIRE(v.size() == 5);
    REQUIRE(v[0] == 'h');
    REQUIRE(v[4] == 'o');
}

TEST_CASE("StringView - construct from nullptr", "[container][StringView]")
{
    gp::StringView v(nullptr);
    REQUIRE(v.size() == 0);
    REQUIRE(v.data() == nullptr);
}

TEST_CASE("StringView - construct from pointer and length", "[container][StringView]")
{
    const char* str = "hello world";
    gp::StringView v(str, 5);
    REQUIRE(v.size() == 5);
    REQUIRE(v[4] == 'o');
}

TEST_CASE("StringView - at", "[container][StringView]")
{
    gp::StringView v("abcdef");
    REQUIRE(v.at(0) == 'a');
    REQUIRE(v.at(5) == 'f');
}

TEST_CASE("StringView - front and back", "[container][StringView]")
{
    gp::StringView v("abc");
    REQUIRE(v.front() == 'a');
    REQUIRE(v.back() == 'c');
}

TEST_CASE("StringView - length and maxSize", "[container][StringView]")
{
    gp::StringView v("test");
    REQUIRE(v.length() == 4);
    REQUIRE(v.maxSize() > 0);
}

TEST_CASE("StringView - iterators", "[container][StringView]")
{
    gp::StringView v("abc");
    REQUIRE(*v.begin() == 'a');
    REQUIRE(*(v.end() - 1) == 'c');
    REQUIRE(*v.cbegin() == 'a');

    gp::USize count = 0;
    for (auto it = v.begin(); it != v.end(); ++it) { ++count; }
    REQUIRE(count == 3);
}

TEST_CASE("StringView - reverse iterators", "[container][StringView]")
{
    gp::StringView v("abc");
    REQUIRE(*v.rbegin() == 'c');
    REQUIRE(*(v.rend() - 1) == 'a');
    REQUIRE(*v.crbegin() == 'c');
}

TEST_CASE("StringView - removePrefix", "[container][StringView]")
{
    gp::StringView v("hello world");
    v.removePrefix(6);
    REQUIRE(v.size() == 5);
    REQUIRE(v == gp::StringView("world"));
}

TEST_CASE("StringView - removeSuffix", "[container][StringView]")
{
    gp::StringView v("hello world");
    v.removeSuffix(6);
    REQUIRE(v.size() == 5);
    REQUIRE(v == gp::StringView("hello"));
}

TEST_CASE("StringView - swap", "[container][StringView]")
{
    gp::StringView a("hello");
    gp::StringView b("world");
    a.swap(b);
    REQUIRE(a == gp::StringView("world"));
    REQUIRE(b == gp::StringView("hello"));
}

TEST_CASE("StringView - copy", "[container][StringView]")
{
    gp::StringView v("hello world");
    char buf[6] = {};
    gp::USize n = v.copy(buf, 5, 6);
    REQUIRE(n == 5);
    REQUIRE(gp::StringView(buf, 5) == gp::StringView("world"));
}

TEST_CASE("StringView - copy clamped to remaining", "[container][StringView]")
{
    gp::StringView v("hi");
    char buf[10] = {};
    gp::USize n = v.copy(buf, 100);
    REQUIRE(n == 2);
}

TEST_CASE("StringView - substr", "[container][StringView]")
{
    gp::StringView v("hello world");
    REQUIRE(v.substr(0, 5) == gp::StringView("hello"));
    REQUIRE(v.substr(6) == gp::StringView("world"));
    REQUIRE(v.substr(6, 100) == gp::StringView("world"));
    REQUIRE(v.substr(0, 0).empty());
}

TEST_CASE("StringView - compare", "[container][StringView]")
{
    REQUIRE(gp::StringView("abc").compare(gp::StringView("abc")) == 0);
    REQUIRE(gp::StringView("abc").compare(gp::StringView("abd")) < 0);
    REQUIRE(gp::StringView("abd").compare(gp::StringView("abc")) > 0);
    REQUIRE(gp::StringView("ab").compare(gp::StringView("abc")) < 0);
    REQUIRE(gp::StringView("abc").compare(gp::StringView("ab")) > 0);
}

TEST_CASE("StringView - compare substrings", "[container][StringView]")
{
    gp::StringView v("hello world");
    REQUIRE(v.compare(0, 5, gp::StringView("hello")) == 0);
    REQUIRE(v.compare(6, 5, gp::StringView("world")) == 0);
}

TEST_CASE("StringView - compare with C string", "[container][StringView]")
{
    REQUIRE(gp::StringView("abc").compare("abc") == 0);
    REQUIRE(gp::StringView("abc").compare("abd") < 0);
}

TEST_CASE("StringView - equality operators", "[container][StringView]")
{
    REQUIRE(gp::StringView("abc") == gp::StringView("abc"));
    REQUIRE(gp::StringView("abc") != gp::StringView("def"));
    REQUIRE_FALSE(gp::StringView("abc") == gp::StringView("ab"));
}

TEST_CASE("StringView - spaceship operator", "[container][StringView]")
{
    CHECK((gp::StringView("abc") <=> gp::StringView("abd")) == std::strong_ordering::less);
    CHECK((gp::StringView("abd") <=> gp::StringView("abc")) == std::strong_ordering::greater);
    CHECK((gp::StringView("abc") <=> gp::StringView("abc")) == std::strong_ordering::equal);
    CHECK((gp::StringView("ab") <=> gp::StringView("abc")) == std::strong_ordering::less);
}

TEST_CASE("StringView - startsWith", "[container][StringView]")
{
    gp::StringView v("hello world");
    REQUIRE(v.startsWith(gp::StringView("hello")));
    REQUIRE(v.startsWith('h'));
    REQUIRE(v.startsWith("hello"));
    REQUIRE_FALSE(v.startsWith(gp::StringView("world")));
    REQUIRE(v.startsWith(gp::StringView("")));
}

TEST_CASE("StringView - endsWith", "[container][StringView]")
{
    gp::StringView v("hello world");
    REQUIRE(v.endsWith(gp::StringView("world")));
    REQUIRE(v.endsWith('d'));
    REQUIRE(v.endsWith("world"));
    REQUIRE_FALSE(v.endsWith(gp::StringView("hello")));
    REQUIRE(v.endsWith(gp::StringView("")));
}

TEST_CASE("StringView - contains", "[container][StringView]")
{
    gp::StringView v("hello world");
    REQUIRE(v.contains(gp::StringView("lo wo")));
    REQUIRE(v.contains('w'));
    REQUIRE(v.contains("world"));
    REQUIRE_FALSE(v.contains(gp::StringView("xyz")));
    REQUIRE(v.contains(gp::StringView("")));
}

TEST_CASE("StringView - find substring", "[container][StringView]")
{
    gp::StringView v("hello hello world");
    REQUIRE(v.find(gp::StringView("hello")) == 0);
    REQUIRE(v.find(gp::StringView("hello"), 1) == 6);
    REQUIRE(v.find(gp::StringView("world")) == 12);
    REQUIRE(v.find(gp::StringView("xyz")) == gp::StringView::npos);
}

TEST_CASE("StringView - find char", "[container][StringView]")
{
    gp::StringView v("abcabc");
    REQUIRE(v.find('a') == 0);
    REQUIRE(v.find('a', 1) == 3);
    REQUIRE(v.find('z') == gp::StringView::npos);
}

TEST_CASE("StringView - find empty substring", "[container][StringView]")
{
    gp::StringView v("hello");
    REQUIRE(v.find(gp::StringView("")) == 0);
    REQUIRE(v.find(gp::StringView(""), 3) == 3);
    REQUIRE(v.find(gp::StringView(""), 5) == 5);
    REQUIRE(v.find(gp::StringView(""), 6) == gp::StringView::npos);
}

TEST_CASE("StringView - find with C string", "[container][StringView]")
{
    gp::StringView v("test string");
    REQUIRE(v.find("string") == 5);
    REQUIRE(v.find("string", 6) == gp::StringView::npos);
}

TEST_CASE("StringView - find with pointer and count", "[container][StringView]")
{
    gp::StringView v("abcdef");
    REQUIRE(v.find("cde_extra", 0, 3) == 2);
}

TEST_CASE("StringView - rfind substring", "[container][StringView]")
{
    gp::StringView v("hello hello world");
    REQUIRE(v.rfind(gp::StringView("hello")) == 6);
    REQUIRE(v.rfind(gp::StringView("hello"), 5) == 0);
    REQUIRE(v.rfind(gp::StringView("xyz")) == gp::StringView::npos);
}

TEST_CASE("StringView - rfind char", "[container][StringView]")
{
    gp::StringView v("abcabc");
    REQUIRE(v.rfind('c') == 5);
    REQUIRE(v.rfind('c', 4) == 2);
    REQUIRE(v.rfind('z') == gp::StringView::npos);
}

TEST_CASE("StringView - rfind empty substring", "[container][StringView]")
{
    gp::StringView v("hello");
    REQUIRE(v.rfind(gp::StringView("")) == 5);
    REQUIRE(v.rfind(gp::StringView(""), 3) == 3);
}

TEST_CASE("StringView - rfind on empty view", "[container][StringView]")
{
    gp::StringView v;
    REQUIRE(v.rfind('a') == gp::StringView::npos);
    REQUIRE(v.rfind(gp::StringView("")) == 0);
}

TEST_CASE("StringView - findFirstOf", "[container][StringView]")
{
    gp::StringView v("hello world");
    REQUIRE(v.findFirstOf(gp::StringView("aeiou")) == 1);
    REQUIRE(v.findFirstOf(gp::StringView("xyz")) == gp::StringView::npos);
    REQUIRE(v.findFirstOf('o') == 4);
    REQUIRE(v.findFirstOf("ow") == 4);
}

TEST_CASE("StringView - findLastOf", "[container][StringView]")
{
    gp::StringView v("hello world");
    REQUIRE(v.findLastOf(gp::StringView("aeiou")) == 7);
    REQUIRE(v.findLastOf(gp::StringView("xyz")) == gp::StringView::npos);
    REQUIRE(v.findLastOf('l') == 9);
    REQUIRE(v.findLastOf("ld") == 10);
}

TEST_CASE("StringView - findFirstOf and findLastOf on empty", "[container][StringView]")
{
    gp::StringView v;
    REQUIRE(v.findFirstOf(gp::StringView("abc")) == gp::StringView::npos);
    REQUIRE(v.findLastOf(gp::StringView("abc")) == gp::StringView::npos);
}

TEST_CASE("StringView - findFirstNotOf", "[container][StringView]")
{
    gp::StringView v("aaabbbccc");
    REQUIRE(v.findFirstNotOf(gp::StringView("a")) == 3);
    REQUIRE(v.findFirstNotOf('a') == 3);
    REQUIRE(v.findFirstNotOf(gp::StringView("abc")) == gp::StringView::npos);
    REQUIRE(v.findFirstNotOf("ab") == 6);
}

TEST_CASE("StringView - findLastNotOf", "[container][StringView]")
{
    gp::StringView v("aaabbbccc");
    REQUIRE(v.findLastNotOf(gp::StringView("c")) == 5);
    REQUIRE(v.findLastNotOf('c') == 5);
    REQUIRE(v.findLastNotOf(gp::StringView("abc")) == gp::StringView::npos);
    REQUIRE(v.findLastNotOf("bc") == 2);
}

TEST_CASE("StringView - findFirstNotOf and findLastNotOf on empty", "[container][StringView]")
{
    gp::StringView v;
    REQUIRE(v.findFirstNotOf(gp::StringView("a")) == gp::StringView::npos);
    REQUIRE(v.findLastNotOf(gp::StringView("a")) == gp::StringView::npos);
}

TEST_CASE("StringView - single character view", "[container][StringView]")
{
    gp::StringView v("x");
    REQUIRE(v.size() == 1);
    REQUIRE(v.front() == 'x');
    REQUIRE(v.back() == 'x');
    REQUIRE(v.find('x') == 0);
    REQUIRE(v.rfind('x') == 0);
    REQUIRE(v.startsWith('x'));
    REQUIRE(v.endsWith('x'));
    REQUIRE(v.contains('x'));
}

TEST_CASE("StringView - find pattern larger than view", "[container][StringView]")
{
    gp::StringView v("ab");
    REQUIRE(v.find(gp::StringView("abc")) == gp::StringView::npos);
    REQUIRE(v.rfind(gp::StringView("abc")) == gp::StringView::npos);
}

TEST_CASE("StringView - substr with pos == size", "[container][StringView]")
{
    gp::StringView v("hello");
    gp::StringView sub = v.substr(5);
    REQUIRE(sub.empty());
}

}   // namespace gp::tests
