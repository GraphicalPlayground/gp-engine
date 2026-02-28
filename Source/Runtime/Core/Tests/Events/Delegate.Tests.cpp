// Copyright (c) - Graphical Playground. All rights reserved.

#include "Events/Delegate.hpp"
#include "Events/DelegateHandle.hpp"
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace GP;
using namespace GP::Events;

static int FreeAdd(int a, int b) { return a + b; }

static void FreeIncrement(int& value) { ++value; }

static int FreeReturnFive() { return 5; }

static void FreeVoidNoArgs() {}

struct FCounter
{
    int value = 0;

    void Increment() { ++value; }

    void Add(int n) { value += n; }

    int Get() const { return value; }

    void Reset() { value = 0; }
};

TEST_CASE("FDelegateHandle — Default Construction", "[GP][Core][Events][FDelegateHandle]")
{
    const FDelegateHandle handle;
    REQUIRE_FALSE(handle.IsValid());
    REQUIRE(handle.id == FDelegateHandle::InvalidID);
}

TEST_CASE("FDelegateHandle — Explicit Construction", "[GP][Core][Events][FDelegateHandle]")
{
    const FDelegateHandle handle{ 42ULL };
    REQUIRE(handle.IsValid());
    REQUIRE(handle.id == 42ULL);
}

TEST_CASE("FDelegateHandle — Reset", "[GP][Core][Events][FDelegateHandle]")
{
    FDelegateHandle handle{ 7ULL };
    REQUIRE(handle.IsValid());

    handle.Reset();
    REQUIRE_FALSE(handle.IsValid());
    REQUIRE(handle.id == FDelegateHandle::InvalidID);
}

TEST_CASE("FDelegateHandle — Equality Operators", "[GP][Core][Events][FDelegateHandle]")
{
    const FDelegateHandle a{ 10ULL };
    const FDelegateHandle b{ 10ULL };
    const FDelegateHandle c{ 20ULL };
    const FDelegateHandle invalid;

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
    REQUIRE(a != c);
    REQUIRE_FALSE(a != b);
    REQUIRE_FALSE(a == invalid);
}

TEST_CASE("FDelegateHandle — Less-Than Operator", "[GP][Core][Events][FDelegateHandle]")
{
    const FDelegateHandle low{ 1ULL };
    const FDelegateHandle high{ 100ULL };

    REQUIRE(low < high);
    REQUIRE_FALSE(high < low);
    REQUIRE_FALSE(low < low);
}

TEST_CASE("FDelegateHandle — Copyable Value Type", "[GP][Core][Events][FDelegateHandle]")
{
    const FDelegateHandle original{ 55ULL };
    const FDelegateHandle copy = original;   // copy-construction
    FDelegateHandle assigned;
    assigned = original;                     // copy-assignment

    REQUIRE(copy == original);
    REQUIRE(assigned == original);
}

TEST_CASE("FDelegateHandle — Generate Produces Unique IDs", "[GP][Core][Events][FDelegateHandle]")
{
    const FDelegateHandle h1 = FDelegateHandle::Generate();
    const FDelegateHandle h2 = FDelegateHandle::Generate();
    const FDelegateHandle h3 = FDelegateHandle::Generate();

    REQUIRE(h1.IsValid());
    REQUIRE(h2.IsValid());
    REQUIRE(h3.IsValid());
    REQUIRE(h1 != h2);
    REQUIRE(h2 != h3);
    REQUIRE(h1 != h3);
}

TEST_CASE("FDelegateHandle — Generate Never Returns InvalidID", "[GP][Core][Events][FDelegateHandle]")
{
    for (int i = 0; i < 1000; ++i)
    {
        const FDelegateHandle h = FDelegateHandle::Generate();
        REQUIRE(h.IsValid());
        REQUIRE(h.id != FDelegateHandle::InvalidID);
    }
}

TEST_CASE("FDelegateHandle — std::hash Specialization", "[GP][Core][Events][FDelegateHandle]")
{
    SECTION("Hash of valid handles")
    {
        std::hash<FDelegateHandle> hasher;
        const FDelegateHandle h1{ 10ULL };
        const FDelegateHandle h2{ 10ULL };
        const FDelegateHandle h3{ 99ULL };

        REQUIRE(hasher(h1) == hasher(h2));   // same id → same hash
        REQUIRE(hasher(h1) != hasher(h3));   // different id → (likely) different hash
    }

    SECTION("Usable as unordered_map key")
    {
        std::unordered_map<FDelegateHandle, int> map;
        const FDelegateHandle h1 = FDelegateHandle::Generate();
        const FDelegateHandle h2 = FDelegateHandle::Generate();

        map[h1] = 42;
        map[h2] = 99;

        REQUIRE(map.at(h1) == 42);
        REQUIRE(map.at(h2) == 99);
    }

    SECTION("Usable as unordered_set key")
    {
        std::unordered_set<FDelegateHandle> set;
        const FDelegateHandle h = FDelegateHandle::Generate();
        set.insert(h);
        REQUIRE(set.count(h) == 1);
    }
}

