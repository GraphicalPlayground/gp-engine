// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Span.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

TEST_CASE("TSpan Construction", "[GP][Core][Container][Span][Construction]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Default Constructor - Dynamic Extent")
    {
        TSpan<int> span;
        REQUIRE(span.Data() == nullptr);
        REQUIRE(span.Size() == 0);
        REQUIRE(span.IsEmpty());
    }

    SECTION("Default Constructor - Zero Static Extent")
    {
        TSpan<int, 0> span;
        REQUIRE(span.Data() == nullptr);
        REQUIRE(span.Size() == 0);
        REQUIRE(span.IsEmpty());
    }

    SECTION("Constructor from Pointer and Size")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span(arr, 5);
        REQUIRE(span.Data() == arr);
        REQUIRE(span.Size() == 5);
        REQUIRE_FALSE(span.IsEmpty());
    }

    SECTION("Constructor from Two Pointers")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span(arr, arr + 5);
        REQUIRE(span.Data() == arr);
        REQUIRE(span.Size() == 5);
    }

    SECTION("Constructor from C-style Array")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span(arr);
        REQUIRE(span.Data() == arr);
        REQUIRE(span.Size() == 5);
    }

    SECTION("Constructor from std::vector")
    {
        std::vector<int> vec = { 1, 2, 3, 4, 5 };
        TSpan<int> span(vec);
        REQUIRE(span.Data() == vec.data());
        REQUIRE(span.Size() == 5);
    }

    SECTION("Constructor from const std::vector")
    {
        const std::vector<int> vec = { 1, 2, 3, 4, 5 };
        TSpan<const int> span(vec);
        REQUIRE(span.Data() == vec.data());
        REQUIRE(span.Size() == 5);
    }

    SECTION("Constructor from std::array")
    {
        std::array<int, 5> arr = { 1, 2, 3, 4, 5 };
        TSpan<int> span(arr);
        REQUIRE(span.Data() == arr.data());
        REQUIRE(span.Size() == 5);
    }

    SECTION("Static Extent Constructor")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int, 5> span(arr, 5);
        REQUIRE(span.Data() == arr);
        REQUIRE(span.Size() == 5);
    }

    SECTION("Conversion from Non-const to Const")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> nonConstSpan(arr);
        TSpan<const int> constSpan(nonConstSpan);
        REQUIRE(constSpan.Data() == arr);
        REQUIRE(constSpan.Size() == 5);
    }

    SECTION("Copy Constructor")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span1(arr);
        TSpan<int> span2(span1);
        REQUIRE(span2.Data() == span1.Data());
        REQUIRE(span2.Size() == span1.Size());
    }
}

TEST_CASE("TSpan Element Access", "[GP][Core][Container][Span][Access]")
{
    using namespace GP::Container;
    using namespace GP;

    int arr[] = { 10, 20, 30, 40, 50 };
    TSpan<int> span(arr);

    SECTION("Operator[]")
    {
        REQUIRE(span[0] == 10);
        REQUIRE(span[2] == 30);
        REQUIRE(span[4] == 50);
    }

    SECTION("At Method")
    {
        REQUIRE(span.At(0) == 10);
        REQUIRE(span.At(2) == 30);
        REQUIRE(span.At(4) == 50);
    }

    SECTION("Front") { REQUIRE(span.Front() == 10); }

    SECTION("Back") { REQUIRE(span.Back() == 50); }

    SECTION("Data")
    {
        REQUIRE(span.Data() == arr);
        REQUIRE(*span.Data() == 10);
    }

    SECTION("Modify Through Span")
    {
        span[1] = 99;
        REQUIRE(arr[1] == 99);
        REQUIRE(span[1] == 99);
    }
}

