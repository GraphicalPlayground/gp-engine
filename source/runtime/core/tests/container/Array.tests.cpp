// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "container/Array.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

namespace gp::tests
{

consteval bool testConstexprArray()
{
    gp::Array<int, 5> arr{ 1, 2, 3, 4, 5 };
    arr.fill(10);
    arr[2] = 42;
    arr.clear();

    gp::Array<int, 5> arr2{ 1, 1, 1, 1, 1 };
    arr.swap(arr2);

    return arr[0] == 1 && arr2[0] == 0 && arr.size() == 5;
}

static_assert(testConstexprArray(), "Array must be fully usable in constant expressions");

static_assert(std::is_trivially_copyable_v<gp::Array<int, 5>>);
static_assert(std::is_trivially_copy_assignable_v<gp::Array<int, 5>>);
static_assert(std::is_nothrow_move_constructible_v<gp::Array<int, 5>>);

TEST_CASE("Array - Initialization and Capacity", "[container][Array]")
{
    SECTION("Default Construction")
    {
        gp::Array<int, 3> arr;
        REQUIRE(arr.size() == 3);
        REQUIRE(arr.capacity() == 3);
        REQUIRE(arr.empty() == false);
        // Value initialization ensures elements are zero
        REQUIRE(arr[0] == 0);
        REQUIRE(arr[1] == 0);
        REQUIRE(arr[2] == 0);
    }

    SECTION("Variadic Initialization")
    {
        gp::Array<int, 4> arr{ 10, 20, 30, 40 };
        REQUIRE(arr[0] == 10);
        REQUIRE(arr[3] == 40);
    }

    SECTION("Partial Variadic Initialization")
    {
        gp::Array<int, 5> arr{ 1, 2 };   // Remaining should be 0
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 0);
        REQUIRE(arr[4] == 0);
    }

    SECTION("Zero-sized Array Edge Case")
    {
        gp::Array<int, 0> emptyArr;
        REQUIRE(emptyArr.size() == 0);
        REQUIRE(emptyArr.capacity() == 0);
        REQUIRE(emptyArr.empty() == true);
        REQUIRE(emptyArr.data() == nullptr);
        REQUIRE(emptyArr.begin() == emptyArr.end());
    }
}

TEST_CASE("Array - Element Access", "[container][Array]")
{
    gp::Array<int, 3> arr{ 5, 10, 15 };
    const gp::Array<int, 3> carr{ 5, 10, 15 };

    SECTION("operator[]")
    {
        REQUIRE(arr[0] == 5);
        REQUIRE(arr[2] == 15);
        REQUIRE(carr[1] == 10);

        arr[1] = 99;
        REQUIRE(arr[1] == 99);
    }

    SECTION("at()")
    {
        REQUIRE(arr.at(0) == 5);
        REQUIRE(arr.at(2) == 15);
        REQUIRE(carr.at(1) == 10);

        arr.at(1) = 99;
        REQUIRE(arr.at(1) == 99);
    }

    SECTION("front() and back()")
    {
        REQUIRE(arr.front() == 5);
        REQUIRE(arr.back() == 15);
        REQUIRE(carr.front() == 5);
        REQUIRE(carr.back() == 15);

        arr.front() = 1;
        arr.back() = 3;
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[2] == 3);
    }

    SECTION("data()")
    {
        REQUIRE(arr.data() == &arr[0]);
        REQUIRE(carr.data() == &carr[0]);

        arr.data()[0] = 42;
        REQUIRE(arr[0] == 42);
    }
}

TEST_CASE("Array - Iterators", "[container][Array]")
{
    gp::Array<int, 4> arr{ 1, 2, 3, 4 };
    const gp::Array<int, 4> carr{ 1, 2, 3, 4 };

    SECTION("Forward Iterators")
    {
        int sum = 0;
        for (auto it = arr.begin(); it != arr.end(); ++it)
        {
            sum += *it;
        }
        REQUIRE(sum == 10);

        sum = 0;
        for (auto it = carr.begin(); it != carr.end(); ++it)
        {
            sum += *it;
        }
        REQUIRE(sum == 10);
    }

    SECTION("Reverse Iterators")
    {
        REQUIRE(*arr.rbegin() == 4);
        REQUIRE(*(arr.rend() - 1) == 1);
        REQUIRE(*carr.crbegin() == 4);

        int sum = 0;
        for (auto it = arr.rbegin(); it != arr.rend(); ++it)
        {
            sum += *it;
        }
        REQUIRE(sum == 10);
    }

    SECTION("Const Iterators (cbegin/cend)")
    {
        REQUIRE(*arr.cbegin() == 1);
        REQUIRE(*(arr.cend() - 1) == 4);
    }

    SECTION("Standard library compatibility")
    {
        gp::Array<int, 4> target;
        std::copy(carr.begin(), carr.end(), target.begin());
        REQUIRE(target == carr);
    }
}

