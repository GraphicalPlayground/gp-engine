// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/LinearAllocator.hpp"
#include "Memory/MemoryUtilities.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>

TEST_CASE("TLinearAllocator Internal Buffer Construction", "[GP][Core][Memory][LinearAllocator][Construction]")
{
    using namespace GP::Memory;

    SECTION("Fixed Size Buffer - Default Alignment")
    {
        TLinearAllocator<1024> allocator;

        REQUIRE(allocator.GetCapacityBytes() == 1024);
        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 1024);
        REQUIRE(allocator.GetPeakUsage() == 0);
    }

    SECTION("Fixed Size Buffer - Custom Alignment")
    {
        TLinearAllocator<2048, 64> allocator;

        REQUIRE(allocator.GetCapacityBytes() == 2048);
        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 2048);
    }

    SECTION("Small Buffer")
    {
        TLinearAllocator<64> allocator;

        REQUIRE(allocator.GetCapacityBytes() == 64);
        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 64);
    }
}

TEST_CASE("TLinearAllocator External Buffer Construction", "[GP][Core][Memory][LinearAllocator][Construction]")
{
    using namespace GP::Memory;

    SECTION("External Buffer")
    {
        alignas(16) char buffer[1024];
        TLinearAllocator<0, 16> allocator(buffer, 1024);

        REQUIRE(allocator.GetCapacityBytes() == 1024);
        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 1024);
    }

    SECTION("Large External Buffer")
    {
        alignas(64) char buffer[8192];
        TLinearAllocator<0, 64> allocator(buffer, 8192);

        REQUIRE(allocator.GetCapacityBytes() == 8192);
        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 8192);
    }
}

TEST_CASE("TLinearAllocator Basic Allocation", "[GP][Core][Memory][LinearAllocator][Allocation]")
{
    using namespace GP::Memory;

    TLinearAllocator<1024> allocator;

    SECTION("Single Allocation")
    {
        void* ptr = allocator.Allocate(64);
        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.Owns(ptr));
        REQUIRE(allocator.GetUsedBytes() == 64);
        REQUIRE(allocator.GetRemainingBytes() == 1024 - 64);
    }

    SECTION("Multiple Sequential Allocations")
    {
        void* ptr1 = allocator.Allocate(64);
        void* ptr2 = allocator.Allocate(128);
        void* ptr3 = allocator.Allocate(256);

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);

        REQUIRE(allocator.GetUsedBytes() == 64 + 128 + 256);
        REQUIRE(allocator.GetRemainingBytes() == 1024 - (64 + 128 + 256));
    }

    SECTION("Allocations are Sequential in Memory")
    {
        char* ptr1 = static_cast<char*>(allocator.Allocate(64));
        char* ptr2 = static_cast<char*>(allocator.Allocate(64));
        char* ptr3 = static_cast<char*>(allocator.Allocate(64));

        REQUIRE(ptr2 == ptr1 + 64);
        REQUIRE(ptr3 == ptr2 + 64);
    }
}

