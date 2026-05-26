// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/arrays/Array.hpp"
#include <gtest/gtest.h>

namespace gp::tests
{

TEST(ArrayTest, StaticConstants)
{
    EXPECT_EQ((Array<int, 5>::arraySize), 5u);
    EXPECT_EQ((Array<int, 5>::byteSize), 5u * sizeof(int));
    EXPECT_EQ((Array<int, 5>::npos), static_cast<gp::USize>(-1));
}

TEST(ArrayTest, StaticConstantsZeroSize)
{
    EXPECT_EQ((Array<int, 0>::arraySize), 0u);
    EXPECT_EQ((Array<int, 0>::byteSize), 0u);
}

TEST(ArrayTest, DefaultConstruction)
{
    Array<int, 4> arr;
    for (gp::USize i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr[i], 0);
    }
}

TEST(ArrayTest, DefaultConstructionZeroSize)
{
    Array<int, 0> arr;
    EXPECT_TRUE(arr.isEmpty());
    EXPECT_EQ(arr.size(), 0u);
}

TEST(ArrayTest, InitializerListConstruction)
{
    Array<int, 4> arr = { 1, 2, 3, 4 };
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(arr[3], 4);
}

TEST(ArrayTest, InitializerListPartialFill)
{
    Array<int, 4> arr = { 10, 20 };
    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[1], 20);
    EXPECT_EQ(arr[2], 0);
    EXPECT_EQ(arr[3], 0);
}

TEST(ArrayTest, VariadicConstruction)
{
    Array<int, 3> arr(7, 8, 9);
    EXPECT_EQ(arr[0], 7);
    EXPECT_EQ(arr[1], 8);
    EXPECT_EQ(arr[2], 9);
}

TEST(ArrayTest, CopyConstruction)
{
    Array<int, 3> src = { 1, 2, 3 };
    Array<int, 3> dst(src);
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[1], 2);
    EXPECT_EQ(dst[2], 3);
}

TEST(ArrayTest, CopyAssignment)
{
    Array<int, 3> src = { 4, 5, 6 };
    Array<int, 3> dst;
    dst = src;
    EXPECT_EQ(dst[0], 4);
    EXPECT_EQ(dst[1], 5);
    EXPECT_EQ(dst[2], 6);
}

TEST(ArrayTest, MoveConstruction)
{
    Array<int, 3> src = { 1, 2, 3 };
    Array<int, 3> dst(std::move(src));
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[1], 2);
    EXPECT_EQ(dst[2], 3);
}

TEST(ArrayTest, MoveAssignment)
{
    Array<int, 3> src = { 7, 8, 9 };
    Array<int, 3> dst;
    dst = std::move(src);
    EXPECT_EQ(dst[0], 7);
    EXPECT_EQ(dst[1], 8);
    EXPECT_EQ(dst[2], 9);
}

TEST(ArrayTest, CopyIsIndependent)
{
    Array<int, 3> src = { 1, 2, 3 };
    Array<int, 3> dst(src);
    dst[0] = 99;
    EXPECT_EQ(src[0], 1);
}

TEST(ArrayTest, OperatorBracketMutable)
{
    Array<int, 3> arr = { 1, 2, 3 };
    arr[1] = 99;
    EXPECT_EQ(arr[1], 99);
}

TEST(ArrayTest, OperatorBracketConst)
{
    const Array<int, 3> arr = { 10, 20, 30 };
    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[2], 30);
}

TEST(ArrayTest, AtMutable)
{
    Array<int, 3> arr = { 1, 2, 3 };
    arr.at(0) = 42;
    EXPECT_EQ(arr.at(0), 42);
    EXPECT_EQ(arr.at(1), 2);
    EXPECT_EQ(arr.at(2), 3);
}

TEST(ArrayTest, AtConst)
{
    const Array<int, 3> arr = { 5, 6, 7 };
    EXPECT_EQ(arr.at(0), 5);
    EXPECT_EQ(arr.at(1), 6);
    EXPECT_EQ(arr.at(2), 7);
}

