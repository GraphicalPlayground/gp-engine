// Copyright (c) - Graphical Playground. All rights reserved.

#include "Events/ScopedDelegateHandle.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;
using namespace GP::Events;

TEST_CASE("FScopedDelegateHandle — Default Construction", "[GP][Core][Events][FScopedDelegateHandle]")
{
    const FScopedDelegateHandle h;
    REQUIRE_FALSE(h.IsValid());
    REQUIRE_FALSE(h.GetHandle().IsValid());
}

TEST_CASE("FScopedDelegateHandle — Construction from delegate and handle", "[GP][Core][Events][FScopedDelegateHandle]")
{
    TMulticastDelegate<void()> d;
    const FDelegateHandle raw = d.AddLambda([]() {});

    FScopedDelegateHandle scoped(d, raw);

    REQUIRE(scoped.IsValid());
    REQUIRE(scoped.GetHandle() == raw);
    REQUIRE(d.Contains(raw));
}

TEST_CASE("FScopedDelegateHandle — Destructor removes binding", "[GP][Core][Events][FScopedDelegateHandle]")
{
    TMulticastDelegate<void()> d;
    int callCount = 0;

    {
        FScopedDelegateHandle scoped(d, d.AddLambda([&callCount]() { ++callCount; }));
        REQUIRE(d.GetBindingCount() == 1);

        d.Broadcast();
        REQUIRE(callCount == 1);
    }   // scoped goes out of scope here

    REQUIRE(d.GetBindingCount() == 0);
    d.Broadcast();
    REQUIRE(callCount == 1);   // listener NOT called after destruction
}

TEST_CASE(
    "FScopedDelegateHandle — Default-constructed destructor is no-op", "[GP][Core][Events][FScopedDelegateHandle]"
)
{
    // Should not crash or assert.
    TMulticastDelegate<void()> d;
    (void)d.AddLambda([]() {});
    {
        FScopedDelegateHandle empty;
    }
    REQUIRE(d.GetBindingCount() == 1);
}

TEST_CASE("FScopedDelegateHandle — Move Construction transfers ownership", "[GP][Core][Events][FScopedDelegateHandle]")
{
    TMulticastDelegate<void()> d;
    const FDelegateHandle raw = d.AddLambda([]() {});

    FScopedDelegateHandle src(d, raw);
    REQUIRE(src.IsValid());

    FScopedDelegateHandle dst = std::move(src);

    REQUIRE_FALSE(src.IsValid());   // source is now empty
    REQUIRE(dst.IsValid());
    REQUIRE(dst.GetHandle() == raw);
    REQUIRE(d.Contains(raw));
}

TEST_CASE(
    "FScopedDelegateHandle — Move Construction source does not remove binding",
    "[GP][Core][Events][FScopedDelegateHandle]"
)
{
    TMulticastDelegate<void()> d;
    const FDelegateHandle raw = d.AddLambda([]() {});

    {
        FScopedDelegateHandle src(d, raw);
        FScopedDelegateHandle dst = std::move(src);
        // src destroyed → should be a no-op since it was moved from
    }

    // dst goes out of scope and should remove the binding
    REQUIRE(d.GetBindingCount() == 0);
}

TEST_CASE(
    "FScopedDelegateHandle — Move Assignment releases old and takes ownership",
    "[GP][Core][Events][FScopedDelegateHandle]"
)
{
    TMulticastDelegate<void()> d;
    int countA = 0, countB = 0;

    const FDelegateHandle rawA = d.AddLambda([&countA]() { ++countA; });
    const FDelegateHandle rawB = d.AddLambda([&countB]() { ++countB; });

    FScopedDelegateHandle hA(d, rawA);
    FScopedDelegateHandle hB(d, rawB);

    REQUIRE(d.GetBindingCount() == 2);

    hA = std::move(hB);   // hA releases rawA, takes ownership of rawB

    // rawA should have been removed by the old hA's release.
    REQUIRE(d.GetBindingCount() == 1);
    REQUIRE_FALSE(d.Contains(rawA));
    REQUIRE(d.Contains(rawB));
    REQUIRE(hA.GetHandle() == rawB);
    REQUIRE_FALSE(hB.IsValid());
}

TEST_CASE(
    "FScopedDelegateHandle — Move Assignment self-assignment is safe", "[GP][Core][Events][FScopedDelegateHandle]"
)
{
    TMulticastDelegate<void()> d;
    const FDelegateHandle raw = d.AddLambda([]() {});

    FScopedDelegateHandle h(d, raw);
    FScopedDelegateHandle& ref = h;

    // Self-move — must not double-free.
    h = std::move(ref);

    // After self-move the binding should still be present (or at worst safely gone).
    // The standard only requires the object is in a valid state.
    REQUIRE(d.GetBindingCount() <= 1);
}

