// Copyright (c) - Graphical Playground. All rights reserved.

#include "Container/Utility/ReverseIterator.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace GP;

namespace
{

/// @brief Helper struct used to exercise TReverseIterator::operator->.
struct FPoint
{
    Int32 x{ 7 };
    Int32 y{ 9 };
};

}   // namespace

TEST_CASE("TReverseIterator: Default construction yields null base", "[Container][ReverseIterator]")
{
    const TReverseIteratorT<Int32> rit;
    REQUIRE(rit.GetBase().GetPtr() == nullptr);
}

TEST_CASE("TReverseIterator: Construction from base iterator preserves base", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TIterator<Int32> base(arr + 3);
    TReverseIteratorT<Int32> rit(base);
    REQUIRE(rit.GetBase().GetPtr() == arr + 3);
}

TEST_CASE("TReverseIterator: Copy construction preserves base", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TReverseIteratorT<Int32> rit1(TIterator<Int32>(arr + 3));
    TReverseIteratorT<Int32> rit2(rit1);
    REQUIRE(rit2.GetBase().GetPtr() == arr + 3);
}

TEST_CASE("TReverseIterator: Copy assignment preserves base", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TReverseIteratorT<Int32> rit1(TIterator<Int32>(arr + 3));
    TReverseIteratorT<Int32> rit2;
    rit2 = rit1;
    REQUIRE(rit2.GetBase().GetPtr() == arr + 3);
}

TEST_CASE("TReverseIterator: Const reverse iterator constructed from non-const", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 3));
    TConstReverseIterator<Int32> crit(rit);
    REQUIRE(crit.GetBase().GetPtr() == arr + 3);
}

TEST_CASE("TReverseIterator: Iterator category matches underlying iterator", "[Container][ReverseIterator]")
{
    using Category = TReverseIteratorT<Int32>::IteratorCategory;
    STATIC_REQUIRE(CSameAs<Category, FRandomAccessIteratorTag>);
}

TEST_CASE("TReverseIteratorT: Is TReverseIterator<TIterator<T>>", "[Container][ReverseIterator]")
{
    STATIC_REQUIRE(CSameAs<TReverseIteratorT<Int32>, TReverseIterator<TIterator<Int32>>>);
}

TEST_CASE("TConstReverseIterator: Is TReverseIterator<TIterator<const T>>", "[Container][ReverseIterator]")
{
    STATIC_REQUIRE(CSameAs<TConstReverseIterator<Int32>, TReverseIterator<TIterator<const Int32>>>);
}

TEST_CASE(
    "TReverseIterator: GetBase returns underlying iterator, pointing one past last element",
    "[Container][ReverseIterator]"
)
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 5));
    REQUIRE(rit.GetBase().GetPtr() == arr + 5);
}

TEST_CASE("TReverseIterator: operator* dereferences the element before the base", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    // base at arr+3 → dereferences arr[2] = 30
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 3));
    REQUIRE(*rit == 30);
}

TEST_CASE("TReverseIterator: operator* after increment dereferences previous element", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 3));
    ++rit;
    REQUIRE(*rit == 20);
}

TEST_CASE("TReverseIterator: operator* does not modify base iterator", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 2));
    (void)*rit;
    REQUIRE(rit.GetBase().GetPtr() == arr + 2);
}

TEST_CASE("TReverseIterator: operator-> accesses member of element before the base", "[Container][ReverseIterator]")
{
    FPoint pts[3];
    pts[0].x = 1;
    pts[1].x = 2;
    pts[2].x = 3;
    TReverseIterator<TIterator<FPoint>> rit(TIterator<FPoint>(pts + 3));
    REQUIRE(rit->x == 3);
}

TEST_CASE("TReverseIterator: operator-> allows member modification", "[Container][ReverseIterator]")
{
    FPoint pts[2];
    TReverseIterator<TIterator<FPoint>> rit(TIterator<FPoint>(pts + 2));
    rit->x = 55;
    REQUIRE(pts[1].x == 55);
}

TEST_CASE("TReverseIterator: operator[] accesses elements at reverse offset", "[Container][ReverseIterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    // base at arr+5: rit[0]=arr[4]=5, rit[1]=arr[3]=4, rit[4]=arr[0]=1
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 5));
    REQUIRE(rit[0] == 5);
    REQUIRE(rit[1] == 4);
    REQUIRE(rit[2] == 3);
    REQUIRE(rit[4] == 1);
}

TEST_CASE(
    "TReverseIterator: Pre-increment moves backward through array and returns self", "[Container][ReverseIterator]"
)
{
    Int32 arr[3]{ 10, 20, 30 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 3));
    TReverseIteratorT<Int32>& ref = ++rit;
    REQUIRE(*rit == 20);
    REQUIRE(&ref == &rit);
}

