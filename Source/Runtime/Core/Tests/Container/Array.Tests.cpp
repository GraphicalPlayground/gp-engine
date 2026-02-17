// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Array.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace GP;
using namespace GP::Container;

// Helper struct for testing
struct TestObject
{
    int value;
    static int constructorCalls;
    static int destructorCalls;
    static int copyConstructorCalls;
    static int moveConstructorCalls;

    TestObject()
        : value(0)
    {
        ++constructorCalls;
    }

    explicit TestObject(int v)
        : value(v)
    {
        ++constructorCalls;
    }

    TestObject(const TestObject& other)
        : value(other.value)
    {
        ++copyConstructorCalls;
    }

    TestObject(TestObject&& other) noexcept
        : value(other.value)
    {
        other.value = -1;
        ++moveConstructorCalls;
    }

    ~TestObject() { ++destructorCalls; }

    TestObject& operator=(const TestObject& other)
    {
        value = other.value;
        return *this;
    }

    TestObject& operator=(TestObject&& other) noexcept
    {
        value = other.value;
        other.value = -1;
        return *this;
    }

    bool operator==(const TestObject& other) const { return value == other.value; }

    bool operator!=(const TestObject& other) const { return value != other.value; }

    static void Reset()
    {
        constructorCalls = 0;
        destructorCalls = 0;
        copyConstructorCalls = 0;
        moveConstructorCalls = 0;
    }
};

int TestObject::constructorCalls = 0;
int TestObject::destructorCalls = 0;
int TestObject::copyConstructorCalls = 0;
int TestObject::moveConstructorCalls = 0;

TEST_CASE("TArray Construction", "[GP][Core][Container][TArray]")
{
    SECTION("Default Constructor")
    {
        TArray<int> arr;
        REQUIRE(arr.Size() == 0);
        REQUIRE(arr.Capacity() == 0);
        REQUIRE(arr.IsEmpty());
        REQUIRE(arr.Data() == nullptr);
    }

    SECTION("Constructor with Allocator")
    {
        GP::Memory::TAllocator<int> alloc;
        TArray<int> arr(alloc);
        REQUIRE(arr.Size() == 0);
        REQUIRE(arr.Capacity() == 0);
        REQUIRE(arr.IsEmpty());
    }

    SECTION("Constructor with Count")
    {
        TArray<int> arr(5);
        REQUIRE(arr.Size() == 5);
        REQUIRE(arr.Capacity() >= 5);
        REQUIRE_FALSE(arr.IsEmpty());
        for (SizeT i = 0; i < arr.Size(); ++i) { REQUIRE(arr[i] == 0); }
    }

    SECTION("Constructor with Count and Value")
    {
        TArray<int> arr(5, 42);
        REQUIRE(arr.Size() == 5);
        REQUIRE(arr.Capacity() >= 5);
        for (SizeT i = 0; i < arr.Size(); ++i) { REQUIRE(arr[i] == 42); }
    }

    SECTION("Constructor with Initializer List")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        REQUIRE(arr.Size() == 5);
        REQUIRE(arr.Capacity() >= 5);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
        REQUIRE(arr[3] == 4);
        REQUIRE(arr[4] == 5);
    }

    SECTION("Constructor with Iterator Range")
    {
        std::vector<int> vec = { 10, 20, 30, 40 };
        TArray<int> arr(vec.begin(), vec.end());
        REQUIRE(arr.Size() == 4);
        REQUIRE(arr[0] == 10);
        REQUIRE(arr[1] == 20);
        REQUIRE(arr[2] == 30);
        REQUIRE(arr[3] == 40);
    }

    SECTION("Copy Constructor")
    {
        TArray<int> arr1 = { 1, 2, 3 };
        TArray<int> arr2(arr1);
        REQUIRE(arr2.Size() == 3);
        REQUIRE(arr2[0] == 1);
        REQUIRE(arr2[1] == 2);
        REQUIRE(arr2[2] == 3);
        REQUIRE(arr1.Size() == 3);   // Original unchanged
    }

    SECTION("Copy Constructor with Empty Array")
    {
        TArray<int> arr1;
        TArray<int> arr2(arr1);
        REQUIRE(arr2.Size() == 0);
        REQUIRE(arr2.IsEmpty());
    }

    SECTION("Move Constructor")
    {
        TArray<int> arr1 = { 1, 2, 3 };
        int* originalData = arr1.Data();
        SizeT originalSize = arr1.Size();
        SizeT originalCapacity = arr1.Capacity();

        TArray<int> arr2(std::move(arr1));
        REQUIRE(arr2.Size() == originalSize);
        REQUIRE(arr2.Capacity() == originalCapacity);
        REQUIRE(arr2.Data() == originalData);
        REQUIRE(arr2[0] == 1);
        REQUIRE(arr2[1] == 2);
        REQUIRE(arr2[2] == 3);

        // Original array should be in moved-from state
        REQUIRE(arr1.Size() == 0);
        REQUIRE(arr1.Capacity() == 0);
        REQUIRE(arr1.Data() == nullptr);
    }
}

