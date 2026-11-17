// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/lists/DoubleLinkedList.hpp"
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
#include <type_traits>
#include <utility>

namespace gp::tests
{

namespace
{

struct DllTracker
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

    DllTracker() noexcept
        : id(0)
    {
        ++s_constructCount;
    }

    explicit DllTracker(const gp::Int32 inId) noexcept
        : id(inId)
    {
        ++s_constructCount;
    }

    DllTracker(const DllTracker& other) noexcept
        : id(other.id)
    {
        ++s_copyCount;
    }

    DllTracker(DllTracker&& other) noexcept
        : id(other.id)
    {
        other.id = -1;
        ++s_moveCount;
    }

    DllTracker& operator=(const DllTracker& other) noexcept
    {
        id = other.id;
        ++s_copyCount;
        return *this;
    }

    DllTracker& operator=(DllTracker&& other) noexcept
    {
        id = other.id;
        other.id = -1;
        ++s_moveCount;
        return *this;
    }

    ~DllTracker() noexcept
    {
        ++s_destructCount;
    }

    [[nodiscard]] bool operator==(const DllTracker& other) const noexcept
    {
        return id == other.id;
    }
};

struct SortableTagItem
{
    gp::Int32 key{ 0 };
    gp::Int32 order{ 0 };

    [[nodiscard]] bool operator<(const SortableTagItem& other) const noexcept
    {
        return key < other.key;
    }

    [[nodiscard]] bool operator==(const SortableTagItem& other) const noexcept
    {
        return key == other.key && order == other.order;
    }
};

struct DllImmovable
{
    gp::Int32 value{ 0 };

    explicit DllImmovable(const gp::Int32 inValue) noexcept
        : value(inValue)
    {}

    DllImmovable(const DllImmovable&) = delete;
    DllImmovable& operator=(const DllImmovable&) = delete;
    DllImmovable(DllImmovable&&) = delete;
    DllImmovable& operator=(DllImmovable&&) = delete;
    ~DllImmovable() noexcept = default;
};

struct DllMoveOnlyKey
{
    gp::Int32 key{ 0 };

    explicit DllMoveOnlyKey(const gp::Int32 inKey) noexcept
        : key(inKey)
    {}

    DllMoveOnlyKey(const DllMoveOnlyKey&) = delete;
    DllMoveOnlyKey& operator=(const DllMoveOnlyKey&) = delete;
    DllMoveOnlyKey(DllMoveOnlyKey&&) noexcept = default;
    DllMoveOnlyKey& operator=(DllMoveOnlyKey&&) noexcept = default;
    ~DllMoveOnlyKey() noexcept = default;

    [[nodiscard]] bool operator==(const DllMoveOnlyKey& other) const noexcept
    {
        return key == other.key;
    }
};

struct alignas(64) DllAligned64Payload
{
    gp::Float32 lanes[16]{};
};

struct DllImmutableRecord
{
    const gp::Int32 id{ 0 };

    explicit DllImmutableRecord(const gp::Int32 inId) noexcept
        : id(inId)
    {}

    DllImmutableRecord(const DllImmutableRecord&) noexcept = default;
    DllImmutableRecord(DllImmutableRecord&&) noexcept = default;
    DllImmutableRecord& operator=(const DllImmutableRecord&) = delete;
    DllImmutableRecord& operator=(DllImmutableRecord&&) = delete;
    ~DllImmutableRecord() noexcept = default;
};

class DllSinglePassIter
{
public:
    using iterator_category = std::input_iterator_tag;
    using iterator_concept = std::input_iterator_tag;
    using value_type = gp::Int32;
    using difference_type = gp::ISize;

    DllSinglePassIter() noexcept = default;

    explicit DllSinglePassIter(const gp::Int32 current) noexcept
        : m_current(current)
    {}

    [[nodiscard]] gp::Int32 operator*() const noexcept
    {
        return m_current;
    }

    DllSinglePassIter& operator++() noexcept
    {
        ++m_current;
        return *this;
    }

    void operator++(int) noexcept
    {
        ++m_current;
    }

    [[nodiscard]] bool operator==(const DllSinglePassIter& other) const noexcept
    {
        return m_current == other.m_current;
    }

private:
    gp::Int32 m_current{ 0 };
};

template <typename InList>
void verifyDoubleListInvariants(const InList& list)
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
    EXPECT_FALSE(list.prevHandle(head).isValid());
    EXPECT_FALSE(list.nextHandle(tail).isValid());

    gp::USize forwardCount = 0u;
    Handle cur = head;
    Handle prev = Handle::invalid();
    while (cur.isValid())
    {
        EXPECT_TRUE(list.isValidHandle(cur));
        EXPECT_EQ(list.prevHandle(cur), prev);
        prev = cur;
        cur = list.nextHandle(cur);
        ++forwardCount;
    }
    EXPECT_EQ(forwardCount, list.size());
    EXPECT_EQ(prev, tail);