TEST_CASE("TReverseIterator: Post-increment returns original then moves backward", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 3));
    TReverseIteratorT<Int32> prev = rit++;
    REQUIRE(*prev == 30);
    REQUIRE(*rit == 20);
}

TEST_CASE(
    "TReverseIterator: Pre-decrement moves forward through array and returns self", "[Container][ReverseIterator]"
)
{
    Int32 arr[3]{ 10, 20, 30 };
    // start pointing at arr[0] (base=arr+1)
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 1));
    TReverseIteratorT<Int32>& ref = --rit;
    REQUIRE(*rit == 20);
    REQUIRE(&ref == &rit);
}

TEST_CASE("TReverseIterator: Post-decrement returns original then moves forward", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 1));
    TReverseIteratorT<Int32> prev = rit--;
    REQUIRE(*prev == 10);
    REQUIRE(*rit == 20);
}

TEST_CASE("TReverseIterator: operator+= advances n steps in reverse and returns self", "[Container][ReverseIterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32>& ref = (rit += 2);
    REQUIRE(*rit == 3);
    REQUIRE(&ref == &rit);
}

TEST_CASE("TReverseIterator: operator+= by zero keeps position", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 2));
    rit += 0;
    REQUIRE(*rit == 20);
}

TEST_CASE("TReverseIterator: operator-= retreats n steps (toward end) and returns self", "[Container][ReverseIterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    // start at arr[1] (base=arr+2), -= 1 → base=arr+3 → dereferences arr[2]=3
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 2));
    TReverseIteratorT<Int32>& ref = (rit -= 1);
    REQUIRE(*rit == 3);
    REQUIRE(&ref == &rit);
}

TEST_CASE("TReverseIterator: operator-= by zero keeps position", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 2));
    rit -= 0;
    REQUIRE(*rit == 20);
}

TEST_CASE("TReverseIterator: operator+(n) creates new iterator n steps in reverse", "[Container][ReverseIterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32> result = rit + 3;
    REQUIRE(*result == 2);
    REQUIRE(*rit == 5);   // original unchanged
}

TEST_CASE("TReverseIterator: operator-(n) creates new iterator n steps toward end", "[Container][ReverseIterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    // start at arr[1] (base=arr+2), - 1 → base=arr+3 → dereferences arr[2]=3
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 2));
    TReverseIteratorT<Int32> result = rit - 1;
    REQUIRE(*result == 3);
    REQUIRE(*rit == 2);   // original unchanged
}

TEST_CASE(
    "TReverseIterator: n + iterator (friend operator) creates iterator n steps in reverse",
    "[Container][ReverseIterator]"
)
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32> result = 4 + rit;
    REQUIRE(*result == 1);
    REQUIRE(*rit == 5);   // original unchanged
}

TEST_CASE("TReverseIterator: iterator - iterator returns number of steps between them", "[Container][ReverseIterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TReverseIteratorT<Int32> rbegin(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32> rend(TIterator<Int32>(arr + 1));
    // rend - rbegin: rbegin.base - rend.base = (arr+5) - (arr+1) = 4
    REQUIRE((rend - rbegin) == 4);
}

TEST_CASE("TReverseIterator: iterator - iterator is zero for equal iterators", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 2));
    REQUIRE((rit - rit) == 0);
}

TEST_CASE("TReverseIterator: operator== true for iterators with same base", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TReverseIteratorT<Int32> rit1(TIterator<Int32>(arr + 2));
    TReverseIteratorT<Int32> rit2(TIterator<Int32>(arr + 2));
    REQUIRE(rit1 == rit2);
}

TEST_CASE("TReverseIterator: operator== false for iterators with different base", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TReverseIteratorT<Int32> rit1(TIterator<Int32>(arr + 1));
    TReverseIteratorT<Int32> rit2(TIterator<Int32>(arr + 3));
    REQUIRE_FALSE(rit1 == rit2);
}

TEST_CASE("TReverseIterator: operator!= true for iterators with different base", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TReverseIteratorT<Int32> rit1(TIterator<Int32>(arr + 1));
    TReverseIteratorT<Int32> rit2(TIterator<Int32>(arr + 3));
    REQUIRE(rit1 != rit2);
}

TEST_CASE("TReverseIterator: operator!= false for iterators with same base", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 1, 2, 3 };
    TReverseIteratorT<Int32> rit1(TIterator<Int32>(arr + 2));
    TReverseIteratorT<Int32> rit2(TIterator<Int32>(arr + 2));
    REQUIRE_FALSE(rit1 != rit2);
}