TEST_CASE("TArray Assignment Operators", "[GP][Core][Container][TArray]")
{
    SECTION("Copy Assignment")
    {
        TArray<int> arr1 = { 1, 2, 3 };
        TArray<int> arr2;
        arr2 = arr1;
        REQUIRE(arr2.Size() == 3);
        REQUIRE(arr2[0] == 1);
        REQUIRE(arr2[1] == 2);
        REQUIRE(arr2[2] == 3);
        REQUIRE(arr1.Size() == 3);   // Original unchanged
    }

    SECTION("Copy Assignment to Non-Empty Array")
    {
        TArray<int> arr1 = { 1, 2, 3 };
        TArray<int> arr2 = { 10, 20 };
        arr2 = arr1;
        REQUIRE(arr2.Size() == 3);
        REQUIRE(arr2[0] == 1);
        REQUIRE(arr2[1] == 2);
        REQUIRE(arr2[2] == 3);
    }

    SECTION("Self Copy Assignment")
    {
        TArray<int> arr = { 1, 2, 3 };
        arr = arr;
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
    }

    SECTION("Move Assignment")
    {
        TArray<int> arr1 = { 1, 2, 3 };
        TArray<int> arr2;
        int* originalData = arr1.Data();
        SizeT originalSize = arr1.Size();
        SizeT originalCapacity = arr1.Capacity();

        arr2 = std::move(arr1);
        REQUIRE(arr2.Size() == originalSize);
        REQUIRE(arr2.Capacity() == originalCapacity);
        REQUIRE(arr2.Data() == originalData);
        REQUIRE(arr2[0] == 1);

        REQUIRE(arr1.Size() == 0);
        REQUIRE(arr1.Capacity() == 0);
    }

    SECTION("Move Assignment to Non-Empty Array")
    {
        TArray<int> arr1 = { 1, 2, 3 };
        TArray<int> arr2 = { 10, 20, 30, 40 };
        arr2 = std::move(arr1);
        REQUIRE(arr2.Size() == 3);
        REQUIRE(arr2[0] == 1);
        REQUIRE(arr2[1] == 2);
        REQUIRE(arr2[2] == 3);
    }

    SECTION("Self Move Assignment")
    {
        TArray<int> arr = { 1, 2, 3 };
        arr = std::move(arr);
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 1);
    }

    SECTION("Initializer List Assignment")
    {
        TArray<int> arr = { 1, 2 };
        arr = { 10, 20, 30 };
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 10);
        REQUIRE(arr[1] == 20);
        REQUIRE(arr[2] == 30);
    }
}

TEST_CASE("TArray Element Access", "[GP][Core][Container][TArray]")
{
    SECTION("Operator[] Non-Const")
    {
        TArray<int> arr = { 1, 2, 3 };
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
        arr[1] = 42;
        REQUIRE(arr[1] == 42);
    }

    SECTION("Operator[] Const")
    {
        const TArray<int> arr = { 1, 2, 3 };
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
    }

    SECTION("At() Method")
    {
        TArray<int> arr = { 1, 2, 3 };
        REQUIRE(arr.At(0) == 1);
        REQUIRE(arr.At(1) == 2);
        REQUIRE(arr.At(2) == 3);
        arr.At(1) = 42;
        REQUIRE(arr.At(1) == 42);
    }

    SECTION("At() Method Const")
    {
        const TArray<int> arr = { 1, 2, 3 };
        REQUIRE(arr.At(0) == 1);
        REQUIRE(arr.At(1) == 2);
        REQUIRE(arr.At(2) == 3);
    }

    SECTION("Front() Method")
    {
        TArray<int> arr = { 1, 2, 3 };
        REQUIRE(arr.Front() == 1);
        arr.Front() = 42;
        REQUIRE(arr.Front() == 42);
    }

    SECTION("Front() Method Const")
    {
        const TArray<int> arr = { 1, 2, 3 };
        REQUIRE(arr.Front() == 1);
    }

    SECTION("Back() Method")
    {
        TArray<int> arr = { 1, 2, 3 };
        REQUIRE(arr.Back() == 3);
        arr.Back() = 42;
        REQUIRE(arr.Back() == 42);
    }

    SECTION("Back() Method Const")
    {
        const TArray<int> arr = { 1, 2, 3 };
        REQUIRE(arr.Back() == 3);
    }

    SECTION("Data() Method")
    {
        TArray<int> arr = { 1, 2, 3 };
        int* data = arr.Data();
        REQUIRE(data != nullptr);
        REQUIRE(data[0] == 1);
        REQUIRE(data[1] == 2);
        REQUIRE(data[2] == 3);
    }

    SECTION("Data() Method Const")
    {
        const TArray<int> arr = { 1, 2, 3 };
        const int* data = arr.Data();
        REQUIRE(data != nullptr);
        REQUIRE(data[0] == 1);
    }

    SECTION("Data() on Empty Array")
    {
        TArray<int> arr;
        REQUIRE(arr.Data() == nullptr);
    }
}

