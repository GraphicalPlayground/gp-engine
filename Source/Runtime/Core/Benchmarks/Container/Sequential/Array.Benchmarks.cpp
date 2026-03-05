// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Sequential/Array.hpp"
#include "CoreTypes.hpp"
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace GP;

TEST_CASE("TArray: Benchmark construction with count", "[Container][Array]")
{
    static const SizeT count = 1000;

    // clang-format off

    BENCHMARK("TArray: Construct with count") { return TArray<Int32>(count); };

    BENCHMARK("std::vector: Construct with count") { return std::vector<Int32>(count); };

    // clang-format on
}

TEST_CASE("TArray: Benchmark construction with count and default value", "[Container][Array]")
{
    static const SizeT count = 1000;
    static const Int32 defaultValue = 42;

    // clang-format off

    BENCHMARK("TArray: Construct with count and default value") { return TArray<Int32>(count, defaultValue); };

    BENCHMARK("std::vector: Construct with count and default value") { return std::vector<Int32>(count, defaultValue); };

    // clang-format on
}

TEST_CASE("TArray: Benchmark construction from initializer list", "[Container][Array]")
{
    // clang-format off

    BENCHMARK("TArray: Construct from initializer list") { return TArray<Int32>{1, 2, 3, 4, 5}; };

    BENCHMARK("std::vector: Construct from initializer list") { return std::vector<Int32>{1, 2, 3, 4, 5}; };

    // clang-format on
}

TEST_CASE("TArray: Benchmark push back", "[Container][Array]")
{
    static const SizeT count = 1000;

    // clang-format off

    BENCHMARK("TArray: Push back")
    {
        TArray<Int32> array;
        for (SizeT i = 0; i < count; ++i)
        {
            array.PushBack(static_cast<Int32>(i));
        }
        return array;
    };

    BENCHMARK("std::vector: Push back")
    {
        std::vector<Int32> vector;
        for (SizeT i = 0; i < count; ++i)
        {
            vector.push_back(static_cast<Int32>(i));
        }
        return vector;
    };

    // clang-format on
}

TEST_CASE("TArray: Benchmark pop back", "[Container][Array]")
{
    static const SizeT count = 1000;

    // clang-format off
    
    BENCHMARK("TArray: Pop back")
    {
        TArray<Int32> array(count);
        for (SizeT i = 0; i < count; ++i)
        {
            array.PopBack();
        }
        return array;
    };

    BENCHMARK("std::vector: Pop back")
    {
        std::vector<Int32> vector(count);
        for (SizeT i = 0; i < count; ++i)
        {
            vector.pop_back();
        }
        return vector;
    };

    // clang-format on
}

TEST_CASE("TArray: Benchmark clear", "[Container][Array]")
{
    static const SizeT count = 1000;

    // clang-format off
    
    BENCHMARK("TArray: Clear")
    {
        TArray<Int32> array(count);
        array.Clear();
        return array;
    };

    BENCHMARK("std::vector: Clear")
    {
        std::vector<Int32> vector(count);
        vector.clear();
        return vector;
    };

    // clang-format on
}

TEST_CASE("TArray: Benchmark resize", "[Container][Array]")
{
    static const SizeT count = 1000;

    // clang-format off

    BENCHMARK("TArray: Resize")
    {
        TArray<Int32> array;
        array.Resize(count);
        return array;
    };

    BENCHMARK("std::vector: Resize")
    {
        std::vector<Int32> vector;
        vector.resize(count);
        return vector;
    };

    // clang-format on
}

TEST_CASE("TArray: Benchmark reserve", "[Container][Array]")
{
    static const SizeT count = 1000;

    // clang-format off

    BENCHMARK("TArray: Reserve")
    {
        TArray<Int32> array;
        array.Reserve(count);
        return array;
    };

    BENCHMARK("std::vector: Reserve")
    {
        std::vector<Int32> vector;
        vector.reserve(count);
        return vector;
    };

    // clang-format on
}
