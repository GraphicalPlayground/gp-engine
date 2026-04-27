// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/UniquePtr.hpp"
#include <catch2/catch_test_macros.hpp>
#include <utility>

namespace gp::tests
{

namespace
{

/// @brief Tracks construction and destruction for lifetime verification.
struct LifetimeProbe
{
    static inline gp::Int32 s_aliveCount = 0;
    gp::Int32 value{ 0 };

    LifetimeProbe() noexcept { ++s_aliveCount; }

    explicit LifetimeProbe(gp::Int32 v) noexcept
        : value(v)
    {
        ++s_aliveCount;
    }

    LifetimeProbe(const LifetimeProbe& other) noexcept
        : value(other.value)
    {
        ++s_aliveCount;
    }

    LifetimeProbe(LifetimeProbe&& other) noexcept
        : value(other.value)
    {
        ++s_aliveCount;
    }

    ~LifetimeProbe() noexcept { --s_aliveCount; }

    LifetimeProbe& operator=(const LifetimeProbe&) noexcept = default;
    LifetimeProbe& operator=(LifetimeProbe&&) noexcept = default;
};

struct Base
{
    virtual ~Base() noexcept = default;

    virtual gp::Int32 tag() const noexcept { return 1; }
};

struct Derived : Base
{
    gp::Int32 tag() const noexcept override { return 2; }
};

}   // namespace

TEST_CASE("UniquePtr - Size guarantee", "[memory][UniquePtr]")
{
    // The core contract: zero overhead with stateless deleter.
    STATIC_REQUIRE(sizeof(gp::UniquePtr<int>) == sizeof(int*));
    STATIC_REQUIRE(sizeof(gp::UniquePtr<LifetimeProbe>) == sizeof(void*));
}

TEST_CASE("UniquePtr - Default construction is empty", "[memory][UniquePtr]")
{
    gp::UniquePtr<int> p;
    REQUIRE_FALSE(static_cast<bool>(p));
    REQUIRE(p.get() == nullptr);
    REQUIRE(p == nullptr);
}

TEST_CASE("UniquePtr - Nullptr construction", "[memory][UniquePtr]")
{
    gp::UniquePtr<int> p{ nullptr };
    REQUIRE(p.get() == nullptr);
    REQUIRE(nullptr == p);
}

TEST_CASE("UniquePtr - makeUnique constructs with arguments", "[memory][UniquePtr]")
{
    LifetimeProbe::s_aliveCount = 0;
    {
        auto p = gp::makeUnique<LifetimeProbe>(42);
        REQUIRE(p);
        REQUIRE(p->value == 42);
        REQUIRE((*p).value == 42);
        REQUIRE(LifetimeProbe::s_aliveCount == 1);
    }
    REQUIRE(LifetimeProbe::s_aliveCount == 0);
}

TEST_CASE("UniquePtr - Destructor destroys the object", "[memory][UniquePtr]")
{
    LifetimeProbe::s_aliveCount = 0;
    {
        auto p = gp::makeUnique<LifetimeProbe>();
        REQUIRE(LifetimeProbe::s_aliveCount == 1);
    }
    REQUIRE(LifetimeProbe::s_aliveCount == 0);
}

TEST_CASE("UniquePtr - Move construction transfers ownership", "[memory][UniquePtr]")
{
    LifetimeProbe::s_aliveCount = 0;
    auto a = gp::makeUnique<LifetimeProbe>(7);
    REQUIRE(LifetimeProbe::s_aliveCount == 1);
    const auto* raw = a.get();

    gp::UniquePtr<LifetimeProbe> b(std::move(a));
    REQUIRE(a.get() == nullptr);
    REQUIRE(b.get() == raw);
    REQUIRE(b->value == 7);
    REQUIRE(LifetimeProbe::s_aliveCount == 1);
}

TEST_CASE("UniquePtr - Move assignment destroys previous owner", "[memory][UniquePtr]")
{
    LifetimeProbe::s_aliveCount = 0;
    auto a = gp::makeUnique<LifetimeProbe>(1);
    auto b = gp::makeUnique<LifetimeProbe>(2);
    REQUIRE(LifetimeProbe::s_aliveCount == 2);

    b = std::move(a);
    REQUIRE(LifetimeProbe::s_aliveCount == 1);
    REQUIRE(b->value == 1);
    REQUIRE(a.get() == nullptr);
}

TEST_CASE("UniquePtr - release relinquishes ownership without destruction", "[memory][UniquePtr]")
{
    LifetimeProbe::s_aliveCount = 0;
    auto p = gp::makeUnique<LifetimeProbe>(99);
    LifetimeProbe* raw = p.release();
    REQUIRE(p.get() == nullptr);
    REQUIRE(raw != nullptr);
    REQUIRE(LifetimeProbe::s_aliveCount == 1);

    // Manually re-adopt and destroy via reset to keep the allocator account balanced.
    gp::UniquePtr<LifetimeProbe> rewrapped(raw);
    rewrapped.reset();
    REQUIRE(LifetimeProbe::s_aliveCount == 0);
}

TEST_CASE("UniquePtr - reset replaces the owned pointer", "[memory][UniquePtr]")
{
    LifetimeProbe::s_aliveCount = 0;
    auto p = gp::makeUnique<LifetimeProbe>(1);
    REQUIRE(LifetimeProbe::s_aliveCount == 1);

    p.reset();
    REQUIRE_FALSE(p);
    REQUIRE(LifetimeProbe::s_aliveCount == 0);

    auto q = gp::makeUnique<LifetimeProbe>(2);
    REQUIRE(LifetimeProbe::s_aliveCount == 1);
    q.reset(nullptr);
    REQUIRE(LifetimeProbe::s_aliveCount == 0);
}

TEST_CASE("UniquePtr - swap exchanges two pointers", "[memory][UniquePtr]")
{
    auto a = gp::makeUnique<LifetimeProbe>(1);
    auto b = gp::makeUnique<LifetimeProbe>(2);
    const auto* rawA = a.get();
    const auto* rawB = b.get();

    a.swap(b);
    REQUIRE(a.get() == rawB);
    REQUIRE(b.get() == rawA);
    REQUIRE(a->value == 2);
    REQUIRE(b->value == 1);

    gp::swap(a, b);
    REQUIRE(a.get() == rawA);
    REQUIRE(b.get() == rawB);
}

TEST_CASE("UniquePtr - Nullptr assignment resets", "[memory][UniquePtr]")
{
    LifetimeProbe::s_aliveCount = 0;
    auto p = gp::makeUnique<LifetimeProbe>();
    p = nullptr;
    REQUIRE_FALSE(p);
    REQUIRE(LifetimeProbe::s_aliveCount == 0);
}

TEST_CASE("UniquePtr - Polymorphic conversion", "[memory][UniquePtr]")
{
    gp::UniquePtr<Derived> d = gp::makeUnique<Derived>();
    gp::UniquePtr<Base> b(std::move(d));
    REQUIRE(d.get() == nullptr);
    REQUIRE(b);
    REQUIRE(b->tag() == 2);
}

TEST_CASE("UniquePtr - Equality comparisons", "[memory][UniquePtr]")
{
    gp::UniquePtr<int> empty;
    auto p = gp::makeUnique<int>(5);

    REQUIRE(empty == nullptr);
    REQUIRE(nullptr == empty);
    REQUIRE(p != nullptr);
    REQUIRE(nullptr != p);
    REQUIRE(p != empty);
}

TEST_CASE("UniquePtr - Self-move assignment is safe", "[memory][UniquePtr]")
{
    LifetimeProbe::s_aliveCount = 0;
    auto p = gp::makeUnique<LifetimeProbe>(11);

    // Use a reference to opaque the compiler's view of self-assignment; this is intentional to
    // exercise the guard inside the move assignment operator.
    auto& ref = p;
    p = std::move(ref);

    REQUIRE(p);
    REQUIRE(p->value == 11);
    REQUIRE(LifetimeProbe::s_aliveCount == 1);
}

}   // namespace gp::tests
