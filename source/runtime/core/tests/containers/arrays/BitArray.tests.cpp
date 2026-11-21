// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/arrays/BitArray.hpp"
#include "containers/sets/BitSet.hpp"
#include "hashing/Crc32.hpp"
#include "hashing/HashOf.hpp"
#include "hashing/Sha256.hpp"
#include "hashing/XxHash.hpp"
#include "memory/allocators/InlineAllocator.hpp"
#include "memory/allocators/LinearAllocator.hpp"
#include "memory/allocators/PolymorphicAllocator.hpp"
#include "memory/allocators/ThreadScratchAllocator.hpp"
#include <algorithm>
#include <compare>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>
#include <ranges>   // IWYU pragma: keep
#include <utility>

namespace gp::tests
{

namespace
{

template <typename InBitArray>
void verifyBitArrayInvariants(const InBitArray& bits)
{
    EXPECT_EQ(bits.isEmpty(), bits.size() == 0u);
    EXPECT_LE(bits.size(), bits.capacity());
    EXPECT_LE(bits.wordCount(), bits.wordCapacity());
    EXPECT_EQ(bits.sizeInBytes(), bits.wordCount() * sizeof(gp::UInt64));
    EXPECT_EQ(bits.countSetBits() + bits.countZeroBits(), bits.size());
    EXPECT_EQ(bits.end() - bits.begin(), static_cast<typename InBitArray::DifferenceType>(bits.size()));
    EXPECT_EQ(bits.rend() - bits.rbegin(), static_cast<typename InBitArray::DifferenceType>(bits.size()));

    // Verify unused trailing bits in the final active word stay strictly zeroed.
    const gp::USize activeWords = bits.wordCount();
    const gp::USize tailBits = bits.size() & 63u;
    if (activeWords > 0u && tailBits != 0u)
    {
        const gp::UInt64 tailWord = bits.getWord(activeWords - 1u);
        const gp::UInt64 invalidHighMask = ~((gp::UInt64{ 1 } << tailBits) - 1ULL);
        EXPECT_EQ(tailWord & invalidHighMask, 0ULL);
    }
}

}   // namespace

TEST(BitArrayTests, FootprintConceptsAndEmptyState)
{
    static_assert(sizeof(gp::BitArray<>) == 24u);
    static_assert(sizeof(gp::ScratchBitArray) == 24u);
    static_assert(sizeof(gp::PolymorphicBitArray) == 32u);

    static_assert(gp::BitArray<>::kBitsPerWord == 64u);
    static_assert(gp::BitArray<>::npos == static_cast<gp::USize>(-1));

    static_assert(gp::concepts::IsBitArray<gp::BitArray<>>);
    static_assert(gp::concepts::IsBitArray<gp::ScratchBitArray>);
    static_assert(gp::concepts::IsBitArray<gp::InlineBitArray<64u>>);
    static_assert(!gp::concepts::IsBitArray<gp::BitSet<64u>>);
    static_assert(!gp::concepts::IsBitArray<gp::Int32>);

    static_assert(gp::concepts::IsIterable<gp::BitArray<>>);
    static_assert(gp::concepts::IsSizedRange<gp::BitArray<>>);
    static_assert(gp::concepts::IsRelocatable<gp::BitArray<>>);
    static_assert(gp::concepts::IsRelocatable<gp::PolymorphicBitArray>);
    static_assert(!gp::concepts::IsRelocatable<gp::InlineBitArray<64u>>);
    static_assert(std::random_access_iterator<gp::BitArray<>::Iterator>);
    static_assert(std::random_access_iterator<gp::BitArray<>::ConstIterator>);
    static_assert(std::forward_iterator<gp::BitArray<>::SetBitIterator>);
    static_assert(std::forward_iterator<gp::BitArray<>::ZeroBitIterator>);

    gp::BitArray<> bits;
    verifyBitArrayInvariants(bits);
    EXPECT_TRUE(bits.isEmpty());
    EXPECT_EQ(bits.size(), 0u);
    EXPECT_EQ(bits.wordCount(), 0u);
    EXPECT_EQ(bits.capacity(), 0u);
    EXPECT_EQ(bits.wordCapacity(), 0u);
    EXPECT_EQ(bits.sizeInBytes(), 0u);
    EXPECT_GT(bits.maxSize(), 0u);
    EXPECT_EQ(bits.data(), nullptr);
    EXPECT_EQ(std::as_const(bits).data(), nullptr);
    EXPECT_EQ(bits.countSetBits(), 0u);
    EXPECT_EQ(bits.countZeroBits(), 0u);
    EXPECT_EQ(bits.countSetBitsInRange(0u, 0u), 0u);
    EXPECT_FALSE(bits.isAnySet());
    EXPECT_TRUE(bits.isAllSet());
    EXPECT_TRUE(bits.isNoneSet());
    EXPECT_EQ(bits.findFirstSet(), gp::BitArray<>::npos);
    EXPECT_EQ(bits.findFirstZero(), gp::BitArray<>::npos);
    EXPECT_EQ(bits.findLastSet(), gp::BitArray<>::npos);
    EXPECT_EQ(bits.findLastZero(), gp::BitArray<>::npos);
    EXPECT_EQ(bits.findContiguousZeros(1u), gp::BitArray<>::npos);
    EXPECT_EQ(bits.findAndSetFirstZero(), gp::BitArray<>::npos);

    bits.setAll();
    bits.clearAll();
    bits.fill(true);
    bits.flipAll();
    bits.setRange(0u, 0u);
    bits.clearRange(0u, 0u);
    bits.flipRange(0u, 0u);
    bits <<= 5u;
    bits >>= 5u;
    bits.clear();
    bits.shrinkToFit();
    bits.reset();
    verifyBitArrayInvariants(bits);
}

TEST(BitArrayTests, ConstructorsAssignmentsAndBitSetInterop)
{
    const gp::BitArray<> zeroSized(0u, true);
    EXPECT_TRUE(zeroSized.isEmpty());

    const gp::BitArray<> filledFalse(70u, false);
    verifyBitArrayInvariants(filledFalse);
    EXPECT_EQ(filledFalse.size(), 70u);
    EXPECT_EQ(filledFalse.wordCount(), 2u);
    EXPECT_TRUE(filledFalse.isNoneSet());

    const gp::BitArray<> filledTrue(130u, true);
    verifyBitArrayInvariants(filledTrue);
    EXPECT_EQ(filledTrue.size(), 130u);
    EXPECT_EQ(filledTrue.wordCount(), 3u);
    EXPECT_TRUE(filledTrue.isAllSet());
    EXPECT_EQ(filledTrue.countSetBits(), 130u);

    const gp::BitArray<> initBits{ true, false, false, true, true };
    verifyBitArrayInvariants(initBits);
    ASSERT_EQ(initBits.size(), 5u);
    EXPECT_TRUE(initBits.test(0u));
    EXPECT_FALSE(initBits.test(1u));
    EXPECT_FALSE(initBits.test(2u));
    EXPECT_TRUE(initBits.test(3u));
    EXPECT_TRUE(initBits.test(4u));

    const gp::BitArray<> emptyInit(std::initializer_list<bool>{});
    EXPECT_TRUE(emptyInit.isEmpty());

    // Construction & assignment from gp::BitSet
    gp::BitSet<70u> bitSet;
    (void)bitSet.set(0u);
    (void)bitSet.set(63u);
    (void)bitSet.set(64u);
    (void)bitSet.set(69u);

    const gp::BitArray<> fromBitSet(bitSet);
    verifyBitArrayInvariants(fromBitSet);
    EXPECT_EQ(fromBitSet.size(), 70u);
    EXPECT_EQ(fromBitSet.countSetBits(), 4u);
    EXPECT_TRUE(fromBitSet.test(0u));
    EXPECT_TRUE(fromBitSet.test(63u));
    EXPECT_TRUE(fromBitSet.test(64u));
    EXPECT_TRUE(fromBitSet.test(69u));

    // Copy, cross-allocator copy, move, and assign overloads
    const gp::BitArray<> copied(fromBitSet);
    EXPECT_EQ(copied, fromBitSet);

    const gp::ScratchBitArray crossCopied(fromBitSet);
    EXPECT_EQ(crossCopied, fromBitSet);

    gp::BitArray<> moveSource(fromBitSet);
    gp::BitArray<> movedClear(std::move(moveSource));
    EXPECT_TRUE(moveSource.isEmpty());
    EXPECT_EQ(moveSource.capacity(), 0u);
    EXPECT_EQ(movedClear, fromBitSet);

    gp::BitArray<> assigned;
    assigned = fromBitSet;
    EXPECT_EQ(assigned, fromBitSet);

    assigned = { false, true, false, true };
    EXPECT_EQ(assigned.size(), 4u);
    EXPECT_EQ(assigned.countSetBits(), 2u);

    assigned.assign(crossCopied);
    EXPECT_EQ(assigned, fromBitSet);

    gp::BitSet<5u> smallSet(true);
    assigned.assign(smallSet);
    EXPECT_EQ(assigned.size(), 5u);
    EXPECT_TRUE(assigned.isAllSet());

    const gp::UInt64 rawWords[2] = { 0xAAAAAAAAAAAAAAAAULL, 0xFFFFFFFFFFFFFFFFULL };
    assigned.assignFromWords(rawWords, 70u);
    verifyBitArrayInvariants(assigned);
    EXPECT_EQ(assigned.size(), 70u);
    EXPECT_EQ(assigned.getWord(0u), 0xAAAAAAAAAAAAAAAAULL);
    EXPECT_EQ(assigned.getWord(1u), 0x3FULL);
    assigned.assignFromWords(nullptr, 0u);
    EXPECT_TRUE(assigned.isEmpty());

    gp::BitArray<> moveAssigned;
    moveAssigned = std::move(assigned);
    EXPECT_TRUE(moveAssigned.isEmpty());
    moveAssigned = fromBitSet;
    gp::BitArray<>& selfRef = moveAssigned;
    moveAssigned = selfRef;
    moveAssigned = std::move(selfRef);
    EXPECT_EQ(moveAssigned, fromBitSet);
}

TEST(BitArrayTests, DynamicGrowthPushPopRemoveAtSwapResizeAndShrink)
{
    gp::BitArray<> bits;

    // Push across word boundary 0 -> 1 -> 2 and verify trailing bits stay zeroed.
    for (gp::USize i = 0u; i < 135u; ++i)
    {
        bits.pushBack((i % 3u) == 0u);
        verifyBitArrayInvariants(bits);
    }
    EXPECT_EQ(bits.size(), 135u);
    for (gp::USize i = 0u; i < 135u; ++i)
    {
        EXPECT_EQ(bits[i], (i % 3u) == 0u);
    }

    bits.pushBack(true, 10u);
    verifyBitArrayInvariants(bits);
    EXPECT_EQ(bits.size(), 145u);
    for (gp::USize i = 135u; i < 145u; ++i)
    {
        EXPECT_TRUE(bits.test(i));
    }
    bits.pushBack(false, 0u);
    EXPECT_EQ(bits.size(), 145u);

    // popBack preserves sanitized tail
    for (gp::USize i = 0u; i < 10u; ++i)
    {
        bits.popBack();
        verifyBitArrayInvariants(bits);
    }
    EXPECT_EQ(bits.size(), 135u);

    // removeAtSwap overwrites target bit with last bit in O(1)
    gp::BitArray<> swapArr{ true, false, false, false, true };
    swapArr.removeAtSwap(1u);   // Last bit (true) moves to index 1 -> [true, true, false, false]
    verifyBitArrayInvariants(swapArr);
    ASSERT_EQ(swapArr.size(), 4u);
    EXPECT_TRUE(swapArr[0u]);
    EXPECT_TRUE(swapArr[1u]);
    EXPECT_FALSE(swapArr[2u]);
    EXPECT_FALSE(swapArr[3u]);
    swapArr.removeAtSwap(3u);   // Removing final element pops without swap copy
    EXPECT_EQ(swapArr.size(), 3u);

    // Clearing and repushing / resizing must never leak old word contents!
    gp::BitArray<> dirtyReuse(130u, true);
    const gp::USize preservedCap = dirtyReuse.capacity();
    dirtyReuse.clear();
    EXPECT_TRUE(dirtyReuse.isEmpty());
    EXPECT_EQ(dirtyReuse.capacity(), preservedCap);
    dirtyReuse.pushBack(false);
    dirtyReuse.pushBack(true);
    verifyBitArrayInvariants(dirtyReuse);
    EXPECT_EQ(dirtyReuse.countSetBits(), 1u);
    EXPECT_FALSE(dirtyReuse.test(0u));
    EXPECT_TRUE(dirtyReuse.test(1u));

    dirtyReuse.clear();
    dirtyReuse.resize(70u, false);
    verifyBitArrayInvariants(dirtyReuse);
    EXPECT_TRUE(dirtyReuse.isNoneSet());

    dirtyReuse.clear();
    dirtyReuse.resize(70u, true);
    verifyBitArrayInvariants(dirtyReuse);
    EXPECT_TRUE(dirtyReuse.isAllSet());
    EXPECT_EQ(dirtyReuse.countSetBits(), 70u);

    // Grow with false, grow with true, shrink, reserve, resizeUninitialized, shrinkToFit, reset
    dirtyReuse.resize(70u, false);   // No-op same size
    dirtyReuse.resize(130u, false);
    verifyBitArrayInvariants(dirtyReuse);
    EXPECT_EQ(dirtyReuse.countSetBits(), 70u);
    EXPECT_FALSE(dirtyReuse.test(70u));
    EXPECT_FALSE(dirtyReuse.test(129u));

    dirtyReuse.resize(150u, true);
    verifyBitArrayInvariants(dirtyReuse);
    EXPECT_EQ(dirtyReuse.countSetBits(), 90u);
    EXPECT_TRUE(dirtyReuse.test(130u));
    EXPECT_TRUE(dirtyReuse.test(149u));

    dirtyReuse.resize(30u);
    verifyBitArrayInvariants(dirtyReuse);
    EXPECT_EQ(dirtyReuse.size(), 30u);
    EXPECT_TRUE(dirtyReuse.isAllSet());

    dirtyReuse.reserve(2000u);
    EXPECT_GE(dirtyReuse.capacity(), 2000u);
    dirtyReuse.resizeUninitialized(200u);
    verifyBitArrayInvariants(dirtyReuse);
    dirtyReuse.clearAll();
    dirtyReuse.setWord(0u, 0xFFULL);
    dirtyReuse.setWord(3u, ~0ULL);   // Trims bits >= 200!
    verifyBitArrayInvariants(dirtyReuse);
    EXPECT_EQ(dirtyReuse.getWord(3u), 0xFFULL);

    dirtyReuse.resize(10u);
    dirtyReuse.shrinkToFit();
    verifyBitArrayInvariants(dirtyReuse);
    EXPECT_LT(dirtyReuse.capacity(), 2000u);

    dirtyReuse.clear();
    dirtyReuse.shrinkToFit();
    EXPECT_EQ(dirtyReuse.capacity(), 0u);
    EXPECT_EQ(dirtyReuse.data(), nullptr);

    dirtyReuse.resize(64u, true);
    dirtyReuse.reset();
    EXPECT_TRUE(dirtyReuse.isEmpty());
    EXPECT_EQ(dirtyReuse.capacity(), 0u);
    EXPECT_EQ(dirtyReuse.data(), nullptr);
}

TEST(BitArrayTests, SingleBitProxyRangeScansAndSetAlgebra)
{
    gp::BitArray<> bits(130u, false);

    (void)bits.set(2u);
    (void)bits.set(65u, true);
    (void)bits.set(65u, false);
    (void)bits.clearBit(2u);
    (void)bits.flipBit(128u);
    EXPECT_TRUE(bits.test(128u));
    (void)bits.flipBit(128u);

    EXPECT_FALSE(bits.testAndSet(42u));
    EXPECT_TRUE(bits.testAndSet(42u));
    EXPECT_TRUE(bits.testAndClear(42u));
    EXPECT_FALSE(bits.testAndClear(42u));

    bits[5u] = true;
    bits[6u] = bits[5u];
    EXPECT_TRUE(bits.test(6u));
    bits[6u].flip();
    EXPECT_FALSE(std::as_const(bits)[6u]);

    bits.setRange(10u, 110u);
    EXPECT_EQ(bits.countSetBitsInRange(10u, 110u), 110u);
    bits.clearRange(30u, 70u);
    EXPECT_EQ(bits.findContiguousZeros(70u, 20u), 30u);
    bits.flipRange(30u, 20u);
    EXPECT_EQ(bits.countSetBitsInRange(30u, 20u), 20u);

    bits.clearAll();
    EXPECT_TRUE(bits.isNoneSet());
    bits.setAll();
    EXPECT_TRUE(bits.isAllSet());
    bits.flipAll();
    EXPECT_TRUE(bits.isNoneSet());
    bits.fill(true);
    EXPECT_TRUE(bits.isAllSet());
    bits.fill(false);
    EXPECT_TRUE(bits.isNoneSet());

    // First / last set & zero scans
    (void)bits.set(12u);
    (void)bits.set(64u);
    (void)bits.set(129u);
    EXPECT_EQ(bits.findFirstSet(), 12u);
    EXPECT_EQ(bits.findFirstSet(13u), 64u);
    EXPECT_EQ(bits.findLastSet(), 129u);
    EXPECT_EQ(bits.findFirstZero(), 0u);
    EXPECT_EQ(bits.findLastZero(), 128u);
    EXPECT_EQ(bits.findAndSetFirstZero(11u), 11u);
    EXPECT_TRUE(bits.test(11u));

    // Cross-size set algebra & self-aliasing
    gp::BitArray<> small(4u);
    gp::BitArray<> large(130u);
    (void)small.set(1u);
    (void)small.set(3u);
    (void)large.set(1u);
    (void)large.set(100u);

    EXPECT_TRUE(large.intersects(small));
    EXPECT_FALSE(large.containsAll(small));
    (void)large.set(3u);
    EXPECT_TRUE(large.containsAll(small));

    gp::BitArray<> orResult = small | large;
    EXPECT_EQ(orResult.size(), 130u);
    EXPECT_EQ(orResult, large | small);

    gp::BitArray<> andResult = small & large;
    EXPECT_EQ(andResult.size(), 4u);
    EXPECT_EQ(andResult, large & small);
    EXPECT_EQ(andResult.countSetBits(), 2u);

    gp::BitArray<> xorResult = small ^ large;
    EXPECT_EQ(xorResult.size(), 130u);
    EXPECT_EQ(xorResult, large ^ small);
    EXPECT_EQ(xorResult.countSetBits(), 1u);
    EXPECT_TRUE(xorResult.test(100u));

    gp::BitArray<> andAssign = large;
    andAssign &= small;
    EXPECT_EQ(andAssign.size(), 130u);
    EXPECT_EQ(andAssign.countSetBits(), 2u);
    EXPECT_FALSE(andAssign.test(100u));
    andAssign &= andAssign;
    EXPECT_EQ(andAssign.countSetBits(), 2u);

    gp::BitArray<> orAssign = small;
    orAssign |= large;
    EXPECT_EQ(orAssign.size(), 130u);
    EXPECT_EQ(orAssign, large);
    orAssign |= orAssign;
    EXPECT_EQ(orAssign, large);

    gp::BitArray<> xorAssign = large;
    xorAssign ^= xorAssign;
    EXPECT_TRUE(xorAssign.isNoneSet());

    gp::BitArray<> subTarget = large;
    subTarget.subtract(small);
    EXPECT_EQ(subTarget.countSetBits(), 1u);
    EXPECT_TRUE(subTarget.test(100u));
    subTarget.subtract(subTarget);
    EXPECT_TRUE(subTarget.isNoneSet());

    const gp::BitArray<> negated = ~small;
    EXPECT_EQ(negated.size(), 4u);
    EXPECT_TRUE(negated.test(0u));
    EXPECT_FALSE(negated.test(1u));
    EXPECT_TRUE(negated.test(2u));
    EXPECT_FALSE(negated.test(3u));

    gp::BitArray<> shiftBits(130u);
    (void)shiftBits.set(0u);
    (void)shiftBits.set(63u);
    (void)shiftBits.set(129u);
    const gp::BitArray<> leftShifted = shiftBits << 65u;
    EXPECT_TRUE(leftShifted.test(65u));
    EXPECT_TRUE(leftShifted.test(128u));
    EXPECT_EQ(leftShifted.countSetBits(), 2u);
    const gp::BitArray<> rightShifted = leftShifted >> 65u;
    EXPECT_TRUE(rightShifted.test(0u));
    EXPECT_TRUE(rightShifted.test(63u));
    EXPECT_EQ(rightShifted.countSetBits(), 2u);
    shiftBits <<= 1u;
    shiftBits >>= 1u;
    EXPECT_FALSE(shiftBits.test(129u));
}

TEST(BitArrayTests, IteratorsVisitorsAndWordLevelSparseSetPattern)
{
    gp::BitArray<> occupancy(140u, false);
    (void)occupancy.set(0u);
    (void)occupancy.set(33u);
    (void)occupancy.set(64u);
    (void)occupancy.set(127u);
    (void)occupancy.set(139u);

    // Direct word-level getAndClearNextBit pattern (SparseSet / ECS query hot loop).
    gp::USize extractedFromWords[5]{};
    gp::USize extractedCount = 0u;
    const gp::UInt64* const words = occupancy.data();
    const gp::USize numWords = occupancy.wordCount();
    for (gp::USize wordIdx = 0u; wordIdx < numWords; ++wordIdx)
    {
        gp::UInt64 word = words[wordIdx];
        const gp::USize baseIndex = wordIdx * 64u;
        while (word != 0ULL)
        {
            const gp::UInt32 localBit = gp::getAndClearNextBit(word);
            extractedFromWords[extractedCount++] = baseIndex + static_cast<gp::USize>(localBit);
        }
    }
    ASSERT_EQ(extractedCount, 5u);
    EXPECT_EQ(extractedFromWords[0], 0u);
    EXPECT_EQ(extractedFromWords[1], 33u);
    EXPECT_EQ(extractedFromWords[2], 64u);
    EXPECT_EQ(extractedFromWords[3], 127u);
    EXPECT_EQ(extractedFromWords[4], 139u);

    // SetBitRange / SetBitIterator / ZeroBitRange / ZeroBitIterator
    gp::USize rangeCount = 0u;
    for (const gp::USize idx: occupancy.setBits())
    {
        EXPECT_EQ(idx, extractedFromWords[rangeCount++]);
    }
    EXPECT_EQ(rangeCount, 5u);
    EXPECT_EQ(*occupancy.beginSetBits(34u), 64u);
    EXPECT_EQ(occupancy.beginSetBits(140u), occupancy.endSetBits());

    gp::USize zeroIterCount = 0u;
    for (const gp::USize idx: occupancy.zeroBits())
    {
        EXPECT_FALSE(occupancy.test(idx));
        ++zeroIterCount;
    }
    EXPECT_EQ(zeroIterCount, 135u);
    EXPECT_EQ(*occupancy.beginZeroBits(138u), 138u);
    EXPECT_EQ(occupancy.beginZeroBits(139u), occupancy.endZeroBits());

    gp::USize visitedSet = 0u;
    occupancy.forEachSetBit(
        [&visitedSet](const gp::USize) noexcept -> void
    {
        ++visitedSet;
    }
    );
    EXPECT_EQ(visitedSet, 5u);

    gp::USize visitedZero = 0u;
    occupancy.forEachZeroBit(
        [&visitedZero](const gp::USize) noexcept -> void
    {
        ++visitedZero;
    }
    );
    EXPECT_EQ(visitedZero, 135u);

    // Random-access BitIterator & reverse iteration
    EXPECT_EQ(occupancy.end() - occupancy.begin(), 140);
    EXPECT_EQ(occupancy.cend() - occupancy.cbegin(), 140);
    EXPECT_EQ(occupancy.rend() - occupancy.rbegin(), 140);
    EXPECT_EQ(occupancy.crend() - occupancy.crbegin(), 140);
    EXPECT_TRUE(occupancy.cbegin()[33]);
    EXPECT_TRUE(*occupancy.crbegin());
    *occupancy.rbegin() = false;
    EXPECT_FALSE(occupancy.test(139u));
    *occupancy.rbegin() = true;
    EXPECT_TRUE(std::as_const(occupancy).begin()[139]);
    EXPECT_EQ(*occupancy.rend().base(), true);

    // consumeSetBits clears all bits in one sweep
    gp::USize consumedCount = 0u;
    occupancy.consumeSetBits(
        [&consumedCount](const gp::USize) noexcept -> void
    {
        ++consumedCount;
    }
    );
    EXPECT_EQ(consumedCount, 5u);
    EXPECT_TRUE(occupancy.isNoneSet());
}

TEST(BitArrayTests, AllocatorsSwapComparisonsAndUnifiedHashing)
{
    // InlineBitArray stays inside its embedded buffer up to 512 bits (64 bytes) and spills cleanly past that.
    gp::InlineBitArray<64u> inlineBits(128u, false);
    (void)inlineBits.set(0u);
    (void)inlineBits.set(127u);
    EXPECT_TRUE(inlineBits.getAllocator().isInlinePointer(inlineBits.data()));

    gp::InlineBitArray<64u> inlineCopy(inlineBits);
    EXPECT_EQ(inlineCopy, inlineBits);
    EXPECT_NE(inlineCopy.data(), inlineBits.data());
    EXPECT_TRUE(inlineCopy.getAllocator().isInlinePointer(inlineCopy.data()));

    gp::InlineBitArray<64u> inlineMoved(std::move(inlineCopy));
    EXPECT_EQ(inlineMoved, inlineBits);
    EXPECT_TRUE(inlineCopy.isEmpty());

    inlineBits.resize(1024u, true);   // 16 words = 128 bytes -> spills to HeapAllocator fallback
    EXPECT_FALSE(inlineBits.getAllocator().isInlinePointer(inlineBits.data()));
    EXPECT_EQ(inlineBits.countSetBits(), 898u);

    // PolymorphicBitArray with distinct LinearAllocator arenas
    alignas(16) gp::UInt8 arenaBufA[1024]{};
    gp::memory::LinearAllocator arenaA(arenaBufA, sizeof(arenaBufA), "BitArenaA");
    alignas(16) gp::UInt8 arenaBufB[1024]{};
    gp::memory::LinearAllocator arenaB(arenaBufB, sizeof(arenaBufB), "BitArenaB");

    {
        gp::PolymorphicBitArray emptyA{ gp::memory::PolymorphicAllocator(arenaA) };
        gp::PolymorphicBitArray polyA(70u, true, gp::memory::PolymorphicAllocator(arenaA));
        gp::PolymorphicBitArray polyB({ true, false, true }, gp::memory::PolymorphicAllocator(arenaB));
        gp::PolymorphicBitArray fromSet(gp::BitSet<8u>::fromMask(0x55ULL), gp::memory::PolymorphicAllocator(arenaA));

        EXPECT_TRUE(emptyA.isEmpty());
        EXPECT_TRUE(arenaA.owns(polyA.data()));
        EXPECT_TRUE(arenaB.owns(polyB.data()));
        EXPECT_EQ(fromSet.countSetBits(), 4u);

        // Unequal-allocator swap preserves each container's arena binding.
        gp::swap(polyA, polyB);
        EXPECT_EQ(polyA.size(), 3u);
        EXPECT_EQ(polyB.size(), 70u);
        EXPECT_TRUE(arenaA.owns(polyA.data()));
        EXPECT_TRUE(arenaB.owns(polyB.data()));

        // Unequal-allocator move constructor & move assignment
        gp::PolymorphicBitArray movedToA{ gp::memory::PolymorphicAllocator(arenaA) };
        movedToA = std::move(polyB);
        EXPECT_TRUE(arenaA.owns(movedToA.data()));
        EXPECT_TRUE(polyB.isEmpty());

        gp::PolymorphicBitArray equalMoved(std::move(movedToA));
        EXPECT_TRUE(arenaA.owns(equalMoved.data()));
        EXPECT_TRUE(movedToA.isEmpty());
        equalMoved.swap(equalMoved);
        (void)std::as_const(equalMoved).getAllocator();
    }

    // ScratchBitArray unwinds completely on ScopedScratchMark destruction
    gp::memory::LinearAllocator& tlsArena = gp::memory::ThreadScratchArena::get();
    const gp::USize baseline = tlsArena.getUsedBytes();
    {
        const gp::memory::ScopedScratchMark mark;
        gp::ScratchBitArray scratch(256u, true);
        EXPECT_GT(tlsArena.getUsedBytes(), baseline);
        EXPECT_EQ(scratch.countSetBits(), 256u);
    }
    EXPECT_EQ(tlsArena.getUsedBytes(), baseline);

    // Cross-allocator equality & three-way comparison
    gp::BitArray<> heapBits(70u, false);
    gp::ScratchBitArray scratchBits(70u, false);
    (void)heapBits.set(10u);
    (void)heapBits.set(69u);
    (void)scratchBits.set(10u);
    (void)scratchBits.set(69u);
    EXPECT_EQ(heapBits, scratchBits);
    EXPECT_EQ(heapBits <=> scratchBits, std::strong_ordering::equal);

    gp::BitArray<> diffSizeBits(71u, false);
    (void)diffSizeBits.set(10u);
    (void)diffSizeBits.set(69u);
    EXPECT_NE(heapBits, diffSizeBits);
    EXPECT_LT(heapBits, diffSizeBits);

    // Unified gp::hashing::hash API (cross-allocator and matching gp::BitSet!)
    gp::BitSet<70u> refSet;
    (void)refSet.set(10u);
    (void)refSet.set(69u);

    EXPECT_EQ(gp::hashing::hash(heapBits), gp::hashing::hash(scratchBits));
    EXPECT_EQ(gp::hashing::hash(heapBits), gp::hashing::hash(refSet));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::XxHash64>(heapBits), gp::hashing::hash<gp::hashing::XxHash64>(refSet));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::Crc32>(heapBits), gp::hashing::hash<gp::hashing::Crc32>(refSet));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::Sha256>(heapBits), gp::hashing::hash<gp::hashing::Sha256>(refSet));
    EXPECT_EQ(
        gp::hashing::hash(gp::hashing::HashAlgorithm::Wyhash64, heapBits),
        gp::hashing::hash(gp::hashing::HashAlgorithm::Wyhash64, scratchBits)
    );
    EXPECT_NE(gp::hashing::hash(heapBits), gp::hashing::hash(diffSizeBits));
}