TEST_CASE("TSpan Size and Capacity", "[GP][Core][Container][Span][Size]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Size of Non-empty Span")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span(arr);
        REQUIRE(span.Size() == 5);
    }

    SECTION("Size of Empty Span")
    {
        TSpan<int> span;
        REQUIRE(span.Size() == 0);
    }

    SECTION("SizeBytes")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span(arr);
        REQUIRE(span.SizeBytes() == 5 * sizeof(int));
    }

    SECTION("IsEmpty")
    {
        TSpan<int> emptySpan;
        REQUIRE(emptySpan.IsEmpty());

        int arr[] = { 1 };
        TSpan<int> nonEmptySpan(arr);
        REQUIRE_FALSE(nonEmptySpan.IsEmpty());
    }

    SECTION("Static Extent")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<int, 3> span(arr, 3);
        REQUIRE(span.Size() == 3);
        REQUIRE(TSpan<int, 3>::ExtentValue == 3);
    }
}

TEST_CASE("TSpan Subspan Operations", "[GP][Core][Container][Span][Subspan]")
{
    using namespace GP::Container;
    using namespace GP;

    int arr[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    TSpan<int> span(arr);

    SECTION("First - Runtime")
    {
        auto sub = span.First(3);
        REQUIRE(sub.Size() == 3);
        REQUIRE(sub[0] == 1);
        REQUIRE(sub[1] == 2);
        REQUIRE(sub[2] == 3);
    }

    SECTION("First - Compile Time")
    {
        auto sub = span.First<3>();
        REQUIRE(sub.Size() == 3);
        REQUIRE(sub[0] == 1);
        REQUIRE(sub[2] == 3);
    }

    SECTION("First - Full Span")
    {
        auto sub = span.First(10);
        REQUIRE(sub.Size() == 10);
        REQUIRE(sub[0] == 1);
        REQUIRE(sub[9] == 10);
    }

    SECTION("Last - Runtime")
    {
        auto sub = span.Last(3);
        REQUIRE(sub.Size() == 3);
        REQUIRE(sub[0] == 8);
        REQUIRE(sub[1] == 9);
        REQUIRE(sub[2] == 10);
    }

    SECTION("Last - Compile Time")
    {
        auto sub = span.Last<3>();
        REQUIRE(sub.Size() == 3);
        REQUIRE(sub[0] == 8);
        REQUIRE(sub[2] == 10);
    }

    SECTION("Subspan - With Count")
    {
        auto sub = span.Subspan(2, 4);
        REQUIRE(sub.Size() == 4);
        REQUIRE(sub[0] == 3);
        REQUIRE(sub[3] == 6);
    }

    SECTION("Subspan - Without Count (to end)")
    {
        auto sub = span.Subspan(5);
        REQUIRE(sub.Size() == 5);
        REQUIRE(sub[0] == 6);
        REQUIRE(sub[4] == 10);
    }

    SECTION("Subspan - Compile Time")
    {
        auto sub = span.Subspan<2, 4>();
        REQUIRE(sub.Size() == 4);
        REQUIRE(sub[0] == 3);
        REQUIRE(sub[3] == 6);
    }

    SECTION("Subspan - Zero Offset")
    {
        auto sub = span.Subspan(0, 5);
        REQUIRE(sub.Size() == 5);
        REQUIRE(sub[0] == 1);
    }

    SECTION("Subspan - End of Span")
    {
        auto sub = span.Subspan(9, 1);
        REQUIRE(sub.Size() == 1);
        REQUIRE(sub[0] == 10);
    }
}

TEST_CASE("TSpan Comparison Operators", "[GP][Core][Container][Span][Comparison]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Equality - Same Data")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span1(arr);
        TSpan<int> span2(arr);
        REQUIRE(span1 == span2);
    }

    SECTION("Equality - Different Data, Same Values")
    {
        int arr1[] = { 1, 2, 3 };
        int arr2[] = { 1, 2, 3 };
        TSpan<int> span1(arr1);
        TSpan<int> span2(arr2);
        REQUIRE(span1 == span2);
    }

    SECTION("Inequality - Different Sizes")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span1(arr, 3);
        TSpan<int> span2(arr, 5);
        REQUIRE_FALSE(span1 == span2);
    }

    SECTION("Inequality - Different Values")
    {
        int arr1[] = { 1, 2, 3 };
        int arr2[] = { 1, 2, 4 };
        TSpan<int> span1(arr1);
        TSpan<int> span2(arr2);
        REQUIRE_FALSE(span1 == span2);
    }

    SECTION("Three-Way Comparison - Less Than")
    {
        int arr1[] = { 1, 2, 3 };
        int arr2[] = { 1, 2, 4 };
        TSpan<int> span1(arr1);
        TSpan<int> span2(arr2);
        REQUIRE((span1 <=> span2) < 0);
    }

    SECTION("Three-Way Comparison - Equal")
    {
        int arr1[] = { 1, 2, 3 };
        int arr2[] = { 1, 2, 3 };
        TSpan<int> span1(arr1);
        TSpan<int> span2(arr2);
        REQUIRE((span1 <=> span2) == 0);
    }

    SECTION("Three-Way Comparison - Greater Than")
    {
        int arr1[] = { 1, 2, 4 };
        int arr2[] = { 1, 2, 3 };
        TSpan<int> span1(arr1);
        TSpan<int> span2(arr2);
        REQUIRE((span1 <=> span2) > 0);
    }
}

