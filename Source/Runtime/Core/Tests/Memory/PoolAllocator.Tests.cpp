// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/PoolAllocator.hpp"
#include "Memory/MemoryUtilities.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <string>

TEST_CASE("TPoolAllocator Construction", "[GP][Core][Memory][PoolAllocator][Construction]")
{
    using namespace GP::Memory;

    SECTION("Basic Construction")
    {
        TPoolAllocator<64, 10> allocator;

        REQUIRE(allocator.GetBlockSize() == 64);
        REQUIRE(allocator.GetBlockCount() == 10);
        REQUIRE(allocator.GetAllocatedCount() == 0);
        REQUIRE(allocator.GetFreeCount() == 10);
        REQUIRE(allocator.IsEmpty());
        REQUIRE_FALSE(allocator.IsFull());
    }

    SECTION("Small Block Size")
    {
        TPoolAllocator<sizeof(void*), 5> allocator;

        REQUIRE(allocator.GetBlockSize() == sizeof(void*));
        REQUIRE(allocator.GetBlockCount() == 5);
    }

    SECTION("Large Block Size")
    {
        TPoolAllocator<1024, 100> allocator;

        REQUIRE(allocator.GetBlockSize() == 1024);
        REQUIRE(allocator.GetBlockCount() == 100);
        REQUIRE(allocator.IsEmpty());
    }

    SECTION("Custom Alignment")
    {
        TPoolAllocator<128, 10, 64> allocator;

        REQUIRE(allocator.GetBlockSize() == 128);
        REQUIRE(allocator.GetBlockCount() == 10);
    }
}

TEST_CASE("TPoolAllocator Basic Allocation", "[GP][Core][Memory][PoolAllocator][Allocation]")
{
    using namespace GP::Memory;

    TPoolAllocator<64, 10> allocator;

    SECTION("Single Allocation")
    {
        void* ptr = allocator.Allocate();
        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.Owns(ptr));
        REQUIRE(allocator.GetAllocatedCount() == 1);
        REQUIRE(allocator.GetFreeCount() == 9);
        REQUIRE_FALSE(allocator.IsEmpty());
        REQUIRE_FALSE(allocator.IsFull());

        allocator.Deallocate(ptr);
    }

    SECTION("Multiple Allocations")
    {
        void* ptr1 = allocator.Allocate();
        void* ptr2 = allocator.Allocate();
        void* ptr3 = allocator.Allocate();

        REQUIRE(ptr1 != nullptr);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr3 != nullptr);

        REQUIRE(ptr1 != ptr2);
        REQUIRE(ptr2 != ptr3);
        REQUIRE(ptr1 != ptr3);

        REQUIRE(allocator.GetAllocatedCount() == 3);
        REQUIRE(allocator.GetFreeCount() == 7);

        allocator.Deallocate(ptr1);
        allocator.Deallocate(ptr2);
        allocator.Deallocate(ptr3);
    }

    SECTION("Allocate All Blocks")
    {
        void* pointers[10];

        for (int i = 0; i < 10; ++i)
        {
            pointers[i] = allocator.Allocate();
            REQUIRE(pointers[i] != nullptr);
        }

        REQUIRE(allocator.GetAllocatedCount() == 10);
        REQUIRE(allocator.GetFreeCount() == 0);
        REQUIRE(allocator.IsFull());
        REQUIRE_FALSE(allocator.IsEmpty());

        for (int i = 0; i < 10; ++i) { allocator.Deallocate(pointers[i]); }
    }

    SECTION("Allocate with Size Parameter")
    {
        // Size parameter is ignored, but should work
        void* ptr = allocator.Allocate(32);
        REQUIRE(ptr != nullptr);
        allocator.Deallocate(ptr);
    }
}

