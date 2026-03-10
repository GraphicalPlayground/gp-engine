// Copyright (c) - Graphical Playground. All rights reserved.

#include "Templates/Events/Event.hpp"
#include <catch2/catch_test_macros.hpp>
#include <type_traits>
#include <vector>

using namespace GP;

namespace
{

class FListener
{
public:
    void OnValue(Int32 v) { m_sum += v; }

    void OnValueConst(Int32 v) const { m_lastSeen = v; }

    void OnFire() { ++m_fireCount; }

    Int32 GetSum() const { return m_sum; }

    Int32 GetLastSeen() const { return m_lastSeen; }

    Int32 GetFireCount() const { return m_fireCount; }

    void Reset()
    {
        m_sum = 0;
        m_lastSeen = -1;
        m_fireCount = 0;
    }

private:
    Int32 m_sum{ 0 };
    mutable Int32 m_lastSeen{ -1 };
    Int32 m_fireCount{ 0 };
};

static Int32 g_freeFnCount = 0;

static void FreeFnVoid(Int32 v) { g_freeFnCount += v; }

}   // anonymous namespace

TEST_CASE("TEvent: default-constructed event is empty", "[Templates][Event]")
{
    TEvent<void(Int32)> ev;
    REQUIRE(ev.IsEmpty());
    REQUIRE(ev.GetBindingCount() == 0);
}

TEST_CASE("TEvent: Add lambda increments binding count", "[Templates][Event]")
{
    TEvent<void(Int32)> ev;
    FDelegateHandle h = ev.Add([](Int32) {});
    REQUIRE(h.IsValid());
    REQUIRE_FALSE(ev.IsEmpty());
    REQUIRE(ev.GetBindingCount() == 1);
}

TEST_CASE("TEvent: AddFunction registers a free function", "[Templates][Event]")
{
    g_freeFnCount = 0;
    TEvent<void(Int32)> ev;
    FDelegateHandle h = ev.AddFunction(&FreeFnVoid);
    REQUIRE(h.IsValid());
    REQUIRE(ev.GetBindingCount() == 1);
    ev.Broadcast(7);
    REQUIRE(g_freeFnCount == 7);
}

TEST_CASE("TEvent: AddMethod mutable member function is invoked", "[Templates][Event]")
{
    FListener listener;
    TEvent<void(Int32)> ev;
    GP_MAYBE_UNUSED FDelegateHandle h = ev.AddMethod(&listener, &FListener::OnValue);
    ev.Broadcast(10);
    ev.Broadcast(5);
    REQUIRE(listener.GetSum() == 15);
}

TEST_CASE("TEvent: AddMethod const member function is invoked", "[Templates][Event]")
{
    FListener listener;
    TEvent<void(Int32)> ev;
    GP_MAYBE_UNUSED FDelegateHandle h =
        ev.AddMethod(static_cast<const FListener*>(&listener), &FListener::OnValueConst);
    ev.Broadcast(42);
    REQUIRE(listener.GetLastSeen() == 42);
}

TEST_CASE("TEvent: multiple Add calls accumulate binding count", "[Templates][Event]")
{
    TEvent<void()> ev;
    GP_MAYBE_UNUSED FDelegateHandle h0 = ev.Add([]() {});
    GP_MAYBE_UNUSED FDelegateHandle h1 = ev.Add([]() {});
    GP_MAYBE_UNUSED FDelegateHandle h2 = ev.Add([]() {});
    REQUIRE(ev.GetBindingCount() == 3);
}

TEST_CASE("TEvent: Broadcast invokes all lambdas and passes arguments", "[Templates][Event]")
{
    Int32 result = 0;
    TEvent<void(Int32)> ev;
    GP_MAYBE_UNUSED FDelegateHandle h0 = ev.Add([&result](Int32 v) { result += v; });
    GP_MAYBE_UNUSED FDelegateHandle h1 = ev.Add([&result](Int32 v) { result += v; });
    ev.Broadcast(3);
    REQUIRE(result == 6);
}

TEST_CASE("TEvent: Broadcast on empty event is a no-op", "[Templates][Event]")
{
    TEvent<void(Int32)> ev;
    REQUIRE_NOTHROW(ev.Broadcast(0));
}

