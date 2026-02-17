// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/BasicString.hpp"
#include "Container/ContainerForward.hpp"
#include <catch2/catch_test_macros.hpp>
#include <compare>
#include <cstring>

using namespace GP;
using namespace GP::Container;

TEST_CASE("TBasicString SSO Layout", "[GP][Core][Container][TBasicString]")
{
    SECTION("Size is 3 pointers plus allocator")
    {
        // The union should be the size of the HeapData struct (3 * sizeof(void*))
        // plus the allocator (which is stateless/empty but takes at least 1 byte + padding)
        REQUIRE(sizeof(TBasicString<char>) <= 32);
    }

    SECTION("SSO capacity for char is at least 22")
    {
        // On 64-bit: union = 24 bytes, SSO_CAPACITY = 24/1 - 1 = 23
        TBasicString<char> s;
        REQUIRE(s.Capacity() >= 22);
    }

    SECTION("Default constructed string uses SSO")
    {
        TBasicString<char> s;
        REQUIRE(s.IsSSO());
    }
}

TEST_CASE("TBasicString Construction", "[GP][Core][Container][TBasicString]")
{
    SECTION("Default Constructor")
    {
        TBasicString<char> s;
        REQUIRE(s.Size() == 0);
        REQUIRE(s.IsEmpty());
        REQUIRE(s.Data() != nullptr);
        REQUIRE(s.Data()[0] == '\0');
        REQUIRE(s.IsSSO());
    }

    SECTION("From C-String (SSO)")
    {
        TBasicString<char> s("Hello");
        REQUIRE(s.Size() == 5);
        REQUIRE_FALSE(s.IsEmpty());
        REQUIRE(std::strcmp(s.CStr(), "Hello") == 0);
        REQUIRE(s.IsSSO());
    }

    SECTION("From C-String (Heap)")
    {
        const char* longStr = "This is a very long string that definitely exceeds the SSO buffer capacity!!!";
        TBasicString<char> s(longStr);
        REQUIRE(s.Size() == std::strlen(longStr));
        REQUIRE(std::strcmp(s.CStr(), longStr) == 0);
        REQUIRE_FALSE(s.IsSSO());
    }

    SECTION("From nullptr")
    {
        TBasicString<char> s(static_cast<const char*>(nullptr));
        REQUIRE(s.Size() == 0);
        REQUIRE(s.IsEmpty());
        REQUIRE(s.IsSSO());
    }

    SECTION("From Pointer and Length (SSO)")
    {
        TBasicString<char> s("Hello World", 5);
        REQUIRE(s.Size() == 5);
        REQUIRE(std::strcmp(s.CStr(), "Hello") == 0);
        REQUIRE(s.IsSSO());
    }

    SECTION("From Pointer and Length (Heap)")
    {
        const char* longStr = "This is a very long string that definitely exceeds SSO";
        SizeT len = std::strlen(longStr);
        TBasicString<char> s(longStr, len);
        REQUIRE(s.Size() == len);
        REQUIRE(std::strcmp(s.CStr(), longStr) == 0);
        REQUIRE_FALSE(s.IsSSO());
    }

    SECTION("From Count and Character (SSO)")
    {
        TBasicString<char> s(5, 'x');
        REQUIRE(s.Size() == 5);
        REQUIRE(std::strcmp(s.CStr(), "xxxxx") == 0);
        REQUIRE(s.IsSSO());
    }

    SECTION("From Count and Character (Heap)")
    {
        TBasicString<char> s(100, 'y');
        REQUIRE(s.Size() == 100);
        for (SizeT i = 0; i < 100; ++i) { REQUIRE(s[i] == 'y'); }
        REQUIRE_FALSE(s.IsSSO());
    }

    SECTION("From StringView")
    {
        TBasicStringView<char> view("TestView");
        TBasicString<char> s(view);
        REQUIRE(s.Size() == 8);
        REQUIRE(std::strcmp(s.CStr(), "TestView") == 0);
    }

    SECTION("From Initializer List")
    {
        TBasicString<char> s({ 'H', 'i', '!' });
        REQUIRE(s.Size() == 3);
        REQUIRE(std::strcmp(s.CStr(), "Hi!") == 0);
    }

    SECTION("Copy Constructor (SSO)")
    {
        TBasicString<char> original("Hello");
        TBasicString<char> copy(original);
        REQUIRE(copy.Size() == original.Size());
        REQUIRE(std::strcmp(copy.CStr(), original.CStr()) == 0);
        REQUIRE(copy.IsSSO());
        // Ensure independent copies
        REQUIRE(copy.Data() != original.Data());
    }

    SECTION("Copy Constructor (Heap)")
    {
        const char* longStr = "This is a very long string that is on the heap for sure!!!!";
        TBasicString<char> original(longStr);
        TBasicString<char> copy(original);
        REQUIRE(copy.Size() == original.Size());
        REQUIRE(std::strcmp(copy.CStr(), original.CStr()) == 0);
        REQUIRE_FALSE(copy.IsSSO());
        REQUIRE(copy.Data() != original.Data());
    }

    SECTION("Move Constructor (SSO)")
    {
        TBasicString<char> original("Hello");
        const char* origData = original.Data();
        TBasicString<char> moved(std::move(original));

        REQUIRE(moved.Size() == 5);
        REQUIRE(std::strcmp(moved.CStr(), "Hello") == 0);
        REQUIRE(moved.IsSSO());
        // Original should be empty
        REQUIRE(original.Size() == 0);
        REQUIRE(original.IsEmpty());
        // SSO data is copied, not transferred
        REQUIRE(moved.Data() != origData);
    }

    SECTION("Move Constructor (Heap)")
    {
        const char* longStr = "This is a very long string that is on the heap for sure!!!!";
        TBasicString<char> original(longStr);
        const char* origData = original.Data();
        TBasicString<char> moved(std::move(original));

        REQUIRE(moved.Size() == std::strlen(longStr));
        REQUIRE(std::strcmp(moved.CStr(), longStr) == 0);
        REQUIRE_FALSE(moved.IsSSO());
        // Pointer should be stolen
        REQUIRE(moved.Data() == origData);
        // Original should be empty SSO
        REQUIRE(original.Size() == 0);
        REQUIRE(original.IsSSO());
    }
}

