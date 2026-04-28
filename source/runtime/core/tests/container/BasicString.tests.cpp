// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "container/BasicString.hpp"
#include "container/BasicStringView.hpp"
#include "container/Forward.hpp"
#include "CoreMinimal.hpp"
#include "memory/DefaultAllocator.hpp"
#include "memory/LinearAllocator.hpp"
#include "memory/PolymorphicAllocator.hpp"
#include <catch2/catch_test_macros.hpp>

namespace gp::tests
{

TEST_CASE("String - sizeof is exactly 32 bytes", "[container][String]")
{
    STATIC_REQUIRE(sizeof(gp::String) == 32);
}

TEST_CASE("String - SSO threshold is 23 characters", "[container][String]")
{
    // 23 chars should stay in SSO
    gp::String sso("12345678901234567890123");
    REQUIRE(sso.size() == 23);
    REQUIRE(sso.capacity() == 23);

    // 24 chars should hit the heap
    gp::String heap("123456789012345678901234");
    REQUIRE(heap.size() == 24);
    REQUIRE(heap.capacity() >= 24);
}

TEST_CASE("String - default construction", "[container][String]")
{
    gp::String s;
    REQUIRE(s.size() == 0);
    REQUIRE(s.empty());
    REQUIRE(s.capacity() == 23);
    REQUIRE(s[0] == '\0');
}

TEST_CASE("String - construction from C string", "[container][String]")
{
    gp::String s("hello");
    REQUIRE(s.size() == 5);
    REQUIRE(s == "hello");
    REQUIRE(s.cStr()[5] == '\0');
}

TEST_CASE("String - construction from nullptr", "[container][String]")
{
    gp::String s(static_cast<const char*>(nullptr));
    REQUIRE(s.empty());
}

TEST_CASE("String - construction from pointer and count", "[container][String]")
{
    gp::String s("hello world", 5);
    REQUIRE(s.size() == 5);
    REQUIRE(s == "hello");
}

TEST_CASE("String - construction with fill character", "[container][String]")
{
    gp::String s(10, 'x');
    REQUIRE(s.size() == 10);
    for (gp::USize i = 0; i < 10; ++i)
    {
        REQUIRE(s[i] == 'x');
    }
}

TEST_CASE("String - construction with fill character (heap)", "[container][String]")
{
    gp::String s(30, 'A');
    REQUIRE(s.size() == 30);
    for (gp::USize i = 0; i < 30; ++i)
    {
        REQUIRE(s[i] == 'A');
    }
}

TEST_CASE("String - initializer list construction", "[container][String]")
{
    gp::String s({ 'a', 'b', 'c' });
    REQUIRE(s.size() == 3);
    REQUIRE(s == "abc");
}

TEST_CASE("String - construction from StringView", "[container][String]")
{
    gp::StringView sv("hello world");
    gp::String s(sv);
    REQUIRE(s.size() == 11);
    REQUIRE(s == "hello world");
}

TEST_CASE("String - substring construction", "[container][String]")
{
    gp::String full("hello world");
    gp::String sub(full, 6);
    REQUIRE(sub == "world");
    REQUIRE(sub.size() == 5);

    gp::String sub2(full, 0, 5);
    REQUIRE(sub2 == "hello");
}

TEST_CASE("String - copy construction (SSO)", "[container][String]")
{
    gp::String a("short");
    gp::String b(a);
    REQUIRE(b == "short");
    REQUIRE(a.data() != b.data());
}

TEST_CASE("String - copy construction (heap)", "[container][String]")
{
    gp::String a("this is a long string that exceeds SSO");
    gp::String b(a);
    REQUIRE(b == a);
    REQUIRE(b.data() != a.data());
}

TEST_CASE("String - move construction", "[container][String]")
{
    gp::String a("hello");
    gp::String b(static_cast<gp::String&&>(a));
    REQUIRE(b == "hello");
    REQUIRE(a.empty());
}

TEST_CASE("String - move construction from heap", "[container][String]")
{
    gp::String a("this is a long string that exceeds SSO buffer");
    const char* old_data = a.data();
    gp::String b(static_cast<gp::String&&>(a));
    REQUIRE(b.data() == old_data);
    REQUIRE(a.empty());
}

TEST_CASE("String - copy assignment", "[container][String]")
{
    gp::String a("hello");
    gp::String b("world");
    b = a;
    REQUIRE(b == "hello");
}

TEST_CASE("String - move assignment", "[container][String]")
{
    gp::String a("hello");
    gp::String b;
    b = static_cast<gp::String&&>(a);
    REQUIRE(b == "hello");
    REQUIRE(a.empty());
}

TEST_CASE("String - self-copy assignment", "[container][String]")
{
    gp::String a("hello");
    a = a;
    REQUIRE(a == "hello");
}

TEST_CASE("String - assign from C string", "[container][String]")
{
    gp::String s;
    s = "hello";
    REQUIRE(s == "hello");
}

TEST_CASE("String - assign single char", "[container][String]")
{
    gp::String s;
    s = 'x';
    REQUIRE(s.size() == 1);
    REQUIRE(s[0] == 'x');
}

TEST_CASE("String - assign from StringView", "[container][String]")
{
    gp::String s;
    s = gp::StringView("test");
    REQUIRE(s == "test");
}

TEST_CASE("String - assign count and char", "[container][String]")
{
    gp::String s("something");
    s.assign(5, 'z');
    REQUIRE(s.size() == 5);
    REQUIRE(s == "zzzzz");
}

TEST_CASE("String - assign from substring", "[container][String]")
{
    gp::String a("hello world");
    gp::String b;
    b.assign(a, 6, 5);
    REQUIRE(b == "world");
}

TEST_CASE("String - operator+=", "[container][String]")
{
    gp::String s("hello");
    s += " world";
    REQUIRE(s == "hello world");

    s += gp::String("!");
    REQUIRE(s == "hello world!");

    s += '?';
    REQUIRE(s == "hello world!?");
}

TEST_CASE("String - operator+ concatenation", "[container][String]")
{
    gp::String a("hello");
    gp::String b(" world");
    gp::String c = a + b;
    REQUIRE(c == "hello world");

    gp::String d = a + " there";
    REQUIRE(d == "hello there");

    gp::String e = "hey " + b;
    REQUIRE(e == "hey  world");
}

TEST_CASE("String - equality and inequality", "[container][String]")
{
    gp::String a("hello");
    gp::String b("hello");
    gp::String c("world");
    REQUIRE(a == b);
    REQUIRE(a != c);
    REQUIRE(a == "hello");
    REQUIRE(a != "world");
}

TEST_CASE("String - spaceship operator", "[container][String]")
{
    gp::String a("abc");
    gp::String b("abd");
    CHECK((a <=> b) == std::strong_ordering::less);
    CHECK((b <=> a) == std::strong_ordering::greater);
    CHECK((a <=> a) == std::strong_ordering::equal);
    CHECK((a <=> "abd") == std::strong_ordering::less);
}

TEST_CASE("String - implicit conversion to StringView", "[container][String]")
{
    gp::String s("hello");
    gp::StringView sv = s;
    REQUIRE(sv.size() == 5);
    REQUIRE(sv == gp::StringView("hello"));
}

TEST_CASE("String - at / operator[]", "[container][String]")
{
    gp::String s("abcdef");
    REQUIRE(s.at(0) == 'a');
    REQUIRE(s.at(5) == 'f');
    REQUIRE(s[3] == 'd');
}

TEST_CASE("String - front and back", "[container][String]")
{
    gp::String s("abc");
    REQUIRE(s.front() == 'a');
    REQUIRE(s.back() == 'c');
}

TEST_CASE("String - data and cStr are null-terminated", "[container][String]")
{
    gp::String s("test");
    REQUIRE(s.data()[4] == '\0');
    REQUIRE(s.cStr()[4] == '\0');
}

TEST_CASE("String - iterators", "[container][String]")
{
    gp::String s("abc");
    REQUIRE(*s.begin() == 'a');
    REQUIRE(*(s.end() - 1) == 'c');

    gp::USize count = 0;
    for (char ch: s)
    {
        (void)ch;
        ++count;
    }
    REQUIRE(count == 3);
}

TEST_CASE("String - reverse iterators", "[container][String]")
{
    gp::String s("abc");
    REQUIRE(*s.rbegin() == 'c');
    REQUIRE(*(s.rend() - 1) == 'a');
}

TEST_CASE("String - reserve transitions SSO to heap", "[container][String]")
{
    gp::String s("hi");
    REQUIRE(s.capacity() == 23);   // SSO
    s.reserve(100);
    REQUIRE(s.capacity() >= 100);
    REQUIRE(s == "hi");
}

TEST_CASE("String - shrinkToFit back to SSO", "[container][String]")
{
    gp::String s("hi");
    s.reserve(100);
    REQUIRE(s.capacity() >= 100);
    s.shrinkToFit();
    REQUIRE(s.capacity() == 23);   // Back to SSO
    REQUIRE(s == "hi");
}

TEST_CASE("String - shrinkToFit on heap", "[container][String]")
{
    gp::String s(50, 'x');
    s.reserve(200);
    REQUIRE(s.capacity() >= 200);
    s.shrinkToFit();
    REQUIRE(s.capacity() == 50);
    REQUIRE(s.size() == 50);
}

TEST_CASE("String - clear", "[container][String]")
{
    gp::String s("hello");
    s.clear();
    REQUIRE(s.empty());
    REQUIRE(s[0] == '\0');
}

TEST_CASE("String - clear on heap preserves capacity", "[container][String]")
{
    gp::String s(50, 'x');
    gp::USize cap = s.capacity();
    s.clear();
    REQUIRE(s.empty());
    REQUIRE(s.capacity() == cap);
}

TEST_CASE("String - pushBack", "[container][String]")
{
    gp::String s;
    for (char c = 'a'; c <= 'z'; ++c)
    {
        s.pushBack(c);
    }
    REQUIRE(s.size() == 26);
    REQUIRE(s[0] == 'a');
    REQUIRE(s[25] == 'z');
}

TEST_CASE("String - pushBack triggers SSO to heap transition", "[container][String]")
{
    gp::String s(23, 'a');
    REQUIRE(s.capacity() == 23);
    s.pushBack('b');
    REQUIRE(s.size() == 24);
    REQUIRE(s.capacity() > 23);
    REQUIRE(s[23] == 'b');
}

TEST_CASE("String - popBack", "[container][String]")
{
    gp::String s("hello");
    s.popBack();
    REQUIRE(s == "hell");
    REQUIRE(s.size() == 4);
}

TEST_CASE("String - append string", "[container][String]")
{
    gp::String s("hello");
    s.append(" world");
    REQUIRE(s == "hello world");
}

TEST_CASE("String - append buffer", "[container][String]")
{
    gp::String s("ab");
    s.append("cdef", 2);
    REQUIRE(s == "abcd");
}

TEST_CASE("String - append fill", "[container][String]")
{
    gp::String s("ab");
    s.append(3, 'c');
    REQUIRE(s == "abccc");
}

TEST_CASE("String - append crossing SSO boundary", "[container][String]")
{
    gp::String s(20, 'a');
    s.append("bbbb");
    REQUIRE(s.size() == 24);
    REQUIRE(s[23] == 'b');
}

TEST_CASE("String - insert", "[container][String]")
{
    gp::String s("helo");
    s.insert(2, "l");
    REQUIRE(s == "hello");
}

TEST_CASE("String - insert fill", "[container][String]")
{
    gp::String s("hlo");
    s.insert(1, 2, 'e');
    REQUIRE(s == "heelo");
}

TEST_CASE("String - insert at beginning", "[container][String]")
{
    gp::String s("world");
    s.insert(0, "hello ");
    REQUIRE(s == "hello world");
}

TEST_CASE("String - erase", "[container][String]")
{
    gp::String s("hello world");
    s.erase(5, 6);
    REQUIRE(s == "hello");
}

TEST_CASE("String - erase to end", "[container][String]")
{
    gp::String s("hello world");
    s.erase(5);
    REQUIRE(s == "hello");
}

TEST_CASE("String - erase iterator", "[container][String]")
{
    gp::String s("hello");
    auto it = s.erase(s.begin() + 1);
    REQUIRE(*it == 'l');
    REQUIRE(s == "hllo");
}

TEST_CASE("String - erase iterator range", "[container][String]")
{
    gp::String s("hello world");
    s.erase(s.begin() + 5, s.end());
    REQUIRE(s == "hello");
}

TEST_CASE("String - replace", "[container][String]")
{
    gp::String s("hello world");
    s.replace(6, 5, "there");
    REQUIRE(s == "hello there");
}

TEST_CASE("String - replace with different length", "[container][String]")
{
    gp::String s("abc");
    s.replace(1, 1, "xyz");
    REQUIRE(s == "axyzc");
}

TEST_CASE("String - resize grow", "[container][String]")
{
    gp::String s("hi");
    s.resize(5, '!');
    REQUIRE(s.size() == 5);
    REQUIRE(s == gp::StringView("hi!!!", 5));
}

TEST_CASE("String - resize shrink", "[container][String]")
{
    gp::String s("hello world");
    s.resize(5);
    REQUIRE(s == "hello");
}

TEST_CASE("String - swap SSO strings", "[container][String]")
{
    gp::String a("hello");
    gp::String b("world");
    a.swap(b);
    REQUIRE(a == "world");
    REQUIRE(b == "hello");
}

TEST_CASE("String - swap heap strings", "[container][String]")
{
    gp::String a(30, 'a');
    gp::String b(30, 'b');
    const char* a_data = a.data();
    const char* b_data = b.data();
    a.swap(b);
    REQUIRE(a.data() == b_data);
    REQUIRE(b.data() == a_data);
}

TEST_CASE("String - swap SSO and heap", "[container][String]")
{
    gp::String sso("short");
    gp::String heap(30, 'x');
    sso.swap(heap);
    REQUIRE(sso.size() == 30);
    REQUIRE(heap == "short");
}

TEST_CASE("String - copy", "[container][String]")
{
    gp::String s("hello world");
    char buf[6] = {};
    gp::USize n = s.copy(buf, 5, 6);
    REQUIRE(n == 5);
    REQUIRE(gp::StringView(buf, 5) == gp::StringView("world"));
}

TEST_CASE("String - substr", "[container][String]")
{
    gp::String s("hello world");
    gp::String sub = s.substr(6);
    REQUIRE(sub == "world");

    gp::String sub2 = s.substr(0, 5);
    REQUIRE(sub2 == "hello");
}

TEST_CASE("String - compare", "[container][String]")
{
    gp::String a("abc");
    gp::String b("abd");
    REQUIRE(a.compare(b) < 0);
    REQUIRE(b.compare(a) > 0);
    REQUIRE(a.compare(a) == 0);
    REQUIRE(a.compare("abc") == 0);
}

TEST_CASE("String - find", "[container][String]")
{
    gp::String s("hello hello world");
    REQUIRE(s.find("hello") == 0);
    REQUIRE(s.find("hello", 1) == 6);
    REQUIRE(s.find('w') == 12);
    REQUIRE(s.find("xyz") == gp::String::npos);
}

TEST_CASE("String - rfind", "[container][String]")
{
    gp::String s("hello hello");
    REQUIRE(s.rfind("hello") == 6);
    REQUIRE(s.rfind("hello", 5) == 0);
    REQUIRE(s.rfind('o') == 10);
}

TEST_CASE("String - findFirstOf and findLastOf", "[container][String]")
{
    gp::String s("hello world");
    REQUIRE(s.findFirstOf("aeiou") == 1);
    REQUIRE(s.findLastOf("aeiou") == 7);
}

TEST_CASE("String - findFirstNotOf and findLastNotOf", "[container][String]")
{
    gp::String s("aaabbb");
    REQUIRE(s.findFirstNotOf("a") == 3);
    REQUIRE(s.findLastNotOf("b") == 2);
}

TEST_CASE("String - startsWith", "[container][String]")
{
    gp::String s("hello world");
    REQUIRE(s.startsWith("hello"));
    REQUIRE(s.startsWith('h'));
    REQUIRE_FALSE(s.startsWith("world"));
}

TEST_CASE("String - endsWith", "[container][String]")
{
    gp::String s("hello world");
    REQUIRE(s.endsWith("world"));
    REQUIRE(s.endsWith('d'));
    REQUIRE_FALSE(s.endsWith("hello"));
}

TEST_CASE("String - contains", "[container][String]")
{
    gp::String s("hello world");
    REQUIRE(s.contains("lo wo"));
    REQUIRE(s.contains('w'));
    REQUIRE_FALSE(s.contains("xyz"));
}

TEST_CASE("String - copy uses select_on_container_copy_construction", "[string][allocator]")
{
    alignas(16) gp::UInt8 buf[4096];
    gp::memory::LinearAllocator lin(buf, sizeof(buf));
    gp::memory::PolymorphicAllocator<char> custom(&lin);

    gp::String a("hello", custom);
    gp::String b(a);
    REQUIRE(b == "hello");
    REQUIRE(b.getAllocator().getResource() == &gp::memory::DefaultAllocator::get());
}

TEST_CASE("String - move transfers allocator", "[string][allocator]")
{
    alignas(16) gp::UInt8 buf[4096];
    gp::memory::LinearAllocator lin(buf, sizeof(buf));
    gp::memory::PolymorphicAllocator<char> custom(&lin);

    gp::String a("hello", custom);
    gp::String b(static_cast<gp::String&&>(a));
    REQUIRE(b.getAllocator().getResource() == &lin);
}

TEST_CASE("String - custom allocator with heap allocation", "[string][allocator]")
{
    alignas(16) gp::UInt8 buf[8192];
    gp::memory::LinearAllocator lin(buf, sizeof(buf));
    gp::memory::PolymorphicAllocator<char> alloc(&lin);

    gp::String s(50, 'x', alloc);
    REQUIRE(s.size() == 50);
    REQUIRE(lin.getAllocatedSize() > 0);
    for (gp::USize i = 0; i < 50; ++i)
    {
        REQUIRE(s[i] == 'x');
    }
}

TEST_CASE("String - exactly at SSO boundary (23 chars)", "[string][sso]")
{
    gp::String s("12345678901234567890123");
    REQUIRE(s.size() == 23);
    REQUIRE(s.capacity() == 23);   // Still in SSO
    REQUIRE(s.cStr()[23] == '\0');
}

TEST_CASE("String - one past SSO boundary (24 chars)", "[string][sso]")
{
    gp::String s("123456789012345678901234");
    REQUIRE(s.size() == 24);
    REQUIRE(s.capacity() >= 24);
    REQUIRE(s.cStr()[24] == '\0');
}

TEST_CASE("String - grow from SSO to heap via append", "[string][sso]")
{
    gp::String s(23, 'a');
    REQUIRE(s.capacity() == 23);
    s.append("b");
    REQUIRE(s.size() == 24);
    REQUIRE(s.capacity() > 23);
    REQUIRE(s[0] == 'a');
    REQUIRE(s[23] == 'b');
}

TEST_CASE("String - empty string is in SSO mode", "[string][sso]")
{
    gp::String s;
    REQUIRE(s.capacity() == 23);
    REQUIRE(s.data() != nullptr);
}

TEST_CASE("String - repeated pushBack across SSO boundary", "[container][String]")
{
    gp::String s;
    for (int i = 0; i < 200; ++i)
    {
        s.pushBack(static_cast<char>('a' + (i % 26)));
    }
    REQUIRE(s.size() == 200);
    for (int i = 0; i < 200; ++i)
    {
        REQUIRE(s[i] == static_cast<char>('a' + (i % 26)));
    }
}

}   // namespace gp::tests
