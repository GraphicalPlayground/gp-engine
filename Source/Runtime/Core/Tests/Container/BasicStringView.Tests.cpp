// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Container.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("BasicStringView Construction", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("Default Constructor")
    {
        TBasicStringView<char> sv;
        REQUIRE(sv.Data() == nullptr);
        REQUIRE(sv.Size() == 0);
        REQUIRE(sv.IsEmpty());
    }

    SECTION("Constructor from Pointer and Length")
    {
        const char* str = "Hello";
        TBasicStringView<char> sv(str, 5);
        REQUIRE(sv.Data() == str);
        REQUIRE(sv.Size() == 5);
        REQUIRE_FALSE(sv.IsEmpty());
    }

    SECTION("Constructor from Null-Terminated String")
    {
        const char* str = "Hello World";
        TBasicStringView<char> sv(str);
        REQUIRE(sv.Data() == str);
        REQUIRE(sv.Size() == 11);
    }

    SECTION("Copy Constructor")
    {
        TBasicStringView<char> sv1("test");
        TBasicStringView<char> sv2(sv1);
        REQUIRE(sv2.Data() == sv1.Data());
        REQUIRE(sv2.Size() == sv1.Size());
    }

    SECTION("Assignment Operator")
    {
        TBasicStringView<char> sv1("original");
        TBasicStringView<char> sv2;
        sv2 = sv1;
        REQUIRE(sv2.Data() == sv1.Data());
        REQUIRE(sv2.Size() == sv1.Size());
    }
}

TEST_CASE("BasicStringView Element Access", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("Operator[] Access")
    {
        TBasicStringView<char> sv("Hello");
        REQUIRE(sv[0] == 'H');
        REQUIRE(sv[4] == 'o');
    }

    SECTION("At() Access")
    {
        TBasicStringView<char> sv("Test");
        REQUIRE(sv.At(0) == 'T');
        REQUIRE(sv.At(3) == 't');
    }

    SECTION("Front()")
    {
        TBasicStringView<char> sv("abc");
        REQUIRE(sv.Front() == 'a');
    }

    SECTION("Back()")
    {
        TBasicStringView<char> sv("xyz");
        REQUIRE(sv.Back() == 'z');
    }

    SECTION("Data()")
    {
        const char* str = "data";
        TBasicStringView<char> sv(str);
        REQUIRE(sv.Data() == str);
    }
}

TEST_CASE("BasicStringView Size Operations", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("Size() and Length()")
    {
        TBasicStringView<char> sv("12345");
        REQUIRE(sv.Size() == 5);
        REQUIRE(sv.Length() == 5);
        REQUIRE(sv.Size() == sv.Length());
    }

    SECTION("IsEmpty()")
    {
        TBasicStringView<char> sv1;
        TBasicStringView<char> sv2("");
        TBasicStringView<char> sv3("not empty");

        REQUIRE(sv1.IsEmpty());
        REQUIRE(sv2.IsEmpty());
        REQUIRE_FALSE(sv3.IsEmpty());
    }
}

TEST_CASE("BasicStringView Modifiers", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("RemovePrefix()")
    {
        TBasicStringView<char> sv("Hello World");
        sv.RemovePrefix(6);
        REQUIRE(sv.Size() == 5);
        REQUIRE(sv[0] == 'W');
    }

    SECTION("RemoveSuffix()")
    {
        TBasicStringView<char> sv("Hello World");
        sv.RemoveSuffix(6);
        REQUIRE(sv.Size() == 5);
        REQUIRE(sv[4] == 'o');
    }

    SECTION("Swap()")
    {
        TBasicStringView<char> sv1("first");
        TBasicStringView<char> sv2("second");
        auto data1 = sv1.Data();
        auto data2 = sv2.Data();
        auto size1 = sv1.Size();
        auto size2 = sv2.Size();

        sv1.Swap(sv2);
        REQUIRE(sv1.Data() == data2);
        REQUIRE(sv1.Size() == size2);
        REQUIRE(sv2.Data() == data1);
        REQUIRE(sv2.Size() == size1);
    }
}