TEST_CASE(
    "FScopedDelegateHandle — Release removes binding and invalidates handle",
    "[GP][Core][Events][FScopedDelegateHandle]"
)
{
    TMulticastDelegate<void()> d;
    int callCount = 0;
    FScopedDelegateHandle h(d, d.AddLambda([&callCount]() { ++callCount; }));

    REQUIRE(h.IsValid());
    REQUIRE(d.GetBindingCount() == 1);

    h.Release();

    REQUIRE_FALSE(h.IsValid());
    REQUIRE(d.GetBindingCount() == 0);

    d.Broadcast();
    REQUIRE(callCount == 0);
}

TEST_CASE("FScopedDelegateHandle — Calling Release twice is safe", "[GP][Core][Events][FScopedDelegateHandle]")
{
    TMulticastDelegate<void()> d;
    FScopedDelegateHandle h(d, d.AddLambda([]() {}));

    h.Release();
    REQUIRE_NOTHROW(h.Release());   // second call must be a no-op
    REQUIRE_FALSE(h.IsValid());
}

TEST_CASE(
    "FScopedDelegateHandle — Release on default-constructed is no-op", "[GP][Core][Events][FScopedDelegateHandle]"
)
{
    FScopedDelegateHandle h;
    REQUIRE_NOTHROW(h.Release());
}

TEST_CASE(
    "FScopedDelegateHandle — Detach returns underlying handle without removing binding",
    "[GP][Core][Events][FScopedDelegateHandle]"
)
{
    TMulticastDelegate<void()> d;
    const FDelegateHandle raw = d.AddLambda([]() {});

    FScopedDelegateHandle h(d, raw);
    const FDelegateHandle detached = h.Detach();

    REQUIRE_FALSE(h.IsValid());   // scoped handle is now empty
    REQUIRE(detached == raw);     // returned the original handle
    REQUIRE(d.Contains(raw));     // binding is still alive
    REQUIRE(d.GetBindingCount() == 1);
}

TEST_CASE(
    "FScopedDelegateHandle — Destructor after Detach does not remove binding",
    "[GP][Core][Events][FScopedDelegateHandle]"
)
{
    TMulticastDelegate<void()> d;
    FDelegateHandle raw;

    {
        FScopedDelegateHandle h(d, d.AddLambda([]() {}));
        raw = h.Detach();
    }   // destructor fires on empty handle → no-op

    REQUIRE(d.GetBindingCount() == 1);
    REQUIRE(d.Contains(raw));
}

TEST_CASE("FScopedDelegateHandle — Swap exchanges bindings", "[GP][Core][Events][FScopedDelegateHandle]")
{
    TMulticastDelegate<void()> d;
    int countA = 0, countB = 0;

    FScopedDelegateHandle hA(d, d.AddLambda([&countA]() { ++countA; }));
    FScopedDelegateHandle hB(d, d.AddLambda([&countB]() { ++countB; }));

    const FDelegateHandle rawA = hA.GetHandle();
    const FDelegateHandle rawB = hB.GetHandle();

    hA.Swap(hB);

    REQUIRE(hA.GetHandle() == rawB);
    REQUIRE(hB.GetHandle() == rawA);

    // Both bindings are still active.
    REQUIRE(d.GetBindingCount() == 2);
}

TEST_CASE("FScopedDelegateHandle — Swap with empty handle", "[GP][Core][Events][FScopedDelegateHandle]")
{
    TMulticastDelegate<void()> d;
    FScopedDelegateHandle hA(d, d.AddLambda([]() {}));
    FScopedDelegateHandle hEmpty;

    const FDelegateHandle rawA = hA.GetHandle();

    hA.Swap(hEmpty);

    REQUIRE_FALSE(hA.IsValid());   // hA is now empty
    REQUIRE(hEmpty.IsValid());     // hEmpty now owns the binding
    REQUIRE(hEmpty.GetHandle() == rawA);
    REQUIRE(d.GetBindingCount() == 1);
}

TEST_CASE("FScopedDelegateHandle — ADL swap via std::swap", "[GP][Core][Events][FScopedDelegateHandle]")
{
    TMulticastDelegate<void()> d;

    FScopedDelegateHandle hA(d, d.AddLambda([]() {}));
    FScopedDelegateHandle hB(d, d.AddLambda([]() {}));

    const FDelegateHandle rawA = hA.GetHandle();
    const FDelegateHandle rawB = hB.GetHandle();

    using std::swap;
    swap(hA, hB);

    REQUIRE(hA.GetHandle() == rawB);
    REQUIRE(hB.GetHandle() == rawA);
}

TEST_CASE(
    "FScopedDelegateHandle — GetHandle returns the underlying handle", "[GP][Core][Events][FScopedDelegateHandle]"
)
{
    TMulticastDelegate<void()> d;
    const FDelegateHandle raw = d.AddLambda([]() {});
    const FScopedDelegateHandle h(d, raw);

    REQUIRE(h.GetHandle() == raw);
    REQUIRE(h.GetHandle().IsValid());
}

TEST_CASE(
    "FScopedDelegateHandle — GetHandle on default-constructed returns invalid handle",
    "[GP][Core][Events][FScopedDelegateHandle]"
)
{
    const FScopedDelegateHandle h;
    REQUIRE_FALSE(h.GetHandle().IsValid());
}