TEST_CASE("TArray Capacity Methods", "[GP][Core][Container][TArray]")
{
    SECTION("Size() and IsEmpty()")
    {
        TArray<int> arr;
        REQUIRE(arr.Size() == 0);
        REQUIRE(arr.IsEmpty());

        arr.PushBack(1);
        REQUIRE(arr.Size() == 1);
        REQUIRE_FALSE(arr.IsEmpty());

        arr.PushBack(2);
        REQUIRE(arr.Size() == 2);
        REQUIRE_FALSE(arr.IsEmpty());
    }

    SECTION("Capacity() and Reserve()")
    {
        TArray<int> arr;
        REQUIRE(arr.Capacity() == 0);

        arr.Reserve(10);
        REQUIRE(arr.Capacity() >= 10);
        REQUIRE(arr.Size() == 0);

        // Reserve with smaller capacity should not reduce capacity
        SizeT currentCapacity = arr.Capacity();
        arr.Reserve(5);
        REQUIRE(arr.Capacity() == currentCapacity);
    }

    SECTION("Reserve() with Elements")
    {
        TArray<int> arr = { 1, 2, 3 };
        arr.Reserve(10);
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr.Capacity() >= 10);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
    }

    SECTION("ShrinkToFit()")
    {
        TArray<int> arr;
        arr.Reserve(100);
        arr.PushBack(1);
        arr.PushBack(2);
        arr.PushBack(3);
        REQUIRE(arr.Capacity() >= 100);

        arr.ShrinkToFit();
        REQUIRE(arr.Capacity() == arr.Size());
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
    }

    SECTION("ShrinkToFit() on Empty Array")
    {
        TArray<int> arr;
        arr.Reserve(10);
        arr.ShrinkToFit();
        REQUIRE(arr.Capacity() == 0);
    }

    SECTION("Clear()")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        SizeT capacity = arr.Capacity();
        arr.Clear();
        REQUIRE(arr.Size() == 0);
        REQUIRE(arr.IsEmpty());
        REQUIRE(arr.Capacity() == capacity);   // Capacity unchanged
    }

    SECTION("Clear() with TestObject")
    {
        TestObject::Reset();
        {
            TArray<TestObject> arr;
            arr.EmplaceBack(1);
            arr.EmplaceBack(2);
            arr.EmplaceBack(3);
            int constructedCount = TestObject::constructorCalls;

            arr.Clear();
            REQUIRE(arr.Size() == 0);
            REQUIRE(TestObject::destructorCalls == constructedCount);
        }
    }

    SECTION("MaxSize()")
    {
        TArray<int> arr;
        REQUIRE(arr.MaxSize() > 0);
        REQUIRE(arr.MaxSize() == static_cast<SizeT>(-1) / sizeof(int));
    }
}

TEST_CASE("TArray PushBack and EmplaceBack", "[GP][Core][Container][TArray]")
{
    SECTION("PushBack Copy")
    {
        TArray<int> arr;
        int value = 42;
        arr.PushBack(value);
        REQUIRE(arr.Size() == 1);
        REQUIRE(arr[0] == 42);
        REQUIRE(value == 42);   // Original unchanged
    }

    SECTION("PushBack Move")
    {
        TArray<std::string> arr;
        std::string str = "Hello";
        arr.PushBack(std::move(str));
        REQUIRE(arr.Size() == 1);
        REQUIRE(arr[0] == "Hello");
    }

    SECTION("PushBack Multiple Elements")
    {
        TArray<int> arr;
        for (int i = 0; i < 100; ++i) { arr.PushBack(i); }
        REQUIRE(arr.Size() == 100);
        for (int i = 0; i < 100; ++i) { REQUIRE(arr[i] == i); }
    }

    SECTION("EmplaceBack")
    {
        TArray<std::string> arr;
        arr.EmplaceBack("Hello");
        arr.EmplaceBack(5, 'x');   // String with 5 'x' characters
        REQUIRE(arr.Size() == 2);
        REQUIRE(arr[0] == "Hello");
        REQUIRE(arr[1] == "xxxxx");
    }

    SECTION("EmplaceBack Returns Reference")
    {
        TArray<int> arr;
        int& ref = arr.EmplaceBack(42);
        REQUIRE(ref == 42);
        ref = 100;
        REQUIRE(arr[0] == 100);
    }

    SECTION("PushBack Triggers Growth")
    {
        TArray<int> arr;
        arr.Reserve(2);
        arr.PushBack(1);
        arr.PushBack(2);
        SizeT oldCapacity = arr.Capacity();
        arr.PushBack(3);   // Should trigger growth
        REQUIRE(arr.Capacity() > oldCapacity);
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[2] == 3);
    }
}