TEST(ArrayTest, FrontMutable)
{
    Array<int, 3> arr = { 10, 20, 30 };
    arr.front() = 99;
    EXPECT_EQ(arr.front(), 99);
}

TEST(ArrayTest, FrontConst)
{
    const Array<int, 3> arr = { 10, 20, 30 };
    EXPECT_EQ(arr.front(), 10);
}

TEST(ArrayTest, BackMutable)
{
    Array<int, 3> arr = { 10, 20, 30 };
    arr.back() = 99;
    EXPECT_EQ(arr.back(), 99);
}

TEST(ArrayTest, BackConst)
{
    const Array<int, 3> arr = { 10, 20, 30 };
    EXPECT_EQ(arr.back(), 30);
}

TEST(ArrayTest, DataMutable)
{
    Array<int, 3> arr = { 1, 2, 3 };
    int* ptr = arr.data();
    ASSERT_NE(ptr, nullptr);
    ptr[0] = 42;
    EXPECT_EQ(arr[0], 42);
}

TEST(ArrayTest, DataConst)
{
    const Array<int, 3> arr = { 1, 2, 3 };
    const int* ptr = arr.data();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr[0], 1);
    EXPECT_EQ(ptr[2], 3);
}

TEST(ArrayTest, DataZeroSizeReturnsNull)
{
    Array<int, 0> arr;
    EXPECT_EQ(arr.data(), nullptr);

    const Array<int, 0> constArr;
    EXPECT_EQ(constArr.data(), nullptr);
}

TEST(ArrayTest, Size)
{
    EXPECT_EQ((Array<int, 5>{}.size()), 5u);
    EXPECT_EQ((Array<float, 1>{}.size()), 1u);
    EXPECT_EQ((Array<double, 0>{}.size()), 0u);
}

TEST(ArrayTest, Capacity)
{
    EXPECT_EQ((Array<int, 5>{}.capacity()), 5u);
    EXPECT_EQ((Array<int, 0>{}.capacity()), 0u);
}

TEST(ArrayTest, IsEmpty)
{
    EXPECT_TRUE((Array<int, 0>{}.isEmpty()));
    EXPECT_FALSE((Array<int, 1>{}.isEmpty()));
    EXPECT_FALSE((Array<int, 5>{}.isEmpty()));
}

TEST(ArrayTest, Fill)
{
    Array<int, 4> arr;
    arr.fill(7);
    for (gp::USize i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(arr[i], 7);
    }
}

TEST(ArrayTest, FillOverwritesExistingValues)
{
    Array<int, 3> arr = { 1, 2, 3 };
    arr.fill(0);
    EXPECT_EQ(arr[0], 0);
    EXPECT_EQ(arr[1], 0);
    EXPECT_EQ(arr[2], 0);
}

TEST(ArrayTest, Clear)
{
    Array<int, 3> arr = { 1, 2, 3 };
    arr.clear();
    EXPECT_EQ(arr[0], 0);
    EXPECT_EQ(arr[1], 0);
    EXPECT_EQ(arr[2], 0);
}

TEST(ArrayTest, MemberSwap)
{
    Array<int, 3> a = { 1, 2, 3 };
    Array<int, 3> b = { 4, 5, 6 };
    a.swap(b);
    EXPECT_EQ(a[0], 4);
    EXPECT_EQ(a[1], 5);
    EXPECT_EQ(a[2], 6);
    EXPECT_EQ(b[0], 1);
    EXPECT_EQ(b[1], 2);
    EXPECT_EQ(b[2], 3);
}

TEST(ArrayTest, FreeFunctionSwap)
{
    Array<int, 3> a = { 7, 8, 9 };
    Array<int, 3> b = { 10, 11, 12 };
    swap(a, b);
    EXPECT_EQ(a[0], 10);
    EXPECT_EQ(a[2], 12);
    EXPECT_EQ(b[0], 7);
    EXPECT_EQ(b[2], 9);
}

