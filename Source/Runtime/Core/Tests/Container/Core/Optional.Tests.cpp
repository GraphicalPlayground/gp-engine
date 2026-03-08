// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Core/Optional.hpp"
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

    bool operator!=(const FMoveSpy& other) const noexcept { return value != other.value; }
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

    bool operator==(const FMultiArg& other) const noexcept { return x == other.x && y == other.y && z == other.z; }
};

}   // namespace

TEST_CASE("TOptional: Default construction yields empty optional", "[Container][Optional]")
{
    const TOptional<Int32> opt;
    REQUIRE_FALSE(opt.HasValue());
    REQUIRE_FALSE(static_cast<bool>(opt));
}

TEST_CASE("TOptional: Default construction is noexcept", "[Container][Optional]")
{
    STATIC_REQUIRE(noexcept(TOptional<Int32>{}));
}

TEST_CASE("TOptional: NullOpt construction yields empty optional", "[Container][Optional]")
{
    const TOptional<Int32> opt(NullOpt);
    REQUIRE_FALSE(opt.HasValue());
    REQUIRE_FALSE(static_cast<bool>(opt));
}

TEST_CASE("TOptional: NullOpt construction is noexcept", "[Container][Optional]")
{
    STATIC_REQUIRE(noexcept(TOptional<Int32>{ NullOpt }));
}

TEST_CASE("TOptional: Copy-construction from value stores the value", "[Container][Optional]")
{
    const Int32 val = 42;
    const TOptional<Int32> opt(val);
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value() == 42);
}

TEST_CASE("TOptional: Move-construction from value stores the value and moves the source", "[Container][Optional]")
{
    FMoveSpy src(99);
    TOptional<FMoveSpy> opt(GP::Move(src));
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value().value == 99);
    REQUIRE(src.moved);
}

TEST_CASE("TOptional: In-place construction from multiple arguments", "[Container][Optional]")
{
    TOptional<FMultiArg> opt(10, 20, 3.14f);
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value().x == 10);
    REQUIRE(opt.Value().y == 20);
    REQUIRE(opt.Value().z == 3.14f);
}

TEST_CASE("TOptional: Copy-construction from non-empty optional copies value", "[Container][Optional]")
{
    const TOptional<Int32> src(7);
    const TOptional<Int32> opt(src);
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value() == 7);
    REQUIRE(src.HasValue());
    REQUIRE(src.Value() == 7);
}

TEST_CASE("TOptional: Copy-construction from empty optional yields empty optional", "[Container][Optional]")
{
    const TOptional<Int32> src;
    const TOptional<Int32> opt(src);
    REQUIRE_FALSE(opt.HasValue());
}

TEST_CASE(
    "TOptional: Move-construction from non-empty optional transfers value and empties source", "[Container][Optional]"
)
{
    TOptional<FMoveSpy> src(FMoveSpy(55));
    TOptional<FMoveSpy> opt(GP::Move(src));
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value().value == 55);
    REQUIRE_FALSE(src.HasValue());
}

TEST_CASE("TOptional: Move-construction from empty optional yields empty optional", "[Container][Optional]")
{
    TOptional<Int32> src;
    const TOptional<Int32> opt(GP::Move(src));
    REQUIRE_FALSE(opt.HasValue());
}

TEST_CASE("TOptional: Destructor destroys the contained value exactly once", "[Container][Optional]")
{
    Int32 destroyCount = 0;
    {
        TOptional<FDestroyTracker> opt{ FDestroyTracker(destroyCount) };
        destroyCount = 0;
    }
    REQUIRE(destroyCount == 1);
}

TEST_CASE("TOptional: Destructor of empty optional does not invoke any destructor", "[Container][Optional]")
{
    Int32 destroyCount = 0;
    {
        TOptional<FDestroyTracker> opt;
    }
    REQUIRE(destroyCount == 0);
}

TEST_CASE("TOptional: NullOpt assignment to non-empty resets the optional", "[Container][Optional]")
{
    TOptional<Int32> opt(42);
    opt = NullOpt;
    REQUIRE_FALSE(opt.HasValue());
}

