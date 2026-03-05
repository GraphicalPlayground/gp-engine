// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/SharedPtr.hpp"
#include "CoreTypes.hpp"
#include "Memory/UniquePtr.hpp"
#include "Memory/WeakPtr.hpp"
#include <catch2/catch_test_macros.hpp>
#include <type_traits>

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

struct FBase
{
    bool& m_destroyed;

    explicit FBase(bool& outDestroyed)
        : m_destroyed(outDestroyed)
    {
        m_destroyed = false;
    }

    virtual ~FBase() { m_destroyed = true; }
};

struct FDerived : FBase
{
    explicit FDerived(bool& outDestroyed)
        : FBase(outDestroyed)
    {}
};

}   // namespace

TEST_CASE("TSharedPtr: Default construction yields null pointer", "[Memory][SharedPtr]")
{
    const TSharedPtr<Int32> ptr;
    REQUIRE(ptr.Get() == nullptr);
}

TEST_CASE("TSharedPtr: Default construction has use count of zero", "[Memory][SharedPtr]")
{
    const TSharedPtr<Int32> ptr;
    REQUIRE(ptr.UseCount() == 0);
}

TEST_CASE("TSharedPtr: Default construction is noexcept", "[Memory][SharedPtr]")
{
    STATIC_REQUIRE(noexcept(TSharedPtr<Int32>{}));
}

TEST_CASE("TSharedPtr: Nullptr construction yields null pointer", "[Memory][SharedPtr]")
{
    const TSharedPtr<Int32> ptr(nullptr);
    REQUIRE(ptr.Get() == nullptr);
    REQUIRE(ptr.UseCount() == 0);
}

TEST_CASE("TSharedPtr: Construction from raw pointer takes ownership", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    {
        TSharedPtr<FTracked> ptr(new FTracked(destroyed));
        REQUIRE(ptr.Get() != nullptr);
        REQUIRE(ptr.UseCount() == 1);
        REQUIRE_FALSE(destroyed);
    }
    REQUIRE(destroyed);
}

TEST_CASE("TSharedPtr: Construction from derived raw pointer is accepted", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    {
        TSharedPtr<FBase> ptr(new FDerived(destroyed));
        REQUIRE(ptr.Get() != nullptr);
        REQUIRE_FALSE(destroyed);
    }
    REQUIRE(destroyed);
}

TEST_CASE("TSharedPtr: Construction with custom deleter invokes it on destroy", "[Memory][SharedPtr]")
{
    bool customCalled = false;

    struct FCustomDeleter
    {
        bool& m_called;

        void operator()(Int32* ptr) const noexcept
        {
            m_called = true;
            delete ptr;
        }
    };

    {
        TSharedPtr<Int32> ptr(new Int32(1), FCustomDeleter{ customCalled });
        REQUIRE_FALSE(customCalled);
    }
    REQUIRE(customCalled);
}

TEST_CASE("TSharedPtr: Copy construction shares ownership and increments use count", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FTracked> a(new FTracked(destroyed));
    TSharedPtr<FTracked> b(a);
    REQUIRE(a.UseCount() == 2);
    REQUIRE(b.UseCount() == 2);
    REQUIRE(a.Get() == b.Get());
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TSharedPtr: Copy construction from derived shares ownership", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FDerived> derived(new FDerived(destroyed));
    TSharedPtr<FBase> base(derived);
    REQUIRE(base.Get() == derived.Get());
    REQUIRE(base.UseCount() == 2);
}

TEST_CASE("TSharedPtr: Move construction transfers ownership", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FTracked> src(new FTracked(destroyed));
    FTracked* raw = src.Get();
    TSharedPtr<FTracked> dst(GP::Move(src));
    REQUIRE(src.Get() == nullptr);
    REQUIRE(src.UseCount() == 0);
    REQUIRE(dst.Get() == raw);
    REQUIRE(dst.UseCount() == 1);
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TSharedPtr: Move construction from derived transfers ownership", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FDerived> derived(new FDerived(destroyed));
    FBase* raw = derived.Get();
    TSharedPtr<FBase> base(GP::Move(derived));
    REQUIRE(derived.Get() == nullptr);
    REQUIRE(base.Get() == raw);
    REQUIRE(base.UseCount() == 1);
}

