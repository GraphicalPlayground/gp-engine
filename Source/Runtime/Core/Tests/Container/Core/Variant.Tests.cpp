// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Core/Variant.hpp"
#include "CoreTypes.hpp"
#include <catch2/catch_test_macros.hpp>
#include <compare>
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

    FDestroyTracker(const FDestroyTracker& other) = default;
    FDestroyTracker(FDestroyTracker&& other) noexcept = default;
    FDestroyTracker& operator=(const FDestroyTracker&) = default;
    FDestroyTracker& operator=(FDestroyTracker&&) noexcept = default;

    ~FDestroyTracker() { ++(*m_count); }

    bool operator==(const FDestroyTracker& other) const noexcept { return m_count == other.m_count; }
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

    auto operator<=>(const FMoveSpy& other) const noexcept { return value <=> other.value; }
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

TEST_CASE("TMonostate: Two TMonostate values are equal", "[Container][Variant]")
{
    STATIC_REQUIRE(TMonostate{} == TMonostate{});
}

TEST_CASE("TMonostate: Three-way comparison of TMonostate is equal", "[Container][Variant]")
{
    STATIC_REQUIRE(std::is_eq(TMonostate{} <=> TMonostate{}));
}

TEST_CASE("TVariant: Default construction holds the first alternative (index 0)", "[Container][Variant]")
{
    TVariant<Int32, Float32> v;
    REQUIRE(v.Index() == 0);
    REQUIRE_FALSE(v.IsValueless());
    REQUIRE(Get<0>(v) == Int32{ 0 });
}

TEST_CASE("TVariant: Default construction with TMonostate as first alternative", "[Container][Variant]")
{
    TVariant<TMonostate, Int32> v;
    REQUIRE(v.Index() == 0);
    REQUIRE(v.HoldsAlternative<TMonostate>());
}

TEST_CASE("TVariant: Converting construction from Int32 chooses the Int32 alternative", "[Container][Variant]")
{
    TVariant<Int32, Float32, bool> v(Int32{ 42 });
    REQUIRE(v.Index() == 0);
    REQUIRE(v.HoldsAlternative<Int32>());
    REQUIRE(Get<Int32>(v) == 42);
}

TEST_CASE("TVariant: Converting construction from Float32 chooses the Float32 alternative", "[Container][Variant]")
{
    TVariant<Int32, Float32, bool> v(Float32{ 1.5f });
    REQUIRE(v.Index() == 1);
    REQUIRE(v.HoldsAlternative<Float32>());
    REQUIRE(Get<Float32>(v) == 1.5f);
}

TEST_CASE("TVariant: In-place-by-type constructor builds FMultiArg", "[Container][Variant]")
{
    TVariant<Int32, FMultiArg> v(InPlaceType<FMultiArg>, Int32{ 1 }, Int32{ 2 }, Float32{ 3.0f });
    REQUIRE(v.Index() == 1);
    REQUIRE(v.HoldsAlternative<FMultiArg>());
    const auto& m = Get<FMultiArg>(v);
    REQUIRE(m.x == 1);
    REQUIRE(m.y == 2);
    REQUIRE(m.z == 3.0f);
}

TEST_CASE("TVariant: In-place-by-index constructor builds the alternative at index 2", "[Container][Variant]")
{
    TVariant<Int32, Float32, FMoveSpy> v(InPlaceIndex<2>, Int32{ 7 });
    REQUIRE(v.Index() == 2);
    REQUIRE(Get<2>(v).value == 7);
}

TEST_CASE("TVariant: In-place-by-index constructor for index 0", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(InPlaceIndex<0>, Int32{ 99 });
    REQUIRE(v.Index() == 0);
    REQUIRE(Get<0>(v) == 99);
}

TEST_CASE("TVariant: Copy construction replicates the active alternative", "[Container][Variant]")
{
    TVariant<Int32, Float32> src(Float32{ 2.5f });
    TVariant<Int32, Float32> dst(src);
    REQUIRE(dst.Index() == src.Index());
    REQUIRE(Get<Float32>(dst) == 2.5f);
}

