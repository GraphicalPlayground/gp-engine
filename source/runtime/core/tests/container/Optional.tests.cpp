// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "container/Optional.hpp"
#include "CoreMinimal.hpp"
#include <catch2/catch_test_macros.hpp>

namespace gp::tests
{

struct DestroyTracker
{
    Int32* m_count;

    explicit DestroyTracker(Int32& count)
        : m_count(&count)
    {}

    DestroyTracker(const DestroyTracker&) = default;
    DestroyTracker(DestroyTracker&&) noexcept = default;
    DestroyTracker& operator=(const DestroyTracker&) = default;
    DestroyTracker& operator=(DestroyTracker&&) noexcept = default;

    ~DestroyTracker()
    {
        ++(*m_count);
    }
};

struct MoveSpy
{
    Int32 value{ 0 };
    bool moved{ false };

    MoveSpy() = default;

    explicit MoveSpy(Int32 v)
        : value(v)
    {}

    MoveSpy(const MoveSpy&) = default;

    MoveSpy(MoveSpy&& other) noexcept
        : value(other.value)
        , moved(false)
    {
        other.moved = true;
    }

    MoveSpy& operator=(const MoveSpy&) = default;

    MoveSpy& operator=(MoveSpy&& other) noexcept
    {
        value = other.value;
        other.moved = true;
        return *this;
    }

    bool operator==(const MoveSpy& other) const noexcept
    {
        return value == other.value;
    }

    bool operator!=(const MoveSpy& other) const noexcept
    {
        return value != other.value;
    }
};

struct MultiArg
{
    Int32 x{ 0 };
    Int32 y{ 0 };
    Float32 z{ 0.0f };

    MultiArg(Int32 inX, Int32 inY, Float32 inZ)
        : x(inX)
        , y(inY)
        , z(inZ)
    {}

    bool operator==(const MultiArg& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

TEST_CASE("Optional - Default construction yields empty optional", "[container][Optional]")
{
    const Optional<Int32> opt;
    REQUIRE_FALSE(opt.hasValue());
    REQUIRE_FALSE(static_cast<bool>(opt));
}

TEST_CASE("Optional - Default construction is noexcept", "[container][Optional]")
{
    STATIC_REQUIRE(noexcept(Optional<Int32>{}));
}

TEST_CASE("Optional - gp::nullOpt construction yields empty optional", "[container][Optional]")
{
    const Optional<Int32> opt(gp::nullOpt);
    REQUIRE_FALSE(opt.hasValue());
    REQUIRE_FALSE(static_cast<bool>(opt));
}

TEST_CASE("Optional - gp::nullOpt construction is noexcept", "[container][Optional]")
{
    STATIC_REQUIRE(noexcept(Optional<Int32>{ gp::nullOpt }));
}

TEST_CASE("Optional - Copy-construction from value stores the value", "[container][Optional]")
{
    const Int32 val = 42;
    const Optional<Int32> opt(val);
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value() == 42);
}

TEST_CASE("Optional - Move-construction from value stores the value and moves the source", "[container][Optional]")
{
    MoveSpy src(99);
    Optional<MoveSpy> opt(std::move(src));
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value().value == 99);
    REQUIRE(src.moved);
}

TEST_CASE("Optional - In-place construction from multiple arguments", "[container][Optional]")
{
    Optional<MultiArg> opt(10, 20, 3.14f);
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value().x == 10);
    REQUIRE(opt.value().y == 20);
    REQUIRE(opt.value().z == 3.14f);
}

TEST_CASE("Optional - Copy-construction from non-empty optional copies value", "[container][Optional]")
{
    const Optional<Int32> src(7);
    const Optional<Int32> opt(src);
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value() == 7);
    REQUIRE(src.hasValue());
    REQUIRE(src.value() == 7);
}

TEST_CASE("Optional - Copy-construction from empty optional yields empty optional", "[container][Optional]")
{
    const Optional<Int32> src;
    const Optional<Int32> opt(src);
    REQUIRE_FALSE(opt.hasValue());
}

TEST_CASE(
    "Optional - Move-construction from non-empty optional transfers value and empties source", "[container][Optional]"
)
{
    Optional<MoveSpy> src(MoveSpy(55));
    Optional<MoveSpy> opt(std::move(src));
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value().value == 55);
    REQUIRE_FALSE(src.hasValue());
}

