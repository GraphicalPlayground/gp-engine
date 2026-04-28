// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/DefaultAllocator.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <string_view>
#include <thread>
#include <vector>

namespace gp::memory::tests
{

TEST_CASE("DefaultAllocator - Singleton identity", "[memory][DefaultAllocator]")
{
    // Ensure the allocator is a true singleton and returns the exact same memory address.
    auto& allocator1 = DefaultAllocator::get();
    auto& allocator2 = DefaultAllocator::get();

    REQUIRE(&allocator1 == &allocator2);
}

TEST_CASE("DefaultAllocator - Basic allocation", "[memory][DefaultAllocator]")
{
    auto& allocator = DefaultAllocator::get();
    constexpr USize allocSize = 256;

    void* ptr = allocator.allocate(allocSize);
    REQUIRE(ptr != nullptr);

    // Verify memory is accessible and writable (will segfault/ASAN trigger if invalid)
    auto* bytes = static_cast<gp::UInt8*>(ptr);
    for (USize i = 0; i < allocSize; ++i)
    {
        bytes[i] = static_cast<gp::UInt8>(i % 256);
    }

    // Verify written data remains intact
    volatile gp::UInt8 checkSum = bytes[128];
    REQUIRE(checkSum == 128);

    REQUIRE_NOTHROW(allocator.deallocate(ptr, allocSize));
}

TEST_CASE("DefaultAllocator - Alignment guarantees", "[memory][DefaultAllocator]")
{
    auto& allocator = DefaultAllocator::get();
    constexpr USize allocSize = 64;

    // Test a spectrum of power-of-two alignments critical for SIMD and cache lines
    constexpr std::array<USize, 6> alignments = { 8, 16, 32, 64, 128, 256 };

    for (const USize alignment: alignments)
    {
        void* ptr = allocator.allocate(allocSize, alignment);
        REQUIRE(ptr != nullptr);

        // Verify the hardware address is a multiple of the requested alignment
        const auto address = reinterpret_cast<gp::UIntPtr>(ptr);
        REQUIRE((address % alignment) == 0);

        allocator.deallocate(ptr, allocSize);
    }
}

TEST_CASE("DefaultAllocator - Nullptr Handling", "[memory][DefaultAllocator]")
{
    auto& allocator = DefaultAllocator::get();

    // The deallocate interface must safely ignore nullptr requests.
    REQUIRE_NOTHROW(allocator.deallocate(nullptr, 1024));
}

TEST_CASE("DefaultAllocator - Debug Name", "[memory][DefaultAllocator]")
{
    const auto& allocator = DefaultAllocator::get();

    // Using string_view for allocation-free string comparison
    REQUIRE(std::string_view(allocator.getDebugName()) == "DefaultAllocator");
}

#if GP_BUILD_DEBUG
TEST_CASE("DefaultAllocator - Debug Allocation Statistics", "[memory][DefaultAllocator]")
{
    auto& allocator = DefaultAllocator::get();

    // Cache initial state to ensure tests run reliably independent of execution order
    const auto initialSize = allocator.getAllocatedSize();
    const auto initialCount = allocator.getAllocationCount();

    void* ptr1 = allocator.allocate(128);
    REQUIRE(allocator.getAllocatedSize() == initialSize + 128);
    REQUIRE(allocator.getAllocationCount() == initialCount + 1);

    void* ptr2 = allocator.allocate(256);
    REQUIRE(allocator.getAllocatedSize() == initialSize + 128 + 256);
    REQUIRE(allocator.getAllocationCount() == initialCount + 2);

    allocator.deallocate(ptr1, 128);
    REQUIRE(allocator.getAllocatedSize() == initialSize + 256);
    REQUIRE(allocator.getAllocationCount() == initialCount + 1);

    // Nullptr deallocation should strictly NOT modify statistics
    allocator.deallocate(nullptr, 512);
    REQUIRE(allocator.getAllocatedSize() == initialSize + 256);
    REQUIRE(allocator.getAllocationCount() == initialCount + 1);

    allocator.deallocate(ptr2, 256);
    REQUIRE(allocator.getAllocatedSize() == initialSize);
    REQUIRE(allocator.getAllocationCount() == initialCount);
}
#endif   // GP_BUILD_DEBUG

TEST_CASE("DefaultAllocator - Multithreaded Contention", "[memory][DefaultAllocator]")
{
    // Validates the class comment: "Thread-safe by virtue of OS-level guarantees on malloc/free."
    auto& allocator = DefaultAllocator::get();

    constexpr USize numThreads = 8;
    constexpr USize allocationsPerThread = 5000;
    constexpr USize allocSize = 32;

#if GP_BUILD_DEBUG
    const auto initialSize = allocator.getAllocatedSize();
    const auto initialCount = allocator.getAllocationCount();
#endif

    auto worker = [&allocator]()
    {
        std::vector<void*> ptrs;
        ptrs.reserve(allocationsPerThread);

        // Heavy allocation phase
        for (USize i = 0; i < allocationsPerThread; ++i)
        {
            ptrs.push_back(allocator.allocate(allocSize));
        }

        // Deallocation phase
        for (void* ptr: ptrs)
        {
            allocator.deallocate(ptr, allocSize);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (USize i = 0; i < numThreads; ++i)
    {
        threads.emplace_back(worker);
    }

    for (auto& t: threads)
    {
        t.join();
    }

    // Ensure atomic counters are perfectly resolved after high contention
#if GP_BUILD_DEBUG
    REQUIRE(allocator.getAllocatedSize() == initialSize);
    REQUIRE(allocator.getAllocationCount() == initialCount);
#endif
}

}   // namespace gp::memory::tests