TEST_CASE("TVariant: Copy construction from variant holding index 0", "[Container][Variant]")
{
    TVariant<Int32, Float32> src(Int32{ 10 });
    TVariant<Int32, Float32> dst(src);
    REQUIRE(dst.HoldsAlternative<Int32>());
    REQUIRE(Get<Int32>(dst) == 10);
}

TEST_CASE("TVariant: Copy construction does not alias the source", "[Container][Variant]")
{
    TVariant<Int32, Float32> src(Int32{ 5 });
    TVariant<Int32, Float32> dst(src);
    Get<Int32>(dst) = 99;
    REQUIRE(Get<Int32>(src) == 5);
}

TEST_CASE("TVariant: Move construction transfers the active alternative", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> src(InPlaceType<FMoveSpy>, Int32{ 42 });
    TVariant<FMoveSpy, Int32> dst(GP::Move(src));
    REQUIRE(dst.HoldsAlternative<FMoveSpy>());
    REQUIRE(Get<FMoveSpy>(dst).value == 42);
    REQUIRE(src.IsValueless());
}

TEST_CASE("TVariant: Move construction sets spy's moved flag on source", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> src(InPlaceType<FMoveSpy>, Int32{ 7 });
    REQUIRE_FALSE(Get<FMoveSpy>(src).moved);

    TVariant<FMoveSpy, Int32> dst(GP::Move(src));
    REQUIRE(Get<FMoveSpy>(dst).value == 7);
    REQUIRE(src.IsValueless());
}

TEST_CASE("TVariant: Destructor calls the active alternative's destructor", "[Container][Variant]")
{
    Int32 count = 0;
    {
        TVariant<FDestroyTracker, Int32> v(InPlaceType<FDestroyTracker>, count);
    }
    REQUIRE(count == 1);
}

TEST_CASE("TVariant: Destructor is only called for the active alternative", "[Container][Variant]")
{
    Int32 countA = 0;
    Int32 countB = 0;
    {
        TVariant<FDestroyTracker, FDestroyTracker> v(InPlaceIndex<0>, countA);
        GP_UNUSED(countB);   // countB's tracker is never stored
    }
    REQUIRE(countA == 1);
    REQUIRE(countB == 0);
}

TEST_CASE("TVariant: Copy assignment between same active index uses copy-assign", "[Container][Variant]")
{
    TVariant<Int32, Float32> a(Int32{ 1 });
    TVariant<Int32, Float32> b(Int32{ 2 });
    a = b;
    REQUIRE(a.Index() == 0);
    REQUIRE(Get<Int32>(a) == 2);
}

TEST_CASE("TVariant: Copy assignment between different active indices replaces alternative", "[Container][Variant]")
{
    TVariant<Int32, Float32> a(Int32{ 10 });
    TVariant<Int32, Float32> b(Float32{ 3.0f });
    a = b;
    REQUIRE(a.HoldsAlternative<Float32>());
    REQUIRE(Get<Float32>(a) == 3.0f);
}

TEST_CASE("TVariant: Copy assignment to self is a no-op", "[Container][Variant]")
{
    TVariant<Int32, Float32> a(Int32{ 55 });
    TVariant<Int32, Float32>& ref = a;
    a = ref;
    REQUIRE(Get<Int32>(a) == 55);
}

TEST_CASE("TVariant: Move assignment between same active index uses move-assign", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> a(InPlaceType<FMoveSpy>, Int32{ 1 });
    TVariant<FMoveSpy, Int32> b(InPlaceType<FMoveSpy>, Int32{ 2 });
    a = GP::Move(b);
    REQUIRE(a.HoldsAlternative<FMoveSpy>());
    REQUIRE(Get<FMoveSpy>(a).value == 2);
    REQUIRE(b.IsValueless());
}