TEST_CASE("TOptional: NullOpt assignment to empty remains empty", "[Container][Optional]")
{
    TOptional<Int32> opt;
    opt = NullOpt;
    REQUIRE_FALSE(opt.HasValue());
}

TEST_CASE("TOptional: NullOpt assignment destroys the contained value", "[Container][Optional]")
{
    Int32 destroyCount = 0;
    TOptional<FDestroyTracker> opt{ FDestroyTracker(destroyCount) };
    destroyCount = 0;
    opt = NullOpt;
    REQUIRE(destroyCount == 1);
}

TEST_CASE("TOptional: Copy-assignment from non-empty to non-empty updates the value", "[Container][Optional]")
{
    TOptional<Int32> dst(1);
    const TOptional<Int32> src(2);
    dst = src;
    REQUIRE(dst.HasValue());
    REQUIRE(dst.Value() == 2);
}

TEST_CASE("TOptional: Copy-assignment from non-empty to empty constructs the value", "[Container][Optional]")
{
    TOptional<Int32> dst;
    const TOptional<Int32> src(99);
    dst = src;
    REQUIRE(dst.HasValue());
    REQUIRE(dst.Value() == 99);
}

TEST_CASE("TOptional: Copy-assignment from empty to non-empty resets the destination", "[Container][Optional]")
{
    TOptional<Int32> dst(10);
    const TOptional<Int32> src;
    dst = src;
    REQUIRE_FALSE(dst.HasValue());
}

TEST_CASE("TOptional: Copy-assignment from empty to empty remains empty", "[Container][Optional]")
{
    TOptional<Int32> dst;
    const TOptional<Int32> src;
    dst = src;
    REQUIRE_FALSE(dst.HasValue());
}

TEST_CASE("TOptional: Copy self-assignment is safe for non-empty optional", "[Container][Optional]")
{
    TOptional<Int32> opt(7);
    opt = opt;
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value() == 7);
}

TEST_CASE(
    "TOptional: Move-assignment from non-empty to non-empty updates destination and clears source",
    "[Container][Optional]"
)
{
    TOptional<FMoveSpy> dst(FMoveSpy(1));
    TOptional<FMoveSpy> src(FMoveSpy(2));
    dst = GP::Move(src);
    REQUIRE(dst.HasValue());
    REQUIRE(dst.Value().value == 2);
    REQUIRE_FALSE(src.HasValue());
}

TEST_CASE(
    "TOptional: Move-assignment from non-empty to empty constructs destination and clears source",
    "[Container][Optional]"
)
{
    TOptional<FMoveSpy> dst;
    TOptional<FMoveSpy> src(FMoveSpy(42));
    dst = GP::Move(src);
    REQUIRE(dst.HasValue());
    REQUIRE(dst.Value().value == 42);
    REQUIRE_FALSE(src.HasValue());
}

TEST_CASE("TOptional: Move-assignment from empty to non-empty resets the destination", "[Container][Optional]")
{
    TOptional<FMoveSpy> dst(FMoveSpy(10));
    TOptional<FMoveSpy> src;
    dst = GP::Move(src);
    REQUIRE_FALSE(dst.HasValue());
}

TEST_CASE("TOptional: Move self-assignment is safe for non-empty optional", "[Container][Optional]")
{
    TOptional<Int32> opt(5);
    opt = GP::Move(opt);
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value() == 5);
}

TEST_CASE("TOptional: Value-assignment to empty constructs the value", "[Container][Optional]")
{
    TOptional<Int32> opt;
    opt = 100;
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value() == 100);
}

TEST_CASE("TOptional: Value-assignment to non-empty updates the value", "[Container][Optional]")
{
    TOptional<Int32> opt(5);
    opt = 50;
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value() == 50);
}

TEST_CASE("TOptional: operator== two non-empty optionals with equal values returns true", "[Container][Optional]")
{
    const TOptional<Int32> a(3);
    const TOptional<Int32> b(3);
    REQUIRE(a == b);
}

