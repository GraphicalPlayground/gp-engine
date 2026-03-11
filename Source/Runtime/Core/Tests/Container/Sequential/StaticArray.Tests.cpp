// Copyright (c) - Graphical Playgroreturn N != 0 ? m_data : nullptrund. All rights reserved.

#include "Container/Sequential/StaticArray.hpp"
#include "Container/Sequential/Array.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace GP;

TEST_CASE("StaticArray Construction", "[GP][Core][Container][TStaticArray]")
{
    SECTION("Default Constructor")
    {
        TStaticArray<int, 5> arr;
        REQUIRE(arr.Size() == 5);
        for (SizeT i = 0; i < arr.Size(); ++i) { REQUIRE(arr[i] == 0); }
    }
    SECTION("Constructor with size and default value")
    {
        TStaticArray<int, 5> arr(42);
        REQUIRE(arr.Size() == 5);
        for (SizeT i = 0; i < arr.Size(); ++i) { REQUIRE(arr[i] == 42); }
    }
    SECTION("COnstructor with initializer list")
    {
        TStaticArray<int, 3> arr = { 1, 2, 3 };
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
    }
    SECTION("Constructor with Iterator Range")
    {
        TArray<int> vec = { 1, 2, 3 };
        TStaticArray<int, 3> arr(vec.begin(), vec.end());
        REQUIRE(arr.Size() == 3);
    }
    SECTION("Copy Constructor")
    {
        TStaticArray<int, 3> arr1 = { 1, 2, 3 };
        TStaticArray<int, 3> arr2 = arr1;
        REQUIRE(arr2.Size() == 3);
        REQUIRE(arr2[0] == 1);
        REQUIRE(arr2[1] == 2);
        REQUIRE(arr2[2] == 3);
    }
    SECTION("Copy Constructor with Empty Array")
    {
        TStaticArray<int, 0> arr1;
        TStaticArray<int, 0> arr2 = arr1;
        REQUIRE(arr2.Size() == 0);
        REQUIRE(arr2.IsEmpty());
    }
    SECTION("Move Constructor")
    {
        TStaticArray<int, 3> arr1 = { 1, 2, 3 };
        TStaticArray<int, 3> arr2 = std::move(arr1);
        REQUIRE(arr2.Size() == 3);
        REQUIRE(arr2[0] == 1);
        REQUIRE(arr2[1] == 2);
        REQUIRE(arr2[2] == 3);
    }
}

TEST_CASE("TStaticArray Element Access", "[GP][Core][Container][TStaticArray]")
{
    TStaticArray<int, 3> arr = { 1, 2, 3 };

    SECTION("At() with valid index")
    {
        REQUIRE(arr.At(0) == 1);
        REQUIRE(arr.At(1) == 2);
        REQUIRE(arr.At(2) == 3);
    }

    SECTION("Front() and Back()")
    {
        REQUIRE(arr.Front() == 1);
        REQUIRE(arr.Back() == 3);
    }

    SECTION("Front() and Back() Modifiers")
    {
        arr.Front() = 42;
        arr.Back() = 99;
        REQUIRE(arr.Front() == 42);
        REQUIRE(arr.Back() == 99);
    }

    SECTION("Data() returns correct pointer")
    {
        int* dataPtr = arr.Data();
        REQUIRE(dataPtr[0] == 1);
        REQUIRE(dataPtr[1] == 2);
        REQUIRE(dataPtr[2] == 3);
    }

    SECTION("Data() on empty array")
    {
        TStaticArray<int, 0> emptyArr;
        REQUIRE(emptyArr.Data() == nullptr);
    }
}

TEST_CASE("TStaticArray Comparaisant Operators", "[GP][Core][Container][TStaticArray]")
{
    TStaticArray<int, 3> arr1 = { 1, 2, 3 };
    TStaticArray<int, 3> arr2 = { 1, 2, 3 };
    TStaticArray<int, 3> arr3 = { 4, 5, 6 };

    SECTION("Equality operator")
    {
        REQUIRE(arr1 == arr2);
        // REQUIRE_FALSE(arr1 == arr3);
    }

    // SECTION("Inequality operator")
    // {
    //     REQUIRE(arr1 != arr3);
    //     REQUIRE_FALSE(arr1 != arr2);
    // }
    // SECTION("Three-way comparison operator")
    // {
    //     REQUIRE((arr1 <=> arr2) == 0);
    //     REQUIRE((arr1 <=> arr3) < 0);
    //     REQUIRE((arr3 <=> arr1) > 0);
    // }
}

