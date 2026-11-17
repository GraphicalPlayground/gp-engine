// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/lists/LinkedList.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "hashing/Crc32.hpp"
#include "hashing/HashOf.hpp"
#include "hashing/Sha256.hpp"
#include "hashing/XxHash.hpp"
#include "memory/allocators/InlineAllocator.hpp"
#include "memory/allocators/LinearAllocator.hpp"
#include "memory/allocators/PolymorphicAllocator.hpp"
#include "memory/allocators/ThreadScratchAllocator.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include <gtest/gtest.h>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace gp::tests
{

namespace
{

struct SllTracker
{
    static inline gp::Int32 s_constructCount{ 0 };
    static inline gp::Int32 s_copyCount{ 0 };
    static inline gp::Int32 s_moveCount{ 0 };
    static inline gp::Int32 s_destructCount{ 0 };

    static void reset() noexcept
    {
        s_constructCount = 0;
        s_copyCount = 0;
        s_moveCount = 0;
        s_destructCount = 0;
    }

    gp::Int32 id{ 0 };

    SllTracker() noexcept
        : id(0)
    {
        ++s_constructCount;
    }

    explicit SllTracker(const gp::Int32 inId) noexcept
        : id(inId)
    {
        ++s_constructCount;
    }

    SllTracker(const SllTracker& other) noexcept
        : id(other.id)
    {
        ++s_copyCount;
    }

    SllTracker(SllTracker&& other) noexcept
        : id(other.id)
    {
        other.id = -1;
        ++s_moveCount;
    }

    SllTracker& operator=(const SllTracker& other) noexcept
    {
        id = other.id;
        ++s_copyCount;
        return *this;
    }

    SllTracker& operator=(SllTracker&& other) noexcept
    {
        id = other.id;
        other.id = -1;
        ++s_moveCount;
        return *this;
    }

    ~SllTracker() noexcept
    {
        ++s_destructCount;
    }

    [[nodiscard]] bool operator==(const SllTracker& other) const noexcept
    {
        return id == other.id;
    }
};

struct SllSortableItem
{
    gp::Int32 key{ 0 };
    gp::Int32 order{ 0 };

    [[nodiscard]] bool operator<(const SllSortableItem& other) const noexcept
    {
        return key < other.key;
    }

    [[nodiscard]] bool operator==(const SllSortableItem& other) const noexcept
    {
        return key == other.key && order == other.order;
    }
};

struct SllImmovable
{
    gp::Int32 value{ 0 };

    explicit SllImmovable(const gp::Int32 inValue) noexcept
        : value(inValue)
    {}

    SllImmovable(const SllImmovable&) = delete;
    SllImmovable& operator=(const SllImmovable&) = delete;
    SllImmovable(SllImmovable&&) = delete;
    SllImmovable& operator=(SllImmovable&&) = delete;
    ~SllImmovable() noexcept = default;
};

struct SllMoveOnlyKey
{
    gp::Int32 key{ 0 };

    explicit SllMoveOnlyKey(const gp::Int32 inKey) noexcept
        : key(inKey)
    {}

    SllMoveOnlyKey(const SllMoveOnlyKey&) = delete;
    SllMoveOnlyKey& operator=(const SllMoveOnlyKey&) = delete;
    SllMoveOnlyKey(SllMoveOnlyKey&&) noexcept = default;
    SllMoveOnlyKey& operator=(SllMoveOnlyKey&&) noexcept = default;
    ~SllMoveOnlyKey() noexcept = default;

    [[nodiscard]] bool operator==(const SllMoveOnlyKey& other) const noexcept
    {
        return key == other.key;
    }
};

struct alignas(64) SllAligned64Payload
{
    gp::Float32 lanes[16]{};
};

struct SllImmutableRecord
{
    const gp::Int32 id{ 0 };

    explicit SllImmutableRecord(const gp::Int32 inId) noexcept
        : id(inId)
    {}

    SllImmutableRecord(const SllImmutableRecord&) noexcept = default;
    SllImmutableRecord(SllImmutableRecord&&) noexcept = default;
    SllImmutableRecord& operator=(const SllImmutableRecord&) = delete;
    SllImmutableRecord& operator=(SllImmutableRecord&&) = delete;
    ~SllImmutableRecord() noexcept = default;
};

class SllSinglePassIter
{
public:
    using iterator_category = std::input_iterator_tag;
    using iterator_concept = std::input_iterator_tag;
    using value_type = gp::Int32;
    using difference_type = gp::ISize;

    SllSinglePassIter() noexcept = default;

    explicit SllSinglePassIter(const gp::Int32 current) noexcept
        : m_current(current)
    {}

    [[nodiscard]] gp::Int32 operator*() const noexcept
    {
        return m_current;
    }

    SllSinglePassIter& operator++() noexcept
    {
        ++m_current;
        return *this;
    }

    void operator++(int) noexcept
    {
        ++m_current;
    }

    [[nodiscard]] bool operator==(const SllSinglePassIter& other) const noexcept
    {
        return m_current == other.m_current;
    }

private:
    gp::Int32 m_current{ 0 };
};

template <typename InList>
void verifyLinkedListInvariants(const InList& list)
{
    EXPECT_EQ(list.isEmpty(), list.size() == 0u);
    EXPECT_LE(list.size(), list.capacity());
    if (list.isEmpty())
    {
        EXPECT_FALSE(list.headHandle().isValid());
        EXPECT_FALSE(list.tailHandle().isValid());
        EXPECT_EQ(list.begin(), list.end());
        return;
    }

    using Handle = typename InList::NodeHandle;
    const Handle head = list.headHandle();
    const Handle tail = list.tailHandle();
    EXPECT_TRUE(list.isValidHandle(head));
    EXPECT_TRUE(list.isValidHandle(tail));
    EXPECT_FALSE(list.nextHandle(tail).isValid());

    gp::USize count = 0u;
    Handle cur = head;
    Handle last = Handle::invalid();
    while (cur.isValid())
    {
        EXPECT_TRUE(list.isValidHandle(cur));
        last = cur;
        cur = list.nextHandle(cur);
        ++count;
    }
    EXPECT_EQ(count, list.size());
    EXPECT_EQ(last, tail);
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

TEST(LinkedListTests, FootprintConceptsAndEmptyState)
{
    using ListType = gp::LinkedList<gp::Int32>;
    using HandleType = ListType::NodeHandle;

    static_assert(sizeof(ListType) == 32u);
    static_assert(sizeof(gp::ScratchLinkedList<gp::Int32>) == 32u);
    static_assert(sizeof(gp::PolymorphicLinkedList<gp::Int32>) == 40u);
    static_assert(sizeof(HandleType) == 8u);

    static_assert(gp::concepts::IsLinkedList<ListType>);
    static_assert(gp::concepts::IsLinkedList<gp::InlineLinkedList<gp::Int32, 128u>>);
    static_assert(!gp::concepts::IsLinkedList<gp::Int32>);

    static_assert(gp::concepts::IsIterable<ListType>);
    static_assert(gp::concepts::IsSizedRange<ListType>);
    static_assert(gp::concepts::IsRelocatable<ListType>);
    static_assert(!gp::concepts::IsRelocatable<gp::InlineLinkedList<gp::Int32, 128u>>);
    static_assert(std::forward_iterator<ListType::Iterator>);
    static_assert(std::forward_iterator<ListType::ConstIterator>);

    ListType list;
    EXPECT_TRUE(list.isEmpty());
    EXPECT_EQ(list.size(), 0u);
    EXPECT_EQ(list.capacity(), 0u);
    EXPECT_GT(list.maxSize(), 0u);
    EXPECT_FALSE(list.headHandle().isValid());
    EXPECT_FALSE(list.tailHandle().isValid());
    EXPECT_EQ(list.tryFront(), nullptr);
    EXPECT_EQ(list.tryBack(), nullptr);
    EXPECT_EQ(std::as_const(list).tryFront(), nullptr);
    EXPECT_EQ(std::as_const(list).tryBack(), nullptr);
    EXPECT_EQ(list.tryGet(HandleType::invalid()), nullptr);
    EXPECT_FALSE(list.isValidHandle(HandleType::invalid()));
    EXPECT_FALSE(list.remove(HandleType::invalid()));
    EXPECT_FALSE(list.removeAfter(HandleType::invalid()));

    gp::Int32 popped = 0;
    EXPECT_FALSE(list.tryPopFront(popped));
}

TEST(LinkedListTests, ConstructorsAndAssignments)
{
    const gp::LinkedList<gp::Int32> sizedDefault(4u);
    ASSERT_EQ(sizedDefault.size(), 4u);
    for (gp::Int32 value: sizedDefault)
    {
        EXPECT_EQ(value, 0);
    }

    const gp::LinkedList<gp::Int32> sizedFill(3u, 11);
    ASSERT_EQ(sizedFill.size(), 3u);
    EXPECT_EQ(sizedFill.front(), 11);
    EXPECT_EQ(sizedFill.back(), 11);

    const gp::Int32 raw[4] = { 1, 2, 3, 4 };
    const gp::LinkedList<gp::Int32> ranged(raw, raw + 4);
    EXPECT_EQ(ranged.size(), 4u);

    gp::LinkedList<gp::Int32> initList{ 10, 20, 30 };
    const gp::LinkedList<gp::Int32> copied(initList);
    EXPECT_EQ(copied, initList);

    const gp::ScratchLinkedList<gp::Int32> crossCopied(initList);
    EXPECT_EQ(crossCopied, initList);

    const gp::LinkedList<gp::Int32>::NodeHandle hHead = initList.headHandle();
    gp::LinkedList<gp::Int32> moved(std::move(initList));
    EXPECT_TRUE(initList.isEmpty());
    EXPECT_TRUE(moved.isValidHandle(hHead));
    EXPECT_EQ(moved[hHead], 10);

    gp::LinkedList<gp::Int32> assigned;
    assigned = copied;
    EXPECT_EQ(assigned, copied);

    assigned = { 4, 5, 6 };
    expectSequence(assigned, { 4, 5, 6 });

    assigned.assign(3u, assigned.front());
    expectSequence(assigned, { 4, 4, 4 });

    assigned.assign(raw, raw + 4);
    expectSequence(assigned, { 1, 2, 3, 4 });

    gp::LinkedList<gp::Int32> moveAssigned;
    moveAssigned = std::move(assigned);
    expectSequence(moveAssigned, { 1, 2, 3, 4 });
    EXPECT_TRUE(assigned.isEmpty());
}

TEST(LinkedListTests, PushInsertRemoveAfterAndGenerationalSafety)
{
    using ListType = gp::LinkedList<gp::Int32>;
    using HandleType = ListType::NodeHandle;

    ListType list;
    const HandleType h20 = list.pushBack(20);
    const HandleType h10 = list.pushFront(10);
    const HandleType h30 = list.emplaceBack(30);
    const HandleType h05 = list.emplaceFront(5);
    const HandleType h25 = list.insertAfter(h20, 25);
    const HandleType h22 = list.emplaceAfter(h20, 22);

    // Sequence: [5, 10, 20, 22, 25, 30]
    expectSequence(list, { 5, 10, 20, 22, 25, 30 });
    EXPECT_EQ(list.headHandle(), h05);
    EXPECT_EQ(list.tailHandle(), h30);
    EXPECT_EQ(list.nextHandle(h20), h22);
    EXPECT_FALSE(list.nextHandle(h30).isValid());
    EXPECT_EQ(list.get(h22), 22);
    list[h22] = 23;
    EXPECT_EQ(*list.tryGet(h22), 23);

    // O(1) removeAfter updates tail when removing last node
    EXPECT_FALSE(list.removeAfter(h30));
    EXPECT_TRUE(list.removeAfter(h25));   // Removes h30
    EXPECT_EQ(list.tailHandle(), h25);
    EXPECT_FALSE(list.isValidHandle(h30));

    // O(1) head remove and O(N) middle remove
    EXPECT_TRUE(list.remove(h05));
    EXPECT_EQ(list.headHandle(), h10);
    EXPECT_TRUE(list.remove(h22));
    expectSequence(list, { 10, 20, 25 });

    // Reusing freed slot bumps generation so stale h22 stays invalid.
    const HandleType reused = list.insertAfter(h25, 35);
    EXPECT_NE(reused.generation, h22.generation);
    EXPECT_FALSE(list.isValidHandle(h22));
    EXPECT_EQ(list.tailHandle(), reused);
    EXPECT_FALSE(list.insertAfter(h22, 999).isValid());

    // popFront & tryPopFront
    list.popFront();
    EXPECT_EQ(list.front(), 20);
    gp::Int32 val = 0;
    EXPECT_TRUE(list.tryPopFront(val));
    EXPECT_EQ(val, 20);
    EXPECT_TRUE(list.tryPopFront(val));
    EXPECT_EQ(val, 25);
    EXPECT_TRUE(list.tryPopFront(val));
    EXPECT_EQ(val, 35);
    EXPECT_TRUE(list.isEmpty());
    EXPECT_FALSE(list.headHandle().isValid());
    EXPECT_FALSE(list.tailHandle().isValid());

    list.clear();
    EXPECT_FALSE(list.isValidHandle(reused));
}

TEST(LinkedListTests, ReverseSortRemoveAllRemoveIfAndIterators)
{
    using ListType = gp::LinkedList<gp::Int32>;
    ListType list{ 10, 20, 30, 40 };

    const ListType::Iterator it20 = list.find(20);
    ASSERT_NE(it20, list.end());
    const ListType::Iterator it25 = list.insertAfter(it20, 25);
    EXPECT_EQ(*it25, 25);
    const ListType::Iterator afterErase = list.eraseAfter(it20);
    EXPECT_EQ(*afterErase, 30);
    expectSequence(list, { 10, 20, 30, 40 });

    list.reverse();
    expectSequence(list, { 40, 30, 20, 10 });
    EXPECT_EQ(list.front(), 40);
    EXPECT_EQ(list.back(), 10);

    // Stable sort preserving handles
    gp::LinkedList<SllSortableItem> items;
    const gp::LinkedList<SllSortableItem>::NodeHandle a = items.pushBack({ 2, 1 });
    const gp::LinkedList<SllSortableItem>::NodeHandle b = items.pushBack({ 1, 1 });
    const gp::LinkedList<SllSortableItem>::NodeHandle c = items.pushBack({ 2, 2 });
    const gp::LinkedList<SllSortableItem>::NodeHandle d = items.pushBack({ 1, 2 });
    items.sort();
    EXPECT_EQ(items.headHandle(), b);
    EXPECT_EQ(items.nextHandle(b), d);
    EXPECT_EQ(items.nextHandle(d), a);
    EXPECT_EQ(items.nextHandle(a), c);
    EXPECT_EQ(items.tailHandle(), c);

    list.sort();
    expectSequence(list, { 10, 20, 30, 40 });
    list.sort(
        [](const gp::Int32 lhs, const gp::Int32 rhs) noexcept -> bool
    {
        return lhs > rhs;
    }
    );
    expectSequence(list, { 40, 30, 20, 10 });

    list.assign({ 1, 2, 1, 3, 1, 4 });
    EXPECT_EQ(list.removeAll(1), 3u);
    expectSequence(list, { 2, 3, 4 });
    EXPECT_EQ(
        list.removeIf(
            [](const gp::Int32 v) noexcept -> bool
    {
        return v >= 3;
    }
        ),
        2u
    );
    expectSequence(list, { 2 });

    const ListType::ConstIterator cit = list.cbegin();
    EXPECT_EQ(*cit, 2);
    EXPECT_EQ(*(cit.operator->()), 2);
    EXPECT_EQ(cit.handle(), list.headHandle());
    EXPECT_EQ(std::as_const(list).find(2), list.cbegin());
    EXPECT_TRUE(list.contains(2));
    EXPECT_EQ(list.count(2), 1u);
    EXPECT_EQ(
        list.findHandleIf(
            [](const gp::Int32 v) noexcept -> bool
    {
        return v == 2;
    }
        ),
        list.headHandle()
    );

    list.reserve(16u);
    list.shrinkToFit();
    list.compact();
    EXPECT_EQ(list.capacity(), list.size());
}

TEST(LinkedListTests, SelfAliasingLifecycleAllocatorsAndHashing)
{
    gp::LinkedList<gp::String> strList;
    strList.reserve(2u);
    const gp::LinkedList<gp::String>::NodeHandle h1 =
        strList.pushBack(gp::String("LongStringPayloadExceedingSso22Bytes"));
    const gp::LinkedList<gp::String>::NodeHandle h2 =
        strList.pushBack(gp::String("AnotherLongStringPayloadExceedingSso22"));
    const gp::LinkedList<gp::String>::NodeHandle h3 = strList.insertAfter(h2, strList[h1]);
    EXPECT_EQ(strList[h3], strList[h1]);

    SllTracker::reset();
    {
        gp::LinkedList<SllTracker> tracked;
        const gp::LinkedList<SllTracker>::NodeHandle t1 = tracked.emplaceBack(1);
        const gp::LinkedList<SllTracker>::NodeHandle t2 = tracked.emplaceBack(2);
        EXPECT_TRUE(tracked.remove(t1));
        tracked.reserve(8u);
        EXPECT_TRUE(tracked.isValidHandle(t2));
    }
    EXPECT_EQ(
        SllTracker::s_constructCount + SllTracker::s_copyCount + SllTracker::s_moveCount, SllTracker::s_destructCount
    );

    gp::LinkedList<SllImmovable> immovable;
    immovable.reserve(2u);
    const gp::LinkedList<SllImmovable>::NodeHandle ih = immovable.emplaceBack(77);
    EXPECT_EQ(immovable[ih].value, 77);

    gp::InlineLinkedList<gp::Int32, 128u> inlineList{ 1, 2, 3 };
    EXPECT_TRUE(inlineList.getAllocator().isInlinePointer(&inlineList.front()));
    gp::InlineLinkedList<gp::Int32, 128u> inlineCopy(inlineList);
    EXPECT_EQ(inlineCopy, inlineList);
    EXPECT_NE(&inlineCopy.front(), &inlineList.front());

    alignas(16) gp::UInt8 bufA[1024]{};
    gp::memory::LinearAllocator arenaA(bufA, sizeof(bufA), "SllArenaA");
    alignas(16) gp::UInt8 bufB[1024]{};
    gp::memory::LinearAllocator arenaB(bufB, sizeof(bufB), "SllArenaB");

    {
        gp::PolymorphicLinkedList<gp::Int32> listA{ gp::memory::PolymorphicAllocator(arenaA) };
        gp::PolymorphicLinkedList<gp::Int32> listB{ gp::memory::PolymorphicAllocator(arenaB) };
        (void)listA.pushBack(1);
        (void)listB.pushBack(2);
        gp::swap(listA, listB);
        EXPECT_EQ(listA.front(), 2);
        EXPECT_EQ(listB.front(), 1);
        EXPECT_TRUE(arenaA.owns(&listA.front()));
        EXPECT_TRUE(arenaB.owns(&listB.front()));
    }

    gp::memory::LinearAllocator& tlsArena = gp::memory::ThreadScratchArena::get();
    const gp::USize baseline = tlsArena.getUsedBytes();
    {
        const gp::memory::ScopedScratchMark mark;
        gp::ScratchLinkedList<gp::Int32> scratchList{ 1, 2, 3, 4 };
        EXPECT_GT(tlsArena.getUsedBytes(), baseline);
    }
    EXPECT_EQ(tlsArena.getUsedBytes(), baseline);

    const gp::LinkedList<gp::Int32> a{ 1, 2, 3 };
    gp::LinkedList<gp::Int32> b;
    const gp::LinkedList<gp::Int32>::NodeHandle tmp = b.pushBack(99);
    (void)b.pushBack(1);
    (void)b.pushBack(2);
    (void)b.pushBack(3);
    EXPECT_TRUE(b.remove(tmp));

    EXPECT_EQ(a, b);
    EXPECT_EQ(gp::hashing::hash(a), gp::hashing::hash(b));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::XxHash64>(a), gp::hashing::hash<gp::hashing::XxHash64>(b));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::Crc32>(a), gp::hashing::hash<gp::hashing::Crc32>(b));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::Sha256>(a), gp::hashing::hash<gp::hashing::Sha256>(b));
    const gp::LinkedList<gp::Int32> greater{ 1, 2, 4 };
    EXPECT_LT(a, greater);
}

