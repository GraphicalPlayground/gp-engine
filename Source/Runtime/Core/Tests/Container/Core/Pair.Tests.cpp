// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Core/Pair.hpp"
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

}   // namespace

TEST_CASE("TPair: Default construction value-initializes both elements", "[Container][Pair]")
{
    const TPair<Int32, Float32> p;
    REQUIRE(p.first == 0);
    REQUIRE(p.second == 0.0f);
}

TEST_CASE("TPair: Default construction is constexpr", "[Container][Pair]")
{
    constexpr TPair<Int32, Int32> p;
    STATIC_REQUIRE(p.first == 0);
    STATIC_REQUIRE(p.second == 0);
}

TEST_CASE("TPair: Copy-construction from lvalue values stores both elements", "[Container][Pair]")
{
    const Int32 a = 1;
    const Float32 b = 2.5f;
    const TPair<Int32, Float32> p(a, b);
    REQUIRE(p.first == 1);
    REQUIRE(p.second == 2.5f);
}

TEST_CASE("TPair: Move-construction from rvalue values moves both elements", "[Container][Pair]")
{
    FMoveSpy a(10);
    FMoveSpy b(20);
    const TPair<FMoveSpy, FMoveSpy> p(GP::Move(a), GP::Move(b));
    REQUIRE(p.first.value == 10);
    REQUIRE(p.second.value == 20);
    REQUIRE(a.moved);
    REQUIRE(b.moved);
}

TEST_CASE("TPair: Heterogeneous construction converts types widening Int32 to Int64", "[Container][Pair]")
{
    const TPair<Int64, Float64> p(5, 3.0f);
    REQUIRE(p.first == 5LL);
    REQUIRE(p.second == static_cast<Float64>(3.0f));
}

TEST_CASE("TPair: Copy-construction from another TPair copies both elements", "[Container][Pair]")
{
    const TPair<Int32, Int32> src(3, 7);
    const TPair<Int32, Int32> dst(src);
    REQUIRE(dst.first == 3);
    REQUIRE(dst.second == 7);
}

TEST_CASE("TPair: Copy-construction does not modify the source", "[Container][Pair]")
{
    const TPair<Int32, Int32> src(3, 7);
    const TPair<Int32, Int32> dst(src);
    REQUIRE(src.first == 3);
    REQUIRE(src.second == 7);
}

TEST_CASE("TPair: Move-construction transfers both elements from the source", "[Container][Pair]")
{
    TPair<FMoveSpy, FMoveSpy> src(FMoveSpy(1), FMoveSpy(2));
    const TPair<FMoveSpy, FMoveSpy> dst(GP::Move(src));
    REQUIRE(dst.first.value == 1);
    REQUIRE(dst.second.value == 2);
    REQUIRE(src.first.moved);
    REQUIRE(src.second.moved);
}

TEST_CASE("TPair: Copy-assignment overwrites both elements", "[Container][Pair]")
{
    TPair<Int32, Int32> dst(0, 0);
    const TPair<Int32, Int32> src(4, 8);
    dst = src;
    REQUIRE(dst.first == 4);
    REQUIRE(dst.second == 8);
}

TEST_CASE("TPair: Copy-assignment does not modify the source", "[Container][Pair]")
{
    TPair<Int32, Int32> dst(0, 0);
    const TPair<Int32, Int32> src(4, 8);
    dst = src;
    REQUIRE(src.first == 4);
    REQUIRE(src.second == 8);
}

TEST_CASE("TPair: Move-assignment transfers both elements and marks source as moved", "[Container][Pair]")
{
    TPair<FMoveSpy, FMoveSpy> dst(FMoveSpy(0), FMoveSpy(0));
    TPair<FMoveSpy, FMoveSpy> src(FMoveSpy(5), FMoveSpy(6));
    dst = GP::Move(src);
    REQUIRE(dst.first.value == 5);
    REQUIRE(dst.second.value == 6);
    REQUIRE(src.first.moved);
    REQUIRE(src.second.moved);
}

TEST_CASE("TPair: operator== returns true when both elements are equal", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(1, 2);
    const TPair<Int32, Int32> b(1, 2);
    REQUIRE(a == b);
}