TEST_CASE("TEvent: Critical fires before High, High before Normal, Normal before Low", "[Templates][Event]")
{
    std::vector<Int32> order;

    TEvent<void()> ev;
    GP_MAYBE_UNUSED FDelegateHandle h0 = ev.Add([&order]() { order.push_back(3); }, EEventPriority::Normal);
    GP_MAYBE_UNUSED FDelegateHandle h1 = ev.Add([&order]() { order.push_back(4); }, EEventPriority::Low);
    GP_MAYBE_UNUSED FDelegateHandle h2 = ev.Add([&order]() { order.push_back(1); }, EEventPriority::Critical);
    GP_MAYBE_UNUSED FDelegateHandle h3 = ev.Add([&order]() { order.push_back(2); }, EEventPriority::High);

    ev.Broadcast();

    REQUIRE(order.size() == 4);
    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
    REQUIRE(order[2] == 3);
    REQUIRE(order[3] == 4);
}

TEST_CASE("TEvent: bindings within the same priority fire in subscription order", "[Templates][Event]")
{
    std::vector<Int32> order;
    TEvent<void()> ev;
    GP_MAYBE_UNUSED FDelegateHandle h0 = ev.Add([&order]() { order.push_back(1); }, EEventPriority::High);
    GP_MAYBE_UNUSED FDelegateHandle h1 = ev.Add([&order]() { order.push_back(2); }, EEventPriority::High);
    GP_MAYBE_UNUSED FDelegateHandle h2 = ev.Add([&order]() { order.push_back(3); }, EEventPriority::High);
    ev.Broadcast();

    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
    REQUIRE(order[2] == 3);
}

TEST_CASE("TEvent: GetBindingCount sums across all priority buckets", "[Templates][Event]")
{
    TEvent<void()> ev;
    GP_MAYBE_UNUSED FDelegateHandle h0 = ev.Add([]() {}, EEventPriority::Critical);
    GP_MAYBE_UNUSED FDelegateHandle h1 = ev.Add([]() {}, EEventPriority::High);
    GP_MAYBE_UNUSED FDelegateHandle h2 = ev.Add([]() {}, EEventPriority::Normal);
    GP_MAYBE_UNUSED FDelegateHandle h3 = ev.Add([]() {}, EEventPriority::Low);
    REQUIRE(ev.GetBindingCount() == 4);
}

TEST_CASE("TEvent: Remove returns true for a known handle", "[Templates][Event]")
{
    TEvent<void()> ev;
    FDelegateHandle h = ev.Add([]() {});
    REQUIRE(ev.Remove(h));
}

TEST_CASE("TEvent: Remove stops future invocations", "[Templates][Event]")
{
    Int32 count = 0;
    TEvent<void()> ev;
    FDelegateHandle h = ev.Add([&count]() { ++count; });
    ev.Broadcast();
    REQUIRE(count == 1);
    ev.Remove(h);
    ev.Broadcast();
    REQUIRE(count == 1);
}

TEST_CASE("TEvent: Remove returns false for an invalid handle", "[Templates][Event]")
{
    TEvent<void()> ev;
    GP_MAYBE_UNUSED FDelegateHandle h = ev.Add([]() {});
    REQUIRE_FALSE(ev.Remove(FDelegateHandle{}));
}

TEST_CASE("TEvent: Remove returns false for an already-removed handle", "[Templates][Event]")
{
    TEvent<void()> ev;
    FDelegateHandle h = ev.Add([]() {});
    ev.Remove(h);
    REQUIRE_FALSE(ev.Remove(h));
}

TEST_CASE("TEvent: Contains returns true for a live binding and false after removal", "[Templates][Event]")
{
    TEvent<void()> ev;
    FDelegateHandle h = ev.Add([]() {});
    REQUIRE(ev.Contains(h));
    ev.Remove(h);
    REQUIRE_FALSE(ev.Contains(h));
}

TEST_CASE("TEvent: Contains returns false for an invalid handle", "[Templates][Event]")
{
    TEvent<void()> ev;
    REQUIRE_FALSE(ev.Contains(FDelegateHandle{}));
}

TEST_CASE("TEvent: Remove finds handle regardless of which priority bucket it was added to", "[Templates][Event]")
{
    TEvent<void()> ev;
    FDelegateHandle hc = ev.Add([]() {}, EEventPriority::Critical);
    FDelegateHandle hl = ev.Add([]() {}, EEventPriority::Low);
    REQUIRE(ev.Remove(hc));
    REQUIRE(ev.Remove(hl));
    REQUIRE(ev.IsEmpty());
}