TEST_CASE("BasicStringView Copy and Substr", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("Copy() Full")
    {
        TBasicStringView<char> sv("copy test");
        char buffer[10] = {};
        auto copied = sv.Copy(buffer, 9);
        REQUIRE(copied == 9);
        REQUIRE(std::string(buffer, 9) == "copy test");
    }

    SECTION("Copy() Partial")
    {
        TBasicStringView<char> sv("Hello World");
        char buffer[6] = {};
        auto copied = sv.Copy(buffer, 5, 6);
        REQUIRE(copied == 5);
        REQUIRE(std::string(buffer, 5) == "World");
    }

    SECTION("Substr() Full")
    {
        TBasicStringView<char> sv("substring");
        auto sub = sv.Substr();
        REQUIRE(sub.Size() == sv.Size());
        REQUIRE(sub.Data() == sv.Data());
    }

    SECTION("Substr() Partial")
    {
        TBasicStringView<char> sv("Hello World");
        auto sub = sv.Substr(6, 5);
        REQUIRE(sub.Size() == 5);
        REQUIRE(sub[0] == 'W');
    }

    SECTION("Substr() To End")
    {
        TBasicStringView<char> sv("Test String");
        auto sub = sv.Substr(5);
        REQUIRE(sub.Size() == 6);
        REQUIRE(sub[0] == 'S');
    }
}

TEST_CASE("BasicStringView Comparison", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("Compare() Equal")
    {
        TBasicStringView<char> sv1("equal");
        TBasicStringView<char> sv2("equal");
        REQUIRE(sv1.Compare(sv2) == 0);
    }

    SECTION("Compare() Less Than")
    {
        TBasicStringView<char> sv1("apple");
        TBasicStringView<char> sv2("banana");
        REQUIRE(sv1.Compare(sv2) < 0);
    }

    SECTION("Compare() Greater Than")
    {
        TBasicStringView<char> sv1("zebra");
        TBasicStringView<char> sv2("apple");
        REQUIRE(sv1.Compare(sv2) > 0);
    }

    SECTION("Compare() Different Lengths")
    {
        TBasicStringView<char> sv1("test");
        TBasicStringView<char> sv2("testing");
        REQUIRE(sv1.Compare(sv2) < 0);
    }

    SECTION("operator==")
    {
        TBasicStringView<char> sv1("same");
        TBasicStringView<char> sv2("same");
        TBasicStringView<char> sv3("different");
        REQUIRE(sv1 == sv2);
        REQUIRE_FALSE(sv1 == sv3);
    }

    SECTION("operator<=>")
    {
        TBasicStringView<char> sv1("a");
        TBasicStringView<char> sv2("b");
        TBasicStringView<char> sv3("a");
        REQUIRE(sv1 < sv2);
        REQUIRE(sv2 > sv1);
        REQUIRE(sv1 <= sv3);
        REQUIRE(sv1 >= sv3);
    }
}

TEST_CASE("BasicStringView Prefix and Suffix", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("StartsWith() String View")
    {
        TBasicStringView<char> sv("Hello World");
        REQUIRE(sv.StartsWith("Hello"));
        REQUIRE_FALSE(sv.StartsWith("World"));
    }

    SECTION("StartsWith() Character")
    {
        TBasicStringView<char> sv("Test");
        REQUIRE(sv.StartsWith('T'));
        REQUIRE_FALSE(sv.StartsWith('t'));
    }

    SECTION("EndsWith() String View")
    {
        TBasicStringView<char> sv("Hello World");
        REQUIRE(sv.EndsWith("World"));
        REQUIRE_FALSE(sv.EndsWith("Hello"));
    }

    SECTION("EndsWith() Character")
    {
        TBasicStringView<char> sv("Test");
        REQUIRE(sv.EndsWith('t'));
        REQUIRE_FALSE(sv.EndsWith('T'));
    }

    SECTION("Empty String Checks")
    {
        TBasicStringView<char> sv("");
        REQUIRE_FALSE(sv.StartsWith('a'));
        REQUIRE_FALSE(sv.EndsWith('a'));
    }
}