TEST_CASE("Optional - Move-construction from empty optional yields empty optional", "[container][Optional]")
{
    Optional<Int32> src;
    const Optional<Int32> opt(std::move(src));
    REQUIRE_FALSE(opt.hasValue());
}

TEST_CASE("Optional - Destructor destroys the contained value exactly once", "[container][Optional]")
{
    Int32 destroyCount = 0;
    {
        Optional<DestroyTracker> opt{ DestroyTracker(destroyCount) };
        destroyCount = 0;
    }
    REQUIRE(destroyCount == 1);
}

TEST_CASE("Optional - Destructor of empty optional does not invoke any destructor", "[container][Optional]")
{
    Int32 destroyCount = 0;
    {
        Optional<DestroyTracker> opt;
    }
    REQUIRE(destroyCount == 0);
}

TEST_CASE("Optional - gp::nullOpt assignment to non-empty resets the optional", "[container][Optional]")
{
    Optional<Int32> opt(42);
    opt = gp::nullOpt;
    REQUIRE_FALSE(opt.hasValue());
}

TEST_CASE("Optional - gp::nullOpt assignment to empty remains empty", "[container][Optional]")
{
    Optional<Int32> opt;
    opt = gp::nullOpt;
    REQUIRE_FALSE(opt.hasValue());
}

TEST_CASE("Optional - gp::nullOpt assignment destroys the contained value", "[container][Optional]")
{
    Int32 destroyCount = 0;
    Optional<DestroyTracker> opt{ DestroyTracker(destroyCount) };
    destroyCount = 0;
    opt = gp::nullOpt;
    REQUIRE(destroyCount == 1);
}

TEST_CASE("Optional - Copy-assignment from non-empty to non-empty updates the value", "[container][Optional]")
{
    Optional<Int32> dst(1);
    const Optional<Int32> src(2);
    dst = src;
    REQUIRE(dst.hasValue());
    REQUIRE(dst.value() == 2);
}

TEST_CASE("Optional - Copy-assignment from non-empty to empty constructs the value", "[container][Optional]")
{
    Optional<Int32> dst;
    const Optional<Int32> src(99);
    dst = src;
    REQUIRE(dst.hasValue());
    REQUIRE(dst.value() == 99);
}

TEST_CASE("Optional - Copy-assignment from empty to non-empty resets the destination", "[container][Optional]")
{
    Optional<Int32> dst(10);
    const Optional<Int32> src;
    dst = src;
    REQUIRE_FALSE(dst.hasValue());
}

TEST_CASE("Optional - Copy-assignment from empty to empty remains empty", "[container][Optional]")
{
    Optional<Int32> dst;
    const Optional<Int32> src;
    dst = src;
    REQUIRE_FALSE(dst.hasValue());
}

TEST_CASE("Optional - Copy self-assignment is safe for non-empty optional", "[container][Optional]")
{
    Optional<Int32> opt(7);
    opt = opt;
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value() == 7);
}

TEST_CASE(
    "Optional - Move-assignment from non-empty to non-empty updates destination and clears source",
    "[container][Optional]"
)
{
    Optional<MoveSpy> dst(MoveSpy(1));
    Optional<MoveSpy> src(MoveSpy(2));
    dst = std::move(src);
    REQUIRE(dst.hasValue());
    REQUIRE(dst.value().value == 2);
    REQUIRE_FALSE(src.hasValue());
}

TEST_CASE(
    "Optional - Move-assignment from non-empty to empty constructs destination and clears source",
    "[container][Optional]"
)
{
    Optional<MoveSpy> dst;
    Optional<MoveSpy> src(MoveSpy(42));
    dst = std::move(src);
    REQUIRE(dst.hasValue());
    REQUIRE(dst.value().value == 42);
    REQUIRE_FALSE(src.hasValue());
}

TEST_CASE("Optional - Move-assignment from empty to non-empty resets the destination", "[container][Optional]")
{
    Optional<MoveSpy> dst(MoveSpy(10));
    Optional<MoveSpy> src;
    dst = std::move(src);
    REQUIRE_FALSE(dst.hasValue());
}