TEST_CASE("TOptional: operator== two non-empty optionals with different values returns false", "[Container][Optional]")
{
    const TOptional<Int32> a(3);
    const TOptional<Int32> b(4);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("TOptional: operator== two empty optionals returns true", "[Container][Optional]")
{
    const TOptional<Int32> a;
    const TOptional<Int32> b;
    REQUIRE(a == b);
}

TEST_CASE("TOptional: operator== empty and non-empty returns false", "[Container][Optional]")
{
    const TOptional<Int32> a;
    const TOptional<Int32> b(1);
    REQUIRE_FALSE(a == b);
    REQUIRE_FALSE(b == a);
}

TEST_CASE("TOptional: operator!= two non-empty optionals with different values returns true", "[Container][Optional]")
{
    const TOptional<Int32> a(1);
    const TOptional<Int32> b(2);
    REQUIRE(a != b);
}

TEST_CASE("TOptional: operator!= two non-empty optionals with equal values returns false", "[Container][Optional]")
{
    const TOptional<Int32> a(5);
    const TOptional<Int32> b(5);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("TOptional: operator!= empty and non-empty returns true", "[Container][Optional]")
{
    const TOptional<Int32> a;
    const TOptional<Int32> b(1);
    REQUIRE(a != b);
    REQUIRE(b != a);
}

TEST_CASE("TOptional: operator== with NullOpt returns true for empty optional", "[Container][Optional]")
{
    const TOptional<Int32> opt;
    REQUIRE(opt == NullOpt);
}

TEST_CASE("TOptional: operator== with NullOpt returns false for non-empty optional", "[Container][Optional]")
{
    const TOptional<Int32> opt(1);
    REQUIRE_FALSE(opt == NullOpt);
}

TEST_CASE("TOptional: operator!= with NullOpt returns false for empty optional", "[Container][Optional]")
{
    const TOptional<Int32> opt;
    REQUIRE_FALSE(opt != NullOpt);
}

TEST_CASE("TOptional: operator!= with NullOpt returns true for non-empty optional", "[Container][Optional]")
{
    const TOptional<Int32> opt(1);
    REQUIRE(opt != NullOpt);
}

TEST_CASE("TOptional: operator== with value returns true when value matches", "[Container][Optional]")
{
    const TOptional<Int32> opt(42);
    REQUIRE(opt == 42);
}

TEST_CASE("TOptional: operator== with value returns false for empty optional", "[Container][Optional]")
{
    const TOptional<Int32> opt;
    REQUIRE_FALSE(opt == 42);
}

TEST_CASE("TOptional: operator== with value returns false when value differs", "[Container][Optional]")
{
    const TOptional<Int32> opt(42);
    REQUIRE_FALSE(opt == 43);
}

TEST_CASE("TOptional: operator!= with value returns true for empty optional", "[Container][Optional]")
{
    const TOptional<Int32> opt;
    REQUIRE(opt != 42);
}

TEST_CASE("TOptional: operator!= with value returns true when values differ", "[Container][Optional]")
{
    const TOptional<Int32> opt(1);
    REQUIRE(opt != 2);
}

TEST_CASE("TOptional: operator!= with value returns false when value matches", "[Container][Optional]")
{
    const TOptional<Int32> opt(7);
    REQUIRE_FALSE(opt != 7);
}

TEST_CASE("TOptional: operator* on lvalue returns reference to contained value", "[Container][Optional]")
{
    TOptional<Int32> opt(10);
    REQUIRE(*opt == 10);
}

TEST_CASE("TOptional: operator* returns a mutable reference", "[Container][Optional]")
{
    TOptional<Int32> opt(10);
    *opt = 20;
    REQUIRE(opt.Value() == 20);
}

TEST_CASE("TOptional: operator* on const lvalue returns const reference", "[Container][Optional]")
{
    const TOptional<Int32> opt(15);
    REQUIRE(*opt == 15);
}

TEST_CASE("TOptional: operator* on rvalue moves out the value", "[Container][Optional]")
{
    TOptional<FMoveSpy> opt(FMoveSpy(77));
    FMoveSpy val = *GP::Move(opt);
    REQUIRE(val.value == 77);
}

TEST_CASE("TOptional: operator-> accesses members of the contained value", "[Container][Optional]")
{
    TOptional<FMoveSpy> opt(FMoveSpy(88));
    REQUIRE(opt->value == 88);
}

TEST_CASE("TOptional: operator-> const accesses members of the contained value", "[Container][Optional]")
{
    const TOptional<FMoveSpy> opt(FMoveSpy(88));
    REQUIRE(opt->value == 88);
}

TEST_CASE("TOptional: HasValue returns true when value is present", "[Container][Optional]")
{
    const TOptional<Int32> opt(1);
    REQUIRE(opt.HasValue());
}

TEST_CASE("TOptional: HasValue returns false when empty", "[Container][Optional]")
{
    const TOptional<Int32> opt;
    REQUIRE_FALSE(opt.HasValue());
}

TEST_CASE("TOptional: HasValue is noexcept", "[Container][Optional]")
{
    const TOptional<Int32> opt;
    STATIC_REQUIRE(noexcept(opt.HasValue()));
}

TEST_CASE("TOptional: operator bool returns true for non-empty optional", "[Container][Optional]")
{
    const TOptional<Int32> opt(1);
    REQUIRE(static_cast<bool>(opt));
}

TEST_CASE("TOptional: operator bool returns false for empty optional", "[Container][Optional]")
{
    const TOptional<Int32> opt;
    REQUIRE_FALSE(static_cast<bool>(opt));
}

TEST_CASE("TOptional: operator bool is noexcept", "[Container][Optional]")
{
    const TOptional<Int32> opt;
    STATIC_REQUIRE(noexcept(static_cast<bool>(opt)));
}

TEST_CASE("TOptional: Value lvalue overload returns reference to stored value", "[Container][Optional]")
{
    TOptional<Int32> opt(55);
    REQUIRE(opt.Value() == 55);
    opt.Value() = 66;
    REQUIRE(opt.Value() == 66);
}

TEST_CASE("TOptional: Value const lvalue overload returns const reference", "[Container][Optional]")
{
    const TOptional<Int32> opt(55);
    REQUIRE(opt.Value() == 55);
}

TEST_CASE("TOptional: Value rvalue overload moves out the value", "[Container][Optional]")
{
    TOptional<FMoveSpy> opt(FMoveSpy(33));
    FMoveSpy val = GP::Move(opt).Value();
    REQUIRE(val.value == 33);
}

TEST_CASE("TOptional: ValueOr returns contained value when present", "[Container][Optional]")
{
    const TOptional<Int32> opt(42);
    REQUIRE(opt.ValueOr(0) == 42);
}

TEST_CASE("TOptional: ValueOr returns default when empty", "[Container][Optional]")
{
    const TOptional<Int32> opt;
    REQUIRE(opt.ValueOr(99) == 99);
}

TEST_CASE("TOptional: ValueOr rvalue overload returns contained value when present", "[Container][Optional]")
{
    TOptional<Int32> opt(42);
    REQUIRE(GP::Move(opt).ValueOr(0) == 42);
}

TEST_CASE("TOptional: ValueOr rvalue overload returns default when empty", "[Container][Optional]")
{
    TOptional<Int32> opt;
    REQUIRE(GP::Move(opt).ValueOr(99) == 99);
}

TEST_CASE("TOptional: Reset destroys the contained value and marks optional as empty", "[Container][Optional]")
{
    Int32 destroyCount = 0;
    TOptional<FDestroyTracker> opt{ FDestroyTracker(destroyCount) };
    destroyCount = 0;
    opt.Reset();
    REQUIRE(destroyCount == 1);
    REQUIRE_FALSE(opt.HasValue());
}

TEST_CASE("TOptional: Reset on empty optional is a no-op", "[Container][Optional]")
{
    TOptional<Int32> opt;
    opt.Reset();
    REQUIRE_FALSE(opt.HasValue());
}

TEST_CASE("TOptional: Double Reset does not double-destroy the value", "[Container][Optional]")
{
    Int32 destroyCount = 0;
    TOptional<FDestroyTracker> opt{ FDestroyTracker(destroyCount) };
    destroyCount = 0;
    opt.Reset();
    opt.Reset();
    REQUIRE(destroyCount == 1);
}

TEST_CASE("TOptional: Emplace constructs a value in-place from multiple arguments", "[Container][Optional]")
{
    TOptional<FMultiArg> opt;
    opt.Emplace(1, 2, 3.0f);
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value().x == 1);
    REQUIRE(opt.Value().y == 2);
    REQUIRE(opt.Value().z == 3.0f);
}

TEST_CASE("TOptional: Emplace on non-empty destroys the old value first", "[Container][Optional]")
{
    Int32 destroyCount = 0;
    TOptional<FDestroyTracker> opt{ FDestroyTracker(destroyCount) };
    destroyCount = 0;
    opt.Emplace(destroyCount);
    REQUIRE(destroyCount == 1);
}

TEST_CASE("TOptional: Emplace returns a reference to the newly constructed value", "[Container][Optional]")
{
    TOptional<Int32> opt;
    Int32& ref = opt.Emplace(77);
    REQUIRE(ref == 77);
    ref = 88;
    REQUIRE(opt.Value() == 88);
}

TEST_CASE("TOptional: Emplace replaces the existing value", "[Container][Optional]")
{
    TOptional<Int32> opt(1);
    opt.Emplace(99);
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value() == 99);
}

