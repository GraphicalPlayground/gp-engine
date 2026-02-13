// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/MemoryUtilities.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>

TEST_CASE("AlignUp", "[GP][Core][Memory][Utilities][Alignment]")
{
    using namespace GP::Memory;

    SECTION("Already Aligned")
    {
        REQUIRE(AlignUp(0, 4) == 0);
        REQUIRE(AlignUp(4, 4) == 4);
        REQUIRE(AlignUp(8, 4) == 8);
        REQUIRE(AlignUp(16, 16) == 16);
        REQUIRE(AlignUp(64, 64) == 64);
    }

    SECTION("Needs Alignment")
    {
        REQUIRE(AlignUp(1, 4) == 4);
        REQUIRE(AlignUp(3, 4) == 4);
        REQUIRE(AlignUp(5, 8) == 8);
        REQUIRE(AlignUp(7, 8) == 8);
        REQUIRE(AlignUp(9, 16) == 16);
        REQUIRE(AlignUp(15, 16) == 16);
    }

    SECTION("Large Values")
    {
        REQUIRE(AlignUp(1000, 64) == 1024);
        REQUIRE(AlignUp(1024, 64) == 1024);
        REQUIRE(AlignUp(1025, 64) == 1088);
    }

    SECTION("Power of Two Alignments")
    {
        REQUIRE(AlignUp(1, 1) == 1);
        REQUIRE(AlignUp(1, 2) == 2);
        REQUIRE(AlignUp(1, 4) == 4);
        REQUIRE(AlignUp(1, 8) == 8);
        REQUIRE(AlignUp(1, 16) == 16);
        REQUIRE(AlignUp(1, 32) == 32);
        REQUIRE(AlignUp(1, 64) == 64);
        REQUIRE(AlignUp(1, 128) == 128);
        REQUIRE(AlignUp(1, 256) == 256);
    }

    SECTION("Constexpr Evaluation")
    {
        constexpr auto aligned = AlignUp(7, 4);
        REQUIRE(aligned == 8);
    }
}

TEST_CASE("AlignDown", "[GP][Core][Memory][Utilities][Alignment]")
{
    using namespace GP::Memory;

    SECTION("Already Aligned")
    {
        REQUIRE(AlignDown(0, 4) == 0);
        REQUIRE(AlignDown(4, 4) == 4);
        REQUIRE(AlignDown(8, 4) == 8);
        REQUIRE(AlignDown(16, 16) == 16);
    }

    SECTION("Needs Alignment")
    {
        REQUIRE(AlignDown(1, 4) == 0);
        REQUIRE(AlignDown(3, 4) == 0);
        REQUIRE(AlignDown(5, 4) == 4);
        REQUIRE(AlignDown(7, 8) == 0);
        REQUIRE(AlignDown(9, 8) == 8);
        REQUIRE(AlignDown(15, 16) == 0);
    }

    SECTION("Large Values")
    {
        REQUIRE(AlignDown(1000, 64) == 960);
        REQUIRE(AlignDown(1024, 64) == 1024);
        REQUIRE(AlignDown(1088, 64) == 1088);
    }

    SECTION("Constexpr Evaluation")
    {
        constexpr auto aligned = AlignDown(7, 4);
        REQUIRE(aligned == 4);
    }
}