TEST_CASE("TPoolAllocator Deallocation", "[GP][Core][Memory][PoolAllocator][Deallocation]")
{
    using namespace GP::Memory;

    TPoolAllocator<64, 10> allocator;

    SECTION("Deallocate Single Block")
    {
        void* ptr = allocator.Allocate();
        REQUIRE(allocator.GetAllocatedCount() == 1);

        allocator.Deallocate(ptr);

        REQUIRE(allocator.GetAllocatedCount() == 0);
        REQUIRE(allocator.GetFreeCount() == 10);
        REQUIRE(allocator.IsEmpty());
    }

    SECTION("Deallocate Multiple Blocks")
    {
        void* ptr1 = allocator.Allocate();
        void* ptr2 = allocator.Allocate();
        void* ptr3 = allocator.Allocate();

        REQUIRE(allocator.GetAllocatedCount() == 3);

        allocator.Deallocate(ptr1);
        REQUIRE(allocator.GetAllocatedCount() == 2);

        allocator.Deallocate(ptr2);
        REQUIRE(allocator.GetAllocatedCount() == 1);

        allocator.Deallocate(ptr3);
        REQUIRE(allocator.GetAllocatedCount() == 0);
        REQUIRE(allocator.IsEmpty());
    }

    SECTION("Deallocate in Reverse Order")
    {
        void* ptr1 = allocator.Allocate();
        void* ptr2 = allocator.Allocate();
        void* ptr3 = allocator.Allocate();

        allocator.Deallocate(ptr3);
        allocator.Deallocate(ptr2);
        allocator.Deallocate(ptr1);

        REQUIRE(allocator.IsEmpty());
    }

    SECTION("Deallocate Nullptr")
    {
        size_t countBefore = allocator.GetAllocatedCount();
        allocator.Deallocate(nullptr);
        REQUIRE(allocator.GetAllocatedCount() == countBefore);
    }
}

TEST_CASE("TPoolAllocator Reuse After Deallocation", "[GP][Core][Memory][PoolAllocator][Reuse]")
{
    using namespace GP::Memory;

    TPoolAllocator<64, 5> allocator;

    SECTION("Reuse Single Block")
    {
        void* ptr1 = allocator.Allocate();
        allocator.Deallocate(ptr1);

        void* ptr2 = allocator.Allocate();

        // Should reuse the same block
        REQUIRE(ptr1 == ptr2);
        allocator.Deallocate(ptr2);
    }

    SECTION("Reuse Multiple Blocks LIFO Order")
    {
        void* ptr1 = allocator.Allocate();
        void* ptr2 = allocator.Allocate();
        void* ptr3 = allocator.Allocate();

        allocator.Deallocate(ptr3);
        allocator.Deallocate(ptr2);
        allocator.Deallocate(ptr1);

        // Reallocate - should get them back in reverse order (LIFO free list)
        void* new1 = allocator.Allocate();
        void* new2 = allocator.Allocate();
        void* new3 = allocator.Allocate();

        REQUIRE(new1 == ptr1);
        REQUIRE(new2 == ptr2);
        REQUIRE(new3 == ptr3);

        allocator.Deallocate(new1);
        allocator.Deallocate(new2);
        allocator.Deallocate(new3);
    }

    SECTION("Interleaved Allocate and Deallocate")
    {
        void* ptr1 = allocator.Allocate();
        void* ptr2 = allocator.Allocate();

        allocator.Deallocate(ptr1);

        void* ptr3 = allocator.Allocate();   // Should reuse ptr1

        REQUIRE(ptr3 == ptr1);

        allocator.Deallocate(ptr2);
        allocator.Deallocate(ptr3);
    }
}

TEST_CASE("TPoolAllocator Pool Exhaustion", "[GP][Core][Memory][PoolAllocator][Exhaustion]")
{
    using namespace GP::Memory;

    TPoolAllocator<64, 5> allocator;

    SECTION("Allocate Beyond Capacity")
    {
        void* pointers[5];

        for (int i = 0; i < 5; ++i)
        {
            pointers[i] = allocator.Allocate();
            REQUIRE(pointers[i] != nullptr);
        }

        REQUIRE(allocator.IsFull());

        // Next allocation should fail
        void* ptr = allocator.Allocate();
        REQUIRE(ptr == nullptr);

        // Free one block and try again
        allocator.Deallocate(pointers[0]);
        REQUIRE_FALSE(allocator.IsFull());

        void* newPtr = allocator.Allocate();
        REQUIRE(newPtr != nullptr);
        REQUIRE(newPtr == pointers[0]);   // Reused block

        for (int i = 1; i < 5; ++i) { allocator.Deallocate(pointers[i]); }
        allocator.Deallocate(newPtr);
    }

    SECTION("Multiple Exhaustion Cycles")
    {
        for (int cycle = 0; cycle < 10; ++cycle)
        {
            void* pointers[5];

            for (int i = 0; i < 5; ++i) { pointers[i] = allocator.Allocate(); }

            REQUIRE(allocator.IsFull());

            void* overflow = allocator.Allocate();
            REQUIRE(overflow == nullptr);

            for (int i = 0; i < 5; ++i) { allocator.Deallocate(pointers[i]); }

            REQUIRE(allocator.IsEmpty());
        }
    }
}