TEST(ArrayTest, BeginEnd)
{
    Array<int, 4> arr = { 1, 2, 3, 4 };
    int sum = 0;
    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
        sum += *it;
    }
    EXPECT_EQ(sum, 10);
}

TEST(ArrayTest, ConstBeginEnd)
{
    const Array<int, 4> arr = { 1, 2, 3, 4 };
    int sum = 0;
    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
        sum += *it;
    }
    EXPECT_EQ(sum, 10);
}

TEST(ArrayTest, CBeginCEnd)
{
    Array<int, 3> arr = { 5, 6, 7 };
    int sum = 0;
    for (auto it = arr.cbegin(); it != arr.cend(); ++it)
    {
        sum += *it;
    }
    EXPECT_EQ(sum, 18);
}

TEST(ArrayTest, ReverseIterator)
{
    Array<int, 4> arr = { 1, 2, 3, 4 };
    int values[4];
    int idx = 0;
    for (auto it = arr.rbegin(); it != arr.rend(); ++it)
    {
        values[idx++] = *it;
    }
    EXPECT_EQ(values[0], 4);
    EXPECT_EQ(values[1], 3);
    EXPECT_EQ(values[2], 2);
    EXPECT_EQ(values[3], 1);
}

TEST(ArrayTest, ConstReverseIterator)
{
    const Array<int, 3> arr = { 10, 20, 30 };
    int values[3];
    int idx = 0;
    for (auto it = arr.rbegin(); it != arr.rend(); ++it)
    {
        values[idx++] = *it;
    }
    EXPECT_EQ(values[0], 30);
    EXPECT_EQ(values[1], 20);
    EXPECT_EQ(values[2], 10);
}

TEST(ArrayTest, CRBeginCREnd)
{
    Array<int, 3> arr = { 1, 2, 3 };
    int values[3];
    int idx = 0;
    for (auto it = arr.crbegin(); it != arr.crend(); ++it)
    {
        values[idx++] = *it;
    }
    EXPECT_EQ(values[0], 3);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 1);
}

TEST(ArrayTest, RangeBasedForLoop)
{
    Array<int, 5> arr = { 2, 4, 6, 8, 10 };
    int sum = 0;
    for (int v: arr)
    {
        sum += v;
    }
    EXPECT_EQ(sum, 30);
}

TEST(ArrayTest, BeginEqualsData)
{
    Array<int, 3> arr = { 1, 2, 3 };
    EXPECT_EQ(arr.begin(), arr.data());
    EXPECT_EQ(arr.cbegin(), arr.data());
}

TEST(ArrayTest, EndEqualsDataPlusSize)
{
    Array<int, 3> arr = { 1, 2, 3 };
    EXPECT_EQ(arr.end(), arr.data() + arr.size());
}

TEST(ArrayTest, FindExistingValue)
{
    Array<int, 5> arr = { 10, 20, 30, 40, 50 };
    auto it = arr.find(30);
    ASSERT_NE(it, arr.end());
    EXPECT_EQ(*it, 30);
}

TEST(ArrayTest, FindMissingValue)
{
    Array<int, 5> arr = { 10, 20, 30, 40, 50 };
    EXPECT_EQ(arr.find(99), arr.end());
}

TEST(ArrayTest, FindReturnsFirstOccurrence)
{
    Array<int, 5> arr = { 1, 2, 1, 2, 1 };
    auto it = arr.find(1);
    EXPECT_EQ(it, arr.begin());
}

TEST(ArrayTest, FindConstExistingValue)
{
    const Array<int, 4> arr = { 1, 2, 3, 4 };
    auto it = arr.find(3);
    ASSERT_NE(it, arr.end());
    EXPECT_EQ(*it, 3);
}

TEST(ArrayTest, FindConstMissingValue)
{
    const Array<int, 3> arr = { 1, 2, 3 };
    EXPECT_EQ(arr.find(42), arr.end());
}

