// Copyright (c) - Graphical Playground. All rights reserved.

#include "Templates/Events/MulticastDelegate.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

namespace
{

// A sample class with mutable and const member functions

class FCounter
{
public:
    void Increment(GP::Int32 amount) { m_count += amount; }

    void Reset() { m_count = 0; }

    GP::Int32 GetCount() const { return m_count; }

    GP::Int32 Add(GP::Int32 a, GP::Int32 b) { return a + b; }

    void OnEvent(GP::Int32 value) const { m_lastSeen = value; }

    GP::Int32 GetLastSeen() const { return m_lastSeen; }

private:
    GP::Int32 m_count = 0;
    mutable GP::Int32 m_lastSeen = -1;
};

}   // anonymous namespace

TEST_CASE("TMulticastDelegate - starts empty", "[Templates][MulticastDelegate]")
{
    GP::TMulticastDelegate<void(GP::Int32)> md;
    REQUIRE(md.IsEmpty());
    REQUIRE(md.GetBindingCount() == 0);
}

TEST_CASE("TMulticastDelegate - Add lambda, Broadcast invokes it", "[Templates][MulticastDelegate]")
{
    GP::Int32 seen = 0;
    GP::TMulticastDelegate<void(GP::Int32)> md;

    GP::FDelegateHandle h = md.Add([&seen](GP::Int32 v) { seen = v; });
    REQUIRE(h.IsValid());
    REQUIRE(md.GetBindingCount() == 1);

    md.Broadcast(42);
    REQUIRE(seen == 42);
}

TEST_CASE("TMulticastDelegate - multiple bindings all invoked in order", "[Templates][MulticastDelegate]")
{
    std::vector<GP::Int32> order;
    GP::TMulticastDelegate<void()> md;

    md.Add([&order]() { order.push_back(1); });
    md.Add([&order]() { order.push_back(2); });
    md.Add([&order]() { order.push_back(3); });

    md.Broadcast();

    REQUIRE(order.size() == 3);
    REQUIRE(order[0] == 1);
    REQUIRE(order[1] == 2);
    REQUIRE(order[2] == 3);
}

TEST_CASE("TMulticastDelegate - Remove stops future invocations", "[Templates][MulticastDelegate]")
{
    GP::Int32 aCount = 0, bCount = 0;
    GP::TMulticastDelegate<void()> md;

    GP::FDelegateHandle ha = md.Add([&aCount]() { ++aCount; });
    GP::FDelegateHandle hb = md.Add([&bCount]() { ++bCount; });

    md.Broadcast();
    REQUIRE(aCount == 1);
    REQUIRE(bCount == 1);

    md.Remove(ha);
    md.Broadcast();
    REQUIRE(aCount == 1);   // not invoked after remove
    REQUIRE(bCount == 2);
}

TEST_CASE("TMulticastDelegate - Remove with invalid handle is a no-op", "[Templates][MulticastDelegate]")
{
    GP::TMulticastDelegate<void()> md;
    md.Add([]() {});

    bool removed = md.Remove(GP::FDelegateHandle{});   // invalid handle
    REQUIRE_FALSE(removed);
    REQUIRE(md.GetBindingCount() == 1);
}

TEST_CASE("TMulticastDelegate - Contains reflects active bindings", "[Templates][MulticastDelegate]")
{
    GP::TMulticastDelegate<void()> md;
    GP::FDelegateHandle h = md.Add([]() {});

    REQUIRE(md.Contains(h));

    md.Remove(h);
    REQUIRE_FALSE(md.Contains(h));
}

TEST_CASE("TMulticastDelegate - Clear removes all bindings", "[Templates][MulticastDelegate]")
{
    GP::TMulticastDelegate<void()> md;
    md.Add([]() {});
    md.Add([]() {});
    md.Add([]() {});
    REQUIRE(md.GetBindingCount() == 3);

    md.Clear();
    REQUIRE(md.IsEmpty());
    REQUIRE(md.GetBindingCount() == 0);
}

