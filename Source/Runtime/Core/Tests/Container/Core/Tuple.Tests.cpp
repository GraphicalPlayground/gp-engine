// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Core/Tuple.hpp"
#include "CoreTypes.hpp"
#include <catch2/catch_test_macros.hpp>
#include <compare>
#include <type_traits>

using namespace GP;

namespace
{

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

    auto operator<=>(const FMoveSpy& other) const noexcept { return value <=> other.value; }
};

struct FDestroyTracker
{
    Int32* counter{ nullptr };

    FDestroyTracker() = default;

    explicit FDestroyTracker(Int32* c)
        : counter(c)
    {}

    FDestroyTracker(const FDestroyTracker&) = default;

    FDestroyTracker(FDestroyTracker&& other) noexcept
        : counter(GP::Exchange(other.counter, nullptr))
    {}

    ~FDestroyTracker()
    {
        if (counter) { ++(*counter); }
    }
};

}   // namespace

TEST_CASE("TTuple: Default construction value-initializes all elements", "[Container][Tuple]")
{
    const TTuple<Int32, Float32, Int64> t;
    REQUIRE(t.Get<0>() == 0);
    REQUIRE(t.Get<1>() == 0.0f);
    REQUIRE(t.Get<2>() == 0LL);
}

TEST_CASE("TTuple: Default construction is constexpr", "[Container][Tuple]")
{
    constexpr TTuple<Int32, Int32> t;
    STATIC_REQUIRE(t.Get<0>() == 0);
    STATIC_REQUIRE(t.Get<1>() == 0);
}

TEST_CASE("TTuple: Empty tuple default construction compiles and has size zero", "[Container][Tuple]")
{
    STATIC_REQUIRE(TTuple<>::kSize == 0);
    STATIC_REQUIRE(TTuple<>::Size() == 0);
}

TEST_CASE("TTuple: Explicit construction stores all values in order", "[Container][Tuple]")
{
    const TTuple<Int32, Float32, Int64> t(1, 2.5f, 3LL);
    REQUIRE(t.Get<0>() == 1);
    REQUIRE(t.Get<1>() == 2.5f);
    REQUIRE(t.Get<2>() == 3LL);
}

TEST_CASE("TTuple: Explicit construction is constexpr", "[Container][Tuple]")
{
    constexpr TTuple<Int32, Int32, Int32> t(10, 20, 30);
    STATIC_REQUIRE(t.Get<0>() == 10);
    STATIC_REQUIRE(t.Get<1>() == 20);
    STATIC_REQUIRE(t.Get<2>() == 30);
}

TEST_CASE("TTuple: Single-element tuple stores the value correctly", "[Container][Tuple]")
{
    const TTuple<Int32> t(42);
    REQUIRE(t.Get<0>() == 42);
}

TEST_CASE("TTuple: Heterogeneous construction widens Int32 to Int64", "[Container][Tuple]")
{
    const TTuple<Int64, Float64> t(5, 3.0f);
    REQUIRE(t.Get<0>() == 5LL);
    REQUIRE(t.Get<1>() == static_cast<Float64>(3.0f));
}

TEST_CASE("TTuple: Construction from lvalue values copies them into storage", "[Container][Tuple]")
{
    const Int32 a = 7;
    const Float32 b = 3.14f;
    const TTuple<Int32, Float32> t(a, b);
    REQUIRE(t.Get<0>() == 7);
    REQUIRE(t.Get<1>() == 3.14f);
}

TEST_CASE("TTuple: Construction from rvalue values moves them into storage", "[Container][Tuple]")
{
    FMoveSpy a(10);
    FMoveSpy b(20);
    const TTuple<FMoveSpy, FMoveSpy> t(GP::Move(a), GP::Move(b));
    REQUIRE(t.Get<0>().value == 10);
    REQUIRE(t.Get<1>().value == 20);
    REQUIRE(a.moved);
    REQUIRE(b.moved);
}

