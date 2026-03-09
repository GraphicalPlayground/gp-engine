// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Core/Any.hpp"
#include "CoreTypes.hpp"
#include <catch2/catch_test_macros.hpp>
#include <type_traits>

using namespace GP;

namespace
{

struct FDestroyTracker
{
    Int32* m_count;

    explicit FDestroyTracker(Int32& count)
        : m_count(&count)
    {}

    FDestroyTracker(const FDestroyTracker&) = default;
    FDestroyTracker(FDestroyTracker&&) noexcept = default;
    FDestroyTracker& operator=(const FDestroyTracker&) = default;
    FDestroyTracker& operator=(FDestroyTracker&&) noexcept = default;

    ~FDestroyTracker() { ++(*m_count); }
};

struct FMoveSpy
{
    Int32 value{ 0 };
    bool moved{ false };

    FMoveSpy() = default;

    explicit FMoveSpy(Int32 v)
        : value(v)
    {}

    FMoveSpy(const FMoveSpy&) = default;

    FMoveSpy(FMoveSpy&& other) noexcept
        : value(other.value)
        , moved(false)
    {
        other.moved = true;
    }

    FMoveSpy& operator=(const FMoveSpy&) = default;

    FMoveSpy& operator=(FMoveSpy&& other) noexcept
    {
        value = other.value;
        other.moved = true;
        return *this;
    }

    bool operator==(const FMoveSpy& other) const noexcept { return value == other.value; }
};

/// @brief A large struct that exceeds the SBO buffer and forces heap allocation.
struct FLargeStruct
{
    // 128 bytes — well above the 24-byte SBO limit.
    Byte data[128]{};
    Int32 value{ 0 };

    FLargeStruct() = default;

    explicit FLargeStruct(Int32 v)
        : value(v)
    {
        data[0] = static_cast<Byte>(v);
    }

    FLargeStruct(const FLargeStruct&) = default;
    FLargeStruct(FLargeStruct&&) noexcept = default;
    FLargeStruct& operator=(const FLargeStruct&) = default;
    FLargeStruct& operator=(FLargeStruct&&) noexcept = default;

    bool operator==(const FLargeStruct& other) const noexcept { return value == other.value; }
};

struct FMultiArg
{
    Int32 x{ 0 };
    Int32 y{ 0 };
    Float32 z{ 0.0f };

    FMultiArg(Int32 inX, Int32 inY, Float32 inZ)
        : x(inX)
        , y(inY)
        , z(inZ)
    {}

    FMultiArg(const FMultiArg&) = default;

    bool operator==(const FMultiArg& other) const noexcept { return x == other.x && y == other.y && z == other.z; }
};

}   // namespace

TEST_CASE("TAny: Default construction yields empty TAny", "[Container][Any]")
{
    TAny a;
    REQUIRE_FALSE(a.HasValue());
    REQUIRE_FALSE(static_cast<bool>(a));
}

TEST_CASE("TAny: Construction from Int32 stores the value", "[Container][Any]")
{
    TAny a(Int32{ 42 });
    REQUIRE(a.HasValue());
    REQUIRE(a.Has<Int32>());
    REQUIRE(a.Cast<Int32>() == 42);
}

TEST_CASE("TAny: Construction from Float32 stores the value", "[Container][Any]")
{
    TAny a(Float32{ 3.14f });
    REQUIRE(a.HasValue());
    REQUIRE(a.Has<Float32>());
    REQUIRE(a.Cast<Float32>() == 3.14f);
}

TEST_CASE("TAny: Has<T> returns false for wrong type", "[Container][Any]")
{
    TAny a(Int32{ 1 });
    REQUIRE_FALSE(a.Has<Float32>());
    REQUIRE_FALSE(a.Has<double>());
}

TEST_CASE("TAny: Construction stores SBO types inline (SBO path)", "[Container][Any]")
{
    // Int32 is small enough for SBO.
    STATIC_REQUIRE(sizeof(Int32) <= TAny::kSBOSize);
    STATIC_REQUIRE(alignof(Int32) <= TAny::kSBOAlign);

    TAny a(Int32{ 99 });
    REQUIRE(a.Has<Int32>());
    REQUIRE(a.Cast<Int32>() == 99);
}

TEST_CASE("TAny: Construction stores large types on heap (heap path)", "[Container][Any]")
{
    // FLargeStruct is larger than the SBO buffer.
    STATIC_REQUIRE(sizeof(FLargeStruct) > TAny::kSBOSize);

    TAny a(FLargeStruct{ 77 });
    REQUIRE(a.Has<FLargeStruct>());
    REQUIRE(a.Cast<FLargeStruct>().value == 77);
}