TEST_CASE("TOptional: Swap exchanges values of two non-empty optionals", "[Container][Optional]")
{
    TOptional<Int32> a(1);
    TOptional<Int32> b(2);
    a.Swap(b);
    REQUIRE(a.Value() == 2);
    REQUIRE(b.Value() == 1);
}

TEST_CASE("TOptional: Swap transfers value from non-empty to empty", "[Container][Optional]")
{
    TOptional<Int32> a;
    TOptional<Int32> b(5);
    a.Swap(b);
    REQUIRE(a.HasValue());
    REQUIRE(a.Value() == 5);
    REQUIRE_FALSE(b.HasValue());
}

TEST_CASE("TOptional: Swap transfers value from empty to non-empty", "[Container][Optional]")
{
    TOptional<Int32> a(3);
    TOptional<Int32> b;
    a.Swap(b);
    REQUIRE_FALSE(a.HasValue());
    REQUIRE(b.HasValue());
    REQUIRE(b.Value() == 3);
}

TEST_CASE("TOptional: Swap two empty optionals stays empty", "[Container][Optional]")
{
    TOptional<Int32> a;
    TOptional<Int32> b;
    a.Swap(b);
    REQUIRE_FALSE(a.HasValue());
    REQUIRE_FALSE(b.HasValue());
}

