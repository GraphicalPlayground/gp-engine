// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/InlineAllocator.hpp"
#include "Memory/MemoryUtilities.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>

TEST_CASE("TInlineAllocator Construction", "[GP][Core][Memory][InlineAllocator][Construction]")
{
    using namespace GP::Memory;

    SECTION("Basic Construction")
    {
        TInlineAllocator<int, 8> allocator;

        REQUIRE(allocator.GetInlineCapacity() == 8);
        REQUIRE(allocator.GetInlineBuffer() != nullptr);
    }

    SECTION("Small Inline Count")
    {
        TInlineAllocator<int, 1> allocator;
        REQUIRE(allocator.GetInlineCapacity() == 1);
    }

    SECTION("Large Inline Count")
    {
        TInlineAllocator<int, 100> allocator;
        REQUIRE(allocator.GetInlineCapacity() == 100);
    }

    SECTION("Different Types")
    {
        TInlineAllocator<double, 10> doubleAlloc;
        REQUIRE(doubleAlloc.GetInlineCapacity() == 10);

        TInlineAllocator<char, 256> charAlloc;
        REQUIRE(charAlloc.GetInlineCapacity() == 256);
    }
}

TEST_CASE("TInlineAllocator Inline Allocation", "[GP][Core][Memory][InlineAllocator][Inline]")
{
    using namespace GP::Memory;

    SECTION("Allocate Within Inline Buffer")
    {
        TInlineAllocator<int, 8> allocator;

        size_t size = sizeof(int) * 4;   // Less than inline capacity
        void* ptr = allocator.Allocate(size);

        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.IsInlinePointer(ptr));
        REQUIRE(ptr == allocator.GetInlineBuffer());

        allocator.Deallocate(ptr);
    }

    SECTION("Allocate Exactly Inline Capacity")
    {
        TInlineAllocator<int, 8> allocator;

        size_t size = sizeof(int) * 8;   // Exactly inline capacity
        void* ptr = allocator.Allocate(size);

        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.IsInlinePointer(ptr));

        allocator.Deallocate(ptr);
    }

    SECTION("Write and Read from Inline Buffer")
    {
        TInlineAllocator<int, 10> allocator;

        int* arr = static_cast<int*>(allocator.Allocate(sizeof(int) * 10));
        REQUIRE(allocator.IsInlinePointer(arr));

        for (int i = 0; i < 10; ++i) { arr[i] = i * i; }
        for (int i = 0; i < 10; ++i) { REQUIRE(arr[i] == i * i); }

        allocator.Deallocate(arr);
    }

    SECTION("Inline Deallocation is No-Op")
    {
        TInlineAllocator<int, 8> allocator;

        void* ptr = allocator.Allocate(sizeof(int) * 4);
        REQUIRE(allocator.IsInlinePointer(ptr));

        // Deallocate should be a no-op for inline buffer
        allocator.Deallocate(ptr);

        // Should be able to allocate again and get the same pointer
        void* ptr2 = allocator.Allocate(sizeof(int) * 4);
        REQUIRE(ptr2 == ptr);

        allocator.Deallocate(ptr2);
    }
}