TEST_CASE("TDelegate — Default Construction", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int(int, int)> d;
    REQUIRE_FALSE(d.IsBound());
}

TEST_CASE("TDelegate — Move Construction", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int()> d1;
    d1.BindLambda([]() { return 42; });
    REQUIRE(d1.IsBound());

    TDelegate<int()> d2 = std::move(d1);
    REQUIRE_FALSE(d1.IsBound());
    REQUIRE(d2.IsBound());
    REQUIRE(d2.Execute() == 42);
}

TEST_CASE("TDelegate — Move Assignment", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int()> d1;
    d1.BindLambda([]() { return 10; });

    TDelegate<int()> d2;
    d2.BindLambda([]() { return 20; });

    d2 = std::move(d1);

    REQUIRE_FALSE(d1.IsBound());
    REQUIRE(d2.IsBound());
    REQUIRE(d2.Execute() == 10);
}

TEST_CASE("TDelegate — BindStatic", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int(int, int)> d;
    d.BindStatic(&FreeAdd);

    REQUIRE(d.IsBound());
    REQUIRE(d.Execute(3, 4) == 7);
}

TEST_CASE("TDelegate — BindStatic void", "[GP][Core][Events][TDelegate]")
{
    int value = 0;
    TDelegate<void(int&)> d;
    d.BindStatic(&FreeIncrement);

    REQUIRE(d.IsBound());
    d.Execute(value);
    REQUIRE(value == 1);
}

TEST_CASE("TDelegate — BindLambda stateless", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int(int)> d;
    d.BindLambda([](int x) { return x * 2; });

    REQUIRE(d.IsBound());
    REQUIRE(d.Execute(5) == 10);
}

TEST_CASE("TDelegate — BindLambda stateful capturing", "[GP][Core][Events][TDelegate]")
{
    int base = 100;
    TDelegate<int(int)> d;
    d.BindLambda([base](int x) { return base + x; });

    REQUIRE(d.IsBound());
    REQUIRE(d.Execute(7) == 107);
}

TEST_CASE("TDelegate — BindLambda mutable stateful", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int()> d;
    int counter = 0;
    d.BindLambda([counter]() mutable { return ++counter; });

    REQUIRE(d.Execute() == 1);
    REQUIRE(d.Execute() == 2);
    REQUIRE(d.Execute() == 3);
}

TEST_CASE("TDelegate — BindMember non-const", "[GP][Core][Events][TDelegate]")
{
    FCounter c;
    TDelegate<void(int)> d;
    d.BindMember(&c, &FCounter::Add);

    REQUIRE(d.IsBound());
    d.Execute(5);
    d.Execute(3);
    REQUIRE(c.value == 8);
}

TEST_CASE("TDelegate — BindMember const", "[GP][Core][Events][TDelegate]")
{
    FCounter c;
    c.value = 77;

    TDelegate<int()> d;
    d.BindMember(&c, &FCounter::Get);

    REQUIRE(d.IsBound());
    REQUIRE(d.Execute() == 77);
}

TEST_CASE("TDelegate — BindMember const object", "[GP][Core][Events][TDelegate]")
{
    const FCounter c{ 42 };
    TDelegate<int()> d;
    d.BindMember(&c, &FCounter::Get);

    REQUIRE(d.IsBound());
    REQUIRE(d.Execute() == 42);
}

TEST_CASE("TDelegate — BindWeakSP non-const member, object alive", "[GP][Core][Events][TDelegate]")
{
    auto sp = std::make_shared<FCounter>();
    TDelegate<void(int)> d;
    d.BindWeakSP(sp, &FCounter::Add);

    REQUIRE(d.IsBound());
    d.Execute(10);
    REQUIRE(sp->value == 10);
}

TEST_CASE("TDelegate — BindWeakSP const member, object alive", "[GP][Core][Events][TDelegate]")
{
    auto sp = std::make_shared<FCounter>();
    sp->value = 55;

    TDelegate<int()> d;
    d.BindWeakSP(sp, &FCounter::Get);

    REQUIRE(d.IsBound());
    REQUIRE(d.Execute() == 55);
}