TEST_CASE("TBasicString Assignment", "[GP][Core][Container][TBasicString]")
{
    SECTION("Copy Assignment")
    {
        TBasicString<char> a("Hello");
        TBasicString<char> b("World!");
        b = a;
        REQUIRE(std::strcmp(b.CStr(), "Hello") == 0);
        REQUIRE(b.Size() == 5);
    }

    SECTION("Copy Assignment Self")
    {
        TBasicString<char> a("Hello");
        a = a;
        REQUIRE(std::strcmp(a.CStr(), "Hello") == 0);
    }

    SECTION("Move Assignment")
    {
        TBasicString<char> a("Hello");
        TBasicString<char> b("World!");
        b = std::move(a);
        REQUIRE(std::strcmp(b.CStr(), "Hello") == 0);
        REQUIRE(a.IsEmpty());
    }

    SECTION("Move Assignment (Heap to Heap)")
    {
        const char* longStr1 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        const char* longStr2 = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
        TBasicString<char> a(longStr1);
        TBasicString<char> b(longStr2);
        const char* aData = a.Data();
        b = std::move(a);
        REQUIRE(b.Data() == aData);
        REQUIRE(std::strcmp(b.CStr(), longStr1) == 0);
        REQUIRE(a.IsEmpty());
    }

    SECTION("Assignment from C-String")
    {
        TBasicString<char> s;
        s = "Hello";
        REQUIRE(std::strcmp(s.CStr(), "Hello") == 0);
    }

    SECTION("Assignment from nullptr")
    {
        TBasicString<char> s("Hello");
        s = static_cast<const char*>(nullptr);
        REQUIRE(s.IsEmpty());
    }

    SECTION("Assignment from Single Character")
    {
        TBasicString<char> s("Hello");
        s = 'X';
        REQUIRE(s.Size() == 1);
        REQUIRE(s[0] == 'X');
    }

    SECTION("Assignment from StringView")
    {
        TBasicString<char> s;
        TBasicStringView<char> view("FromView");
        s = view;
        REQUIRE(std::strcmp(s.CStr(), "FromView") == 0);
    }

    SECTION("Assignment from Initializer List")
    {
        TBasicString<char> s;
        s = { 'A', 'B', 'C' };
        REQUIRE(s.Size() == 3);
        REQUIRE(std::strcmp(s.CStr(), "ABC") == 0);
    }

    SECTION("Assignment Heap Reuse")
    {
        const char* longStr = "This is a very long string that exceeds SSO capacity!!!!!";
        TBasicString<char> s(longStr);
        const char* heapPtr = s.Data();
        SizeT heapCap = s.Capacity();

        // Assign a shorter (but still heap) string - should reuse buffer
        const char* shorterLong = "Still long enough to be on the heap!!!!!!!!!!!";
        s = shorterLong;
        if (std::strlen(shorterLong) <= heapCap) { REQUIRE(s.Data() == heapPtr); }
        REQUIRE(std::strcmp(s.CStr(), shorterLong) == 0);
    }
}