TEST_CASE(
    "TOptional: Swap preserves both values alive and destroys each exactly once on scope exit", "[Container][Optional]"
)
{
    Int32 destroyA = 0;
    Int32 destroyB = 0;
    {
        TOptional<FDestroyTracker> a{ FDestroyTracker(destroyA) };
        TOptional<FDestroyTracker> b{ FDestroyTracker(destroyB) };
        destroyA = 0;
        destroyB = 0;
        a.Swap(b);
        destroyA = 0;
        destroyB = 0;
    }
    REQUIRE(destroyA == 1);
    REQUIRE(destroyB == 1);
}

TEST_CASE("TOptional: MakeOptional constructs a non-empty optional", "[Container][Optional]")
{
    auto opt = MakeOptional<Int32>(42);
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value() == 42);
}

TEST_CASE("TOptional: MakeOptional with multiple arguments constructs value in-place", "[Container][Optional]")
{
    auto opt = MakeOptional<FMultiArg>(3, 4, 1.5f);
    REQUIRE(opt.HasValue());
    REQUIRE(opt.Value().x == 3);
    REQUIRE(opt.Value().y == 4);
    REQUIRE(opt.Value().z == 1.5f);
}

TEST_CASE("TOptional: Storage alignment is correct for the contained type", "[Container][Optional]")
{
    const TOptional<Float64> opt(3.14);
    REQUIRE(reinterpret_cast<UIntPtrT>(&opt.Value()) % alignof(Float64) == 0);
}

TEST_CASE("TOptional: ValueType alias resolves to the contained type", "[Container][Optional]")
{
    STATIC_REQUIRE(std::is_same_v<TOptional<Int32>::ValueType, Int32>);
}