TEST_CASE("TTuple: Copy-construction from another TTuple copies all elements", "[Container][Tuple]")
{
    const TTuple<Int32, Int32, Int32> src(1, 2, 3);
    const TTuple<Int32, Int32, Int32> dst(src);
    REQUIRE(dst.Get<0>() == 1);
    REQUIRE(dst.Get<1>() == 2);
    REQUIRE(dst.Get<2>() == 3);
}

TEST_CASE("TTuple: Copy-construction does not modify the source", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> src(5, 6);
    const TTuple<Int32, Int32> dst(src);
    REQUIRE(src.Get<0>() == 5);
    REQUIRE(src.Get<1>() == 6);
}

TEST_CASE("TTuple: Move-construction transfers all elements from the source", "[Container][Tuple]")
{
    TTuple<FMoveSpy, FMoveSpy> src(FMoveSpy(1), FMoveSpy(2));
    const TTuple<FMoveSpy, FMoveSpy> dst(GP::Move(src));
    REQUIRE(dst.Get<0>().value == 1);
    REQUIRE(dst.Get<1>().value == 2);
    REQUIRE(src.Get<0>().moved);
    REQUIRE(src.Get<1>().moved);
}

TEST_CASE("TTuple: Copy-construction is noexcept for trivial types", "[Container][Tuple]")
{
    STATIC_REQUIRE(std::is_nothrow_copy_constructible_v<TTuple<Int32, Float32>>);
}

TEST_CASE("TTuple: Move-construction is noexcept for trivial types", "[Container][Tuple]")
{
    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<TTuple<Int32, Float32>>);
}

TEST_CASE("TTuple: Copy-assignment overwrites all elements", "[Container][Tuple]")
{
    TTuple<Int32, Int32> dst(0, 0);
    const TTuple<Int32, Int32> src(4, 8);
    dst = src;
    REQUIRE(dst.Get<0>() == 4);
    REQUIRE(dst.Get<1>() == 8);
}

TEST_CASE("TTuple: Copy-assignment does not modify the source", "[Container][Tuple]")
{
    TTuple<Int32, Int32> dst(0, 0);
    const TTuple<Int32, Int32> src(4, 8);
    dst = src;
    REQUIRE(src.Get<0>() == 4);
    REQUIRE(src.Get<1>() == 8);
}

TEST_CASE("TTuple: Move-assignment transfers all elements and marks source as moved", "[Container][Tuple]")
{
    TTuple<FMoveSpy, FMoveSpy> dst(FMoveSpy(0), FMoveSpy(0));
    TTuple<FMoveSpy, FMoveSpy> src(FMoveSpy(5), FMoveSpy(6));
    dst = GP::Move(src);
    REQUIRE(dst.Get<0>().value == 5);
    REQUIRE(dst.Get<1>().value == 6);
    REQUIRE(src.Get<0>().moved);
    REQUIRE(src.Get<1>().moved);
}

TEST_CASE("TTuple: Copy-assignment returns reference to self", "[Container][Tuple]")
{
    TTuple<Int32> a(1);
    TTuple<Int32> b(2);
    TTuple<Int32>& ref = (a = b);
    REQUIRE(&ref == &a);
}

TEST_CASE("TTuple: Move-assignment returns reference to self", "[Container][Tuple]")
{
    TTuple<Int32> a(1);
    TTuple<Int32> b(2);
    TTuple<Int32>& ref = (a = GP::Move(b));
    REQUIRE(&ref == &a);
}

TEST_CASE("TTuple: Get<0> returns reference to the first element", "[Container][Tuple]")
{
    TTuple<Int32, Int32> t(10, 20);
    REQUIRE(t.Get<0>() == 10);
}

TEST_CASE("TTuple: Get<1> returns reference to the second element", "[Container][Tuple]")
{
    TTuple<Int32, Int32> t(10, 20);
    REQUIRE(t.Get<1>() == 20);
}