TEST(LinkedListTests, AdditionalApiAndConstOverloadCoverage)
{
    alignas(16) gp::UInt8 arenaBuf[2048]{};
    gp::memory::LinearAllocator arena(arenaBuf, sizeof(arenaBuf), "SllApiArena");
    const gp::memory::PolymorphicAllocator alloc(arena);

    gp::PolymorphicLinkedList<gp::Int32> emptyAlloc{ alloc };
    gp::PolymorphicLinkedList<gp::Int32> movedAlloc{ gp::memory::PolymorphicAllocator(arena) };
    EXPECT_TRUE(emptyAlloc.isEmpty());
    EXPECT_TRUE(movedAlloc.isEmpty());

    gp::PolymorphicLinkedList<gp::Int32> countAlloc(3u, 66, alloc);
    expectSequence(countAlloc, { 66, 66, 66 });

    const gp::Int32 raw[3] = { 1, 2, 3 };
    gp::PolymorphicLinkedList<gp::Int32> rangeAlloc(raw, raw + 3, alloc);
    expectSequence(rangeAlloc, { 1, 2, 3 });

    gp::PolymorphicLinkedList<gp::Int32> initAlloc({ 9, 8, 7 }, alloc);
    expectSequence(initAlloc, { 9, 8, 7 });

    const gp::LinkedList<gp::Int32> crossCopied(initAlloc);
    expectSequence(crossCopied, { 9, 8, 7 });

    rangeAlloc.swap(initAlloc);
    expectSequence(rangeAlloc, { 9, 8, 7 });
    expectSequence(initAlloc, { 1, 2, 3 });
    initAlloc.swap(initAlloc);

    gp::LinkedList<gp::String> list;
    const gp::LinkedList<gp::String>::NodeHandle hHead = list.emplaceFront(gp::String("first"));
    const gp::LinkedList<gp::String>::Iterator itThird = list.emplaceAfter(list.cbegin(), gp::String("third"));
    (void)list.insertAfter(list.cbegin(), gp::String("second"));
    (void)list.insertAfter(itThird.handle(), gp::String("fourth"));
    EXPECT_EQ(list.size(), 4u);

    const gp::LinkedList<gp::String>::NodeHandle hSecond = list.findHandle(gp::String("second"));
    EXPECT_TRUE(list.isValidHandle(hSecond));
    EXPECT_FALSE(list.findHandle(gp::String("missing")).isValid());

    ASSERT_NE(list.tryFront(), nullptr);
    ASSERT_NE(list.tryBack(), nullptr);
    (void)list.getAllocator();

    const gp::LinkedList<gp::String>& constRef = list;
    EXPECT_EQ(constRef[hHead], "first");
    EXPECT_EQ(constRef.get(hHead), "first");
    ASSERT_NE(constRef.tryGet(hHead), nullptr);
    EXPECT_EQ(*constRef.tryGet(hHead), "first");
    EXPECT_EQ(constRef.front(), "first");
    EXPECT_EQ(constRef.back(), "fourth");
    ASSERT_NE(constRef.tryFront(), nullptr);
    ASSERT_NE(constRef.tryBack(), nullptr);
    EXPECT_EQ(*constRef.tryFront(), "first");
    EXPECT_EQ(*constRef.tryBack(), "fourth");
    EXPECT_EQ(*constRef.begin(), "first");
    EXPECT_NE(constRef.begin(), constRef.end());
    (void)constRef.getAllocator();

    list.reset();
    EXPECT_TRUE(list.isEmpty());
    EXPECT_EQ(list.capacity(), 0u);
    EXPECT_FALSE(list.isValidHandle(hHead));
}