TEST_CASE("TPoolAllocator Reset", "[GP][Core][Memory][PoolAllocator][Reset]")
{
    using namespace GP::Memory;

    TPoolAllocator<64, 10> allocator;

    SECTION("Reset After Allocations")
    {
        (void)allocator.Allocate();
        (void)allocator.Allocate();
        (void)allocator.Allocate();

        REQUIRE(allocator.GetAllocatedCount() == 3);

        allocator.Reset();

        REQUIRE(allocator.GetAllocatedCount() == 0);
        REQUIRE(allocator.GetFreeCount() == 10);
        REQUIRE(allocator.IsEmpty());
    }

    SECTION("Allocate After Reset")
    {
        void* ptr1 = allocator.Allocate();
        void* ptr2 = allocator.Allocate();

        allocator.Reset();

        void* ptr3 = allocator.Allocate();
        void* ptr4 = allocator.Allocate();

        // After reset, should get the first blocks again
        REQUIRE(ptr3 != nullptr);
        REQUIRE(ptr4 != nullptr);

        allocator.Deallocate(ptr3);
        allocator.Deallocate(ptr4);
    }

    SECTION("Reset Full Pool")
    {
        void* pointers[10];
        for (int i = 0; i < 10; ++i) { pointers[i] = allocator.Allocate(); }

        REQUIRE(allocator.IsFull());

        allocator.Reset();

        REQUIRE(allocator.IsEmpty());
        REQUIRE(allocator.GetFreeCount() == 10);

        // Should be able to allocate again
        void* ptr = allocator.Allocate();
        REQUIRE(ptr != nullptr);
        allocator.Deallocate(ptr);
    }
}

TEST_CASE("TPoolAllocator Owns", "[GP][Core][Memory][PoolAllocator][Ownership]")
{
    using namespace GP::Memory;

    TPoolAllocator<64, 10> allocator;

    SECTION("Owns Allocated Pointers")
    {
        void* ptr1 = allocator.Allocate();
        void* ptr2 = allocator.Allocate();
        void* ptr3 = allocator.Allocate();

        REQUIRE(allocator.Owns(ptr1));
        REQUIRE(allocator.Owns(ptr2));
        REQUIRE(allocator.Owns(ptr3));

        allocator.Deallocate(ptr1);
        allocator.Deallocate(ptr2);
        allocator.Deallocate(ptr3);
    }

    SECTION("Owns Deallocated Pointer")
    {
        void* ptr = allocator.Allocate();
        allocator.Deallocate(ptr);

        // Still owns the memory (it's in the pool)
        REQUIRE(allocator.Owns(ptr));
    }

    SECTION("Does Not Own External Pointer")
    {
        int external = 42;
        REQUIRE_FALSE(allocator.Owns(&external));
    }

    SECTION("Does Not Own Nullptr") { REQUIRE_FALSE(allocator.Owns(nullptr)); }

    SECTION("Does Not Own Misaligned Pointer Within Range")
    {
        void* ptr = allocator.Allocate();
        char* misaligned = static_cast<char*>(ptr) + 1;

        // Owns checks alignment, so this should fail
        REQUIRE_FALSE(allocator.Owns(misaligned));

        allocator.Deallocate(ptr);
    }
}

