// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the header for the UniquePtr class
#include "memory/pointers/UniquePtr.hpp"

namespace gp::memory::tests
{

struct LifetimeTracker
{
    static int s_activeInstances;
    int value{ 0 };

    LifetimeTracker(int v = 0)
        : value(v)
    {
        ++s_activeInstances;
    }

    LifetimeTracker(const LifetimeTracker&) = delete;
    LifetimeTracker& operator=(const LifetimeTracker&) = delete;

    ~LifetimeTracker()
    {
        --s_activeInstances;
    }
};

int LifetimeTracker::s_activeInstances = 0;

struct Base
{
    virtual ~Base() = default;

    virtual int getValue() const
    {
        return 1;
    }
};

struct Derived : public Base
{
    int getValue() const override
    {
        return 2;
    }
};

struct StatefulDeleter
{
    int* deleteCount;

    constexpr StatefulDeleter(int* counter)
        : deleteCount(counter)
    {}

    template <typename T>
    constexpr void operator()(T* ptr) const noexcept
    {
        if (ptr)
        {
            (*deleteCount)++;
            delete ptr;
        }
    }
};

constexpr bool testConstexprUniquePtr()
{
    // Test basic allocation and default destruction
    UniquePtr<int> ptr1(new int(42));
    if (*ptr1 != 42)
    {
        return false;
    }

    // Test reset
    ptr1.reset(new int(100));
    if (*ptr1 != 100)
    {
        return false;
    }

    // Test move semantics
    UniquePtr<int> ptr2(std::move(ptr1));
    if (ptr1.isValid() || *ptr2 != 100)
    {
        return false;
    }

    // Test release
    int* raw = ptr2.release();
    if (ptr2.isValid() || raw == nullptr)
    {
        return false;
    }
    delete raw;

    // Test factory functions
    auto makePtr = makeUnique<int>(77);
    if (*makePtr != 77)
    {
        return false;
    }

    return true;
}

static_assert(testConstexprUniquePtr(), "Constexpr UniquePtr evaluation failed!");

class UniquePtrTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        LifetimeTracker::s_activeInstances = 0;
    }

    void TearDown() override
    {
        // Ensure no tests leaked memory
        EXPECT_EQ(LifetimeTracker::s_activeInstances, 0) << "Memory leak detected!";
    }
};

TEST_F(UniquePtrTest, DefaultConstruction)
{
    UniquePtr<int> ptr;
    EXPECT_FALSE(ptr.isValid());
    EXPECT_EQ(ptr.get(), nullptr);
}

TEST_F(UniquePtrTest, NullptrConstruction)
{
    UniquePtr<int> ptr(nullptr);
    EXPECT_FALSE(ptr.isValid());
    EXPECT_EQ(ptr.get(), nullptr);
}

TEST_F(UniquePtrTest, PointerConstructionAndDestruction)
{
    {
        UniquePtr<LifetimeTracker> ptr(new LifetimeTracker(42));
        EXPECT_TRUE(ptr.isValid());
        EXPECT_NE(ptr.get(), nullptr);
        EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);
        EXPECT_EQ((*ptr).value, 42);   // Testing operator*
    }
    // Should be destroyed when going out of scope
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);
}

TEST_F(UniquePtrTest, MoveConstruction)
{
    UniquePtr<LifetimeTracker> ptr1(new LifetimeTracker(10));
    UniquePtr<LifetimeTracker> ptr2(std::move(ptr1));

    EXPECT_FALSE(ptr1.isValid());
    EXPECT_EQ(ptr1.get(), nullptr);

    EXPECT_TRUE(ptr2.isValid());
    EXPECT_EQ((*ptr2).value, 10);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);
}

TEST_F(UniquePtrTest, Release)
{
    UniquePtr<LifetimeTracker> ptr(new LifetimeTracker(99));
    LifetimeTracker* raw = ptr.release();

    EXPECT_FALSE(ptr.isValid());
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_EQ(raw->value, 99);

    // Clean up manually since we released ownership
    delete raw;
}

TEST_F(UniquePtrTest, Reset)
{
    UniquePtr<LifetimeTracker> ptr(new LifetimeTracker(1));
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);

    // Reset with new pointer
    ptr.reset(new LifetimeTracker(2));
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);   // Old one deleted, new one created
    EXPECT_EQ((*ptr).value, 2);

    // Reset to nullptr
    ptr.reset();
    EXPECT_FALSE(ptr.isValid());
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 0);
}

TEST_F(UniquePtrTest, CustomDeleter)
{
    int deleteCount = 0;
    {
        StatefulDeleter deleter(&deleteCount);
        UniquePtr<int, StatefulDeleter> ptr(new int(5), deleter);
        EXPECT_EQ(deleteCount, 0);
    }
    EXPECT_EQ(deleteCount, 1);
}

TEST_F(UniquePtrTest, Polymorphism)
{
    UniquePtr<Base> ptr(new Derived());
    EXPECT_EQ(ptr->getValue(), 2);

    // Testing polymorphic move
    UniquePtr<Base> ptr2(std::move(ptr));
    EXPECT_EQ(ptr2->getValue(), 2);
}

TEST_F(UniquePtrTest, Comparisons)
{
    UniquePtr<int> ptr1(new int(1));
    UniquePtr<int> ptr2(new int(2));
    UniquePtr<int> ptrNull;

    // Compare with nullptr
    EXPECT_TRUE(ptrNull == nullptr);
    EXPECT_FALSE(ptr1 == nullptr);
    EXPECT_TRUE(nullptr == ptrNull);

    // Three-way comparison with nullptr
    EXPECT_TRUE((ptr1 <=> nullptr) > 0);
    EXPECT_TRUE((ptrNull <=> nullptr) == 0);

    // Compare two UniquePtrs
    EXPECT_TRUE(ptr1 != ptr2);
    EXPECT_FALSE(ptr1 == ptr2);

    // Test strong ordering (relies on underlying pointer addresses)
    if (ptr1.get() < ptr2.get())
    {
        EXPECT_TRUE((ptr1 <=> ptr2) < 0);
    }
    else
    {
        EXPECT_TRUE((ptr1 <=> ptr2) > 0);
    }
}

TEST_F(UniquePtrTest, FactoryMakeUnique)
{
    auto ptr = makeUnique<LifetimeTracker>(7);
    EXPECT_TRUE(ptr.isValid());
    EXPECT_EQ((*ptr).value, 7);
    EXPECT_EQ(LifetimeTracker::s_activeInstances, 1);
}

TEST_F(UniquePtrTest, FactoryMakeUniqueForOverwrite)
{
    auto ptr = makeUniqueForOverwrite<int>();
    EXPECT_TRUE(ptr.isValid());
}

}   // namespace gp::memory::tests