TEST_CASE("BasicStringView Contains", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("Contains() String View")
    {
        TBasicStringView<char> sv("Hello World");
        REQUIRE(sv.Contains("World"));
        REQUIRE(sv.Contains("o W"));
        REQUIRE_FALSE(sv.Contains("xyz"));
    }

    SECTION("Contains() Character")
    {
        TBasicStringView<char> sv("Hello");
        REQUIRE(sv.Contains('H'));
        REQUIRE(sv.Contains('o'));
        REQUIRE_FALSE(sv.Contains('x'));
    }

    SECTION("Contains() Empty")
    {
        TBasicStringView<char> sv("test");
        REQUIRE(sv.Contains(""));
    }
}

TEST_CASE("BasicStringView Find", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("Find() String View")
    {
        TBasicStringView<char> sv("Hello World Hello");
        REQUIRE(sv.Find("Hello") == 0);
        REQUIRE(sv.Find("World") == 6);
        REQUIRE(sv.Find("Hello", 1) == 12);
        REQUIRE(sv.Find("xyz") == TBasicStringView<char>::NPOS);
    }

    SECTION("Find() Character")
    {
        TBasicStringView<char> sv("Hello");
        REQUIRE(sv.Find('H') == 0);
        REQUIRE(sv.Find('l') == 2);
        REQUIRE(sv.Find('l', 3) == 3);
        REQUIRE(sv.Find('x') == TBasicStringView<char>::NPOS);
    }

    SECTION("Find() Empty String")
    {
        TBasicStringView<char> sv("test");
        REQUIRE(sv.Find("") == 0);
        REQUIRE(sv.Find("", 2) == 2);
    }

    SECTION("Find() Past End")
    {
        TBasicStringView<char> sv("test");
        REQUIRE(sv.Find("t", 100) == TBasicStringView<char>::NPOS);
    }
}

TEST_CASE("BasicStringView RFind", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("RFind() String View")
    {
        TBasicStringView<char> sv("Hello World Hello");
        REQUIRE(sv.RFind("Hello") == 12);
        REQUIRE(sv.RFind("Hello", 11) == 0);
        REQUIRE(sv.RFind("xyz") == TBasicStringView<char>::NPOS);
    }

    SECTION("RFind() Character")
    {
        TBasicStringView<char> sv("Hello");
        REQUIRE(sv.RFind('l') == 3);
        REQUIRE(sv.RFind('l', 2) == 2);
        REQUIRE(sv.RFind('H') == 0);
        REQUIRE(sv.RFind('x') == TBasicStringView<char>::NPOS);
    }

    SECTION("RFind() Empty String")
    {
        TBasicStringView<char> sv("test");
        REQUIRE(sv.RFind("") == 4);
        REQUIRE(sv.RFind("", 2) == 2);
    }

    SECTION("RFind() Empty Source")
    {
        TBasicStringView<char> sv;
        REQUIRE(sv.RFind('a') == TBasicStringView<char>::NPOS);
    }
}

TEST_CASE("BasicStringView FindFirstOf", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("FindFirstOf() String View")
    {
        TBasicStringView<char> sv("Hello World");
        REQUIRE(sv.FindFirstOf("aeiou") == 1);
        REQUIRE(sv.FindFirstOf("xyz") == TBasicStringView<char>::NPOS);
        REQUIRE(sv.FindFirstOf("o", 5) == 7);
    }

    SECTION("FindFirstOf() Character")
    {
        TBasicStringView<char> sv("Hello");
        REQUIRE(sv.FindFirstOf('l') == 2);
        REQUIRE(sv.FindFirstOf('x') == TBasicStringView<char>::NPOS);
    }

    SECTION("FindFirstOf() With Count")
    {
        TBasicStringView<char> sv("Hello World");
        REQUIRE(sv.FindFirstOf("aeiou", 0, 3) == 1);
    }

    SECTION("FindFirstOf() C-String")
    {
        TBasicStringView<char> sv("Hello");
        REQUIRE(sv.FindFirstOf("lo") == 2);
    }

    SECTION("FindFirstOf() Empty")
    {
        TBasicStringView<char> sv("test");
        REQUIRE(sv.FindFirstOf("") == TBasicStringView<char>::NPOS);
    }
}