TEST(BitArrayTests, BoundaryEdgesCrossAllocatorOpsRangesAndInlineLifecycle)
{
    // Word-boundary popBack (65 -> 64 -> 63 -> ... -> 0) and tail sanitization.
    gp::BitArray<> boundaryPop(65u, true);
    verifyBitArrayInvariants(boundaryPop);
    EXPECT_EQ(boundaryPop.wordCount(), 2u);
    boundaryPop.popBack();
    verifyBitArrayInvariants(boundaryPop);
    EXPECT_EQ(boundaryPop.size(), 64u);
    EXPECT_EQ(boundaryPop.wordCount(), 1u);
    boundaryPop.popBack();
    verifyBitArrayInvariants(boundaryPop);
    EXPECT_EQ(boundaryPop.size(), 63u);
    EXPECT_EQ(boundaryPop.getWord(0u), 0x7FFFFFFFFFFFFFFFULL);
    while (!boundaryPop.isEmpty())
    {
        boundaryPop.popBack();
        verifyBitArrayInvariants(boundaryPop);
    }
    EXPECT_EQ(boundaryPop.wordCount(), 0u);

    // Cross-word removeAtSwap (129 -> 128 -> 0).
    gp::BitArray<> swapBoundary(129u, false);
    (void)swapBoundary.set(128u);
    swapBoundary.removeAtSwap(10u);
    verifyBitArrayInvariants(swapBoundary);
    EXPECT_EQ(swapBoundary.size(), 128u);
    EXPECT_EQ(swapBoundary.wordCount(), 2u);
    EXPECT_TRUE(swapBoundary.test(10u));
    EXPECT_EQ(swapBoundary.countSetBits(), 1u);

    gp::BitArray<> singleSwap{ true };
    singleSwap.removeAtSwap(0u);
    verifyBitArrayInvariants(singleSwap);
    EXPECT_TRUE(singleSwap.isEmpty());

    // Shrink then re-grow with false must never revive stale high bits.
    gp::BitArray<> shrinkGrow(130u, true);
    shrinkGrow.resize(64u);
    shrinkGrow.resize(130u, false);
    verifyBitArrayInvariants(shrinkGrow);
    EXPECT_EQ(shrinkGrow.countSetBits(), 64u);
    shrinkGrow.resize(5u);
    shrinkGrow.resize(64u, false);
    verifyBitArrayInvariants(shrinkGrow);
    EXPECT_EQ(shrinkGrow.countSetBits(), 5u);

    // Empty and cross-size set algebra edge cases.
    gp::BitArray<> empty;
    gp::BitArray<> small(4u, false);
    gp::BitArray<> allZeroLarge(130u, false);
    gp::BitArray<> largeWithBit(130u, false);
    (void)largeWithBit.set(70u);

    EXPECT_TRUE((empty & largeWithBit).isEmpty());
    EXPECT_TRUE((largeWithBit & empty).isEmpty());
    EXPECT_EQ(empty | largeWithBit, largeWithBit);
    EXPECT_EQ(empty ^ largeWithBit, largeWithBit);
    EXPECT_TRUE(small.containsAll(allZeroLarge));
    EXPECT_TRUE(empty.containsAll(allZeroLarge));
    EXPECT_FALSE(empty.containsAll(largeWithBit));
    EXPECT_FALSE(empty.intersects(largeWithBit));
    EXPECT_FALSE(small.intersects(largeWithBit));

    gp::BitArray<> shrunkMask(largeWithBit);
    shrunkMask &= empty;
    EXPECT_EQ(shrunkMask.size(), 130u);
    EXPECT_TRUE(shrunkMask.isNoneSet());

    // Heterogeneous-allocator binary operators (&, |, ^) preserve lhs allocator.
    alignas(16) gp::UInt8 arenaBuf[1024]{};
    gp::memory::LinearAllocator arena(arenaBuf, sizeof(arenaBuf), "CrossOpArena");
    {
        const gp::memory::ScopedScratchMark scratchMark;
        gp::BitArray<> heapA(130u, false);
        (void)heapA.set(1u);
        (void)heapA.set(65u);

        gp::ScratchBitArray scratchB(130u, false);
        (void)scratchB.set(65u);
        (void)scratchB.set(129u);

        gp::PolymorphicBitArray polyC(130u, false, gp::memory::PolymorphicAllocator(arena));
        (void)polyC.set(1u);
        (void)polyC.set(129u);

        const gp::BitArray<> heapInter = heapA & scratchB;
        EXPECT_EQ(heapInter.countSetBits(), 1u);
        EXPECT_TRUE(heapInter.test(65u));

        const gp::ScratchBitArray scratchUnion = scratchB | heapA;
        EXPECT_EQ(scratchUnion.countSetBits(), 3u);
        EXPECT_TRUE(scratchUnion.test(1u));
        EXPECT_TRUE(scratchUnion.test(65u));
        EXPECT_TRUE(scratchUnion.test(129u));

        const gp::PolymorphicBitArray polyXor = polyC ^ heapA;
        EXPECT_TRUE(arena.owns(polyXor.data()));
        EXPECT_EQ(polyXor.countSetBits(), 2u);
        EXPECT_TRUE(polyXor.test(65u));
        EXPECT_TRUE(polyXor.test(129u));
    }

    // Aligned word shifts and ranges/iterator checks on BitArray.
    gp::BitArray<> rangeBits(256u, false);
    (void)rangeBits.set(0u);
    (void)rangeBits.set(255u);
    EXPECT_EQ(rangeBits << 0u, rangeBits);
    EXPECT_EQ(rangeBits >> 0u, rangeBits);
    const gp::BitArray<> shifted128 = rangeBits << 128u;
    EXPECT_TRUE(shifted128.test(128u));
    EXPECT_EQ(shifted128.countSetBits(), 1u);
    EXPECT_EQ(
        (rangeBits << 64u) >> 64u,
        gp::BitArray<>(256u, false) | []() noexcept -> gp::BitArray<>
    {
        gp::BitArray<> one(256u, false);
        (void)one.set(0u);
        return one;
    }()
    );

    gp::BitArray<>::SetBitIterator setItA = rangeBits.beginSetBits();
    gp::BitArray<>::SetBitIterator setItB = setItA;
    EXPECT_EQ(*setItA++, 0u);
    EXPECT_EQ(*setItA++, 255u);
    EXPECT_EQ(setItA, rangeBits.endSetBits());
    EXPECT_EQ(*setItB, 0u);
    EXPECT_EQ(std::ranges::count(rangeBits, true), 2);
    EXPECT_TRUE(std::ranges::equal(rangeBits, gp::BitArray<>(rangeBits)));

    // InlineBitArray swap + spill + reset + re-inline lifecycle.
    gp::InlineBitArray<64u> inlA(64u, false);
    gp::InlineBitArray<64u> inlB(64u, true);
    (void)inlA.set(3u);
    EXPECT_TRUE(inlA.getAllocator().isInlinePointer(inlA.data()));
    EXPECT_TRUE(inlB.getAllocator().isInlinePointer(inlB.data()));
    gp::swap(inlA, inlB);
    EXPECT_EQ(inlA.countSetBits(), 64u);
    EXPECT_EQ(inlB.countSetBits(), 1u);
    EXPECT_TRUE(inlB.test(3u));
    EXPECT_TRUE(inlA.getAllocator().isInlinePointer(inlA.data()));
    EXPECT_TRUE(inlB.getAllocator().isInlinePointer(inlB.data()));

    inlA.resize(1024u, true);
    EXPECT_FALSE(inlA.getAllocator().isInlinePointer(inlA.data()));
    inlA.reset();
    inlA.resize(64u, false);
    EXPECT_TRUE(inlA.getAllocator().isInlinePointer(inlA.data()));

    // Length-framed hashing distinguishes 64 vs 65 zero bits; test all gp::* bit extraction aliases.
    const gp::BitArray<> zero64(64u, false);
    const gp::BitArray<> zero65(65u, false);
    EXPECT_NE(gp::hashing::hash(zero64), gp::hashing::hash(zero65));
    EXPECT_NE(gp::hashing::hash<gp::hashing::XxHash64>(zero64), gp::hashing::hash<gp::hashing::XxHash64>(zero65));
    EXPECT_NE(gp::hashing::hash<gp::hashing::Crc32>(zero64), gp::hashing::hash<gp::hashing::Crc32>(zero65));
    EXPECT_NE(gp::hashing::hash<gp::hashing::Sha256>(zero64), gp::hashing::hash<gp::hashing::Sha256>(zero65));

    gp::UInt64 aliasMask = (1ULL << 2u) | (1ULL << 60u);
    gp::UInt32 outIdx = 0u;
    ASSERT_TRUE(gp::tryGetAndClearNextBit(aliasMask, outIdx));
    EXPECT_EQ(outIdx, 2u);
    ASSERT_TRUE(gp::tryGetAndClearHighestBit(aliasMask, outIdx));
    EXPECT_EQ(outIdx, 60u);
    EXPECT_EQ(aliasMask, 0ULL);
    aliasMask = 1ULL << 19u;
    EXPECT_EQ(gp::getAndClearHighestBit(aliasMask), 19u);
    EXPECT_EQ(aliasMask, 0ULL);

    // Grow-with-true across word boundaries, assign(count, value), self-aliasing, and chained mutators.
    gp::BitArray<> boundaryFill;
    boundaryFill.reserve(256u);
    boundaryFill.resize(5u, true);
    verifyBitArrayInvariants(boundaryFill);
    EXPECT_EQ(boundaryFill.countSetBits(), 5u);
    boundaryFill.resize(64u, true);
    verifyBitArrayInvariants(boundaryFill);
    EXPECT_EQ(boundaryFill.countSetBits(), 64u);
    boundaryFill.resize(192u, true);
    verifyBitArrayInvariants(boundaryFill);
    EXPECT_TRUE(boundaryFill.isAllSet());
    EXPECT_EQ(boundaryFill.countSetBits(), 192u);

    boundaryFill.clear();
    boundaryFill.resize(10u, false);
    boundaryFill.resize(70u, true);
    verifyBitArrayInvariants(boundaryFill);
    EXPECT_EQ(boundaryFill.countSetBits(), 60u);
    EXPECT_FALSE(boundaryFill.test(9u));
    EXPECT_TRUE(boundaryFill.test(10u));
    EXPECT_TRUE(boundaryFill.test(69u));

    boundaryFill.assign(130u, true);
    verifyBitArrayInvariants(boundaryFill);
    EXPECT_TRUE(boundaryFill.isAllSet());
    boundaryFill.assign(0u, true);
    EXPECT_TRUE(boundaryFill.isEmpty());

    boundaryFill.resize(70u, false);
    (void)boundaryFill.set(1u).set(5u).flipBit(1u).clearBit(5u).set(69u);
    EXPECT_FALSE(boundaryFill.test(1u));
    EXPECT_FALSE(boundaryFill.test(5u));
    EXPECT_TRUE(boundaryFill.test(69u));
    boundaryFill.assignFromWords(boundaryFill.data(), 70u);
    EXPECT_TRUE(boundaryFill.test(69u));
    boundaryFill.assign(boundaryFill);
    EXPECT_EQ(boundaryFill.countSetBits(), 1u);

    // InlineBitArray swap across mixed inline/spilled and dual-spilled states.
    gp::InlineBitArray<64u> mixInline(32u, false);
    (void)mixInline.set(7u);
    gp::InlineBitArray<64u> mixSpilled(1024u, true);
    EXPECT_TRUE(mixInline.getAllocator().isInlinePointer(mixInline.data()));
    EXPECT_FALSE(mixSpilled.getAllocator().isInlinePointer(mixSpilled.data()));
    gp::swap(mixInline, mixSpilled);
    EXPECT_EQ(mixInline.size(), 1024u);
    EXPECT_EQ(mixSpilled.size(), 32u);
    EXPECT_TRUE(mixSpilled.test(7u));
    EXPECT_FALSE(mixInline.getAllocator().isInlinePointer(mixInline.data()));
    EXPECT_TRUE(mixSpilled.getAllocator().isInlinePointer(mixSpilled.data()));

    mixSpilled.resize(900u, false);
    EXPECT_FALSE(mixSpilled.getAllocator().isInlinePointer(mixSpilled.data()));
    gp::swap(mixInline, mixSpilled);
    EXPECT_EQ(mixInline.size(), 900u);
    EXPECT_EQ(mixSpilled.size(), 1024u);

    // Multi-word zero iterator skips, cross-container iterator inequality, and consumeSetBits re-queue safety.
    gp::BitArray<> fullWithHoles(256u, true);
    (void)fullWithHoles.clearBit(1u);
    (void)fullWithHoles.clearBit(250u);
    gp::BitArray<>::ZeroBitIterator zeroSkipIt = fullWithHoles.beginZeroBits();
    ASSERT_NE(zeroSkipIt, fullWithHoles.endZeroBits());
    EXPECT_EQ(*zeroSkipIt++, 1u);
    EXPECT_EQ(*zeroSkipIt++, 250u);
    EXPECT_EQ(zeroSkipIt, fullWithHoles.endZeroBits());

    gp::BitArray<> otherSameBits(fullWithHoles);
    EXPECT_NE(fullWithHoles.beginZeroBits(), otherSameBits.beginZeroBits());

    gp::BitArray<> requeueBits(70u, false);
    (void)requeueBits.set(2u);
    (void)requeueBits.set(15u);
    requeueBits.consumeSetBits(
        [&requeueBits](const gp::USize bitIdx) noexcept -> void
    {
        if (bitIdx == 2u)
        {
            (void)requeueBits.set(22u);
        }
    }
    );
    EXPECT_FALSE(requeueBits.test(2u));
    EXPECT_FALSE(requeueBits.test(15u));
    EXPECT_TRUE(requeueBits.test(22u));
    EXPECT_EQ(requeueBits.countSetBits(), 1u);

    // Unified hashOf / hashValues / DynamicHasher parity and maxSize() safety bound.
    EXPECT_EQ(zero64.maxSize(), static_cast<gp::USize>(std::numeric_limits<gp::ISize>::max()));
    const gp::BitArray<> zero64Copy(zero64);
    EXPECT_EQ(gp::hashing::hashOf(zero64), gp::hashing::hash(zero64Copy));
    EXPECT_EQ(gp::hashing::hashValues(zero64, zero65), gp::hashing::hashValues(zero64Copy, zero65));
    gp::hashing::DynamicHasher dynArrA(gp::hashing::HashAlgorithm::Crc32);
    gp::hashing::DynamicHasher dynArrB(gp::hashing::HashAlgorithm::Crc32);
    zero64.hashAppend(dynArrA);
    zero64Copy.hashAppend(dynArrB);
    EXPECT_EQ(dynArrA.finalize32(), dynArrB.finalize32());
}

}   // namespace gp::tests
