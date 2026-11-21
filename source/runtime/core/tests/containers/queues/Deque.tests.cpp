// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/queues/Deque.hpp"
#include "containers/strings/String.hpp"
#include "hashing/Crc32.hpp"
#include "hashing/HashOf.hpp"
#include "hashing/Sha256.hpp"
#include "hashing/XxHash.hpp"
#include "memory/allocators/InlineAllocator.hpp"
#include "memory/allocators/LinearAllocator.hpp"
#include "memory/allocators/PolymorphicAllocator.hpp"
#include "memory/allocators/ThreadScratchAllocator.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include <compare>
#include <gtest/gtest.h>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace gp::tests
{

namespace
{

struct DequeTracker
{
    static inline gp::Int32 s_constructCount{ 0 };
    static inline gp::Int32 s_copyCount{ 0 };
    static inline gp::Int32 s_moveCount{ 0 };
    static inline gp::Int32 s_copyAssignCount{ 0 };
    static inline gp::Int32 s_moveAssignCount{ 0 };
    static inline gp::Int32 s_destructCount{ 0 };

    static void reset() noexcept
    {
        s_constructCount = 0;
        s_copyCount = 0;
        s_moveCount = 0;
        s_copyAssignCount = 0;
        s_moveAssignCount = 0;
        s_destructCount = 0;
    }

    gp::Int32 id{ 0 };
    bool alive{ true };

    DequeTracker() noexcept
        : id(0)
    {
        ++s_constructCount;
    }

    explicit DequeTracker(const gp::Int32 inId) noexcept
        : id(inId)
    {
        ++s_constructCount;
    }

    DequeTracker(const DequeTracker& other) noexcept
        : id(other.id)
    {
        ++s_copyCount;
    }

    DequeTracker(DequeTracker&& other) noexcept
        : id(other.id)
    {
        other.id = -1;
        ++s_moveCount;
    }

    DequeTracker& operator=(const DequeTracker& other) noexcept
    {
        id = other.id;
        ++s_copyAssignCount;
        return *this;
    }

    DequeTracker& operator=(DequeTracker&& other) noexcept
    {
        id = other.id;
        other.id = -1;
        ++s_moveAssignCount;
        return *this;
    }

    ~DequeTracker() noexcept
    {
        alive = false;
        ++s_destructCount;
    }

    [[nodiscard]] bool operator==(const DequeTracker& other) const noexcept
    {
        return id == other.id;
    }

    [[nodiscard]] std::strong_ordering operator<=>(const DequeTracker& other) const noexcept
    {
        return id <=> other.id;
    }
};

struct ImmovableItem
{
    gp::Int32 value{ 0 };

    explicit ImmovableItem(const gp::Int32 inValue) noexcept
        : value(inValue)
    {}

    ImmovableItem(const ImmovableItem&) = delete;
    ImmovableItem& operator=(const ImmovableItem&) = delete;
    ImmovableItem(ImmovableItem&&) = delete;
    ImmovableItem& operator=(ImmovableItem&&) = delete;
    ~ImmovableItem() noexcept = default;
};

struct LargeRelocatableBlock
{
    gp::UInt64 words[11]{};

    [[nodiscard]] bool operator==(const LargeRelocatableBlock& other) const noexcept
    {
        return words[0] == other.words[0] && words[10] == other.words[10];
    }
};

struct alignas(64) Aligned64Payload
{
    gp::Float32 lanes[16]{};
};

struct ImmutableRecord
{
    const gp::Int32 id{ 0 };

    explicit ImmutableRecord(const gp::Int32 inId) noexcept
        : id(inId)
    {}

    ImmutableRecord(const ImmutableRecord&) noexcept = default;
    ImmutableRecord(ImmutableRecord&&) noexcept = default;
    ImmutableRecord& operator=(const ImmutableRecord&) = delete;
    ImmutableRecord& operator=(ImmutableRecord&&) = delete;
    ~ImmutableRecord() noexcept = default;
};

class SinglePassIntIter
{
public:
    using iterator_category = std::input_iterator_tag;
    using iterator_concept = std::input_iterator_tag;
    using value_type = gp::Int32;
    using difference_type = gp::ISize;

    SinglePassIntIter() noexcept = default;

    explicit SinglePassIntIter(const gp::Int32 current) noexcept
        : m_current(current)
    {}

    [[nodiscard]] gp::Int32 operator*() const noexcept
    {
        return m_current;
    }

    SinglePassIntIter& operator++() noexcept
    {
        ++m_current;
        return *this;
    }

    void operator++(int) noexcept
    {
        ++m_current;
    }

    [[nodiscard]] bool operator==(const SinglePassIntIter& other) const noexcept
    {
        return m_current == other.m_current;
    }

private:
    gp::Int32 m_current{ 0 };
};

template <typename InDeque>
void verifyDequeInvariants(const InDeque& dq)
{
    EXPECT_EQ(dq.isEmpty(), dq.size() == 0u);
    EXPECT_LE(dq.size(), dq.capacity());
    EXPECT_EQ(dq.sizeInBytes(), dq.size() * sizeof(typename InDeque::ValueType));
    if (dq.capacity() > 0u)
    {
        EXPECT_TRUE((dq.capacity() & (dq.capacity() - 1u)) == 0u);
    }

    typename InDeque::ConstSpan s1;
    typename InDeque::ConstSpan s2;
    dq.getSpans(s1, s2);
    EXPECT_EQ(s1.count + s2.count, dq.size());
    EXPECT_EQ(dq.isLinear(), s2.count == 0u);
    EXPECT_EQ(dq.end() - dq.begin(), static_cast<typename InDeque::DifferenceType>(dq.size()));
    EXPECT_EQ(dq.rend() - dq.rbegin(), static_cast<typename InDeque::DifferenceType>(dq.size()));

    if (dq.size() > 0u)
    {
        ASSERT_NE(dq.tryFront(), nullptr);
        ASSERT_NE(dq.tryBack(), nullptr);
        EXPECT_EQ(dq.tryFront(), &dq.front());
        EXPECT_EQ(dq.tryBack(), &dq.back());
    }
}

template <typename InContainer>
void expectSequence(const InContainer& actual, const std::initializer_list<typename InContainer::ValueType> expected)
{
    ASSERT_EQ(actual.size(), expected.size());
    typename InContainer::ConstIterator actualIt = actual.cbegin();
    const typename InContainer::ValueType* expectedIt = expected.begin();
    for (gp::USize idx = 0u; idx < expected.size(); ++idx, ++actualIt, ++expectedIt)
    {
        EXPECT_EQ(*actualIt, *expectedIt);
    }
}

}   // namespace

TEST(DequeTests, FootprintConceptsAndEmptyState)
{
    static_assert(sizeof(gp::Deque<gp::Int32>) == 32u);
    static_assert(sizeof(gp::ScratchDeque<gp::Int32>) == 32u);
    static_assert(sizeof(gp::PolymorphicDeque<gp::Int32>) == 40u);

    static_assert(gp::concepts::IsDeque<gp::Deque<gp::Int32>>);
    static_assert(gp::concepts::IsDeque<gp::ScratchDeque<gp::Int32>>);
    static_assert(gp::concepts::IsDeque<gp::InlineDeque<gp::Int32, 64u>>);
    static_assert(!gp::concepts::IsDeque<gp::Int32>);

    static_assert(gp::concepts::IsIterable<gp::Deque<gp::Int32>>);
    static_assert(gp::concepts::IsSizedRange<gp::Deque<gp::Int32>>);
    static_assert(gp::concepts::IsRelocatable<gp::Deque<gp::Int32>>);
    static_assert(gp::concepts::IsRelocatable<gp::PolymorphicDeque<gp::Int32>>);
    static_assert(!gp::concepts::IsRelocatable<gp::InlineDeque<gp::Int32, 64u>>);
    static_assert(std::random_access_iterator<gp::Deque<gp::Int32>::Iterator>);
    static_assert(std::random_access_iterator<gp::Deque<gp::Int32>::ConstIterator>);

    gp::Deque<gp::Int32> dq;
    EXPECT_TRUE(dq.isEmpty());
    EXPECT_EQ(dq.size(), 0u);
    EXPECT_EQ(dq.sizeInBytes(), 0u);
    EXPECT_EQ(dq.capacity(), 0u);
    EXPECT_GT(dq.maxSize(), 0u);
    EXPECT_TRUE(dq.isLinear());
    EXPECT_EQ(dq.tryFront(), nullptr);
    EXPECT_EQ(dq.tryBack(), nullptr);
    EXPECT_EQ(std::as_const(dq).tryFront(), nullptr);
    EXPECT_EQ(std::as_const(dq).tryBack(), nullptr);

    gp::Int32 outVal = 0;
    EXPECT_FALSE(dq.tryPopFront(outVal));
    EXPECT_FALSE(dq.tryPopBack(outVal));

    gp::Deque<gp::Int32>::Span s1;
    gp::Deque<gp::Int32>::Span s2;
    dq.getSpans(s1, s2);
    EXPECT_EQ(s1.data, nullptr);
    EXPECT_EQ(s1.count, 0u);
    EXPECT_EQ(s2.data, nullptr);
    EXPECT_EQ(s2.count, 0u);
}

TEST(DequeTests, ConstructorsAndAssignments)
{
    const gp::Deque<gp::Int32> sizedDefault(5u);
    ASSERT_EQ(sizedDefault.size(), 5u);
    EXPECT_EQ(sizedDefault.capacity(), 8u);
    for (gp::USize i = 0u; i < 5u; ++i)
    {
        EXPECT_EQ(sizedDefault[i], 0);
    }

    const gp::Deque<gp::Int32> sizedFill(4u, 77);
    ASSERT_EQ(sizedFill.size(), 4u);
    EXPECT_EQ(sizedFill.capacity(), 4u);
    for (gp::USize i = 0u; i < 4u; ++i)
    {
        EXPECT_EQ(sizedFill.at(i), 77);
    }

    const gp::Int32 raw[4] = { 10, 20, 30, 40 };
    const gp::Deque<gp::Int32> ranged(raw, raw + 4);
    EXPECT_EQ(ranged.size(), 4u);
    EXPECT_EQ(ranged.front(), 10);
    EXPECT_EQ(ranged.back(), 40);

    gp::Deque<gp::Int32> initList{ 1, 2, 3, 4, 5 };
    EXPECT_EQ(initList.size(), 5u);
    EXPECT_EQ(initList.capacity(), 8u);

    const gp::Deque<gp::Int32> copied(initList);
    EXPECT_EQ(copied, initList);

    const gp::ScratchDeque<gp::Int32> crossCopied(initList);
    EXPECT_EQ(crossCopied, initList);

    gp::Deque<gp::Int32> moved(std::move(initList));
    EXPECT_EQ(moved.size(), 5u);
    EXPECT_TRUE(initList.isEmpty());
    EXPECT_EQ(initList.capacity(), 0u);

    gp::Deque<gp::Int32> assigned;
    assigned = copied;
    EXPECT_EQ(assigned, copied);

    assigned = { 9, 8, 7 };
    ASSERT_EQ(assigned.size(), 3u);
    EXPECT_EQ(assigned[0], 9);
    EXPECT_EQ(assigned[1], 8);
    EXPECT_EQ(assigned[2], 7);

    assigned.assign(3u, assigned.front());
    ASSERT_EQ(assigned.size(), 3u);
    EXPECT_EQ(assigned[0], 9);
    EXPECT_EQ(assigned[1], 9);
    EXPECT_EQ(assigned[2], 9);

    assigned.assign(raw, raw + 4);
    ASSERT_EQ(assigned.size(), 4u);
    EXPECT_EQ(assigned[3], 40);

    gp::Deque<gp::Int32> moveAssigned;
    moveAssigned = std::move(assigned);
    EXPECT_EQ(moveAssigned.size(), 4u);
    EXPECT_TRUE(assigned.isEmpty());
}

TEST(DequeTests, DoubleEndedPushPopAndRingWrapAround)
{
    gp::Deque<gp::Int32> dq;
    dq.pushBack(10);
    dq.pushBack(20);
    dq.pushFront(5);
    dq.emplaceFront(1);
    dq.emplaceBack(30);

    // Logical sequence: [1, 5, 10, 20, 30]
    ASSERT_EQ(dq.size(), 5u);
    EXPECT_EQ(dq.front(), 1);
    EXPECT_EQ(dq[1], 5);
    EXPECT_EQ(dq[2], 10);
    EXPECT_EQ(dq[3], 20);
    EXPECT_EQ(dq.back(), 30);
    EXPECT_EQ(dq.sizeInBytes(), 5u * sizeof(gp::Int32));

    ASSERT_NE(dq.tryFront(), nullptr);
    ASSERT_NE(dq.tryBack(), nullptr);
    *dq.tryFront() = 2;
    *dq.tryBack() = 31;
    EXPECT_EQ(dq.front(), 2);
    EXPECT_EQ(dq.back(), 31);

    dq.popFront();
    dq.popBack();
    ASSERT_EQ(dq.size(), 3u);
    EXPECT_EQ(dq.front(), 5);
    EXPECT_EQ(dq.back(), 20);

    gp::Int32 popped = 0;
    EXPECT_TRUE(dq.tryPopFront(popped));
    EXPECT_EQ(popped, 5);
    EXPECT_TRUE(dq.tryPopBack(popped));
    EXPECT_EQ(popped, 20);
    EXPECT_TRUE(dq.tryPopFront(popped));
    EXPECT_EQ(popped, 10);
    EXPECT_TRUE(dq.isEmpty());
    EXPECT_TRUE(dq.isLinear());

    // Repeated sliding-window wraparound without growth.
    dq.reserve(8u);
    const gp::USize stableCap = dq.capacity();
    for (gp::Int32 cycle = 0; cycle < 100; ++cycle)
    {
        dq.pushBack(cycle);
        dq.pushBack(cycle + 1000);
        dq.pushFront(cycle + 2000);

        gp::Int32 v = 0;
        ASSERT_TRUE(dq.tryPopFront(v));
        EXPECT_EQ(v, cycle + 2000);
        ASSERT_TRUE(dq.tryPopFront(v));
        EXPECT_EQ(v, cycle);
        ASSERT_TRUE(dq.tryPopBack(v));
        EXPECT_EQ(v, cycle + 1000);
    }
    EXPECT_EQ(dq.capacity(), stableCap);
    EXPECT_TRUE(dq.isEmpty());
}

TEST(DequeTests, SelfAliasingAcrossReallocation)
{
    gp::Deque<gp::String> dq;
    dq.pushBack(gp::String("FirstElementExceedingSsoCapacity22"));
    dq.pushBack(gp::String("SecondElementExceedingSsoCapacity22"));
    dq.pushBack(gp::String("ThirdElementExceedingSsoCapacity22"));
    dq.pushBack(gp::String("FourthElementExceedingSsoCapacity22"));
    ASSERT_EQ(dq.size(), 4u);
    ASSERT_EQ(dq.capacity(), 4u);

    // Push elements that alias inside `m_data` at the exact reallocation boundary.
    dq.pushBack(dq.front());
    EXPECT_EQ(dq.size(), 5u);
    EXPECT_EQ(dq.back(), "FirstElementExceedingSsoCapacity22");

    // Fill to next power-of-two capacity (8) then pushFront(dq.back()) across growth.
    dq.pushBack("5");
    dq.pushBack("6");
    dq.pushBack("7");
    ASSERT_EQ(dq.size(), 8u);
    ASSERT_EQ(dq.capacity(), 8u);

    dq.pushFront(dq.back());
    EXPECT_EQ(dq.size(), 9u);
    EXPECT_EQ(dq.front(), "7");

    // Fill to 16 and insert self-aliasing element into interior.
    while (dq.size() < dq.capacity())
    {
        dq.pushBack("pad");
    }
    (void)dq.insert(4u, dq[1]);
    EXPECT_EQ(dq[4], dq[1]);
}

TEST(DequeTests, InteriorInsertEmplaceEraseAndResize)
{
    gp::Deque<gp::Int32> dq{ 10, 20, 30, 40 };

    // First-half insert (shifts prefix left)
    const gp::Deque<gp::Int32>::Iterator it1 = dq.insert(1u, 15);
    EXPECT_EQ(*it1, 15);
    expectSequence(dq, { 10, 15, 20, 30, 40 });

    // Second-half emplace (shifts suffix right)
    const gp::Deque<gp::Int32>::Iterator it2 = dq.emplace(dq.cbegin() + 4, 35);
    EXPECT_EQ(*it2, 35);
    expectSequence(dq, { 10, 15, 20, 30, 35, 40 });

    // Boundary insert at front and end
    (void)dq.insert(dq.cbegin(), 5);
    (void)dq.insert(dq.cend(), 45);
    expectSequence(dq, { 5, 10, 15, 20, 30, 35, 40, 45 });

    // First-half erase, second-half erase, and boundary erase
    const gp::Deque<gp::Int32>::Iterator afterFirstHalf = dq.erase(1u);
    EXPECT_EQ(*afterFirstHalf, 15);
    const gp::Deque<gp::Int32>::Iterator afterSecondHalf = dq.erase(dq.cbegin() + 4);
    EXPECT_EQ(*afterSecondHalf, 40);
    (void)dq.erase(0u);
    (void)dq.erase(dq.size() - 1u);
    expectSequence(dq, { 15, 20, 30, 40 });

    dq.resize(6u, 99);
    expectSequence(dq, { 15, 20, 30, 40, 99, 99 });

    dq.resize(8u);
    EXPECT_EQ(dq[6], 0);
    EXPECT_EQ(dq[7], 0);

    dq.resize(2u);
    expectSequence(dq, { 15, 20 });

    dq.resize(4u, dq.front());
    expectSequence(dq, { 15, 20, 15, 15 });
}

TEST(DequeTests, SpansLinearizeShrinkAndGrowth)
{
    gp::Deque<gp::Int32> dq;
    dq.reserve(8u);
    dq.pushBack(3);
    dq.pushBack(4);
    dq.pushFront(2);
    dq.pushFront(1);

    // Wrapped layout: [3, 4, _, _, _, _, 1, 2]
    EXPECT_FALSE(dq.isLinear());
    gp::Deque<gp::Int32>::ConstSpan cs1;
    gp::Deque<gp::Int32>::ConstSpan cs2;
    std::as_const(dq).getSpans(cs1, cs2);
    ASSERT_EQ(cs1.count, 2u);
    ASSERT_EQ(cs2.count, 2u);
    EXPECT_EQ(cs1.data[0], 1);
    EXPECT_EQ(cs1.data[1], 2);
    EXPECT_EQ(cs2.data[0], 3);
    EXPECT_EQ(cs2.data[1], 4);

    // Mutable spans edit in place
    gp::Deque<gp::Int32>::Span ms1;
    gp::Deque<gp::Int32>::Span ms2;
    dq.getSpans(ms1, ms2);
    ms1.data[0] = 10;
    ms2.data[1] = 40;
    EXPECT_EQ(dq.front(), 10);
    EXPECT_EQ(dq.back(), 40);

    dq.linearize();
    EXPECT_TRUE(dq.isLinear());
    expectSequence(dq, { 10, 2, 3, 40 });

    // Full wrapped buffer tests zero-allocation rotateRelocatableLeft path in linearize()
    gp::Deque<gp::Int32> fullWrapped;
    fullWrapped.reserve(4u);
    fullWrapped.pushBack(30);
    fullWrapped.pushBack(40);
    fullWrapped.pushFront(20);
    fullWrapped.pushFront(10);
    ASSERT_EQ(fullWrapped.size(), 4u);
    ASSERT_EQ(fullWrapped.capacity(), 4u);
    ASSERT_FALSE(fullWrapped.isLinear());
    fullWrapped.linearize();
    EXPECT_TRUE(fullWrapped.isLinear());
    expectSequence(fullWrapped, { 10, 20, 30, 40 });

    // Non-wrapped `m_head > 0` linearize()
    fullWrapped.popFront();
    ASSERT_TRUE(fullWrapped.isLinear());
    fullWrapped.linearize();
    EXPECT_TRUE(fullWrapped.isLinear());
    expectSequence(fullWrapped, { 20, 30, 40 });

    // Shrink wrapped half-full deque in place
    gp::Deque<gp::Int32> wrappedShrink;
    wrappedShrink.reserve(16u);
    for (gp::Int32 i = 0; i < 6; ++i)
    {
        wrappedShrink.pushBack(i + 10);
    }
    wrappedShrink.pushFront(9);
    wrappedShrink.pushFront(8);
    ASSERT_EQ(wrappedShrink.size(), 8u);
    ASSERT_EQ(wrappedShrink.capacity(), 16u);
    ASSERT_FALSE(wrappedShrink.isLinear());

    wrappedShrink.shrinkToFit();
    EXPECT_EQ(wrappedShrink.capacity(), 8u);
    EXPECT_TRUE(wrappedShrink.isLinear());
    expectSequence(wrappedShrink, { 8, 9, 10, 11, 12, 13, 14, 15 });

    wrappedShrink.clear();
    EXPECT_TRUE(wrappedShrink.isEmpty());
    EXPECT_EQ(wrappedShrink.capacity(), 8u);
    wrappedShrink.shrinkToFit();
    EXPECT_EQ(wrappedShrink.capacity(), 0u);
}

TEST(DequeTests, IteratorsSearchAndComparisons)
{
    gp::Deque<gp::Int32> dq{ 10, 20, 30, 20, 50 };

    gp::Deque<gp::Int32>::Iterator it = dq.begin();
    EXPECT_EQ(*it, 10);
    EXPECT_EQ(it[2], 30);
    it += 3;
    EXPECT_EQ(*it, 20);
    --it;
    EXPECT_EQ(*it, 30);
    EXPECT_EQ((2 + dq.begin())[1], 20);
    EXPECT_EQ(dq.end() - dq.begin(), 5);
    EXPECT_LT(dq.begin(), dq.end());

    const gp::Deque<gp::Int32>::ConstIterator cit = it;
    EXPECT_EQ(*cit, 30);
    EXPECT_EQ(*(cit.operator->()), 30);

    // Reverse iteration
    gp::Int32 revExpected[5] = { 50, 20, 30, 20, 10 };
    gp::USize revIdx = 0u;
    for (gp::Deque<gp::Int32>::ConstReverseIterator rit = dq.crbegin(); rit != dq.crend(); ++rit)
    {
        EXPECT_EQ(*rit, revExpected[revIdx++]);
    }
    gp::Deque<gp::Int32>::ReverseIterator mutRit = dq.rbegin();
    *mutRit = 55;
    EXPECT_EQ(dq.back(), 55);
    EXPECT_NE(dq.rend(), dq.rbegin());

    EXPECT_EQ(dq.find(20), dq.begin() + 1);
    EXPECT_EQ(dq.find(999), dq.end());
    EXPECT_EQ(std::as_const(dq).find(30), dq.cbegin() + 2);
    EXPECT_EQ(dq.indexOf(20), 1u);
    EXPECT_EQ(dq.indexOf(999), gp::Deque<gp::Int32>::npos);
    EXPECT_TRUE(dq.contains(55));
    EXPECT_FALSE(dq.contains(999));
    EXPECT_EQ(dq.count(20), 2u);
    EXPECT_EQ(dq.count(999), 0u);

    const gp::Deque<gp::Int32> dqEqual{ 10, 20, 30, 20, 55 };
    const gp::Deque<gp::Int32> dqGreater{ 10, 20, 31, 20, 55 };
    const gp::Deque<gp::Int32> dqShorter{ 10, 20, 30 };
    EXPECT_EQ(dq, dqEqual);
    EXPECT_LT(dq, dqGreater);
    EXPECT_GT(dq, dqShorter);
}

TEST(DequeTests, NonTrivialLifecycleAndImmovableSupport)
{
    DequeTracker::reset();
    {
        gp::Deque<DequeTracker> dq;
        dq.emplaceBack(1);
        dq.emplaceBack(2);
        dq.emplaceFront(0);
        dq.emplaceBack(3);
        dq.emplaceBack(4);   // Triggers resize 4 -> 8
        dq.emplaceFront(-1);
        EXPECT_EQ(dq.size(), 6u);

        dq.popFront();
        dq.popBack();
        (void)dq.erase(2u);
        dq.linearize();
        EXPECT_EQ(dq.size(), 3u);
    }
    EXPECT_EQ(
        DequeTracker::s_constructCount + DequeTracker::s_copyCount + DequeTracker::s_moveCount,
        DequeTracker::s_destructCount
    );

    // Pre-reserved immovable elements compile and stay in place without moves.
    gp::Deque<ImmovableItem> immovable;
    immovable.reserve(4u);
    (void)immovable.emplaceBack(100);
    (void)immovable.emplaceFront(50);
    EXPECT_EQ(immovable.front().value, 50);
    EXPECT_EQ(immovable.back().value, 100);
    immovable.popFront();
    EXPECT_EQ(immovable.front().value, 100);
}

TEST(DequeTests, AllocatorsSwapAndUnifiedHashing)
{
    // InlineDeque stays inside embedded buffer until overflow and shrinks back in place.
    gp::InlineDeque<gp::Int32, 64u> inlineDq;
    inlineDq.pushBack(1);
    inlineDq.pushBack(2);
    inlineDq.pushFront(0);
    inlineDq.pushBack(3);
    EXPECT_TRUE(inlineDq.getAllocator().isInlinePointer(&inlineDq.front()));

    // Wrapped shrink stays inside inline buffer without touching fallback heap.
    inlineDq.reserve(8u);   // 32 bytes fits in 64B inline buffer
    inlineDq.pushFront(-1);
    EXPECT_TRUE(inlineDq.getAllocator().isInlinePointer(&inlineDq.front()));
    inlineDq.shrinkToFit();
    EXPECT_TRUE(inlineDq.getAllocator().isInlinePointer(&inlineDq.front()));
    const gp::Deque<gp::Int32> expectedInline{ -1, 0, 1, 2, 3 };
    EXPECT_EQ(inlineDq, expectedInline);

    // Copy & move of InlineDeque do not alias source inline buffer
    gp::InlineDeque<gp::Int32, 64u> inlineCopy(inlineDq);
    EXPECT_EQ(inlineCopy, inlineDq);
    EXPECT_NE(&inlineCopy.front(), &inlineDq.front());

    // Overflow spills cleanly to fallback allocator
    for (gp::Int32 i = 4; i < 20; ++i)
    {
        inlineDq.pushBack(i);
    }
    EXPECT_FALSE(inlineDq.getAllocator().isInlinePointer(&inlineDq.front()));

    // PolymorphicDeque backed by LinearAllocator
    alignas(16) gp::UInt8 arenaBuf[1024]{};
    gp::memory::LinearAllocator arenaA(arenaBuf, sizeof(arenaBuf), "DequeArenaA");
    alignas(16) gp::UInt8 arenaBufB[1024]{};
    gp::memory::LinearAllocator arenaB(arenaBufB, sizeof(arenaBufB), "DequeArenaB");

    {
        gp::PolymorphicDeque<gp::Int32> polyA{ gp::memory::PolymorphicAllocator(arenaA) };
        gp::PolymorphicDeque<gp::Int32> polyB{ gp::memory::PolymorphicAllocator(arenaB) };
        polyA.pushBack(1);
        polyA.pushBack(2);
        polyA.pushFront(0);
        polyB.pushBack(10);
        polyB.pushBack(20);

        EXPECT_GT(arenaA.getUsedBytes(), 0u);
        EXPECT_GT(arenaB.getUsedBytes(), 0u);

        // Unequal-allocator swap preserves each container's arena binding.
        gp::swap(polyA, polyB);
        const gp::Deque<gp::Int32> expectedA{ 10, 20 };
        const gp::Deque<gp::Int32> expectedB{ 0, 1, 2 };
        EXPECT_EQ(polyA, expectedA);
        EXPECT_EQ(polyB, expectedB);
        EXPECT_TRUE(polyA.getAllocator().owns(&polyA.front()));
        EXPECT_TRUE(polyB.getAllocator().owns(&polyB.front()));
        EXPECT_TRUE(arenaA.owns(&polyA.front()));
        EXPECT_TRUE(arenaB.owns(&polyB.front()));
    }

    // ThreadScratchAllocator integration
    gp::memory::LinearAllocator& tlsArena = gp::memory::ThreadScratchArena::get();
    const gp::USize baseline = tlsArena.getUsedBytes();
    {
        const gp::memory::ScopedScratchMark mark;
        gp::ScratchDeque<gp::Int32> scratchDq;
        for (gp::Int32 i = 0; i < 16; ++i)
        {
            scratchDq.pushBack(i);
        }
        EXPECT_GT(tlsArena.getUsedBytes(), baseline);
    }
    EXPECT_EQ(tlsArena.getUsedBytes(), baseline);

    // Unified gp::hashing::hash integration: wrapped vs linear deques with identical logical sequences match.
    gp::Deque<gp::Int32> linearDq{ 1, 2, 3, 4 };
    gp::Deque<gp::Int32> wrappedDq;
    wrappedDq.reserve(8u);
    wrappedDq.pushBack(3);
    wrappedDq.pushBack(4);
    wrappedDq.pushFront(2);
    wrappedDq.pushFront(1);
    ASSERT_FALSE(wrappedDq.isLinear());

    EXPECT_EQ(gp::hashing::hash(linearDq), gp::hashing::hash(wrappedDq));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::XxHash64>(linearDq), gp::hashing::hash<gp::hashing::XxHash64>(wrappedDq));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::Crc32>(linearDq), gp::hashing::hash<gp::hashing::Crc32>(wrappedDq));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::Sha256>(linearDq), gp::hashing::hash<gp::hashing::Sha256>(wrappedDq));

    const gp::Deque<gp::Int32> diffDq{ 1, 2, 3, 5 };
    EXPECT_NE(gp::hashing::hash(linearDq), gp::hashing::hash(diffDq));
}