TEST_CASE("BasicStringView FindLastOf", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("FindLastOf() String View")
    {
        TBasicStringView<char> sv("Hello World");
        REQUIRE(sv.FindLastOf("aeiou") == 7);
        REQUIRE(sv.FindLastOf("H") == 0);
        REQUIRE(sv.FindLastOf("xyz") == TBasicStringView<char>::NPOS);
    }

    SECTION("FindLastOf() Character")
    {
        TBasicStringView<char> sv("Hello");
        REQUIRE(sv.FindLastOf('l') == 3);
        REQUIRE(sv.FindLastOf('H') == 0);
    }

    SECTION("FindLastOf() With Position")
    {
        TBasicStringView<char> sv("Hello World");
        REQUIRE(sv.FindLastOf("l", 3) == 3);
    }

    SECTION("FindLastOf() Empty")
    {
        TBasicStringView<char> sv("test");
        REQUIRE(sv.FindLastOf("") == TBasicStringView<char>::NPOS);
    }

    SECTION("FindLastOf() Empty Source")
    {
        TBasicStringView<char> sv;
        REQUIRE(sv.FindLastOf("abc") == TBasicStringView<char>::NPOS);
    }
}

TEST_CASE("BasicStringView FindFirstNotOf", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("FindFirstNotOf() String View")
    {
        TBasicStringView<char> sv("aaaabcd");
        REQUIRE(sv.FindFirstNotOf("a") == 4);
        REQUIRE(sv.FindFirstNotOf("ab") == 5);
    }

    SECTION("FindFirstNotOf() Character")
    {
        TBasicStringView<char> sv("aaaabc");
        REQUIRE(sv.FindFirstNotOf('a') == 4);
        REQUIRE(sv.FindFirstNotOf('x') == 0);
    }

    SECTION("FindFirstNotOf() Empty Set")
    {
        TBasicStringView<char> sv("test");
        REQUIRE(sv.FindFirstNotOf("") == 0);
    }

    SECTION("FindFirstNotOf() All Match")
    {
        TBasicStringView<char> sv("aaaa");
        REQUIRE(sv.FindFirstNotOf("a") == TBasicStringView<char>::NPOS);
    }

    SECTION("FindFirstNotOf() With Position")
    {
        TBasicStringView<char> sv("aaaabcd");
        REQUIRE(sv.FindFirstNotOf("a", 5) == 5);
    }
}

TEST_CASE("BasicStringView FindLastNotOf", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("FindLastNotOf() String View")
    {
        TBasicStringView<char> sv("abcdaaaa");
        REQUIRE(sv.FindLastNotOf("a") == 3);
        REQUIRE(sv.FindLastNotOf("ab") == 3);
    }

    SECTION("FindLastNotOf() Character")
    {
        TBasicStringView<char> sv("abcaaa");
        REQUIRE(sv.FindLastNotOf('a') == 2);
    }

    SECTION("FindLastNotOf() Empty Set")
    {
        TBasicStringView<char> sv("test");
        REQUIRE(sv.FindLastNotOf("") == 3);
    }

    SECTION("FindLastNotOf() All Match")
    {
        TBasicStringView<char> sv("aaaa");
        REQUIRE(sv.FindLastNotOf("a") == TBasicStringView<char>::NPOS);
    }

    SECTION("FindLastNotOf() Empty Source")
    {
        TBasicStringView<char> sv;
        REQUIRE(sv.FindLastNotOf("a") == TBasicStringView<char>::NPOS);
    }
}