TEST_CASE("TAny: In-place constructor (TInPlaceTypeT) with multiple arguments", "[Container][Any]")
{
    TAny a(InPlaceType<FMultiArg>, Int32{ 1 }, Int32{ 2 }, Float32{ 3.0f });
    REQUIRE(a.HasValue());
    REQUIRE(a.Has<FMultiArg>());
    const auto& v = a.Cast<FMultiArg>();
    REQUIRE(v.x == 1);
    REQUIRE(v.y == 2);
    REQUIRE(v.z == 3.0f);
}

TEST_CASE("TAny: MakeAny factory constructs in-place", "[Container][Any]")
{
    TAny a = MakeAny<FMultiArg>(Int32{ 10 }, Int32{ 20 }, Float32{ 30.0f });
    REQUIRE(a.Has<FMultiArg>());
    REQUIRE(a.Cast<FMultiArg>().x == 10);
}

TEST_CASE("TAny: Copy construction from empty produces empty", "[Container][Any]")
{
    TAny src;
    TAny dst(src);
    REQUIRE_FALSE(dst.HasValue());
}

TEST_CASE("TAny: Copy construction from value-holding TAny copies the value (SBO)", "[Container][Any]")
{
    TAny src(Int32{ 7 });
    TAny dst(src);
    REQUIRE(dst.Has<Int32>());
    REQUIRE(dst.Cast<Int32>() == 7);

    // Modifying dst must not affect src.
    dst.Cast<Int32>() = 99;
    REQUIRE(src.Cast<Int32>() == 7);
}

TEST_CASE("TAny: Copy construction from value-holding TAny copies the value (heap)", "[Container][Any]")
{
    TAny src(FLargeStruct{ 12 });
    TAny dst(src);
    REQUIRE(dst.Has<FLargeStruct>());
    REQUIRE(dst.Cast<FLargeStruct>().value == 12);
}

TEST_CASE("TAny: Move construction transfers value and empties source (SBO)", "[Container][Any]")
{
    TAny src(Int32{ 55 });
    TAny dst(GP::Move(src));
    REQUIRE(dst.Has<Int32>());
    REQUIRE(dst.Cast<Int32>() == 55);
    REQUIRE_FALSE(src.HasValue());
}

TEST_CASE("TAny: Move construction transfers value and empties source (heap)", "[Container][Any]")
{
    TAny src(FLargeStruct{ 88 });
    TAny dst(GP::Move(src));
    REQUIRE(dst.Has<FLargeStruct>());
    REQUIRE(dst.Cast<FLargeStruct>().value == 88);
    REQUIRE_FALSE(src.HasValue());
}

TEST_CASE("TAny: Move construction uses move semantics for the stored type", "[Container][Any]")
{
    FMoveSpy spy(42);
    TAny src(spy);
    REQUIRE(src.Has<FMoveSpy>());

    TAny dst(GP::Move(src));
    REQUIRE(dst.Has<FMoveSpy>());
    REQUIRE(dst.Cast<FMoveSpy>().value == 42);
    REQUIRE_FALSE(src.HasValue());
}

TEST_CASE("TAny: Destructor invokes the stored type's destructor (SBO)", "[Container][Any]")
{
    Int32 count = 0;
    {
        TAny a(FDestroyTracker{ count });
        REQUIRE(count == 1);     // Temporary destroyed immediately after construction
        REQUIRE(a.HasValue());   // a holds a copy

        count = 0;
    }
    REQUIRE(count == 1);   // Destroyed when a goes out of scope
}

TEST_CASE("TAny: Destructor invokes the stored type's destructor (heap)", "[Container][Any]")
{
    Int32 count = 0;
    {
        // FLargeStruct itself does not track, but wrap DestroyTracker in a larger struct.
        // Instead, directly confirm with a large-enough tracked type.
        struct FLargeTracker
        {
            Byte pad[32]{};
            Int32* m_count;

            explicit FLargeTracker(Int32& c)
                : m_count(&c)
            {}

            FLargeTracker(const FLargeTracker&) = default;

            ~FLargeTracker() { ++(*m_count); }
        };

        STATIC_REQUIRE(sizeof(FLargeTracker) > TAny::kSBOSize);

        TAny a(FLargeTracker{ count });
        REQUIRE(count == 1);   // Temporary destroyed after construction
        count = 0;
    }
    REQUIRE(count == 1);
}

TEST_CASE("TAny: Copy assignment from empty empties the target", "[Container][Any]")
{
    TAny src;
    TAny dst(Int32{ 1 });
    dst = src;
    REQUIRE_FALSE(dst.HasValue());
}

