// Copyright (c) - Graphical Playground. All rights reserved.

#include "Memory/UniquePtr.hpp"
#include "CoreTypes.hpp"
#include "Memory/DefaultDeleter.hpp"
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

struct FStatefulDeleter
{
    Int32& m_callCount;

    explicit FStatefulDeleter(Int32& outCallCount)
        : m_callCount(outCallCount)
    {}

    void operator()(Int32* ptr) const noexcept
    {
        ++m_callCount;
        delete ptr;
    }
};

}   // namespace

TEST_CASE("TUniquePtr: Default construction yields null pointer", "[Memory][UniquePtr]")
{
    const TUniquePtr<Int32> ptr;
    REQUIRE(ptr.Get() == nullptr);
}

TEST_CASE("TUniquePtr: Default construction is noexcept", "[Memory][UniquePtr]")
{
    STATIC_REQUIRE(noexcept(TUniquePtr<Int32>{}));
}

TEST_CASE("TUniquePtr: Nullptr construction yields null pointer", "[Memory][UniquePtr]")
{
    const TUniquePtr<Int32> ptr(nullptr);
    REQUIRE(ptr.Get() == nullptr);
}

TEST_CASE("TUniquePtr: Construction from raw pointer takes ownership", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    {
        TUniquePtr<FTracked> ptr(new FTracked(destroyed));
        REQUIRE(ptr.Get() != nullptr);
        REQUIRE_FALSE(destroyed);
    }
    REQUIRE(destroyed);
}

TEST_CASE("TUniquePtr: Construction with lvalue deleter copies it", "[Memory][UniquePtr]")
{
    Int32 callCount = 0;
    {
        const FStatefulDeleter deleter{ callCount };
        TUniquePtr<Int32, FStatefulDeleter> ptr(new Int32(1), deleter);
        REQUIRE(callCount == 0);
    }
    REQUIRE(callCount == 1);
}

TEST_CASE("TUniquePtr: Construction with rvalue deleter moves it", "[Memory][UniquePtr]")
{
    Int32 callCount = 0;
    {
        TUniquePtr<Int32, FStatefulDeleter> ptr(new Int32(1), FStatefulDeleter{ callCount });
        REQUIRE(callCount == 0);
    }
    REQUIRE(callCount == 1);
}

TEST_CASE("TUniquePtr: Move construction transfers ownership", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    TUniquePtr<FTracked> src(new FTracked(destroyed));
    FTracked* rawPtr = src.Get();

    TUniquePtr<FTracked> dst(GP::Move(src));
    REQUIRE(src.Get() == nullptr);
    REQUIRE(dst.Get() == rawPtr);
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TUniquePtr: Move construction from derived pointer", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    TUniquePtr<FDerived> derived(new FDerived(destroyed));
    TUniquePtr<FBase> base(GP::Move(derived));
    REQUIRE(derived.Get() == nullptr);
    REQUIRE(base.Get() != nullptr);
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TUniquePtr: Is not copy constructible", "[Memory][UniquePtr]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<TUniquePtr<Int32>>);
}

TEST_CASE("TUniquePtr: Destructor calls deleter exactly once", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    {
        TUniquePtr<FTracked> ptr(new FTracked(destroyed));
    }
    REQUIRE(destroyed);
}

TEST_CASE("TUniquePtr: Destructor on null pointer is a no-op", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32> ptr;
    REQUIRE(ptr.Get() == nullptr);
}