TEST(DequeTests, AdditionalApiAndConstOverloadCoverage)
{
    alignas(16) gp::UInt8 arenaBuf[2048]{};
    gp::memory::LinearAllocator arena(arenaBuf, sizeof(arenaBuf), "DequeApiArena");
    const gp::memory::PolymorphicAllocator alloc(arena);

    gp::PolymorphicDeque<gp::Int32> emptyAlloc{ alloc };
    gp::PolymorphicDeque<gp::Int32> movedAlloc{ gp::memory::PolymorphicAllocator(arena) };
    EXPECT_TRUE(emptyAlloc.isEmpty());
    EXPECT_TRUE(movedAlloc.isEmpty());

    gp::PolymorphicDeque<gp::Int32> countAlloc(3u, 42, alloc);
    expectSequence(countAlloc, { 42, 42, 42 });

    const gp::Int32 raw[3] = { 7, 8, 9 };
    gp::PolymorphicDeque<gp::Int32> rangeAlloc(raw, raw + 3, alloc);
    expectSequence(rangeAlloc, { 7, 8, 9 });

    gp::PolymorphicDeque<gp::Int32> initAlloc({ 11, 22, 33 }, alloc);
    expectSequence(initAlloc, { 11, 22, 33 });

    // Cross-allocator copy constructor
    const gp::Deque<gp::Int32> crossCopied(initAlloc);
    expectSequence(crossCopied, { 11, 22, 33 });

    // Equal-allocator member swap and self-swap
    rangeAlloc.swap(initAlloc);
    expectSequence(rangeAlloc, { 11, 22, 33 });
    expectSequence(initAlloc, { 7, 8, 9 });
    initAlloc.swap(initAlloc);
    expectSequence(initAlloc, { 7, 8, 9 });

    // Move-insert overloads and const-qualified accessors / iterators
    gp::Deque<gp::String> strDq;
    (void)strDq.insert(0u, gp::String("beta"));
    (void)strDq.insert(strDq.cbegin(), gp::String("alpha"));
    strDq.pushBack(gp::String("gamma"));
    const gp::Deque<gp::String>& constRef = strDq;
    EXPECT_EQ(constRef.at(0u), "alpha");
    EXPECT_EQ(constRef[2u], "gamma");
    EXPECT_EQ(constRef.front(), "alpha");
    EXPECT_EQ(constRef.back(), "gamma");
    ASSERT_NE(constRef.tryFront(), nullptr);
    ASSERT_NE(constRef.tryBack(), nullptr);
    EXPECT_EQ(*constRef.tryFront(), "alpha");
    EXPECT_EQ(*constRef.tryBack(), "gamma");
    EXPECT_EQ(*constRef.begin(), "alpha");
    EXPECT_EQ(constRef.end() - constRef.begin(), 3);
    EXPECT_EQ(*constRef.rbegin(), "gamma");
    EXPECT_EQ(constRef.rend() - constRef.rbegin(), 3);
    (void)constRef.getAllocator();

    strDq.reset();
    EXPECT_TRUE(strDq.isEmpty());
    EXPECT_EQ(strDq.capacity(), 0u);
}