TEST_CASE("BasicStringView Iterators", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("Forward Iterators")
    {
        TBasicStringView<char> sv("abc");
        auto it = sv.begin();
        REQUIRE(*it == 'a');
        ++it;
        REQUIRE(*it == 'b');
        ++it;
        REQUIRE(*it == 'c');
        ++it;
        REQUIRE(it == sv.end());
    }

    SECTION("Const Iterators")
    {
        TBasicStringView<char> sv("xyz");
        auto it = sv.cbegin();
        REQUIRE(*it == 'x');
        REQUIRE(sv.cend() - sv.cbegin() == 3);
    }

    SECTION("Reverse Iterators")
    {
        TBasicStringView<char> sv("abc");
        auto it = sv.rbegin();
        REQUIRE(*it == 'c');
        ++it;
        REQUIRE(*it == 'b');
        ++it;
        REQUIRE(*it == 'a');
        ++it;
        REQUIRE(it == sv.rend());
    }

    SECTION("Const Reverse Iterators")
    {
        TBasicStringView<char> sv("xyz");
        auto it = sv.crbegin();
        REQUIRE(*it == 'z');
    }

    SECTION("Range-Based For Loop")
    {
        TBasicStringView<char> sv("test");
        std::string result;
        for (char c: sv) { result += c; }
        REQUIRE(result == "test");
    }
}

TEST_CASE("BasicStringView Conversion", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("To std::string_view")
    {
        TBasicStringView<char> sv("conversion");
        std::string_view stdView = sv;
        REQUIRE(stdView.data() == sv.Data());
        REQUIRE(stdView.size() == sv.Size());
    }
}

TEST_CASE("BasicStringView Hash", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("GetHash() Consistency")
    {
        TBasicStringView<char> sv1("hash test");
        TBasicStringView<char> sv2("hash test");
        REQUIRE(sv1.GetHash() == sv2.GetHash());
    }

    SECTION("GetHash() Different Strings")
    {
        TBasicStringView<char> sv1("first");
        TBasicStringView<char> sv2("second");
        REQUIRE(sv1.GetHash() != sv2.GetHash());
    }

    SECTION("GetHash() Empty String")
    {
        TBasicStringView<char> sv("");
        auto hash = sv.GetHash();
        REQUIRE(hash != 0);
    }
}

TEST_CASE("BasicStringView Edge Cases", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("Empty String Operations")
    {
        TBasicStringView<char> sv("");
        REQUIRE(sv.Size() == 0);
        REQUIRE(sv.IsEmpty());
        REQUIRE(sv.Find("") == 0);
        REQUIRE(sv.Find("a") == TBasicStringView<char>::NPOS);
    }

    SECTION("Single Character")
    {
        TBasicStringView<char> sv("x");
        REQUIRE(sv.Size() == 1);
        REQUIRE(sv[0] == 'x');
        REQUIRE(sv.Front() == 'x');
        REQUIRE(sv.Back() == 'x');
    }

    SECTION("Binary Data with Nulls")
    {
        const char data[] = { 'a', '\0', 'b', '\0' };
        TBasicStringView<char> sv(data, 4);
        REQUIRE(sv.Size() == 4);
        REQUIRE(sv[1] == '\0');
        REQUIRE(sv[3] == '\0');
    }

    SECTION("Very Long String")
    {
        std::string longStr(10000, 'x');
        TBasicStringView<char> sv(longStr.data(), longStr.size());
        REQUIRE(sv.Size() == 10000);
        REQUIRE(sv[9999] == 'x');
    }
}

TEST_CASE("BasicStringView Type Aliases", "[GP][Core][Container][BasicStringView]")
{
    using namespace GP::Container;

    SECTION("FStringView")
    {
        FStringView sv("test");
        REQUIRE(sv.Size() == 4);
    }

    SECTION("Different Character Types")
    {
        TBasicStringView<GP::WideChar> wsv(L"wide");
        REQUIRE(wsv.Size() == 4);
    }
}