TEST_CASE("TPoolAllocator With Objects", "[GP][Core][Memory][PoolAllocator][Objects]")
{
    using namespace GP::Memory;

    SECTION("Allocate and Construct Objects")
    {
        TPoolAllocator<sizeof(std::string), 5> allocator;

        std::string* str = static_cast<std::string*>(allocator.Allocate());
        REQUIRE(str != nullptr);

        Construct<std::string>(str, "Hello Pool");
        REQUIRE(*str == "Hello Pool");

        Destroy(str);
        allocator.Deallocate(str);
    }

    SECTION("Multiple String Objects")
    {
        TPoolAllocator<sizeof(std::string), 5> allocator;
        std::string* strings[5];

        for (int i = 0; i < 5; ++i)
        {
            strings[i] = static_cast<std::string*>(allocator.Allocate());
            Construct<std::string>(strings[i], "String " + std::to_string(i));
        }

        for (int i = 0; i < 5; ++i) { REQUIRE(*strings[i] == "String " + std::to_string(i)); }

        for (int i = 0; i < 5; ++i)
        {
            Destroy(strings[i]);
            allocator.Deallocate(strings[i]);
        }

        REQUIRE(allocator.IsEmpty());
    }

    SECTION("Complex Type")
    {
        struct Entity
        {
            int id;
            std::string name;
            float health;

            Entity(int i, std::string n, float h)
                : id(i)
                , name(std::move(n))
                , health(h)
            {}
        };

        TPoolAllocator<sizeof(Entity), 10> allocator;

        Entity* entity = static_cast<Entity*>(allocator.Allocate());
        Construct<Entity>(entity, 1, "Player", 100.0f);

        REQUIRE(entity->id == 1);
        REQUIRE(entity->name == "Player");
        REQUIRE(entity->health == 100.0f);

        Destroy(entity);
        allocator.Deallocate(entity);
    }

    SECTION("Object Pooling Pattern")
    {
        struct Particle
        {
            float x, y, z;
            float vx, vy, vz;
            bool active;
        };

        TPoolAllocator<sizeof(Particle), 100> particlePool;

        // Allocate some particles
        Particle* particles[10];
        for (int i = 0; i < 10; ++i)
        {
            particles[i] = static_cast<Particle*>(particlePool.Allocate());
            particles[i]->active = true;
            particles[i]->x = static_cast<float>(i);
        }

        // "Kill" some particles (free back to pool)
        for (int i = 0; i < 5; ++i)
        {
            particles[i]->active = false;
            particlePool.Deallocate(particles[i]);
        }

        REQUIRE(particlePool.GetAllocatedCount() == 5);

        // Spawn new particles (reuse freed blocks)
        for (int i = 0; i < 5; ++i)
        {
            Particle* newParticle = static_cast<Particle*>(particlePool.Allocate());
            newParticle->active = true;
            newParticle->x = 100.0f + i;
        }

        REQUIRE(particlePool.GetAllocatedCount() == 10);

        // Clean up
        for (int i = 5; i < 10; ++i) { particlePool.Deallocate(particles[i]); }

        particlePool.Reset();
    }
}

TEST_CASE("TPoolAllocator Alignment", "[GP][Core][Memory][PoolAllocator][Alignment]")
{
    using namespace GP::Memory;

    SECTION("Default Alignment")
    {
        TPoolAllocator<64, 10> allocator;
        void* ptr = allocator.Allocate();
        REQUIRE(IsAligned(ptr, alignof(std::max_align_t)));
        allocator.Deallocate(ptr);
    }

    SECTION("16-byte Alignment")
    {
        TPoolAllocator<64, 10, 16> allocator;
        void* ptr = allocator.Allocate();
        REQUIRE(IsAligned(ptr, 16));
        allocator.Deallocate(ptr);
    }

    SECTION("32-byte Alignment")
    {
        TPoolAllocator<128, 10, 32> allocator;
        void* ptr = allocator.Allocate();
        REQUIRE(IsAligned(ptr, 32));
        allocator.Deallocate(ptr);
    }

    SECTION("64-byte Alignment (Cache Line)")
    {
        TPoolAllocator<256, 10, 64> allocator;
        void* ptr = allocator.Allocate();
        REQUIRE(IsAligned(ptr, 64));
        allocator.Deallocate(ptr);
    }

    SECTION("All Blocks Properly Aligned")
    {
        TPoolAllocator<64, 10, 32> allocator;
        void* pointers[10];

        for (int i = 0; i < 10; ++i)
        {
            pointers[i] = allocator.Allocate();
            REQUIRE(IsAligned(pointers[i], 32));
        }

        for (int i = 0; i < 10; ++i) { allocator.Deallocate(pointers[i]); }
    }
}