    gp::USize reverseCount = 0u;
    cur = tail;
    while (cur.isValid())
    {
        cur = list.prevHandle(cur);
        ++reverseCount;
    }
    EXPECT_EQ(reverseCount, list.size());
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

TEST(DoubleLinkedListTests, FootprintConceptsAndHandleProperties)
{
    using ListType = gp::DoubleLinkedList<gp::Int32>;
    using HandleType = ListType::NodeHandle;

    static_assert(sizeof(ListType) == 32u);
    static_assert(sizeof(gp::ScratchDoubleLinkedList<gp::Int32>) == 32u);
    static_assert(sizeof(gp::PolymorphicDoubleLinkedList<gp::Int32>) == 40u);
    static_assert(sizeof(HandleType) == 8u);

    static_assert(gp::concepts::IsDoubleLinkedList<ListType>);
    static_assert(gp::concepts::IsDoubleLinkedList<gp::InlineDoubleLinkedList<gp::Int32, 128u>>);
    static_assert(!gp::concepts::IsDoubleLinkedList<gp::Int32>);

    static_assert(gp::concepts::IsIterable<ListType>);
    static_assert(gp::concepts::IsSizedRange<ListType>);
    static_assert(gp::concepts::IsRelocatable<ListType>);
    static_assert(!gp::concepts::IsRelocatable<gp::InlineDoubleLinkedList<gp::Int32, 128u>>);
    static_assert(std::bidirectional_iterator<ListType::Iterator>);
    static_assert(std::bidirectional_iterator<ListType::ConstIterator>);

    HandleType invalid = HandleType::invalid();
    EXPECT_FALSE(invalid.isValid());
    EXPECT_FALSE(static_cast<bool>(invalid));

    HandleType valid{ 0u, 1u };
    EXPECT_TRUE(valid.isValid());
    EXPECT_TRUE(static_cast<bool>(valid));
    EXPECT_NE(valid, invalid);
    EXPECT_NE(gp::hashing::hash(valid), gp::hashing::hash(invalid));
    EXPECT_EQ(gp::hashing::hash(valid), valid.hash());
    EXPECT_EQ(
        gp::hashing::hash<gp::hashing::Fnv1a64>(valid),
        gp::hashing::hashValues<gp::hashing::Fnv1a64>(valid.index, valid.generation)
    );
    EXPECT_EQ(
        gp::hashing::hash<gp::hashing::XxHash64>(valid),
        gp::hashing::hashValues<gp::hashing::XxHash64>(valid.index, valid.generation)
    );
    valid.invalidate();
    EXPECT_EQ(valid, invalid);

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
    EXPECT_EQ(list.tryGet(invalid), nullptr);
    EXPECT_FALSE(list.isValidHandle(invalid));
    EXPECT_FALSE(list.remove(invalid));

    gp::Int32 popped = 0;
    EXPECT_FALSE(list.tryPopFront(popped));
    EXPECT_FALSE(list.tryPopBack(popped));
}

TEST(DoubleLinkedListTests, ConstructorsAndAssignments)
{
    const gp::DoubleLinkedList<gp::Int32> sizedDefault(4u);
    ASSERT_EQ(sizedDefault.size(), 4u);
    for (gp::Int32 value: sizedDefault)
    {
        EXPECT_EQ(value, 0);
    }

    const gp::DoubleLinkedList<gp::Int32> sizedFill(3u, 42);
    ASSERT_EQ(sizedFill.size(), 3u);
    EXPECT_EQ(sizedFill.front(), 42);
    EXPECT_EQ(sizedFill.back(), 42);

    const gp::Int32 raw[4] = { 1, 2, 3, 4 };
    const gp::DoubleLinkedList<gp::Int32> ranged(raw, raw + 4);
    EXPECT_EQ(ranged.size(), 4u);

    gp::DoubleLinkedList<gp::Int32> initList{ 10, 20, 30 };
    EXPECT_EQ(initList.size(), 3u);

    const gp::DoubleLinkedList<gp::Int32> copied(initList);
    EXPECT_EQ(copied, initList);

    const gp::ScratchDoubleLinkedList<gp::Int32> crossCopied(initList);
    EXPECT_EQ(crossCopied, initList);

    // Move preserves existing handles in O(1) when allocators are equal.
    const gp::DoubleLinkedList<gp::Int32>::NodeHandle hHead = initList.headHandle();
    gp::DoubleLinkedList<gp::Int32> moved(std::move(initList));
    EXPECT_TRUE(initList.isEmpty());
    EXPECT_FALSE(initList.isValidHandle(hHead));
    EXPECT_TRUE(moved.isValidHandle(hHead));
    EXPECT_EQ(moved[hHead], 10);

    gp::DoubleLinkedList<gp::Int32> assigned;
    assigned = copied;
    EXPECT_EQ(assigned, copied);

    assigned = { 5, 6, 7 };
    expectSequence(assigned, { 5, 6, 7 });

    assigned.assign(3u, assigned.front());
    expectSequence(assigned, { 5, 5, 5 });

    assigned.assign(raw, raw + 4);
    expectSequence(assigned, { 1, 2, 3, 4 });

    gp::DoubleLinkedList<gp::Int32> moveAssigned;
    moveAssigned = std::move(assigned);
    expectSequence(moveAssigned, { 1, 2, 3, 4 });
    EXPECT_TRUE(assigned.isEmpty());
}

TEST(DoubleLinkedListTests, PushInsertRemoveAndGenerationalSafety)
{
    using ListType = gp::DoubleLinkedList<gp::Int32>;
    using HandleType = ListType::NodeHandle;

    ListType list;
    const HandleType h20 = list.pushBack(20);
    const HandleType h10 = list.pushFront(10);
    const HandleType h30 = list.emplaceBack(30);
    const HandleType h05 = list.emplaceFront(5);
    const HandleType h15 = list.insertBefore(h20, 15);
    const HandleType h25 = list.emplaceAfter(h20, 25);

    // Sequence: [5, 10, 15, 20, 25, 30]
    expectSequence(list, { 5, 10, 15, 20, 25, 30 });
    EXPECT_EQ(list.headHandle(), h05);
    EXPECT_EQ(list.tailHandle(), h30);
    EXPECT_EQ(list.nextHandle(h15), h20);
    EXPECT_EQ(list.prevHandle(h15), h10);
    EXPECT_FALSE(list.prevHandle(h05).isValid());
    EXPECT_FALSE(list.nextHandle(h30).isValid());
    EXPECT_EQ(list.get(h25), 25);
    list[h25] = 26;
    EXPECT_EQ(*list.tryGet(h25), 26);

    // Verify all handles stay valid across slot-pool growth.
    for (gp::Int32 i = 100; i < 164; ++i)
    {
        (void)list.pushBack(i);
    }
    EXPECT_TRUE(list.isValidHandle(h05));
    EXPECT_TRUE(list.isValidHandle(h25));
    EXPECT_EQ(list.get(h25), 26);

    // Remove h25 and verify stale handle catches ABA / double-remove / neighbor ops.
    EXPECT_TRUE(list.remove(h25));
    EXPECT_FALSE(list.isValidHandle(h25));
    EXPECT_EQ(list.tryGet(h25), nullptr);
    EXPECT_EQ(std::as_const(list).tryGet(h25), nullptr);
    EXPECT_FALSE(list.remove(h25));
    EXPECT_FALSE(list.insertBefore(h25, 999).isValid());
    EXPECT_FALSE(list.insertAfter(h25, 999).isValid());
    EXPECT_FALSE(list.nextHandle(h25).isValid());
    EXPECT_FALSE(list.prevHandle(h25).isValid());

    // Reusing freed slot bumps generation so old h25 remains invalid.
    const HandleType reused = list.insertAfter(h20, 27);
    EXPECT_EQ(reused.index, h25.index);
    EXPECT_NE(reused.generation, h25.generation);
    EXPECT_TRUE(list.isValidHandle(reused));
    EXPECT_FALSE(list.isValidHandle(h25));
    EXPECT_EQ(list[reused], 27);

    // Even after clear(), handles created prior to clear() remain stale when slots are reused.
    const HandleType prevTail = list.tailHandle();
    list.clear();
    EXPECT_TRUE(list.isEmpty());
    EXPECT_FALSE(list.isValidHandle(reused));
    EXPECT_FALSE(list.isValidHandle(prevTail));
    const HandleType afterClear = list.pushBack(500);
    EXPECT_EQ(afterClear.index, prevTail.index);
    EXPECT_NE(afterClear.generation, prevTail.generation);
    EXPECT_NE(afterClear, reused);
    EXPECT_FALSE(list.isValidHandle(reused));
}

TEST(DoubleLinkedListTests, LruRelinkingReverseSortAndFilters)
{
    using ListType = gp::DoubleLinkedList<gp::Int32>;
    using HandleType = ListType::NodeHandle;

    ListType list;
    const HandleType h1 = list.pushBack(1);
    const HandleType h2 = list.pushBack(2);
    const HandleType h3 = list.pushBack(3);
    const HandleType h4 = list.pushBack(4);

    // O(1) moveToFront and moveToBack preserve handles
    EXPECT_TRUE(list.moveToFront(h3));
    EXPECT_TRUE(list.moveToFront(h3));   // No-op when already head
    expectSequence(list, { 3, 1, 2, 4 });

    EXPECT_TRUE(list.moveToBack(h1));
    EXPECT_TRUE(list.moveToBack(h1));   // No-op when already tail
    expectSequence(list, { 3, 2, 4, 1 });
    EXPECT_FALSE(list.moveToFront(HandleType::invalid()));
    EXPECT_FALSE(list.moveToBack(HandleType::invalid()));

    list.reverse();
    expectSequence(list, { 1, 4, 2, 3 });
    EXPECT_EQ(list.headHandle(), h1);
    EXPECT_EQ(list.tailHandle(), h3);

    // Verify reverse iteration checks all prev links after reverse().
    gp::Int32 expectedRev[4] = { 3, 2, 4, 1 };
    gp::USize revIdx = 0u;
    for (ListType::ConstReverseIterator rit = list.crbegin(); rit != list.crend(); ++rit)
    {
        EXPECT_EQ(*rit, expectedRev[revIdx++]);
    }

    // Sort preserves handles and stability for equal keys.
    gp::DoubleLinkedList<SortableTagItem> items;
    const gp::DoubleLinkedList<SortableTagItem>::NodeHandle a = items.pushBack({ 3, 1 });
    const gp::DoubleLinkedList<SortableTagItem>::NodeHandle b = items.pushBack({ 1, 1 });
    const gp::DoubleLinkedList<SortableTagItem>::NodeHandle c = items.pushBack({ 3, 2 });
    const gp::DoubleLinkedList<SortableTagItem>::NodeHandle d = items.pushBack({ 2, 1 });
    const gp::DoubleLinkedList<SortableTagItem>::NodeHandle e = items.pushBack({ 1, 2 });
    items.sort();

    ASSERT_EQ(items.size(), 5u);
    EXPECT_EQ(items.headHandle(), b);
    EXPECT_EQ(items.nextHandle(b), e);
    EXPECT_EQ(items.nextHandle(e), d);
    EXPECT_EQ(items.nextHandle(d), a);
    EXPECT_EQ(items.nextHandle(a), c);
    EXPECT_EQ(items.tailHandle(), c);
    EXPECT_EQ(items.prevHandle(c), a);
    EXPECT_EQ(items.prevHandle(a), d);

    // Custom descending sort
    list.sort(
        [](const gp::Int32 lhs, const gp::Int32 rhs) noexcept -> bool
    {
        return lhs > rhs;
    }
    );
    expectSequence(list, { 4, 3, 2, 1 });

    // popFront / popBack / tryPopFront / tryPopBack / removeAll / removeIf
    list.popFront();
    list.popBack();
    expectSequence(list, { 3, 2 });
    gp::Int32 val = 0;
    EXPECT_TRUE(list.tryPopFront(val));
    EXPECT_EQ(val, 3);
    EXPECT_TRUE(list.tryPopBack(val));
    EXPECT_EQ(val, 2);
    EXPECT_TRUE(list.isEmpty());

    list.assign({ 1, 2, 3, 2, 4, 2, 5 });
    EXPECT_EQ(list.removeAll(2), 3u);
    expectSequence(list, { 1, 3, 4, 5 });
    EXPECT_EQ(
        list.removeIf(
            [](const gp::Int32 v) noexcept -> bool
    {
        return (v % 2) != 0;
    }
        ),
        3u
    );
    expectSequence(list, { 4 });
}

TEST(DoubleLinkedListTests, IteratorsSearchReserveShrinkAndCompact)
{
    using ListType = gp::DoubleLinkedList<gp::Int32>;
    ListType list{ 10, 20, 30, 40 };

    ListType::Iterator it = list.end();
    --it;
    EXPECT_EQ(*it, 40);
    EXPECT_EQ(it.handle(), list.tailHandle());
    it--;
    EXPECT_EQ(*it, 30);

    const ListType::Iterator inserted = list.insert(it, 25);
    EXPECT_EQ(*inserted, 25);
    (void)list.insert(list.cend(), 50);
    const ListType::Iterator afterErase = list.erase(inserted);
    EXPECT_EQ(*afterErase, 30);
    expectSequence(list, { 10, 20, 30, 40, 50 });

    EXPECT_EQ(*list.find(30), 30);
    EXPECT_EQ(list.find(999), list.end());
    EXPECT_EQ(std::as_const(list).find(20).handle(), list.nextHandle(list.headHandle()));
    EXPECT_TRUE(list.contains(40));
    EXPECT_FALSE(list.contains(999));
    EXPECT_EQ(list.count(20), 1u);

    const ListType::NodeHandle matchPred = list.findHandleIf(
        [](const gp::Int32 v) noexcept -> bool
    {
        return v > 25;
    }
    );
    EXPECT_EQ(list[matchPred], 30);

    // Reserve extra slots, then shrinkToFit trims trailing uncarved slack while preserving active handles.
    const ListType::NodeHandle keepHandle = list.tailHandle();
    list.reserve(32u);
    EXPECT_GE(list.capacity(), 32u);
    list.shrinkToFit();
    EXPECT_EQ(list.capacity(), 6u);   // 5 live + 1 previously removed slot
    EXPECT_TRUE(list.isValidHandle(keepHandle));
    EXPECT_EQ(list[keepHandle], 50);

    // compact() eliminates free-list holes and shrinks capacity to exact size().
    list.compact();
    EXPECT_EQ(list.capacity(), list.size());
    expectSequence(list, { 10, 20, 30, 40, 50 });
}

TEST(DoubleLinkedListTests, SelfAliasingLifecycleAndImmovableSupport)
{
    gp::DoubleLinkedList<gp::String> strList;
    strList.reserve(2u);
    const gp::DoubleLinkedList<gp::String>::NodeHandle hFirst =
        strList.pushBack(gp::String("LongStringValueExceedingSso22Bytes"));
    (void)strList.pushBack(gp::String("SecondLongStringValueExceedingSso22"));
    ASSERT_EQ(strList.size(), strList.capacity());

    // Self-aliasing across growth must not dangle.
    const gp::DoubleLinkedList<gp::String>::NodeHandle hCopy = strList.pushBack(strList.front());
    EXPECT_EQ(strList[hCopy], strList[hFirst]);

    DllTracker::reset();
    {
        gp::DoubleLinkedList<DllTracker> tracked;
        const gp::DoubleLinkedList<DllTracker>::NodeHandle h1 = tracked.emplaceBack(1);
        const gp::DoubleLinkedList<DllTracker>::NodeHandle h2 = tracked.emplaceBack(2);
        const gp::DoubleLinkedList<DllTracker>::NodeHandle h3 = tracked.emplaceBack(3);
        EXPECT_TRUE(tracked.remove(h2));
        // Grow after leaving a free-list hole: hole must NOT invoke move/destruct!
        tracked.reserve(16u);
        EXPECT_TRUE(tracked.isValidHandle(h1));
        EXPECT_TRUE(tracked.isValidHandle(h3));
        EXPECT_FALSE(tracked.isValidHandle(h2));
    }
    EXPECT_EQ(
        DllTracker::s_constructCount + DllTracker::s_copyCount + DllTracker::s_moveCount, DllTracker::s_destructCount
    );

    gp::DoubleLinkedList<DllImmovable> immovable;
    immovable.reserve(4u);
    const gp::DoubleLinkedList<DllImmovable>::NodeHandle ih1 = immovable.emplaceBack(10);
    const gp::DoubleLinkedList<DllImmovable>::NodeHandle ih2 = immovable.emplaceFront(5);
    EXPECT_EQ(immovable[ih2].value, 5);
    EXPECT_EQ(immovable[ih1].value, 10);
}

TEST(DoubleLinkedListTests, AllocatorsSwapComparisonsAndHashing)
{
    gp::InlineDoubleLinkedList<gp::Int32, 256u> inlineList;
    (void)inlineList.pushBack(1);
    (void)inlineList.pushBack(2);
    (void)inlineList.pushFront(0);
    EXPECT_TRUE(inlineList.getAllocator().isInlinePointer(&inlineList.front()));

    gp::InlineDoubleLinkedList<gp::Int32, 256u> inlineCopy(inlineList);
    EXPECT_EQ(inlineCopy, inlineList);
    EXPECT_NE(&inlineCopy.front(), &inlineList.front());

    alignas(16) gp::UInt8 bufA[1024]{};
    gp::memory::LinearAllocator arenaA(bufA, sizeof(bufA), "DllArenaA");
    alignas(16) gp::UInt8 bufB[1024]{};
    gp::memory::LinearAllocator arenaB(bufB, sizeof(bufB), "DllArenaB");

    {
        gp::PolymorphicDoubleLinkedList<gp::Int32> listA{ gp::memory::PolymorphicAllocator(arenaA) };
        gp::PolymorphicDoubleLinkedList<gp::Int32> listB{ gp::memory::PolymorphicAllocator(arenaB) };
        (void)listA.pushBack(1);
        (void)listA.pushBack(2);
        (void)listB.pushBack(10);

        gp::swap(listA, listB);
        const gp::DoubleLinkedList<gp::Int32> expectedA{ 10 };
        const gp::DoubleLinkedList<gp::Int32> expectedB{ 1, 2 };
        EXPECT_EQ(listA, expectedA);
        EXPECT_EQ(listB, expectedB);
        EXPECT_TRUE(arenaA.owns(&listA.front()));
        EXPECT_TRUE(arenaB.owns(&listB.front()));
    }

    gp::memory::LinearAllocator& tlsArena = gp::memory::ThreadScratchArena::get();
    const gp::USize baseline = tlsArena.getUsedBytes();
    {
        const gp::memory::ScopedScratchMark mark;
        gp::ScratchDoubleLinkedList<gp::Int32> scratchList;
        for (gp::Int32 i = 0; i < 8; ++i)
        {
            (void)scratchList.pushBack(i);
        }
        EXPECT_GT(tlsArena.getUsedBytes(), baseline);
    }
    EXPECT_EQ(tlsArena.getUsedBytes(), baseline);

    // Fragmented vs dense lists with identical logical sequences compare and hash identically.
    gp::DoubleLinkedList<gp::Int32> dense{ 1, 2, 3 };
    gp::DoubleLinkedList<gp::Int32> fragmented;
    const gp::DoubleLinkedList<gp::Int32>::NodeHandle hTmp = fragmented.pushBack(99);
    (void)fragmented.pushBack(1);
    (void)fragmented.pushBack(2);
    (void)fragmented.pushBack(3);
    EXPECT_TRUE(fragmented.remove(hTmp));

    EXPECT_EQ(dense, fragmented);
    EXPECT_EQ(gp::hashing::hash(dense), gp::hashing::hash(fragmented));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::XxHash64>(dense), gp::hashing::hash<gp::hashing::XxHash64>(fragmented));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::Crc32>(dense), gp::hashing::hash<gp::hashing::Crc32>(fragmented));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::Sha256>(dense), gp::hashing::hash<gp::hashing::Sha256>(fragmented));

    const gp::DoubleLinkedList<gp::Int32> greater{ 1, 2, 4 };
    EXPECT_LT(dense, greater);
}