TEST_CASE("TArray PopBack", "[GP][Core][Container][TArray]")
{
    SECTION("PopBack Single Element")
    {
        TArray<int> arr = { 1, 2, 3 };
        arr.PopBack();
        REQUIRE(arr.Size() == 2);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
    }

    SECTION("PopBack All Elements")
    {
        TArray<int> arr = { 1, 2, 3 };
        arr.PopBack();
        arr.PopBack();
        arr.PopBack();
        REQUIRE(arr.Size() == 0);
        REQUIRE(arr.IsEmpty());
    }

    SECTION("PopBack with TestObject")
    {
        TestObject::Reset();
        {
            TArray<TestObject> arr;
            arr.EmplaceBack(1);
            arr.EmplaceBack(2);
            int constructedCount = TestObject::constructorCalls;

            arr.PopBack();
            REQUIRE(arr.Size() == 1);
            REQUIRE(TestObject::destructorCalls == 1);
        }
    }
}

TEST_CASE("TArray Resize", "[GP][Core][Container][TArray]")
{
    SECTION("Resize Up")
    {
        TArray<int> arr = { 1, 2, 3 };
        arr.Resize(5);
        REQUIRE(arr.Size() == 5);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
        REQUIRE(arr[3] == 0);
        REQUIRE(arr[4] == 0);
    }

    SECTION("Resize Down")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        arr.Resize(3);
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
    }

    SECTION("Resize to Zero")
    {
        TArray<int> arr = { 1, 2, 3 };
        arr.Resize(0);
        REQUIRE(arr.Size() == 0);
        REQUIRE(arr.IsEmpty());
    }

    SECTION("Resize with Value")
    {
        TArray<int> arr = { 1, 2, 3 };
        arr.Resize(5, 42);
        REQUIRE(arr.Size() == 5);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
        REQUIRE(arr[3] == 42);
        REQUIRE(arr[4] == 42);
    }

    SECTION("Resize Empty Array")
    {
        TArray<int> arr;
        arr.Resize(3);
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 0);
        REQUIRE(arr[1] == 0);
        REQUIRE(arr[2] == 0);
    }

    SECTION("Resize Empty Array with Value")
    {
        TArray<int> arr;
        arr.Resize(3, 99);
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 99);
        REQUIRE(arr[1] == 99);
        REQUIRE(arr[2] == 99);
    }
}

TEST_CASE("TArray Insert and Emplace", "[GP][Core][Container][TArray]")
{
    SECTION("Insert at Beginning")
    {
        TArray<int> arr = { 1, 2, 3 };
        arr.Insert(arr.Begin(), 0);
        REQUIRE(arr.Size() == 4);
        REQUIRE(arr[0] == 0);
        REQUIRE(arr[1] == 1);
        REQUIRE(arr[2] == 2);
        REQUIRE(arr[3] == 3);
    }

    SECTION("Insert in Middle")
    {
        TArray<int> arr = { 1, 2, 4 };
        auto it = arr.Begin() + 2;
        arr.Insert(it, 3);
        REQUIRE(arr.Size() == 4);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
        REQUIRE(arr[3] == 4);
    }

    SECTION("Insert at End")
    {
        TArray<int> arr = { 1, 2, 3 };
        arr.Insert(arr.End(), 4);
        REQUIRE(arr.Size() == 4);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
        REQUIRE(arr[3] == 4);
    }

    SECTION("Insert Move")
    {
        TArray<std::string> arr = { "a", "c" };
        std::string str = "b";
        arr.Insert(arr.Begin() + 1, std::move(str));
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == "a");
        REQUIRE(arr[1] == "b");
        REQUIRE(arr[2] == "c");
    }

    SECTION("Emplace in Middle")
    {
        TArray<std::string> arr = { "Hello", "World" };
        arr.Emplace(arr.Begin() + 1, 3, 'x');
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == "Hello");
        REQUIRE(arr[1] == "xxx");
        REQUIRE(arr[2] == "World");
    }

    SECTION("Insert Returns Iterator")
    {
        TArray<int> arr = { 1, 2, 3 };
        auto it = arr.Insert(arr.Begin() + 1, 99);
        REQUIRE(*it == 99);
        REQUIRE(it == arr.Begin() + 1);
    }

    SECTION("Insert into Empty Array")
    {
        TArray<int> arr;
        arr.Insert(arr.Begin(), 42);
        REQUIRE(arr.Size() == 1);
        REQUIRE(arr[0] == 42);
    }

    SECTION("Insert Triggers Reallocation")
    {
        TArray<int> arr;
        arr.Reserve(2);
        arr.PushBack(1);
        arr.PushBack(2);
        arr.Insert(arr.Begin(), 0);   // Should trigger reallocation
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 0);
        REQUIRE(arr[1] == 1);
        REQUIRE(arr[2] == 2);
    }
}