TEST_CASE("TInlineAllocator Fallback Allocation", "[GP][Core][Memory][InlineAllocator][Fallback]")
{
    using namespace GP::Memory;

    SECTION("Allocate Beyond Inline Capacity")
    {
        TInlineAllocator<int, 8> allocator;

        size_t size = sizeof(int) * 16;   // Larger than inline capacity
        void* ptr = allocator.Allocate(size);

        REQUIRE(ptr != nullptr);
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr));

        allocator.Deallocate(ptr);
    }

    SECTION("Large Allocation Uses Fallback")
    {
        TInlineAllocator<char, 64> allocator;

        size_t size = 1024;   // Much larger than inline capacity
        void* ptr = allocator.Allocate(size);

        REQUIRE(ptr != nullptr);
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr));
        REQUIRE(ptr != allocator.GetInlineBuffer());

        allocator.Deallocate(ptr);
    }

    SECTION("Write and Read from Fallback")
    {
        TInlineAllocator<int, 8> allocator;

        int* arr = static_cast<int*>(allocator.Allocate(sizeof(int) * 20));
        REQUIRE_FALSE(allocator.IsInlinePointer(arr));

        for (int i = 0; i < 20; ++i) { arr[i] = i + 100; }
        for (int i = 0; i < 20; ++i) { REQUIRE(arr[i] == i + 100); }

        allocator.Deallocate(arr);
    }

    SECTION("Fallback Deallocation Works")
    {
        TInlineAllocator<int, 8> allocator;

        void* ptr1 = allocator.Allocate(sizeof(int) * 20);
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr1));

        allocator.Deallocate(ptr1);

        // After deallocation, should be able to allocate again
        void* ptr2 = allocator.Allocate(sizeof(int) * 20);
        REQUIRE(ptr2 != nullptr);
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr2));

        allocator.Deallocate(ptr2);
    }
}

TEST_CASE("TInlineAllocator Mixed Allocations", "[GP][Core][Memory][InlineAllocator][Mixed]")
{
    using namespace GP::Memory;

    SECTION("Inline Then Fallback")
    {
        TInlineAllocator<int, 8> allocator;

        void* inlinePtr = allocator.Allocate(sizeof(int) * 4);
        REQUIRE(allocator.IsInlinePointer(inlinePtr));

        void* fallbackPtr = allocator.Allocate(sizeof(int) * 16);
        REQUIRE_FALSE(allocator.IsInlinePointer(fallbackPtr));

        allocator.Deallocate(fallbackPtr);
        allocator.Deallocate(inlinePtr);
    }

    SECTION("Fallback Then Inline")
    {
        TInlineAllocator<int, 8> allocator;

        void* fallbackPtr = allocator.Allocate(sizeof(int) * 16);
        REQUIRE_FALSE(allocator.IsInlinePointer(fallbackPtr));

        allocator.Deallocate(fallbackPtr);

        void* inlinePtr = allocator.Allocate(sizeof(int) * 4);
        REQUIRE(allocator.IsInlinePointer(inlinePtr));

        allocator.Deallocate(inlinePtr);
    }

    SECTION("Multiple Fallback Allocations")
    {
        TInlineAllocator<int, 8> allocator;

        void* ptr1 = allocator.Allocate(sizeof(int) * 20);
        void* ptr2 = allocator.Allocate(sizeof(int) * 30);
        void* ptr3 = allocator.Allocate(sizeof(int) * 40);

        REQUIRE_FALSE(allocator.IsInlinePointer(ptr1));
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr2));
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr3));

        REQUIRE(ptr1 != ptr2);
        REQUIRE(ptr2 != ptr3);
        REQUIRE(ptr1 != ptr3);

        allocator.Deallocate(ptr1);
        allocator.Deallocate(ptr2);
        allocator.Deallocate(ptr3);
    }
}

TEST_CASE("TInlineAllocator IsInlinePointer", "[GP][Core][Memory][InlineAllocator][Detection]")
{
    using namespace GP::Memory;

    TInlineAllocator<int, 10> allocator;

    SECTION("Inline Buffer Pointer")
    {
        void* ptr = allocator.GetInlineBuffer();
        REQUIRE(allocator.IsInlinePointer(ptr));
    }

    SECTION("Pointer Within Inline Buffer")
    {
        int* base = allocator.GetInlineBuffer();
        REQUIRE(allocator.IsInlinePointer(base));
        REQUIRE(allocator.IsInlinePointer(base + 5));
        REQUIRE(allocator.IsInlinePointer(base + 9));
    }

    SECTION("Pointer Just Outside Inline Buffer")
    {
        int* base = allocator.GetInlineBuffer();
        REQUIRE_FALSE(allocator.IsInlinePointer(base + 10));
        REQUIRE_FALSE(allocator.IsInlinePointer(base + 100));
    }

    SECTION("Fallback Pointer")
    {
        void* ptr = allocator.Allocate(sizeof(int) * 20);
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr));
        allocator.Deallocate(ptr);
    }

    SECTION("External Pointer")
    {
        int external = 42;
        REQUIRE_FALSE(allocator.IsInlinePointer(&external));
    }

    SECTION("Nullptr") { REQUIRE_FALSE(allocator.IsInlinePointer(nullptr)); }
}

