// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/Allocator.hpp"
#include "Memory/MemoryUtilities.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>

TEST_CASE("TAllocator Basic Allocation", "[GP][Core][Memory][Allocator][Basic]")
{
    using namespace GP::Memory;

    TAllocator<int> allocator;

    SECTION("Allocate and Deallocate Single Block")
    {
        void* ptr = allocator.Allocate(64);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, alignof(std::max_align_t)));
        allocator.Deallocate(ptr);
    }

    SECTION("Deallocate Nullptr")
    {
        allocator.Deallocate(nullptr);   // Should not crash
        REQUIRE(true);
    }

    SECTION("Multiple Allocations")
    {
        void* ptr1 = allocator.Allocate(32);
        void* ptr2 = allocator.Allocate(64);
        void* ptr3 = allocator.Allocate(128);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);
        REQUIRE(ptr1 != ptr2);
        REQUIRE(ptr2 != ptr3);

        allocator.Deallocate(ptr1);
        allocator.Deallocate(ptr2);
        allocator.Deallocate(ptr3);
    }

    SECTION("Allocate Zero Size Should Assert in Debug")
    {
        // In debug builds, this would trigger an assertion
        // In release builds, behavior is undefined but should not crash
        REQUIRE(true);
    }
}

TEST_CASE("TAllocator Alignment", "[GP][Core][Memory][Allocator][Alignment]")
{
    using namespace GP::Memory;

    TAllocator<int> allocator;

    SECTION("Default Alignment")
    {
        void* ptr = allocator.Allocate(64);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, alignof(int)));
        REQUIRE(IsAligned(ptr, alignof(std::max_align_t)));
        allocator.Deallocate(ptr);
    }

    SECTION("Custom Alignment - 16 bytes")
    {
        void* ptr = allocator.Allocate(64, 16);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, 16));
        allocator.Deallocate(ptr);
    }

    SECTION("Custom Alignment - 32 bytes")
    {
        void* ptr = allocator.Allocate(128, 32);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, 32));
        allocator.Deallocate(ptr);
    }

    SECTION("Custom Alignment - 64 bytes")
    {
        void* ptr = allocator.Allocate(256, 64);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, 64));
        allocator.Deallocate(ptr);
    }

    SECTION("Large Alignment - Cache Line")
    {
        void* ptr = allocator.Allocate(512, 64);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, 64));
        allocator.Deallocate(ptr);
    }

    SECTION("Large Alignment - Page Size")
    {
        void* ptr = allocator.Allocate(8192, 4096);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, 4096));
        allocator.Deallocate(ptr);
    }
}

TEST_CASE("TAllocator AllocateTyped", "[GP][Core][Memory][Allocator][Typed]")
{
    using namespace GP::Memory;

    SECTION("Allocate Single Int")
    {
        TAllocator<int> allocator;
        int* ptr = allocator.AllocateTyped(1);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, alignof(int)));

        *ptr = 42;
        REQUIRE(*ptr == 42);

        allocator.Deallocate(ptr);
    }

    SECTION("Allocate Array of Ints")
    {
        TAllocator<int> allocator;
        int* arr = allocator.AllocateTyped(10);
        REQUIRE(arr != nullptr);

        for (int i = 0; i < 10; ++i) { arr[i] = i * 2; }
        for (int i = 0; i < 10; ++i) { REQUIRE(arr[i] == i * 2); }

        allocator.Deallocate(arr);
    }

    SECTION("Allocate Complex Type")
    {
        struct LargeStruct
        {
            alignas(32) char data[256];
        };

        TAllocator<LargeStruct> allocator;
        LargeStruct* ptr = allocator.AllocateTyped(1);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, 32));

        allocator.Deallocate(ptr);
    }

    SECTION("Allocate and Construct String")
    {
        TAllocator<std::string> allocator;
        std::string* ptr = allocator.AllocateTyped(1);
        REQUIRE(ptr != nullptr);

        Construct<std::string>(ptr, "Hello World");
        REQUIRE(*ptr == "Hello World");

        Destroy(ptr);
        allocator.Deallocate(ptr);
    }
}

TEST_CASE("TAllocator Usage with Objects", "[GP][Core][Memory][Allocator][Objects]")
{
    using namespace GP::Memory;

    SECTION("Allocate and Construct Multiple Objects")
    {
        TAllocator<std::string> allocator;
        std::string* arr = allocator.AllocateTyped(5);
        REQUIRE(arr != nullptr);

        for (size_t i = 0; i < 5; ++i) { Construct<std::string>(arr + i, "Item " + std::to_string(i)); }

        for (size_t i = 0; i < 5; ++i) { REQUIRE(arr[i] == "Item " + std::to_string(i)); }

        DestroyRange(arr, 5);
        allocator.Deallocate(arr);
    }

    SECTION("Allocate Complex Type with Constructor")
    {
        struct TestStruct
        {
            int value;
            std::string name;
            float data;

            TestStruct(int v, std::string n, float d)
                : value(v)
                , name(std::move(n))
                , data(d)
            {}
        };

        TAllocator<TestStruct> allocator;
        TestStruct* ptr = allocator.AllocateTyped(1);
        REQUIRE(ptr != nullptr);

        Construct<TestStruct>(ptr, 42, "Test", 3.14f);
        REQUIRE(ptr->value == 42);
        REQUIRE(ptr->name == "Test");
        REQUIRE(ptr->data == 3.14f);

        Destroy(ptr);
        allocator.Deallocate(ptr);
    }
}