TEST_CASE("TLinearAllocator Alignment", "[GP][Core][Memory][LinearAllocator][Alignment]")
{
    using namespace GP::Memory;

    TLinearAllocator<2048> allocator;

    SECTION("Default Alignment")
    {
        void* ptr = allocator.Allocate(64);
        REQUIRE(IsAligned(ptr, alignof(std::max_align_t)));
    }

    SECTION("Custom Alignment - 16 bytes")
    {
        void* ptr = allocator.Allocate(64, 16);
        REQUIRE(IsAligned(ptr, 16));
    }

    SECTION("Custom Alignment - 32 bytes")
    {
        void* ptr = allocator.Allocate(128, 32);
        REQUIRE(IsAligned(ptr, 32));
    }

    SECTION("Custom Alignment - 64 bytes")
    {
        void* ptr = allocator.Allocate(256, 64);
        REQUIRE(IsAligned(ptr, 64));
    }

    SECTION("Mixed Alignments")
    {
        void* ptr1 = allocator.Allocate(1, 1);    // Minimal alignment
        void* ptr2 = allocator.Allocate(1, 16);   // Force 16-byte alignment
        void* ptr3 = allocator.Allocate(1, 32);   // Force 32-byte alignment

        REQUIRE(IsAligned(ptr1, 1));
        REQUIRE(IsAligned(ptr2, 16));
        REQUIRE(IsAligned(ptr3, 32));
    }

    SECTION("Alignment Padding is Accounted For")
    {
        // Allocate 1 byte with default alignment
        void* ptr1 = allocator.Allocate(1);
        size_t used1 = allocator.GetUsedBytes();

        // Allocate another byte with 64-byte alignment
        // This should add padding to reach the next 64-byte boundary
        void* ptr2 = allocator.Allocate(1, 64);
        size_t used2 = allocator.GetUsedBytes();

        REQUIRE(IsAligned(ptr2, 64));
        REQUIRE(used2 > used1 + 1);   // Padding was added
    }
}

TEST_CASE("TLinearAllocator Reset", "[GP][Core][Memory][LinearAllocator][Reset]")
{
    using namespace GP::Memory;

    TLinearAllocator<1024> allocator;

    SECTION("Reset After Allocations")
    {
        (void)allocator.Allocate(64);
        (void)allocator.Allocate(128);
        (void)allocator.Allocate(256);

        REQUIRE(allocator.GetUsedBytes() > 0);

        allocator.Reset();

        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 1024);
    }

    SECTION("Allocate After Reset")
    {
        void* ptr1 = allocator.Allocate(64);
        allocator.Reset();

        void* ptr2 = allocator.Allocate(64);

        // After reset, new allocation should be at the same address
        REQUIRE(ptr1 == ptr2);
    }

    SECTION("Multiple Reset Cycles")
    {
        for (int i = 0; i < 10; ++i)
        {
            (void)allocator.Allocate(100);
            REQUIRE(allocator.GetUsedBytes() > 0);

            allocator.Reset();
            REQUIRE(allocator.GetUsedBytes() == 0);
        }
    }
}

TEST_CASE("TLinearAllocator Peak Usage Tracking", "[GP][Core][Memory][LinearAllocator][Tracking]")
{
    using namespace GP::Memory;

    TLinearAllocator<1024> allocator;

    SECTION("Peak Usage Increases")
    {
        REQUIRE(allocator.GetPeakUsage() == 0);

        (void)allocator.Allocate(64);
        REQUIRE(allocator.GetPeakUsage() == 64);

        (void)allocator.Allocate(128);
        REQUIRE(allocator.GetPeakUsage() == 64 + 128);

        (void)allocator.Allocate(256);
        REQUIRE(allocator.GetPeakUsage() == 64 + 128 + 256);
    }

    SECTION("Peak Usage Persists After Reset")
    {
        (void)allocator.Allocate(512);
        size_t peak = allocator.GetPeakUsage();

        allocator.Reset();

        // Peak usage should remain the same after reset
        REQUIRE(allocator.GetPeakUsage() == peak);

        // Allocate less than peak
        (void)allocator.Allocate(256);
        REQUIRE(allocator.GetPeakUsage() == peak);

        // Allocate more than peak
        (void)allocator.Allocate(600);
        REQUIRE(allocator.GetPeakUsage() > peak);
    }
}