TEST_CASE("TSharedPtr: Aliasing constructor shares control block but stores different pointer", "[Memory][SharedPtr]")
{
    struct FOwner
    {
        Int32 value{ 42 };
    };

    TSharedPtr<FOwner> owner(new FOwner{});
    TSharedPtr<Int32> alias(owner, &owner->value);
    REQUIRE(alias.Get() == &owner->value);
    REQUIRE(alias.UseCount() == 2);
    REQUIRE(*alias == 42);
}

TEST_CASE(
    "TSharedPtr: Aliasing move constructor transfers control block and stores different pointer", "[Memory][SharedPtr]"
)
{
    struct FOwner
    {
        Int32 value{ 7 };
    };

    TSharedPtr<FOwner> owner(new FOwner{});
    Int32* memberPtr = &owner->value;
    TSharedPtr<Int32> alias(GP::Move(owner), memberPtr);
    REQUIRE(owner.Get() == nullptr);
    REQUIRE(alias.Get() == memberPtr);
    REQUIRE(alias.UseCount() == 1);
    REQUIRE(*alias == 7);
}

TEST_CASE("TSharedPtr: Destructor destroys object when last owner is destroyed", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    {
        TSharedPtr<FTracked> ptr(new FTracked(destroyed));
    }
    REQUIRE(destroyed);
}

TEST_CASE("TSharedPtr: Object is kept alive while multiple owners exist", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FTracked> a(new FTracked(destroyed));
    {
        TSharedPtr<FTracked> b(a);
        REQUIRE_FALSE(destroyed);
    }
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TSharedPtr: Copy assignment shares ownership and increments use count", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FTracked> a(new FTracked(destroyed));
    TSharedPtr<FTracked> b;
    b = a;
    REQUIRE(b.Get() == a.Get());
    REQUIRE(b.UseCount() == 2);
}

TEST_CASE("TSharedPtr: Copy assignment releases previous ownership", "[Memory][SharedPtr]")
{
    bool destroyed1 = false;
    bool destroyed2 = false;
    TSharedPtr<FTracked> a(new FTracked(destroyed1));
    TSharedPtr<FTracked> b(new FTracked(destroyed2));
    b = a;
    REQUIRE(destroyed2);
    REQUIRE_FALSE(destroyed1);
}

TEST_CASE("TSharedPtr: Copy assignment from derived shares ownership", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FDerived> derived(new FDerived(destroyed));
    TSharedPtr<FBase> base;
    base = derived;
    REQUIRE(base.Get() == derived.Get());
    REQUIRE(base.UseCount() == 2);
}