TEST_CASE("TBasicString Element Access", "[GP][Core][Container][TBasicString]")
{
    SECTION("Operator[]")
    {
        TBasicString<char> s("ABCDE");
        REQUIRE(s[0] == 'A');
        REQUIRE(s[4] == 'E');
        s[2] = 'Z';
        REQUIRE(s[2] == 'Z');
    }

    SECTION("At()")
    {
        TBasicString<char> s("Hello");
        REQUIRE(s.At(0) == 'H');
        REQUIRE(s.At(4) == 'o');
    }

    SECTION("Front() and Back()")
    {
        TBasicString<char> s("Hello");
        REQUIRE(s.Front() == 'H');
        REQUIRE(s.Back() == 'o');
        s.Front() = 'J';
        REQUIRE(s.Front() == 'J');
    }

    SECTION("Data() returns null-terminated string")
    {
        TBasicString<char> s("Test");
        REQUIRE(s.Data()[4] == '\0');
    }

    SECTION("CStr() same as Data()")
    {
        TBasicString<char> s("Test");
        REQUIRE(s.CStr() == s.Data());
    }

    SECTION("Const Element Access")
    {
        const TBasicString<char> s("Hello");
        REQUIRE(s[0] == 'H');
        REQUIRE(s.At(0) == 'H');
        REQUIRE(s.Front() == 'H');
        REQUIRE(s.Back() == 'o');
    }
}

TEST_CASE("TBasicString Size and Capacity", "[GP][Core][Container][TBasicString]")
{
    SECTION("Size() and Length() are the same")
    {
        TBasicString<char> s("Hello");
        REQUIRE(s.Size() == s.Length());
    }

    SECTION("IsEmpty()")
    {
        TBasicString<char> empty;
        TBasicString<char> notEmpty("Hi");
        REQUIRE(empty.IsEmpty());
        REQUIRE_FALSE(notEmpty.IsEmpty());
    }

    SECTION("Capacity() for SSO")
    {
        TBasicString<char> s;
        REQUIRE(s.Capacity() >= 22);   // At least 22 on 64-bit
    }

    SECTION("Capacity() grows on heap")
    {
        TBasicString<char> s(100, 'x');
        REQUIRE(s.Capacity() >= 100);
    }

    SECTION("MaxSize()")
    {
        TBasicString<char> s;
        REQUIRE(s.MaxSize() > 0);
    }
}

TEST_CASE("TBasicString Capacity Management", "[GP][Core][Container][TBasicString]")
{
    SECTION("Reserve does not shrink")
    {
        TBasicString<char> s("Hello");
        SizeT oldCap = s.Capacity();
        s.Reserve(1);
        REQUIRE(s.Capacity() >= oldCap);
    }

    SECTION("Reserve transitions to heap")
    {
        TBasicString<char> s("Hi");
        REQUIRE(s.IsSSO());
        s.Reserve(100);
        REQUIRE_FALSE(s.IsSSO());
        REQUIRE(s.Capacity() >= 100);
        REQUIRE(std::strcmp(s.CStr(), "Hi") == 0);
    }

    SECTION("ShrinkToFit - Heap to SSO")
    {
        TBasicString<char> s("Hi");
        s.Reserve(100);   // Force to heap
        REQUIRE_FALSE(s.IsSSO());
        s.ShrinkToFit();
        REQUIRE(s.IsSSO());
        REQUIRE(std::strcmp(s.CStr(), "Hi") == 0);
    }

    SECTION("ShrinkToFit - Heap stays Heap")
    {
        const char* longStr = "This is a very long string that won't fit in SSO at all!!";
        TBasicString<char> s(longStr);
        s.Reserve(500);
        SizeT oldCap = s.Capacity();
        s.ShrinkToFit();
        REQUIRE(s.Capacity() < oldCap);
        REQUIRE(s.Capacity() == s.Size());
        REQUIRE(std::strcmp(s.CStr(), longStr) == 0);
    }

    SECTION("Clear resets to empty")
    {
        TBasicString<char> s("Hello");
        s.Clear();
        REQUIRE(s.IsEmpty());
        REQUIRE(s.Size() == 0);
        REQUIRE(s.Data()[0] == '\0');
    }

    SECTION("Clear preserves heap buffer")
    {
        const char* longStr = "This is a very long string that is on the heap for sure!!!!";
        TBasicString<char> s(longStr);
        REQUIRE_FALSE(s.IsSSO());
        SizeT oldCap = s.Capacity();
        const char* oldData = s.Data();
        s.Clear();
        REQUIRE(s.IsEmpty());
        REQUIRE_FALSE(s.IsSSO());
        REQUIRE(s.Capacity() == oldCap);
        REQUIRE(s.Data() == oldData);
    }
}