TEST_CASE("TArray Erase", "[GP][Core][Container][TArray]")
{
    SECTION("Erase Beginning")
    {
        TArray<int> arr = { 1, 2, 3, 4 };
        auto it = arr.Erase(arr.Begin());
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 2);
        REQUIRE(arr[1] == 3);
        REQUIRE(arr[2] == 4);
        REQUIRE(*it == 2);
    }

    SECTION("Erase Middle")
    {
        TArray<int> arr = { 1, 2, 3, 4 };
        auto it = arr.Erase(arr.Begin() + 1);
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 3);
        REQUIRE(arr[2] == 4);
        REQUIRE(*it == 3);
    }

    SECTION("Erase End")
    {
        TArray<int> arr = { 1, 2, 3, 4 };
        auto it = arr.Erase(arr.Begin() + 3);
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
        REQUIRE(it == arr.End());
    }

    SECTION("Erase Range")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it = arr.Erase(arr.Begin() + 1, arr.Begin() + 4);
        REQUIRE(arr.Size() == 2);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 5);
        REQUIRE(*it == 5);
    }

    SECTION("Erase Range to End")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it = arr.Erase(arr.Begin() + 2, arr.End());
        REQUIRE(arr.Size() == 2);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(it == arr.End());
    }

    SECTION("Erase Empty Range")
    {
        TArray<int> arr = { 1, 2, 3 };
        auto it = arr.Erase(arr.Begin() + 1, arr.Begin() + 1);
        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
    }

    SECTION("Erase All Elements")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it = arr.Erase(arr.Begin(), arr.End());
        REQUIRE(arr.Size() == 0);
        REQUIRE(arr.IsEmpty());
        REQUIRE(it == arr.End());
    }
}

TEST_CASE("TArray EraseSwapBack", "[GP][Core][Container][TArray]")
{
    SECTION("EraseSwapBack First Element")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        arr.EraseSwapBack(0);
        REQUIRE(arr.Size() == 4);
        REQUIRE(arr[0] == 5);   // Last element moved to first position
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
        REQUIRE(arr[3] == 4);
    }

    SECTION("EraseSwapBack Middle Element")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        arr.EraseSwapBack(2);
        REQUIRE(arr.Size() == 4);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 5);   // Last element moved here
        REQUIRE(arr[3] == 4);
    }

    SECTION("EraseSwapBack Last Element")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        arr.EraseSwapBack(4);
        REQUIRE(arr.Size() == 4);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 3);
        REQUIRE(arr[3] == 4);
    }

    SECTION("EraseSwapBack Single Element Array")
    {
        TArray<int> arr = { 42 };
        arr.EraseSwapBack(0);
        REQUIRE(arr.Size() == 0);
        REQUIRE(arr.IsEmpty());
    }
}

TEST_CASE("TArray Swap", "[GP][Core][Container][TArray]")
{
    SECTION("Swap Two Arrays")
    {
        TArray<int> arr1 = { 1, 2, 3 };
        TArray<int> arr2 = { 4, 5, 6, 7 };

        arr1.Swap(arr2);

        REQUIRE(arr1.Size() == 4);
        REQUIRE(arr1[0] == 4);
        REQUIRE(arr1[1] == 5);
        REQUIRE(arr1[2] == 6);
        REQUIRE(arr1[3] == 7);

        REQUIRE(arr2.Size() == 3);
        REQUIRE(arr2[0] == 1);
        REQUIRE(arr2[1] == 2);
        REQUIRE(arr2[2] == 3);
    }

    SECTION("Swap with Empty Array")
    {
        TArray<int> arr1 = { 1, 2, 3 };
        TArray<int> arr2;

        arr1.Swap(arr2);

        REQUIRE(arr1.Size() == 0);
        REQUIRE(arr1.IsEmpty());

        REQUIRE(arr2.Size() == 3);
        REQUIRE(arr2[0] == 1);
        REQUIRE(arr2[1] == 2);
        REQUIRE(arr2[2] == 3);
    }

    SECTION("Swap Two Empty Arrays")
    {
        TArray<int> arr1;
        TArray<int> arr2;

        arr1.Swap(arr2);

        REQUIRE(arr1.Size() == 0);
        REQUIRE(arr2.Size() == 0);
    }
}