TEST(DequeTests, RelocatableWrappedLinearizeMoveOnlyOverAlignedAndEdgeCases)
{
    // Verify wrapped linearize() makes zero extra allocations for all fill ratios on relocatable buffers.
    alignas(16) gp::UInt8 arenaBuf[4096]{};
    gp::memory::LinearAllocator arena(arenaBuf, sizeof(arenaBuf), "DequeLinearizeArena");
    const gp::memory::PolymorphicAllocator alloc(arena);

    // 1. Small relocatable (< 64B): m_size > capacity/2 wrapped case (`size=6, cap=8, m_head=5`)
    {
        gp::PolymorphicDeque<gp::Int32> dq{ alloc };
        dq.reserve(8u);
        const gp::USize bytesAfterReserve = arena.getUsedBytes();
        dq.pushBack(40);
        dq.pushBack(50);
        dq.pushBack(60);
        dq.pushFront(30);
        dq.pushFront(20);
        dq.pushFront(10);
        ASSERT_EQ(dq.size(), 6u);
        ASSERT_FALSE(dq.isLinear());

        dq.linearize();
        EXPECT_EQ(arena.getUsedBytes(), bytesAfterReserve);
        EXPECT_TRUE(dq.isLinear());
        expectSequence(dq, { 10, 20, 30, 40, 50, 60 });
    }

    // 2. Small relocatable: m_size <= capacity/2 wrapped case (`size=3, cap=8, m_head=7`)
    {
        gp::PolymorphicDeque<gp::Int32> dq{ alloc };
        dq.reserve(8u);
        const gp::USize bytesAfterReserve = arena.getUsedBytes();
        dq.pushBack(20);
        dq.pushBack(30);
        dq.pushFront(10);
        ASSERT_EQ(dq.size(), 3u);
        ASSERT_FALSE(dq.isLinear());

        dq.linearize();
        EXPECT_EQ(arena.getUsedBytes(), bytesAfterReserve);
        EXPECT_TRUE(dq.isLinear());
        expectSequence(dq, { 10, 20, 30 });
    }

    // 3. Large > 64B relocatable (`88 B` struct): full and 75%-full wrapped linearize() multi-chunk swap path
    {
        gp::PolymorphicDeque<LargeRelocatableBlock> largeDq{ alloc };
        largeDq.reserve(8u);
        const gp::USize bytesAfterReserve = arena.getUsedBytes();
        for (gp::UInt64 i = 3u; i <= 5u; ++i)
        {
            LargeRelocatableBlock block{};
            block.words[0] = i;
            block.words[10] = i * 10u;
            largeDq.pushBack(block);
        }
        for (gp::UInt64 i = 2u; i >= 1u; --i)
        {
            LargeRelocatableBlock block{};
            block.words[0] = i;
            block.words[10] = i * 10u;
            largeDq.pushFront(block);
        }
        LargeRelocatableBlock block6{};
        block6.words[0] = 6u;
        block6.words[10] = 60u;
        largeDq.pushBack(block6);
        ASSERT_EQ(largeDq.size(), 6u);
        ASSERT_FALSE(largeDq.isLinear());

        largeDq.linearize();
        EXPECT_EQ(arena.getUsedBytes(), bytesAfterReserve);
        EXPECT_TRUE(largeDq.isLinear());
        for (gp::USize idx = 0u; idx < 6u; ++idx)
        {
            EXPECT_EQ(largeDq[idx].words[0], idx + 1u);
            EXPECT_EQ(largeDq[idx].words[10], (idx + 1u) * 10u);
        }

        // Now fill to capacity 8 while wrapped by wrapping front once and pushing back, then rotate full ring
        LargeRelocatableBlock dummy{};
        (void)largeDq.tryPopFront(dummy);
        LargeRelocatableBlock block7{
            .words = { 7u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 70u }
        };
        LargeRelocatableBlock block8{
            .words = { 8u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 80u }
        };
        LargeRelocatableBlock block9{
            .words = { 9u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 90u }
        };
        largeDq.pushBack(block7);
        largeDq.pushBack(block8);
        largeDq.pushBack(block9);
        ASSERT_EQ(largeDq.size(), 8u);
        ASSERT_FALSE(largeDq.isLinear());

        largeDq.linearize();
        EXPECT_EQ(arena.getUsedBytes(), bytesAfterReserve);
        EXPECT_TRUE(largeDq.isLinear());
        for (gp::USize idx = 0u; idx < 8u; ++idx)
        {
            EXPECT_EQ(largeDq[idx].words[0], idx + 2u);
            EXPECT_EQ(largeDq[idx].words[10], (idx + 2u) * 10u);
        }
    }

    // Non-relocatable DequeTracker wrapped linearize() & wrapped shrinkToFit()
    DequeTracker::reset();
    {
        gp::Deque<DequeTracker> nonReloc;
        nonReloc.reserve(8u);
        nonReloc.emplaceBack(2);
        nonReloc.emplaceBack(3);
        nonReloc.emplaceFront(1);
        ASSERT_FALSE(nonReloc.isLinear());
        nonReloc.linearize();
        EXPECT_TRUE(nonReloc.isLinear());
        nonReloc.shrinkToFit();
        EXPECT_EQ(nonReloc.capacity(), 4u);
        EXPECT_EQ(nonReloc.front().id, 1);
        EXPECT_EQ(nonReloc.back().id, 3);
    }
    EXPECT_EQ(
        DequeTracker::s_constructCount + DequeTracker::s_copyCount + DequeTracker::s_moveCount,
        DequeTracker::s_destructCount
    );

    // Move-only relocatable gp::UniquePtr<gp::Int32> end-to-end
    {
        gp::Deque<gp::UniquePtr<gp::Int32>> ptrDq;
        ptrDq.pushBack(gp::makeUnique<gp::Int32>(20));
        ptrDq.pushFront(gp::makeUnique<gp::Int32>(10));
        ptrDq.emplaceBack(gp::makeUnique<gp::Int32>(30));
        ptrDq.emplaceFront(gp::makeUnique<gp::Int32>(5));
        (void)ptrDq.insert(2u, gp::makeUnique<gp::Int32>(15));
        ASSERT_EQ(ptrDq.size(), 5u);
        ptrDq.linearize();
        EXPECT_TRUE(ptrDq.isLinear());
        EXPECT_EQ(*ptrDq[0], 5);
        EXPECT_EQ(*ptrDq[2], 15);
        EXPECT_EQ(*ptrDq[4], 30);
        (void)ptrDq.erase(2u);
        ptrDq.shrinkToFit();
        EXPECT_EQ(ptrDq.size(), 4u);
        EXPECT_EQ(*ptrDq.back(), 30);
    }

    // tryAt(), findIf(), mixed Iterator/ConstIterator comparison & difference, self-assignment guards
    gp::Deque<gp::Int32> q{ 10, 20, 30, 40 };
    EXPECT_EQ(q.tryAt(10u), nullptr);
    EXPECT_EQ(std::as_const(q).tryAt(10u), nullptr);
    ASSERT_NE(q.tryAt(2u), nullptr);
    *q.tryAt(2u) = 35;
    EXPECT_EQ(*std::as_const(q).tryAt(2u), 35);

    const gp::Deque<gp::Int32>::Iterator foundMut = q.findIf(
        [](const gp::Int32 v) noexcept -> bool
    {
        return v > 25;
    }
    );
    ASSERT_NE(foundMut, q.end());
    EXPECT_EQ(*foundMut, 35);
    EXPECT_EQ(
        std::as_const(q).findIf(
            [](const gp::Int32 v) noexcept -> bool
    {
        return v > 999;
    }
        ),
        q.cend()
    );

    EXPECT_EQ(q.begin(), q.cbegin());
    EXPECT_LT(q.begin(), q.cend());
    EXPECT_EQ(q.end() - q.cbegin(), 4);
    EXPECT_EQ(q.cend() - q.begin(), 4);

    gp::Deque<gp::Int32>& aliasRef = q;
    q = aliasRef;
    q = std::move(aliasRef);
    expectSequence(q, { 10, 20, 35, 40 });

    const gp::Deque<gp::Int32> emptyDq;
    EXPECT_NE(q, emptyDq);
    EXPECT_GT(q, emptyDq);
    EXPECT_LE(emptyDq, q);
    EXPECT_GE(q, q);
    EXPECT_LE(q, q);

    gp::Deque<gp::Int32> cleared;
    cleared.linearize();
    cleared.reserve(0u);
    cleared.shrinkToFit();
    EXPECT_EQ(cleared.capacity(), 0u);

    // Over-aligned 64-byte SIMD payload alignment
    gp::Deque<Aligned64Payload> alignedDq;
    (void)alignedDq.emplaceBack();
    (void)alignedDq.emplaceFront();
    (void)alignedDq.emplaceBack();
    EXPECT_EQ(reinterpret_cast<gp::USize>(&alignedDq.front()) % 64u, 0u);
    EXPECT_EQ(reinterpret_cast<gp::USize>(&alignedDq.back()) % 64u, 0u);
}