TEST_CASE("TBasicString PushBack and PopBack", "[GP][Core][Container][TBasicString]")
{
    SECTION("PushBack character")
    {
        TBasicString<char> s("AB");
        s.PushBack('C');
        REQUIRE(s.Size() == 3);
        REQUIRE(std::strcmp(s.CStr(), "ABC") == 0);
    }

    SECTION("PushBack causes SSO to heap transition")
    {
        TBasicString<char> s;
        SizeT ssoCap = s.Capacity();
        for (SizeT i = 0; i <= ssoCap; ++i) { s.PushBack('x'); }
        REQUIRE(s.Size() == ssoCap + 1);
        REQUIRE_FALSE(s.IsSSO());
    }

    SECTION("PopBack")
    {
        TBasicString<char> s("ABC");
        s.PopBack();
        REQUIRE(s.Size() == 2);
        REQUIRE(std::strcmp(s.CStr(), "AB") == 0);
    }
}

TEST_CASE("TBasicString Append", "[GP][Core][Container][TBasicString]")
{
    SECTION("Append string")
    {
        TBasicString<char> a("Hello");
        TBasicString<char> b(" World");
        a.Append(b);
        REQUIRE(std::strcmp(a.CStr(), "Hello World") == 0);
    }

    SECTION("Append C-string")
    {
        TBasicString<char> s("Hello");
        s.Append(" World");
        REQUIRE(std::strcmp(s.CStr(), "Hello World") == 0);
    }

    SECTION("Append pointer and count")
    {
        TBasicString<char> s("Hello");
        s.Append(" World!", 6);
        REQUIRE(std::strcmp(s.CStr(), "Hello World") == 0);
    }

    SECTION("Append count and char")
    {
        TBasicString<char> s("Hi");
        s.Append(3, '!');
        REQUIRE(std::strcmp(s.CStr(), "Hi!!!") == 0);
    }

    SECTION("Append view")
    {
        TBasicString<char> s("Hello");
        TBasicStringView<char> view(" World");
        s.Append(view);
        REQUIRE(std::strcmp(s.CStr(), "Hello World") == 0);
    }

    SECTION("Append causes growth")
    {
        TBasicString<char> s("Short");
        REQUIRE(s.IsSSO());
        const char* longSuffix = " and now a very long suffix that exceeds SSO!!!!!";
        s.Append(longSuffix);
        REQUIRE_FALSE(s.IsSSO());
        REQUIRE(s.StartsWith("Short and now"));
    }

    SECTION("operator+=")
    {
        TBasicString<char> s("Hello");
        s += " World";
        REQUIRE(std::strcmp(s.CStr(), "Hello World") == 0);

        s += '!';
        REQUIRE(std::strcmp(s.CStr(), "Hello World!") == 0);
    }
}

TEST_CASE("TBasicString Insert", "[GP][Core][Container][TBasicString]")
{
    SECTION("Insert C-string at beginning")
    {
        TBasicString<char> s("World");
        s.Insert(0, "Hello ");
        REQUIRE(std::strcmp(s.CStr(), "Hello World") == 0);
    }

    SECTION("Insert C-string at end")
    {
        TBasicString<char> s("Hello");
        s.Insert(5, " World");
        REQUIRE(std::strcmp(s.CStr(), "Hello World") == 0);
    }

    SECTION("Insert C-string in middle")
    {
        TBasicString<char> s("Hlo");
        s.Insert(1, "el");
        REQUIRE(std::strcmp(s.CStr(), "Hello") == 0);
    }

    SECTION("Insert with pointer and count")
    {
        TBasicString<char> s("AC");
        s.Insert(1, "BXY", 1);
        REQUIRE(std::strcmp(s.CStr(), "ABC") == 0);
    }

    SECTION("Insert repeated character")
    {
        TBasicString<char> s("AC");
        s.Insert(1, 3, 'B');
        REQUIRE(std::strcmp(s.CStr(), "ABBBC") == 0);
    }

    SECTION("Insert view")
    {
        TBasicString<char> s("AD");
        TBasicStringView<char> view("BC");
        s.Insert(1, view);
        REQUIRE(std::strcmp(s.CStr(), "ABCD") == 0);
    }
}