TEST_CASE("TSharedPtr: Move assignment transfers ownership", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FTracked> src(new FTracked(destroyed));
    FTracked* raw = src.Get();
    TSharedPtr<FTracked> dst;
    dst = GP::Move(src);
    REQUIRE(src.Get() == nullptr);
    REQUIRE(dst.Get() == raw);
    REQUIRE(dst.UseCount() == 1);
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TSharedPtr: Move assignment releases previous ownership", "[Memory][SharedPtr]")
{
    bool destroyed1 = false;
    bool destroyed2 = false;
    TSharedPtr<FTracked> src(new FTracked(destroyed1));
    TSharedPtr<FTracked> dst(new FTracked(destroyed2));
    dst = GP::Move(src);
    REQUIRE(destroyed2);
    REQUIRE_FALSE(destroyed1);
}

TEST_CASE("TSharedPtr: Nullptr assignment releases ownership and destroys object", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FTracked> ptr(new FTracked(destroyed));
    ptr = nullptr;
    REQUIRE(ptr.Get() == nullptr);
    REQUIRE(destroyed);
}

TEST_CASE("TSharedPtr: Self copy assignment is safe", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FTracked> ptr(new FTracked(destroyed));
    FTracked* raw = ptr.Get();
    ptr = ptr;
    REQUIRE(ptr.Get() == raw);
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TSharedPtr: operator* dereferences the pointer", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> ptr(new Int32(55));
    REQUIRE(*ptr == 55);
}

TEST_CASE("TSharedPtr: operator* allows mutation", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> ptr(new Int32(1));
    *ptr = 77;
    REQUIRE(*ptr == 77);
}

TEST_CASE("TSharedPtr: operator-> accesses members", "[Memory][SharedPtr]")
{
    struct FPoint
    {
        Int32 x{ 3 };
        Int32 y{ 9 };
    };

    TSharedPtr<FPoint> ptr(new FPoint{});
    REQUIRE(ptr->x == 3);
    REQUIRE(ptr->y == 9);
}

TEST_CASE("TSharedPtr: operator bool returns true for non-null pointer", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> ptr(new Int32(0));
    REQUIRE(static_cast<bool>(ptr));
}

TEST_CASE("TSharedPtr: operator bool returns false for null pointer", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> ptr;
    REQUIRE_FALSE(static_cast<bool>(ptr));
}

TEST_CASE("TSharedPtr: Get returns the raw pointer", "[Memory][SharedPtr]")
{
    Int32* raw = new Int32(99);
    TSharedPtr<Int32> ptr(raw);
    REQUIRE(ptr.Get() == raw);
}

TEST_CASE("TSharedPtr: IsUnique returns true when only one owner", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> ptr(new Int32(1));
    REQUIRE(ptr.IsUnique());
}

TEST_CASE("TSharedPtr: IsUnique returns false when multiple owners", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> a(new Int32(1));
    TSharedPtr<Int32> b(a);
    REQUIRE_FALSE(a.IsUnique());
    REQUIRE_FALSE(b.IsUnique());
}

TEST_CASE("TSharedPtr: UseCount reflects number of owning instances", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> a(new Int32(1));
    REQUIRE(a.UseCount() == 1);
    TSharedPtr<Int32> b(a);
    REQUIRE(a.UseCount() == 2);
    TSharedPtr<Int32> c(a);
    REQUIRE(a.UseCount() == 3);
}

TEST_CASE("TSharedPtr: Reset releases ownership and destroys object when last owner", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FTracked> ptr(new FTracked(destroyed));
    ptr.Reset();
    REQUIRE(ptr.Get() == nullptr);
    REQUIRE(destroyed);
}

TEST_CASE("TSharedPtr: Reset with new pointer transfers ownership", "[Memory][SharedPtr]")
{
    bool destroyed1 = false;
    bool destroyed2 = false;
    TSharedPtr<FTracked> ptr(new FTracked(destroyed1));
    ptr.Reset(new FTracked(destroyed2));
    REQUIRE(destroyed1);
    REQUIRE_FALSE(destroyed2);
    REQUIRE(ptr.UseCount() == 1);
}

TEST_CASE("TSharedPtr: Swap exchanges two shared pointers", "[Memory][SharedPtr]")
{
    Int32* raw1 = new Int32(1);
    Int32* raw2 = new Int32(2);
    TSharedPtr<Int32> a(raw1);
    TSharedPtr<Int32> b(raw2);
    a.Swap(b);
    REQUIRE(a.Get() == raw2);
    REQUIRE(b.Get() == raw1);
}

TEST_CASE("TSharedPtr: Non-member Swap exchanges two shared pointers", "[Memory][SharedPtr]")
{
    Int32* raw1 = new Int32(3);
    Int32* raw2 = new Int32(4);
    TSharedPtr<Int32> a(raw1);
    TSharedPtr<Int32> b(raw2);
    Swap(a, b);
    REQUIRE(a.Get() == raw2);
    REQUIRE(b.Get() == raw1);
}

TEST_CASE("TSharedPtr: Equality operator compares raw pointers", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> a(new Int32(1));
    TSharedPtr<Int32> b(a);
    TSharedPtr<Int32> c(new Int32(1));
    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
}

TEST_CASE("TSharedPtr: Equality with nullptr", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> empty;
    TSharedPtr<Int32> nonEmpty(new Int32(1));
    REQUIRE(empty == nullptr);
    REQUIRE_FALSE(nonEmpty == nullptr);
}

TEST_CASE("TSharedPtr: Spaceship operator provides ordering", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> empty;
    TSharedPtr<Int32> nonEmpty(new Int32(0));
    REQUIRE((empty <=> empty) == std::strong_ordering::equal);
    REQUIRE((empty <=> nullptr) == std::strong_ordering::equal);
    REQUIRE((nonEmpty <=> nullptr) != std::strong_ordering::equal);
}

TEST_CASE("MakeShared: Constructs object with arguments in single allocation", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    {
        auto ptr = MakeShared<FTracked>(destroyed);
        REQUIRE(ptr.Get() != nullptr);
        REQUIRE(ptr.UseCount() == 1);
        REQUIRE_FALSE(destroyed);
    }
    REQUIRE(destroyed);
}

TEST_CASE("MakeShared: Use count starts at 1", "[Memory][SharedPtr]")
{
    auto ptr = MakeShared<Int32>(42);
    REQUIRE(ptr.UseCount() == 1);
}

TEST_CASE("MakeShared: Constructed value is accessible", "[Memory][SharedPtr]")
{
    auto ptr = MakeShared<Int32>(99);
    REQUIRE(*ptr == 99);
}

TEST_CASE("MakeShared: Multiple copies share the same object", "[Memory][SharedPtr]")
{
    auto a = MakeShared<Int32>(5);
    auto b = a;
    REQUIRE(a.UseCount() == 2);
    REQUIRE(a.Get() == b.Get());
}

TEST_CASE("StaticPointerCast: Casts base shared pointer to derived", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FBase> base(new FDerived(destroyed));
    auto derived = StaticPointerCast<FDerived>(base);
    REQUIRE(derived.Get() != nullptr);
    REQUIRE(derived.UseCount() == 2);
}

TEST_CASE("StaticPointerCast: Move overload transfers without extra refcount", "[Memory][SharedPtr]")
{
    bool destroyed = false;
    TSharedPtr<FBase> base(new FDerived(destroyed));
    FBase* raw = base.Get();
    auto derived = StaticPointerCast<FDerived>(GP::Move(base));
    REQUIRE(derived.Get() == static_cast<FDerived*>(raw));
    REQUIRE(derived.UseCount() == 1);
}

TEST_CASE("ConstPointerCast: Casts const shared pointer to non-const", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> src(new Int32(7));
    TSharedPtr<const Int32> constPtr(src, src.Get());
    auto mutablePtr = ConstPointerCast<Int32>(constPtr);
    REQUIRE(mutablePtr.Get() == src.Get());
    REQUIRE(mutablePtr.UseCount() == 3);
}

TEST_CASE("ConstPointerCast: Move overload transfers without extra refcount", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> src(new Int32(3));
    TSharedPtr<const Int32> constPtr(src, src.Get());
    const Int32* raw = constPtr.Get();
    auto mutablePtr = ConstPointerCast<Int32>(GP::Move(constPtr));
    REQUIRE(mutablePtr.Get() == const_cast<Int32*>(raw));
    REQUIRE(mutablePtr.UseCount() == 2);
}

TEST_CASE("ReinterpretPointerCast: Casts pointer type without changing control block", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> intPtr(new Int32(0));
    auto bytePtr = ReinterpretPointerCast<Byte>(intPtr);
    REQUIRE(bytePtr.Get() == reinterpret_cast<Byte*>(intPtr.Get()));
    REQUIRE(bytePtr.UseCount() == 2);
}

TEST_CASE("ReinterpretPointerCast: Move overload transfers without extra refcount", "[Memory][SharedPtr]")
{
    TSharedPtr<Int32> intPtr(new Int32(0));
    Byte* raw = reinterpret_cast<Byte*>(intPtr.Get());
    auto bytePtr = ReinterpretPointerCast<Byte>(GP::Move(intPtr));
    REQUIRE(bytePtr.Get() == raw);
    REQUIRE(bytePtr.UseCount() == 1);
}

TEST_CASE("MakeUnique: Constructs scalar object with arguments", "[Memory][UniquePtr]")
{
    auto ptr = MakeUnique<Int32>(42);
    REQUIRE(ptr.Get() != nullptr);
    REQUIRE(*ptr == 42);
}

TEST_CASE("MakeUnique: Constructs default-initialized scalar", "[Memory][UniquePtr]")
{
    auto ptr = MakeUnique<Int32>();
    REQUIRE(ptr.Get() != nullptr);
    REQUIRE(*ptr == 0);
}

TEST_CASE("MakeUnique: Constructs array of given count", "[Memory][UniquePtr]")
{
    auto ptr = MakeUnique<Int32[]>(4);
    REQUIRE(ptr.Get() != nullptr);
}

TEST_CASE("MakeUnique: Array elements are value-initialized to zero", "[Memory][UniquePtr]")
{
    auto ptr = MakeUnique<Int32[]>(3);
    REQUIRE(ptr[0] == 0);
    REQUIRE(ptr[1] == 0);
    REQUIRE(ptr[2] == 0);
}

TEST_CASE("MakeUnique: Object is destroyed when pointer goes out of scope", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    {
        auto ptr = MakeUnique<FTracked>(destroyed);
        REQUIRE_FALSE(destroyed);
    }
    REQUIRE(destroyed);
}

TEST_CASE("MakeUniqueForOverwrite: Constructs scalar object without zero-fill", "[Memory][UniquePtr]")
{
    auto ptr = MakeUniqueForOverwrite<Int32>();
    REQUIRE(ptr.Get() != nullptr);
}
