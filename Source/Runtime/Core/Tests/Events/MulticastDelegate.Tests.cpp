// Copyright (c) - Graphical Playground. All rights reserved.

#include "Events/MulticastDelegate.hpp"
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace GP;
using namespace GP::Events;

static void FreeAppend(std::vector<int>& out, int value) { out.push_back(value); }

struct FObserver
{
    std::vector<int> received;

    void OnValue(int v) { received.push_back(v); }

    void OnValueConst(int v) const { const_cast<FObserver*>(this)->received.push_back(v); }
};

TEST_CASE("TMulticastDelegate — Default Construction", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    REQUIRE_FALSE(d.IsBound());
    REQUIRE(d.IsEmpty());
    REQUIRE(d.GetBindingCount() == 0);
    REQUIRE_FALSE(d.IsBroadcasting());
}

TEST_CASE("TMulticastDelegate — Move Construction", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d1;
    int callCount = 0;
    (void)d1.AddLambda([&callCount]() { ++callCount; });
    REQUIRE(d1.GetBindingCount() == 1);

    TMulticastDelegate<void()> d2 = std::move(d1);
    REQUIRE(d1.GetBindingCount() == 0);
    REQUIRE(d2.GetBindingCount() == 1);

    d2.Broadcast();
    REQUIRE(callCount == 1);
}

TEST_CASE("TMulticastDelegate — Move Assignment", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d1;
    int callCount = 0;
    (void)d1.AddLambda([&callCount]() { ++callCount; });

    TMulticastDelegate<void()> d2;
    (void)d2.AddLambda([]() {});
    REQUIRE(d2.GetBindingCount() == 1);

    d2 = std::move(d1);
    REQUIRE(d1.GetBindingCount() == 0);
    REQUIRE(d2.GetBindingCount() == 1);

    d2.Broadcast();
    REQUIRE(callCount == 1);
}

TEST_CASE("TMulticastDelegate — AddStatic returns valid handle", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void(std::vector<int>&, int)> d;
    std::vector<int> out;

    const FDelegateHandle h = d.AddStatic(&FreeAppend);

    REQUIRE(h.IsValid());
    REQUIRE(d.GetBindingCount() == 1);

    d.Broadcast(out, 7);
    REQUIRE(out.size() == 1);
    REQUIRE(out[0] == 7);
}

TEST_CASE("TMulticastDelegate — AddLambda", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void(int)> d;
    int sum = 0;

    const FDelegateHandle h = d.AddLambda([&sum](int x) { sum += x; });

    REQUIRE(h.IsValid());
    REQUIRE(d.GetBindingCount() == 1);

    d.Broadcast(5);
    d.Broadcast(3);
    REQUIRE(sum == 8);
}

TEST_CASE("TMulticastDelegate — AddMember non-const", "[GP][Core][Events][TMulticastDelegate]")
{
    FObserver obs;
    TMulticastDelegate<void(int)> d;
    const FDelegateHandle h = d.AddMember(&obs, &FObserver::OnValue);

    REQUIRE(h.IsValid());
    d.Broadcast(42);
    REQUIRE(obs.received.size() == 1);
    REQUIRE(obs.received[0] == 42);
}

TEST_CASE("TMulticastDelegate — AddMember const", "[GP][Core][Events][TMulticastDelegate]")
{
    FObserver obs;
    TMulticastDelegate<void(int)> d;
    const FDelegateHandle h = d.AddMember(&obs, &FObserver::OnValueConst);

    REQUIRE(h.IsValid());
    d.Broadcast(99);
    REQUIRE(obs.received.size() == 1);
    REQUIRE(obs.received[0] == 99);
}

TEST_CASE("TMulticastDelegate — AddWeakSP non-const, object alive", "[GP][Core][Events][TMulticastDelegate]")
{
    auto sp = std::make_shared<FObserver>();
    TMulticastDelegate<void(int)> d;
    const FDelegateHandle h = d.AddWeakSP(sp, &FObserver::OnValue);

    REQUIRE(h.IsValid());
    d.Broadcast(11);
    REQUIRE(sp->received.size() == 1);
    REQUIRE(sp->received[0] == 11);
}

TEST_CASE("TMulticastDelegate — AddWeakSP const member, object alive", "[GP][Core][Events][TMulticastDelegate]")
{
    auto sp = std::make_shared<FObserver>();
    TMulticastDelegate<void(int)> d;
    const FDelegateHandle h = d.AddWeakSP(sp, &FObserver::OnValueConst);

    REQUIRE(h.IsValid());
    d.Broadcast(22);
    REQUIRE(sp->received.size() == 1);
    REQUIRE(sp->received[0] == 22);
}

TEST_CASE(
    "TMulticastDelegate — AddWeakSP skips invocation when object expired", "[GP][Core][Events][TMulticastDelegate]"
)
{
    auto sp = std::make_shared<FObserver>();
    TMulticastDelegate<void(int)> d;
    (void)d.AddWeakSP(sp, &FObserver::OnValue);

    sp.reset();   // destroy the managed object

    // Broadcast must NOT invoke the expired binding (no crash).
    REQUIRE_NOTHROW(d.Broadcast(55));
}