TEST_CASE("TSpan Byte Conversion", "[GP][Core][Container][Span][Bytes]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("AsBytes")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<int> span(arr);
        auto bytes = span.AsBytes();
        REQUIRE(bytes.Size() == 3 * sizeof(int));
        REQUIRE(bytes.Data() == reinterpret_cast<const Byte*>(arr));
    }

    SECTION("AsWritableBytes")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<int> span(arr);
        auto bytes = span.AsWritableBytes();
        REQUIRE(bytes.Size() == 3 * sizeof(int));
        REQUIRE(bytes.Data() == reinterpret_cast<Byte*>(arr));

        // Modify through byte view
        bytes[0] = static_cast<Byte>(0xFF);
        REQUIRE(bytes[0] == static_cast<Byte>(0xFF));
    }

    SECTION("AsBytes with Const Span")
    {
        const int arr[] = { 1, 2, 3 };
        TSpan<const int> span(arr);
        auto bytes = span.AsBytes();
        REQUIRE(bytes.Size() == 3 * sizeof(int));
    }
}

TEST_CASE("TSpan ReinterpretAs", "[GP][Core][Container][Span][Reinterpret]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Reinterpret int to byte")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<int> span(arr);
        auto byteSpan = span.ReinterpretAs<Byte>();
        REQUIRE(byteSpan.Size() == 3 * sizeof(int));
    }

    SECTION("Reinterpret byte to int")
    {
        alignas(int) Byte arr[sizeof(int) * 3];
        TSpan<Byte> span(arr, sizeof(int) * 3);
        auto intSpan = span.ReinterpretAs<int>();
        REQUIRE(intSpan.Size() == 3);
    }

    SECTION("Reinterpret float to int")
    {
        float arr[] = { 1.0f, 2.0f, 3.0f };
        TSpan<float> span(arr);
        auto intSpan = span.ReinterpretAs<int>();
        REQUIRE(intSpan.Size() == 3);
    }
}

TEST_CASE("TSpan Fill Operation", "[GP][Core][Container][Span][Fill]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Fill with Value")
    {
        int arr[5] = { 0 };
        TSpan<int> span(arr);
        span.Fill(42);
        for (int i = 0; i < 5; ++i) { REQUIRE(arr[i] == 42); }
    }

    SECTION("Fill Single Element")
    {
        int arr[1] = { 0 };
        TSpan<int> span(arr);
        span.Fill(99);
        REQUIRE(arr[0] == 99);
    }

    SECTION("Fill Large Array")
    {
        int arr[1000] = { 0 };
        TSpan<int> span(arr);
        span.Fill(7);
        REQUIRE(arr[0] == 7);
        REQUIRE(arr[500] == 7);
        REQUIRE(arr[999] == 7);
    }
}

