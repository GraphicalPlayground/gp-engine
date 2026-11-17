// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "templates/TypeCompatibleBytes.hpp"
#include "CoreMinimal.hpp"
#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::tests
{

TEST(TypeCompatibleBytesTest, LayoutAlignmentAndTrivialStorageProperties)
{
    struct alignas(64) CacheAlignedEvent
    {
        UInt64 timestamp = 0;
        UInt32 kind = 0;
    };

    struct NonTrivialPayload
    {
        Int32 value;

        NonTrivialPayload()
            : value(1)
        {}

        ~NonTrivialPayload()
        {}
    };

    static_assert(sizeof(TypeCompatibleBytes<UInt64>) == sizeof(UInt64));
    static_assert(alignof(TypeCompatibleBytes<UInt64>) == alignof(UInt64));

    static_assert(sizeof(TypeCompatibleBytes<CacheAlignedEvent>) == sizeof(CacheAlignedEvent));
    static_assert(alignof(TypeCompatibleBytes<CacheAlignedEvent>) == 64);

    // Backing cell stays trivial even when the stored type is non-trivial.
    static_assert(std::is_trivially_default_constructible_v<TypeCompatibleBytes<NonTrivialPayload>>);
    static_assert(std::is_trivially_destructible_v<TypeCompatibleBytes<NonTrivialPayload>>);
    static_assert(std::is_trivially_copyable_v<TypeCompatibleBytes<NonTrivialPayload>>);

    TypeCompatibleBytes<CacheAlignedEvent> alignedCell;
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(alignedCell.data()) % 64u, 0u);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(alignedCell.getTypedPtr()) % 64u, 0u);
}

TEST(TypeCompatibleBytesTest, DefersConstructionSupportsLaunderedReuseAndExplicitDestruction)
{
    static Int32 s_ctorCount = 0;
    static Int32 s_dtorCount = 0;
    s_ctorCount = 0;
    s_dtorCount = 0;

    struct TrackedObject
    {
        const Int32 id;
        Int32 payload;

        TrackedObject(const Int32 inId, const Int32 inPayload) noexcept
            : id(inId)
            , payload(inPayload)
        {
            ++s_ctorCount;
        }

        ~TrackedObject() noexcept
        {
            ++s_dtorCount;
        }
    };

    TypeCompatibleBytes<TrackedObject> slots[4];
    EXPECT_EQ(s_ctorCount, 0);
    EXPECT_EQ(s_dtorCount, 0);

    // Construct only slot 2
    TrackedObject* live = slots[2].construct(10, 42);
    EXPECT_EQ(s_ctorCount, 1);
    EXPECT_EQ(live->id, 10);
    EXPECT_EQ(slots[2].asRef().payload, 42);

    slots[2].asRef().payload = 99;
    const TypeCompatibleBytes<TrackedObject>& constSlot = slots[2];
    EXPECT_EQ(constSlot.getTypedPtr()->payload, 99);
    EXPECT_EQ(constSlot.asRef().id, 10);

    slots[2].destruct();
    EXPECT_EQ(s_dtorCount, 1);

    // Re-construct with a new const member value in the same raw buffer (validates std::launder access).
    slots[2].construct(77, 123);
    EXPECT_EQ(s_ctorCount, 2);
    EXPECT_EQ(constSlot.getTypedPtr()->id, 77);
    EXPECT_EQ(constSlot.getTypedPtr()->payload, 123);

    slots[2].destruct();
    EXPECT_EQ(s_dtorCount, 2);
}

}   // namespace gp::tests