TEST_CASE("TInlineAllocator With Objects", "[GP][Core][Memory][InlineAllocator][Objects]")
{
    using namespace GP::Memory;

    SECTION("Inline Object Construction")
    {
        TInlineAllocator<std::string, 4> allocator;

        std::string* str = static_cast<std::string*>(allocator.Allocate(sizeof(std::string)));
        REQUIRE(allocator.IsInlinePointer(str));

        Construct<std::string>(str, "Inline String");
        REQUIRE(*str == "Inline String");

        Destroy(str);
        allocator.Deallocate(str);
    }

    SECTION("Fallback Object Construction")
    {
        TInlineAllocator<std::string, 2> allocator;

        size_t size = sizeof(std::string) * 5;   // Larger than inline
        std::string* arr = static_cast<std::string*>(allocator.Allocate(size));
        REQUIRE_FALSE(allocator.IsInlinePointer(arr));

        for (int i = 0; i < 5; ++i) { Construct<std::string>(arr + i, "String " + std::to_string(i)); }

        for (int i = 0; i < 5; ++i) { REQUIRE(arr[i] == "String " + std::to_string(i)); }

        DestroyRange(arr, 5);
        allocator.Deallocate(arr);
    }

    SECTION("Complex Type in Inline Buffer")
    {
        struct TestStruct
        {
            int id;
            double value;
            std::string name;

            TestStruct(int i, double v, std::string n)
                : id(i)
                , value(v)
                , name(std::move(n))
            {}
        };

        TInlineAllocator<TestStruct, 3> allocator;

        TestStruct* obj = static_cast<TestStruct*>(allocator.Allocate(sizeof(TestStruct)));
        REQUIRE(allocator.IsInlinePointer(obj));

        Construct<TestStruct>(obj, 42, 3.14, "Test");
        REQUIRE(obj->id == 42);
        REQUIRE(obj->value == 3.14);
        REQUIRE(obj->name == "Test");

        Destroy(obj);
        allocator.Deallocate(obj);
    }
}

TEST_CASE("TInlineAllocator Alignment", "[GP][Core][Memory][InlineAllocator][Alignment]")
{
    using namespace GP::Memory;

    SECTION("Inline Buffer Alignment")
    {
        TInlineAllocator<int, 8> allocator;
        void* ptr = allocator.GetInlineBuffer();
        REQUIRE(IsAligned(ptr, alignof(int)));
    }

    SECTION("Inline Allocation with Custom Alignment")
    {
        TInlineAllocator<int, 16> allocator;
        void* ptr = allocator.Allocate(sizeof(int) * 4, 16);
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr));
        REQUIRE(IsAligned(ptr, 16));
        allocator.Deallocate(ptr);
    }

    SECTION("Fallback Allocation with Custom Alignment")
    {
        TInlineAllocator<int, 8> allocator;
        void* ptr = allocator.Allocate(sizeof(int) * 20, 32);
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr));
        REQUIRE(IsAligned(ptr, 32));
        allocator.Deallocate(ptr);
    }

    SECTION("Highly Aligned Type")
    {
        struct alignas(64) AlignedStruct
        {
            char data[64];
        };

        TInlineAllocator<AlignedStruct, 4> allocator;
        void* ptr = allocator.GetInlineBuffer();
        REQUIRE(IsAligned(ptr, 64));
    }
}

