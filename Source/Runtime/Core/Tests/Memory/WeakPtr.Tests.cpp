// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/WeakPtr.hpp"
#include "CoreTypes.hpp"
#include "Memory/SharedPtr.hpp"
#include <catch2/catch_test_macros.hpp>

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

struct FBase
{
    bool& m_destroyed;

    explicit FBase(bool& outDestroyed)
        : m_destroyed(outDestroyed)
    {
        m_destroyed = false;
    }

    virtual ~FBase() { m_destroyed = true; }
};

struct FDerived : FBase
{
    explicit FDerived(bool& outDestroyed)
        : FBase(outDestroyed)
    {}
};

}   // namespace

TEST_CASE("TWeakPtr: Default construction yields expired weak pointer", "[Memory][WeakPtr]")
{
    TWeakPtr<Int32> weak;
    REQUIRE(weak.IsExpired());
    REQUIRE(weak.UseCount() == 0);
}

TEST_CASE("TWeakPtr: Default construction is noexcept", "[Memory][WeakPtr]")
{
    STATIC_REQUIRE(noexcept(TWeakPtr<Int32>{}));
}

TEST_CASE("TWeakPtr: Construction from TSharedPtr observes the managed object", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> shared(new Int32(5));
    TWeakPtr<Int32> weak(shared);
    REQUIRE_FALSE(weak.IsExpired());
    REQUIRE(weak.UseCount() == 1);
}

TEST_CASE("TWeakPtr: Construction from TSharedPtr does not increment strong count", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> shared(new Int32(1));
    TWeakPtr<Int32> weak(shared);
    REQUIRE(shared.UseCount() == 1);
}

TEST_CASE("TWeakPtr: Construction from derived TSharedPtr is accepted", "[Memory][WeakPtr]")
{
    bool destroyed = false;
    TSharedPtr<FDerived> derived(new FDerived(destroyed));
    TWeakPtr<FBase> weakBase(derived);
    REQUIRE_FALSE(weakBase.IsExpired());
    REQUIRE(weakBase.UseCount() == 1);
}

TEST_CASE("TWeakPtr: Copy construction shares observation", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> shared(new Int32(2));
    TWeakPtr<Int32> a(shared);
    TWeakPtr<Int32> b(a);
    REQUIRE_FALSE(b.IsExpired());
    REQUIRE(b.UseCount() == 1);
    REQUIRE(shared.UseCount() == 1);
}

TEST_CASE("TWeakPtr: Copy construction from derived weak pointer is accepted", "[Memory][WeakPtr]")
{
    bool destroyed = false;
    TSharedPtr<FDerived> derived(new FDerived(destroyed));
    TWeakPtr<FDerived> weakDerived(derived);
    TWeakPtr<FBase> weakBase(weakDerived);
    REQUIRE_FALSE(weakBase.IsExpired());
}

TEST_CASE("TWeakPtr: Move construction transfers observation and empties source", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> shared(new Int32(3));
    TWeakPtr<Int32> a(shared);
    TWeakPtr<Int32> b(GP::Move(a));
    REQUIRE(a.IsExpired());
    REQUIRE_FALSE(b.IsExpired());
    REQUIRE(b.UseCount() == 1);
}

TEST_CASE("TWeakPtr: Move construction from derived weak pointer is accepted", "[Memory][WeakPtr]")
{
    bool destroyed = false;
    TSharedPtr<FDerived> derived(new FDerived(destroyed));
    TWeakPtr<FDerived> weakDerived(derived);
    TWeakPtr<FBase> weakBase(GP::Move(weakDerived));
    REQUIRE(weakDerived.IsExpired());
    REQUIRE_FALSE(weakBase.IsExpired());
}

TEST_CASE("TWeakPtr: Destructor releases weak reference without destroying object", "[Memory][WeakPtr]")
{
    bool destroyed = false;
    TSharedPtr<FTracked> shared(new FTracked(destroyed));
    {
        TWeakPtr<FTracked> weak(shared);
        REQUIRE_FALSE(destroyed);
    }
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TWeakPtr: Copy assignment replaces observation", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> a(new Int32(1));
    TSharedPtr<Int32> b(new Int32(2));
    TWeakPtr<Int32> wa(a);
    TWeakPtr<Int32> wb(b);
    wa = wb;
    auto locked = wa.Lock();
    REQUIRE(locked.Get() == b.Get());
}

TEST_CASE("TWeakPtr: Move assignment transfers observation and empties source", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> shared(new Int32(10));
    TWeakPtr<Int32> a(shared);
    TWeakPtr<Int32> b;
    b = GP::Move(a);
    REQUIRE(a.IsExpired());
    REQUIRE_FALSE(b.IsExpired());
}