TEST_CASE(
    "TReverseIterator: operator< is true when lhs has later base (earlier in reverse)", "[Container][ReverseIterator]"
)
{
    // rit_begin (base=arr+5) is at the "start" of reverse traversal → considered less than rit_near_end (base=arr+1)
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TReverseIteratorT<Int32> rbegin(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32> rend(TIterator<Int32>(arr + 1));
    REQUIRE(rbegin < rend);
    REQUIRE_FALSE(rend < rbegin);
    REQUIRE_FALSE(rbegin < rbegin);
}

TEST_CASE("TReverseIterator: operator<= reflects reversed pointer ordering", "[Container][ReverseIterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TReverseIteratorT<Int32> rbegin(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32> rsame(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32> rend(TIterator<Int32>(arr + 1));
    REQUIRE(rbegin <= rend);
    REQUIRE(rbegin <= rsame);
    REQUIRE_FALSE(rend <= rbegin);
}

TEST_CASE("TReverseIterator: operator> reflects reversed pointer ordering", "[Container][ReverseIterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TReverseIteratorT<Int32> rbegin(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32> rend(TIterator<Int32>(arr + 1));
    REQUIRE(rend > rbegin);
    REQUIRE_FALSE(rbegin > rend);
    REQUIRE_FALSE(rbegin > rbegin);
}

TEST_CASE("TReverseIterator: operator>= reflects reversed pointer ordering", "[Container][ReverseIterator]")
{
    Int32 arr[5]{ 1, 2, 3, 4, 5 };
    TReverseIteratorT<Int32> rbegin(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32> rsame(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32> rend(TIterator<Int32>(arr + 1));
    REQUIRE(rend >= rbegin);
    REQUIRE(rbegin >= rsame);
    REQUIRE_FALSE(rbegin >= rend);
}

TEST_CASE("TReverseIterator: Full reverse traversal visits elements in reverse order", "[Container][ReverseIterator]")
{
    Int32 arr[5]{ 10, 20, 30, 40, 50 };
    TReverseIteratorT<Int32> rbegin(TIterator<Int32>(arr + 5));
    TReverseIteratorT<Int32> rend{ TIterator<Int32>{ arr } };

    Int32 expected[5]{ 50, 40, 30, 20, 10 };
    Int32 index = 0;
    for (TReverseIteratorT<Int32> it = rbegin; it != rend; ++it)
    {
        REQUIRE(*it == expected[index]);
        ++index;
    }
    REQUIRE(index == 5);
}

TEST_CASE("TReverseIterator: Reverse traversal over single element", "[Container][ReverseIterator]")
{
    Int32 val = 42;
    TReverseIteratorT<Int32> rbegin(TIterator<Int32>(&val + 1));
    TReverseIteratorT<Int32> rend{ TIterator<Int32>{ &val } };

    Int32 count = 0;
    for (TReverseIteratorT<Int32> it = rbegin; it != rend; ++it)
    {
        REQUIRE(*it == 42);
        ++count;
    }
    REQUIRE(count == 1);
}

TEST_CASE("TReverseIterator: Reverse traversal writes values correctly", "[Container][ReverseIterator]")
{
    Int32 arr[4]{ 0, 0, 0, 0 };
    TReverseIteratorT<Int32> rbegin(TIterator<Int32>(arr + 4));
    TReverseIteratorT<Int32> rend{ TIterator<Int32>{ arr } };

    Int32 val = 1;
    for (TReverseIteratorT<Int32> it = rbegin; it != rend; ++it, ++val) { *it = val * 10; }

    REQUIRE(arr[3] == 10);
    REQUIRE(arr[2] == 20);
    REQUIRE(arr[1] == 30);
    REQUIRE(arr[0] == 40);
}

TEST_CASE("TConstReverseIterator: Reads elements without modification", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 100, 200, 300 };
    TConstReverseIterator<Int32> crit(TIterator<const Int32>(arr + 3));
    REQUIRE(*crit == 300);
    REQUIRE(*(crit + 1) == 200);
    REQUIRE(*(crit + 2) == 100);
}

TEST_CASE("TConstReverseIterator: Constructed from non-const reverse iterator", "[Container][ReverseIterator]")
{
    Int32 arr[3]{ 10, 20, 30 };
    TReverseIteratorT<Int32> rit(TIterator<Int32>(arr + 3));
    TConstReverseIterator<Int32> crit(rit);
    REQUIRE(crit.GetBase().GetPtr() == arr + 3);
    REQUIRE(*crit == 30);
}