TEST_CASE("TMulticastDelegate - AddMethod (mutable) invoked correctly", "[Templates][MulticastDelegate]")
{
    FCounter c;
    GP::TMulticastDelegate<void(GP::Int32)> md;
    md.AddMethod(&c, &FCounter::Increment);

    md.Broadcast(10);
    md.Broadcast(5);
    REQUIRE(c.GetCount() == 15);
}

TEST_CASE("TMulticastDelegate - AddMethod (const) invoked correctly", "[Templates][MulticastDelegate]")
{
    FCounter c;
    GP::TMulticastDelegate<void(GP::Int32)> md;
    md.AddMethod(static_cast<const FCounter*>(&c), &FCounter::OnEvent);

    md.Broadcast(77);
    REQUIRE(c.GetLastSeen() == 77);
}

TEST_CASE(
    "TMulticastDelegate - RemoveAll removes only bindings for the given instance", "[Templates][MulticastDelegate]"
)
{
    FCounter c1, c2;
    GP::Int32 lambdaCount = 0;

    GP::TMulticastDelegate<void(GP::Int32)> md;
    md.AddMethod(&c1, &FCounter::Increment);
    md.AddMethod(&c1, &FCounter::Increment);                // two bindings for c1
    md.AddMethod(&c2, &FCounter::Increment);                // one binding for c2
    md.Add([&lambdaCount](GP::Int32) { ++lambdaCount; });   // lambda (no owner)

    REQUIRE(md.GetBindingCount() == 4);

    GP::Int32 removed = md.RemoveAll(static_cast<void*>(&c1));
    REQUIRE(removed == 2);
    REQUIRE(md.GetBindingCount() == 2);

    md.Broadcast(1);
    REQUIRE(c1.GetCount() == 0);   // c1 no longer receives
    REQUIRE(c2.GetCount() == 1);   // c2 still receives
    REQUIRE(lambdaCount == 1);     // lambda still receives
}

TEST_CASE("TMulticastDelegate - Remove during Broadcast is safe (deferred)", "[Templates][MulticastDelegate]")
{
    GP::Int32 aCount = 0, bCount = 0;
    GP::TMulticastDelegate<void()> md;
    GP::FDelegateHandle hb;

    // Binding A: removes binding B during broadcast.
    md.Add(
        [&md, &hb, &aCount]()
        {
            ++aCount;
            md.Remove(hb);
        }
    );

    hb = md.Add([&bCount]() { ++bCount; });

    // First broadcast: both A and B were active at the start of this cycle,
    // so B is still invoked (snapshot semantics). A then removes B.
    md.Broadcast();
    REQUIRE(aCount == 1);
    REQUIRE(bCount == 1);

    // Second broadcast: B has been removed; only A fires.
    md.Broadcast();
    REQUIRE(aCount == 2);
    REQUIRE(bCount == 1);
}

TEST_CASE(
    "TMulticastDelegate - Add during Broadcast is safe (excluded from current cycle)", "[Templates][MulticastDelegate]"
)
{
    GP::Int32 existingCount = 0, newCount = 0;
    GP::TMulticastDelegate<void()> md;

    // Existing binding adds a new binding mid-broadcast.
    md.Add(
        [&md, &existingCount, &newCount]()
        {
            ++existingCount;
            // Add a new binding; it must NOT fire in this cycle.
            md.Add([&newCount]() { ++newCount; });
        }
    );

    md.Broadcast();   // cycle 1: existing fires, new is enqueued but not invoked
    REQUIRE(existingCount == 1);
    REQUIRE(newCount == 0);

    md.Broadcast();   // cycle 2: both existing and new fire
    REQUIRE(existingCount == 2);
    REQUIRE(newCount == 1);
}

TEST_CASE("TMulticastDelegate - self-remove during Broadcast does not corrupt state", "[Templates][MulticastDelegate]")
{
    GP::Int32 callCount = 0;
    GP::TMulticastDelegate<void()> md;
    GP::FDelegateHandle self;

    self = md.Add(
        [&md, &self, &callCount]()
        {
            ++callCount;
            md.Remove(self);   // self-remove
        }
    );

    md.Broadcast();
    REQUIRE(callCount == 1);
    REQUIRE(md.IsEmpty());

    // Second broadcast should not call the removed binding.
    md.Broadcast();
    REQUIRE(callCount == 1);
}
