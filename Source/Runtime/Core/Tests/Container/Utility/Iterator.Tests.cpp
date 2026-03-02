// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Utility/Iterator.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

namespace
{

/// @brief Helper struct used to exercise TIterator::operator->.
struct FPoint
{
    Int32 x{ 1 };
    Int32 y{ 2 };
};

}   // namespace

TEST_CASE("TIterator: Default construction yields null pointer", "[Container][Iterator]")
{
    const TIterator<Int32> it;
    REQUIRE(it.GetPtr() == nullptr);
}

TEST_CASE("TIterator: Construction from raw pointer", "[Container][Iterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    const TIterator<Int32> it(arr);
    REQUIRE(it.GetPtr() == arr);
}

TEST_CASE("TIterator: Copy construction preserves pointer", "[Container][Iterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    const TIterator<Int32> it1(arr);
    const TIterator<Int32> it2(it1);
    REQUIRE(it2.GetPtr() == arr);
}

TEST_CASE("TIterator: Copy assignment preserves pointer", "[Container][Iterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    const TIterator<Int32> it1(arr);
    TIterator<Int32> it2;
    it2 = it1;
    REQUIRE(it2.GetPtr() == arr);
}

TEST_CASE("TIterator: Const iterator constructed from non-const iterator", "[Container][Iterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    const TIterator<Int32> it(arr);
    const TConstIterator<Int32> cit(it);
    REQUIRE(cit.GetPtr() == arr);
}

TEST_CASE("TIterator: Iterator category is random access", "[Container][Iterator]")
{
    using Category = TIterator<Int32>::IteratorCategory;
    STATIC_REQUIRE(CSameAs<Category, FRandomAccessIteratorTag>);
}

TEST_CASE("TIterator: ValueType strips cv-qualifiers", "[Container][Iterator]")
{
    STATIC_REQUIRE(CSameAs<TIterator<const Int32>::ValueType, Int32>);
    STATIC_REQUIRE(CSameAs<TIterator<Int32>::ValueType, Int32>);
}

TEST_CASE("TIterator: operator* returns reference to pointed-to element", "[Container][Iterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    const TIterator<Int32> it(arr);
    REQUIRE(*it == 10);
}

TEST_CASE("TIterator: operator* allows modification of element", "[Container][Iterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TIterator<Int32> it(arr);
    *it = 99;
    REQUIRE(arr[0] == 99);
}

TEST_CASE("TIterator: operator-> accesses member of pointed-to element", "[Container][Iterator]")
{
    FPoint pt;
    const TIterator<FPoint> it(&pt);
    REQUIRE(it->x == 1);
    REQUIRE(it->y == 2);
}

TEST_CASE("TIterator: operator-> allows member modification", "[Container][Iterator]")
{
    FPoint pt;
    TIterator<FPoint> it(&pt);
    it->x = 42;
    REQUIRE(pt.x == 42);
}

TEST_CASE("TIterator: operator[] accesses element at positive offset", "[Container][Iterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    const TIterator<Int32> it(arr);
    REQUIRE(it[0] == 1);
    REQUIRE(it[2] == 3);
    REQUIRE(it[4] == 5);
}

TEST_CASE("TIterator: operator[] allows modification of offset element", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it(arr);
    it[1] = 77;
    REQUIRE(arr[1] == 77);
}

TEST_CASE("TIterator: Pre-increment advances iterator and returns self-reference", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it(arr);
    TIterator<Int32>& ref = ++it;
    REQUIRE(*it == 2);
    REQUIRE(&ref == &it);
}

TEST_CASE("TIterator: Post-increment returns original position then advances", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it(arr);
    TIterator<Int32> prev = it++;
    REQUIRE(*prev == 1);
    REQUIRE(*it == 2);
}

TEST_CASE("TIterator: Pre-decrement moves iterator back and returns self-reference", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it(arr + 2);
    TIterator<Int32>& ref = --it;
    REQUIRE(*it == 2);
    REQUIRE(&ref == &it);
}

TEST_CASE("TIterator: Post-decrement returns original position then moves back", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it(arr + 2);
    TIterator<Int32> prev = it--;
    REQUIRE(*prev == 3);
    REQUIRE(*it == 2);
}

TEST_CASE("TIterator: operator+= advances by n and returns self-reference", "[Container][Iterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TIterator<Int32> it(arr);
    TIterator<Int32>& ref = (it += 3);
    REQUIRE(*it == 4);
    REQUIRE(&ref == &it);
}

TEST_CASE("TIterator: operator+= by zero keeps position", "[Container][Iterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TIterator<Int32> it(arr + 1);
    it += 0;
    REQUIRE(*it == 20);
}

TEST_CASE("TIterator: operator-= moves back by n and returns self-reference", "[Container][Iterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TIterator<Int32> it(arr + 4);
    TIterator<Int32>& ref = (it -= 2);
    REQUIRE(*it == 3);
    REQUIRE(&ref == &it);
}

TEST_CASE("TIterator: operator-= by zero keeps position", "[Container][Iterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TIterator<Int32> it(arr + 1);
    it -= 0;
    REQUIRE(*it == 20);
}

TEST_CASE("TIterator: operator+(n) creates advanced iterator without modifying original", "[Container][Iterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TIterator<Int32> it(arr);
    TIterator<Int32> result = it + 3;
    REQUIRE(*result == 4);
    REQUIRE(*it == 1);
}