TEST_CASE("TUniquePtr: Move assignment transfers ownership", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    TUniquePtr<FTracked> src(new FTracked(destroyed));
    FTracked* rawPtr = src.Get();

    TUniquePtr<FTracked> dst;
    dst = GP::Move(src);
    REQUIRE(src.Get() == nullptr);
    REQUIRE(dst.Get() == rawPtr);
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TUniquePtr: Move assignment destroys existing resource", "[Memory][UniquePtr]")
{
    bool destroyed1 = false;
    bool destroyed2 = false;
    TUniquePtr<FTracked> src(new FTracked(destroyed1));
    TUniquePtr<FTracked> dst(new FTracked(destroyed2));

    dst = GP::Move(src);
    REQUIRE(destroyed2);
    REQUIRE_FALSE(destroyed1);
}

TEST_CASE("TUniquePtr: Move assignment from derived pointer", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    TUniquePtr<FDerived> derived(new FDerived(destroyed));
    TUniquePtr<FBase> base;
    base = GP::Move(derived);
    REQUIRE(derived.Get() == nullptr);
    REQUIRE(base.Get() != nullptr);
}

TEST_CASE("TUniquePtr: Nullptr assignment resets the pointer", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    TUniquePtr<FTracked> ptr(new FTracked(destroyed));
    ptr = nullptr;
    REQUIRE(ptr.Get() == nullptr);
    REQUIRE(destroyed);
}

TEST_CASE("TUniquePtr: Is not copy assignable", "[Memory][UniquePtr]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<TUniquePtr<Int32>>);
}

TEST_CASE("TUniquePtr: Self move assignment is a no-op", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    TUniquePtr<FTracked> ptr(new FTracked(destroyed));
    FTracked* rawPtr = ptr.Get();
    ptr = GP::Move(ptr);
    REQUIRE(ptr.Get() == rawPtr);
    REQUIRE_FALSE(destroyed);
}

TEST_CASE("TUniquePtr: operator* dereferences the pointer", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32> ptr(new Int32(42));
    REQUIRE(*ptr == 42);
}

TEST_CASE("TUniquePtr: operator* allows mutation", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32> ptr(new Int32(1));
    *ptr = 99;
    REQUIRE(*ptr == 99);
}

TEST_CASE("TUniquePtr: operator-> accesses members", "[Memory][UniquePtr]")
{
    struct FPoint
    {
        Int32 x{ 3 };
        Int32 y{ 7 };
    };

    TUniquePtr<FPoint> ptr(new FPoint{});
    REQUIRE(ptr->x == 3);
    REQUIRE(ptr->y == 7);
}

TEST_CASE("TUniquePtr: operator bool returns true for non-null pointer", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32> ptr(new Int32(0));
    REQUIRE(static_cast<bool>(ptr));
}

TEST_CASE("TUniquePtr: operator bool returns false for null pointer", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32> ptr;
    REQUIRE_FALSE(static_cast<bool>(ptr));
}

TEST_CASE("TUniquePtr: Get returns the raw pointer", "[Memory][UniquePtr]")
{
    Int32* raw = new Int32(5);
    TUniquePtr<Int32> ptr(raw);
    REQUIRE(ptr.Get() == raw);
}

TEST_CASE("TUniquePtr: GetDeleter returns reference to deleter", "[Memory][UniquePtr]")
{
    Int32 callCount = 0;
    TUniquePtr<Int32, FStatefulDeleter> ptr(new Int32(1), FStatefulDeleter{ callCount });
    REQUIRE(&ptr.GetDeleter() != nullptr);
}

TEST_CASE("TUniquePtr: GetDeleter const overload returns const reference", "[Memory][UniquePtr]")
{
    const TUniquePtr<Int32> ptr(new Int32(1));
    const TDefaultDeleter<Int32>& d = ptr.GetDeleter();
    (void)d;
    SUCCEED();
}

TEST_CASE("TUniquePtr: Release yields raw pointer and nulls internal pointer", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    Int32* raw = nullptr;
    {
        TUniquePtr<FTracked> ptr(new FTracked(destroyed));
        raw = reinterpret_cast<Int32*>(ptr.Release());
        REQUIRE(ptr.Get() == nullptr);
        REQUIRE_FALSE(destroyed);
    }
    REQUIRE_FALSE(destroyed);
    delete reinterpret_cast<FTracked*>(raw);
    REQUIRE(destroyed);
}

TEST_CASE("TUniquePtr: Reset with new pointer destroys previous object", "[Memory][UniquePtr]")
{
    bool destroyed1 = false;
    bool destroyed2 = false;
    TUniquePtr<FTracked> ptr(new FTracked(destroyed1));
    ptr.Reset(new FTracked(destroyed2));
    REQUIRE(destroyed1);
    REQUIRE_FALSE(destroyed2);
}

TEST_CASE("TUniquePtr: Reset with nullptr destroys object and nulls pointer", "[Memory][UniquePtr]")
{
    bool destroyed = false;
    TUniquePtr<FTracked> ptr(new FTracked(destroyed));
    ptr.Reset();
    REQUIRE(destroyed);
    REQUIRE(ptr.Get() == nullptr);
}

TEST_CASE("TUniquePtr: Swap exchanges pointers between two instances", "[Memory][UniquePtr]")
{
    Int32* raw1 = new Int32(10);
    Int32* raw2 = new Int32(20);
    TUniquePtr<Int32> a(raw1);
    TUniquePtr<Int32> b(raw2);
    a.Swap(b);
    REQUIRE(a.Get() == raw2);
    REQUIRE(b.Get() == raw1);
}

TEST_CASE("TUniquePtr: Non-member Swap exchanges pointers", "[Memory][UniquePtr]")
{
    Int32* raw1 = new Int32(1);
    Int32* raw2 = new Int32(2);
    TUniquePtr<Int32> a(raw1);
    TUniquePtr<Int32> b(raw2);
    Swap(a, b);
    REQUIRE(a.Get() == raw2);
    REQUIRE(b.Get() == raw1);
}

TEST_CASE("TUniquePtr: Equality operator compares pointers", "[Memory][UniquePtr]")
{
    Int32* raw = new Int32(1);
    TUniquePtr<Int32> a(raw);
    TUniquePtr<Int32> b;
    REQUIRE_FALSE(a == b);
    Int32* released = a.Release();
    TUniquePtr<Int32> c(released);
    TUniquePtr<Int32> d(released);
    REQUIRE(c == d);
    (void)d.Release();
}

TEST_CASE("TUniquePtr: Equality with nullptr", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32> empty;
    TUniquePtr<Int32> nonEmpty(new Int32(1));
    REQUIRE(empty == nullptr);
    REQUIRE_FALSE(nonEmpty == nullptr);
}

TEST_CASE("TUniquePtr: Spaceship operator provides ordering", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32> empty;
    TUniquePtr<Int32> nonEmpty(new Int32(0));
    REQUIRE((empty <=> empty) == std::strong_ordering::equal);
    REQUIRE((empty <=> nullptr) == std::strong_ordering::equal);
    REQUIRE((nonEmpty <=> nullptr) != std::strong_ordering::equal);
}

TEST_CASE("TUniquePtr<T[]>: Default construction yields null pointer", "[Memory][UniquePtr]")
{
    const TUniquePtr<Int32[]> ptr;
    REQUIRE(ptr.Get() == nullptr);
}

TEST_CASE("TUniquePtr<T[]>: Construction from raw pointer takes ownership of array", "[Memory][UniquePtr]")
{
    bool destroyed[2] = { false, false };
    {
        TUniquePtr<FTracked[]> ptr(new FTracked[2]{ FTracked(destroyed[0]), FTracked(destroyed[1]) });
        REQUIRE(ptr.Get() != nullptr);
    }
    REQUIRE(destroyed[0]);
    REQUIRE(destroyed[1]);
}

TEST_CASE("TUniquePtr<T[]>: operator[] accesses elements by index", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32[]> ptr(new Int32[3]{ 10, 20, 30 });
    REQUIRE(ptr[0] == 10);
    REQUIRE(ptr[1] == 20);
    REQUIRE(ptr[2] == 30);
}

TEST_CASE("TUniquePtr<T[]>: operator[] allows mutation", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32[]> ptr(new Int32[3]{ 1, 2, 3 });
    ptr[1] = 99;
    REQUIRE(ptr[1] == 99);
}

TEST_CASE("TUniquePtr<T[]>: Move construction transfers array ownership", "[Memory][UniquePtr]")
{
    Int32* raw = new Int32[4]{ 1, 2, 3, 4 };
    TUniquePtr<Int32[]> src(raw);
    TUniquePtr<Int32[]> dst(GP::Move(src));
    REQUIRE(src.Get() == nullptr);
    REQUIRE(dst.Get() == raw);
}

TEST_CASE("TUniquePtr<T[]>: Move assignment transfers array ownership", "[Memory][UniquePtr]")
{
    Int32* raw = new Int32[2]{ 5, 6 };
    TUniquePtr<Int32[]> src(raw);
    TUniquePtr<Int32[]> dst;
    dst = GP::Move(src);
    REQUIRE(src.Get() == nullptr);
    REQUIRE(dst.Get() == raw);
}

TEST_CASE("TUniquePtr<T[]>: Nullptr assignment resets the array pointer", "[Memory][UniquePtr]")
{
    bool destroyed[2] = { false, false };
    TUniquePtr<FTracked[]> ptr(new FTracked[2]{ FTracked(destroyed[0]), FTracked(destroyed[1]) });
    ptr = nullptr;
    REQUIRE(ptr.Get() == nullptr);
    REQUIRE(destroyed[0]);
    REQUIRE(destroyed[1]);
}

TEST_CASE("TUniquePtr<T[]>: Release yields raw pointer and nulls internal pointer", "[Memory][UniquePtr]")
{
    Int32* raw = new Int32[3]{ 7, 8, 9 };
    TUniquePtr<Int32[]> ptr(raw);
    Int32* released = ptr.Release();
    REQUIRE(released == raw);
    REQUIRE(ptr.Get() == nullptr);
    delete[] released;
}

TEST_CASE("TUniquePtr<T[]>: Reset with new array destroys previous array", "[Memory][UniquePtr]")
{
    bool destroyed[2] = { false, false };
    TUniquePtr<FTracked[]> ptr(new FTracked[2]{ FTracked(destroyed[0]), FTracked(destroyed[1]) });
    bool destroyed2[1] = { false };
    ptr.Reset(new FTracked[1]{ FTracked(destroyed2[0]) });
    REQUIRE(destroyed[0]);
    REQUIRE(destroyed[1]);
    REQUIRE_FALSE(destroyed2[0]);
}

TEST_CASE("TUniquePtr<T[]>: Reset with nullptr destroys array and nulls pointer", "[Memory][UniquePtr]")
{
    bool destroyed[2] = { false, false };
    TUniquePtr<FTracked[]> ptr(new FTracked[2]{ FTracked(destroyed[0]), FTracked(destroyed[1]) });
    ptr.Reset();
    REQUIRE(ptr.Get() == nullptr);
    REQUIRE(destroyed[0]);
    REQUIRE(destroyed[1]);
}

TEST_CASE("TUniquePtr<T[]>: Swap exchanges array pointers", "[Memory][UniquePtr]")
{
    Int32* raw1 = new Int32[2]{ 1, 2 };
    Int32* raw2 = new Int32[3]{ 3, 4, 5 };
    TUniquePtr<Int32[]> a(raw1);
    TUniquePtr<Int32[]> b(raw2);
    a.Swap(b);
    REQUIRE(a.Get() == raw2);
    REQUIRE(b.Get() == raw1);
}

TEST_CASE("TUniquePtr<T[]>: operator bool returns true for non-null array", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32[]> ptr(new Int32[1]{ 0 });
    REQUIRE(static_cast<bool>(ptr));
}

TEST_CASE("TUniquePtr<T[]>: operator bool returns false for null array pointer", "[Memory][UniquePtr]")
{
    TUniquePtr<Int32[]> ptr;
    REQUIRE_FALSE(static_cast<bool>(ptr));
}

TEST_CASE("TUniquePtr<T[]>: Is not copy constructible", "[Memory][UniquePtr]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<TUniquePtr<Int32[]>>);
}

TEST_CASE("TUniquePtr<T[]>: Is not copy assignable", "[Memory][UniquePtr]")
{
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<TUniquePtr<Int32[]>>);
}