TEST_CASE("TDelegate — BindWeakSP IsBound returns false when object expires", "[GP][Core][Events][TDelegate]")
{
    auto sp = std::make_shared<FCounter>();
    TDelegate<void(int)> d;
    d.BindWeakSP(sp, &FCounter::Add);

    REQUIRE(d.IsBound());

    sp.reset();   // destroy the managed object

    REQUIRE_FALSE(d.IsBound());
}

TEST_CASE("TDelegate — BindWeakSP ExecuteIfBound returns false when expired (void)", "[GP][Core][Events][TDelegate]")
{
    auto sp = std::make_shared<FCounter>();
    TDelegate<void(int)> d;
    d.BindWeakSP(sp, &FCounter::Add);

    sp.reset();

    const bool invoked = d.ExecuteIfBound(99);
    REQUIRE_FALSE(invoked);
}

TEST_CASE(
    "TDelegate — BindWeakSP ExecuteIfBound returns nullopt when expired (non-void)", "[GP][Core][Events][TDelegate]"
)
{
    auto sp = std::make_shared<FCounter>();
    sp->value = 7;

    TDelegate<int()> d;
    d.BindWeakSP(sp, &FCounter::Get);

    sp.reset();

    const auto result = d.ExecuteIfBound();
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("TDelegate — Rebind replaces previous binding", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int()> d;
    d.BindLambda([]() { return 1; });
    REQUIRE(d.Execute() == 1);

    d.BindLambda([]() { return 99; });
    REQUIRE(d.Execute() == 99);
}

TEST_CASE("TDelegate — Rebind clears weak-ptr validator", "[GP][Core][Events][TDelegate]")
{
    auto sp = std::make_shared<FCounter>();
    TDelegate<int()> d;
    d.BindWeakSP(sp, &FCounter::Get);
    REQUIRE(d.IsBound());

    // Replace with a plain lambda — the expired weak_ptr should no longer affect IsBound().
    sp.reset();
    d.BindLambda([]() { return 123; });

    REQUIRE(d.IsBound());
    REQUIRE(d.Execute() == 123);
}

TEST_CASE("TDelegate — Unbind", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int()> d;
    d.BindLambda([]() { return 5; });
    REQUIRE(d.IsBound());

    d.Unbind();
    REQUIRE_FALSE(d.IsBound());
}

TEST_CASE("TDelegate — Unbind clears weak-ptr validator", "[GP][Core][Events][TDelegate]")
{
    auto sp = std::make_shared<FCounter>();
    TDelegate<void(int)> d;
    d.BindWeakSP(sp, &FCounter::Add);
    REQUIRE(d.IsBound());

    d.Unbind();
    REQUIRE_FALSE(d.IsBound());
}

TEST_CASE("TDelegate — Execute returns correct value", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int(int, int)> d;
    d.BindStatic(&FreeAdd);
    REQUIRE(d.Execute(10, 32) == 42);
}

TEST_CASE("TDelegate — Execute with multiple argument types", "[GP][Core][Events][TDelegate]")
{
    TDelegate<std::string(std::string, int)> d;
    d.BindLambda([](std::string prefix, int n) { return prefix + std::to_string(n); });
    REQUIRE(d.Execute("val", 7) == "val7");
}

TEST_CASE("TDelegate — ExecuteIfBound when bound (non-void)", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int(int)> d;
    d.BindLambda([](int x) { return x + 1; });

    const auto result = d.ExecuteIfBound(41);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 42);
}

TEST_CASE("TDelegate — ExecuteIfBound when unbound (non-void)", "[GP][Core][Events][TDelegate]")
{
    TDelegate<int(int)> d;
    const auto result = d.ExecuteIfBound(10);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("TDelegate — ExecuteIfBound when bound (void)", "[GP][Core][Events][TDelegate]")
{
    int value = 0;
    TDelegate<void()> d;
    d.BindLambda([&value]() { value = 42; });

    const bool invoked = d.ExecuteIfBound();
    REQUIRE(invoked);
    REQUIRE(value == 42);
}

TEST_CASE("TDelegate — ExecuteIfBound when unbound (void)", "[GP][Core][Events][TDelegate]")
{
    TDelegate<void()> d;
    const bool invoked = d.ExecuteIfBound();
    REQUIRE_FALSE(invoked);
}

TEST_CASE("TDelegate — ExecuteIfBound with weak-ptr binding alive", "[GP][Core][Events][TDelegate]")
{
    auto sp = std::make_shared<FCounter>();
    TDelegate<int()> d;
    d.BindWeakSP(sp, &FCounter::Get);
    sp->value = 42;

    const auto result = d.ExecuteIfBound();
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 42);
}
