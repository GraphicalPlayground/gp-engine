// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "memory/RefCountPtr.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <utility>

namespace gp::tests
{

namespace
{

/// @brief GP-native refcounted object exposing addRef/release (lowerCamelCase).
class GpRefCounted
{
public:
    static inline std::atomic<gp::Int32> s_aliveCount{ 0 };

    GpRefCounted() noexcept
    {
        s_aliveCount.fetch_add(1, std::memory_order_relaxed);
    }

    virtual ~GpRefCounted() noexcept
    {
        s_aliveCount.fetch_sub(1, std::memory_order_relaxed);
    }

    void addRef() noexcept
    {
        m_count.fetch_add(1, std::memory_order_relaxed);
    }

    void release() noexcept
    {
        if (m_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }

    gp::Int32 getRefCount() const noexcept
    {
        return m_count.load(std::memory_order_acquire);
    }

private:
    std::atomic<gp::Int32> m_count{ 1 };
};

class DerivedGpRefCounted : public GpRefCounted
{
public:
    gp::Int32 identifier{ 42 };
};

}   // namespace

TEST_CASE("RefCountPtr - Size guarantee", "[memory][RefCountPtr]")
{
    STATIC_REQUIRE(sizeof(gp::RefCountPtr<GpRefCounted>) == sizeof(void*));
}

TEST_CASE("RefCountPtr - Concept accepts both naming conventions", "[memory][RefCountPtr]")
{
    STATIC_REQUIRE(gp::IsRefCounted<GpRefCounted>);
    STATIC_REQUIRE_FALSE(gp::IsRefCounted<int>);
}

TEST_CASE("RefCountPtr - Default construction is empty", "[memory][RefCountPtr]")
{
    gp::RefCountPtr<GpRefCounted> p;
    REQUIRE_FALSE(static_cast<bool>(p));
    REQUIRE(p.get() == nullptr);
    REQUIRE(p == nullptr);
}

TEST_CASE("RefCountPtr - Explicit construction increments count", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        auto* raw = new GpRefCounted();
        REQUIRE(raw->getRefCount() == 1);
        {
            gp::RefCountPtr<GpRefCounted> p(raw);
            REQUIRE(raw->getRefCount() == 2);
        }
        REQUIRE(raw->getRefCount() == 1);
        raw->release();
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

TEST_CASE("RefCountPtr - attach adopts without AddRef", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        auto* raw = new GpRefCounted();
        REQUIRE(raw->getRefCount() == 1);

        gp::RefCountPtr<GpRefCounted> p(raw, gp::RefCountPtr<GpRefCounted>::attachT);
        REQUIRE(raw->getRefCount() == 1);   // no bump
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

TEST_CASE("RefCountPtr - Copy construction shares ownership", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        auto* raw = new GpRefCounted();
        gp::RefCountPtr<GpRefCounted> a(raw, gp::RefCountPtr<GpRefCounted>::attachT);
        {
            gp::RefCountPtr<GpRefCounted> b(a);
            REQUIRE(raw->getRefCount() == 2);
        }
        REQUIRE(raw->getRefCount() == 1);
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

TEST_CASE("RefCountPtr - Move construction transfers without bump", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        auto* raw = new GpRefCounted();
        gp::RefCountPtr<GpRefCounted> a(raw, gp::RefCountPtr<GpRefCounted>::attachT);
        gp::RefCountPtr<GpRefCounted> b(std::move(a));
        REQUIRE(raw->getRefCount() == 1);
        REQUIRE(a.get() == nullptr);
        REQUIRE(b.get() == raw);
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

TEST_CASE("RefCountPtr - Copy assignment balances old and new refs", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        auto* rawA = new GpRefCounted();
        auto* rawB = new GpRefCounted();
        gp::RefCountPtr<GpRefCounted> a(rawA, gp::RefCountPtr<GpRefCounted>::attachT);
        gp::RefCountPtr<GpRefCounted> b(rawB, gp::RefCountPtr<GpRefCounted>::attachT);
        REQUIRE(GpRefCounted::s_aliveCount == 2);

        b = a;
        REQUIRE(GpRefCounted::s_aliveCount == 1);   // rawB deleted
        REQUIRE(rawA->getRefCount() == 2);
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

TEST_CASE("RefCountPtr - Move assignment", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        auto* rawA = new GpRefCounted();
        auto* rawB = new GpRefCounted();
        gp::RefCountPtr<GpRefCounted> a(rawA, gp::RefCountPtr<GpRefCounted>::attachT);
        gp::RefCountPtr<GpRefCounted> b(rawB, gp::RefCountPtr<GpRefCounted>::attachT);

        b = std::move(a);
        REQUIRE(GpRefCounted::s_aliveCount == 1);
        REQUIRE(b.get() == rawA);
        REQUIRE(a.get() == nullptr);
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

TEST_CASE("RefCountPtr - reset / detach", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        auto* raw = new GpRefCounted();
        gp::RefCountPtr<GpRefCounted> p(raw, gp::RefCountPtr<GpRefCounted>::attachT);

        GpRefCounted* detached = p.detach();
        REQUIRE(p.get() == nullptr);
        REQUIRE(detached == raw);
        REQUIRE(raw->getRefCount() == 1);
        raw->release();
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);

    {
        auto* raw = new GpRefCounted();
        gp::RefCountPtr<GpRefCounted> p(raw, gp::RefCountPtr<GpRefCounted>::attachT);
        p.reset();
        REQUIRE_FALSE(p);
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

TEST_CASE("RefCountPtr - getAddressOf resets and exposes slot", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        gp::RefCountPtr<GpRefCounted> p(new GpRefCounted(), gp::RefCountPtr<GpRefCounted>::attachT);
        GpRefCounted** slot = p.getAddressOf();
        REQUIRE(p.get() == nullptr);

        // Simulate a COM-style factory filling the slot.
        *slot = new GpRefCounted();
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

TEST_CASE("RefCountPtr - Polymorphic conversion", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        gp::RefCountPtr<DerivedGpRefCounted> d(
            new DerivedGpRefCounted(), gp::RefCountPtr<DerivedGpRefCounted>::attachT
        );
        gp::RefCountPtr<GpRefCounted> b(d);
        REQUIRE(b.get() == d.get());
        REQUIRE(d->getRefCount() == 2);
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

TEST_CASE("RefCountPtr - Nullptr assignment", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        gp::RefCountPtr<GpRefCounted> p(new GpRefCounted(), gp::RefCountPtr<GpRefCounted>::attachT);
        p = nullptr;
        REQUIRE_FALSE(p);
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

TEST_CASE("RefCountPtr - Equality comparisons", "[memory][RefCountPtr]")
{
    GpRefCounted::s_aliveCount = 0;
    {
        auto* raw = new GpRefCounted();
        gp::RefCountPtr<GpRefCounted> a(raw, gp::RefCountPtr<GpRefCounted>::attachT);
        gp::RefCountPtr<GpRefCounted> b(a);
        gp::RefCountPtr<GpRefCounted> empty;

        REQUIRE(a == b);
        REQUIRE_FALSE(a != b);
        REQUIRE(empty == nullptr);
        REQUIRE(a != nullptr);
        REQUIRE(a != empty);
    }
    REQUIRE(GpRefCounted::s_aliveCount == 0);
}

}   // namespace gp::tests