TEST_CASE("TLinearAllocator Out of Memory", "[GP][Core][Memory][LinearAllocator][OOM]")
{
    using namespace GP::Memory;

    TLinearAllocator<256> allocator;

    SECTION("Fill Buffer Completely")
    {
        void* ptr = allocator.Allocate(256);
        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.GetRemainingBytes() == 0);

        // Next allocation should fail
        void* ptr2 = allocator.Allocate(1);
        REQUIRE(ptr2 == nullptr);
    }

    SECTION("Allocation Larger Than Buffer")
    {
        void* ptr = allocator.Allocate(512);
        REQUIRE(ptr == nullptr);
    }

    SECTION("Partial Fill Then Overflow")
    {
        (void)allocator.Allocate(100);
        (void)allocator.Allocate(100);

        // Only 56 bytes remaining, request 128
        void* ptr = allocator.Allocate(128);
        REQUIRE(ptr == nullptr);
    }

    SECTION("Out of Memory with Alignment Padding")
    {
        (void)allocator.Allocate(200);   // 56 bytes remaining

        // Request 56 bytes with 64-byte alignment
        // This should fail because alignment padding would exceed capacity
        void* ptr = allocator.Allocate(56, 64);
        REQUIRE(ptr == nullptr);
    }
}

TEST_CASE("TLinearAllocator Owns", "[GP][Core][Memory][LinearAllocator][Ownership]")
{
    using namespace GP::Memory;

    TLinearAllocator<1024> allocator;

    SECTION("Owns Allocated Pointer")
    {
        void* ptr = allocator.Allocate(64);
        REQUIRE(allocator.Owns(ptr));
    }

    SECTION("Owns Multiple Allocated Pointers")
    {
        void* ptr1 = allocator.Allocate(64);
        void* ptr2 = allocator.Allocate(128);
        void* ptr3 = allocator.Allocate(256);

        REQUIRE(allocator.Owns(ptr1));
        REQUIRE(allocator.Owns(ptr2));
        REQUIRE(allocator.Owns(ptr3));
    }

    SECTION("Does Not Own External Pointer")
    {
        int external = 42;
        REQUIRE_FALSE(allocator.Owns(&external));
    }

    SECTION("Does Not Own Nullptr") { REQUIRE_FALSE(allocator.Owns(nullptr)); }

    SECTION("Owns Pointer Within Buffer Range")
    {
        char* base = static_cast<char*>(allocator.Allocate(64));
        REQUIRE(allocator.Owns(base + 32));   // Middle of allocation
    }
}

TEST_CASE("TLinearAllocator Deallocate is No-Op", "[GP][Core][Memory][LinearAllocator][Deallocation]")
{
    using namespace GP::Memory;

    TLinearAllocator<1024> allocator;

    SECTION("Deallocate Does Not Free Memory")
    {
        void* ptr = allocator.Allocate(64);
        size_t usedBefore = allocator.GetUsedBytes();

        allocator.Deallocate(ptr);

        // Memory should not be freed
        REQUIRE(allocator.GetUsedBytes() == usedBefore);
    }

    SECTION("Deallocate Multiple Times")
    {
        void* ptr = allocator.Allocate(64);

        allocator.Deallocate(ptr);
        allocator.Deallocate(ptr);
        allocator.Deallocate(ptr);

        // Should not crash or affect state
        REQUIRE(allocator.GetUsedBytes() == 64);
    }

    SECTION("Deallocate Nullptr")
    {
        allocator.Deallocate(nullptr);   // Should not crash
        REQUIRE(true);
    }
}

TEST_CASE("TLinearAllocator With Objects", "[GP][Core][Memory][LinearAllocator][Objects]")
{
    using namespace GP::Memory;

    TLinearAllocator<2048> allocator;

    SECTION("Construct and Destroy Objects")
    {
        std::string* str = static_cast<std::string*>(allocator.Allocate(sizeof(std::string)));
        REQUIRE(str != nullptr);

        Construct<std::string>(str, "Hello Linear Allocator");
        REQUIRE(*str == "Hello Linear Allocator");

        Destroy(str);
        allocator.Reset();
    }

    SECTION("Multiple Objects")
    {
        constexpr size_t count = 5;
        std::string* strings = static_cast<std::string*>(allocator.Allocate(sizeof(std::string) * count));

        for (size_t i = 0; i < count; ++i) { Construct<std::string>(strings + i, "String " + std::to_string(i)); }

        for (size_t i = 0; i < count; ++i) { REQUIRE(strings[i] == "String " + std::to_string(i)); }

        DestroyRange(strings, count);
        allocator.Reset();
    }

    SECTION("Complex Type")
    {
        struct TestStruct
        {
            int value;
            std::string name;
            double data;

            TestStruct(int v, std::string n, double d)
                : value(v)
                , name(std::move(n))
                , data(d)
            {}
        };

        TestStruct* obj = static_cast<TestStruct*>(allocator.Allocate(sizeof(TestStruct)));
        Construct<TestStruct>(obj, 100, "Test", 3.14159);

        REQUIRE(obj->value == 100);
        REQUIRE(obj->name == "Test");
        REQUIRE(obj->data == 3.14159);

        Destroy(obj);
        allocator.Reset();
    }
}