TEST_CASE("TVariant: Move assignment between different active indices replaces alternative", "[Container][Variant]")
{
    TVariant<Int32, Float32> a(Int32{ 10 });
    TVariant<Int32, Float32> b(Float32{ 5.0f });
    a = GP::Move(b);
    REQUIRE(a.HoldsAlternative<Float32>());
    REQUIRE(Get<Float32>(a) == 5.0f);
    REQUIRE(b.IsValueless());
}

TEST_CASE("TVariant: Converting assignment replaces with the matching alternative", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Int32{ 0 });
    v = Float32{ 9.9f };
    REQUIRE(v.HoldsAlternative<Float32>());
    REQUIRE(Get<Float32>(v) == 9.9f);
}

TEST_CASE("TVariant: Converting assignment to same active type uses assignment operator", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> v(InPlaceType<FMoveSpy>, Int32{ 1 });
    FMoveSpy spy(99);
    v = spy;
    REQUIRE(v.HoldsAlternative<FMoveSpy>());
    REQUIRE(Get<FMoveSpy>(v).value == 99);
}

TEST_CASE("TVariant: Equality returns true when both hold the same value", "[Container][Variant]")
{
    TVariant<Int32, Float32> a(Int32{ 5 });
    TVariant<Int32, Float32> b(Int32{ 5 });
    REQUIRE(a == b);
}

TEST_CASE("TVariant: Equality returns false when active indices differ", "[Container][Variant]")
{
    TVariant<Int32, Float32> a(Int32{ 5 });
    TVariant<Int32, Float32> b(Float32{ 5.0f });
    REQUIRE(a != b);
}

TEST_CASE("TVariant: Equality returns false when values differ (same index)", "[Container][Variant]")
{
    TVariant<Int32, Float32> a(Int32{ 1 });
    TVariant<Int32, Float32> b(Int32{ 2 });
    REQUIRE(a != b);
}

TEST_CASE("TVariant: Spaceship returns equal for identical variants", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> a(InPlaceType<FMoveSpy>, Int32{ 3 });
    TVariant<FMoveSpy, Int32> b(InPlaceType<FMoveSpy>, Int32{ 3 });
    REQUIRE(std::is_eq(a <=> b));
}

TEST_CASE("TVariant: Spaceship compares by value when same index", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> a(InPlaceType<FMoveSpy>, Int32{ 1 });
    TVariant<FMoveSpy, Int32> b(InPlaceType<FMoveSpy>, Int32{ 2 });
    REQUIRE(std::is_lt(a <=> b));
    REQUIRE(std::is_gt(b <=> a));
}

TEST_CASE("TVariant: Spaceship compares by index when alternatives differ", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> a(InPlaceType<FMoveSpy>, Int32{ 99 });
    TVariant<FMoveSpy, Int32> b(Int32{ 0 });
    // a holds index 0, b holds index 1: a < b
    REQUIRE(std::is_lt(a <=> b));
}

TEST_CASE("TVariant: IsValueless returns false for a normally constructed variant", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Int32{ 0 });
    REQUIRE_FALSE(v.IsValueless());
}

TEST_CASE("TVariant: Index returns the correct zero-based index", "[Container][Variant]")
{
    TVariant<Int32, Float32, FMoveSpy> v(InPlaceType<FMoveSpy>, Int32{ 0 });
    REQUIRE(v.Index() == 2);
}

TEST_CASE("TVariant: HoldsAlternative<I> returns true for active index", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Float32{ 1.0f });
    REQUIRE(v.HoldsAlternative<1>());
    REQUIRE_FALSE(v.HoldsAlternative<0>());
}

TEST_CASE("TVariant: HoldsAlternative<T> returns true for active type", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Float32{ 1.0f });
    REQUIRE(v.HoldsAlternative<Float32>());
    REQUIRE_FALSE(v.HoldsAlternative<Int32>());
}

TEST_CASE("TVariant: Free function HoldsAlternative<T> matches member version", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Int32{ 3 });
    REQUIRE(HoldsAlternative<Int32>(v));
    REQUIRE_FALSE(HoldsAlternative<Float32>(v));
}