TEST_CASE("TMulticastDelegate — Multiple bindings", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    int a = 0, b = 0, c = 0;
    (void)d.AddLambda([&a]() { ++a; });
    (void)d.AddLambda([&b]() { ++b; });
    (void)d.AddLambda([&c]() { ++c; });

    REQUIRE(d.GetBindingCount() == 3);
    REQUIRE(d.IsBound());

    d.Broadcast();
    REQUIRE(a == 1);
    REQUIRE(b == 1);
    REQUIRE(c == 1);
}

TEST_CASE(
    "TMulticastDelegate — Broadcast calls listeners in registration order", "[GP][Core][Events][TMulticastDelegate]"
)
{
    TMulticastDelegate<void()> d;
    std::vector<int> order;
    (void)d.AddLambda([&order]() { order.push_back(1); });
    (void)d.AddLambda([&order]() { order.push_back(2); });
    (void)d.AddLambda([&order]() { order.push_back(3); });

    d.Broadcast();

    REQUIRE(order.size() == 3);
    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
    REQUIRE(order[2] == 3);
}

TEST_CASE("TMulticastDelegate — Broadcast on empty delegate is no-op", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    REQUIRE_NOTHROW(d.Broadcast());
}

TEST_CASE("TMulticastDelegate — IsBroadcasting is true during Broadcast", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    bool wasTrue = false;
    (void)d.AddLambda([&]() { wasTrue = d.IsBroadcasting(); });

    REQUIRE_FALSE(d.IsBroadcasting());
    d.Broadcast();
    REQUIRE(wasTrue);
    REQUIRE_FALSE(d.IsBroadcasting());
}

TEST_CASE("TMulticastDelegate — Contains valid handle", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    const FDelegateHandle h = d.AddLambda([]() {});
    REQUIRE(d.Contains(h));
}

TEST_CASE("TMulticastDelegate — Contains invalid handle returns false", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    (void)d.AddLambda([]() {});
    REQUIRE_FALSE(d.Contains(FDelegateHandle{}));
}

TEST_CASE("TMulticastDelegate — Contains returns false after Remove", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    const FDelegateHandle h = d.AddLambda([]() {});
    d.Remove(h);
    REQUIRE_FALSE(d.Contains(h));
}

TEST_CASE(
    "TMulticastDelegate — Contains detects pending addition during Broadcast", "[GP][Core][Events][TMulticastDelegate]"
)
{
    TMulticastDelegate<void()> d;
    FDelegateHandle newHandle;
    bool containsDuringBroadcast = false;

    (void)d.AddLambda(
        [&]()
        {
            newHandle = d.AddLambda([]() {});
            containsDuringBroadcast = d.Contains(newHandle);
        }
    );

    d.Broadcast();
    REQUIRE(containsDuringBroadcast);   // pending addition is visible via Contains
    REQUIRE(d.Contains(newHandle));     // still active after broadcast
}

TEST_CASE("TMulticastDelegate — Remove with valid handle returns true", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    const FDelegateHandle h = d.AddLambda([]() {});
    REQUIRE(d.Remove(h));
}

TEST_CASE("TMulticastDelegate — Remove actually removes the binding", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    int count = 0;
    const FDelegateHandle h = d.AddLambda([&count]() { ++count; });
    d.Broadcast();
    REQUIRE(count == 1);

    d.Remove(h);
    d.Broadcast();
    REQUIRE(count == 1);   // listener NOT called after removal
    REQUIRE(d.GetBindingCount() == 0);
}

TEST_CASE("TMulticastDelegate — Remove invalid handle returns false", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    REQUIRE_FALSE(d.Remove(FDelegateHandle{}));
}

TEST_CASE("TMulticastDelegate — Remove already-removed handle returns false", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    const FDelegateHandle h = d.AddLambda([]() {});
    REQUIRE(d.Remove(h));
    REQUIRE_FALSE(d.Remove(h));
}

TEST_CASE("TMulticastDelegate — Remove pending addition during Broadcast", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    FDelegateHandle pendingHandle;

    (void)d.AddLambda(
        [&]()
        {
            // Add and immediately remove an element that hasn't been merged yet.
            pendingHandle = d.AddLambda([]() {});
            d.Remove(pendingHandle);
        }
    );

    d.Broadcast();

    REQUIRE(d.GetBindingCount() == 1);   // only the original listener survives
    REQUIRE_FALSE(d.Contains(pendingHandle));
}

TEST_CASE("TMulticastDelegate — RemoveAll clears all bindings", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    (void)d.AddLambda([]() {});
    (void)d.AddLambda([]() {});
    (void)d.AddLambda([]() {});

    REQUIRE(d.GetBindingCount() == 3);
    d.RemoveAll();
    REQUIRE(d.GetBindingCount() == 0);
    REQUIRE(d.IsEmpty());
}

