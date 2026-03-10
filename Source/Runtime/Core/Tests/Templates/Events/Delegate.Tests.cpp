// Copyright (c) - Graphical Playground. All rights reserved.

#include "Templates/Events/Delegate.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

namespace
{

// Free functions

static GP::Int32 g_freeFnCallCount = 0;

void FreeFnVoid(GP::Int32 value) { g_freeFnCallCount += value; }

GP::Int32 FreeFnReturn(GP::Int32 a, GP::Int32 b) { return a + b; }

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

// Large functor that will NOT fit in SBO (> 32 bytes)

struct FLargeFunctor
{
    // 64 bytes of payload to force heap allocation
    GP::Byte payload[64]{};
    GP::Int32 callCount = 0;

    void operator()(GP::Int32) { ++callCount; }
};

static_assert(
    sizeof(FLargeFunctor) > Detail::kSBOCapacity, "FLargeFunctor must exceed SBO capacity to test heap fallback path."
);

}   // anonymous namespace

TEST_CASE("TDelegate - BindFunction / Execute (void return)", "[Templates][Delegate]")
{
    g_freeFnCallCount = 0;

    GP::TDelegate<void(GP::Int32)> d;
    REQUIRE_FALSE(d.IsBound());

    d.BindFunction(&FreeFnVoid);
    REQUIRE(d.IsBound());

    d.Execute(3);
    REQUIRE(g_freeFnCallCount == 3);

    d.Execute(7);
    REQUIRE(g_freeFnCallCount == 10);
}

TEST_CASE("TDelegate - CreateFromFunction factory (non-void return)", "[Templates][Delegate]")
{
    auto d = GP::TDelegate<GP::Int32(GP::Int32, GP::Int32)>::CreateFromFunction(&FreeFnReturn);
    REQUIRE(d.IsBound());
    REQUIRE(d.Execute(4, 6) == 10);
}

TEST_CASE("TDelegate - ExecuteIfBound returns false when unbound", "[Templates][Delegate]")
{
    GP::TDelegate<void(GP::Int32)> d;
    bool invoked = d.ExecuteIfBound(99);
    REQUIRE_FALSE(invoked);
}

TEST_CASE("TDelegate - ExecuteIfBound returns true and invokes when bound", "[Templates][Delegate]")
{
    GP::Int32 seen = 0;
    GP::TDelegate<void(GP::Int32)> d;
    d.BindLambda([&seen](GP::Int32 v) { seen = v; });

    bool invoked = d.ExecuteIfBound(42);
    REQUIRE(invoked);
    REQUIRE(seen == 42);
}

TEST_CASE("TDelegate - BindMethod (mutable member function)", "[Templates][Delegate]")
{
    FCounter counter;
    GP::TDelegate<void(GP::Int32)> d;
    d.BindMethod(&counter, &FCounter::Increment);

    d.Execute(5);
    d.Execute(3);
    REQUIRE(counter.GetCount() == 8);
}

TEST_CASE("TDelegate - BindMethod (const member function)", "[Templates][Delegate]")
{
    FCounter counter;
    GP::TDelegate<void(GP::Int32)> d;
    d.BindMethod(static_cast<const FCounter*>(&counter), &FCounter::OnEvent);

    d.Execute(77);
    REQUIRE(counter.GetLastSeen() == 77);
}

TEST_CASE("TDelegate - CreateFromMethod factory (non-void return)", "[Templates][Delegate]")
{
    FCounter counter;
    auto d = GP::TDelegate<GP::Int32(GP::Int32, GP::Int32)>::CreateFromMethod(&counter, &FCounter::Add);

    REQUIRE(d.Execute(10, 20) == 30);
}

TEST_CASE("TDelegate - BindLambda capturing local state", "[Templates][Delegate]")
{
    GP::Int32 accumulator = 0;
    GP::TDelegate<void(GP::Int32)> d;
    d.BindLambda([&accumulator](GP::Int32 v) { accumulator += v; });

    d.Execute(10);
    d.Execute(5);
    REQUIRE(accumulator == 15);
}

TEST_CASE("TDelegate - large functor uses heap path and remains functional", "[Templates][Delegate]")
{
    FLargeFunctor functor;
    GP::TDelegate<void(GP::Int32)> d;
    d.BindCallable(functor);   // copy into delegate

    d.Execute(1);
    // The original functor is unchanged (bind copies); delegate has its own copy.
    REQUIRE(functor.callCount == 0);
}

TEST_CASE("TDelegate - copy constructor propagates binding", "[Templates][Delegate]")
{
    GP::Int32 value = 0;
    GP::TDelegate<void(GP::Int32)> original;
    original.BindLambda([&value](GP::Int32 v) { value = v; });

    GP::TDelegate<void(GP::Int32)> copy = original;
    REQUIRE(copy.IsBound());

    copy.Execute(99);
    REQUIRE(value == 99);
}

TEST_CASE("TDelegate - move constructor transfers binding", "[Templates][Delegate]")
{
    GP::Int32 value = 0;
    GP::TDelegate<void(GP::Int32)> original;
    original.BindLambda([&value](GP::Int32 v) { value = v; });

    GP::TDelegate<void(GP::Int32)> moved = std::move(original);
    REQUIRE(moved.IsBound());
    REQUIRE_FALSE(original.IsBound());

    moved.Execute(55);
    REQUIRE(value == 55);
}

TEST_CASE("TDelegate - copy of large (heap) functor is independent", "[Templates][Delegate]")
{
    GP::Int32 callsA = 0, callsB = 0;

    GP::TDelegate<void(GP::Int32)> a;
    a.BindLambda([&callsA](GP::Int32) { ++callsA; });

    GP::TDelegate<void(GP::Int32)> b = a;   // copy
    // Rebind b to a different lambda; a must be unaffected.
    b.BindLambda([&callsB](GP::Int32) { ++callsB; });

    a.Execute(0);
    b.Execute(0);

    REQUIRE(callsA == 1);
    REQUIRE(callsB == 1);
}

TEST_CASE("TDelegate - Unbind clears the binding", "[Templates][Delegate]")
{
    GP::TDelegate<void(GP::Int32)> d;
    d.BindLambda([](GP::Int32) {});
    REQUIRE(d.IsBound());

    d.Unbind();
    REQUIRE_FALSE(d.IsBound());
}