TEST_CASE("TTuple: Get<I> on const tuple returns const reference", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> t(3, 7);
    STATIC_REQUIRE(std::is_same_v<decltype(t.Get<0>()), const Int32&>);
    REQUIRE(t.Get<0>() == 3);
}

TEST_CASE("TTuple: Get<I> on mutable tuple allows element mutation", "[Container][Tuple]")
{
    TTuple<Int32, Int32> t(1, 2);
    t.Get<0>() = 99;
    REQUIRE(t.Get<0>() == 99);
}

TEST_CASE("TTuple: Get<I> is constexpr for all indices of a three-element tuple", "[Container][Tuple]")
{
    constexpr TTuple<Int32, Int32, Int32> t(1, 2, 3);
    STATIC_REQUIRE(t.Get<0>() == 1);
    STATIC_REQUIRE(t.Get<1>() == 2);
    STATIC_REQUIRE(t.Get<2>() == 3);
}

TEST_CASE("TTuple: GP::Get free function returns correct element by index", "[Container][Tuple]")
{
    TTuple<Int32, Float32, Int64> t(7, 1.5f, 99LL);
    REQUIRE(GP::Get<0>(t) == 7);
    REQUIRE(GP::Get<1>(t) == 1.5f);
    REQUIRE(GP::Get<2>(t) == 99LL);
}

TEST_CASE("TTuple: GP::Get on const tuple returns const reference", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> t(3, 4);
    STATIC_REQUIRE(std::is_same_v<decltype(GP::Get<0>(t)), const Int32&>);
    REQUIRE(GP::Get<0>(t) == 3);
}

TEST_CASE("TTuple: GP::Get on rvalue tuple returns rvalue reference", "[Container][Tuple]")
{
    TTuple<FMoveSpy, FMoveSpy> t(FMoveSpy(10), FMoveSpy(20));
    FMoveSpy extracted = GP::Get<0>(GP::Move(t));
    REQUIRE(extracted.value == 10);
}

TEST_CASE("TTuple: GP::Get free function allows mutation through non-const reference", "[Container][Tuple]")
{
    TTuple<Int32, Int32> t(1, 2);
    GP::Get<1>(t) = 42;
    REQUIRE(t.Get<1>() == 42);
}

TEST_CASE("TTuple: GP::Get is constexpr", "[Container][Tuple]")
{
    constexpr TTuple<Int32, Int32> t(5, 10);
    STATIC_REQUIRE(GP::Get<0>(t) == 5);
    STATIC_REQUIRE(GP::Get<1>(t) == 10);
}

TEST_CASE("TTuple: Size() returns the number of element types", "[Container][Tuple]")
{
    STATIC_REQUIRE(TTuple<Int32, Float32, Int64>::Size() == 3);
}

TEST_CASE("TTuple: kSize equals the number of element types", "[Container][Tuple]")
{
    STATIC_REQUIRE(TTuple<Int32, Float32>::kSize == 2);
}

TEST_CASE("TTuple: Size() returns zero for an empty tuple", "[Container][Tuple]")
{
    STATIC_REQUIRE(TTuple<>::Size() == 0);
}

TEST_CASE("TTuple: Size() is consistent with kSize", "[Container][Tuple]")
{
    STATIC_REQUIRE(TTuple<Int32, Int32, Int32>::Size() == TTuple<Int32, Int32, Int32>::kSize);
}

TEST_CASE("TTuple: operator== returns true when all elements are equal", "[Container][Tuple]")
{
    const TTuple<Int32, Float32> a(1, 2.0f);
    const TTuple<Int32, Float32> b(1, 2.0f);
    REQUIRE(a == b);
}