TEST_CASE("Optional - Move self-assignment is safe for non-empty optional", "[container][Optional]")
{
    Optional<Int32> opt(5);
    opt = std::move(opt);
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value() == 5);
}

TEST_CASE("Optional - Value-assignment to empty constructs the value", "[container][Optional]")
{
    Optional<Int32> opt;
    opt = 100;
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value() == 100);
}

TEST_CASE("Optional - Value-assignment to non-empty updates the value", "[container][Optional]")
{
    Optional<Int32> opt(5);
    opt = 50;
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value() == 50);
}

TEST_CASE("Optional - operator== two non-empty optionals with equal values returns true", "[container][Optional]")
{
    const Optional<Int32> a(3);
    const Optional<Int32> b(3);
    REQUIRE(a == b);
}

TEST_CASE("Optional - operator== two non-empty optionals with different values returns false", "[container][Optional]")
{
    const Optional<Int32> a(3);
    const Optional<Int32> b(4);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("Optional - operator== two empty optionals returns true", "[container][Optional]")
{
    const Optional<Int32> a;
    const Optional<Int32> b;
    REQUIRE(a == b);
}

TEST_CASE("Optional - operator== empty and non-empty returns false", "[container][Optional]")
{
    const Optional<Int32> a;
    const Optional<Int32> b(1);
    REQUIRE_FALSE(a == b);
    REQUIRE_FALSE(b == a);
}

TEST_CASE("Optional - operator!= two non-empty optionals with different values returns true", "[container][Optional]")
{
    const Optional<Int32> a(1);
    const Optional<Int32> b(2);
    REQUIRE(a != b);
}

TEST_CASE("Optional - operator!= two non-empty optionals with equal values returns false", "[container][Optional]")
{
    const Optional<Int32> a(5);
    const Optional<Int32> b(5);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("Optional - operator!= empty and non-empty returns true", "[container][Optional]")
{
    const Optional<Int32> a;
    const Optional<Int32> b(1);
    REQUIRE(a != b);
    REQUIRE(b != a);
}

TEST_CASE("Optional - operator== with gp::nullOpt returns true for empty optional", "[container][Optional]")
{
    const Optional<Int32> opt;
    REQUIRE(opt == gp::nullOpt);
}

TEST_CASE("Optional - operator== with gp::nullOpt returns false for non-empty optional", "[container][Optional]")
{
    const Optional<Int32> opt(1);
    REQUIRE_FALSE(opt == gp::nullOpt);
}

TEST_CASE("Optional - operator!= with gp::nullOpt returns false for empty optional", "[container][Optional]")
{
    const Optional<Int32> opt;
    REQUIRE_FALSE(opt != gp::nullOpt);
}

TEST_CASE("Optional - operator!= with gp::nullOpt returns true for non-empty optional", "[container][Optional]")
{
    const Optional<Int32> opt(1);
    REQUIRE(opt != gp::nullOpt);
}

TEST_CASE("Optional - operator== with value returns true when value matches", "[container][Optional]")
{
    const Optional<Int32> opt(42);
    REQUIRE(opt == 42);
}

TEST_CASE("Optional - operator== with value returns false for empty optional", "[container][Optional]")
{
    const Optional<Int32> opt;
    REQUIRE_FALSE(opt == 42);
}

TEST_CASE("Optional - operator== with value returns false when value differs", "[container][Optional]")
{
    const Optional<Int32> opt(42);
    REQUIRE_FALSE(opt == 43);
}

TEST_CASE("Optional - operator!= with value returns true for empty optional", "[container][Optional]")
{
    const Optional<Int32> opt;
    REQUIRE(opt != 42);
}

TEST_CASE("Optional - operator!= with value returns true when values differ", "[container][Optional]")
{
    const Optional<Int32> opt(1);
    REQUIRE(opt != 2);
}

TEST_CASE("Optional - operator!= with value returns false when value matches", "[container][Optional]")
{
    const Optional<Int32> opt(7);
    REQUIRE_FALSE(opt != 7);
}

TEST_CASE("Optional - operator* on lvalue returns reference to contained value", "[container][Optional]")
{
    Optional<Int32> opt(10);
    REQUIRE(*opt == 10);
}

TEST_CASE("Optional - operator* returns a mutable reference", "[container][Optional]")
{
    Optional<Int32> opt(10);
    *opt = 20;
    REQUIRE(opt.value() == 20);
}

TEST_CASE("Optional - operator* on const lvalue returns const reference", "[container][Optional]")
{
    const Optional<Int32> opt(15);
    REQUIRE(*opt == 15);
}

TEST_CASE("Optional - operator* on rvalue moves out the value", "[container][Optional]")
{
    Optional<MoveSpy> opt(MoveSpy(77));
    MoveSpy val = *std::move(opt);
    REQUIRE(val.value == 77);
}

TEST_CASE("Optional - operator-> accesses members of the contained value", "[container][Optional]")
{
    Optional<MoveSpy> opt(MoveSpy(88));
    REQUIRE(opt->value == 88);
}

TEST_CASE("Optional - operator-> const accesses members of the contained value", "[container][Optional]")
{
    const Optional<MoveSpy> opt(MoveSpy(88));
    REQUIRE(opt->value == 88);
}

TEST_CASE("Optional - hasValue returns true when value is present", "[container][Optional]")
{
    const Optional<Int32> opt(1);
    REQUIRE(opt.hasValue());
}

TEST_CASE("Optional - hasValue returns false when empty", "[container][Optional]")
{
    const Optional<Int32> opt;
    REQUIRE_FALSE(opt.hasValue());
}

TEST_CASE("Optional - hasValue is noexcept", "[container][Optional]")
{
    const Optional<Int32> opt;
    STATIC_REQUIRE(noexcept(opt.hasValue()));
}

TEST_CASE("Optional - operator bool returns true for non-empty optional", "[container][Optional]")
{
    const Optional<Int32> opt(1);
    REQUIRE(static_cast<bool>(opt));
}

TEST_CASE("Optional - operator bool returns false for empty optional", "[container][Optional]")
{
    const Optional<Int32> opt;
    REQUIRE_FALSE(static_cast<bool>(opt));
}

TEST_CASE("Optional - operator bool is noexcept", "[container][Optional]")
{
    const Optional<Int32> opt;
    STATIC_REQUIRE(noexcept(static_cast<bool>(opt)));
}

TEST_CASE("Optional - Value lvalue overload returns reference to stored value", "[container][Optional]")
{
    Optional<Int32> opt(55);
    REQUIRE(opt.value() == 55);
    opt.value() = 66;
    REQUIRE(opt.value() == 66);
}

TEST_CASE("Optional - Value const lvalue overload returns const reference", "[container][Optional]")
{
    const Optional<Int32> opt(55);
    REQUIRE(opt.value() == 55);
}

TEST_CASE("Optional - Value rvalue overload moves out the value", "[container][Optional]")
{
    Optional<MoveSpy> opt(MoveSpy(33));
    MoveSpy val = std::move(opt).value();
    REQUIRE(val.value == 33);
}

TEST_CASE("Optional - valueOr returns contained value when present", "[container][Optional]")
{
    const Optional<Int32> opt(42);
    REQUIRE(opt.valueOr(0) == 42);
}

TEST_CASE("Optional - valueOr returns default when empty", "[container][Optional]")
{
    const Optional<Int32> opt;
    REQUIRE(opt.valueOr(99) == 99);
}

TEST_CASE("Optional - valueOr rvalue overload returns contained value when present", "[container][Optional]")
{
    Optional<Int32> opt(42);
    REQUIRE(std::move(opt).valueOr(0) == 42);
}

TEST_CASE("Optional - valueOr rvalue overload returns default when empty", "[container][Optional]")
{
    Optional<Int32> opt;
    REQUIRE(std::move(opt).valueOr(99) == 99);
}

TEST_CASE("Optional - Reset destroys the contained value and marks optional as empty", "[container][Optional]")
{
    Int32 destroyCount = 0;
    Optional<DestroyTracker> opt{ DestroyTracker(destroyCount) };
    destroyCount = 0;
    opt.reset();
    REQUIRE(destroyCount == 1);
    REQUIRE_FALSE(opt.hasValue());
}

TEST_CASE("Optional - Reset on empty optional is a no-op", "[container][Optional]")
{
    Optional<Int32> opt;
    opt.reset();
    REQUIRE_FALSE(opt.hasValue());
}

TEST_CASE("Optional - Double Reset does not double-destroy the value", "[container][Optional]")
{
    Int32 destroyCount = 0;
    Optional<DestroyTracker> opt{ DestroyTracker(destroyCount) };
    destroyCount = 0;
    opt.reset();
    opt.reset();
    REQUIRE(destroyCount == 1);
}

TEST_CASE("Optional - Emplace constructs a value in-place from multiple arguments", "[container][Optional]")
{
    Optional<MultiArg> opt;
    opt.emplace(1, 2, 3.0f);
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value().x == 1);
    REQUIRE(opt.value().y == 2);
    REQUIRE(opt.value().z == 3.0f);
}

TEST_CASE("Optional - Emplace on non-empty destroys the old value first", "[container][Optional]")
{
    Int32 destroyCount = 0;
    Optional<DestroyTracker> opt{ DestroyTracker(destroyCount) };
    destroyCount = 0;
    opt.emplace(destroyCount);
    REQUIRE(destroyCount == 1);
}

TEST_CASE("Optional - Emplace returns a reference to the newly constructed value", "[container][Optional]")
{
    Optional<Int32> opt;
    Int32& ref = opt.emplace(77);
    REQUIRE(ref == 77);
    ref = 88;
    REQUIRE(opt.value() == 88);
}

TEST_CASE("Optional - Emplace replaces the existing value", "[container][Optional]")
{
    Optional<Int32> opt(1);
    opt.emplace(99);
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value() == 99);
}