TEST(DoubleLinkedListTests, AdditionalApiAndConstOverloadCoverage)
{
    alignas(16) gp::UInt8 arenaBuf[2048]{};
    gp::memory::LinearAllocator arena(arenaBuf, sizeof(arenaBuf), "DllApiArena");
    const gp::memory::PolymorphicAllocator alloc(arena);

    gp::PolymorphicDoubleLinkedList<gp::Int32> emptyAlloc{ alloc };
    gp::PolymorphicDoubleLinkedList<gp::Int32> movedAlloc{ gp::memory::PolymorphicAllocator(arena) };
    EXPECT_TRUE(emptyAlloc.isEmpty());
    EXPECT_TRUE(movedAlloc.isEmpty());

    gp::PolymorphicDoubleLinkedList<gp::Int32> countAlloc(3u, 55, alloc);
    expectSequence(countAlloc, { 55, 55, 55 });

    const gp::Int32 raw[3] = { 30, 10, 20 };
    gp::PolymorphicDoubleLinkedList<gp::Int32> rangeAlloc(raw, raw + 3, alloc);
    expectSequence(rangeAlloc, { 30, 10, 20 });

    gp::PolymorphicDoubleLinkedList<gp::Int32> initAlloc({ 4, 2, 3 }, alloc);
    expectSequence(initAlloc, { 4, 2, 3 });

    const gp::DoubleLinkedList<gp::Int32> crossCopied(initAlloc);
    expectSequence(crossCopied, { 4, 2, 3 });

    rangeAlloc.sort();
    expectSequence(rangeAlloc, { 10, 20, 30 });

    rangeAlloc.swap(initAlloc);
    expectSequence(rangeAlloc, { 4, 2, 3 });
    expectSequence(initAlloc, { 10, 20, 30 });
    initAlloc.swap(initAlloc);

    gp::DoubleLinkedList<gp::Int32> list{ 20, 40 };
    const gp::DoubleLinkedList<gp::Int32>::NodeHandle h20 = list.headHandle();
    const gp::DoubleLinkedList<gp::Int32>::NodeHandle h40 = list.tailHandle();
    const gp::DoubleLinkedList<gp::Int32>::NodeHandle h10 = list.emplaceBefore(h20, 10);
    const gp::DoubleLinkedList<gp::Int32>::Iterator it30 = list.emplace(list.find(40), 30);
    (void)list.insert(list.cend(), 50);
    expectSequence(list, { 10, 20, 30, 40, 50 });

    EXPECT_EQ(list.findHandle(30), it30.handle());
    EXPECT_EQ(list.findHandle(10), h10);
    EXPECT_FALSE(list.findHandle(999).isValid());

    *list.rbegin() = 55;
    EXPECT_NE(list.rbegin(), list.rend());

    const gp::DoubleLinkedList<gp::Int32>& constRef = list;
    EXPECT_EQ(constRef[h40], 40);
    EXPECT_EQ(constRef.get(h40), 40);
    ASSERT_NE(constRef.tryGet(h40), nullptr);
    EXPECT_EQ(*constRef.tryGet(h40), 40);
    EXPECT_EQ(constRef.front(), 10);
    EXPECT_EQ(constRef.back(), 55);
    EXPECT_EQ(*constRef.begin(), 10);
    EXPECT_NE(constRef.begin(), constRef.end());
    EXPECT_EQ(*constRef.rbegin(), 55);
    EXPECT_NE(constRef.rbegin(), constRef.rend());
    (void)constRef.getAllocator();

    list.reset();
    EXPECT_TRUE(list.isEmpty());
    EXPECT_EQ(list.capacity(), 0u);
    EXPECT_FALSE(list.isValidHandle(h40));
}

