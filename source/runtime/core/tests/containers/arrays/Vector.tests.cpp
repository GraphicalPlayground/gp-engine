// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/arrays/Vector.hpp"           // IWYU pragma: keep
#include "containers/strings/String.hpp"          // IWYU pragma: keep
#include "memory/allocators/LinearAllocator.hpp"
#include "memory/allocators/StackAllocator.hpp"   // IWYU pragma: keep
#include "memory/allocators/TaggedAllocator.hpp"
#include "memory/allocators/ThreadScratchAllocator.hpp"
#include "memory/pointers/UniquePtr.hpp"
#include <gtest/gtest.h>

namespace gp::tests
{

namespace
{

/// @brief Tracks constructor, copy, move, and destructor invocations to verify container RAII invariants.
struct LifecycleTracker
{
    static inline gp::Int32 s_constructCount{ 0 };
    static inline gp::Int32 s_copyCount{ 0 };
    static inline gp::Int32 s_moveCount{ 0 };
    static inline gp::Int32 s_destructCount{ 0 };

    static void resetCounts() noexcept
    {
        s_constructCount = 0;
        s_copyCount = 0;
        s_moveCount = 0;
        s_destructCount = 0;
    }

    gp::Int32 id{ 777 };

    LifecycleTracker() noexcept
        : id(777)
    {
        ++s_constructCount;
    }

    explicit LifecycleTracker(const gp::Int32 inId) noexcept
        : id(inId)
    {
        ++s_constructCount;
    }

    LifecycleTracker(const LifecycleTracker& other) noexcept
        : id(other.id)
    {
        ++s_constructCount;
        ++s_copyCount;
    }

    LifecycleTracker(LifecycleTracker&& other) noexcept
        : id(other.id)
    {
        ++s_constructCount;
        ++s_moveCount;
        other.id = -1;
    }

    LifecycleTracker& operator=(const LifecycleTracker& other) noexcept
    {
        id = other.id;
        ++s_copyCount;
        return *this;
    }

    LifecycleTracker& operator=(LifecycleTracker&& other) noexcept
    {
        id = other.id;
        other.id = -1;
        ++s_moveCount;
        return *this;
    }

    ~LifecycleTracker() noexcept
    {
        ++s_destructCount;
    }

    [[nodiscard]] bool operator==(const LifecycleTracker& other) const noexcept
    {
        return id == other.id;
    }