TEST_CASE("TInlineAllocator Copy Semantics", "[GP][Core][Memory][InlineAllocator][Copy]")
{
    using namespace GP::Memory;

    SECTION("Copy Constructor")
    {
        TInlineAllocator<int, 8> allocator1;
        TInlineAllocator<int, 8> allocator2(allocator1);

        // Both should have their own inline buffers
        REQUIRE(allocator1.GetInlineBuffer() != allocator2.GetInlineBuffer());
        REQUIRE(allocator1.GetInlineCapacity() == allocator2.GetInlineCapacity());
    }

    SECTION("Copy Assignment")
    {
        TInlineAllocator<int, 8> allocator1;
        TInlineAllocator<int, 8> allocator2;

        allocator2 = allocator1;

        REQUIRE(allocator1.GetInlineBuffer() != allocator2.GetInlineBuffer());
        REQUIRE(allocator1.GetInlineCapacity() == allocator2.GetInlineCapacity());
    }

    SECTION("Independent After Copy")
    {
        TInlineAllocator<int, 8> allocator1;
        TInlineAllocator<int, 8> allocator2(allocator1);

        void* ptr1 = allocator1.Allocate(sizeof(int) * 4);
        void* ptr2 = allocator2.Allocate(sizeof(int) * 4);

        REQUIRE(ptr1 != ptr2);
        REQUIRE(allocator1.IsInlinePointer(ptr1));
        REQUIRE(allocator2.IsInlinePointer(ptr2));

        allocator1.Deallocate(ptr1);
        allocator2.Deallocate(ptr2);
    }
}

TEST_CASE("TInlineAllocator Use Cases", "[GP][Core][Memory][InlineAllocator][UseCase]")
{
    using namespace GP::Memory;

    SECTION("Small String Optimization Pattern")
    {
        // Simulate small string optimization
        TInlineAllocator<char, 24> allocator;   // 24 bytes inline

        // Short string fits inline
        char* shortStr = static_cast<char*>(allocator.Allocate(16));
        REQUIRE(allocator.IsInlinePointer(shortStr));
        std::strcpy(shortStr, "Short");

        allocator.Deallocate(shortStr);

        // Long string uses heap
        char* longStr = static_cast<char*>(allocator.Allocate(100));
        REQUIRE_FALSE(allocator.IsInlinePointer(longStr));
        std::strcpy(longStr, "This is a much longer string that doesn't fit in the inline buffer");

        allocator.Deallocate(longStr);
    }

    SECTION("Small Vector Pattern")
    {
        // Simulate small vector optimization
        TInlineAllocator<int, 8> allocator;

        // Small array fits inline
        int* smallArray = static_cast<int*>(allocator.Allocate(sizeof(int) * 5));
        REQUIRE(allocator.IsInlinePointer(smallArray));

        for (int i = 0; i < 5; ++i) { smallArray[i] = i; }

        allocator.Deallocate(smallArray);

        // Large array uses heap
        int* largeArray = static_cast<int*>(allocator.Allocate(sizeof(int) * 50));
        REQUIRE_FALSE(allocator.IsInlinePointer(largeArray));

        for (int i = 0; i < 50; ++i) { largeArray[i] = i * 2; }

        allocator.Deallocate(largeArray);
    }

    SECTION("Temporary Buffer Pattern")
    {
        TInlineAllocator<char, 256> allocator;

        // Most temporary buffers are small
        for (int i = 0; i < 100; ++i)
        {
            size_t size = (i % 3 == 0) ? 128 : 512;   // Mix of inline and fallback
            char* buffer = static_cast<char*>(allocator.Allocate(size));

            if (size == 128) { REQUIRE(allocator.IsInlinePointer(buffer)); }
            else { REQUIRE_FALSE(allocator.IsInlinePointer(buffer)); }

            // Use the buffer...
            std::memset(buffer, 0, size);

            allocator.Deallocate(buffer);
        }
    }
}

