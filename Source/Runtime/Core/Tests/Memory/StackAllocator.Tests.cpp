// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/StackAllocator.hpp"
#include "Memory/MemoryUtilities.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>

TEST_CASE("TStackAllocator Internal Buffer Construction", "[GP][Core][Memory][StackAllocator][Construction]")
{
    using namespace GP::Memory;

    SECTION("Fixed Size Buffer - Default Alignment")
    {
        TStackAllocator<1024> allocator;

        REQUIRE(allocator.GetCapacityBytes() == 1024);
        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 1024);
    }

    SECTION("Fixed Size Buffer - Custom Alignment")
    {
        TStackAllocator<2048, 64> allocator;

        REQUIRE(allocator.GetCapacityBytes() == 2048);
        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 2048);
    }

    SECTION("Small Buffer")
    {
        TStackAllocator<64> allocator;

        REQUIRE(allocator.GetCapacityBytes() == 64);
        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 64);
    }
}

TEST_CASE("TStackAllocator External Buffer Construction", "[GP][Core][Memory][StackAllocator][Construction]")
{
    using namespace GP::Memory;

    SECTION("External Buffer")
    {
        alignas(16) char buffer[1024];
        TStackAllocator<0, 16> allocator(buffer, 1024);

        REQUIRE(allocator.GetCapacityBytes() == 1024);
        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 1024);
    }

    SECTION("Large External Buffer")
    {
        alignas(64) char buffer[8192];
        TStackAllocator<0, 64> allocator(buffer, 8192);

        REQUIRE(allocator.GetCapacityBytes() == 8192);
        REQUIRE(allocator.GetUsedBytes() == 0);
        REQUIRE(allocator.GetRemainingBytes() == 8192);
    }
}