TEST_CASE("TTuple: operator== returns false when first element differs", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 2);
    const TTuple<Int32, Int32> b(9, 2);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("TTuple: operator== returns false when second element differs", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 2);
    const TTuple<Int32, Int32> b(1, 9);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("TTuple: operator== returns false when last element of three-element tuple differs", "[Container][Tuple]")
{
    const TTuple<Int32, Int32, Int32> a(1, 2, 3);
    const TTuple<Int32, Int32, Int32> b(1, 2, 4);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("TTuple: operator== is symmetric", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(3, 4);
    const TTuple<Int32, Int32> b(3, 4);
    REQUIRE(a == b);
    REQUIRE(b == a);
}

TEST_CASE("TTuple: operator== is constexpr", "[Container][Tuple]")
{
    constexpr TTuple<Int32, Int32> a(5, 6);
    constexpr TTuple<Int32, Int32> b(5, 6);
    STATIC_REQUIRE(a == b);
}

TEST_CASE("TTuple: Empty tuple equality always returns true", "[Container][Tuple]")
{
    constexpr TTuple<> a;
    constexpr TTuple<> b;
    STATIC_REQUIRE(a == b);
}

TEST_CASE("TTuple: operator!= returns true when any element differs", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 2);
    const TTuple<Int32, Int32> b(3, 4);
    REQUIRE(a != b);
}

TEST_CASE("TTuple: operator!= returns false when all elements are equal", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(5, 6);
    const TTuple<Int32, Int32> b(5, 6);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("TTuple: operator!= is symmetric", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 2);
    const TTuple<Int32, Int32> b(3, 4);
    REQUIRE(a != b);
    REQUIRE(b != a);
}

TEST_CASE("TTuple: operator!= is constexpr", "[Container][Tuple]")
{
    constexpr TTuple<Int32, Int32> a(1, 2);
    constexpr TTuple<Int32, Int32> b(3, 4);
    STATIC_REQUIRE(a != b);
}

TEST_CASE("TTuple: operator<=> returns less-than when first element is smaller", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 5);
    const TTuple<Int32, Int32> b(2, 3);
    REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TTuple: operator<=> returns less-than when first element equal and second is smaller", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 2);
    const TTuple<Int32, Int32> b(1, 5);
    REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TTuple: operator<=> returns equivalent when all elements are equal", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(3, 4);
    const TTuple<Int32, Int32> b(3, 4);
    REQUIRE(std::is_eq(a <=> b));
}

TEST_CASE("TTuple: operator<=> returns greater-than when first element is larger", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(5, 0);
    const TTuple<Int32, Int32> b(2, 9);
    REQUIRE(std::is_gt(a <=> b));
}

TEST_CASE("TTuple: operator<=> returns greater-than when first equal and second is larger", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(3, 9);
    const TTuple<Int32, Int32> b(3, 4);
    REQUIRE(std::is_gt(a <=> b));
}