TEST_CASE("TWeakPtr: Assignment from TSharedPtr observes the managed object", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> shared(new Int32(7));
    TWeakPtr<Int32> weak;
    weak = shared;
    REQUIRE_FALSE(weak.IsExpired());
    REQUIRE(weak.UseCount() == 1);
}

TEST_CASE("TWeakPtr: Lock returns valid TSharedPtr when object is alive", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> shared(new Int32(99));
    TWeakPtr<Int32> weak(shared);
    auto locked = weak.Lock();
    REQUIRE(locked.Get() == shared.Get());
    REQUIRE(locked.UseCount() == 2);
}

TEST_CASE("TWeakPtr: Lock returns empty TSharedPtr when object is destroyed", "[Memory][WeakPtr]")
{
    TWeakPtr<Int32> weak;
    {
        TSharedPtr<Int32> shared(new Int32(1));
        weak = shared;
        REQUIRE_FALSE(weak.IsExpired());
    }
    auto locked = weak.Lock();
    REQUIRE(locked.Get() == nullptr);
    REQUIRE(weak.IsExpired());
}

TEST_CASE("TWeakPtr: Lock increments strong count", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> shared(new Int32(1));
    TWeakPtr<Int32> weak(shared);
    auto locked = weak.Lock();
    REQUIRE(shared.UseCount() == 2);
}

TEST_CASE("TWeakPtr: IsExpired returns false while shared owner exists", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> shared(new Int32(1));
    TWeakPtr<Int32> weak(shared);
    REQUIRE_FALSE(weak.IsExpired());
}

TEST_CASE("TWeakPtr: IsExpired returns true after all shared owners are gone", "[Memory][WeakPtr]")
{
    TWeakPtr<Int32> weak;
    {
        TSharedPtr<Int32> shared(new Int32(1));
        weak = shared;
    }
    REQUIRE(weak.IsExpired());
}

TEST_CASE("TWeakPtr: UseCount matches the strong reference count", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> a(new Int32(1));
    TWeakPtr<Int32> weak(a);
    REQUIRE(weak.UseCount() == 1);
    TSharedPtr<Int32> b(a);
    REQUIRE(weak.UseCount() == 2);
}

TEST_CASE("TWeakPtr: UseCount is zero after all owners are destroyed", "[Memory][WeakPtr]")
{
    TWeakPtr<Int32> weak;
    {
        TSharedPtr<Int32> shared(new Int32(1));
        weak = shared;
        REQUIRE(weak.UseCount() == 1);
    }
    REQUIRE(weak.UseCount() == 0);
}

TEST_CASE("TWeakPtr: Reset makes weak pointer expired", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> shared(new Int32(1));
    TWeakPtr<Int32> weak(shared);
    REQUIRE_FALSE(weak.IsExpired());
    weak.Reset();
    REQUIRE(weak.IsExpired());
}

TEST_CASE("TWeakPtr: Reset does not destroy the managed object", "[Memory][WeakPtr]")
{
    bool destroyed = false;
    TSharedPtr<FTracked> shared(new FTracked(destroyed));
    TWeakPtr<FTracked> weak(shared);
    weak.Reset();
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TWeakPtr: Swap exchanges two weak pointers", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> a(new Int32(1));
    TSharedPtr<Int32> b(new Int32(2));
    TWeakPtr<Int32> wa(a);
    TWeakPtr<Int32> wb(b);
    wa.Swap(wb);
    REQUIRE(wa.Lock().Get() == b.Get());
    REQUIRE(wb.Lock().Get() == a.Get());
}

TEST_CASE("TWeakPtr: Non-member Swap exchanges two weak pointers", "[Memory][WeakPtr]")
{
    TSharedPtr<Int32> a(new Int32(1));
    TSharedPtr<Int32> b(new Int32(2));
    TWeakPtr<Int32> wa(a);
    TWeakPtr<Int32> wb(b);
    Swap(wa, wb);
    REQUIRE(wa.Lock().Get() == b.Get());
    REQUIRE(wb.Lock().Get() == a.Get());
}

TEST_CASE("TWeakPtr: Control block outlives all shared owners while weak ref exists", "[Memory][WeakPtr]")
{
    bool destroyed = false;
    TWeakPtr<FTracked> weak;
    {
        TSharedPtr<FTracked> shared(new FTracked(destroyed));
        weak = shared;
        REQUIRE_FALSE(destroyed);
    }
    REQUIRE(destroyed);
    REQUIRE(weak.IsExpired());
}