TEST_CASE("TSpan CopyFrom Operation", "[GP][Core][Container][Span][Copy]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Copy Same Size")
    {
        int src[] = { 1, 2, 3, 4, 5 };
        int dst[5] = { 0 };
        TSpan<int> srcSpan(src);
        TSpan<int> dstSpan(dst);
        dstSpan.CopyFrom(srcSpan);
        for (int i = 0; i < 5; ++i) { REQUIRE(dst[i] == src[i]); }
    }

    SECTION("Copy Smaller to Larger")
    {
        int src[] = { 1, 2, 3 };
        int dst[5] = { 0 };
        TSpan<const int> srcSpan(src);
        TSpan<int> dstSpan(dst);
        dstSpan.CopyFrom(srcSpan);
        REQUIRE(dst[0] == 1);
        REQUIRE(dst[1] == 2);
        REQUIRE(dst[2] == 3);
        REQUIRE(dst[3] == 0);
        REQUIRE(dst[4] == 0);
    }

    SECTION("Copy Single Element")
    {
        int src[] = { 42 };
        int dst[] = { 0 };
        TSpan<const int> srcSpan(src);
        TSpan<int> dstSpan(dst);
        dstSpan.CopyFrom(srcSpan);
        REQUIRE(dst[0] == 42);
    }
}

TEST_CASE("TSpan ZeroMemory Operation", "[GP][Core][Container][Span][Zero]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Zero Small Array")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span(arr);
        span.ZeroMemory();
        for (int i = 0; i < 5; ++i) { REQUIRE(arr[i] == 0); }
    }

    SECTION("Zero Byte Array")
    {
        Byte arr[10];
        for (int i = 0; i < 10; ++i) { arr[i] = static_cast<Byte>(0xFF); }
        TSpan<Byte> span(arr);
        span.ZeroMemory();
        for (int i = 0; i < 10; ++i) { REQUIRE(arr[i] == static_cast<Byte>(0)); }
    }

    SECTION("Zero Large Array")
    {
        int arr[1000];
        for (int i = 0; i < 1000; ++i) { arr[i] = 123; }
        TSpan<int> span(arr);
        span.ZeroMemory();
        REQUIRE(arr[0] == 0);
        REQUIRE(arr[500] == 0);
        REQUIRE(arr[999] == 0);
    }
}

TEST_CASE("TSpan Alignment Operations", "[GP][Core][Container][Span][Alignment]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("IsAligned - 4 byte")
    {
        alignas(4) int arr[5];
        TSpan<int> span(arr);
        REQUIRE(span.IsAligned<4>());
    }

    SECTION("IsAligned - 16 byte")
    {
        alignas(16) int arr[5];
        TSpan<int> span(arr);
        REQUIRE(span.IsAligned<16>());
        REQUIRE(span.IsAligned<4>());
    }

    SECTION("GetAlignment")
    {
        alignas(16) int arr[5];
        TSpan<int> span(arr);
        REQUIRE(span.GetAlignment() >= 16);
    }
}