TEST_CASE("TBasicString Erase", "[GP][Core][Container][TBasicString]")
{
    SECTION("Erase from beginning")
    {
        TBasicString<char> s("Hello World");
        s.Erase(0, 6);
        REQUIRE(std::strcmp(s.CStr(), "World") == 0);
    }

    SECTION("Erase from middle")
    {
        TBasicString<char> s("Hello World");
        s.Erase(5, 1);
        REQUIRE(std::strcmp(s.CStr(), "HelloWorld") == 0);
    }

    SECTION("Erase to end")
    {
        TBasicString<char> s("Hello World");
        s.Erase(5);
        REQUIRE(std::strcmp(s.CStr(), "Hello") == 0);
    }

    SECTION("Erase all")
    {
        TBasicString<char> s("Hello");
        s.Erase();
        REQUIRE(s.IsEmpty());
    }

    SECTION("Erase zero characters")
    {
        TBasicString<char> s("Hello");
        s.Erase(2, 0);
        REQUIRE(std::strcmp(s.CStr(), "Hello") == 0);
    }
}

TEST_CASE("TBasicString Replace", "[GP][Core][Container][TBasicString]")
{
    SECTION("Replace with same length")
    {
        TBasicString<char> s("Hello World");
        s.Replace(0, 5, "Goodbye", 7);
        REQUIRE(std::strcmp(s.CStr(), "Goodbye World") == 0);
    }

    SECTION("Replace with shorter")
    {
        TBasicString<char> s("Hello World");
        s.Replace(0, 5, "Hi");
        REQUIRE(std::strcmp(s.CStr(), "Hi World") == 0);
    }

    SECTION("Replace with longer")
    {
        TBasicString<char> s("Hi World");
        s.Replace(0, 2, "Hello");
        REQUIRE(std::strcmp(s.CStr(), "Hello World") == 0);
    }

    SECTION("Replace with view")
    {
        TBasicString<char> s("Hello World");
        TBasicStringView<char> view("Earth");
        s.Replace(6, 5, view);
        REQUIRE(std::strcmp(s.CStr(), "Hello Earth") == 0);
    }
}

TEST_CASE("TBasicString Resize", "[GP][Core][Container][TBasicString]")
{
    SECTION("Resize larger with default fill")
    {
        TBasicString<char> s("Hi");
        s.Resize(5);
        REQUIRE(s.Size() == 5);
        REQUIRE(s[0] == 'H');
        REQUIRE(s[1] == 'i');
        REQUIRE(s[2] == '\0');
    }

    SECTION("Resize larger with fill character")
    {
        TBasicString<char> s("Hi");
        s.Resize(5, 'x');
        REQUIRE(s.Size() == 5);
        REQUIRE(s[0] == 'H');
        REQUIRE(s[1] == 'i');
        REQUIRE(s[2] == 'x');
        REQUIRE(s[3] == 'x');
        REQUIRE(s[4] == 'x');
    }

    SECTION("Resize smaller")
    {
        TBasicString<char> s("Hello World");
        s.Resize(5);
        REQUIRE(s.Size() == 5);
        REQUIRE(std::strcmp(s.CStr(), "Hello") == 0);
    }

    SECTION("Resize same")
    {
        TBasicString<char> s("Hello");
        s.Resize(5);
        REQUIRE(s.Size() == 5);
        REQUIRE(std::strcmp(s.CStr(), "Hello") == 0);
    }
}

TEST_CASE("TBasicString Swap", "[GP][Core][Container][TBasicString]")
{
    SECTION("Swap two SSO strings")
    {
        TBasicString<char> a("Hello");
        TBasicString<char> b("World");
        a.Swap(b);
        REQUIRE(std::strcmp(a.CStr(), "World") == 0);
        REQUIRE(std::strcmp(b.CStr(), "Hello") == 0);
    }

    SECTION("Swap SSO with Heap")
    {
        TBasicString<char> a("Hi");
        const char* longStr = "This is a long string that lives on the heap for sure!!!!!";
        TBasicString<char> b(longStr);
        a.Swap(b);
        REQUIRE(std::strcmp(a.CStr(), longStr) == 0);
        REQUIRE(std::strcmp(b.CStr(), "Hi") == 0);
    }

    SECTION("Swap two Heap strings")
    {
        const char* str1 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        const char* str2 = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
        TBasicString<char> a(str1);
        TBasicString<char> b(str2);
        a.Swap(b);
        REQUIRE(std::strcmp(a.CStr(), str2) == 0);
        REQUIRE(std::strcmp(b.CStr(), str1) == 0);
    }
}