TEST(ArrayTest, ContainsTrue)
{
    Array<int, 4> arr = { 5, 10, 15, 20 };
    EXPECT_TRUE(arr.contains(10));
    EXPECT_TRUE(arr.contains(5));
    EXPECT_TRUE(arr.contains(20));
}

TEST(ArrayTest, ContainsFalse)
{
    Array<int, 4> arr = { 5, 10, 15, 20 };
    EXPECT_FALSE(arr.contains(0));
    EXPECT_FALSE(arr.contains(99));
}

TEST(ArrayTest, CountSingleOccurrence)
{
    Array<int, 5> arr = { 1, 2, 3, 4, 5 };
    EXPECT_EQ(arr.count(3), 1u);
}

TEST(ArrayTest, CountMultipleOccurrences)
{
    Array<int, 6> arr = { 1, 2, 2, 3, 2, 4 };
    EXPECT_EQ(arr.count(2), 3u);
}

TEST(ArrayTest, CountNoOccurrence)
{
    Array<int, 4> arr = { 1, 2, 3, 4 };
    EXPECT_EQ(arr.count(99), 0u);
}

TEST(ArrayTest, CountAllElements)
{
    Array<int, 4> arr = { 7, 7, 7, 7 };
    EXPECT_EQ(arr.count(7), 4u);
}

TEST(ArrayTest, IndexOf)
{
    Array<int, 5> arr = { 10, 20, 30, 40, 50 };
    EXPECT_EQ(arr.indexOf(10), 0u);
    EXPECT_EQ(arr.indexOf(30), 2u);
    EXPECT_EQ(arr.indexOf(50), 4u);
}

TEST(ArrayTest, IndexOfNotFound)
{
    Array<int, 3> arr = { 1, 2, 3 };
    constexpr auto npos = Array<int, 3>::npos;
    EXPECT_EQ(arr.indexOf(99), npos);
}

TEST(ArrayTest, IndexOfFirstOccurrence)
{
    Array<int, 5> arr = { 1, 2, 1, 2, 1 };
    EXPECT_EQ(arr.indexOf(1), 0u);
    EXPECT_EQ(arr.indexOf(2), 1u);
}

TEST(ArrayTest, LastIndexOf)
{
    Array<int, 5> arr = { 1, 2, 1, 2, 1 };
    EXPECT_EQ(arr.lastIndexOf(1), 4u);
    EXPECT_EQ(arr.lastIndexOf(2), 3u);
}

TEST(ArrayTest, LastIndexOfSingleOccurrence)
{
    Array<int, 4> arr = { 10, 20, 30, 40 };
    EXPECT_EQ(arr.lastIndexOf(10), 0u);
    EXPECT_EQ(arr.lastIndexOf(40), 3u);
}

TEST(ArrayTest, LastIndexOfNotFound)
{
    Array<int, 3> arr = { 1, 2, 3 };
    constexpr auto npos = Array<int, 3>::npos;
    EXPECT_EQ(arr.lastIndexOf(99), npos);
}

TEST(ArrayTest, LastIndexOfEqualsIndexOfWhenUnique)
{
    Array<int, 4> arr = { 1, 2, 3, 4 };
    EXPECT_EQ(arr.lastIndexOf(2), arr.indexOf(2));
}

TEST(ArrayTest, IndexNotOf)
{
    Array<int, 4> arr = { 0, 0, 1, 0 };
    EXPECT_EQ(arr.indexNotOf(0), 2u);
}

TEST(ArrayTest, IndexNotOfAllSame)
{
    Array<int, 3> arr = { 5, 5, 5 };
    constexpr auto npos = Array<int, 3>::npos;
    EXPECT_EQ(arr.indexNotOf(5), npos);
}

TEST(ArrayTest, IndexNotOfFirstElement)
{
    Array<int, 4> arr = { 1, 0, 0, 0 };
    EXPECT_EQ(arr.indexNotOf(0), 0u);
}