TEST_CASE("IsPowerOfTwo", "[GP][Core][Memory][Utilities][Alignment]")
{
    using namespace GP::Memory;

    SECTION("Valid Powers of Two")
    {
        REQUIRE(IsPowerOfTwo(1));
        REQUIRE(IsPowerOfTwo(2));
        REQUIRE(IsPowerOfTwo(4));
        REQUIRE(IsPowerOfTwo(8));
        REQUIRE(IsPowerOfTwo(16));
        REQUIRE(IsPowerOfTwo(32));
        REQUIRE(IsPowerOfTwo(64));
        REQUIRE(IsPowerOfTwo(128));
        REQUIRE(IsPowerOfTwo(256));
        REQUIRE(IsPowerOfTwo(512));
        REQUIRE(IsPowerOfTwo(1024));
    }

    SECTION("Non-Powers of Two")
    {
        REQUIRE_FALSE(IsPowerOfTwo(0));
        REQUIRE_FALSE(IsPowerOfTwo(3));
        REQUIRE_FALSE(IsPowerOfTwo(5));
        REQUIRE_FALSE(IsPowerOfTwo(6));
        REQUIRE_FALSE(IsPowerOfTwo(7));
        REQUIRE_FALSE(IsPowerOfTwo(9));
        REQUIRE_FALSE(IsPowerOfTwo(10));
        REQUIRE_FALSE(IsPowerOfTwo(15));
        REQUIRE_FALSE(IsPowerOfTwo(100));
        REQUIRE_FALSE(IsPowerOfTwo(1000));
    }

    SECTION("Edge Cases")
    {
        REQUIRE(IsPowerOfTwo(1));   // 2^0
        REQUIRE_FALSE(IsPowerOfTwo(0));
    }

    SECTION("Constexpr Evaluation")
    {
        constexpr bool isPow2 = IsPowerOfTwo(16);
        REQUIRE(isPow2);
        constexpr bool notPow2 = IsPowerOfTwo(15);
        REQUIRE_FALSE(notPow2);
    }
}

TEST_CASE("IsAligned", "[GP][Core][Memory][Utilities][Alignment]")
{
    using namespace GP::Memory;

    SECTION("Aligned Pointers")
    {
        alignas(16) char buffer[64];
        REQUIRE(IsAligned(buffer, 1));
        REQUIRE(IsAligned(buffer, 2));
        REQUIRE(IsAligned(buffer, 4));
        REQUIRE(IsAligned(buffer, 8));
        REQUIRE(IsAligned(buffer, 16));
    }

    SECTION("Misaligned Pointers")
    {
        alignas(16) char buffer[64];
        char* misaligned = buffer + 1;

        REQUIRE(IsAligned(misaligned, 1));
        REQUIRE_FALSE(IsAligned(misaligned, 2));
        REQUIRE_FALSE(IsAligned(misaligned, 4));
        REQUIRE_FALSE(IsAligned(misaligned, 8));
        REQUIRE_FALSE(IsAligned(misaligned, 16));
    }

    SECTION("Offset Aligned Pointers")
    {
        alignas(64) char buffer[128];
        REQUIRE(IsAligned(buffer + 0, 64));
        REQUIRE(IsAligned(buffer + 64, 64));
        REQUIRE_FALSE(IsAligned(buffer + 32, 64));
        REQUIRE(IsAligned(buffer + 32, 32));
    }

    SECTION("Nullptr")
    {
        // nullptr is technically aligned to any alignment
        REQUIRE(IsAligned(nullptr, 1));
        REQUIRE(IsAligned(nullptr, 4));
        REQUIRE(IsAligned(nullptr, 16));
    }

    SECTION("Compile-Time Constants")
    {
        // Note: While IsAligned is constexpr, pointer-to-integer conversion isn't constexpr,
        // so we verify the alignment at runtime with compile-time aligned buffers
        alignas(16) static constexpr char buffer[16] = {};
        REQUIRE(IsAligned(buffer, 16));
    }
}

TEST_CASE("Construct and Destroy", "[GP][Core][Memory][Utilities][Construction]")
{
    using namespace GP::Memory;

    SECTION("Trivial Type")
    {
        alignas(int) char buffer[sizeof(int)];
        int* ptr = Construct<int>(buffer, 42);
        REQUIRE(*ptr == 42);
        Destroy(ptr);
    }

    SECTION("Non-Trivial Type")
    {
        alignas(std::string) char buffer[sizeof(std::string)];
        std::string* ptr = Construct<std::string>(buffer, "Hello");
        REQUIRE(*ptr == "Hello");
        Destroy(ptr);
    }

    SECTION("Multiple Arguments")
    {
        struct TestStruct
        {
            int a;
            float b;
            std::string c;

            TestStruct(int x, float y, std::string z)
                : a(x)
                , b(y)
                , c(std::move(z))
            {}
        };

        alignas(TestStruct) char buffer[sizeof(TestStruct)];
        TestStruct* ptr = Construct<TestStruct>(buffer, 10, 3.14f, "Test");
        REQUIRE(ptr->a == 10);
        REQUIRE(ptr->b == 3.14f);
        REQUIRE(ptr->c == "Test");
        Destroy(ptr);
    }

    SECTION("Destroy Nullptr")
    {
        int* ptr = nullptr;
        Destroy(ptr);   // Should not crash
        REQUIRE(true);
    }
}