TEST_CASE("TIterator: operator-(n) creates moved-back iterator without modifying original", "[Container][Iterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TIterator<Int32> it(arr + 4);
    TIterator<Int32> result = it - 3;
    REQUIRE(*result == 2);
    REQUIRE(*it == 5);
}

TEST_CASE("TIterator: n + iterator (friend operator) creates advanced iterator", "[Container][Iterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TIterator<Int32> it(arr);
    TIterator<Int32> result = 4 + it;
    REQUIRE(*result == 5);
    REQUIRE(*it == 1);
}

TEST_CASE("TIterator: iterator - iterator returns element distance", "[Container][Iterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TIterator<Int32> first(arr);
    TIterator<Int32> last(arr + 4);
    REQUIRE((last - first) == 4);
}

TEST_CASE("TIterator: iterator - iterator returns zero for equal iterators", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it(arr + 1);
    REQUIRE((it - it) == 0);
}

TEST_CASE("TIterator: operator== is true for iterators at same position", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it1(arr + 1);
    TIterator<Int32> it2(arr + 1);
    REQUIRE(it1 == it2);
}

TEST_CASE("TIterator: operator== is false for iterators at different positions", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it1(arr);
    TIterator<Int32> it2(arr + 2);
    REQUIRE_FALSE(it1 == it2);
}

TEST_CASE("TIterator: operator!= is true for iterators at different positions", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it1(arr);
    TIterator<Int32> it2(arr + 1);
    REQUIRE(it1 != it2);
}

TEST_CASE("TIterator: operator!= is false for iterators at same position", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it1(arr + 2);
    TIterator<Int32> it2(arr + 2);
    REQUIRE_FALSE(it1 != it2);
}

TEST_CASE("TIterator: operator< reflects pointer ordering", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> early(arr);
    TIterator<Int32> late(arr + 2);
    REQUIRE(early < late);
    REQUIRE_FALSE(late < early);
    REQUIRE_FALSE(early < early);
}

TEST_CASE("TIterator: operator<= reflects pointer ordering", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> early(arr);
    TIterator<Int32> same(arr);
    TIterator<Int32> late(arr + 2);
    REQUIRE(early <= late);
    REQUIRE(early <= same);
    REQUIRE_FALSE(late <= early);
}

TEST_CASE("TIterator: operator> reflects pointer ordering", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> early(arr);
    TIterator<Int32> late(arr + 2);
    REQUIRE(late > early);
    REQUIRE_FALSE(early > late);
    REQUIRE_FALSE(early > early);
}

TEST_CASE("TIterator: operator>= reflects pointer ordering", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> early(arr);
    TIterator<Int32> same(arr);
    TIterator<Int32> late(arr + 2);
    REQUIRE(late >= early);
    REQUIRE(early >= same);
    REQUIRE_FALSE(early >= late);
}

TEST_CASE("TIterator: operator<=> provides correct strong ordering", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> early(arr);
    TIterator<Int32> late(arr + 2);
    REQUIRE((early <=> late) == std::strong_ordering::less);
    REQUIRE((late <=> early) == std::strong_ordering::greater);
    REQUIRE((early <=> early) == std::strong_ordering::equal);
}

TEST_CASE("TIterator: GetPtr returns the underlying pointer", "[Container][Iterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> it(arr + 1);
    REQUIRE(it.GetPtr() == &arr[1]);
}

TEST_CASE("TConstIterator: Reads elements without modification", "[Container][Iterator]")
{
    Int32 arr[3]{ 100, 200, 300 };
    TConstIterator<Int32> it(arr);
    REQUIRE(*it == 100);
    REQUIRE((it + 2).GetPtr() == arr + 2);
}

TEST_CASE("TConstIterator: Constructed from TIterator shares pointer", "[Container][Iterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TIterator<Int32> mut(arr);
    TConstIterator<Int32> cit(mut);
    REQUIRE(cit.GetPtr() == arr);
    REQUIRE(*cit == 10);
}

TEST_CASE("TConstIterator: Supports full arithmetic identical to TIterator", "[Container][Iterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TConstIterator<Int32> it(arr);
    it += 2;
    REQUIRE(*it == 3);
    REQUIRE(*(it + 1) == 4);
    REQUIRE(*(it - 1) == 2);
    it -= 1;
    REQUIRE(*it == 2);
}

TEST_CASE("TIterator: Forward range iteration visits all elements in order", "[Container][Iterator]")
{
    Int32 arr[5]{ 10, 20, 30, 40, 50 };
    TIterator<Int32> first(arr);
    TIterator<Int32> last(arr + 5);

    Int32 index = 0;
    for (TIterator<Int32> it = first; it != last; ++it)
    {
        REQUIRE(*it == arr[index]);
        ++index;
    }
    REQUIRE(index == 5);
}

TEST_CASE("TIterator: Pointer-based loop correctly writes values", "[Container][Iterator]")
{
    Int32 arr[4]{ 0, 0, 0, 0 };
    TIterator<Int32> it(arr);
    for (Int32 i = 0; i < 4; ++i, ++it) { *it = (i + 1) * 10; }

    REQUIRE(arr[0] == 10);
    REQUIRE(arr[1] == 20);
    REQUIRE(arr[2] == 30);
    REQUIRE(arr[3] == 40);
}