TEST_CASE("TAny: Copy assignment replaces the stored value", "[Container][Any]")
{
    TAny src(Int32{ 10 });
    TAny dst(Float32{ 2.0f });
    dst = src;
    REQUIRE(dst.Has<Int32>());
    REQUIRE(dst.Cast<Int32>() == 10);
}

TEST_CASE("TAny: Copy assignment to self is a no-op", "[Container][Any]")
{
    TAny a(Int32{ 42 });
    // Suppress self-assignment warning; intentional for test.
    TAny& ref = a;
    a = ref;
    REQUIRE(a.Has<Int32>());
    REQUIRE(a.Cast<Int32>() == 42);
}

TEST_CASE("TAny: Move assignment transfers value and empties source", "[Container][Any]")
{
    TAny src(Int32{ 7 });
    TAny dst;
    dst = GP::Move(src);
    REQUIRE(dst.Has<Int32>());
    REQUIRE(dst.Cast<Int32>() == 7);
    REQUIRE_FALSE(src.HasValue());
}

TEST_CASE("TAny: Move assignment from empty empties the target", "[Container][Any]")
{
    TAny src;
    TAny dst(Int32{ 5 });
    dst = GP::Move(src);
    REQUIRE_FALSE(dst.HasValue());
}

TEST_CASE("TAny: Move assignment to self is a no-op", "[Container][Any]")
{
    TAny a(Int32{ 99 });
    TAny& ref = a;
    a = GP::Move(ref);
    REQUIRE(a.Has<Int32>());
    REQUIRE(a.Cast<Int32>() == 99);
}

TEST_CASE("TAny: operator=(T&&) replaces the stored value with a new type", "[Container][Any]")
{
    TAny a(Int32{ 1 });
    a = Float32{ 2.5f };
    REQUIRE(a.Has<Float32>());
    REQUIRE(a.Cast<Float32>() == 2.5f);
}

TEST_CASE("TAny: operator=(T&&) replaces empty TAny with a value", "[Container][Any]")
{
    TAny a;
    a = Int32{ 100 };
    REQUIRE(a.Has<Int32>());
    REQUIRE(a.Cast<Int32>() == 100);
}

TEST_CASE("TAny: Reset clears the stored value", "[Container][Any]")
{
    TAny a(Int32{ 5 });
    a.Reset();
    REQUIRE_FALSE(a.HasValue());
}

TEST_CASE("TAny: Reset on empty TAny is a no-op", "[Container][Any]")
{
    TAny a;
    a.Reset();
    REQUIRE_FALSE(a.HasValue());
}

TEST_CASE("TAny: Reset calls the destructor of the stored value", "[Container][Any]")
{
    Int32 count = 0;
    TAny a(FDestroyTracker{ count });
    count = 0;   // Clear the temporary destruction count
    REQUIRE(a.HasValue());

    a.Reset();
    REQUIRE(count == 1);
    REQUIRE_FALSE(a.HasValue());
}

TEST_CASE("TAny: Emplace<T> constructs T in-place, replacing any existing value", "[Container][Any]")
{
    TAny a(Int32{ 1 });
    auto& ref = a.Emplace<FMultiArg>(Int32{ 4 }, Int32{ 5 }, Float32{ 6.0f });
    REQUIRE(a.Has<FMultiArg>());
    REQUIRE(ref.x == 4);
    REQUIRE(ref.y == 5);
    REQUIRE(ref.z == 6.0f);
}

TEST_CASE("TAny: Emplace<T> on empty TAny constructs the value", "[Container][Any]")
{
    TAny a;
    a.Emplace<Int32>(42);
    REQUIRE(a.Has<Int32>());
    REQUIRE(a.Cast<Int32>() == 42);
}

TEST_CASE("TAny: Emplace<T> destroys the previous value before constructing the new one", "[Container][Any]")
{
    Int32 count = 0;
    TAny a(FDestroyTracker{ count });
    count = 0;

    a.Emplace<Int32>(10);
    REQUIRE(count == 1);   // FDestroyTracker was destroyed
    REQUIRE(a.Has<Int32>());
}

TEST_CASE("TAny: TryCast<T> returns pointer on type match", "[Container][Any]")
{
    TAny a(Int32{ 33 });
    Int32* p = a.TryCast<Int32>();
    REQUIRE(p != nullptr);
    REQUIRE(*p == 33);
}

TEST_CASE("TAny: TryCast<T> returns nullptr on type mismatch", "[Container][Any]")
{
    TAny a(Int32{ 33 });
    Float32* p = a.TryCast<Float32>();
    REQUIRE(p == nullptr);
}