TEST_CASE("Optional - Swap exchanges values of two non-empty optionals", "[container][Optional]")
{
    Optional<Int32> a(1);
    Optional<Int32> b(2);
    a.swap(b);
    REQUIRE(a.value() == 2);
    REQUIRE(b.value() == 1);
}

TEST_CASE("Optional - Swap transfers value from non-empty to empty", "[container][Optional]")
{
    Optional<Int32> a;
    Optional<Int32> b(5);
    a.swap(b);
    REQUIRE(a.hasValue());
    REQUIRE(a.value() == 5);
    REQUIRE_FALSE(b.hasValue());
}

TEST_CASE("Optional - Swap transfers value from empty to non-empty", "[container][Optional]")
{
    Optional<Int32> a(3);
    Optional<Int32> b;
    a.swap(b);
    REQUIRE_FALSE(a.hasValue());
    REQUIRE(b.hasValue());
    REQUIRE(b.value() == 3);
}

TEST_CASE("Optional - Swap two empty optionals stays empty", "[container][Optional]")
{
    Optional<Int32> a;
    Optional<Int32> b;
    a.swap(b);
    REQUIRE_FALSE(a.hasValue());
    REQUIRE_FALSE(b.hasValue());
}

TEST_CASE(
    "Optional - Swap preserves both values alive and destroys each exactly once on scope exit", "[container][Optional]"
)
{
    Int32 destroyA = 0;
    Int32 destroyB = 0;
    {
        Optional<DestroyTracker> a{ DestroyTracker(destroyA) };
        Optional<DestroyTracker> b{ DestroyTracker(destroyB) };
        destroyA = 0;
        destroyB = 0;
        a.swap(b);
        destroyA = 0;
        destroyB = 0;
    }
    REQUIRE(destroyA == 1);
    REQUIRE(destroyB == 1);
}

TEST_CASE("Optional - MakeOptional constructs a non-empty optional", "[container][Optional]")
{
    auto opt = gp::makeOptional<Int32>(42);
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value() == 42);
}

TEST_CASE("Optional - MakeOptional with multiple arguments constructs value in-place", "[container][Optional]")
{
    auto opt = gp::makeOptional<MultiArg>(3, 4, 1.5f);
    REQUIRE(opt.hasValue());
    REQUIRE(opt.value().x == 3);
    REQUIRE(opt.value().y == 4);
    REQUIRE(opt.value().z == 1.5f);
}

TEST_CASE("Optional - Storage alignment is correct for the contained type", "[container][Optional]")
{
    const Optional<Float64> opt(3.14);
    REQUIRE(reinterpret_cast<UIntPtr>(&opt.value()) % alignof(Float64) == 0);
}

TEST_CASE("Optional - ValueType alias resolves to the contained type", "[container][Optional]")
{
    STATIC_REQUIRE(std::is_same_v<Optional<Int32>::ValueType, Int32>);
}

}   // namespace gp::tests