TEST_CASE("TBasicString Search", "[GP][Core][Container][TBasicString]")
{
    TBasicString<char> s("Hello World Hello");

    SECTION("Find substring")
    {
        REQUIRE(s.Find("World") == 6);
        REQUIRE(s.Find("Hello") == 0);
        REQUIRE(s.Find("Hello", 1) == 12);
        REQUIRE(s.Find("Missing") == TBasicString<char>::NPOS);
    }

    SECTION("Find character")
    {
        REQUIRE(s.Find('W') == 6);
        REQUIRE(s.Find('o') == 4);
        REQUIRE(s.Find('o', 5) == 7);
        REQUIRE(s.Find('Z') == TBasicString<char>::NPOS);
    }

    SECTION("RFind substring")
    {
        REQUIRE(s.RFind("Hello") == 12);
        REQUIRE(s.RFind("World") == 6);
    }

    SECTION("RFind character")
    {
        REQUIRE(s.RFind('o') == 16);
        REQUIRE(s.RFind('H') == 12);
    }

    SECTION("FindFirstOf")
    {
        TBasicStringView<char> chars("dW");
        REQUIRE(s.FindFirstOf(chars) == 6);
        REQUIRE(s.FindFirstOf('l') == 2);
    }

    SECTION("FindLastOf")
    {
        TBasicStringView<char> chars("dW");
        REQUIRE(s.FindLastOf(chars) == 10);
        REQUIRE(s.FindLastOf('l') == 15);
    }

    SECTION("FindFirstNotOf")
    {
        TBasicString<char> s2("aaabcd");
        REQUIRE(s2.FindFirstNotOf('a') == 3);
        TBasicStringView<char> chars("abc");
        REQUIRE(s2.FindFirstNotOf(chars) == 5);
    }

    SECTION("FindLastNotOf")
    {
        TBasicString<char> s2("abcddd");
        REQUIRE(s2.FindLastNotOf('d') == 2);
    }

    SECTION("Contains")
    {
        REQUIRE(s.Contains("World"));
        REQUIRE(s.Contains('H'));
        REQUIRE_FALSE(s.Contains("Missing"));
        REQUIRE_FALSE(s.Contains('Z'));
    }

    SECTION("StartsWith")
    {
        REQUIRE(s.StartsWith("Hello"));
        REQUIRE(s.StartsWith('H'));
        REQUIRE_FALSE(s.StartsWith("World"));
        REQUIRE_FALSE(s.StartsWith('W'));
    }

    SECTION("EndsWith")
    {
        REQUIRE(s.EndsWith("Hello"));
        REQUIRE(s.EndsWith('o'));
        REQUIRE_FALSE(s.EndsWith("World"));
    }
}

TEST_CASE("TBasicString String Operations", "[GP][Core][Container][TBasicString]")
{
    SECTION("Substr")
    {
        TBasicString<char> s("Hello World");
        TBasicString<char> sub = s.Substr(6);
        REQUIRE(std::strcmp(sub.CStr(), "World") == 0);

        TBasicString<char> sub2 = s.Substr(0, 5);
        REQUIRE(std::strcmp(sub2.CStr(), "Hello") == 0);
    }

    SECTION("Compare")
    {
        TBasicString<char> a("abc");
        TBasicString<char> b("abc");
        TBasicString<char> c("abd");
        TBasicString<char> d("ab");

        REQUIRE(a.Compare(TBasicStringView<char>(b)) == 0);
        REQUIRE(a.Compare(TBasicStringView<char>(c)) < 0);
        REQUIRE(c.Compare(TBasicStringView<char>(a)) > 0);
        REQUIRE(a.Compare(TBasicStringView<char>(d)) > 0);
    }

    SECTION("GetHash")
    {
        TBasicString<char> a("Hello");
        TBasicString<char> b("Hello");
        TBasicString<char> c("World");
        REQUIRE(a.GetHash() == b.GetHash());
        REQUIRE(a.GetHash() != c.GetHash());
    }
}

TEST_CASE("TBasicString Conversion", "[GP][Core][Container][TBasicString]")
{
    SECTION("Implicit conversion to StringView")
    {
        TBasicString<char> s("Hello");
        TBasicStringView<char> view = s;
        REQUIRE(view.Size() == 5);
        REQUIRE(view.Data() == s.Data());
    }

    SECTION("Explicit construction from StringView")
    {
        TBasicStringView<char> view("Hello");
        TBasicString<char> s(view);
        REQUIRE(std::strcmp(s.CStr(), "Hello") == 0);
    }
}

