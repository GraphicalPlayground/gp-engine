// Copyright (c) - Graphical Playground. All rights reserved.

#include "Templates/Events/DelegateHandle.hpp"
#include "Templates/Events/Event.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

TEST_CASE("FDelegateHandle - default-constructed handle is invalid", "[Templates][DelegateHandle]")
{
    GP::FDelegateHandle handle;
    REQUIRE_FALSE(handle.IsValid());
}

TEST_CASE("FDelegateHandle - Generate() produces valid handles", "[Templates][DelegateHandle]")
{
    GP::FDelegateHandle h = GP::FDelegateHandle::Generate();
    REQUIRE(h.IsValid());
}

TEST_CASE("FDelegateHandle - Generate() produces unique handles", "[Templates][DelegateHandle]")
{
    static constexpr GP::SizeT k_count = 1000;
    std::vector<GP::FDelegateHandle> handles;
    handles.reserve(k_count);

    for (GP::SizeT i = 0; i < k_count; ++i) { handles.push_back(GP::FDelegateHandle::Generate()); }

    // All handles must be distinct.
    for (GP::SizeT i = 0; i < k_count; ++i)
    {
        for (GP::SizeT j = i + 1; j < k_count; ++j) { REQUIRE(handles[i] != handles[j]); }
    }
}

TEST_CASE("FDelegateHandle - Invalidate() marks handle invalid", "[Templates][DelegateHandle]")
{
    GP::FDelegateHandle h = GP::FDelegateHandle::Generate();
    REQUIRE(h.IsValid());
    h.Invalidate();
    REQUIRE_FALSE(h.IsValid());
}

TEST_CASE("FDelegateHandle - equality operators", "[Templates][DelegateHandle]")
{
    GP::FDelegateHandle h1 = GP::FDelegateHandle::Generate();
    GP::FDelegateHandle h2 = h1;   // copy
    GP::FDelegateHandle h3 = GP::FDelegateHandle::Generate();

    REQUIRE(h1 == h2);
    REQUIRE(h1 != h3);
}

TEST_CASE("FScopedDelegateHandle: default-constructed handle is not valid", "[Templates][ScopedDelegateHandle]")
{
    FScopedDelegateHandle scoped;
    REQUIRE_FALSE(scoped.IsValid());
}

TEST_CASE("FScopedDelegateHandle: Reset on already-invalid handle is a no-op", "[Templates][ScopedDelegateHandle]")
{
    FScopedDelegateHandle scoped;
    REQUIRE_NOTHROW(scoped.Reset());
    REQUIRE_FALSE(scoped.IsValid());
}

TEST_CASE(
    "FScopedDelegateHandle: Reset removes subscription and marks handle invalid", "[Templates][ScopedDelegateHandle]"
)
{
    Int32 count = 0;
    TEvent<void()> ev;
    FScopedDelegateHandle scoped = ev.AddScoped([&count]() { ++count; });
    REQUIRE(scoped.IsValid());

    scoped.Reset();
    REQUIRE_FALSE(scoped.IsValid());

    ev.Broadcast();
    REQUIRE(count == 0);
}

TEST_CASE(
    "FScopedDelegateHandle: multiple Reset calls are safe no-ops after first", "[Templates][ScopedDelegateHandle]"
)
{
    TEvent<void()> ev;
    FScopedDelegateHandle scoped = ev.AddScoped([]() {});
    scoped.Reset();
    REQUIRE_NOTHROW(scoped.Reset());
    REQUIRE_NOTHROW(scoped.Reset());
}

TEST_CASE(
    "FScopedDelegateHandle: Release returns raw handle without unsubscribing", "[Templates][ScopedDelegateHandle]"
)
{
    Int32 count = 0;
    TEvent<void()> ev;
    FScopedDelegateHandle scoped = ev.AddScoped([&count]() { ++count; });

    FDelegateHandle raw = scoped.Release();
    REQUIRE(raw.IsValid());
    REQUIRE_FALSE(scoped.IsValid());

    ev.Broadcast();
    REQUIRE(count == 1);   // subscription still active

    ev.Remove(raw);
    ev.Broadcast();
    REQUIRE(count == 1);
}

TEST_CASE("FScopedDelegateHandle: move-construct transfers ownership", "[Templates][ScopedDelegateHandle]")
{
    Int32 count = 0;
    TEvent<void()> ev;
    FScopedDelegateHandle a = ev.AddScoped([&count]() { ++count; });
    FScopedDelegateHandle b = GP::Move(a);

    REQUIRE_FALSE(a.IsValid());
    REQUIRE(b.IsValid());

    ev.Broadcast();
    REQUIRE(count == 1);
}

TEST_CASE(
    "FScopedDelegateHandle: original is invalided after move, source not unsubscribed prematurely",
    "[Templates][ScopedDelegateHandle]"
)
{
    Int32 count = 0;
    TEvent<void()> ev;
    FScopedDelegateHandle a = ev.AddScoped([&count]() { ++count; });
    FScopedDelegateHandle b = GP::Move(a);

    // Only b owns the subscription; when b is destroyed the subscription should fire one last time.
    ev.Broadcast();
    REQUIRE(count == 1);

    b.Reset();
    ev.Broadcast();
    REQUIRE(count == 1);   // removed
}

TEST_CASE(
    "FScopedDelegateHandle: move-assign unsubscribes current before taking new ownership",
    "[Templates][ScopedDelegateHandle]"
)
{
    Int32 countA = 0, countB = 0;
    TEvent<void()> ev;

    FScopedDelegateHandle a = ev.AddScoped([&countA]() { ++countA; });
    FScopedDelegateHandle b = ev.AddScoped([&countB]() { ++countB; });

    a = GP::Move(b);   // a must unsubscribe its old binding and take b's

    ev.Broadcast();
    REQUIRE(countA == 0);   // a's original binding was removed
    REQUIRE(countB == 1);   // b's binding, now managed by a, is still active
}

TEST_CASE("FScopedDelegateHandle: is not copy-constructible or copy-assignable", "[Templates][ScopedDelegateHandle]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<FScopedDelegateHandle>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<FScopedDelegateHandle>);
}