TEST_CASE("TVariant: Emplace<I> replaces the active alternative and constructs in-place", "[Container][Variant]")
{
    TVariant<Int32, FMultiArg> v(Int32{ 0 });
    auto& ref = v.Emplace<1>(Int32{ 10 }, Int32{ 20 }, Float32{ 30.0f });
    REQUIRE(v.Index() == 1);
    REQUIRE(ref.x == 10);
    REQUIRE(ref.y == 20);
    REQUIRE(ref.z == 30.0f);
}

TEST_CASE("TVariant: Emplace<T> constructs by type", "[Container][Variant]")
{
    TVariant<Int32, FMultiArg> v(Int32{ 0 });
    v.Emplace<FMultiArg>(Int32{ 7 }, Int32{ 8 }, Float32{ 9.0f });
    REQUIRE(v.HoldsAlternative<FMultiArg>());
    REQUIRE(Get<FMultiArg>(v).x == 7);
}

TEST_CASE("TVariant: Emplace destroys the previous alternative's value", "[Container][Variant]")
{
    Int32 count = 0;
    TVariant<FDestroyTracker, Int32> v(InPlaceType<FDestroyTracker>, count);
    count = 0;

    v.Emplace<1>(Int32{ 42 });
    REQUIRE(count == 1);
    REQUIRE(v.HoldsAlternative<Int32>());
}

TEST_CASE("TVariant: Get<I> returns mutable reference to active alternative", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Int32{ 5 });
    Get<0>(v) = 10;
    REQUIRE(Get<0>(v) == 10);
}

TEST_CASE("TVariant: Get<I> on const variant returns const reference", "[Container][Variant]")
{
    const TVariant<Int32, Float32> v(Int32{ 7 });
    const Int32& ref = Get<0>(v);
    REQUIRE(ref == 7);
}

TEST_CASE("TVariant: Get<I> on rvalue variant moves the value out", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> v(InPlaceType<FMoveSpy>, Int32{ 3 });
    FMoveSpy spy = Get<0>(GP::Move(v));
    REQUIRE(spy.value == 3);
}

TEST_CASE("TVariant: Get<T> returns mutable reference to the active type", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Float32{ 2.0f });
    Get<Float32>(v) = 9.9f;
    REQUIRE(Get<Float32>(v) == 9.9f);
}

TEST_CASE("TVariant: Get<T> on const variant returns const reference", "[Container][Variant]")
{
    const TVariant<Int32, Float32> v(Float32{ 6.0f });
    const Float32& ref = Get<Float32>(v);
    REQUIRE(ref == 6.0f);
}

TEST_CASE("TVariant: TryGet<I> returns pointer when index matches", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Int32{ 4 });
    Int32* p = TryGet<0>(&v);
    REQUIRE(p != nullptr);
    REQUIRE(*p == 4);
}

TEST_CASE("TVariant: TryGet<I> returns nullptr when index does not match", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Int32{ 4 });
    Float32* p = TryGet<1>(&v);
    REQUIRE(p == nullptr);
}

TEST_CASE("TVariant: TryGet<I> returns nullptr for null pointer", "[Container][Variant]")
{
    Int32* p = TryGet<0>(static_cast<TVariant<Int32, Float32>*>(nullptr));
    REQUIRE(p == nullptr);
}

TEST_CASE("TVariant: TryGet<I> const overload returns const pointer on match", "[Container][Variant]")
{
    const TVariant<Int32, Float32> v(Int32{ 8 });
    const Int32* p = TryGet<0>(&v);
    REQUIRE(p != nullptr);
    REQUIRE(*p == 8);
}

TEST_CASE("TVariant: TryGet<T> returns pointer when type matches", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Float32{ 5.0f });
    Float32* p = TryGet<Float32>(&v);
    REQUIRE(p != nullptr);
    REQUIRE(*p == 5.0f);
}

TEST_CASE("TVariant: TryGet<T> returns nullptr when type does not match", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Float32{ 5.0f });
    Int32* p = TryGet<Int32>(&v);
    REQUIRE(p == nullptr);
}

