// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/DefaultDeleter.hpp"
#include "CoreTypes.hpp"
#include <catch2/catch_test_macros.hpp>
#include <type_traits>

using namespace GP;

namespace
{

struct FTracked
{
    bool& m_destroyed;

    explicit FTracked(bool& outDestroyed)
        : m_destroyed(outDestroyed)
    {
        m_destroyed = false;
    }

    ~FTracked() { m_destroyed = true; }
};

struct FDerived : FTracked
{
    explicit FDerived(bool& outDestroyed)
        : FTracked(outDestroyed)
    {}
};

}   // namespace

TEST_CASE("TDefaultDeleter: Default construction is constexpr and noexcept", "[Memory][DefaultDeleter]")
{
    constexpr TDefaultDeleter<Int32> deleter{};
    STATIC_REQUIRE(noexcept(TDefaultDeleter<Int32>{}));
    (void)deleter;
}

TEST_CASE("TDefaultDeleter: operator() deletes scalar object", "[Memory][DefaultDeleter]")
{
    bool destroyed = false;
    auto* ptr = new FTracked(destroyed);
    REQUIRE_FALSE(destroyed);
    TDefaultDeleter<FTracked> deleter{};
    deleter(ptr);
    REQUIRE(destroyed);
}

TEST_CASE("TDefaultDeleter: operator() is noexcept", "[Memory][DefaultDeleter]")
{
    TDefaultDeleter<Int32> deleter{};
    Int32* ptr = new Int32(42);
    STATIC_REQUIRE(noexcept(deleter(ptr)));
    deleter(ptr);
}

TEST_CASE("TDefaultDeleter: Converting constructor from derived deleter", "[Memory][DefaultDeleter]")
{
    bool destroyed = false;
    auto* ptr = new FDerived(destroyed);
    TDefaultDeleter<FDerived> derivedDeleter{};
    TDefaultDeleter<FTracked> baseDeleter(derivedDeleter);
    baseDeleter(ptr);
    REQUIRE(destroyed);
}

TEST_CASE("TDefaultDeleter: Converting constructor is constexpr and noexcept", "[Memory][DefaultDeleter]")
{
    STATIC_REQUIRE(noexcept(TDefaultDeleter<FTracked>{ TDefaultDeleter<FDerived>{} }));
}

TEST_CASE("TDefaultDeleter: Stateless deleter has no size overhead", "[Memory][DefaultDeleter]")
{
    STATIC_REQUIRE(std::is_empty_v<TDefaultDeleter<Int32>>);
}

TEST_CASE("TDefaultDeleter<T[]>: Default construction is constexpr and noexcept", "[Memory][DefaultDeleter]")
{
    constexpr TDefaultDeleter<Int32[]> deleter{};
    STATIC_REQUIRE(noexcept(TDefaultDeleter<Int32[]>{}));
    (void)deleter;
}

TEST_CASE("TDefaultDeleter<T[]>: operator() deletes array", "[Memory][DefaultDeleter]")
{
    bool destroyed[2] = { false, false };
    auto* arr = new FTracked[2]{ FTracked(destroyed[0]), FTracked(destroyed[1]) };
    TDefaultDeleter<FTracked[]> deleter{};
    deleter(arr);
    REQUIRE(destroyed[0]);
    REQUIRE(destroyed[1]);
}

TEST_CASE("TDefaultDeleter<T[]>: operator() is noexcept", "[Memory][DefaultDeleter]")
{
    TDefaultDeleter<Int32[]> deleter{};
    Int32* ptr = new Int32[3]{ 1, 2, 3 };
    STATIC_REQUIRE(noexcept(deleter(ptr)));
    deleter(ptr);
}

TEST_CASE("TDefaultDeleter<T[]>: Stateless array deleter has no size overhead", "[Memory][DefaultDeleter]")
{
    STATIC_REQUIRE(std::is_empty_v<TDefaultDeleter<Int32[]>>);
}

TEST_CASE("TNullDeleter: operator() is a no-op — pointer remains valid after call", "[Memory][DefaultDeleter]")
{
    Int32 value = 99;
    Int32* ptr = &value;
    TNullDeleter<Int32> deleter{};
    deleter(ptr);
    REQUIRE(*ptr == 99);
}

TEST_CASE("TNullDeleter: Stateless no-op deleter has no size overhead", "[Memory][DefaultDeleter]")
{
    STATIC_REQUIRE(std::is_empty_v<TNullDeleter<Int32>>);
}