TEST_CASE("TArray Find Methods", "[GP][Core][Container][TArray]")
{
    SECTION("Find Existing Element")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it = arr.Find(3);
        REQUIRE(it != arr.End());
        REQUIRE(*it == 3);
        REQUIRE(it == arr.Begin() + 2);
    }

    SECTION("Find Non-Existing Element")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it = arr.Find(10);
        REQUIRE(it == arr.End());
    }

    SECTION("Find in Empty Array")
    {
        TArray<int> arr;
        auto it = arr.Find(1);
        REQUIRE(it == arr.End());
    }

    SECTION("Find Const")
    {
        const TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it = arr.Find(3);
        REQUIRE(it != arr.End());
        REQUIRE(*it == 3);
    }

    SECTION("FindFirstOf")
    {
        TArray<int> arr = { 1, 2, 3, 2, 1 };
        auto it = arr.FindFirstOf(2);
        REQUIRE(it != arr.End());
        REQUIRE(*it == 2);
        REQUIRE(it == arr.Begin() + 1);
    }

    SECTION("FindLastOf")
    {
        TArray<int> arr = { 1, 2, 3, 2, 1 };
        auto it = arr.FindLastOf(2);
        REQUIRE(it != arr.End());
        REQUIRE(*it == 2);
        REQUIRE(it == arr.Begin() + 3);
    }

    SECTION("FindLastOf Single Occurrence")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it = arr.FindLastOf(3);
        REQUIRE(it != arr.End());
        REQUIRE(*it == 3);
        REQUIRE(it == arr.Begin() + 2);
    }

    SECTION("FindFirstNotOf")
    {
        TArray<int> arr = { 1, 1, 1, 2, 3 };
        auto it = arr.FindFirstNotOf(1);
        REQUIRE(it != arr.End());
        REQUIRE(*it == 2);
        REQUIRE(it == arr.Begin() + 3);
    }

    SECTION("FindFirstNotOf All Same")
    {
        TArray<int> arr = { 1, 1, 1, 1 };
        auto it = arr.FindFirstNotOf(1);
        REQUIRE(it == arr.End());
    }

    SECTION("FindLastNotOf")
    {
        TArray<int> arr = { 1, 2, 3, 3, 3 };
        auto it = arr.FindLastNotOf(3);
        REQUIRE(it != arr.End());
        REQUIRE(*it == 2);
        REQUIRE(it == arr.Begin() + 1);
    }

    SECTION("FindLastNotOf All Same")
    {
        TArray<int> arr = { 1, 1, 1, 1 };
        auto it = arr.FindLastNotOf(1);
        REQUIRE(it == arr.End());
    }
}

TEST_CASE("TArray Contains and Index Methods", "[GP][Core][Container][TArray]")
{
    SECTION("Contains Existing Element")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        REQUIRE(arr.Contains(3));
        REQUIRE(arr.Contains(1));
        REQUIRE(arr.Contains(5));
    }

    SECTION("Contains Non-Existing Element")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        REQUIRE_FALSE(arr.Contains(10));
        REQUIRE_FALSE(arr.Contains(0));
    }

    SECTION("Contains in Empty Array")
    {
        TArray<int> arr;
        REQUIRE_FALSE(arr.Contains(1));
    }

    SECTION("IndexOf")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        REQUIRE(arr.IndexOf(1) == 0);
        REQUIRE(arr.IndexOf(3) == 2);
        REQUIRE(arr.IndexOf(5) == 4);
        REQUIRE(arr.IndexOf(10) == TArray<int>::NPOS);
    }

    SECTION("IndexFirstOf")
    {
        TArray<int> arr = { 1, 2, 3, 2, 1 };
        REQUIRE(arr.IndexFirstOf(2) == 1);
        REQUIRE(arr.IndexFirstOf(1) == 0);
    }

    SECTION("IndexLastOf")
    {
        TArray<int> arr = { 1, 2, 3, 2, 1 };
        REQUIRE(arr.IndexLastOf(2) == 3);
        REQUIRE(arr.IndexLastOf(1) == 4);
        REQUIRE(arr.IndexLastOf(3) == 2);
    }

    SECTION("IndexFirstNotOf")
    {
        TArray<int> arr = { 1, 1, 1, 2, 3 };
        REQUIRE(arr.IndexFirstNotOf(1) == 3);
        REQUIRE(arr.IndexFirstNotOf(2) == 0);
    }

    SECTION("IndexFirstNotOf All Same")
    {
        TArray<int> arr = { 1, 1, 1 };
        REQUIRE(arr.IndexFirstNotOf(1) == TArray<int>::NPOS);
    }

    SECTION("IndexLastNotOf")
    {
        TArray<int> arr = { 1, 2, 3, 3, 3 };
        REQUIRE(arr.IndexLastNotOf(3) == 1);
        REQUIRE(arr.IndexLastNotOf(1) == 4);
    }

    SECTION("IndexLastNotOf All Same")
    {
        TArray<int> arr = { 1, 1, 1 };
        REQUIRE(arr.IndexLastNotOf(1) == TArray<int>::NPOS);
    }
}