TEST_CASE("TMulticastDelegate — RemoveAll during Broadcast is deferred", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    int callCount = 0;

    (void)d.AddLambda(
        [&]()
        {
            ++callCount;
            d.RemoveAll();   // marks all remaining bindings as pendingRemoval immediately,
                             // so binding[1] and binding[2] are skipped in this pass.
        }
    );
    (void)d.AddLambda([&]() { ++callCount; });
    (void)d.AddLambda([&]() { ++callCount; });

    d.Broadcast();
    REQUIRE(callCount == 1);             // only binding[0] fired; the rest were already marked
    REQUIRE(d.GetBindingCount() == 0);   // all cleared after the broadcast
}

TEST_CASE("TMulticastDelegate — Listener removes itself during Broadcast", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    int callCount = 0;
    FDelegateHandle selfHandle;

    selfHandle = d.AddLambda(
        [&]()
        {
            ++callCount;
            d.Remove(selfHandle);   // deferred self-removal
        }
    );
    (void)d.AddLambda([&]() { ++callCount; });

    d.Broadcast();
    REQUIRE(callCount == 2);                 // both fired this pass
    REQUIRE_FALSE(d.Contains(selfHandle));   // removed after
    REQUIRE(d.GetBindingCount() == 1);       // only second listener remains
}

TEST_CASE(
    "TMulticastDelegate — Listener removes a future listener during Broadcast", "[GP][Core][Events][TMulticastDelegate]"
)
{
    TMulticastDelegate<void()> d;
    int callCount = 0;
    FDelegateHandle futureHandle;

    (void)d.AddLambda(
        [&]()
        {
            ++callCount;
            d.Remove(futureHandle);   // remove the next listener; it is already marked, so won't fire
        }
    );
    futureHandle = d.AddLambda([&]() { ++callCount; });

    d.Broadcast();
    // The future listener is marked pendingRemoval immediately, so it must NOT fire.
    REQUIRE(callCount == 1);
}

TEST_CASE("TMulticastDelegate — Listener added during Broadcast is deferred", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    int callCount = 0;

    (void)d.AddLambda(
        [&]()
        {
            ++callCount;
            (void)d.AddLambda([&]() { ++callCount; });   // added during broadcast → deferred
        }
    );

    d.Broadcast();
    REQUIRE(callCount == 1);   // new listener NOT called in this pass
    REQUIRE(d.GetBindingCount() == 2);

    d.Broadcast();
    REQUIRE(callCount == 1 + 2);   // original + newly merged listener both fire
}

TEST_CASE("TMulticastDelegate — Nested Broadcast is fully re-entrant", "[GP][Core][Events][TMulticastDelegate]")
{
    TMulticastDelegate<void()> d;
    std::vector<int> order;

    // Use a guard flag so binding[0] only triggers one level of nested broadcast,
    // preventing infinite recursion while still exercising the re-entrancy path.
    bool nestedTriggered = false;

    (void)d.AddLambda(
        [&]()
        {
            order.push_back(1);
            if (!nestedTriggered)
            {
                nestedTriggered = true;
                d.Broadcast();   // one nested broadcast; guard prevents further recursion
            }
            order.push_back(3);
        }
    );
    (void)d.AddLambda([&]() { order.push_back(2); });

    d.Broadcast();

    // Expected execution sequence:
    //   outer  binding[0]: push 1, nestedTriggered=false → set true, enter inner broadcast
    //     inner binding[0]: push 1, nestedTriggered=true → skip recursion, push 3
    //     inner binding[1]: push 2
    //   outer  binding[0] resumes: push 3
    //   outer  binding[1]: push 2
    REQUIRE(order.size() == 6);
    REQUIRE(order[0] == 1);   // outer binding[0] — enter
    REQUIRE(order[1] == 1);   // inner binding[0] — enter
    REQUIRE(order[2] == 3);   // inner binding[0] — exit
    REQUIRE(order[3] == 2);   // inner binding[1]
    REQUIRE(order[4] == 3);   // outer binding[0] — exit
    REQUIRE(order[5] == 2);   // outer binding[1]
}

TEST_CASE(
    "TMulticastDelegate — GetBindingCount excludes pending-removal bindings", "[GP][Core][Events][TMulticastDelegate]"
)
{
    TMulticastDelegate<void()> d;
    FDelegateHandle h1 = d.AddLambda([]() {});
    FDelegateHandle h2 = d.AddLambda([]() {});

    REQUIRE(d.GetBindingCount() == 2);

    // Remove h1 during a broadcast so it stays in m_bindings as pendingRemoval during the loop.
    (void)d.AddLambda([&]() { d.Remove(h1); });
    d.Broadcast();

    // After the broadcast the pending removal is flushed.
    REQUIRE(d.GetBindingCount() == 2);   // h2 + the lambda that removed h1
}