TEST_CASE("TPoolAllocator Stress Test", "[GP][Core][Memory][PoolAllocator][Stress]")
{
    using namespace GP::Memory;

    TPoolAllocator<128, 100> allocator;

    SECTION("Rapid Allocation and Deallocation")
    {
        for (int cycle = 0; cycle < 1000; ++cycle)
        {
            void* ptr = allocator.Allocate();
            REQUIRE(ptr != nullptr);
            allocator.Deallocate(ptr);
        }

        REQUIRE(allocator.IsEmpty());
    }

    SECTION("Random Allocation Pattern")
    {
        void* pointers[100];
        int allocCount = 0;

        // Allocate 50 blocks
        for (int i = 0; i < 50; ++i) { pointers[allocCount++] = allocator.Allocate(); }

        // Deallocate 25 blocks
        for (int i = 0; i < 25; ++i) { allocator.Deallocate(pointers[--allocCount]); }

        REQUIRE(allocator.GetAllocatedCount() == 25);

        // Allocate 50 more blocks
        for (int i = 0; i < 50; ++i) { pointers[allocCount++] = allocator.Allocate(); }

        REQUIRE(allocator.GetAllocatedCount() == 75);

        // Deallocate all
        for (int i = 0; i < 75; ++i) { allocator.Deallocate(pointers[--allocCount]); }

        REQUIRE(allocator.IsEmpty());
    }

    SECTION("Fill and Empty Multiple Times")
    {
        for (int cycle = 0; cycle < 100; ++cycle)
        {
            void* pointers[100];

            for (int i = 0; i < 100; ++i) { pointers[i] = allocator.Allocate(); }

            REQUIRE(allocator.IsFull());

            for (int i = 0; i < 100; ++i) { allocator.Deallocate(pointers[i]); }

            REQUIRE(allocator.IsEmpty());
        }
    }
}

TEST_CASE("TPoolAllocator Edge Cases", "[GP][Core][Memory][PoolAllocator][Edge]")
{
    using namespace GP::Memory;

    SECTION("Minimum Block Size")
    {
        TPoolAllocator<sizeof(void*), 10> allocator;
        void* ptr = allocator.Allocate();
        REQUIRE(ptr != nullptr);
        allocator.Deallocate(ptr);
    }

    SECTION("Single Block Pool")
    {
        TPoolAllocator<64, 1> allocator;

        REQUIRE(allocator.GetBlockCount() == 1);
        REQUIRE(allocator.IsEmpty());

        void* ptr = allocator.Allocate();
        REQUIRE(ptr != nullptr);
        REQUIRE(allocator.IsFull());

        void* ptr2 = allocator.Allocate();
        REQUIRE(ptr2 == nullptr);

        allocator.Deallocate(ptr);
        REQUIRE(allocator.IsEmpty());
    }

    SECTION("Large Block Size")
    {
        TPoolAllocator<4096, 10> allocator;
        void* ptr = allocator.Allocate();
        REQUIRE(ptr != nullptr);

        // Write pattern to verify memory is valid
        char* bytes = static_cast<char*>(ptr);
        for (int i = 0; i < 4096; ++i) { bytes[i] = static_cast<char>(i & 0xFF); }

        // Verify pattern
        for (int i = 0; i < 4096; ++i) { REQUIRE(bytes[i] == static_cast<char>(i & 0xFF)); }

        allocator.Deallocate(ptr);
    }

    SECTION("Deallocate All Blocks in Various Orders")
    {
        TPoolAllocator<64, 5> allocator;
        void* pointers[5];

        for (int i = 0; i < 5; ++i) { pointers[i] = allocator.Allocate(); }

        // Deallocate in middle-out order
        allocator.Deallocate(pointers[2]);
        allocator.Deallocate(pointers[1]);
        allocator.Deallocate(pointers[3]);
        allocator.Deallocate(pointers[0]);
        allocator.Deallocate(pointers[4]);

        REQUIRE(allocator.IsEmpty());
    }
}

TEST_CASE("TPoolAllocator State Consistency", "[GP][Core][Memory][PoolAllocator][Consistency]")
{
    using namespace GP::Memory;

    TPoolAllocator<64, 20> allocator;

    SECTION("Allocated + Free = Total")
    {
        for (int i = 0; i < 15; ++i)
        {
            (void)allocator.Allocate();
            REQUIRE(allocator.GetAllocatedCount() + allocator.GetFreeCount() == 20);
        }
    }

    SECTION("IsEmpty and IsFull Consistency")
    {
        REQUIRE(allocator.IsEmpty());
        REQUIRE_FALSE(allocator.IsFull());
        REQUIRE(allocator.GetAllocatedCount() == 0);

        void* pointers[20];
        for (int i = 0; i < 20; ++i) { pointers[i] = allocator.Allocate(); }

        REQUIRE_FALSE(allocator.IsEmpty());
        REQUIRE(allocator.IsFull());
        REQUIRE(allocator.GetAllocatedCount() == 20);

        for (int i = 0; i < 20; ++i) { allocator.Deallocate(pointers[i]); }

        REQUIRE(allocator.IsEmpty());
        REQUIRE_FALSE(allocator.IsFull());
    }
}