TEST_CASE("TEvent: RemoveAll removes all method bindings for a given instance", "[Templates][Event]")
{
    FListener a, b;
    TEvent<void(Int32)> ev;
    GP_MAYBE_UNUSED FDelegateHandle ha0 = ev.AddMethod(&a, &FListener::OnValue);
    GP_MAYBE_UNUSED FDelegateHandle ha1 = ev.AddMethod(&a, &FListener::OnValue);
    GP_MAYBE_UNUSED FDelegateHandle hb = ev.AddMethod(&b, &FListener::OnValue);

    Int32 removed = ev.RemoveAll(static_cast<void*>(&a));
    REQUIRE(removed == 2);
    REQUIRE(ev.GetBindingCount() == 1);

    ev.Broadcast(5);
    REQUIRE(a.GetSum() == 0);
    REQUIRE(b.GetSum() == 5);
}

TEST_CASE("TEvent: RemoveAll across priority buckets removes all matching", "[Templates][Event]")
{
    FListener listener;
    TEvent<void(Int32)> ev;
    GP_MAYBE_UNUSED FDelegateHandle hc = ev.AddMethod(&listener, &FListener::OnValue, EEventPriority::Critical);
    GP_MAYBE_UNUSED FDelegateHandle hl = ev.AddMethod(&listener, &FListener::OnValue, EEventPriority::Low);

    Int32 removed = ev.RemoveAll(static_cast<void*>(&listener));
    REQUIRE(removed == 2);
    REQUIRE(ev.IsEmpty());
}

TEST_CASE("TEvent: Clear removes all bindings from all priority buckets", "[Templates][Event]")
{
    TEvent<void()> ev;
    GP_MAYBE_UNUSED FDelegateHandle h0 = ev.Add([]() {}, EEventPriority::Critical);
    GP_MAYBE_UNUSED FDelegateHandle h1 = ev.Add([]() {}, EEventPriority::High);
    GP_MAYBE_UNUSED FDelegateHandle h2 = ev.Add([]() {}, EEventPriority::Normal);
    GP_MAYBE_UNUSED FDelegateHandle h3 = ev.Add([]() {}, EEventPriority::Low);
    REQUIRE(ev.GetBindingCount() == 4);

    ev.Clear();
    REQUIRE(ev.IsEmpty());
    REQUIRE(ev.GetBindingCount() == 0);
}

TEST_CASE("TEvent: operator+= adds callable at Normal priority", "[Templates][Event]")
{
    Int32 count = 0;
    TEvent<void()> ev;
    ev += [&count]() { ++count; };
    REQUIRE(ev.GetBindingCount() == 1);
    ev.Broadcast();
    REQUIRE(count == 1);
}

TEST_CASE("TEvent: operator-= with handle removes that binding", "[Templates][Event]")
{
    Int32 count = 0;
    TEvent<void()> ev;
    FDelegateHandle h = ev.Add([&count]() { ++count; });
    ev -= h;
    ev.Broadcast();
    REQUIRE(count == 0);
}

TEST_CASE("TEvent: operator-= with raw instance removes all method bindings for that instance", "[Templates][Event]")
{
    FListener listener;
    TEvent<void(Int32)> ev;
    GP_MAYBE_UNUSED FDelegateHandle h0 = ev.AddMethod(&listener, &FListener::OnValue);
    GP_MAYBE_UNUSED FDelegateHandle h1 = ev.AddMethod(&listener, &FListener::OnValue);
    ev -= static_cast<void*>(&listener);
    REQUIRE(ev.IsEmpty());
}

TEST_CASE("TEvent: operator() broadcasts the event", "[Templates][Event]")
{
    Int32 count = 0;
    TEvent<void()> ev;
    GP_MAYBE_UNUSED FDelegateHandle h = ev.Add([&count]() { ++count; });
    ev();
    REQUIRE(count == 1);
}

TEST_CASE("TEvent: AddScoped returns a valid FScopedDelegateHandle", "[Templates][Event]")
{
    TEvent<void()> ev;
    FScopedDelegateHandle scoped = ev.AddScoped([]() {});
    REQUIRE(scoped.IsValid());
    REQUIRE(ev.GetBindingCount() == 1);
}

TEST_CASE("TEvent: FScopedDelegateHandle auto-removes on destruction", "[Templates][Event]")
{
    Int32 count = 0;
    TEvent<void()> ev;

    {
        FScopedDelegateHandle scoped = ev.AddScoped([&count]() { ++count; });
        ev.Broadcast();
        REQUIRE(count == 1);
    }

    // scoped is destroyed — subscription must be removed.
    ev.Broadcast();
    REQUIRE(count == 1);
    REQUIRE(ev.IsEmpty());
}

