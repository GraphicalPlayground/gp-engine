// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/ControlBlock.hpp"
#include "CoreTypes.hpp"
#include "Memory/DefaultDeleter.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

namespace
{

struct FTracked
{
    bool& m_destroyed;

    explicit FTracked(bool& outDestroyed)
        : m_destroyed(outDestroyed)
    {
        m_destroyed = false;
    }

    ~FTracked() { m_destroyed = true; }
};

}   // namespace

TEST_CASE("FControlBlockBase: Initial strong count is 1", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(
        new FTracked(destroyed), TDefaultDeleter<FTracked>{}
    );
    REQUIRE(block->GetStrongCount() == 1);
    block->ReleaseStrongRef();
}

TEST_CASE("FControlBlockBase: Initial weak count is 1 (implicit weak ref)", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(
        new FTracked(destroyed), TDefaultDeleter<FTracked>{}
    );
    REQUIRE(block->GetWeakCount() == 1);
    block->ReleaseStrongRef();
}

TEST_CASE("FControlBlockBase: AddStrongRef increments strong count", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(
        new FTracked(destroyed), TDefaultDeleter<FTracked>{}
    );
    block->AddStrongRef();
    REQUIRE(block->GetStrongCount() == 2);
    block->ReleaseStrongRef();
    block->ReleaseStrongRef();
}

TEST_CASE("FControlBlockBase: AddWeakRef increments weak count", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(
        new FTracked(destroyed), TDefaultDeleter<FTracked>{}
    );
    block->AddWeakRef();
    REQUIRE(block->GetWeakCount() == 2);
    block->ReleaseWeakRef();
    block->ReleaseStrongRef();
}

TEST_CASE("FControlBlockBase: ReleaseStrongRef to zero calls DestroyObject", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(
        new FTracked(destroyed), TDefaultDeleter<FTracked>{}
    );
    REQUIRE_FALSE(destroyed);
    block->ReleaseStrongRef();
    REQUIRE(destroyed);
}

TEST_CASE("FControlBlockBase: ReleaseWeakRef to zero deletes control block", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(
        new FTracked(destroyed), TDefaultDeleter<FTracked>{}
    );
    block->AddWeakRef();
    block->ReleaseStrongRef();
    REQUIRE(destroyed);
    block->ReleaseWeakRef();
}

TEST_CASE("FControlBlockBase: Multiple strong refs keep object alive", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(
        new FTracked(destroyed), TDefaultDeleter<FTracked>{}
    );
    block->AddStrongRef();
    block->AddStrongRef();
    REQUIRE(block->GetStrongCount() == 3);
    block->ReleaseStrongRef();
    REQUIRE_FALSE(destroyed);
    block->ReleaseStrongRef();
    REQUIRE_FALSE(destroyed);
    block->ReleaseStrongRef();
    REQUIRE(destroyed);
}

TEST_CASE("FControlBlockBase: TryAddStrongRef succeeds when strong count is non-zero", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(
        new FTracked(destroyed), TDefaultDeleter<FTracked>{}
    );
    REQUIRE(block->GetStrongCount() == 1);
    const bool result = block->TryAddStrongRef();
    REQUIRE(result);
    REQUIRE(block->GetStrongCount() == 2);
    block->ReleaseStrongRef();
    block->ReleaseStrongRef();
}

TEST_CASE("FControlBlockBase: TryAddStrongRef fails when strong count is zero", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(
        new FTracked(destroyed), TDefaultDeleter<FTracked>{}
    );
    block->AddWeakRef();
    block->ReleaseStrongRef();
    REQUIRE(block->GetStrongCount() == 0);
    const bool result = block->TryAddStrongRef();
    REQUIRE_FALSE(result);
    block->ReleaseWeakRef();
}

TEST_CASE("TControlBlockWithPtr: Constructed with pointer and deleter stores object", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* tracked = new FTracked(destroyed);
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(tracked, TDefaultDeleter<FTracked>{});
    REQUIRE_FALSE(destroyed);
    block->ReleaseStrongRef();
    REQUIRE(destroyed);
}

TEST_CASE("TControlBlockWithPtr: Custom deleter is invoked on destroy", "[Memory][ControlBlock]")
{
    bool customDeleterCalled = false;

    struct FCustomDeleter
    {
        bool& m_called;

        void operator()(Int32* ptr) const noexcept
        {
            m_called = true;
            delete ptr;
        }
    };

    auto* block = new TControlBlockWithPtr<Int32, FCustomDeleter>(new Int32(42), FCustomDeleter{ customDeleterCalled });
    REQUIRE_FALSE(customDeleterCalled);
    block->ReleaseStrongRef();
    REQUIRE(customDeleterCalled);
}

TEST_CASE("TControlBlockInline: Object is constructed in-place", "[Memory][ControlBlock]")
{
    auto* block = new TControlBlockInline<Int32>(77);
    REQUIRE(*block->GetPtr() == 77);
    block->ReleaseStrongRef();
}

TEST_CASE("TControlBlockInline: GetPtr returns non-null pointer", "[Memory][ControlBlock]")
{
    auto* block = new TControlBlockInline<Int32>(0);
    REQUIRE(block->GetPtr() != nullptr);
    block->ReleaseStrongRef();
}

TEST_CASE("TControlBlockInline: Const GetPtr returns non-null const pointer", "[Memory][ControlBlock]")
{
    const auto* block = new TControlBlockInline<Int32>(55);
    REQUIRE(block->GetPtr() != nullptr);
    REQUIRE(*block->GetPtr() == 55);
    const_cast<TControlBlockInline<Int32>*>(block)->ReleaseStrongRef();
}

TEST_CASE("TControlBlockInline: DestroyObject calls destructor of managed object", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    FTracked* dummy = nullptr;
    auto* block = new TControlBlockInline<FTracked>(destroyed);
    dummy = block->GetPtr();
    REQUIRE_FALSE(destroyed);
    block->ReleaseStrongRef();
    REQUIRE(destroyed);
    (void)dummy;
}

TEST_CASE("TControlBlockInline: Supports non-trivial multi-arg construction", "[Memory][ControlBlock]")
{
    struct FPoint
    {
        Int32 x;
        Int32 y;
    };

    auto* block = new TControlBlockInline<FPoint>(FPoint{ 3, 7 });
    REQUIRE(block->GetPtr()->x == 3);
    REQUIRE(block->GetPtr()->y == 7);
    block->ReleaseStrongRef();
}

TEST_CASE("FControlBlockBase: Weak count decrements correctly across multiple weak refs", "[Memory][ControlBlock]")
{
    bool destroyed = false;
    auto* block = new TControlBlockWithPtr<FTracked, TDefaultDeleter<FTracked>>(
        new FTracked(destroyed), TDefaultDeleter<FTracked>{}
    );
    block->AddWeakRef();
    block->AddWeakRef();
    REQUIRE(block->GetWeakCount() == 3);
    block->ReleaseWeakRef();
    REQUIRE(block->GetWeakCount() == 2);
    block->ReleaseWeakRef();
    REQUIRE(block->GetWeakCount() == 1);
    block->ReleaseStrongRef();
}