TEST_CASE("TTuple: operator<=> uses first element exclusively to determine order when it differs", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 100);
    const TTuple<Int32, Int32> b(2, 0);
    REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TTuple: operator<=> compares all three elements lexicographically", "[Container][Tuple]")
{
    const TTuple<Int32, Int32, Int32> a(1, 2, 3);
    const TTuple<Int32, Int32, Int32> b(1, 2, 4);
    REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TTuple: operator<=> is constexpr", "[Container][Tuple]")
{
    constexpr TTuple<Int32, Int32> a(1, 2);
    constexpr TTuple<Int32, Int32> b(1, 3);
    STATIC_REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TTuple: operator<=> returns strong_ordering for integral element types", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 2);
    const TTuple<Int32, Int32> b(1, 2);
    STATIC_REQUIRE(std::is_same_v<decltype(a <=> b), std::strong_ordering>);
}

TEST_CASE("TTuple: operator<=> works for non-trivial three-way-comparable types", "[Container][Tuple]")
{
    const TTuple<FMoveSpy, FMoveSpy> a(FMoveSpy(1), FMoveSpy(2));
    const TTuple<FMoveSpy, FMoveSpy> b(FMoveSpy(1), FMoveSpy(3));
    REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TTuple: Swap exchanges all elements between two tuples", "[Container][Tuple]")
{
    TTuple<Int32, Int32> a(1, 2);
    TTuple<Int32, Int32> b(3, 4);
    a.Swap(b);
    REQUIRE(a.Get<0>() == 3);
    REQUIRE(a.Get<1>() == 4);
    REQUIRE(b.Get<0>() == 1);
    REQUIRE(b.Get<1>() == 2);
}

TEST_CASE("TTuple: Swap with self leaves the tuple unchanged", "[Container][Tuple]")
{
    TTuple<Int32, Int32> a(7, 8);
    a.Swap(a);
    REQUIRE(a.Get<0>() == 7);
    REQUIRE(a.Get<1>() == 8);
}

TEST_CASE("TTuple: Swap works for non-trivial types", "[Container][Tuple]")
{
    TTuple<FMoveSpy, FMoveSpy> a(FMoveSpy(10), FMoveSpy(20));
    TTuple<FMoveSpy, FMoveSpy> b(FMoveSpy(30), FMoveSpy(40));
    a.Swap(b);
    REQUIRE(a.Get<0>().value == 30);
    REQUIRE(a.Get<1>().value == 40);
    REQUIRE(b.Get<0>().value == 10);
    REQUIRE(b.Get<1>().value == 20);
}

TEST_CASE("TTuple: Swap is noexcept for trivially swappable types", "[Container][Tuple]")
{
    TTuple<Int32, Int32> a(1, 2);
    TTuple<Int32, Int32> b(3, 4);
    STATIC_REQUIRE(noexcept(a.Swap(b)));
}

TEST_CASE("TTuple: Swap three-element tuple exchanges all elements correctly", "[Container][Tuple]")
{
    TTuple<Int32, Int32, Int32> a(1, 2, 3);
    TTuple<Int32, Int32, Int32> b(4, 5, 6);
    a.Swap(b);
    REQUIRE(a.Get<0>() == 4);
    REQUIRE(a.Get<1>() == 5);
    REQUIRE(a.Get<2>() == 6);
    REQUIRE(b.Get<0>() == 1);
    REQUIRE(b.Get<1>() == 2);
    REQUIRE(b.Get<2>() == 3);
}

TEST_CASE("TTuple: MakeTuple deduces correct element types and stores values", "[Container][Tuple]")
{
    auto t = MakeTuple(Int32{ 1 }, Float32{ 2.5f }, Int64{ 3LL });
    STATIC_REQUIRE(std::is_same_v<decltype(t), TTuple<Int32, Float32, Int64>>);
    REQUIRE(t.Get<0>() == 1);
    REQUIRE(t.Get<1>() == 2.5f);
    REQUIRE(t.Get<2>() == 3LL);
}

TEST_CASE("TTuple: MakeTuple decays reference arguments to value types", "[Container][Tuple]")
{
    Int32 x = 42;
    auto t = MakeTuple(x);
    STATIC_REQUIRE(std::is_same_v<decltype(t), TTuple<Int32>>);
    REQUIRE(t.Get<0>() == 42);
}

TEST_CASE("TTuple: MakeTuple decays const reference arguments to value types", "[Container][Tuple]")
{
    const Int32 x = 7;
    auto t = MakeTuple(x);
    STATIC_REQUIRE(std::is_same_v<decltype(t), TTuple<Int32>>);
    REQUIRE(t.Get<0>() == 7);
}

TEST_CASE("TTuple: MakeTuple moves rvalue non-trivial elements into storage", "[Container][Tuple]")
{
    FMoveSpy spy(55);
    auto t = MakeTuple(GP::Move(spy));
    REQUIRE(t.Get<0>().value == 55);
    REQUIRE(spy.moved);
}

TEST_CASE("TTuple: MakeTuple is constexpr for trivial types", "[Container][Tuple]")
{
    constexpr TTuple<Int32, Int32> t = MakeTuple(Int32{ 3 }, Int32{ 7 });
    STATIC_REQUIRE(t.Get<0>() == 3);
    STATIC_REQUIRE(t.Get<1>() == 7);
}

TEST_CASE("TTuple: MakeTuple creates empty tuple", "[Container][Tuple]")
{
    auto t = MakeTuple();
    STATIC_REQUIRE(std::is_same_v<decltype(t), TTuple<>>);
    STATIC_REQUIRE(decltype(t)::kSize == 0);
}

TEST_CASE("TTuple: CTAD deduces element types from constructor arguments", "[Container][Tuple]")
{
    const TTuple t(Int32{ 1 }, Float32{ 2.0f }, Int64{ 3LL });
    STATIC_REQUIRE(std::is_same_v<decltype(t), const TTuple<Int32, Float32, Int64>>);
}

TEST_CASE("TTuple: CTAD stores values correctly when types are deduced", "[Container][Tuple]")
{
    const TTuple t(Int32{ 10 }, Float32{ 20.0f });
    REQUIRE(t.Get<0>() == 10);
    REQUIRE(t.Get<1>() == 20.0f);
}

TEST_CASE("TTuple: std::tuple_size reports correct element count", "[Container][Tuple]")
{
    STATIC_REQUIRE(std::tuple_size_v<TTuple<Int32, Float32, Int64>> == 3);
}

TEST_CASE("TTuple: std::tuple_size reports zero for empty tuple", "[Container][Tuple]")
{
    STATIC_REQUIRE(std::tuple_size_v<TTuple<>> == 0);
}

TEST_CASE("TTuple: std::tuple_element reports correct type at each index", "[Container][Tuple]")
{
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, TTuple<Int32, Float32, Int64>>, Int32>);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<1, TTuple<Int32, Float32, Int64>>, Float32>);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<2, TTuple<Int32, Float32, Int64>>, Int64>);
}