TEST_CASE("TVariant: Visit invokes the callable with the active Int32 value", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Int32{ 11 });
    Int32 result = 0;
    Visit(
        [&result](auto& value)
        {
            using T = std::remove_reference_t<decltype(value)>;
            if constexpr (std::is_same_v<T, Int32>) { result = value; }
        },
        v
    );
    REQUIRE(result == 11);
}

TEST_CASE("TVariant: Visit invokes the callable with the active Float32 value", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Float32{ 3.14f });
    Float32 result = 0.0f;
    Visit(
        [&result](auto& value)
        {
            using T = std::remove_reference_t<decltype(value)>;
            if constexpr (std::is_same_v<T, Float32>) { result = value; }
        },
        v
    );
    REQUIRE(result == 3.14f);
}

TEST_CASE("TVariant: Visit on const variant passes const reference to visitor", "[Container][Variant]")
{
    const TVariant<Int32, Float32> v(Int32{ 77 });
    bool wasConst = false;
    Visit(
        [&wasConst](const auto& value)
        {
            using T = std::remove_cv_t<std::remove_reference_t<decltype(value)>>;
            if constexpr (std::is_same_v<T, Int32>) { wasConst = true; }
        },
        v
    );
    REQUIRE(wasConst);
}

TEST_CASE("TVariant: Visit returns the visitor's return value", "[Container][Variant]")
{
    TVariant<Int32, Float32> v(Int32{ 50 });
    Int32 result = Visit([](auto& val) -> Int32 { return static_cast<Int32>(val); }, v);
    REQUIRE(result == 50);
}

TEST_CASE("TVariant: Visit on rvalue variant allows moving the value out", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> v(InPlaceType<FMoveSpy>, Int32{ 9 });
    FMoveSpy extracted(0);
    Visit(
        [&extracted](auto&& val)
        {
            using T = std::remove_reference_t<decltype(val)>;
            if constexpr (std::is_same_v<T, FMoveSpy>) { extracted = GP::Move(val); }
        },
        GP::Move(v)
    );
    REQUIRE(extracted.value == 9);
}

TEST_CASE("TVariant: Swap exchanges same-type alternatives", "[Container][Variant]")
{
    TVariant<Int32, Float32> a(Int32{ 1 });
    TVariant<Int32, Float32> b(Int32{ 2 });
    a.Swap(b);
    REQUIRE(Get<Int32>(a) == 2);
    REQUIRE(Get<Int32>(b) == 1);
}

TEST_CASE("TVariant: Swap exchanges different-type alternatives", "[Container][Variant]")
{
    TVariant<Int32, Float32> a(Int32{ 3 });
    TVariant<Int32, Float32> b(Float32{ 4.0f });
    a.Swap(b);
    REQUIRE(a.HoldsAlternative<Float32>());
    REQUIRE(Get<Float32>(a) == 4.0f);
    REQUIRE(b.HoldsAlternative<Int32>());
    REQUIRE(Get<Int32>(b) == 3);
}

TEST_CASE("TVariant: kSize reports the correct number of alternatives", "[Container][Variant]")
{
    STATIC_REQUIRE(TVariant<Int32, Float32, bool>::kSize == 3);
    STATIC_REQUIRE(TVariant<Int32>::kSize == 1);
    STATIC_REQUIRE(TVariant<TMonostate, Int32, Float32>::kSize == 3);
}

TEST_CASE("TVariant: Move-construction leaves source in valueless state", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> src(InPlaceType<FMoveSpy>, Int32{ 1 });
    TVariant<FMoveSpy, Int32> dst(GP::Move(src));
    REQUIRE(src.IsValueless());
    REQUIRE(src.Index() == TVariant<FMoveSpy, Int32>::kInvalidIndex);
}

TEST_CASE("TVariant: Move-assignment leaves source in valueless state", "[Container][Variant]")
{
    TVariant<FMoveSpy, Int32> src(InPlaceType<FMoveSpy>, Int32{ 2 });
    TVariant<FMoveSpy, Int32> dst(Int32{ 0 });
    dst = GP::Move(src);
    REQUIRE(src.IsValueless());
}