TEST(DequeTests, InvariantsInlineSwapNonPowerOfTwoShrinkInputRangeAndNestedRelocation)
{
    // InlineDeque swap and occupied move assignment stay inside inline buffers
    gp::InlineDeque<gp::Int32, 64u> inlineA{ 1, 2, 3, 4 };
    gp::InlineDeque<gp::Int32, 64u> inlineB{ 10, 20, 30, 40, 50, 60 };
    EXPECT_TRUE(inlineA.getAllocator().isInlinePointer(&inlineA.front()));
    EXPECT_TRUE(inlineB.getAllocator().isInlinePointer(&inlineB.front()));
    gp::swap(inlineA, inlineB);
    EXPECT_TRUE(inlineA.getAllocator().isInlinePointer(&inlineA.front()));
    EXPECT_TRUE(inlineB.getAllocator().isInlinePointer(&inlineB.front()));
    expectSequence(inlineA, { 10, 20, 30, 40, 50, 60 });
    expectSequence(inlineB, { 1, 2, 3, 4 });
    verifyDequeInvariants(inlineA);
    verifyDequeInvariants(inlineB);

    inlineB = std::move(inlineA);
    EXPECT_TRUE(inlineB.getAllocator().isInlinePointer(&inlineB.front()));
    expectSequence(inlineB, { 10, 20, 30, 40, 50, 60 });

    // Non-power-of-two wrapped shrink (size=5, cap=16, m_head=14 -> cap=8) and drain to empty
    gp::Deque<gp::Int32> oddShrink;
    oddShrink.reserve(16u);
    oddShrink.pushBack(3);
    oddShrink.pushBack(4);
    oddShrink.pushBack(5);
    oddShrink.pushFront(2);
    oddShrink.pushFront(1);
    ASSERT_EQ(oddShrink.size(), 5u);
    ASSERT_EQ(oddShrink.capacity(), 16u);
    ASSERT_FALSE(oddShrink.isLinear());
    oddShrink.shrinkToFit();
    EXPECT_EQ(oddShrink.capacity(), 8u);
    EXPECT_TRUE(oddShrink.isLinear());
    expectSequence(oddShrink, { 1, 2, 3, 4, 5 });
    verifyDequeInvariants(oddShrink);

    // Draining a wrapped ring back to empty resets m_head to 0 (linear)
    oddShrink.pushFront(0);
    ASSERT_FALSE(oddShrink.isLinear());
    while (!oddShrink.isEmpty())
    {
        oddShrink.popBack();
    }
    EXPECT_TRUE(oddShrink.isLinear());
    verifyDequeInvariants(oddShrink);

    // Move-only tryPopFront / tryPopBack & swap
    gp::Deque<gp::UniquePtr<gp::Int32>> moveDqA;
    gp::Deque<gp::UniquePtr<gp::Int32>> moveDqB;
    moveDqA.pushBack(gp::makeUnique<gp::Int32>(11));
    moveDqA.pushBack(gp::makeUnique<gp::Int32>(22));
    moveDqB.pushBack(gp::makeUnique<gp::Int32>(33));
    gp::swap(moveDqA, moveDqB);
    gp::UniquePtr<gp::Int32> poppedPtr;
    EXPECT_TRUE(moveDqB.tryPopFront(poppedPtr));
    EXPECT_EQ(*poppedPtr, 11);
    EXPECT_TRUE(moveDqB.tryPopBack(poppedPtr));
    EXPECT_EQ(*poppedPtr, 22);
    EXPECT_FALSE(moveDqB.tryPopFront(poppedPtr));
    EXPECT_EQ(*moveDqA.front(), 33);

    // ImmutableRecord (const field, non-assignable) supports push/linearize/shrink/pop cleanly
    gp::Deque<ImmutableRecord> recDq;
    (void)recDq.emplaceBack(2);
    (void)recDq.emplaceFront(1);
    (void)recDq.emplaceBack(3);
    (void)recDq.emplaceBack(4);
    (void)recDq.emplaceBack(5);
    recDq.linearize();
    recDq.shrinkToFit();
    EXPECT_EQ(recDq.front().id, 1);
    EXPECT_EQ(recDq.back().id, 5);
    recDq.popFront();
    EXPECT_EQ(recDq.front().id, 2);

    // Immovable linearize() compiles and succeeds when empty / m_head == 0
    gp::Deque<ImmovableItem> immovable;
    immovable.linearize();
    immovable.reserve(2u);
    (void)immovable.emplaceBack(9);
    immovable.linearize();
    EXPECT_EQ(immovable.front().value, 9);

    // Single-pass non-forward InputIterator constructor and assign()
    static_assert(!std::forward_iterator<SinglePassIntIter>);
    const gp::Deque<gp::Int32> genCtor(SinglePassIntIter(10), SinglePassIntIter(15));
    expectSequence(genCtor, { 10, 11, 12, 13, 14 });
    gp::Deque<gp::Int32> genAssign;
    genAssign.assign(SinglePassIntIter(1), SinglePassIntIter(5));
    expectSequence(genAssign, { 1, 2, 3, 4 });

    // Nested relocatable Deque<Deque<Int32>> across growth (exercises IsRelocatableTrait<Deque>)
    gp::Deque<gp::Deque<gp::Int32>> nested;
    for (gp::Int32 row = 0; row < 10; ++row)
    {
        gp::Deque<gp::Int32> inner{ row, row * 10 };
        nested.pushBack(std::move(inner));
    }
    EXPECT_EQ(nested.size(), 10u);
    EXPECT_EQ(nested[7][1], 70);

    // Nested hash boundary framing prevents prefix collisions across inner containers
    gp::Deque<gp::Deque<gp::Int32>> nestedA;
    nestedA.pushBack(gp::Deque<gp::Int32>{ 1, 2 });
    nestedA.pushBack(gp::Deque<gp::Int32>{ 3 });
    gp::Deque<gp::Deque<gp::Int32>> nestedB;
    nestedB.pushBack(gp::Deque<gp::Int32>{ 1 });
    nestedB.pushBack(gp::Deque<gp::Int32>{ 2, 3 });
    EXPECT_NE(gp::hashing::hash(nestedA), gp::hashing::hash(nestedB));
    EXPECT_NE(gp::hashing::hash(gp::Deque<gp::Int32>{}), gp::hashing::hash(gp::Deque<gp::Int32>{ 0 }));

    // Uncomparable, non-default, move-only type constructed in place via multi-arg emplace
    struct UncomparablePair
    {
        gp::Int32 tag{ 0 };
        gp::Int64 weight{ 0 };

        UncomparablePair() = delete;

        UncomparablePair(const gp::Int32 inTag, const gp::Int64 inWeight) noexcept
            : tag(inTag)
            , weight(inWeight)
        {}

        UncomparablePair(const UncomparablePair&) = delete;
        UncomparablePair& operator=(const UncomparablePair&) = delete;
        UncomparablePair(UncomparablePair&&) noexcept = default;
        UncomparablePair& operator=(UncomparablePair&&) noexcept = default;
        ~UncomparablePair() noexcept = default;
    };

    gp::Deque<UncomparablePair> uncompDq;
    (void)uncompDq.emplaceBack(1, 100);
    (void)uncompDq.emplaceFront(0, 50);
    (void)uncompDq.emplace(1u, 2, 200);
    EXPECT_EQ(uncompDq.front().tag, 0);
    EXPECT_EQ(uncompDq[1].weight, 200);
    EXPECT_EQ(uncompDq.back().tag, 1);
}

}   // namespace gp::tests
