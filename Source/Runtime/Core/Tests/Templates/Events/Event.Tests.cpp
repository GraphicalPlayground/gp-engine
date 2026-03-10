// Copyright (c) - Graphical Playground. All rights reserved.

#include "Templates/Events/Event.hpp"
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

// Simulated "owner" for TEvent tests

class FEventOwner
{
public:
    GP::TEvent<FEventOwner, void(GP::Int32)>& OnValueChanged() { return m_onValueChanged; }

    void SetValue(GP::Int32 value)
    {
        m_value = value;
        m_onValueChanged.Broadcast(m_value);
    }

    GP::Int32 GetValue() const { return m_value; }

private:
    GP::TEvent<FEventOwner, void(GP::Int32)> m_onValueChanged;
    GP::Int32 m_value = 0;
};

}   // anonymous namespace

TEST_CASE("TEvent - subscriber can Add and receive broadcasts", "[Templates][Event]")
{
    FEventOwner owner;
    GP::Int32 seen = -1;

    GP::FDelegateHandle h = owner.OnValueChanged().Add([&seen](GP::Int32 v) { seen = v; });
    REQUIRE(h.IsValid());

    owner.SetValue(42);
    REQUIRE(seen == 42);

    owner.SetValue(100);
    REQUIRE(seen == 100);
}

TEST_CASE("TEvent - subscriber can Remove its binding", "[Templates][Event]")
{
    FEventOwner owner;
    GP::Int32 callCount = 0;

    GP::FDelegateHandle h = owner.OnValueChanged().Add([&callCount](GP::Int32) { ++callCount; });

    owner.SetValue(1);
    REQUIRE(callCount == 1);

    owner.OnValueChanged().Remove(h);

    owner.SetValue(2);
    REQUIRE(callCount == 1);   // no longer receives
}

TEST_CASE("TEvent - multiple subscribers receive broadcasts independently", "[Templates][Event]")
{
    FEventOwner owner;
    GP::Int32 a = 0, b = 0, c = 0;

    owner.OnValueChanged().Add([&a](GP::Int32 v) { a = v; });
    owner.OnValueChanged().Add([&b](GP::Int32 v) { b = v * 2; });
    owner.OnValueChanged().Add([&c](GP::Int32 v) { c = v + 1; });

    owner.SetValue(10);
    REQUIRE(a == 10);
    REQUIRE(b == 20);
    REQUIRE(c == 11);
}

TEST_CASE("TEvent - IsEmpty and GetBindingCount reflect current state", "[Templates][Event]")
{
    FEventOwner owner;
    REQUIRE(owner.OnValueChanged().IsEmpty());
    REQUIRE(owner.OnValueChanged().GetBindingCount() == 0);

    GP::FDelegateHandle h1 = owner.OnValueChanged().Add([](GP::Int32) {});
    GP::FDelegateHandle h2 = owner.OnValueChanged().Add([](GP::Int32) {});

    REQUIRE_FALSE(owner.OnValueChanged().IsEmpty());
    REQUIRE(owner.OnValueChanged().GetBindingCount() == 2);

    owner.OnValueChanged().Remove(h1);
    REQUIRE(owner.OnValueChanged().GetBindingCount() == 1);

    owner.OnValueChanged().Remove(h2);
    REQUIRE(owner.OnValueChanged().IsEmpty());
}

TEST_CASE("TEvent - AddMethod and RemoveAll work through event interface", "[Templates][Event]")
{
    FEventOwner owner;
    FCounter c;

    owner.OnValueChanged().AddMethod(&c, &FCounter::Increment);
    owner.SetValue(7);
    REQUIRE(c.GetCount() == 7);

    owner.OnValueChanged().RemoveAll(static_cast<void*>(&c));
    owner.SetValue(3);
    REQUIRE(c.GetCount() == 7);   // no further increments after RemoveAll
}
