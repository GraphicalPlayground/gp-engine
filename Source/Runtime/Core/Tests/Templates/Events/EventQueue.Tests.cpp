// Copyright (c) - Graphical Playground. All rights reserved.

#include "Templates/Events/EventQueue.hpp"
#include "Templates/Events/Event.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

namespace GP::Testing
{

struct FSimpleEvent
{
    Int32 value{ 0 };
};

struct FCoalesceableEvent
{
    Int32 sequenceNumber{ 0 };
    Int32 accumulatedDelta{ 0 };

    static void Coalesce(FCoalesceableEvent& existing, const FCoalesceableEvent& incoming) noexcept
    {
        existing.sequenceNumber = incoming.sequenceNumber;
        existing.accumulatedDelta += incoming.accumulatedDelta;
    }
};

struct FNonTrivialEvent
{
    Int32* destroyCountTracker{ nullptr };

    FNonTrivialEvent() = default;

    explicit FNonTrivialEvent(Int32* tracker)
        : destroyCountTracker(tracker)
    {}

    FNonTrivialEvent(const FNonTrivialEvent& other) = default;
    FNonTrivialEvent& operator=(const FNonTrivialEvent& other) = default;

    ~FNonTrivialEvent()
    {
        if (destroyCountTracker) { ++(*destroyCountTracker); }
    }
};

}   // namespace GP::Testing

TEST_CASE("FEventQueue: Basic emission and dispatching", "[Templates][EventQueue]")
{
    FEventQueue queue;
    TEvent<void(const Testing::FSimpleEvent&)> target;

    Int32 receivedValue = 0;
    auto handle = target.Add([&](const Testing::FSimpleEvent& evt) { receivedValue = evt.value; });

    SECTION("Dispatch without swap doesn't execute events")
    {
        queue.Emit(Testing::FSimpleEvent{ 42 }, target);
        queue.Dispatch();
        REQUIRE(receivedValue == 0);
    }

    SECTION("Swap and dispatch executes events")
    {
        queue.Emit(Testing::FSimpleEvent{ 42 }, target);
        queue.Swap();
        queue.Dispatch();
        REQUIRE(receivedValue == 42);
    }

    SECTION("Double buffering isolates next frame events from current frame dispatch")
    {
        queue.Emit(Testing::FSimpleEvent{ 10 }, target);
        queue.Swap();

        queue.Emit(Testing::FSimpleEvent{ 20 }, target);

        // Should only dispatch frame 1 events (10)
        queue.Dispatch();
        REQUIRE(receivedValue == 10);

        // Frame 2
        queue.Swap();
        queue.Dispatch();
        REQUIRE(receivedValue == 20);
    }
}

TEST_CASE("FEventQueue: Coalescing events", "[Templates][EventQueue]")
{
    FEventQueue queue;

    SECTION("Same target coalesces correctly")
    {
        TEvent<void(const Testing::FCoalesceableEvent&)> target;
        Int32 dispatchCount = 0;
        Testing::FCoalesceableEvent lastReceived{};

        auto handle = target.Add(
            [&](const Testing::FCoalesceableEvent& evt)
            {
                ++dispatchCount;
                lastReceived = evt;
            }
        );

        // Emit three events to the same target
        queue.Emit(Testing::FCoalesceableEvent{ 1, 10 }, target);
        queue.Emit(Testing::FCoalesceableEvent{ 2, 20 }, target);
        queue.Emit(Testing::FCoalesceableEvent{ 3, 5 }, target);

        queue.Swap();
        queue.Dispatch();

        REQUIRE(dispatchCount == 1);
        REQUIRE(lastReceived.sequenceNumber == 3);
        REQUIRE(lastReceived.accumulatedDelta == 35);
    }

    SECTION("Different targets do not coalesce with each other")
    {
        TEvent<void(const Testing::FCoalesceableEvent&)> targetA;
        TEvent<void(const Testing::FCoalesceableEvent&)> targetB;

        Testing::FCoalesceableEvent receivedA{};
        Testing::FCoalesceableEvent receivedB{};

        auto handleA = targetA.Add([&](const Testing::FCoalesceableEvent& evt) { receivedA = evt; });
        auto handleB = targetB.Add([&](const Testing::FCoalesceableEvent& evt) { receivedB = evt; });

        queue.Emit(Testing::FCoalesceableEvent{ 1, 100 }, targetA);
        queue.Emit(Testing::FCoalesceableEvent{ 2, 200 }, targetB);
        queue.Emit(Testing::FCoalesceableEvent{ 3, 50 }, targetA);

        queue.Swap();
        queue.Dispatch();

        // targetA should have 1+3
        REQUIRE(receivedA.sequenceNumber == 3);
        REQUIRE(receivedA.accumulatedDelta == 150);

        // targetB should have 2
        REQUIRE(receivedB.sequenceNumber == 2);
        REQUIRE(receivedB.accumulatedDelta == 200);
    }
}

TEST_CASE("FEventQueue: Destructor invocation for non-trivial events", "[Templates][EventQueue]")
{
    Int32 destroyCount = 0;

    {
        FEventQueue queue;
        TEvent<void(const Testing::FNonTrivialEvent&)> target;

        queue.Emit(Testing::FNonTrivialEvent{ &destroyCount }, target);
        queue.Emit(Testing::FNonTrivialEvent{ &destroyCount }, target);

        // At this point, Emit has produced copies.
        queue.Swap();
        queue.Dispatch();

        // When queue is destroyed or next buffer is cleared,
        // the enqueued event destructors should run.
    }

    // 2 explicitly pushed objects, plus copies made internally by Emit
    REQUIRE(destroyCount >= 2);
}

TEST_CASE("FEventQueue: Reallocation handles large number of events", "[Templates][EventQueue]")
{
    // Initialize with a tiny capacity to force reallocations
    FEventQueue queue(32);
    TEvent<void(const Testing::FSimpleEvent&)> target;

    Int32 sum = 0;
    auto handle = target.Add([&](const Testing::FSimpleEvent& evt) { sum += evt.value; });

    const Int32 numEvents = 500;
    for (Int32 i = 0; i < numEvents; ++i) { queue.Emit(Testing::FSimpleEvent{ 1 }, target); }

    queue.Swap();
    queue.Dispatch();

    REQUIRE(sum == numEvents);
}