TEST_CASE("TSpan GetChunk Operation", "[GP][Core][Container][Span][Chunk]")
{
    using namespace GP::Container;
    using namespace GP;

    int arr[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    TSpan<int> span(arr);

    SECTION("Get First Chunk")
    {
        auto chunk = span.GetChunk(0, 3);
        REQUIRE(chunk.Size() == 3);
        REQUIRE(chunk[0] == 0);
        REQUIRE(chunk[1] == 1);
        REQUIRE(chunk[2] == 2);
    }

    SECTION("Get Middle Chunk")
    {
        auto chunk = span.GetChunk(1, 3);
        REQUIRE(chunk.Size() == 3);
        REQUIRE(chunk[0] == 3);
        REQUIRE(chunk[1] == 4);
        REQUIRE(chunk[2] == 5);
    }

    SECTION("Get Last Chunk")
    {
        auto chunk = span.GetChunk(3, 3);
        REQUIRE(chunk.Size() == 1);   // Only 1 element remaining
        REQUIRE(chunk[0] == 9);
    }

    SECTION("Get Chunk with Exact Size")
    {
        auto chunk = span.GetChunk(0, 10);
        REQUIRE(chunk.Size() == 10);
        REQUIRE(chunk[0] == 0);
        REQUIRE(chunk[9] == 9);
    }
}

TEST_CASE("TSpan Iterator Operations", "[GP][Core][Container][Span][Iterator]")
{
    using namespace GP::Container;
    using namespace GP;

    int arr[] = { 1, 2, 3, 4, 5 };
    TSpan<int> span(arr);

    SECTION("begin and end")
    {
        auto it = span.begin();
        REQUIRE(*it == 1);
        REQUIRE(span.end() - span.begin() == 5);
    }

    SECTION("Range-based for loop")
    {
        int sum = 0;
        for (int val: span) { sum += val; }
        REQUIRE(sum == 15);
    }

    SECTION("Iterator Increment")
    {
        auto it = span.begin();
        ++it;
        REQUIRE(*it == 2);
        ++it;
        REQUIRE(*it == 3);
    }

    SECTION("const iterators")
    {
        auto it = span.cbegin();
        REQUIRE(*it == 1);
        REQUIRE(span.cend() - span.cbegin() == 5);
    }

    SECTION("Reverse iterators")
    {
        auto it = span.rbegin();
        REQUIRE(*it == 5);
        ++it;
        REQUIRE(*it == 4);
    }

    SECTION("const reverse iterators")
    {
        auto it = span.crbegin();
        REQUIRE(*it == 5);
        ++it;
        REQUIRE(*it == 4);
    }
}

TEST_CASE("TSpan Edge Cases", "[GP][Core][Container][Span][Edge]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Single Element Span")
    {
        int arr[] = { 42 };
        TSpan<int> span(arr);
        REQUIRE(span.Size() == 1);
        REQUIRE(span.Front() == 42);
        REQUIRE(span.Back() == 42);
        REQUIRE(span[0] == 42);
    }

    SECTION("Empty Span Operations")
    {
        TSpan<int> span;
        REQUIRE(span.IsEmpty());
        REQUIRE(span.Size() == 0);
        REQUIRE(span.SizeBytes() == 0);
    }

    SECTION("Span from Empty Vector")
    {
        std::vector<int> vec;
        TSpan<int> span(vec);
        REQUIRE(span.IsEmpty());
        REQUIRE(span.Size() == 0);
    }

    SECTION("Const Span Cannot Modify")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<const int> span(arr);
        REQUIRE(span[0] == 1);
        // span[0] = 5; // Should not compile
    }

    SECTION("Subspan at Boundary")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<int> span(arr);
        auto sub = span.Subspan(3, 0);
        REQUIRE(sub.IsEmpty());
    }

    SECTION("First/Last with Zero Count")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<int> span(arr);
        auto first = span.First(0);
        auto last = span.Last(0);
        REQUIRE(first.IsEmpty());
        REQUIRE(last.IsEmpty());
    }

    SECTION("Multiple Subspans Chain")
    {
        int arr[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        TSpan<int> span(arr);
        auto sub1 = span.Subspan(2, 6);   // [3,4,5,6,7,8]
        auto sub2 = sub1.First(3);        // [3,4,5]
        auto sub3 = sub2.Last(2);         // [4,5]
        REQUIRE(sub3.Size() == 2);
        REQUIRE(sub3[0] == 4);
        REQUIRE(sub3[1] == 5);
    }
}

TEST_CASE("TSpan Static Extent", "[GP][Core][Container][Span][Static]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Static Extent Creation")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int, 5> span(arr, 5);
        REQUIRE(span.Size() == 5);
        REQUIRE(TSpan<int, 5>::ExtentValue == 5);
    }

    SECTION("Static Extent from C Array")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<int, 3> span(arr);
        REQUIRE(span.Size() == 3);
    }

    SECTION("Static Extent Subspan")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int, 5> span(arr, 5);
        auto sub = span.First<3>();
        REQUIRE(sub.Size() == 3);
        REQUIRE(sub[0] == 1);
    }

    SECTION("Static to Dynamic Conversion")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<int, 3> staticSpan(arr, 3);
        TSpan<int> dynamicSpan = staticSpan.Subspan(0);
        REQUIRE(dynamicSpan.Size() == 3);
    }
}