TEST_CASE("TAny: TryCast<T> returns nullptr on empty TAny", "[Container][Any]")
{
    TAny a;
    Int32* p = a.TryCast<Int32>();
    REQUIRE(p == nullptr);
}

TEST_CASE("TAny: const TryCast<T> returns const pointer on type match", "[Container][Any]")
{
    const TAny a(Int32{ 7 });
    const Int32* p = a.TryCast<Int32>();
    REQUIRE(p != nullptr);
    REQUIRE(*p == 7);
}

TEST_CASE("TAny: AnyCast(TAny*) returns pointer on type match", "[Container][Any]")
{
    TAny a(Int32{ 5 });
    Int32* p = AnyCast<Int32>(&a);
    REQUIRE(p != nullptr);
    REQUIRE(*p == 5);
}

TEST_CASE("TAny: AnyCast(TAny*) returns nullptr on type mismatch", "[Container][Any]")
{
    TAny a(Int32{ 5 });
    Float32* p = AnyCast<Float32>(&a);
    REQUIRE(p == nullptr);
}

TEST_CASE("TAny: AnyCast(TAny*) returns nullptr when pointer is null", "[Container][Any]")
{
    Int32* p = AnyCast<Int32>(static_cast<TAny*>(nullptr));
    REQUIRE(p == nullptr);
}

TEST_CASE("TAny: AnyCast(const TAny*) returns const pointer on type match", "[Container][Any]")
{
    const TAny a(Int32{ 9 });
    const Int32* p = AnyCast<Int32>(&a);
    REQUIRE(p != nullptr);
    REQUIRE(*p == 9);
}

TEST_CASE("TAny: AnyCast(TAny&) returns mutable reference on type match", "[Container][Any]")
{
    TAny a(Int32{ 3 });
    Int32& ref = AnyCast<Int32>(a);
    ref = 99;
    REQUIRE(a.Cast<Int32>() == 99);
}

TEST_CASE("TAny: AnyCast(const TAny&) returns const reference on type match", "[Container][Any]")
{
    const TAny a(Int32{ 8 });
    const Int32& ref = AnyCast<Int32>(a);
    REQUIRE(ref == 8);
}

TEST_CASE("TAny: AnyCast(TAny&&) moves the value out", "[Container][Any]")
{
    FMoveSpy spy(13);
    TAny a(spy);

    FMoveSpy result = AnyCast<FMoveSpy>(GP::Move(a));
    REQUIRE(result.value == 13);
}

TEST_CASE("TAny: Swap exchanges stored values (SBO)", "[Container][Any]")
{
    TAny a(Int32{ 1 });
    TAny b(Int32{ 2 });
    a.Swap(b);
    REQUIRE(a.Cast<Int32>() == 2);
    REQUIRE(b.Cast<Int32>() == 1);
}

TEST_CASE("TAny: Swap between empty and value-holding TAny", "[Container][Any]")
{
    TAny a;
    TAny b(Int32{ 5 });
    a.Swap(b);
    REQUIRE(a.Has<Int32>());
    REQUIRE(a.Cast<Int32>() == 5);
    REQUIRE_FALSE(b.HasValue());
}

TEST_CASE("TAny: Swap of two empty TAny instances remains empty", "[Container][Any]")
{
    TAny a;
    TAny b;
    a.Swap(b);
    REQUIRE_FALSE(a.HasValue());
    REQUIRE_FALSE(b.HasValue());
}

TEST_CASE("TAny: Swap exchanges stored values (heap)", "[Container][Any]")
{
    TAny a(FLargeStruct{ 11 });
    TAny b(FLargeStruct{ 22 });
    a.Swap(b);
    REQUIRE(a.Cast<FLargeStruct>().value == 22);
    REQUIRE(b.Cast<FLargeStruct>().value == 11);
}

TEST_CASE("TAny: operator bool returns false for empty", "[Container][Any]")
{
    TAny a;
    REQUIRE_FALSE(static_cast<bool>(a));
}

TEST_CASE("TAny: operator bool returns true when a value is stored", "[Container][Any]")
{
    TAny a(Int32{ 1 });
    REQUIRE(static_cast<bool>(a));
}

TEST_CASE("TAny: Stored type identity is preserved through copy", "[Container][Any]")
{
    TAny src(Int32{ 6 });
    TAny dst(src);
    REQUIRE(dst.Has<Int32>());
    REQUIRE_FALSE(dst.Has<Float32>());
}

TEST_CASE("TAny: Stored type identity is preserved through move", "[Container][Any]")
{
    TAny src(Float32{ 1.0f });
    TAny dst(GP::Move(src));
    REQUIRE(dst.Has<Float32>());
}