TEST_CASE("TBasicString Comparison Operators", "[GP][Core][Container][TBasicString]")
{
    SECTION("Equality with TBasicString")
    {
        TBasicString<char> a("Hello");
        TBasicString<char> b("Hello");
        TBasicString<char> c("World");
        REQUIRE(a == b);
        REQUIRE_FALSE(a == c);
    }

    SECTION("Equality with C-string")
    {
        TBasicString<char> s("Hello");
        REQUIRE(s == "Hello");
        REQUIRE_FALSE(s == "World");
    }

    SECTION("Equality with StringView")
    {
        TBasicString<char> s("Hello");
        TBasicStringView<char> view("Hello");
        REQUIRE(s == view);
    }

    SECTION("Three-way comparison")
    {
        TBasicString<char> a("abc");
        TBasicString<char> b("abd");
        TBasicString<char> c("abc");
        REQUIRE(std::is_lt(a <=> b));
        REQUIRE(std::is_gt(b <=> a));
        REQUIRE(std::is_eq(a <=> c));
    }

    SECTION("Ordering operators")
    {
        TBasicString<char> a("abc");
        TBasicString<char> b("abd");
        REQUIRE(a < b);
        REQUIRE(b > a);
        REQUIRE(a <= a);
        REQUIRE(a >= a);
        REQUIRE(a <= b);
        REQUIRE(b >= a);
    }
}

TEST_CASE("TBasicString Concatenation", "[GP][Core][Container][TBasicString]")
{
    SECTION("String + String")
    {
        TBasicString<char> a("Hello");
        TBasicString<char> b(" World");
        TBasicString<char> result = a + b;
        REQUIRE(std::strcmp(result.CStr(), "Hello World") == 0);
    }

    SECTION("String + C-string")
    {
        TBasicString<char> a("Hello");
        TBasicString<char> result = a + " World";
        REQUIRE(std::strcmp(result.CStr(), "Hello World") == 0);
    }

    SECTION("C-string + String")
    {
        TBasicString<char> b(" World");
        TBasicString<char> result = "Hello" + b;
        REQUIRE(std::strcmp(result.CStr(), "Hello World") == 0);
    }

    SECTION("String + char")
    {
        TBasicString<char> a("Hello");
        TBasicString<char> result = a + '!';
        REQUIRE(std::strcmp(result.CStr(), "Hello!") == 0);
    }

    SECTION("char + String")
    {
        TBasicString<char> b("ello");
        TBasicString<char> result = 'H' + b;
        REQUIRE(std::strcmp(result.CStr(), "Hello") == 0);
    }
}

TEST_CASE("TBasicString Iterators", "[GP][Core][Container][TBasicString]")
{
    SECTION("PascalCase iterators")
    {
        TBasicString<char> s("Hello");
        REQUIRE(*s.Begin() == 'H');
        REQUIRE(s.End() - s.Begin() == static_cast<std::ptrdiff_t>(5));
        REQUIRE(*s.CBegin() == 'H');
        REQUIRE(s.CEnd() - s.CBegin() == static_cast<std::ptrdiff_t>(5));
    }

    SECTION("Lowercase iterators (STL-compatible)")
    {
        TBasicString<char> s("Hello");
        REQUIRE(*s.begin() == 'H');
        REQUIRE(s.end() - s.begin() == static_cast<std::ptrdiff_t>(5));
    }

    SECTION("Range-for loop")
    {
        TBasicString<char> s("ABC");
        TBasicString<char> result;
        for (char c: s) { result.PushBack(c); }
        REQUIRE(result == "ABC");
    }

    SECTION("Reverse iterators")
    {
        TBasicString<char> s("ABC");
        TBasicString<char> reversed;
        for (auto it = s.rbegin(); it != s.rend(); ++it) { reversed.PushBack(*it); }
        REQUIRE(std::strcmp(reversed.CStr(), "CBA") == 0);
    }

    SECTION("PascalCase reverse iterators")
    {
        TBasicString<char> s("ABC");
        TBasicString<char> reversed;
        for (auto it = s.RBegin(); it != s.REnd(); ++it) { reversed.PushBack(*it); }
        REQUIRE(std::strcmp(reversed.CStr(), "CBA") == 0);
    }
}