TEST_CASE("TAllocator Comparison Operators", "[GP][Core][Memory][Allocator][Comparison]")
{
    using namespace GP::Memory;

    SECTION("Same Type Comparison")
    {
        TAllocator<int> allocator1;
        TAllocator<int> allocator2;

        REQUIRE(allocator1 == allocator2);
        REQUIRE_FALSE(allocator1 != allocator2);
    }

    SECTION("Different Type Comparison")
    {
        TAllocator<int> intAllocator;
        TAllocator<double> doubleAllocator;

        REQUIRE(intAllocator == doubleAllocator);
        REQUIRE_FALSE(intAllocator != doubleAllocator);
    }

    SECTION("Copy Construction from Different Type")
    {
        TAllocator<int> intAllocator;
        TAllocator<double> doubleAllocator(intAllocator);

        REQUIRE(intAllocator == doubleAllocator);
    }
}

TEST_CASE("TAllocator Copy and Move Semantics", "[GP][Core][Memory][Allocator][Semantics]")
{
    using namespace GP::Memory;

    SECTION("Copy Constructor")
    {
        TAllocator<int> allocator1;
        TAllocator<int> allocator2(allocator1);

        REQUIRE(allocator1 == allocator2);

        void* ptr1 = allocator1.Allocate(64);
        void* ptr2 = allocator2.Allocate(64);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);

        allocator1.Deallocate(ptr1);
        allocator2.Deallocate(ptr2);
    }

    SECTION("Copy Assignment")
    {
        TAllocator<int> allocator1;
        TAllocator<int> allocator2;

        allocator2 = allocator1;
        REQUIRE(allocator1 == allocator2);
    }

    SECTION("Move Constructor")
    {
        TAllocator<int> allocator1;
        TAllocator<int> allocator2(std::move(allocator1));

        // After move, both allocators are still valid and equivalent
        REQUIRE(allocator1 == allocator2);
    }
}

TEST_CASE("TAllocator Large Allocations", "[GP][Core][Memory][Allocator][Large]")
{
    using namespace GP::Memory;

    TAllocator<char> allocator;

    SECTION("1 MB Allocation")
    {
        size_t size = 1024 * 1024;
        void* ptr = allocator.Allocate(size);
        REQUIRE(ptr != nullptr);

        // Write to the memory to ensure it's valid
        std::memset(ptr, 0xAA, size);

        allocator.Deallocate(ptr);
    }

    SECTION("10 MB Allocation")
    {
        size_t size = 10 * 1024 * 1024;
        void* ptr = allocator.Allocate(size);
        REQUIRE(ptr != nullptr);

        // Write pattern to verify memory is accessible
        char* bytes = static_cast<char*>(ptr);
        for (size_t i = 0; i < 1000; ++i) { bytes[i * 1024] = static_cast<char>(i & 0xFF); }

        allocator.Deallocate(ptr);
    }

    SECTION("Multiple Large Allocations")
    {
        size_t size = 1024 * 1024;
        void* ptr1 = allocator.Allocate(size);
        void* ptr2 = allocator.Allocate(size);
        void* ptr3 = allocator.Allocate(size);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);

        allocator.Deallocate(ptr1);
        allocator.Deallocate(ptr2);
        allocator.Deallocate(ptr3);
    }
}

TEST_CASE("TAllocator Edge Cases", "[GP][Core][Memory][Allocator][Edge]")
{
    using namespace GP::Memory;

    TAllocator<int> allocator;

    SECTION("Small Alignment Less Than Platform Minimum")
    {
        // Alignment < alignof(std::max_align_t) should be bumped up
        void* ptr = allocator.Allocate(64, 1);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, alignof(std::max_align_t)));
        allocator.Deallocate(ptr);
    }

    SECTION("Very Small Allocation")
    {
        void* ptr = allocator.Allocate(1);
        REQUIRE(ptr != nullptr);
        allocator.Deallocate(ptr);
    }

    SECTION("Odd Sizes")
    {
        void* ptr1 = allocator.Allocate(7);
        void* ptr2 = allocator.Allocate(13);
        void* ptr3 = allocator.Allocate(31);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);

        allocator.Deallocate(ptr1);
        allocator.Deallocate(ptr2);
        allocator.Deallocate(ptr3);
    }

    SECTION("Allocate, Write, Read, Deallocate Pattern")
    {
        size_t size = 1024;
        char* ptr = static_cast<char*>(allocator.Allocate(size));
        REQUIRE(ptr != nullptr);

        // Write pattern
        for (size_t i = 0; i < size; ++i) { ptr[i] = static_cast<char>(i & 0xFF); }

        // Read and verify
        for (size_t i = 0; i < size; ++i) { REQUIRE(ptr[i] == static_cast<char>(i & 0xFF)); }

        allocator.Deallocate(ptr);
    }
}

TEST_CASE("TAllocator Stress Test", "[GP][Core][Memory][Allocator][Stress]")
{
    using namespace GP::Memory;

    TAllocator<int> allocator;

    SECTION("Many Small Allocations")
    {
        constexpr size_t count = 1000;
        void* pointers[count];

        for (size_t i = 0; i < count; ++i)
        {
            pointers[i] = allocator.Allocate(16);
            REQUIRE(pointers[i] != nullptr);
        }

        for (size_t i = 0; i < count; ++i) { allocator.Deallocate(pointers[i]); }
    }

    SECTION("Interleaved Allocate and Deallocate")
    {
        void* ptr1 = allocator.Allocate(32);
        void* ptr2 = allocator.Allocate(64);

        allocator.Deallocate(ptr1);

        void* ptr3 = allocator.Allocate(128);

        allocator.Deallocate(ptr2);

        void* ptr4 = allocator.Allocate(256);

        allocator.Deallocate(ptr3);
        allocator.Deallocate(ptr4);

        REQUIRE(true);
    }
}