TEST_CASE("TEvent: AddMethodScoped mutable auto-removes on destruction", "[Templates][Event]")
{
    FListener listener;
    TEvent<void(Int32)> ev;

    {
        FScopedDelegateHandle scoped = ev.AddMethodScoped(&listener, &FListener::OnValue);
        ev.Broadcast(10);
        REQUIRE(listener.GetSum() == 10);
    }

    ev.Broadcast(5);
    REQUIRE(listener.GetSum() == 10);   // not invoked after scoped destroyed
}

TEST_CASE("TEvent: AddMethodScoped const auto-removes on destruction", "[Templates][Event]")
{
    FListener listener;
    TEvent<void(Int32)> ev;

    {
        FScopedDelegateHandle scoped =
            ev.AddMethodScoped(static_cast<const FListener*>(&listener), &FListener::OnValueConst);
        ev.Broadcast(99);
        REQUIRE(listener.GetLastSeen() == 99);
    }

    ev.Broadcast(0);
    REQUIRE(listener.GetLastSeen() == 99);
}

TEST_CASE("TEvent: AddScoped with explicit priority respects that priority", "[Templates][Event]")
{
    std::vector<Int32> order;
    TEvent<void()> ev;

    FScopedDelegateHandle low = ev.AddScoped([&order]() { order.push_back(2); }, EEventPriority::Low);
    FScopedDelegateHandle crit = ev.AddScoped([&order]() { order.push_back(1); }, EEventPriority::Critical);

    ev.Broadcast();
    REQUIRE(order.size() == 2);
    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
}

TEST_CASE("TEvent: Remove during Broadcast is deferred and takes effect next broadcast", "[Templates][Event]")
{
    Int32 aCount = 0, bCount = 0;
    TEvent<void()> ev;
    FDelegateHandle hb;

    GP_MAYBE_UNUSED FDelegateHandle ha = ev.Add(
        [&ev, &hb, &aCount]()
        {
            ++aCount;
            ev.Remove(hb);
        }
    );
    hb = ev.Add([&bCount]() { ++bCount; });

    // First broadcast: B is still in the snapshot, so both fire.
    ev.Broadcast();
    REQUIRE(aCount == 1);
    REQUIRE(bCount == 1);

    // Second broadcast: B has been swept, only A fires.
    ev.Broadcast();
    REQUIRE(aCount == 2);
    REQUIRE(bCount == 1);
}

TEST_CASE("TEvent: Add during Broadcast is not invoked in the same cycle", "[Templates][Event]")
{
    Int32 addedCount = 0;
    TEvent<void()> ev;
    bool hasDynamicBinding = false;
    FDelegateHandle dynamicHandle;

    GP_MAYBE_UNUSED FDelegateHandle ha = ev.Add(
        [&ev, &addedCount, &hasDynamicBinding, &dynamicHandle]()
        {
            if (!hasDynamicBinding)
            {
                hasDynamicBinding = true;
                dynamicHandle = ev.Add([&addedCount]() { ++addedCount; });
            }
        }
    );

    ev.Broadcast();
    REQUIRE(addedCount == 0);   // the dynamically-added binding was not invoked

    ev.Broadcast();
    REQUIRE(addedCount == 1);   // it fires on the next broadcast
}

TEST_CASE("TEvent: is move-constructible and move-assignable", "[Templates][Event]")
{
    STATIC_REQUIRE(std::is_move_constructible_v<TEvent<void()>>);
    STATIC_REQUIRE(std::is_move_assignable_v<TEvent<void()>>);
}

TEST_CASE("TEvent: is not copy-constructible or copy-assignable", "[Templates][Event]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<TEvent<void()>>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<TEvent<void()>>);
}

TEST_CASE("TEvent: move-construct transfers bindings to new instance", "[Templates][Event]")
{
    Int32 count = 0;
    TEvent<void()> a;
    GP_MAYBE_UNUSED FDelegateHandle h = a.Add([&count]() { ++count; });

    TEvent<void()> b = GP::Move(a);
    REQUIRE(b.GetBindingCount() == 1);
    b.Broadcast();
    REQUIRE(count == 1);
}

TEST_CASE("TEvent: GetView returns a valid TEventView bound to the source", "[Templates][Event]")
{
    TEvent<void()> ev;
    GP_MAYBE_UNUSED TEventView<void()> view = ev.GetView();
    REQUIRE(view.IsValid());
}