TEST(LinkedListTests, AbaCompactSelfAliasRemoveAllMoveOnlyAlignedAndEdgeCases)
{
    // Regression test: compact() relocating a lower-generation node into a previously freed high-gen slot (slot 0)
    // must keep stale handles addressing slot 0 invalidated (zero ABA collision).
    gp::LinkedList<gp::Int32> compactList;
    const gp::LinkedList<gp::Int32>::NodeHandle hA = compactList.pushBack(10);   // {index=0, gen=1}
    EXPECT_TRUE(compactList.remove(hA));                                         // slot 0 gen -> 2
    const gp::LinkedList<gp::Int32>::NodeHandle hB = compactList.pushBack(20);   // {index=0, gen=3}
    const gp::LinkedList<gp::Int32>::NodeHandle hC = compactList.pushBack(30);   // {index=1, gen=1}
    ASSERT_EQ(hB.index, 0u);
    ASSERT_EQ(hB.generation, 3u);
    EXPECT_TRUE(compactList.remove(hB));

    compactList.compact();
    EXPECT_EQ(compactList.size(), 1u);
    EXPECT_EQ(compactList.front(), 30);
    EXPECT_FALSE(compactList.isValidHandle(hB));
    EXPECT_FALSE(compactList.isValidHandle(hC));
    EXPECT_EQ(compactList.headHandle().index, 0u);
    EXPECT_GT(compactList.headHandle().generation, hB.generation);

    // Self-aliasing removeAll(front()) with long heap-allocated gp::String (copyable staging path)
    gp::LinkedList<gp::String> strList;
    (void)strList.pushBack(gp::String("SharedLongStringPayloadExceedingSso22Bytes"));
    (void)strList.pushBack(gp::String("UniqueMiddlePayloadExceedingSso22Bytes"));
    (void)strList.pushBack(gp::String("SharedLongStringPayloadExceedingSso22Bytes"));
    (void)strList.pushBack(gp::String("SharedLongStringPayloadExceedingSso22Bytes"));
    EXPECT_EQ(strList.removeAll(strList.front()), 3u);
    EXPECT_EQ(strList.size(), 1u);
    EXPECT_EQ(strList.front(), "UniqueMiddlePayloadExceedingSso22Bytes");
    EXPECT_EQ(strList.tailHandle(), strList.headHandle());

    // Self-aliasing removeAll(front()) on move-only elements (deferred slot release path)
    gp::LinkedList<SllMoveOnlyKey> moveKeyList;
    (void)moveKeyList.emplaceBack(5);
    (void)moveKeyList.emplaceBack(1);
    (void)moveKeyList.emplaceBack(5);
    (void)moveKeyList.emplaceBack(3);
    (void)moveKeyList.emplaceBack(5);
    EXPECT_EQ(moveKeyList.removeAll(moveKeyList.front()), 3u);
    ASSERT_EQ(moveKeyList.size(), 2u);
    EXPECT_EQ(moveKeyList.front().key, 1);
    EXPECT_EQ(moveKeyList.back().key, 3);

    // Move-only UniquePtr end-to-end
    {
        gp::LinkedList<gp::UniquePtr<gp::Int32>> ptrList;
        const gp::LinkedList<gp::UniquePtr<gp::Int32>>::NodeHandle hFirst =
            ptrList.pushBack(gp::makeUnique<gp::Int32>(20));
        (void)ptrList.pushFront(gp::makeUnique<gp::Int32>(10));
        (void)ptrList.insertAfter(hFirst, gp::makeUnique<gp::Int32>(30));
        ptrList.reverse();
        EXPECT_EQ(*ptrList.front(), 30);
        ptrList.sort(
            [](const gp::UniquePtr<gp::Int32>& lhs, const gp::UniquePtr<gp::Int32>& rhs) noexcept -> bool
        {
            return *lhs < *rhs;
        }
        );
        EXPECT_EQ(*ptrList.front(), 10);
        EXPECT_EQ(*ptrList.back(), 30);
        ptrList.compact();
        ptrList.shrinkToFit();
        EXPECT_EQ(ptrList.size(), 3u);
    }

    // 0-element / 1-element boundary cases, tail eraseAfter(), findIf(), comparisons, self-assignment
    gp::LinkedList<gp::Int32> emptyList;
    emptyList.reverse();
    emptyList.sort();
    emptyList.shrinkToFit();
    emptyList.compact();
    EXPECT_EQ(emptyList.removeAll(1), 0u);
    EXPECT_EQ(
        emptyList.findIf(
            [](const gp::Int32 v) noexcept -> bool
    {
        return v == 1;
    }
        ),
        emptyList.end()
    );

    gp::LinkedList<gp::Int32> singleList;
    const gp::LinkedList<gp::Int32>::NodeHandle hOnly = singleList.pushBack(42);
    singleList.reverse();
    singleList.sort();
    EXPECT_EQ(singleList.headHandle(), hOnly);
    EXPECT_EQ(singleList.tailHandle(), hOnly);
    EXPECT_EQ(singleList.eraseAfter(singleList.cbegin()), singleList.end());
    EXPECT_EQ(
        *singleList.findIf(
            [](const gp::Int32 v) noexcept -> bool
    {
        return v == 42;
    }
        ),
        42
    );
    EXPECT_EQ(
        std::as_const(singleList)
            .findIf(
                [](const gp::Int32 v) noexcept -> bool
    {
        return v == 99;
    }
            ),
        singleList.cend()
    );
    EXPECT_EQ(singleList.begin(), singleList.cbegin());
    EXPECT_TRUE(singleList.remove(hOnly));
    EXPECT_TRUE(singleList.isEmpty());
    EXPECT_FALSE(singleList.headHandle().isValid());
    EXPECT_FALSE(singleList.tailHandle().isValid());

    gp::LinkedList<gp::Int32> cmpA{ 1, 2 };
    gp::LinkedList<gp::Int32>& aliasA = cmpA;
    cmpA = aliasA;
    cmpA = std::move(aliasA);
    EXPECT_NE(cmpA, emptyList);
    EXPECT_GT(cmpA, emptyList);
    EXPECT_LE(emptyList, cmpA);
    EXPECT_GE(cmpA, cmpA);
    EXPECT_LE(cmpA, cmpA);

    // Over-aligned 64-byte node alignment check
    gp::LinkedList<SllAligned64Payload> alignedList;
    (void)alignedList.emplaceBack();
    (void)alignedList.emplaceBack();
    EXPECT_EQ(reinterpret_cast<gp::USize>(&alignedList.front()) % 64u, 0u);
    EXPECT_EQ(reinterpret_cast<gp::USize>(&alignedList.back()) % 64u, 0u);
}