TEST_CASE("TStackAllocator Basic Allocation", "[GP][Core][Memory][StackAllocator][Allocation]")
{
    using namespace GP::Memory;

    TStackAllocator<1024> allocator;

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

TEST_CASE("TStackAllocator Marker Operations", "[GP][Core][Memory][StackAllocator][Marker]")
{
    using namespace GP::Memory;

    TStackAllocator<1024> allocator;

    SECTION("Save and Restore Marker")
    {
        auto marker1 = allocator.SaveMarker();
        REQUIRE(marker1.offset == 0);

        (void)allocator.Allocate(64);
        auto marker2 = allocator.SaveMarker();
        REQUIRE(marker2.offset == 64);

        (void)allocator.Allocate(128);
        REQUIRE(allocator.GetUsedBytes() == 64 + 128);

        // Rewind to marker2
        allocator.FreeToMarker(marker2);
        REQUIRE(allocator.GetUsedBytes() == 64);

        // Rewind to marker1
        allocator.FreeToMarker(marker1);
        REQUIRE(allocator.GetUsedBytes() == 0);
    }

    SECTION("Nested Markers")
    {
        auto outer = allocator.SaveMarker();
        (void)allocator.Allocate(64);

        auto middle = allocator.SaveMarker();
        (void)allocator.Allocate(128);

        auto inner = allocator.SaveMarker();
        (void)allocator.Allocate(256);

        REQUIRE(allocator.GetUsedBytes() == 64 + 128 + 256);

        // Unwind in LIFO order
        allocator.FreeToMarker(inner);
        REQUIRE(allocator.GetUsedBytes() == 64 + 128);

        allocator.FreeToMarker(middle);
        REQUIRE(allocator.GetUsedBytes() == 64);

        allocator.FreeToMarker(outer);
        REQUIRE(allocator.GetUsedBytes() == 0);
    }

    SECTION("Multiple Allocations Between Markers")
    {
        auto marker1 = allocator.SaveMarker();

        (void)allocator.Allocate(32);
        (void)allocator.Allocate(64);
        (void)allocator.Allocate(96);

        auto marker2 = allocator.SaveMarker();
        size_t used = allocator.GetUsedBytes();
        REQUIRE(used == 32 + 64 + 96);

        (void)allocator.Allocate(128);
        (void)allocator.Allocate(256);

        REQUIRE(allocator.GetUsedBytes() > used);

        allocator.FreeToMarker(marker2);
        REQUIRE(allocator.GetUsedBytes() == used);

        allocator.FreeToMarker(marker1);
        REQUIRE(allocator.GetUsedBytes() == 0);
    }

    SECTION("Marker at Empty State")
    {
        auto marker = allocator.SaveMarker();
        REQUIRE(marker.offset == 0);

        (void)allocator.Allocate(512);
        REQUIRE(allocator.GetUsedBytes() == 512);

        allocator.FreeToMarker(marker);
        REQUIRE(allocator.GetUsedBytes() == 0);
    }

    SECTION("Save Marker Multiple Times at Same Position")
    {
        (void)allocator.Allocate(128);

        auto marker1 = allocator.SaveMarker();
        auto marker2 = allocator.SaveMarker();
        auto marker3 = allocator.SaveMarker();

        REQUIRE(marker1.offset == marker2.offset);
        REQUIRE(marker2.offset == marker3.offset);

        (void)allocator.Allocate(256);

        allocator.FreeToMarker(marker1);
        REQUIRE(allocator.GetUsedBytes() == 128);
    }
}

TEST_CASE("TStackAllocator Reset", "[GP][Core][Memory][StackAllocator][Reset]")
{
    using namespace GP::Memory;

    TStackAllocator<1024> allocator;

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

    SECTION("Reset is Equivalent to FreeToMarker(0)")
    {
        auto startMarker = allocator.SaveMarker();

        (void)allocator.Allocate(512);
        allocator.Reset();

        REQUIRE(allocator.GetUsedBytes() == startMarker.offset);
        REQUIRE(allocator.GetUsedBytes() == 0);
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

TEST_CASE("TStackAllocator Alignment", "[GP][Core][Memory][StackAllocator][Alignment]")
{
    using namespace GP::Memory;

    TStackAllocator<2048> allocator;

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

    SECTION("Mixed Alignments with Markers")
    {
        auto marker = allocator.SaveMarker();

        void* ptr1 = allocator.Allocate(1, 1);
        void* ptr2 = allocator.Allocate(1, 16);
        void* ptr3 = allocator.Allocate(1, 32);

        REQUIRE(IsAligned(ptr1, 1));
        REQUIRE(IsAligned(ptr2, 16));
        REQUIRE(IsAligned(ptr3, 32));

        allocator.FreeToMarker(marker);

        // Reallocate with different alignment
        void* ptr4 = allocator.Allocate(1, 64);
        REQUIRE(IsAligned(ptr4, 64));
    }
}

TEST_CASE("TStackAllocator Out of Memory", "[GP][Core][Memory][StackAllocator][OOM]")
{
    using namespace GP::Memory;

    TStackAllocator<256> allocator;

    SECTION("Fill Buffer Completely")
    {
        void* ptr = allocator.Allocate(256);
        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.GetRemainingBytes() == 0);

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

        void* ptr = allocator.Allocate(128);
        REQUIRE(ptr == nullptr);
    }

    SECTION("Free to Marker Then Reallocate")
    {
        auto marker = allocator.SaveMarker();
        void* ptr1 = allocator.Allocate(200);
        REQUIRE(ptr1 != nullptr);

        // Try to allocate more, should fail
        void* ptr2 = allocator.Allocate(100);
        REQUIRE(ptr2 == nullptr);

        // Free back to marker and try again
        allocator.FreeToMarker(marker);
        void* ptr3 = allocator.Allocate(200);
        REQUIRE(ptr3 != nullptr);
        REQUIRE(ptr3 == ptr1);   // Same address as before
    }
}

TEST_CASE("TStackAllocator Owns", "[GP][Core][Memory][StackAllocator][Ownership]")
{
    using namespace GP::Memory;

    TStackAllocator<1024> allocator;

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

    SECTION("Owns Freed Pointer")
    {
        auto marker = allocator.SaveMarker();
        void* ptr = allocator.Allocate(64);

        allocator.FreeToMarker(marker);

        // Pointer is still within buffer, so Owns returns true
        REQUIRE(allocator.Owns(ptr));
    }

    SECTION("Does Not Own External Pointer")
    {
        int external = 42;
        REQUIRE_FALSE(allocator.Owns(&external));
    }

    SECTION("Does Not Own Nullptr") { REQUIRE_FALSE(allocator.Owns(nullptr)); }
}

TEST_CASE("TStackAllocator Deallocate is No-Op", "[GP][Core][Memory][StackAllocator][Deallocation]")
{
    using namespace GP::Memory;

    TStackAllocator<1024> allocator;

    SECTION("Deallocate Does Not Free Memory")
    {
        void* ptr = allocator.Allocate(64);
        size_t usedBefore = allocator.GetUsedBytes();

        allocator.Deallocate(ptr);

        REQUIRE(allocator.GetUsedBytes() == usedBefore);
    }

    SECTION("Deallocate Nullptr")
    {
        allocator.Deallocate(nullptr);
        REQUIRE(true);
    }
}

TEST_CASE("TStackAllocator With Objects", "[GP][Core][Memory][StackAllocator][Objects]")
{
    using namespace GP::Memory;

    TStackAllocator<2048> allocator;

    SECTION("Construct and Destroy with Markers")
    {
        auto marker = allocator.SaveMarker();

        std::string* str = static_cast<std::string*>(allocator.Allocate(sizeof(std::string)));
        Construct<std::string>(str, "Hello Stack Allocator");
        REQUIRE(*str == "Hello Stack Allocator");

        Destroy(str);
        allocator.FreeToMarker(marker);
        REQUIRE(allocator.GetUsedBytes() == 0);
    }

    SECTION("Nested Scopes with Objects")
    {
        auto outer = allocator.SaveMarker();
        {
            std::string* str1 = static_cast<std::string*>(allocator.Allocate(sizeof(std::string)));
            Construct<std::string>(str1, "Outer");

            auto inner = allocator.SaveMarker();
            {
                std::string* str2 = static_cast<std::string*>(allocator.Allocate(sizeof(std::string)));
                Construct<std::string>(str2, "Inner");

                REQUIRE(*str1 == "Outer");
                REQUIRE(*str2 == "Inner");

                Destroy(str2);
            }
            allocator.FreeToMarker(inner);

            REQUIRE(*str1 == "Outer");
            Destroy(str1);
        }
        allocator.FreeToMarker(outer);
        REQUIRE(allocator.GetUsedBytes() == 0);
    }

    SECTION("Array of Objects")
    {
        auto marker = allocator.SaveMarker();

        constexpr size_t count = 5;
        std::string* strings = static_cast<std::string*>(allocator.Allocate(sizeof(std::string) * count));

        for (size_t i = 0; i < count; ++i) { Construct<std::string>(strings + i, "Item " + std::to_string(i)); }

        for (size_t i = 0; i < count; ++i) { REQUIRE(strings[i] == "Item " + std::to_string(i)); }

        DestroyRange(strings, count);
        allocator.FreeToMarker(marker);
    }
}

TEST_CASE("TStackAllocator Scope Pattern", "[GP][Core][Memory][StackAllocator][Pattern]")
{
    using namespace GP::Memory;

    TStackAllocator<4096> allocator;

    SECTION("Simulated Nested Function Calls")
    {
        // Function A
        auto markerA = allocator.SaveMarker();
        int* dataA = static_cast<int*>(allocator.Allocate(sizeof(int) * 10));
        for (int i = 0; i < 10; ++i) { dataA[i] = i; }

        {
            // Function B (called by A)
            auto markerB = allocator.SaveMarker();
            int* dataB = static_cast<int*>(allocator.Allocate(sizeof(int) * 20));
            for (int i = 0; i < 20; ++i) { dataB[i] = i * 2; }

            {
                // Function C (called by B)
                auto markerC = allocator.SaveMarker();
                int* dataC = static_cast<int*>(allocator.Allocate(sizeof(int) * 30));
                for (int i = 0; i < 30; ++i) { dataC[i] = i * 3; }

                REQUIRE(dataC[10] == 30);

                // Function C returns
                allocator.FreeToMarker(markerC);
            }

            REQUIRE(dataB[10] == 20);

            // Function B returns
            allocator.FreeToMarker(markerB);
        }

        REQUIRE(dataA[5] == 5);

        // Function A returns
        allocator.FreeToMarker(markerA);
        REQUIRE(allocator.GetUsedBytes() == 0);
    }

    SECTION("Simulated Render Job Submission")
    {
        // Main thread scope
        auto mainMarker = allocator.SaveMarker();

        // Allocate command buffer
        void* cmdBuffer = allocator.Allocate(512);
        REQUIRE(cmdBuffer != nullptr);

        // Job 1
        {
            auto jobMarker = allocator.SaveMarker();
            void* jobData = allocator.Allocate(256);
            REQUIRE(jobData != nullptr);

            // Process job...

            allocator.FreeToMarker(jobMarker);
        }

        // Job 2
        {
            auto jobMarker = allocator.SaveMarker();
            void* jobData = allocator.Allocate(128);
            REQUIRE(jobData != nullptr);

            // Process job...

            allocator.FreeToMarker(jobMarker);
        }

        // End of frame
        allocator.FreeToMarker(mainMarker);
        REQUIRE(allocator.GetUsedBytes() == 0);
    }
}

TEST_CASE("TStackAllocator Stress Test", "[GP][Core][Memory][StackAllocator][Stress]")
{
    using namespace GP::Memory;

    TStackAllocator<8192> allocator;

    SECTION("Deep Nesting")
    {
        constexpr int depth = 100;
        TStackAllocator<8192>::FMarker markers[depth];

        for (int i = 0; i < depth; ++i)
        {
            markers[i] = allocator.SaveMarker();
            (void)allocator.Allocate(8);
        }

        constexpr size_t align = alignof(std::max_align_t);

        size_t current = 0;
        size_t expectedOffsets[depth];

        for (int i = 0; i < depth; ++i)
        {
            expectedOffsets[i] = current;
            current = AlignUp(current, align);
            current += 8;
        }

        REQUIRE(allocator.GetUsedBytes() == current);

        // Unwind
        for (int i = depth - 1; i >= 0; --i)
        {
            allocator.FreeToMarker(markers[i]);
            REQUIRE(allocator.GetUsedBytes() == expectedOffsets[i]);
        }
    }

    SECTION("Many Allocation Cycles")
    {
        for (int cycle = 0; cycle < 1000; ++cycle)
        {
            auto marker = allocator.SaveMarker();
            (void)allocator.Allocate(100);
            allocator.FreeToMarker(marker);
            REQUIRE(allocator.GetUsedBytes() == 0);
        }
    }

    SECTION("Random Pattern")
    {
        auto marker1 = allocator.SaveMarker();
        (void)allocator.Allocate(100);

        auto marker2 = allocator.SaveMarker();
        (void)allocator.Allocate(200);

        auto marker3 = allocator.SaveMarker();
        (void)allocator.Allocate(300);

        // Non-LIFO unwind (but still valid)
        allocator.FreeToMarker(marker2);
        REQUIRE(allocator.GetUsedBytes() == 100);

        (void)allocator.Allocate(150);
        allocator.FreeToMarker(marker1);
        REQUIRE(allocator.GetUsedBytes() == 0);
    }
}

TEST_CASE("TStackAllocator Edge Cases", "[GP][Core][Memory][StackAllocator][Edge]")
{
    using namespace GP::Memory;

    SECTION("Marker at Full Buffer")
    {
        TStackAllocator<256> allocator;
        (void)allocator.Allocate(256);

        auto marker = allocator.SaveMarker();
        REQUIRE(marker.offset == 256);

        allocator.FreeToMarker(marker);
        REQUIRE(allocator.GetUsedBytes() == 256);
    }

    SECTION("Free to Same Marker Multiple Times")
    {
        TStackAllocator<1024> allocator;
        auto marker = allocator.SaveMarker();

        (void)allocator.Allocate(100);
        allocator.FreeToMarker(marker);
        REQUIRE(allocator.GetUsedBytes() == 0);

        (void)allocator.Allocate(200);
        allocator.FreeToMarker(marker);
        REQUIRE(allocator.GetUsedBytes() == 0);
    }

    SECTION("Minimum Size Buffer")
    {
        TStackAllocator<1> allocator;
        void* ptr = allocator.Allocate(1);
        REQUIRE(ptr != nullptr);
    }
}