TEST_CASE("TArray Iterators", "[GP][Core][Container][TArray]")
{
    SECTION("Begin and End")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it = arr.Begin();
        REQUIRE(*it == 1);
        ++it;
        REQUIRE(*it == 2);
        it += 2;
        REQUIRE(*it == 4);
        ++it;
        ++it;
        REQUIRE(it == arr.End());
    }

    SECTION("CBegin and CEnd")
    {
        TArray<int> arr = { 1, 2, 3 };
        auto it = arr.CBegin();
        REQUIRE(*it == 1);
        ++it;
        REQUIRE(*it == 2);
        ++it;
        REQUIRE(*it == 3);
        ++it;
        REQUIRE(it == arr.CEnd());
    }

    SECTION("Range-Based For Loop")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        int sum = 0;
        for (int val: arr) { sum += val; }
        REQUIRE(sum == 15);
    }

    SECTION("Range-Based For Loop Modify")
    {
        TArray<int> arr = { 1, 2, 3 };
        for (int& val: arr) { val *= 2; }
        REQUIRE(arr[0] == 2);
        REQUIRE(arr[1] == 4);
        REQUIRE(arr[2] == 6);
    }

    SECTION("Reverse Iterators")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it = arr.RBegin();
        REQUIRE(*it == 5);
        ++it;
        REQUIRE(*it == 4);
        ++it;
        REQUIRE(*it == 3);
    }

    SECTION("Const Reverse Iterators")
    {
        const TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it = arr.CRBegin();
        REQUIRE(*it == 5);
        ++it;
        REQUIRE(*it == 4);
    }

    SECTION("STL-style Iterators")
    {
        TArray<int> arr = { 1, 2, 3 };
        auto it = arr.begin();
        REQUIRE(*it == 1);
        ++it;
        REQUIRE(*it == 2);
        ++it;
        REQUIRE(*it == 3);
        ++it;
        REQUIRE(it == arr.end());
    }

    SECTION("STL Algorithms with Iterators")
    {
        TArray<int> arr = { 5, 2, 8, 1, 9 };
        std::sort(arr.begin(), arr.end());
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 2);
        REQUIRE(arr[2] == 5);
        REQUIRE(arr[3] == 8);
        REQUIRE(arr[4] == 9);
    }

    SECTION("Iterator Arithmetic")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };
        auto it1 = arr.Begin();
        auto it2 = arr.Begin() + 3;
        REQUIRE(*it2 == 4);
        REQUIRE(it2 - it1 == 3);
    }
}

TEST_CASE("TArray Edge Cases", "[GP][Core][Container][TArray]")
{
    SECTION("Empty Array Operations")
    {
        TArray<int> arr;
        REQUIRE(arr.IsEmpty());
        REQUIRE(arr.Size() == 0);
        REQUIRE(arr.Begin() == arr.End());
        REQUIRE(arr.Find(1) == arr.End());
        REQUIRE_FALSE(arr.Contains(1));
    }

    SECTION("Single Element Array")
    {
        TArray<int> arr = { 42 };
        REQUIRE(arr.Size() == 1);
        REQUIRE(arr.Front() == 42);
        REQUIRE(arr.Back() == 42);
        REQUIRE(arr[0] == 42);
        REQUIRE(arr.Contains(42));
    }

    SECTION("Large Array")
    {
        TArray<int> arr;
        const int size = 10000;
        for (int i = 0; i < size; ++i) { arr.PushBack(i); }
        REQUIRE(arr.Size() == size);
        REQUIRE(arr[0] == 0);
        REQUIRE(arr[size / 2] == size / 2);
        REQUIRE(arr[size - 1] == size - 1);
    }

    SECTION("Alternating Push and Pop")
    {
        TArray<int> arr;
        for (int i = 0; i < 100; ++i)
        {
            arr.PushBack(i);
            if (i % 2 == 1) { arr.PopBack(); }
        }
        REQUIRE(arr.Size() == 50);
    }

    SECTION("Multiple Reallocations")
    {
        TArray<int> arr;
        arr.Reserve(1);
        for (int i = 0; i < 100; ++i) { arr.PushBack(i); }
        REQUIRE(arr.Size() == 100);
        for (int i = 0; i < 100; ++i) { REQUIRE(arr[i] == i); }
    }

    SECTION("Copy After Modification")
    {
        TArray<int> arr1 = { 1, 2, 3 };
        arr1.PushBack(4);
        arr1.PopBack();
        arr1.Resize(5, 10);

        TArray<int> arr2 = arr1;
        REQUIRE(arr2.Size() == arr1.Size());
        for (SizeT i = 0; i < arr1.Size(); ++i) { REQUIRE(arr2[i] == arr1[i]); }
    }

    SECTION("Move After Modification")
    {
        TArray<int> arr1 = { 1, 2, 3 };
        arr1.PushBack(4);
        arr1.PopBack();
        arr1.Resize(5, 10);

        SizeT originalSize = arr1.Size();
        TArray<int> arr2 = std::move(arr1);
        REQUIRE(arr2.Size() == originalSize);
        REQUIRE(arr1.Size() == 0);
    }

    SECTION("Chained Operations")
    {
        TArray<int> arr;
        arr.PushBack(1);
        arr.PushBack(2);
        arr.PushBack(3);
        arr.Insert(arr.Begin() + 1, 10);
        arr.Erase(arr.Begin() + 2);
        arr.Resize(5, 99);

        REQUIRE(arr.Size() == 5);
        REQUIRE(arr[0] == 1);
        REQUIRE(arr[1] == 10);
        REQUIRE(arr[2] == 3);
        REQUIRE(arr[3] == 99);
        REQUIRE(arr[4] == 99);
    }
}