TEST_CASE("TEventView: default-constructed view is not valid", "[Templates][EventView]")
{
    TEventView<void()> view;
    REQUIRE_FALSE(view.IsValid());
}

TEST_CASE("TEventView: default-constructed view IsEmpty returns true", "[Templates][EventView]")
{
    TEventView<void()> view;
    REQUIRE(view.IsEmpty());
}

TEST_CASE("TEventView: default-constructed view GetBindingCount returns 0", "[Templates][EventView]")
{
    TEventView<void()> view;
    REQUIRE(view.GetBindingCount() == 0);
}

TEST_CASE("TEventView: default-constructed view Contains returns false", "[Templates][EventView]")
{
    TEventView<void()> view;
    REQUIRE_FALSE(view.Contains(FDelegateHandle::Generate()));
}

TEST_CASE("TEventView: IsEmpty and GetBindingCount reflect source state", "[Templates][EventView]")
{
    TEvent<void()> ev;
    TEventView<void()> view = ev.GetView();

    REQUIRE(view.IsEmpty());
    REQUIRE(view.GetBindingCount() == 0);

    GP_MAYBE_UNUSED FDelegateHandle h = ev.Add([]() {});
    REQUIRE_FALSE(view.IsEmpty());
    REQUIRE(view.GetBindingCount() == 1);
}

TEST_CASE("TEventView: Add subscribes and returns a valid handle", "[Templates][EventView]")
{
    TEvent<void(Int32)> ev;
    TEventView<void(Int32)> view = ev.GetView();

    Int32 seen = 0;
    FDelegateHandle h = view.Add([&seen](Int32 v) { seen = v; });
    REQUIRE(h.IsValid());
    ev.Broadcast(7);
    REQUIRE(seen == 7);
}

TEST_CASE("TEventView: AddScoped auto-removes when handle is destroyed", "[Templates][EventView]")
{
    Int32 count = 0;
    TEvent<void()> ev;
    TEventView<void()> view = ev.GetView();

    {
        FScopedDelegateHandle scoped = view.AddScoped([&count]() { ++count; });
        ev.Broadcast();
        REQUIRE(count == 1);
    }

    ev.Broadcast();
    REQUIRE(count == 1);
    REQUIRE(view.IsEmpty());
}

TEST_CASE("TEventView: AddMethod mutable subscribes correctly", "[Templates][EventView]")
{
    FListener listener;
    TEvent<void(Int32)> ev;
    TEventView<void(Int32)> view = ev.GetView();

    GP_MAYBE_UNUSED FDelegateHandle h = view.AddMethod(&listener, &FListener::OnValue);
    ev.Broadcast(20);
    REQUIRE(listener.GetSum() == 20);
}

TEST_CASE("TEventView: AddMethod const subscribes correctly", "[Templates][EventView]")
{
    FListener listener;
    TEvent<void(Int32)> ev;
    TEventView<void(Int32)> view = ev.GetView();

    GP_MAYBE_UNUSED FDelegateHandle h =
        view.AddMethod(static_cast<const FListener*>(&listener), &FListener::OnValueConst);
    ev.Broadcast(55);
    REQUIRE(listener.GetLastSeen() == 55);
}

TEST_CASE("TEventView: AddMethodScoped mutable auto-removes on destruction", "[Templates][EventView]")
{
    FListener listener;
    TEvent<void(Int32)> ev;
    TEventView<void(Int32)> view = ev.GetView();

    {
        FScopedDelegateHandle scoped = view.AddMethodScoped(&listener, &FListener::OnValue);
        ev.Broadcast(3);
        REQUIRE(listener.GetSum() == 3);
    }

    ev.Broadcast(10);
    REQUIRE(listener.GetSum() == 3);
}

TEST_CASE("TEventView: AddMethodScoped const auto-removes on destruction", "[Templates][EventView]")
{
    FListener listener;
    TEvent<void(Int32)> ev;
    TEventView<void(Int32)> view = ev.GetView();

    {
        FScopedDelegateHandle scoped =
            view.AddMethodScoped(static_cast<const FListener*>(&listener), &FListener::OnValueConst);
        ev.Broadcast(11);
        REQUIRE(listener.GetLastSeen() == 11);
    }

    ev.Broadcast(99);
    REQUIRE(listener.GetLastSeen() == 11);
}