TEST_CASE("TTuple: Structured binding decomposes two-element tuple correctly", "[Container][Tuple]")
{
    const TTuple<Int32, Float32> t(5, 1.5f);
    const auto& [a, b] = t;
    REQUIRE(a == 5);
    REQUIRE(b == 1.5f);
}

TEST_CASE("TTuple: Structured binding decomposes three-element tuple correctly", "[Container][Tuple]")
{
    const TTuple<Int32, Int32, Int32> t(1, 2, 3);
    const auto& [x, y, z] = t;
    REQUIRE(x == 1);
    REQUIRE(y == 2);
    REQUIRE(z == 3);
}

TEST_CASE("TTuple: Structured binding allows mutation through mutable references", "[Container][Tuple]")
{
    TTuple<Int32, Int32> t(10, 20);
    auto& [a, b] = t;
    a = 99;
    b = 88;
    REQUIRE(t.Get<0>() == 99);
    REQUIRE(t.Get<1>() == 88);
}

TEST_CASE("TTuple: GP::get ADL accessor returns correct elements", "[Container][Tuple]")
{
    TTuple<Int32, Float32> t(11, 3.0f);
    REQUIRE(GP::Get<0>(t) == 11);
    REQUIRE(GP::Get<1>(t) == 3.0f);
}

TEST_CASE("TTuple: Default construction is noexcept for trivial types", "[Container][Tuple]")
{
    STATIC_REQUIRE(noexcept(TTuple<Int32, Float32>{}));
}

TEST_CASE("TTuple: Construction from values is noexcept for trivial types", "[Container][Tuple]")
{
    STATIC_REQUIRE(noexcept(TTuple<Int32, Int32>(Int32{}, Int32{})));
}

TEST_CASE("TTuple: Get<I> is noexcept on mutable tuple", "[Container][Tuple]")
{
    TTuple<Int32, Int32> t(1, 2);
    STATIC_REQUIRE(noexcept(t.Get<0>()));
}

TEST_CASE("TTuple: Get<I> is noexcept on const tuple", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> t(1, 2);
    STATIC_REQUIRE(noexcept(t.Get<0>()));
}

TEST_CASE("TTuple: GP::Get free function is noexcept on mutable tuple", "[Container][Tuple]")
{
    TTuple<Int32, Int32> t(1, 2);
    STATIC_REQUIRE(noexcept(GP::Get<0>(t)));
}