TEST_CASE("TArray with Complex Types", "[GP][Core][Container][TArray]")
{
    SECTION("Array of Strings")
    {
        TArray<std::string> arr;
        arr.PushBack("Hello");
        arr.PushBack("World");
        arr.EmplaceBack(5, '!');

        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0] == "Hello");
        REQUIRE(arr[1] == "World");
        REQUIRE(arr[2] == "!!!!!");
    }

    SECTION("Array of Arrays")
    {
        TArray<TArray<int>> arr;
        arr.PushBack({ 1, 2, 3 });
        arr.PushBack({ 4, 5, 6 });
        arr.EmplaceBack();
        arr[2].PushBack(7);

        REQUIRE(arr.Size() == 3);
        REQUIRE(arr[0].Size() == 3);
        REQUIRE(arr[1].Size() == 3);
        REQUIRE(arr[2].Size() == 1);
        REQUIRE(arr[0][0] == 1);
        REQUIRE(arr[1][2] == 6);
        REQUIRE(arr[2][0] == 7);
    }

    SECTION("Move-Only Type")
    {
        TArray<std::unique_ptr<int>> arr;
        arr.PushBack(std::make_unique<int>(42));
        arr.EmplaceBack(std::make_unique<int>(99));

        REQUIRE(arr.Size() == 2);
        REQUIRE(*arr[0] == 42);
        REQUIRE(*arr[1] == 99);

        auto moved = std::move(arr);
        REQUIRE(moved.Size() == 2);
        REQUIRE(*moved[0] == 42);
    }

    SECTION("TestObject Lifecycle")
    {
        TestObject::Reset();
        {
            TArray<TestObject> arr;
            arr.EmplaceBack(1);
            arr.EmplaceBack(2);
            arr.EmplaceBack(3);

            REQUIRE(TestObject::constructorCalls == 3);

            arr.PopBack();
            REQUIRE(TestObject::destructorCalls == 1);

            arr.Resize(5);
            REQUIRE(TestObject::constructorCalls == 6);
        }
        // All objects should be destroyed when arr goes out of scope
        // Destructor count: 1 (PopBack) + 2 (reallocation during Resize) + 5 (final cleanup) = 8
        REQUIRE(TestObject::destructorCalls == 8);
    }

    SECTION("TestObject Move Semantics")
    {
        TestObject::Reset();
        {
            TArray<TestObject> arr1;
            arr1.EmplaceBack(1);
            arr1.EmplaceBack(2);

            TArray<TestObject> arr2 = std::move(arr1);

            // Move constructor should be used for array movement
            REQUIRE(arr2.Size() == 2);
            REQUIRE(arr1.Size() == 0);
        }
    }
}

TEST_CASE("TArray GetAllocator", "[GP][Core][Container][TArray]")
{
    SECTION("GetAllocator Non-Const")
    {
        TArray<int> arr;
        auto& alloc = arr.GetAllocator();
        (void)alloc;   // Use the allocator reference
    }

    SECTION("GetAllocator Const")
    {
        const TArray<int> arr;
        const auto& alloc = arr.GetAllocator();
        (void)alloc;   // Use the allocator reference
    }
}

TEST_CASE("TArray NPOS Constant", "[GP][Core][Container][TArray]")
{
    SECTION("NPOS Value") { REQUIRE(TArray<int>::NPOS == static_cast<SizeT>(-1)); }

    SECTION("IndexOf Returns NPOS")
    {
        TArray<int> arr = { 1, 2, 3 };
        auto idx = arr.IndexOf(99);
        REQUIRE(idx == TArray<int>::NPOS);
    }
}

TEST_CASE("TArray Stress Tests", "[GP][Core][Container][TArray]")
{
    SECTION("Many Insertions and Deletions")
    {
        TArray<int> arr;

        // Add elements
        for (int i = 0; i < 1000; ++i) { arr.PushBack(i); }

        // Remove every other element
        for (int i = 999; i >= 0; i -= 2) { arr.Erase(arr.Begin() + i); }

        REQUIRE(arr.Size() == 500);

        // Verify remaining elements
        for (SizeT i = 0; i < arr.Size(); ++i) { REQUIRE(arr[i] == static_cast<int>(i * 2)); }
    }

    SECTION("Repeated Growth and Shrink")
    {
        TArray<int> arr;

        for (int cycle = 0; cycle < 10; ++cycle)
        {
            // Grow
            for (int i = 0; i < 100; ++i) { arr.PushBack(i); }

            // Shrink
            for (int i = 0; i < 50; ++i) { arr.PopBack(); }
        }

        REQUIRE(arr.Size() == 500);
    }

    SECTION("Mixed Operations")
    {
        TArray<int> arr = { 1, 2, 3, 4, 5 };

        arr.Insert(arr.Begin() + 2, 99);
        arr.Erase(arr.Begin());
        arr.PushBack(100);
        arr.Resize(10, 42);
        arr.EraseSwapBack(0);
        arr.ShrinkToFit();

        REQUIRE(arr.Size() == 9);
        REQUIRE(arr.Capacity() == 9);
    }
}