TEST(ArrayTest, LastIndexNotOf)
{
    Array<int, 5> arr = { 1, 0, 0, 0, 2 };
    EXPECT_EQ(arr.lastIndexNotOf(0), 4u);
}

TEST(ArrayTest, LastIndexNotOfAllSame)
{
    Array<int, 3> arr = { 7, 7, 7 };
    constexpr auto npos = Array<int, 3>::npos;
    EXPECT_EQ(arr.lastIndexNotOf(7), npos);
}

TEST(ArrayTest, LastIndexNotOfLastElement)
{
    Array<int, 4> arr = { 0, 0, 0, 3 };
    EXPECT_EQ(arr.lastIndexNotOf(0), 3u);
}

TEST(ArrayTest, EqualityEqual)
{
    Array<int, 3> a = { 1, 2, 3 };
    Array<int, 3> b = { 1, 2, 3 };
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(ArrayTest, EqualityNotEqual)
{
    Array<int, 3> a = { 1, 2, 3 };
    Array<int, 3> b = { 1, 2, 4 };
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST(ArrayTest, LessThan)
{
    Array<int, 3> a = { 1, 2, 3 };
    Array<int, 3> b = { 1, 2, 4 };
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
}

TEST(ArrayTest, LessOrEqual)
{
    Array<int, 3> a = { 1, 2, 3 };
    Array<int, 3> b = { 1, 2, 3 };
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(b <= a);

    Array<int, 3> c = { 1, 2, 2 };
    EXPECT_TRUE(c <= a);
    EXPECT_FALSE(a <= c);
}

TEST(ArrayTest, GreaterThan)
{
    Array<int, 3> a = { 1, 2, 5 };
    Array<int, 3> b = { 1, 2, 3 };
    EXPECT_TRUE(a > b);
    EXPECT_FALSE(b > a);
}

TEST(ArrayTest, GreaterOrEqual)
{
    Array<int, 3> a = { 1, 2, 3 };
    Array<int, 3> b = { 1, 2, 3 };
    EXPECT_TRUE(a >= b);
    EXPECT_TRUE(b >= a);

    Array<int, 3> c = { 1, 2, 4 };
    EXPECT_TRUE(c >= a);
    EXPECT_FALSE(a >= c);
}

TEST(ArrayTest, CustomAlignment)
{
    Array<int, 4, 16> arr = { 1, 2, 3, 4 };
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(arr.data()) % 16, 0u);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[3], 4);
}

TEST(ArrayTest, NonTrivialTypeConstruction)
{
    Array<std::string, 3> arr = { "hello", "world", "!" };
    EXPECT_EQ(arr[0], "hello");
    EXPECT_EQ(arr[1], "world");
    EXPECT_EQ(arr[2], "!");
}

TEST(ArrayTest, NonTrivialTypeFind)
{
    Array<std::string, 3> arr = { "alpha", "beta", "gamma" };
    EXPECT_TRUE(arr.contains("beta"));
    EXPECT_FALSE(arr.contains("delta"));
    EXPECT_EQ(arr.indexOf("gamma"), 2u);
}

TEST(ArrayTest, NonTrivialTypeFillAndClear)
{
    Array<std::string, 3> arr = { "a", "b", "c" };
    arr.fill("x");
    EXPECT_EQ(arr[0], "x");
    EXPECT_EQ(arr[2], "x");
    arr.clear();
    EXPECT_EQ(arr[0], "");
}

TEST(ArrayTest, NonTrivialTypeSwap)
{
    Array<std::string, 2> a = { "foo", "bar" };
    Array<std::string, 2> b = { "baz", "qux" };
    a.swap(b);
    EXPECT_EQ(a[0], "baz");
    EXPECT_EQ(a[1], "qux");
    EXPECT_EQ(b[0], "foo");
    EXPECT_EQ(b[1], "bar");
}

}   // namespace gp::tests