TEST_CASE("TTuple: GP::Get free function is noexcept on const tuple", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> t(1, 2);
    STATIC_REQUIRE(noexcept(GP::Get<0>(t)));
}

TEST_CASE("TTuple: operator== is noexcept for trivial types", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 2);
    const TTuple<Int32, Int32> b(1, 2);
    STATIC_REQUIRE(noexcept(a == b));
}

TEST_CASE("TTuple: operator!= is noexcept for trivial types", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 2);
    const TTuple<Int32, Int32> b(1, 2);
    STATIC_REQUIRE(noexcept(a != b));
}

TEST_CASE("TTuple: operator<=> is noexcept for trivial types", "[Container][Tuple]")
{
    const TTuple<Int32, Int32> a(1, 2);
    const TTuple<Int32, Int32> b(1, 2);
    STATIC_REQUIRE(noexcept(a <=> b));
}

TEST_CASE("TTuple: Size() is noexcept", "[Container][Tuple]") { STATIC_REQUIRE(noexcept(TTuple<Int32>::Size())); }

TEST_CASE("TTuple: Is trivially copy-constructible for trivial element types", "[Container][Tuple]")
{
    STATIC_REQUIRE(std::is_trivially_copy_constructible_v<TTuple<Int32, Float32>>);
}

TEST_CASE("TTuple: Is trivially move-constructible for trivial element types", "[Container][Tuple]")
{
    STATIC_REQUIRE(std::is_trivially_move_constructible_v<TTuple<Int32, Float32>>);
}

TEST_CASE("TTuple: Is trivially copy-assignable for trivial element types", "[Container][Tuple]")
{
    STATIC_REQUIRE(std::is_trivially_copy_assignable_v<TTuple<Int32, Float32>>);
}

TEST_CASE("TTuple: Is trivially move-assignable for trivial element types", "[Container][Tuple]")
{
    STATIC_REQUIRE(std::is_trivially_move_assignable_v<TTuple<Int32, Float32>>);
}

TEST_CASE("TTuple: Is trivially destructible for trivial element types", "[Container][Tuple]")
{
    STATIC_REQUIRE(std::is_trivially_destructible_v<TTuple<Int32, Float32>>);
}

TEST_CASE("TTuple: Destructor runs element destructors exactly once each", "[Container][Tuple]")
{
    Int32 countA = 0;
    Int32 countB = 0;
    {
        const TTuple<FDestroyTracker, FDestroyTracker> t(FDestroyTracker{ &countA }, FDestroyTracker{ &countB });
    }
    // Each element's destructor ran exactly once; nullifying move prevents double-counting from temporaries
    REQUIRE(countA == 1);
    REQUIRE(countB == 1);
}

TEST_CASE("TTuple: Modifying one element does not affect another element", "[Container][Tuple]")
{
    TTuple<Int32, Int32> t(1, 2);
    t.Get<0>() = 99;
    REQUIRE(t.Get<0>() == 99);
    REQUIRE(t.Get<1>() == 2);
}

TEST_CASE("TTuple: Each element occupies independent storage", "[Container][Tuple]")
{
    TTuple<Int32, Int32> t(0, 0);
    REQUIRE(&t.Get<0>() != &t.Get<1>());
}

TEST_CASE("TTuple: Five-element tuple stores all values correctly", "[Container][Tuple]")
{
    const TTuple<Int32, Int32, Int32, Int32, Int32> t(1, 2, 3, 4, 5);
    REQUIRE(t.Get<0>() == 1);
    REQUIRE(t.Get<1>() == 2);
    REQUIRE(t.Get<2>() == 3);
    REQUIRE(t.Get<3>() == 4);
    REQUIRE(t.Get<4>() == 5);
}

TEST_CASE("TTuple: Five-element tuple kSize equals five", "[Container][Tuple]")
{
    STATIC_REQUIRE(TTuple<Int32, Int32, Int32, Int32, Int32>::kSize == 5);
}