    [[nodiscard]] auto operator<=>(const LifecycleTracker& other) const noexcept = default;
};

/// @brief Compile-time verification of `gp::Vector` operations in `constexpr` contexts.
[[nodiscard]] constexpr bool testConstexprVectorOperations() noexcept
{
    gp::Vector<gp::Int32> vec;
    if (!vec.isEmpty() || vec.size() != 0u || vec.capacity() != 0u)
    {
        return false;
    }

    vec.pushBack(30);
    vec.pushBack(10);
    vec.emplaceBack(20);
    vec.insert(vec.begin() + 1u, 15);

    if (vec.size() != 4u || vec[0] != 30 || vec[1] != 15 || vec[2] != 10 || vec[3] != 20)
    {
        return false;
    }

    vec.sort();
    if (vec[0] != 10 || vec[1] != 15 || vec[2] != 20 || vec[3] != 30)
    {
        return false;
    }

    vec.reverse();
    if (vec.front() != 30 || vec.back() != 10 || vec.last(1u) != 15)
    {
        return false;
    }

    vec.eraseSwap(vec.begin());
    if (vec.size() != 3u || vec[0] != 10)
    {
        return false;
    }

    const gp::Int32 popped = vec.popBackValue();
    return popped == 15 && vec.size() == 2u && vec.contains(20);
}

static_assert(testConstexprVectorOperations(), "Constexpr gp::Vector evaluation failed.");

}   // namespace

template <typename InT>
class VectorTypedTest : public ::testing::Test
{};

using NumericTypes = ::testing::Types<gp::Int32, gp::UInt64, gp::Float32, gp::Float64>;
TYPED_TEST_SUITE(VectorTypedTest, NumericTypes);

TYPED_TEST(VectorTypedTest, BasicConstructionAccessAndIteration)
{
    using Elem = TypeParam;
    gp::Vector<Elem> emptyVec;
    EXPECT_TRUE(emptyVec.isEmpty());
    EXPECT_EQ(emptyVec.size(), 0u);
    EXPECT_EQ(emptyVec.capacity(), 0u);
    EXPECT_EQ(emptyVec.data(), nullptr);

    gp::Vector<Elem> countVec(4u, static_cast<Elem>(5));
    ASSERT_EQ(countVec.size(), 4u);
    for (gp::USize i = 0u; i < countVec.size(); ++i)
    {
        EXPECT_EQ(countVec[i], static_cast<Elem>(5));
        EXPECT_EQ(countVec.at(i), static_cast<Elem>(5));
    }

    gp::Vector<Elem> listVec{ static_cast<Elem>(1), static_cast<Elem>(2), static_cast<Elem>(3), static_cast<Elem>(4) };
    ASSERT_EQ(listVec.size(), 4u);
    EXPECT_EQ(listVec.front(), static_cast<Elem>(1));
    EXPECT_EQ(listVec.back(), static_cast<Elem>(4));
    EXPECT_EQ(listVec.last(0u), static_cast<Elem>(4));
    EXPECT_EQ(listVec.last(2u), static_cast<Elem>(2));
    EXPECT_TRUE(listVec.isValidIndex(3u));
    EXPECT_FALSE(listVec.isValidIndex(4u));

    Elem sum = static_cast<Elem>(0);
    for (const Elem val: listVec)
    {
        sum += val;
    }
    EXPECT_EQ(sum, static_cast<Elem>(10));

    gp::Vector<Elem> reversed;
    for (auto it = listVec.rbegin(); it != listVec.rend(); ++it)
    {
        reversed.pushBack(*it);
    }
    ASSERT_EQ(reversed.size(), 4u);
    EXPECT_EQ(reversed[0], static_cast<Elem>(4));
    EXPECT_EQ(reversed[3], static_cast<Elem>(1));
}

TEST(VectorTests, FootprintAndConceptInvariants)
{
    static_assert(sizeof(gp::Vector<gp::Int32>) == 24u);
    static_assert(sizeof(gp::Vector64<gp::Int32>) == 24u);
    static_assert(sizeof(gp::ScratchVector<gp::Int32>) == 24u);
    static_assert(sizeof(gp::AlignedVector<gp::Int32, 64u>) == 24u);
    static_assert(sizeof(gp::PolymorphicVector<gp::Int32>) == 32u);

    static_assert(gp::concepts::IsVector<gp::Vector<gp::Int32>>);
    static_assert(gp::concepts::IsVector<const gp::Vector<gp::Int32>&>);
    static_assert(gp::concepts::IsVector<gp::InlineVector<gp::Int32, 8u>>);
    static_assert(gp::concepts::IsVector<gp::FixedVector<gp::Int32, 8u>>);
    static_assert(gp::concepts::IsVector<gp::ScratchVector<gp::Int32>>);
    static_assert(gp::concepts::IsVector<gp::PolymorphicVector<gp::Int32>>);
    static_assert(gp::concepts::IsVector<gp::AlignedVector<gp::Int32, 32u>>);
    static_assert(!gp::concepts::IsVector<gp::Array<gp::Int32, 4u>>);
    static_assert(!gp::concepts::IsVector<gp::String>);

    static_assert(gp::concepts::IsRelocatable<gp::Vector<gp::Int32>>);
    static_assert(gp::concepts::IsRelocatable<gp::ScratchVector<gp::Int32>>);
    static_assert(gp::concepts::IsRelocatable<gp::AlignedVector<gp::Int32, 32u>>);
    static_assert(gp::concepts::IsRelocatable<gp::String>);
    static_assert(!gp::concepts::IsRelocatable<gp::InlineVector<gp::Int32, 8u>>);
    static_assert(!gp::concepts::IsRelocatable<gp::FixedVector<gp::Int32, 8u>>);
    static_assert(gp::concepts::IsRelocatable<gp::PolymorphicVector<gp::Int32>>);
    static_assert(!gp::concepts::IsRelocatable<LifecycleTracker>);
}

TEST(VectorTests, ConstructorsAndAssignments)
{
    const gp::Int32 raw[4] = { 10, 20, 30, 40 };
    gp::Vector<gp::Int32> fromPtr(raw, 4u);
    EXPECT_EQ(fromPtr.size(), 4u);
    EXPECT_EQ(fromPtr[2], 30);

    gp::Vector<gp::Int32> fromIter(fromPtr.cbegin() + 1u, fromPtr.cend());
    ASSERT_EQ(fromIter.size(), 3u);
    EXPECT_EQ(fromIter[0], 20);
    EXPECT_EQ(fromIter[2], 40);

    const gp::Array<gp::Int32, 3u> fixedArr{ 7, 8, 9 };
    gp::Vector<gp::Int32> fromArray(fixedArr);
    ASSERT_EQ(fromArray.size(), 3u);
    EXPECT_EQ(fromArray[0], 7);
    EXPECT_EQ(fromArray[2], 9);

    gp::Vector<gp::Int32> copyVec(fromArray);
    EXPECT_EQ(copyVec, fromArray);
    EXPECT_NE(copyVec.data(), fromArray.data());

    const gp::Int32* const expectedPtr = copyVec.data();
    gp::Vector<gp::Int32> movedVec(std::move(copyVec));
    EXPECT_EQ(movedVec.data(), expectedPtr);
    EXPECT_EQ(movedVec.size(), 3u);
    EXPECT_TRUE(copyVec.isEmpty());
    EXPECT_EQ(copyVec.data(), nullptr);

    copyVec = { 100, 200 };
    ASSERT_EQ(copyVec.size(), 2u);
    EXPECT_EQ(copyVec[1], 200);

    copyVec.assign(3u, 42);
    ASSERT_EQ(copyVec.size(), 3u);
    EXPECT_EQ(copyVec[0], 42);
    EXPECT_EQ(copyVec[2], 42);
}

TEST(VectorTests, LifecycleTrackingMoveAndDestructionParity)
{
    LifecycleTracker::resetCounts();
    {
        gp::Vector<LifecycleTracker> vec;
        vec.reserve(2u);
        EXPECT_EQ(LifecycleTracker::s_constructCount, 0);

        vec.emplaceBack(1);
        vec.emplaceBack(2);
        EXPECT_EQ(LifecycleTracker::s_constructCount, 2);
        EXPECT_EQ(LifecycleTracker::s_copyCount, 0);
        EXPECT_EQ(LifecycleTracker::s_moveCount, 0);

        // Trigger reallocation past capacity 2: should move-relocate old elements with zero copies
        vec.emplaceBack(3);
        EXPECT_EQ(LifecycleTracker::s_copyCount, 0);
        EXPECT_GT(LifecycleTracker::s_moveCount, 0);

        const gp::Int32 movesBeforeContainerMove = LifecycleTracker::s_moveCount;
        gp::Vector<LifecycleTracker> movedVec(std::move(vec));
        // Moving the container steals m_data in O(1): zero element moves and zero copies
        EXPECT_EQ(LifecycleTracker::s_copyCount, 0);
        EXPECT_EQ(LifecycleTracker::s_moveCount, movesBeforeContainerMove);
        EXPECT_EQ(movedVec.size(), 3u);
        EXPECT_TRUE(vec.isEmpty());
    }
    EXPECT_EQ(LifecycleTracker::s_constructCount, LifecycleTracker::s_destructCount);
}

TEST(VectorTests, MoveOnlyElementsWithUniquePtr)
{
    gp::Vector<gp::UniquePtr<gp::Int32>> ptrs;
    ptrs.pushBack(gp::makeUnique<gp::Int32>(10));
    ptrs.pushBack(gp::makeUnique<gp::Int32>(20));
    ptrs.pushBack(gp::makeUnique<gp::Int32>(30));

    ASSERT_EQ(ptrs.size(), 3u);
    EXPECT_EQ(*ptrs[0], 10);
    EXPECT_EQ(*ptrs[1], 20);
    EXPECT_EQ(*ptrs[2], 30);

    gp::UniquePtr<gp::Int32> popped = ptrs.popBackValue();
    ASSERT_NE(popped.get(), nullptr);
    EXPECT_EQ(*popped, 30);
    EXPECT_EQ(ptrs.size(), 2u);

    ptrs.removeAtSwap(0u);
    ASSERT_EQ(ptrs.size(), 1u);
    EXPECT_EQ(*ptrs[0], 20);
}

TEST(VectorTests, RelocatableContainersVectorOfStringsAndVectors)
{
    gp::Vector<gp::String> strings;
    for (gp::Int32 i = 0; i < 32; ++i)
    {
        if ((i % 2) == 0)
        {
            strings.emplaceBack("ShortSSO");
        }
        else
        {
            strings.emplaceBack("This string exceeds twenty-two characters to test heap relocation!");
        }
    }

    ASSERT_EQ(strings.size(), 32u);
    for (gp::USize i = 0u; i < strings.size(); ++i)
    {
        if ((i % 2u) == 0u)
        {
            EXPECT_EQ(strings[i], "ShortSSO");
            EXPECT_TRUE(strings[i].isSso());
        }
        else
        {
            EXPECT_EQ(strings[i], "This string exceeds twenty-two characters to test heap relocation!");
            EXPECT_TRUE(strings[i].isLong());
        }
    }

    strings.removeAtSwap(0u, 4u);
    EXPECT_EQ(strings.size(), 28u);

    gp::Vector<gp::Vector<gp::Int32>> nested;
    for (gp::Int32 i = 0; i < 16; ++i)
    {
        nested.pushBack({ i, i * 10, i * 100 });
    }
    ASSERT_EQ(nested.size(), 16u);
    EXPECT_EQ(nested[15][2], 1500);
}

TEST(VectorTests, ResizeModesValueDefaultedUninitializedAndZeroed)
{
    gp::Vector<gp::Int32> vec;
    vec.resize(4u);
    ASSERT_EQ(vec.size(), 4u);
    for (gp::USize i = 0u; i < 4u; ++i)
    {
        EXPECT_EQ(vec[i], 0);
    }

    vec.resize(6u, 99);
    ASSERT_EQ(vec.size(), 6u);
    EXPECT_EQ(vec[4], 99);
    EXPECT_EQ(vec[5], 99);

    vec.resizeUninitialized(10u);
    EXPECT_EQ(vec.size(), 10u);
    for (gp::USize i = 6u; i < 10u; ++i)
    {
        vec[i] = static_cast<gp::Int32>(i * 10u);
    }
    EXPECT_EQ(vec[9], 90);

    vec.resizeZeroed(14u);
    ASSERT_EQ(vec.size(), 14u);
    for (gp::USize i = 10u; i < 14u; ++i)
    {
        EXPECT_EQ(vec[i], 0);
    }

    // Verify resizeDefaulted invokes default constructor on non-trivial types
    LifecycleTracker::resetCounts();
    {
        gp::Vector<LifecycleTracker> tracked;
        tracked.resizeDefaulted(5u);
        ASSERT_EQ(tracked.size(), 5u);
        for (gp::USize i = 0u; i < 5u; ++i)
        {
            EXPECT_EQ(tracked[i].id, 777);
        }
        tracked.resizeDefaulted(2u);
        EXPECT_EQ(tracked.size(), 2u);
    }
    EXPECT_EQ(LifecycleTracker::s_constructCount, LifecycleTracker::s_destructCount);
}

TEST(VectorTests, AddAndPushBackFastPathsAndUnique)
{
    gp::Vector<gp::Int32> vec;
    gp::Int32& uninitRef = vec.pushBackUninitialized();
    uninitRef = 11;
    gp::Int32& defaultedRef = vec.pushBackDefaulted();
    defaultedRef = 22;
    gp::Int32& zeroedRef = vec.pushBackZeroed();
    EXPECT_EQ(zeroedRef, 0);
    zeroedRef = 33;

    const gp::USize baseIdx = vec.addZeroed(2u);
    EXPECT_EQ(baseIdx, 3u);
    EXPECT_EQ(vec.size(), 5u);
    EXPECT_EQ(vec[3], 0);
    EXPECT_EQ(vec[4], 0);

    // pushBackUnique
    EXPECT_EQ(vec.pushBackUnique(22), 1u);
    EXPECT_EQ(vec.size(), 5u);
    EXPECT_EQ(vec.pushBackUnique(44), 5u);
    EXPECT_EQ(vec.size(), 6u);
    EXPECT_EQ(vec.back(), 44);
}

TEST(VectorTests, SelfAliasingSafetyOnPushBackInsertAppendAndAssign)
{
    gp::Vector<gp::String> vec;
    vec.pushBack("Alpha");
    vec.pushBack("Beta");

    // Force multiple reallocations while pushing an element referencing vec[0]
    for (gp::USize i = 0u; i < 32u; ++i)
    {
        vec.pushBack(vec[0]);
    }
    ASSERT_EQ(vec.size(), 34u);
    for (gp::USize i = 2u; i < vec.size(); ++i)
    {
        EXPECT_EQ(vec[i], "Alpha");
    }

    // Self-aliasing insert from inside the vector
    vec.insert(vec.begin(), vec.back());
    EXPECT_EQ(vec.front(), "Alpha");
    EXPECT_EQ(vec.size(), 35u);

    // Self-aliasing range append
    gp::Vector<gp::Int32> nums{ 1, 2, 3, 4 };
    nums.append(nums.data() + 1u, 2u);
    ASSERT_EQ(nums.size(), 6u);
    EXPECT_EQ(nums[4], 2);
    EXPECT_EQ(nums[5], 3);

    // Self-aliasing range insert
    nums.insert(nums.begin() + 1u, nums.data() + 2u, 2u);
    ASSERT_EQ(nums.size(), 8u);
    EXPECT_EQ(nums[0], 1);
    EXPECT_EQ(nums[1], 3);
    EXPECT_EQ(nums[2], 4);
    EXPECT_EQ(nums[3], 2);

    // Self-aliasing assign from sub-range
    nums.assign(nums.data() + 1u, 3u);
    ASSERT_EQ(nums.size(), 3u);
    EXPECT_EQ(nums[0], 3);
    EXPECT_EQ(nums[1], 4);
    EXPECT_EQ(nums[2], 2);
}

TEST(VectorTests, OrderedEraseAndUnorderedSwapErase)
{
    gp::Vector<gp::Int32> ordered{ 10, 20, 30, 40, 50, 20, 60 };
    EXPECT_TRUE(ordered.removeFirst(20));
    // [10, 30, 40, 50, 20, 60]
    ASSERT_EQ(ordered.size(), 6u);
    EXPECT_EQ(ordered[1], 30);

    ordered.removeAt(1u, 2u);
    // [10, 50, 20, 60]
    ASSERT_EQ(ordered.size(), 4u);
    EXPECT_EQ(ordered[0], 10);
    EXPECT_EQ(ordered[1], 50);
    EXPECT_EQ(ordered[2], 20);
    EXPECT_EQ(ordered[3], 60);

    const gp::USize removedEvens = ordered.removeAllIf(
        [](const gp::Int32 v) -> bool
    {
        return v >= 50;
    }
    );
    EXPECT_EQ(removedEvens, 2u);
    ASSERT_EQ(ordered.size(), 2u);
    EXPECT_EQ(ordered[0], 10);
    EXPECT_EQ(ordered[1], 20);

    // Unordered O(1) swap-erase
    gp::Vector<gp::Int32> unordered{ 1, 2, 3, 4, 5 };
    unordered.removeAtSwap(1u, 1u);
    // Slot 1 now holds former tail element 5: [1, 5, 3, 4]
    ASSERT_EQ(unordered.size(), 4u);
    EXPECT_EQ(unordered[0], 1);
    EXPECT_EQ(unordered[1], 5);
    EXPECT_EQ(unordered[2], 3);
    EXPECT_EQ(unordered[3], 4);

    // Multi-element swap-erase when tail has fewer elements than count
    unordered.removeAtSwap(1u, 2u);
    // Removes [5, 3], moves tail [4] into slot 1: [1, 4]
    ASSERT_EQ(unordered.size(), 2u);
    EXPECT_EQ(unordered[0], 1);
    EXPECT_EQ(unordered[1], 4);

    gp::Vector<gp::Int32> swapPred{ 2, 4, 1, 6, 3, 8, 5 };
    const gp::USize removedSwapCount = swapPred.removeAllIfSwap(
        [](const gp::Int32 v) -> bool
    {
        return (v % 2) == 0;
    }
    );
    EXPECT_EQ(removedSwapCount, 4u);
    ASSERT_EQ(swapPred.size(), 3u);
    EXPECT_TRUE(swapPred.contains(1));
    EXPECT_TRUE(swapPred.contains(3));
    EXPECT_TRUE(swapPred.contains(5));
}

TEST(VectorTests, SearchCountSortAndComparisons)
{
    gp::Vector<gp::Int32> vec{ 5, 2, 8, 2, 9, 1, 2 };
    EXPECT_EQ(vec.count(2), 3u);
    EXPECT_EQ(
        vec.countIf(
            [](const gp::Int32 v) -> bool
    {
        return v > 4;
    }
        ),
        3u
    );
    EXPECT_EQ(vec.indexOf(2), 1u);
    EXPECT_EQ(vec.lastIndexOf(2), 6u);
    EXPECT_EQ(vec.indexOf(999), gp::Vector<gp::Int32>::npos);
    EXPECT_TRUE(vec.contains(8));
    EXPECT_FALSE(vec.contains(42));
    EXPECT_TRUE(vec.containsIf(
        [](const gp::Int32 v) -> bool
    {
        return v == 9;
    }
    ));

    auto foundIt = vec.find(8);
    ASSERT_NE(foundIt, vec.end());
    EXPECT_EQ(foundIt - vec.begin(), 2);

    auto lastIt = vec.findLast(2);
    ASSERT_NE(lastIt, vec.end());
    EXPECT_EQ(lastIt - vec.begin(), 6);

    vec.sort(
        [](const gp::Int32 a, const gp::Int32 b) -> bool
    {
        return a > b;
    }
    );
    EXPECT_EQ(vec.front(), 9);
    EXPECT_EQ(vec.back(), 1);

    gp::InlineVector<gp::Int32, 8u> inlineVec{ 9, 8, 5, 2, 2, 2, 1 };
    EXPECT_EQ(vec, inlineVec);
    inlineVec.back() = 0;
    EXPECT_GT(vec, inlineVec);
}

TEST(VectorTests, ArenaIntegrationLinearInlineFixedScratchAndAligned)
{
    // 1. LinearAllocator top-of-bumper O(1) in-place expansion
    alignas(16) gp::UInt8 rawArena[1024]{};
    gp::memory::LinearAllocator linearArena(rawArena, sizeof(rawArena), "TestVectorLinearArena");
    {
        gp::PolymorphicVector<gp::Int32> arenaVec{ gp::memory::PolymorphicAllocator(&linearArena) };
        arenaVec.reserve(4u);
        const gp::Int32* const initialPtr = arenaVec.data();
        ASSERT_NE(initialPtr, nullptr);

        for (gp::Int32 i = 0; i < 16; ++i)
        {
            arenaVec.pushBack(i * 3);
        }

        // Because arenaVec is the top allocation in linearArena and Int32 is relocatable,
        // reallocate expands in place without moving the base pointer!
        EXPECT_EQ(arenaVec.data(), initialPtr);
        EXPECT_EQ(arenaVec.size(), 16u);
        EXPECT_EQ(arenaVec[15], 45);
    }

    // 2. InlineVector SBO -> Heap Spillover -> ShrinkToFit SBO Demotion
    LifecycleTracker::resetCounts();
    {
        gp::InlineVector<LifecycleTracker, 4u> inlineVec;
        for (gp::Int32 i = 0; i < 4; ++i)
        {
            inlineVec.emplaceBack(i + 10);
        }

        EXPECT_TRUE(inlineVec.getAllocator().isInlinePointer(inlineVec.data()));
        EXPECT_EQ(inlineVec.capacity(), 4u);
        // Zero moves while filling up to the inline capacity of 4!
        EXPECT_EQ(LifecycleTracker::s_moveCount, 0);
        EXPECT_EQ(LifecycleTracker::s_copyCount, 0);

        // Spilling past 4 transitions to the fallback heap
        inlineVec.emplaceBack(50);
        EXPECT_FALSE(inlineVec.getAllocator().isInlinePointer(inlineVec.data()));
        EXPECT_GT(inlineVec.capacity(), 4u);
        EXPECT_EQ(inlineVec.size(), 5u);

        // Pop back to 3 and shrinkToFit: demotes back into the inline buffer!
        inlineVec.popBack();
        inlineVec.popBack();
        inlineVec.shrinkToFit();
        EXPECT_TRUE(inlineVec.getAllocator().isInlinePointer(inlineVec.data()));
        EXPECT_EQ(inlineVec.capacity(), 4u);
        EXPECT_EQ(inlineVec.size(), 3u);
        EXPECT_EQ(inlineVec[0].id, 10);
        EXPECT_EQ(inlineVec[2].id, 12);

        // Moving an inline-resident InlineVector relocates elements with zero copies
        gp::InlineVector<LifecycleTracker, 4u> movedInline(std::move(inlineVec));
        EXPECT_EQ(LifecycleTracker::s_copyCount, 0);
        EXPECT_EQ(movedInline.size(), 3u);
        EXPECT_TRUE(movedInline.getAllocator().isInlinePointer(movedInline.data()));
    }
    EXPECT_EQ(LifecycleTracker::s_constructCount, LifecycleTracker::s_destructCount);

    // 3. FixedVector bounded stack buffer
    gp::FixedVector<gp::Int32, 8u> fixedVec;
    for (gp::Int32 i = 0; i < 8; ++i)
    {
        EXPECT_TRUE(fixedVec.tryPushBack(i + 1));
    }
    EXPECT_TRUE(fixedVec.getAllocator().owns(fixedVec.data()));
    EXPECT_EQ(fixedVec.size(), 8u);
    EXPECT_EQ(fixedVec.capacity(), 8u);
    // Bounded arena is full: tryPushBack and tryReserve return false without asserting
    EXPECT_FALSE(fixedVec.tryPushBack(9));
    EXPECT_FALSE(fixedVec.tryReserve(16u));
    EXPECT_FALSE(fixedVec.tryResize(12u));
    EXPECT_EQ(fixedVec.size(), 8u);

    // 4. ScratchVector with ScopedScratchMark
    {
        gp::memory::ScopedScratchMark scratchScope;
        gp::ScratchVector<gp::Int32> scratchVec;
        for (gp::Int32 i = 0; i < 64; ++i)
        {
            scratchVec.pushBack(i);
        }
        EXPECT_EQ(scratchVec.size(), 64u);
        EXPECT_EQ(scratchVec.back(), 63);
    }

    // 5. AlignedVector (64-byte cache-line alignment)
    gp::AlignedVector<gp::Int32, 64u> alignedVec;
    alignedVec.resize(16u, 7);
    ASSERT_NE(alignedVec.data(), nullptr);
    EXPECT_TRUE(gp::memory::isAligned(alignedVec.data(), 64u));

    // 6. TaggedAllocator telemetry
    gp::memory::TaggedAllocator<gp::memory::HeapAllocator> taggedAlloc("TestVectorPool");
    {
        gp::Vector<gp::UInt64, gp::memory::TaggedAllocator<gp::memory::HeapAllocator>> taggedVec(taggedAlloc);
        taggedVec.reserve(32u);
        EXPECT_EQ(taggedVec.getAllocator().getStats().activeAllocations, 1u);
        EXPECT_GE(taggedVec.getAllocator().getStats().allocatedBytes, 32u * sizeof(gp::UInt64));
        taggedVec.reset();
        EXPECT_EQ(taggedVec.getAllocator().getStats().activeAllocations, 0u);
        EXPECT_EQ(taggedVec.getAllocator().getStats().allocatedBytes, 0u);
    }
}

TEST(VectorTests, SelfCritiqueEnhancementsInsertGapBinarySearchDeduplicateAndViews)
{
    // 1. Overlapping range insert straddling the insertion index with zero heap allocations on FixedVector
    gp::FixedVector<gp::Int32, 16u> fixed{ 10, 20, 30, 40, 50 };
    // Insert [20, 30, 40] (indices 1..3) at index 2 (inside the source range!)
    fixed.insert(fixed.begin() + 2u, fixed.data() + 1u, 3u);
    // Expected: [10, 20, 20, 30, 40, 30, 40, 50]
    ASSERT_EQ(fixed.size(), 8u);
    EXPECT_EQ(fixed[0], 10);
    EXPECT_EQ(fixed[1], 20);
    EXPECT_EQ(fixed[2], 20);
    EXPECT_EQ(fixed[3], 30);
    EXPECT_EQ(fixed[4], 40);
    EXPECT_EQ(fixed[5], 30);
    EXPECT_EQ(fixed[6], 40);
    EXPECT_EQ(fixed[7], 50);

    // 2. insertDefaulted / insertUninitialized / insertZeroed
    gp::Vector<gp::Int32> gapVec{ 1, 4 };
    auto uninitIt = gapVec.insertUninitialized(1u, 2u);
    uninitIt[0] = 2;
    uninitIt[1] = 3;
    ASSERT_EQ(gapVec.size(), 4u);
    EXPECT_EQ(gapVec[0], 1);
    EXPECT_EQ(gapVec[1], 2);
    EXPECT_EQ(gapVec[2], 3);
    EXPECT_EQ(gapVec[3], 4);

    gapVec.insertZeroed(2u, 2u);
    ASSERT_EQ(gapVec.size(), 6u);
    EXPECT_EQ(gapVec[2], 0);
    EXPECT_EQ(gapVec[3], 0);
    EXPECT_EQ(gapVec[4], 3);

    // 3. Binary search and deduplication
    gp::Vector<gp::Int32> dupes{ 30, 10, 20, 10, 30, 20, 40 };
    const gp::USize removedDupes = dupes.sortAndDeduplicate();
    EXPECT_EQ(removedDupes, 3u);
    ASSERT_EQ(dupes.size(), 4u);
    EXPECT_EQ(dupes[0], 10);
    EXPECT_EQ(dupes[1], 20);
    EXPECT_EQ(dupes[2], 30);
    EXPECT_EQ(dupes[3], 40);

    EXPECT_EQ(dupes.binarySearch(20), 1u);
    EXPECT_EQ(dupes.binarySearch(25), gp::Vector<gp::Int32>::npos);
    EXPECT_EQ(dupes.lowerBound(25), 2u);
    EXPECT_EQ(dupes.upperBound(20), 2u);

    // 4. Views, slicing, and AoS member projection directly from gp::Vector
    gp::VectorView<gp::Int32> subSlice = dupes.slice(1u, 2u);
    ASSERT_EQ(subSlice.size(), 2u);
    EXPECT_EQ(subSlice[0], 20);
    EXPECT_EQ(subSlice[1], 30);
    subSlice[0] = 25;
    EXPECT_EQ(dupes[1], 25);

    struct ItemRecord
    {
        gp::Int32 key;
        gp::Float32 weight;
    };

    gp::Vector<ItemRecord> records{
        { 1, 1.5f },
        { 2, 2.5f },
        { 3, 3.5f }
    };
    gp::StridedView<gp::Int32> keys = records.project(&ItemRecord::key);
    ASSERT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], 1);
    EXPECT_EQ(keys[1], 2);
    EXPECT_EQ(keys[2], 3);

    gp::Vector<gp::Int32> copiedKeys(keys);
    ASSERT_EQ(copiedKeys.size(), 3u);
    EXPECT_EQ(copiedKeys[0], 1);
    EXPECT_EQ(copiedKeys[2], 3);
}

}   // namespace gp::tests