TEST_CASE("ConstructRange", "[GP][Core][Memory][Utilities][Construction]")
{
    using namespace GP::Memory;

    SECTION("Trivial Type")
    {
        alignas(int) char buffer[sizeof(int) * 5];
        int* arr = reinterpret_cast<int*>(buffer);
        ConstructRange(arr, 5);

        for (size_t i = 0; i < 5; ++i) { REQUIRE(arr[i] == 0); }
    }

    SECTION("Non-Trivial Type")
    {
        alignas(std::string) char buffer[sizeof(std::string) * 3];
        std::string* arr = reinterpret_cast<std::string*>(buffer);
        ConstructRange(arr, 3);

        for (size_t i = 0; i < 3; ++i)
        {
            REQUIRE(arr[i].empty());
            DestroyRange(arr, 3);
        }
    }

    SECTION("Zero Count")
    {
        alignas(int) char buffer[sizeof(int) * 5];
        int* arr = reinterpret_cast<int*>(buffer);
        ConstructRange(arr, 0);   // Should not crash
        REQUIRE(true);
    }
}

TEST_CASE("DestroyRange", "[GP][Core][Memory][Utilities][Construction]")
{
    using namespace GP::Memory;

    SECTION("Trivial Type")
    {
        int arr[5] = { 1, 2, 3, 4, 5 };
        DestroyRange(arr, 5);   // Should be no-op
        REQUIRE(true);
    }

    SECTION("Non-Trivial Type")
    {
        alignas(std::string) char buffer[sizeof(std::string) * 3];
        std::string* arr = reinterpret_cast<std::string*>(buffer);
        for (size_t i = 0; i < 3; ++i) { Construct<std::string>(arr + i, "Test"); }

        DestroyRange(arr, 3);   // Should call destructors
        REQUIRE(true);
    }

    SECTION("Zero Count")
    {
        int arr[5];
        DestroyRange(arr, 0);   // Should not crash
        REQUIRE(true);
    }
}

TEST_CASE("CopyConstructRange", "[GP][Core][Memory][Utilities][Construction]")
{
    using namespace GP::Memory;

    SECTION("Trivial Type")
    {
        int src[5] = { 1, 2, 3, 4, 5 };
        alignas(int) char buffer[sizeof(int) * 5];
        int* dest = reinterpret_cast<int*>(buffer);

        CopyConstructRange(dest, src, 5);

        for (size_t i = 0; i < 5; ++i) { REQUIRE(dest[i] == src[i]); }
    }

    SECTION("Non-Trivial Type")
    {
        std::string src[3] = { "one", "two", "three" };
        alignas(std::string) char buffer[sizeof(std::string) * 3];
        std::string* dest = reinterpret_cast<std::string*>(buffer);

        CopyConstructRange(dest, src, 3);

        for (size_t i = 0; i < 3; ++i)
        {
            REQUIRE(dest[i] == src[i]);
            REQUIRE(src[i] == dest[i]);   // Source unchanged
        }

        DestroyRange(dest, 3);
    }
}

TEST_CASE("MoveConstructRange", "[GP][Core][Memory][Utilities][Construction]")
{
    using namespace GP::Memory;

    SECTION("Trivial Type")
    {
        int src[5] = { 1, 2, 3, 4, 5 };
        alignas(int) char buffer[sizeof(int) * 5];
        int* dest = reinterpret_cast<int*>(buffer);

        MoveConstructRange(dest, src, 5);

        for (size_t i = 0; i < 5; ++i) { REQUIRE(dest[i] == i + 1); }
    }

    SECTION("Non-Trivial Type")
    {
        std::string original[3] = { "one", "two", "three" };
        std::string src[3];
        for (int i = 0; i < 3; ++i) { src[i] = original[i]; }

        alignas(std::string) char buffer[sizeof(std::string) * 3];
        std::string* dest = reinterpret_cast<std::string*>(buffer);

        MoveConstructRange(dest, src, 3);

        for (size_t i = 0; i < 3; ++i)
        {
            REQUIRE(dest[i] == original[i]);
            REQUIRE(src[i].empty());   // Source moved from
        }

        DestroyRange(dest, 3);
    }
}