TEST_CASE("TEventView: AddFunction subscribes a free function", "[Templates][EventView]")
{
    g_freeFnCount = 0;
    TEvent<void(Int32)> ev;
    TEventView<void(Int32)> view = ev.GetView();

    FDelegateHandle h = view.AddFunction(&FreeFnVoid);
    REQUIRE(h.IsValid());
    ev.Broadcast(4);
    REQUIRE(g_freeFnCount == 4);
}

TEST_CASE("TEventView: Add with explicit priority is respected", "[Templates][EventView]")
{
    std::vector<Int32> order;
    TEvent<void()> ev;
    TEventView<void()> view = ev.GetView();

    GP_MAYBE_UNUSED FDelegateHandle hl = view.Add([&order]() { order.push_back(2); }, EEventPriority::Low);
    GP_MAYBE_UNUSED FDelegateHandle hc = view.Add([&order]() { order.push_back(1); }, EEventPriority::Critical);

    ev.Broadcast();
    REQUIRE(order.size() == 2);
    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
}

TEST_CASE("TEventView: Remove returns true and stops invocations", "[Templates][EventView]")
{
    Int32 count = 0;
    TEvent<void()> ev;
    TEventView<void()> view = ev.GetView();

    FDelegateHandle h = view.Add([&count]() { ++count; });
    ev.Broadcast();
    REQUIRE(count == 1);

    bool removed = view.Remove(h);
    REQUIRE(removed);
    ev.Broadcast();
    REQUIRE(count == 1);
}

TEST_CASE("TEventView: Remove returns false for invalid handle", "[Templates][EventView]")
{
    TEvent<void()> ev;
    TEventView<void()> view = ev.GetView();
    REQUIRE_FALSE(view.Remove(FDelegateHandle{}));
}

TEST_CASE("TEventView: RemoveAll removes all bindings for an instance", "[Templates][EventView]")
{
    FListener listener;
    TEvent<void(Int32)> ev;
    TEventView<void(Int32)> view = ev.GetView();

    GP_MAYBE_UNUSED FDelegateHandle h0 = view.AddMethod(&listener, &FListener::OnValue);
    GP_MAYBE_UNUSED FDelegateHandle h1 = view.AddMethod(&listener, &FListener::OnValue);
    REQUIRE(view.GetBindingCount() == 2);

    Int32 removed = view.RemoveAll(static_cast<void*>(&listener));
    REQUIRE(removed == 2);
    REQUIRE(view.IsEmpty());
}

TEST_CASE("TEventView: Contains returns true for live binding and false after removal", "[Templates][EventView]")
{
    TEvent<void()> ev;
    TEventView<void()> view = ev.GetView();

    FDelegateHandle h = view.Add([]() {});
    REQUIRE(view.Contains(h));

    view.Remove(h);
    REQUIRE_FALSE(view.Contains(h));
}

TEST_CASE("TEventView: operator+= adds at Normal priority", "[Templates][EventView]")
{
    Int32 count = 0;
    TEvent<void()> ev;
    TEventView<void()> view = ev.GetView();

    view += [&count]() { ++count; };
    REQUIRE(view.GetBindingCount() == 1);
    ev.Broadcast();
    REQUIRE(count == 1);
}

TEST_CASE("TEventView: operator-= with handle removes that binding", "[Templates][EventView]")
{
    Int32 count = 0;
    TEvent<void()> ev;
    TEventView<void()> view = ev.GetView();

    FDelegateHandle h = view.Add([&count]() { ++count; });
    view -= h;
    ev.Broadcast();
    REQUIRE(count == 0);
}

TEST_CASE("TEventView: operator-= with raw instance removes all method bindings", "[Templates][EventView]")
{
    FListener listener;
    TEvent<void(Int32)> ev;
    TEventView<void(Int32)> view = ev.GetView();

    GP_UNUSED(view.AddMethod(&listener, &FListener::OnValue));
    GP_UNUSED(view.AddMethod(&listener, &FListener::OnValue));
    view -= static_cast<void*>(&listener);
    REQUIRE(view.IsEmpty());
}

TEST_CASE("TEventView: copied view refers to the same source", "[Templates][EventView]")
{
    Int32 count = 0;
    TEvent<void()> ev;
    TEventView<void()> viewA = ev.GetView();
    TEventView<void()> viewB = viewA;   // copy

    GP_MAYBE_UNUSED FDelegateHandle h = viewA.Add([&count]() { ++count; });
    REQUIRE(viewB.GetBindingCount() == 1);
    ev.Broadcast();
    REQUIRE(count == 1);
}
