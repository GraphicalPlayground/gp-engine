// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/EnableSharedFromThis.hpp"
#include "CoreTypes.hpp"
#include "Memory/SharedPtr.hpp"
#include "Memory/WeakPtr.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

namespace
{

struct FNode : TEnableSharedFromThis<FNode>
{
    Int32 value{ 0 };

    explicit FNode(Int32 v)
        : value(v)
    {}
};

struct FBaseNode : TEnableSharedFromThis<FBaseNode>
{
    bool& m_destroyed;

    explicit FBaseNode(bool& outDestroyed)
        : m_destroyed(outDestroyed)
    {
        m_destroyed = false;
    }

    virtual ~FBaseNode() { m_destroyed = true; }
};

struct FDerivedNode : FBaseNode
{
    explicit FDerivedNode(bool& outDestroyed)
        : FBaseNode(outDestroyed)
    {}
};

}   // namespace

TEST_CASE("TEnableSharedFromThis: SharedFromThis returns valid shared pointer", "[Memory][EnableSharedFromThis]")
{
    auto owner = MakeShared<FNode>(42);
    auto self = owner->SharedFromThis();
    REQUIRE(self.Get() == owner.Get());
}

TEST_CASE(
    "TEnableSharedFromThis: SharedFromThis shares ownership with existing owner", "[Memory][EnableSharedFromThis]"
)
{
    auto owner = MakeShared<FNode>(1);
    auto self = owner->SharedFromThis();
    REQUIRE(owner.UseCount() == 2);
}

TEST_CASE(
    "TEnableSharedFromThis: SharedFromThis does not create a second control block", "[Memory][EnableSharedFromThis]"
)
{
    auto owner = MakeShared<FNode>(5);
    auto self1 = owner->SharedFromThis();
    auto self2 = owner->SharedFromThis();
    REQUIRE(owner.UseCount() == 3);
    REQUIRE(self1.Get() == self2.Get());
}

TEST_CASE(
    "TEnableSharedFromThis: Object is destroyed when all shared pointers are gone", "[Memory][EnableSharedFromThis]"
)
{
    bool destroyed = false;
    TSharedPtr<FBaseNode> self;
    {
        auto owner = MakeShared<FBaseNode>(destroyed);
        self = owner->SharedFromThis();
        REQUIRE_FALSE(destroyed);
    }
    REQUIRE_FALSE(destroyed);
    self.Reset();
    REQUIRE(destroyed);
}

TEST_CASE("TEnableSharedFromThis: Const SharedFromThis returns TSharedPtr<const T>", "[Memory][EnableSharedFromThis]")
{
    auto owner = MakeShared<FNode>(7);
    const FNode* raw = owner.Get();
    auto constSelf = raw->SharedFromThis();
    REQUIRE(constSelf.Get() == owner.Get());
    REQUIRE(owner.UseCount() == 2);
}

TEST_CASE("TEnableSharedFromThis: WeakFromThis returns non-expired weak pointer", "[Memory][EnableSharedFromThis]")
{
    auto owner = MakeShared<FNode>(10);
    auto weak = owner->WeakFromThis();
    REQUIRE_FALSE(weak.IsExpired());
    REQUIRE(weak.UseCount() == 1);
}

TEST_CASE("TEnableSharedFromThis: WeakFromThis does not increment strong count", "[Memory][EnableSharedFromThis]")
{
    auto owner = MakeShared<FNode>(3);
    auto weak = owner->WeakFromThis();
    REQUIRE(owner.UseCount() == 1);
}

TEST_CASE("TEnableSharedFromThis: WeakFromThis becomes expired after all owners gone", "[Memory][EnableSharedFromThis]")
{
    TWeakPtr<FNode> weak;
    {
        auto owner = MakeShared<FNode>(0);
        weak = owner->WeakFromThis();
        REQUIRE_FALSE(weak.IsExpired());
    }
    REQUIRE(weak.IsExpired());
}

TEST_CASE(
    "TEnableSharedFromThis: Const WeakFromThis returns non-expired weak pointer", "[Memory][EnableSharedFromThis]"
)
{
    auto owner = MakeShared<FNode>(9);
    const FNode* raw = owner.Get();
    auto weak = raw->WeakFromThis();
    REQUIRE_FALSE(weak.IsExpired());
}

TEST_CASE(
    "TEnableSharedFromThis: Lock on WeakFromThis result returns valid shared pointer", "[Memory][EnableSharedFromThis]"
)
{
    auto owner = MakeShared<FNode>(99);
    auto locked = owner->WeakFromThis().Lock();
    REQUIRE(locked.Get() == owner.Get());
    REQUIRE(owner.UseCount() == 2);
}

TEST_CASE(
    "TEnableSharedFromThis: Works correctly when constructed via TSharedPtr raw pointer ctor",
    "[Memory][EnableSharedFromThis]"
)
{
    TSharedPtr<FNode> owner(new FNode(55));
    auto self = owner->SharedFromThis();
    REQUIRE(self.Get() == owner.Get());
    REQUIRE(owner.UseCount() == 2);
}

TEST_CASE(
    "TEnableSharedFromThis: Copy construction preserves the shared from this contract", "[Memory][EnableSharedFromThis]"
)
{
    auto a = MakeShared<FNode>(11);
    auto b = a;
    auto selfA = a->SharedFromThis();
    auto selfB = b->SharedFromThis();
    REQUIRE(selfA.Get() == selfB.Get());
    REQUIRE(a.UseCount() == 4);
}

TEST_CASE("TEnableSharedFromThis: Derived class can use SharedFromThis via base", "[Memory][EnableSharedFromThis]")
{
    bool destroyed = false;
    auto owner = MakeShared<FDerivedNode>(destroyed);
    TSharedPtr<FBaseNode> base = owner;
    auto self = base->SharedFromThis();
    REQUIRE(self.Get() == owner.Get());
    REQUIRE(owner.UseCount() == 3);
}