TEST_CASE("TSpan with Complex Types", "[GP][Core][Container][Span][Complex]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Span of Strings")
    {
        std::string arr[] = { "hello", "world", "test" };
        TSpan<std::string> span(arr);
        REQUIRE(span.Size() == 3);
        REQUIRE(span[0] == "hello");
        REQUIRE(span[2] == "test");
    }

    SECTION("Span of Const Strings")
    {
        const std::string arr[] = { "alpha", "beta", "gamma" };
        TSpan<const std::string> span(arr);
        REQUIRE(span.Size() == 3);
        REQUIRE(span[1] == "beta");
    }

    SECTION("Modify Complex Type")
    {
        std::string arr[] = { "hello", "world" };
        TSpan<std::string> span(arr);
        span[0] = "goodbye";
        REQUIRE(arr[0] == "goodbye");
    }

    SECTION("Struct Span")
    {
        struct Point
        {
            int x, y;
        };

        Point arr[] = {
            { 1, 2 },
            { 3, 4 },
            { 5, 6 }
        };
        TSpan<Point> span(arr);
        REQUIRE(span.Size() == 3);
        REQUIRE(span[0].x == 1);
        REQUIRE(span[2].y == 6);
    }
}

TEST_CASE("TSpan Assignment", "[GP][Core][Container][Span][Assignment]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Copy Assignment")
    {
        int arr1[] = { 1, 2, 3 };
        int arr2[] = { 4, 5, 6 };
        TSpan<int> span1(arr1);
        TSpan<int> span2(arr2);

        span2 = span1;
        REQUIRE(span2.Data() == arr1);
        REQUIRE(span2.Size() == 3);
        REQUIRE(span2[0] == 1);
    }

    SECTION("Self Assignment")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<int> span(arr);
        span = span;
        REQUIRE(span.Data() == arr);
        REQUIRE(span.Size() == 3);
    }
}

TEST_CASE("TSpan STL Algorithm Compatibility", "[GP][Core][Container][Span][STL]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("std::find")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span(arr);
        auto it = std::find(span.begin(), span.end(), 3);
        REQUIRE(it != span.end());
        REQUIRE(*it == 3);
    }

    SECTION("std::count")
    {
        int arr[] = { 1, 2, 3, 2, 2, 4 };
        TSpan<int> span(arr);
        auto count = std::count(span.begin(), span.end(), 2);
        REQUIRE(count == 3);
    }

    SECTION("std::accumulate")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span(arr);
        int sum = 0;
        for (int val: span) { sum += val; }
        REQUIRE(sum == 15);
    }

    SECTION("std::sort")
    {
        int arr[] = { 5, 2, 4, 1, 3 };
        TSpan<int> span(arr);
        std::sort(span.begin(), span.end());
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[4] == 5);
    }

    SECTION("std::reverse")
    {
        int arr[] = { 1, 2, 3, 4, 5 };
        TSpan<int> span(arr);
        std::reverse(span.begin(), span.end());
        REQUIRE(arr[0] == 5);
        REQUIRE(arr[4] == 1);
    }
}

TEST_CASE("TSpan Performance Patterns", "[GP][Core][Container][Span][Performance]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Zero Copy Subrange")
    {
        std::vector<int> vec(1000);
        for (int i = 0; i < 1000; ++i) { vec[i] = i; }

        TSpan<int> fullSpan(vec);
        auto sub = fullSpan.Subspan(100, 100);

        // Verify no copy was made
        REQUIRE(sub.Data() == vec.data() + 100);
        REQUIRE(sub[0] == 100);
        REQUIRE(sub[99] == 199);
    }

    SECTION("Efficient Chunk Processing")
    {
        int arr[100];
        for (int i = 0; i < 100; ++i) { arr[i] = i; }

        TSpan<int> span(arr);
        constexpr size_t chunkSize = 10;

        for (size_t i = 0; i < 10; ++i)
        {
            auto chunk = span.GetChunk(i, chunkSize);
            int expected = static_cast<int>(i * chunkSize);
            REQUIRE(chunk[0] == expected);
        }
    }
}

TEST_CASE("TSpan Type Safety", "[GP][Core][Container][Span][Safety]")
{
    using namespace GP::Container;
    using namespace GP;

    SECTION("Cannot Convert Const to Non-Const")
    {
        const int arr[] = { 1, 2, 3 };
        TSpan<const int> constSpan(arr);
        // TSpan<int> nonConstSpan = constSpan; // Should not compile
        REQUIRE(constSpan.Size() == 3);
    }

    SECTION("Const Span from Non-Const Data")
    {
        int arr[] = { 1, 2, 3 };
        TSpan<const int> span(arr);
        REQUIRE(span[0] == 1);
        // span[0] = 5; // Should not compile
    }
}