TEST_CASE("TBasicString SSO/Heap Transitions", "[GP][Core][Container][TBasicString]")
{
    SECTION("SSO to Heap via Append")
    {
        TBasicString<char> s("Hi");
        REQUIRE(s.IsSSO());
        s.Append(100, 'x');
        REQUIRE_FALSE(s.IsSSO());
        REQUIRE(s.Size() == 102);
        REQUIRE(s[0] == 'H');
        REQUIRE(s[1] == 'i');
        REQUIRE(s[2] == 'x');
    }

    SECTION("Heap to SSO via ShrinkToFit after Erase")
    {
        TBasicString<char> s(100, 'x');
        REQUIRE_FALSE(s.IsSSO());
        s.Erase(2);   // Now size is 2
        s.ShrinkToFit();
        REQUIRE(s.IsSSO());
        REQUIRE(s.Size() == 2);
    }

    SECTION("SSO to Heap via Insert")
    {
        TBasicString<char> s("AB");
        REQUIRE(s.IsSSO());
        s.Insert(1, 100, 'x');
        REQUIRE_FALSE(s.IsSSO());
        REQUIRE(s.Size() == 102);
        REQUIRE(s[0] == 'A');
        REQUIRE(s[101] == 'B');
    }

    SECTION("SSO to Heap via Replace")
    {
        TBasicString<char> s("AB");
        REQUIRE(s.IsSSO());
        TBasicString<char> longReplace(100, 'x');
        s.Replace(1, 0, longReplace.CStr(), longReplace.Size());
        REQUIRE_FALSE(s.IsSSO());
    }

    SECTION("SSO to Heap via Resize")
    {
        TBasicString<char> s("AB");
        REQUIRE(s.IsSSO());
        s.Resize(100, 'z');
        REQUIRE_FALSE(s.IsSSO());
        REQUIRE(s.Size() == 100);
        REQUIRE(s[0] == 'A');
        REQUIRE(s[1] == 'B');
        REQUIRE(s[99] == 'z');
    }
}

TEST_CASE("TBasicString Type Aliases", "[GP][Core][Container][TBasicString]")
{
    SECTION("FString is TBasicString<char>")
    {
        FString s("Hello");
        REQUIRE(std::strcmp(s.CStr(), "Hello") == 0);
    }

    SECTION("FWString is TBasicString<wchar_t>")
    {
        FWString s(L"Wide");
        REQUIRE(s.Size() == 4);
    }
}

TEST_CASE("TBasicString Edge Cases", "[GP][Core][Container][TBasicString]")
{
    SECTION("Empty string operations")
    {
        TBasicString<char> s;
        REQUIRE(s.Find("anything") == TBasicString<char>::NPOS);
        REQUIRE(s.Contains(""));
        REQUIRE(s.StartsWith(""));
        REQUIRE(s.EndsWith(""));
        TBasicString<char> sub = s.Substr(0, 0);
        REQUIRE(sub.IsEmpty());
    }

    SECTION("Null termination after all operations")
    {
        TBasicString<char> s("Hello");
        s.PushBack('!');
        REQUIRE(s.Data()[s.Size()] == '\0');

        s.PopBack();
        REQUIRE(s.Data()[s.Size()] == '\0');

        s.Erase(2, 1);
        REQUIRE(s.Data()[s.Size()] == '\0');

        s.Insert(1, "XY");
        REQUIRE(s.Data()[s.Size()] == '\0');

        s.Replace(0, 2, "AB");
        REQUIRE(s.Data()[s.Size()] == '\0');

        s.Resize(3);
        REQUIRE(s.Data()[s.Size()] == '\0');

        s.Clear();
        REQUIRE(s.Data()[s.Size()] == '\0');
    }

    SECTION("SSO boundary - exact capacity fill")
    {
        TBasicString<char> s;
        SizeT cap = s.Capacity();
        // Fill exactly to SSO capacity
        for (SizeT i = 0; i < cap; ++i) { s.PushBack('a'); }
        REQUIRE(s.IsSSO());
        REQUIRE(s.Size() == cap);
        REQUIRE(s.Data()[cap] == '\0');

        // One more triggers heap
        s.PushBack('b');
        REQUIRE_FALSE(s.IsSSO());
        REQUIRE(s.Size() == cap + 1);
    }

    SECTION("Repeated Clear and Refill")
    {
        TBasicString<char> s;
        for (int i = 0; i < 10; ++i)
        {
            s = "Hello World";
            REQUIRE(std::strcmp(s.CStr(), "Hello World") == 0);
            s.Clear();
            REQUIRE(s.IsEmpty());
        }
    }

    SECTION("Self-append should work")
    {
        TBasicString<char> s("abc");
        // Self-append via view
        TBasicStringView<char> selfView = s;
        s.Append(selfView.Data(), selfView.Size());
        // Note: this may or may not work correctly depending on buffer reallocation
        // For SSO where the buffer doesn't move, it should be fine
        REQUIRE(s.Size() == 6);
    }
}