TEST(LinkedListTests, InvariantsMultiCardinalitySortInlineSwapAndNestedRelocation)
{
    const gp::USize counts[5] = { 2u, 3u, 5u, 9u, 16u };
    for (gp::USize idx = 0u; idx < 5u; ++idx)
    {
        const gp::USize count = counts[idx];
        gp::LinkedList<gp::Int32> list;
        const gp::LinkedList<gp::Int32>::NodeHandle dummy = list.pushBack(-1);
        for (gp::USize i = 0u; i < count; ++i)
        {
            const gp::Int32 val = static_cast<gp::Int32>((i * 5u + 2u) % 11u);
            if ((i & 1u) == 0u)
            {
                (void)list.pushFront(val);
            }
            else
            {
                (void)list.pushBack(val);
            }
        }
        EXPECT_TRUE(list.remove(dummy));
        verifyLinkedListInvariants(list);

        list.sort();
        verifyLinkedListInvariants(list);

        gp::LinkedList<gp::Int32>::ConstIterator it = list.cbegin();
        gp::Int32 prevVal = *it;
        ++it;
        for (; it != list.cend(); ++it)
        {
            EXPECT_LE(prevVal, *it);
            prevVal = *it;
        }

        list.reverse();
        verifyLinkedListInvariants(list);
    }

    // InlineLinkedList swap & occupied move assignment stay inside inline buffer
    gp::InlineLinkedList<gp::Int32, 256u> inlineA{ 1, 2, 3 };
    gp::InlineLinkedList<gp::Int32, 256u> inlineB{ 10, 20, 30, 40 };
    EXPECT_TRUE(inlineA.getAllocator().isInlinePointer(&inlineA.front()));
    EXPECT_TRUE(inlineB.getAllocator().isInlinePointer(&inlineB.front()));
    gp::swap(inlineA, inlineB);
    EXPECT_TRUE(inlineA.getAllocator().isInlinePointer(&inlineA.front()));
    EXPECT_TRUE(inlineB.getAllocator().isInlinePointer(&inlineB.front()));
    expectSequence(inlineA, { 10, 20, 30, 40 });
    expectSequence(inlineB, { 1, 2, 3 });
    inlineB = std::move(inlineA);
    EXPECT_TRUE(inlineB.getAllocator().isInlinePointer(&inlineB.front()));
    verifyLinkedListInvariants(inlineB);

    // Move-only UniquePtr tryPopFront, removeAfter, and remove tail
    gp::LinkedList<gp::UniquePtr<gp::Int32>> moveList;
    const gp::LinkedList<gp::UniquePtr<gp::Int32>>::NodeHandle h1 = moveList.pushBack(gp::makeUnique<gp::Int32>(1));
    const gp::LinkedList<gp::UniquePtr<gp::Int32>>::NodeHandle h2 = moveList.pushBack(gp::makeUnique<gp::Int32>(2));
    const gp::LinkedList<gp::UniquePtr<gp::Int32>>::NodeHandle h3 = moveList.pushBack(gp::makeUnique<gp::Int32>(3));
    EXPECT_TRUE(moveList.removeAfter(h1));
    EXPECT_FALSE(moveList.isValidHandle(h2));
    EXPECT_TRUE(moveList.remove(h3));
    EXPECT_EQ(moveList.tailHandle(), h1);
    gp::UniquePtr<gp::Int32> popped;
    EXPECT_TRUE(moveList.tryPopFront(popped));
    EXPECT_EQ(*popped, 1);
    EXPECT_FALSE(moveList.tryPopFront(popped));
    verifyLinkedListInvariants(moveList);

    // ImmutableRecord (const field, non-assignable) full handle & algorithm compatibility
    gp::LinkedList<SllImmutableRecord> records;
    const gp::LinkedList<SllImmutableRecord>::NodeHandle r2 = records.emplaceBack(2);
    (void)records.emplaceFront(1);
    (void)records.emplaceAfter(r2, 3);
    records.reverse();
    records.sort(
        [](const SllImmutableRecord& lhs, const SllImmutableRecord& rhs) noexcept -> bool
    {
        return lhs.id < rhs.id;
    }
    );
    records.compact();
    EXPECT_EQ(records.front().id, 1);
    EXPECT_EQ(records.back().id, 3);
    verifyLinkedListInvariants(records);

    // Single-pass non-forward InputIterator range ctor & assign()
    static_assert(!std::forward_iterator<SllSinglePassIter>);
    const gp::LinkedList<gp::Int32> genCtor(SllSinglePassIter(3), SllSinglePassIter(7));
    expectSequence(genCtor, { 3, 4, 5, 6 });
    gp::LinkedList<gp::Int32> genAssign;
    genAssign.assign(SllSinglePassIter(8), SllSinglePassIter(11));
    expectSequence(genAssign, { 8, 9, 10 });

    // Nested relocatable LinkedList<LinkedList<Int32>> across slot-pool growth
    gp::LinkedList<gp::LinkedList<gp::Int32>> nested;
    for (gp::Int32 i = 0; i < 10; ++i)
    {
        gp::LinkedList<gp::Int32> child{ i, i + 200 };
        (void)nested.pushBack(std::move(child));
    }
    EXPECT_EQ(nested.size(), 10u);
    EXPECT_EQ(nested.back().back(), 209);

    // Nested hash boundary framing prevents prefix collisions across inner lists
    gp::LinkedList<gp::LinkedList<gp::Int32>> nestedA;
    (void)nestedA.pushBack(gp::LinkedList<gp::Int32>{ 1, 2 });
    (void)nestedA.pushBack(gp::LinkedList<gp::Int32>{ 3 });
    gp::LinkedList<gp::LinkedList<gp::Int32>> nestedB;
    (void)nestedB.pushBack(gp::LinkedList<gp::Int32>{ 1 });
    (void)nestedB.pushBack(gp::LinkedList<gp::Int32>{ 2, 3 });
    EXPECT_NE(gp::hashing::hash(nestedA), gp::hashing::hash(nestedB));
    EXPECT_NE(gp::hashing::hash(gp::LinkedList<gp::Int32>{}), gp::hashing::hash(gp::LinkedList<gp::Int32>{ 0 }));

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

    gp::LinkedList<UncomparablePair> uncompList;
    const gp::LinkedList<UncomparablePair>::NodeHandle hHead = uncompList.emplaceFront(0, 10);
    (void)uncompList.emplaceAfter(hHead, 1, 100);
    (void)uncompList.emplaceBack(2, 200);
    (void)uncompList.emplaceAfter(uncompList.cbegin(), 3, 300);
    EXPECT_EQ(uncompList.front().tag, 0);
    EXPECT_EQ(uncompList.back().weight, 200);
    EXPECT_EQ(uncompList.size(), 4u);
}

}   // namespace gp::tests