TEST_CASE("TPair: operator== returns false when first elements differ", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(1, 2);
    const TPair<Int32, Int32> b(9, 2);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("TPair: operator== returns false when second elements differ", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(1, 2);
    const TPair<Int32, Int32> b(1, 9);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("TPair: operator!= returns true when elements differ", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(1, 2);
    const TPair<Int32, Int32> b(3, 4);
    REQUIRE(a != b);
}

TEST_CASE("TPair: operator!= returns false when elements are equal", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(5, 6);
    const TPair<Int32, Int32> b(5, 6);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("TPair: operator<=> returns less-than when first element is smaller", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(1, 5);
    const TPair<Int32, Int32> b(2, 3);
    REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TPair: operator<=> returns less-than when first equal and second is smaller", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(1, 2);
    const TPair<Int32, Int32> b(1, 5);
    REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TPair: operator<=> returns equal when both elements are equal", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(3, 4);
    const TPair<Int32, Int32> b(3, 4);
    REQUIRE(std::is_eq(a <=> b));
}

TEST_CASE("TPair: operator<=> returns greater-than when first element is larger", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(5, 0);
    const TPair<Int32, Int32> b(2, 9);
    REQUIRE(std::is_gt(a <=> b));
}

TEST_CASE("TPair: operator<=> returns greater-than when first equal and second is larger", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(3, 9);
    const TPair<Int32, Int32> b(3, 4);
    REQUIRE(std::is_gt(a <=> b));
}

TEST_CASE("TPair: operator<=> uses first element exclusively to determine order when it differs", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(1, 100);
    const TPair<Int32, Int32> b(2, 0);
    REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TPair: operator<=> compiles and works for mixed comparison categories", "[Container][Pair]")
{
    const TPair<Float32, Int32> a(1.0f, 2);
    const TPair<Float32, Int32> b(1.0f, 3);
    REQUIRE(std::is_lt(a <=> b));
    STATIC_REQUIRE(std::is_same_v<
                   decltype(a <=> b),
                   std::common_comparison_category_t<std::partial_ordering, std::strong_ordering>>);
}

TEST_CASE("TPair: operator<=> is constexpr", "[Container][Pair]")
{
    constexpr TPair<Int32, Int32> a(1, 2);
    constexpr TPair<Int32, Int32> b(1, 3);
    STATIC_REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TPair: Swap exchanges both elements", "[Container][Pair]")
{
    TPair<Int32, Int32> a(1, 2);
    TPair<Int32, Int32> b(3, 4);
    a.Swap(b);
    REQUIRE(a.first == 3);
    REQUIRE(a.second == 4);
    REQUIRE(b.first == 1);
    REQUIRE(b.second == 2);
}

TEST_CASE("TPair: Swap with self leaves the pair unchanged", "[Container][Pair]")
{
    TPair<Int32, Int32> a(7, 8);
    a.Swap(a);
    REQUIRE(a.first == 7);
    REQUIRE(a.second == 8);
}

TEST_CASE("TPair: Swap works for non-trivial types", "[Container][Pair]")
{
    TPair<FMoveSpy, FMoveSpy> a(FMoveSpy(10), FMoveSpy(20));
    TPair<FMoveSpy, FMoveSpy> b(FMoveSpy(30), FMoveSpy(40));
    a.Swap(b);
    REQUIRE(a.first.value == 30);
    REQUIRE(a.second.value == 40);
    REQUIRE(b.first.value == 10);
    REQUIRE(b.second.value == 20);
}

TEST_CASE("TPair: MakePair deduces correct types and stores values", "[Container][Pair]")
{
    auto p = MakePair(Int32{ 1 }, Float32{ 2.5f });
    STATIC_REQUIRE(std::is_same_v<decltype(MakePair(Int32{}, Float32{})), TPair<Int32, Float32>>);
    REQUIRE(p.first == Int32{ 1 });
    REQUIRE(p.second == Float32{ 2.5f });
}

TEST_CASE("TPair: MakePair stores pointer first element correctly", "[Container][Pair]")
{
    const char* str = "hello";
    auto p = MakePair(str, Int32{ 42 });
    REQUIRE(p.first == str);
    REQUIRE(p.second == 42);
}

TEST_CASE("TPair: MakePair is constexpr", "[Container][Pair]")
{
    constexpr TPair<Int32, Int32> p = MakePair(Int32{ 5 }, Int32{ 10 });
    REQUIRE(p.first == 5);
    REQUIRE(p.second == 10);
}

TEST_CASE("TPair: CTAD deduces element types from constructor arguments", "[Container][Pair]")
{
    const TPair p(Int32{ 3 }, Float32{ 4.0f });
    STATIC_REQUIRE(std::is_same_v<decltype(p), const TPair<Int32, Float32>>);
    REQUIRE(p.first == 3);
    REQUIRE(p.second == 4.0f);
}

TEST_CASE("TPair: FirstType and SecondType aliases match the template parameters", "[Container][Pair]")
{
    STATIC_REQUIRE(std::is_same_v<TPair<Int32, Float32>::FirstType, Int32>);
    STATIC_REQUIRE(std::is_same_v<TPair<Int32, Float32>::SecondType, Float32>);
}

TEST_CASE("TPair: ThisType alias matches the instantiated type", "[Container][Pair]")
{
    STATIC_REQUIRE(std::is_same_v<TPair<Int32, Float32>::ThisType, TPair<Int32, Float32>>);
}

TEST_CASE("TPair: Public members are directly readable and writable", "[Container][Pair]")
{
    TPair<Int32, Int32> p(10, 20);
    p.first = 100;
    p.second = 200;
    REQUIRE(p.first == 100);
    REQUIRE(p.second == 200);
}

TEST_CASE("TPair: constexpr construction and equality comparison", "[Container][Pair]")
{
    constexpr TPair<Int32, Int32> a(1, 2);
    constexpr TPair<Int32, Int32> b(1, 2);
    STATIC_REQUIRE(a == b);
}

TEST_CASE("TPair: constexpr inequality comparison", "[Container][Pair]")
{
    constexpr TPair<Int32, Int32> a(1, 2);
    constexpr TPair<Int32, Int32> b(3, 4);
    STATIC_REQUIRE(a != b);
}

TEST_CASE("TPair: Copy-construction from lvalue TPair is noexcept for trivial types", "[Container][Pair]")
{
    STATIC_REQUIRE(std::is_nothrow_copy_constructible_v<TPair<Int32, Int32>>);
}

TEST_CASE("TPair: Move-construction is noexcept for trivial types", "[Container][Pair]")
{
    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<TPair<Int32, Int32>>);
}

TEST_CASE("TPair: operator== is symmetric", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(1, 2);
    const TPair<Int32, Int32> b(1, 2);
    REQUIRE(a == b);
    REQUIRE(b == a);
}

TEST_CASE("TPair: operator!= is symmetric", "[Container][Pair]")
{
    const TPair<Int32, Int32> a(1, 2);
    const TPair<Int32, Int32> b(3, 4);
    REQUIRE(a != b);
    REQUIRE(b != a);
}