TEST(DoubleLinkedListTests, AbaCompactSelfAliasRemoveAllMoveOnlyAlignedAndEdgeCases)
{
    // Regression test: compact() relocating a lower-generation node into a previously freed high-gen slot (slot 0)
    // must keep stale handles addressing slot 0 invalidated (zero ABA collision).
    gp::DoubleLinkedList<gp::Int32> compactList;
    const gp::DoubleLinkedList<gp::Int32>::NodeHandle hA = compactList.pushBack(10);   // {index=0, gen=1}
    EXPECT_TRUE(compactList.remove(hA));                                               // slot 0 gen -> 2
    const gp::DoubleLinkedList<gp::Int32>::NodeHandle hB = compactList.pushBack(20);   // {index=0, gen=3}
    const gp::DoubleLinkedList<gp::Int32>::NodeHandle hC = compactList.pushBack(30);   // {index=1, gen=1}
    ASSERT_EQ(hB.index, 0u);
    ASSERT_EQ(hB.generation, 3u);
    EXPECT_TRUE(compactList.remove(hB));   // slot 0 gen -> 4, stale handle hB still has gen=3

    compactList.compact();                 // Relocates hC (30) into slot 0
    EXPECT_EQ(compactList.size(), 1u);
    EXPECT_EQ(compactList.front(), 30);
    EXPECT_FALSE(compactList.isValidHandle(hB));
    EXPECT_FALSE(compactList.isValidHandle(hC));
    EXPECT_EQ(compactList.headHandle().index, 0u);
    EXPECT_GT(compactList.headHandle().generation, hB.generation);

    // Self-aliasing removeAll(front()) with long heap-allocated gp::String (copyable staging path)
    gp::DoubleLinkedList<gp::String> strList;
    (void)strList.pushBack(gp::String("SharedLongStringPayloadExceedingSso22Bytes"));
    (void)strList.pushBack(gp::String("UniqueMiddlePayloadExceedingSso22Bytes"));
    (void)strList.pushBack(gp::String("SharedLongStringPayloadExceedingSso22Bytes"));
    (void)strList.pushBack(gp::String("UniqueSecondPayloadExceedingSso22Bytes"));
    (void)strList.pushBack(gp::String("SharedLongStringPayloadExceedingSso22Bytes"));
    EXPECT_EQ(strList.removeAll(strList.front()), 3u);
    EXPECT_EQ(strList.size(), 2u);
    EXPECT_EQ(strList.front(), "UniqueMiddlePayloadExceedingSso22Bytes");
    EXPECT_EQ(strList.back(), "UniqueSecondPayloadExceedingSso22Bytes");

    // Self-aliasing removeAll(front()) on move-only elements (deferred slot release path)
    gp::DoubleLinkedList<DllMoveOnlyKey> moveKeyList;
    (void)moveKeyList.emplaceBack(7);
    (void)moveKeyList.emplaceBack(2);
    (void)moveKeyList.emplaceBack(7);
    (void)moveKeyList.emplaceBack(9);
    (void)moveKeyList.emplaceBack(7);
    EXPECT_EQ(moveKeyList.removeAll(moveKeyList.front()), 3u);
    ASSERT_EQ(moveKeyList.size(), 2u);
    EXPECT_EQ(moveKeyList.front().key, 2);
    EXPECT_EQ(moveKeyList.back().key, 9);

    // Move-only UniquePtr end-to-end
    {
        gp::DoubleLinkedList<gp::UniquePtr<gp::Int32>> ptrList;
        const gp::DoubleLinkedList<gp::UniquePtr<gp::Int32>>::NodeHandle hPtr2 =
            ptrList.pushBack(gp::makeUnique<gp::Int32>(20));
        (void)ptrList.insertBefore(hPtr2, gp::makeUnique<gp::Int32>(10));
        (void)ptrList.insertAfter(hPtr2, gp::makeUnique<gp::Int32>(30));
        EXPECT_TRUE(ptrList.moveToBack(ptrList.headHandle()));
        ptrList.reverse();
        EXPECT_EQ(*ptrList.front(), 10);
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

    // 0-element and 1-element edge cases + findIf + mixed iterators + comparisons + self-assignment
    gp::DoubleLinkedList<gp::Int32> emptyList;
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

    gp::DoubleLinkedList<gp::Int32> singleList;
    const gp::DoubleLinkedList<gp::Int32>::NodeHandle hOnly = singleList.pushBack(99);
    singleList.reverse();
    singleList.sort();
    EXPECT_EQ(singleList.headHandle(), hOnly);
    EXPECT_EQ(singleList.tailHandle(), hOnly);
    EXPECT_TRUE(singleList.moveToFront(hOnly));
    EXPECT_TRUE(singleList.moveToBack(hOnly));
    EXPECT_EQ(
        *singleList.findIf(
            [](const gp::Int32 v) noexcept -> bool
    {
        return v == 99;
    }
        ),
        99
    );
    EXPECT_EQ(
        std::as_const(singleList)
            .findIf(
                [](const gp::Int32 v) noexcept -> bool
    {
        return v == 123;
    }
            ),
        singleList.cend()
    );
    EXPECT_EQ(singleList.begin(), singleList.cbegin());
    EXPECT_TRUE(singleList.remove(hOnly));
    EXPECT_TRUE(singleList.isEmpty());
    EXPECT_FALSE(singleList.headHandle().isValid());
    EXPECT_FALSE(singleList.tailHandle().isValid());

    gp::DoubleLinkedList<gp::Int32> cmpA{ 1, 2 };
    gp::DoubleLinkedList<gp::Int32>& aliasA = cmpA;
    cmpA = aliasA;
    cmpA = std::move(aliasA);
    EXPECT_NE(cmpA, emptyList);
    EXPECT_GT(cmpA, emptyList);
    EXPECT_LE(emptyList, cmpA);
    EXPECT_GE(cmpA, cmpA);
    EXPECT_LE(cmpA, cmpA);

    // Over-aligned 64-byte node alignment check
    gp::DoubleLinkedList<DllAligned64Payload> alignedList;
    (void)alignedList.emplaceBack();
    (void)alignedList.emplaceBack();
    EXPECT_EQ(reinterpret_cast<gp::USize>(&alignedList.front()) % 64u, 0u);
    EXPECT_EQ(reinterpret_cast<gp::USize>(&alignedList.back()) % 64u, 0u);
}

TEST(DoubleLinkedListTests, InvariantsMultiCardinalitySortInlineSwapAndNestedRelocation)
{
    // Sort across tricky cardinalities (2, 3, 5, 9, 16) after free-list fragmentation
    const gp::USize counts[5] = { 2u, 3u, 5u, 9u, 16u };
    for (gp::USize idx = 0u; idx < 5u; ++idx)
    {
        const gp::USize count = counts[idx];
        gp::DoubleLinkedList<gp::Int32> list;
        const gp::DoubleLinkedList<gp::Int32>::NodeHandle dummy = list.pushBack(-999);
        for (gp::USize i = 0u; i < count; ++i)
        {
            const gp::Int32 val = static_cast<gp::Int32>((i * 7u + 3u) % 13u);
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
        verifyDoubleListInvariants(list);

        list.sort();
        verifyDoubleListInvariants(list);

        gp::DoubleLinkedList<gp::Int32>::ConstIterator it = list.cbegin();
        gp::Int32 prevVal = *it;
        ++it;
        for (; it != list.cend(); ++it)
        {
            EXPECT_LE(prevVal, *it);
            prevVal = *it;
        }

        list.reverse();
        verifyDoubleListInvariants(list);
    }

    // InlineDoubleLinkedList swap & occupied move assignment stay inside inline buffer
    gp::InlineDoubleLinkedList<gp::Int32, 256u> inlineA{ 1, 2, 3 };
    gp::InlineDoubleLinkedList<gp::Int32, 256u> inlineB{ 10, 20, 30, 40 };
    EXPECT_TRUE(inlineA.getAllocator().isInlinePointer(&inlineA.front()));
    EXPECT_TRUE(inlineB.getAllocator().isInlinePointer(&inlineB.front()));
    gp::swap(inlineA, inlineB);
    EXPECT_TRUE(inlineA.getAllocator().isInlinePointer(&inlineA.front()));
    EXPECT_TRUE(inlineB.getAllocator().isInlinePointer(&inlineB.front()));
    expectSequence(inlineA, { 10, 20, 30, 40 });
    expectSequence(inlineB, { 1, 2, 3 });
    inlineB = std::move(inlineA);
    EXPECT_TRUE(inlineB.getAllocator().isInlinePointer(&inlineB.front()));
    verifyDoubleListInvariants(inlineB);

    // Move-only UniquePtr tryPopFront, tryPopBack, and erase(pos)
    gp::DoubleLinkedList<gp::UniquePtr<gp::Int32>> moveList;
    (void)moveList.pushBack(gp::makeUnique<gp::Int32>(1));
    (void)moveList.pushBack(gp::makeUnique<gp::Int32>(2));
    (void)moveList.pushBack(gp::makeUnique<gp::Int32>(3));
    gp::DoubleLinkedList<gp::UniquePtr<gp::Int32>>::Iterator middle = moveList.begin();
    ++middle;
    const gp::DoubleLinkedList<gp::UniquePtr<gp::Int32>>::Iterator afterMid = moveList.erase(middle);
    EXPECT_EQ(**afterMid, 3);
    gp::UniquePtr<gp::Int32> popped;
    EXPECT_TRUE(moveList.tryPopFront(popped));
    EXPECT_EQ(*popped, 1);
    EXPECT_TRUE(moveList.tryPopBack(popped));
    EXPECT_EQ(*popped, 3);
    EXPECT_FALSE(moveList.tryPopBack(popped));
    verifyDoubleListInvariants(moveList);

    // ImmutableRecord (const field, non-assignable) full handle & algorithm compatibility
    gp::DoubleLinkedList<DllImmutableRecord> records;
    const gp::DoubleLinkedList<DllImmutableRecord>::NodeHandle r2 = records.emplaceBack(2);
    const gp::DoubleLinkedList<DllImmutableRecord>::NodeHandle r1 = records.emplaceFront(1);
    (void)records.emplaceAfter(r2, 3);
    (void)records.emplaceBefore(r1, 0);
    records.reverse();
    records.sort(
        [](const DllImmutableRecord& lhs, const DllImmutableRecord& rhs) noexcept -> bool
    {
        return lhs.id < rhs.id;
    }
    );
    records.compact();
    EXPECT_EQ(records.front().id, 0);
    EXPECT_EQ(records.back().id, 3);
    verifyDoubleListInvariants(records);

    // Single-pass non-forward InputIterator range ctor & assign()
    static_assert(!std::forward_iterator<DllSinglePassIter>);
    const gp::DoubleLinkedList<gp::Int32> genCtor(DllSinglePassIter(5), DllSinglePassIter(9));
    expectSequence(genCtor, { 5, 6, 7, 8 });
    gp::DoubleLinkedList<gp::Int32> genAssign;
    genAssign.assign(DllSinglePassIter(1), DllSinglePassIter(4));
    expectSequence(genAssign, { 1, 2, 3 });

    // Nested relocatable DoubleLinkedList<DoubleLinkedList<Int32>> across slot-pool growth
    gp::DoubleLinkedList<gp::DoubleLinkedList<gp::Int32>> nested;
    for (gp::Int32 i = 0; i < 10; ++i)
    {
        gp::DoubleLinkedList<gp::Int32> child{ i, i + 100 };
        (void)nested.pushBack(std::move(child));
    }
    EXPECT_EQ(nested.size(), 10u);
    EXPECT_EQ(nested.back().back(), 109);

    // Handle tag type-safety across element types and list families
    static_assert(
        !std::is_convertible_v<gp::DoubleLinkedListNodeHandle<gp::Int32>, gp::LinkedListNodeHandle<gp::Int32>>
    );
    static_assert(
        !std::is_convertible_v<gp::DoubleLinkedListNodeHandle<gp::Int32>, gp::DoubleLinkedListNodeHandle<gp::Float32>>
    );

    // Nested hash boundary framing prevents prefix collisions across inner lists
    gp::DoubleLinkedList<gp::DoubleLinkedList<gp::Int32>> nestedA;
    (void)nestedA.pushBack(gp::DoubleLinkedList<gp::Int32>{ 1, 2 });
    (void)nestedA.pushBack(gp::DoubleLinkedList<gp::Int32>{ 3 });
    gp::DoubleLinkedList<gp::DoubleLinkedList<gp::Int32>> nestedB;
    (void)nestedB.pushBack(gp::DoubleLinkedList<gp::Int32>{ 1 });
    (void)nestedB.pushBack(gp::DoubleLinkedList<gp::Int32>{ 2, 3 });
    EXPECT_NE(gp::hashing::hash(nestedA), gp::hashing::hash(nestedB));
    EXPECT_NE(
        gp::hashing::hash(gp::DoubleLinkedList<gp::Int32>{}), gp::hashing::hash(gp::DoubleLinkedList<gp::Int32>{ 0 })
    );

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

    gp::DoubleLinkedList<UncomparablePair> uncompList;
    const gp::DoubleLinkedList<UncomparablePair>::NodeHandle hTail = uncompList.emplaceBack(2, 200);
    const gp::DoubleLinkedList<UncomparablePair>::NodeHandle hHead = uncompList.emplaceFront(0, 10);
    (void)uncompList.emplaceAfter(hHead, 1, 100);
    (void)uncompList.emplaceBefore(hTail, 3, 300);
    EXPECT_EQ(uncompList.front().tag, 0);
    EXPECT_EQ(uncompList.back().weight, 200);
    EXPECT_EQ(uncompList.size(), 4u);
}

}   // namespace gp::tests