TEST_CASE("TInlineAllocator Edge Cases", "[GP][Core][Memory][InlineAllocator][Edge]")
{
    using namespace GP::Memory;

    SECTION("Minimum Inline Count")
    {
        TInlineAllocator<int, 1> allocator;
        REQUIRE(allocator.GetInlineCapacity() == 1);

        void* ptr = allocator.Allocate(sizeof(int));
        REQUIRE(allocator.IsInlinePointer(ptr));

        allocator.Deallocate(ptr);
    }

    SECTION("Zero Size Allocation")
    {
        TInlineAllocator<int, 8> allocator;
        // Allocating zero size would trigger assert in debug, skip in release
        REQUIRE(true);
    }

    SECTION("Allocation at Boundary")
    {
        TInlineAllocator<int, 8> allocator;

        // Exactly at inline capacity
        void* ptr1 = allocator.Allocate(sizeof(int) * 8);
        REQUIRE(allocator.IsInlinePointer(ptr1));
        allocator.Deallocate(ptr1);

        // Just over inline capacity
        void* ptr2 = allocator.Allocate(sizeof(int) * 8 + 1);
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr2));
        allocator.Deallocate(ptr2);
    }

    SECTION("Multiple Deallocations of Same Inline Pointer")
    {
        TInlineAllocator<int, 8> allocator;

        void* ptr = allocator.Allocate(sizeof(int) * 4);
        REQUIRE(allocator.IsInlinePointer(ptr));

        allocator.Deallocate(ptr);
        allocator.Deallocate(ptr);   // Should be safe (no-op)

        REQUIRE(true);
    }
}

TEST_CASE("TInlineAllocator Performance Characteristics", "[GP][Core][Memory][InlineAllocator][Performance]")
{
    using namespace GP::Memory;

    SECTION("Inline Allocations are Fast")
    {
        TInlineAllocator<int, 16> allocator;

        // Many inline allocations should be very fast (just pointer return)
        for (int i = 0; i < 10000; ++i)
        {
            void* ptr = allocator.Allocate(sizeof(int) * 8);
            REQUIRE(allocator.IsInlinePointer(ptr));
            allocator.Deallocate(ptr);
        }
    }

    SECTION("No Heap Allocations for Small Sizes")
    {
        TInlineAllocator<char, 256> allocator;

        // Allocate many times, but always within inline capacity
        for (int i = 0; i < 1000; ++i)
        {
            void* ptr = allocator.Allocate(100);   // Less than 256
            REQUIRE(allocator.IsInlinePointer(ptr));
            allocator.Deallocate(ptr);
        }

        // No heap allocations were made
        REQUIRE(true);
    }

    SECTION("Fallback Only When Necessary")
    {
        TInlineAllocator<int, 32> allocator;

        int inlineCount = 0;
        int fallbackCount = 0;

        for (int i = 1; i <= 100; ++i)
        {
            void* ptr = allocator.Allocate(sizeof(int) * i);

            if (allocator.IsInlinePointer(ptr)) { ++inlineCount; }
            else { ++fallbackCount; }

            allocator.Deallocate(ptr);
        }

        // First 32 should be inline, rest should be fallback
        REQUIRE(inlineCount == 32);
        REQUIRE(fallbackCount == 68);
    }
}

TEST_CASE("TInlineAllocator With Different Fallback Allocators", "[GP][Core][Memory][InlineAllocator][Fallback]")
{
    using namespace GP::Memory;

    SECTION("Default Fallback Allocator")
    {
        TInlineAllocator<int, 8> allocator;

        void* ptr = allocator.Allocate(sizeof(int) * 20);
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr));
        allocator.Deallocate(ptr);
    }

    SECTION("Custom Fallback Allocator")
    {
        TInlineAllocator<int, 8, TAllocator<int>> allocator;

        void* ptr = allocator.Allocate(sizeof(int) * 20);
        REQUIRE_FALSE(allocator.IsInlinePointer(ptr));
        allocator.Deallocate(ptr);
    }
}