TEST_CASE("Array - Modifiers", "[container][Array]")
{
    SECTION("fill()")
    {
        gp::Array<int, 3> arr;
        arr.fill(42);
        // clang-format off
        REQUIRE((arr[0] == 42 && arr[1] == 42 && arr[2] == 42));
        // clang-format on
    }

    SECTION("clear()")
    {
        gp::Array<int, 3> arr{ 1, 2, 3 };
        arr.clear();
        // clang-format off
        REQUIRE((arr[0] == 0 && arr[1] == 0 && arr[2] == 0));
        // clang-format on
    }

    SECTION("swap() method")
    {
        gp::Array<int, 3> arr1{ 1, 2, 3 };
        gp::Array<int, 3> arr2{ 7, 8, 9 };

        arr1.swap(arr2);

        REQUIRE(arr1[0] == 7);
        REQUIRE(arr2[0] == 1);
    }

    SECTION("ADL swap free function")
    {
        gp::Array<int, 3> arr1{ 1, 2, 3 };
        gp::Array<int, 3> arr2{ 7, 8, 9 };

        using std::swap;
        swap(arr1, arr2);   // Should resolve to gp::swap

        REQUIRE(arr1[0] == 7);
        REQUIRE(arr2[0] == 1);
    }
}

TEST_CASE("Array - Operations and Search Algorithms", "[container][Array]")
{
    const gp::Array<int, 6> arr{ 10, 20, 20, 30, 20, 40 };

    SECTION("find()")
    {
        REQUIRE(*arr.find(30) == 30);
        REQUIRE(arr.find(99) == arr.end());

        gp::Array<int, 6> mutArr = arr;
        *mutArr.find(10) = 15;
        REQUIRE(mutArr[0] == 15);
    }

    SECTION("contains()")
    {
        REQUIRE(arr.contains(20) == true);
        REQUIRE(arr.contains(99) == false);
    }

    SECTION("count()")
    {
        REQUIRE(arr.count(20) == 3);
        REQUIRE(arr.count(10) == 1);
        REQUIRE(arr.count(99) == 0);
    }

    SECTION("indexOf() and lastIndexOf()")
    {
        REQUIRE(arr.indexOf(20) == 1);
        REQUIRE(arr.lastIndexOf(20) == 4);

        REQUIRE(arr.indexOf(99) == gp::Array<int, 6>::npos);
        REQUIRE(arr.lastIndexOf(99) == gp::Array<int, 6>::npos);
    }

    SECTION("indexNotOf() and lastIndexNotOf()")
    {
        gp::Array<int, 4> uniform{ 5, 5, 5, 5 };
        REQUIRE(uniform.indexNotOf(5) == gp::Array<int, 4>::npos);
        REQUIRE(uniform.lastIndexNotOf(5) == gp::Array<int, 4>::npos);

        gp::Array<int, 5> mixed{ 5, 5, 7, 5, 8 };
        REQUIRE(mixed.indexNotOf(5) == 2);
        REQUIRE(mixed.lastIndexNotOf(5) == 4);
    }
}

TEST_CASE("Array - Rule of Zero and Comparisons", "[container][Array]")
{
    gp::Array<int, 3> arr1{ 1, 2, 3 };
    gp::Array<int, 3> arr2{ 1, 2, 3 };
    gp::Array<int, 3> arr3{ 1, 9, 3 };

    SECTION("Copy Semantics")
    {
        gp::Array<int, 3> copyConstructed = arr1;
        REQUIRE(copyConstructed == arr1);

        gp::Array<int, 3> copyAssigned;
        copyAssigned = arr1;
        REQUIRE(copyAssigned == arr1);
    }

    SECTION("Move Semantics")
    {
        gp::Array<int, 3> moveConstructed = std::move(arr1);
        REQUIRE(moveConstructed[0] == 1);   // Note: moving trivial types is just a copy

        gp::Array<int, 3> moveAssigned;
        moveAssigned = std::move(arr2);
        REQUIRE(moveAssigned[1] == 2);
    }

    SECTION("Spaceship Operator (<=>)")
    {
        // Equality
        REQUIRE(arr1 == arr2);
        REQUIRE(arr1 != arr3);

        // Lexicographical ordering
        REQUIRE(arr1 < arr3);   // 1,2,3 < 1,9,3
        REQUIRE(arr3 > arr1);
        REQUIRE(arr1 <= arr2);
        REQUIRE(arr2 >= arr1);
    }
}

}   // namespace gp::tests