TEST_CASE("TLinearAllocator Stress Test", "[GP][Core][Memory][LinearAllocator][Stress]")
{
    using namespace GP::Memory;

    TLinearAllocator<8192> allocator;

    SECTION("Many Small Allocations")
    {
        size_t totalAllocated = 0;
        int count = 0;

        while (totalAllocated < 8000)
        {
            void* ptr = allocator.Allocate(8);
            if (ptr == nullptr) { break; }

            totalAllocated += 8;
            ++count;
        }

        constexpr size_t effectiveSize = std::max<size_t>(8, alignof(std::max_align_t));

        REQUIRE(count >= 8192 / effectiveSize);
        REQUIRE(allocator.GetUsedBytes() <= 8192);
    }

    SECTION("Varying Size Allocations")
    {
        size_t sizes[] = { 8, 16, 32, 64, 128, 256 };
        int totalCount = 0;

        for (int round = 0; round < 10; ++round)
        {
            for (size_t size: sizes)
            {
                void* ptr = allocator.Allocate(size);
                if (ptr != nullptr) { ++totalCount; }
            }
        }

        REQUIRE(totalCount > 0);
    }

    SECTION("Frame Allocation Pattern")
    {
        // Simulate per-frame allocation pattern
        for (int frame = 0; frame < 100; ++frame)
        {
            // Allocate some temporary data
            (void)allocator.Allocate(100);
            (void)allocator.Allocate(200);
            (void)allocator.Allocate(50);

            // Reset at end of frame
            allocator.Reset();

            REQUIRE(allocator.GetUsedBytes() == 0);
        }
    }
}

TEST_CASE("TLinearAllocator Edge Cases", "[GP][Core][Memory][LinearAllocator][Edge]")
{
    using namespace GP::Memory;

    SECTION("Minimum Size Buffer")
    {
        TLinearAllocator<1> allocator;
        void* ptr = allocator.Allocate(1);
        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.GetUsedBytes() == 1);
    }

    SECTION("Allocation Exactly Equal to Capacity")
    {
        TLinearAllocator<256> allocator;
        void* ptr = allocator.Allocate(256);
        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.GetRemainingBytes() == 0);
    }

    SECTION("Very Large Alignment")
    {
        TLinearAllocator<2048, 256> allocator;
        void* ptr = allocator.Allocate(1, 256);
        REQUIRE(ptr != nullptr);
        REQUIRE(IsAligned(ptr, 256));
    }

    SECTION("Empty Allocations Pattern")
    {
        TLinearAllocator<1024> allocator;

        allocator.Reset();
        REQUIRE(allocator.GetUsedBytes() == 0);

        allocator.Reset();
        REQUIRE(allocator.GetUsedBytes() == 0);
    }

    SECTION("Write and Read Pattern")
    {
        TLinearAllocator<1024> allocator;
        int* arr = static_cast<int*>(allocator.Allocate(sizeof(int) * 10));

        for (int i = 0; i < 10; ++i) { arr[i] = i * i; }
        for (int i = 0; i < 10; ++i) { REQUIRE(arr[i] == i * i); }
    }
}