TEST_CASE("RelocateRange", "[GP][Core][Memory][Utilities][Construction]")
{
    using namespace GP::Memory;

    SECTION("Trivial Type")
    {
        int src[5] = { 1, 2, 3, 4, 5 };
        alignas(int) char buffer[sizeof(int) * 5];
        int* dest = reinterpret_cast<int*>(buffer);

        RelocateRange(dest, src, 5);

        for (size_t i = 0; i < 5; ++i) { REQUIRE(dest[i] == i + 1); }
    }

    SECTION("Non-Trivial Type")
    {
        alignas(std::string) char srcBuffer[sizeof(std::string) * 3];
        std::string* src = reinterpret_cast<std::string*>(srcBuffer);
        Construct<std::string>(src + 0, "one");
        Construct<std::string>(src + 1, "two");
        Construct<std::string>(src + 2, "three");

        alignas(std::string) char destBuffer[sizeof(std::string) * 3];
        std::string* dest = reinterpret_cast<std::string*>(destBuffer);

        RelocateRange(dest, src, 3);

        REQUIRE(dest[0] == "one");
        REQUIRE(dest[1] == "two");
        REQUIRE(dest[2] == "three");

        DestroyRange(dest, 3);
    }
}

TEST_CASE("CopyAssignRange", "[GP][Core][Memory][Utilities][Assignment]")
{
    using namespace GP::Memory;

    SECTION("Trivial Type")
    {
        int src[5] = { 1, 2, 3, 4, 5 };
        int dest[5] = { 0, 0, 0, 0, 0 };

        CopyAssignRange(dest, src, 5);

        for (size_t i = 0; i < 5; ++i) { REQUIRE(dest[i] == src[i]); }
    }

    SECTION("Non-Trivial Type")
    {
        std::string src[3] = { "alpha", "beta", "gamma" };
        std::string dest[3] = { "old1", "old2", "old3" };

        CopyAssignRange(dest, src, 3);

        for (size_t i = 0; i < 3; ++i)
        {
            REQUIRE(dest[i] == src[i]);
            REQUIRE(src[i] == dest[i]);   // Source unchanged
        }
    }
}

TEST_CASE("MoveAssignRange", "[GP][Core][Memory][Utilities][Assignment]")
{
    using namespace GP::Memory;

    SECTION("Trivial Type")
    {
        int src[5] = { 1, 2, 3, 4, 5 };
        int dest[5] = { 0, 0, 0, 0, 0 };

        MoveAssignRange(dest, src, 5);

        for (size_t i = 0; i < 5; ++i) { REQUIRE(dest[i] == i + 1); }
    }

    SECTION("Non-Trivial Type")
    {
        std::string original[3] = { "alpha", "beta", "gamma" };
        std::string src[3];
        for (int i = 0; i < 3; ++i) { src[i] = original[i]; }

        std::string dest[3] = { "old1", "old2", "old3" };

        MoveAssignRange(dest, src, 3);

        for (size_t i = 0; i < 3; ++i)
        {
            REQUIRE(dest[i] == original[i]);
            REQUIRE(src[i].empty());   // Source moved from
        }
    }
}

TEST_CASE("UninitializedFill", "[GP][Core][Memory][Utilities][Construction]")
{
    using namespace GP::Memory;

    SECTION("Trivial Type")
    {
        alignas(int) char buffer[sizeof(int) * 5];
        int* arr = reinterpret_cast<int*>(buffer);

        UninitializedFill(arr, 5, 42);

        for (size_t i = 0; i < 5; ++i) { REQUIRE(arr[i] == 42); }
    }

    SECTION("Non-Trivial Type")
    {
        alignas(std::string) char buffer[sizeof(std::string) * 3];
        std::string* arr = reinterpret_cast<std::string*>(buffer);

        UninitializedFill(arr, 3, std::string("Hello"));

        for (size_t i = 0; i < 3; ++i) { REQUIRE(arr[i] == "Hello"); }

        DestroyRange(arr, 3);
    }

    SECTION("Zero Count")
    {
        alignas(int) char buffer[sizeof(int) * 5];
        int* arr = reinterpret_cast<int*>(buffer);
        UninitializedFill(arr, 0, 42);   // Should not crash
        REQUIRE(true);
    }
}
