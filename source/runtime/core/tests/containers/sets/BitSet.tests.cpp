// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/sets/BitSet.hpp"
#include "hashing/Crc32.hpp"
#include "hashing/HashOf.hpp"
#include "hashing/Sha256.hpp"
#include "hashing/XxHash.hpp"
#include <algorithm>
#include <compare>
#include <gtest/gtest.h>
#include <iterator>
#include <ranges>   // IWYU pragma: keep
#include <utility>

namespace gp::tests
{

namespace
{

consteval bool verifyConstexprBitSetSuite() noexcept
{
    gp::BitSet<130u> bs;
    if (bs.size() != 130u || bs.capacity() != 130u || bs.wordCount() != 3u || bs.isEmpty())
    {
        return false;
    }

    (void)bs.set(0u);
    (void)bs.set(63u);
    (void)bs.set(64u);
    (void)bs.set(129u);
    if (!bs.test(0u) || !bs.test(64u) || !bs.test(129u) || bs.test(1u))
    {
        return false;
    }

    if (bs.countSetBits() != 4u || bs.countZeroBits() != 126u)
    {
        return false;
    }

    if (bs.findFirstSet() != 0u || bs.findFirstSet(1u) != 63u || bs.findLastSet() != 129u)
    {
        return false;
    }

    if (bs.findFirstZero() != 1u || bs.findLastZero() != 128u)
    {
        return false;
    }

    if (bs.testAndSet(1u) || !bs.testAndSet(1u))
    {
        return false;
    }
    if (!bs.testAndClear(1u) || bs.testAndClear(1u))
    {
        return false;
    }

    bs.setRange(10u, 70u);
    if (bs.countSetBitsInRange(10u, 70u) != 70u)
    {
        return false;
    }
    bs.clearRange(20u, 50u);
    if (bs.findContiguousZeros(50u, 10u) != 20u)
    {
        return false;
    }
    bs.flipRange(10u, 10u);

    const gp::USize claimed = bs.findAndSetFirstZero(1u);
    if (claimed != 1u || !bs.test(1u))
    {
        return false;
    }

    gp::BitSet<130u> shifted = bs << 5u;
    shifted >>= 5u;
    if (shifted.test(129u))
    {
        return false;
    }

    gp::USize visitedCount = 0u;
    for (const gp::USize idx: bs.setBits())
    {
        (void)idx;
        ++visitedCount;
    }
    if (visitedCount != bs.countSetBits())
    {
        return false;
    }

    gp::USize drainedCount = 0u;
    bs.consumeSetBits(
        [&drainedCount](const gp::USize) noexcept -> void
    {
        ++drainedCount;
    }
    );
    if (drainedCount != visitedCount || !bs.isNoneSet())
    {
        return false;
    }

    const gp::BitSet<5u> masked = gp::BitSet<5u>::fromMask(0xFFULL);
    if (!masked.isAllSet() || masked.toUInt64() != 0x1FULL)
    {
        return false;
    }

    const gp::BitSet<64u> hashA = gp::BitSet<64u>::fromMask(0xCAFEBABE12345678ULL);
    gp::BitSet<64u> hashB;
    hashB.setWord(0u, 0xCAFEBABE12345678ULL);
    if (gp::hashing::hash(hashA) != gp::hashing::hash(hashB))
    {
        return false;
    }
    if (gp::hashing::hash<gp::hashing::XxHash64>(hashA) != gp::hashing::hash<gp::hashing::XxHash64>(hashB))
    {
        return false;
    }

    // Verify remaining constexpr algebra, swaps, visitors, iterators, and zero-bit edge case.
    gp::BitSet<130u> algA;
    algA.fill(true);
    algA.flipAll();
    if (!algA.isNoneSet())
    {
        return false;
    }
    (void)algA.set(2u);
    (void)algA.set(129u);
    gp::BitSet<130u> algB;
    (void)algB.set(2u);
    if (!algA.containsAll(algB) || !algA.intersects(algB))
    {
        return false;
    }
    gp::BitSet<130u> algDiff = algA;
    algDiff.subtract(algB);
    if (algDiff.test(2u) || !algDiff.test(129u) || ((algA & algB) != algB))
    {
        return false;
    }
    if ((~(algA | algB)).test(129u) || !((algA ^ algB) == algDiff))
    {
        return false;
    }
    algA.swap(algB);
    if (algA.test(129u) || !algB.test(129u) || (algA >= algB))
    {
        return false;
    }
    if (*algB.beginSetBits() != 2u || *algA.beginZeroBits() != 0u || !algB.begin()[129])
    {
        return false;
    }
    if (gp::BitSet<130u>::fromWords(algB.data(), 3u) != algB || gp::BitSet<0u>{}.findLastZero() != gp::BitSet<0u>::npos)
    {
        return false;
    }

    return true;
}

static_assert(verifyConstexprBitSetSuite(), "Constexpr BitSet operations failed.");

template <typename InSet>
concept HasToUInt64 = requires(const InSet& value) { value.toUInt64(); };

}   // namespace

TEST(BitSetTests, FootprintConceptsAndZeroBitEdgeCase)
{
    static_assert(sizeof(gp::BitSet<1u>) == 8u);
    static_assert(sizeof(gp::BitSet<64u>) == 8u);
    static_assert(sizeof(gp::BitSet<65u>) == 16u);
    static_assert(sizeof(gp::BitSet<128u>) == 16u);
    static_assert(sizeof(gp::BitSet<256u>) == 32u);

    static_assert(gp::BitSet<130u>::kBitCount == 130u);
    static_assert(gp::BitSet<130u>::kBitsPerWord == 64u);
    static_assert(gp::BitSet<130u>::kWordCount == 3u);
    static_assert(gp::BitSet<130u>::npos == static_cast<gp::USize>(-1));

    static_assert(gp::concepts::IsBitSet<gp::BitSet<64u>>);
    static_assert(gp::concepts::IsBitSet<gp::BitSet<0u>>);
    static_assert(!gp::concepts::IsBitSet<gp::Int32>);
    static_assert(gp::concepts::IsTriviallyCopyable<gp::BitSet<130u>>);
    static_assert(gp::concepts::IsStandardLayout<gp::BitSet<130u>>);
    static_assert(gp::concepts::IsRelocatable<gp::BitSet<130u>>);
    static_assert(gp::concepts::IsIterable<gp::BitSet<64u>>);
    static_assert(gp::concepts::IsSizedRange<gp::BitSet<64u>>);
    static_assert(std::random_access_iterator<gp::BitSet<64u>::Iterator>);
    static_assert(std::random_access_iterator<gp::BitSet<64u>::ConstIterator>);
    static_assert(std::forward_iterator<gp::BitSet<64u>::SetBitIterator>);
    static_assert(std::forward_iterator<gp::BitSet<64u>::ZeroBitIterator>);

    gp::BitSet<0u> emptyBits(true);
    EXPECT_TRUE(emptyBits.isEmpty());
    EXPECT_EQ(emptyBits.size(), 0u);
    EXPECT_EQ(emptyBits.capacity(), 0u);
    EXPECT_EQ(emptyBits.wordCount(), 0u);
    EXPECT_EQ(emptyBits.sizeInBytes(), 0u);
    EXPECT_EQ(emptyBits.data(), nullptr);
    EXPECT_EQ(std::as_const(emptyBits).data(), nullptr);
    EXPECT_EQ(emptyBits.toUInt64(), 0ULL);
    EXPECT_EQ(emptyBits.countSetBits(), 0u);
    EXPECT_EQ(emptyBits.countZeroBits(), 0u);
    EXPECT_EQ(emptyBits.countSetBitsInRange(0u, 0u), 0u);
    EXPECT_FALSE(emptyBits.isAnySet());
    EXPECT_TRUE(emptyBits.isAllSet());
    EXPECT_TRUE(emptyBits.isNoneSet());
    EXPECT_EQ(emptyBits.findFirstSet(), gp::BitSet<0u>::npos);
    EXPECT_EQ(emptyBits.findFirstZero(), gp::BitSet<0u>::npos);
    EXPECT_EQ(emptyBits.findLastSet(), gp::BitSet<0u>::npos);
    EXPECT_EQ(emptyBits.findLastZero(), gp::BitSet<0u>::npos);
    EXPECT_EQ(emptyBits.findContiguousZeros(1u), gp::BitSet<0u>::npos);
    EXPECT_EQ(emptyBits.findAndSetFirstZero(), gp::BitSet<0u>::npos);

    gp::BitSet<0u> emptyOther = gp::BitSet<0u>::fromMask(0xFFULL);
    const gp::UInt64 dummyWord = 123ULL;
    emptyOther = gp::BitSet<0u>::fromWords(&dummyWord, 1u);
    emptyBits.setAll();
    emptyBits.clearAll();
    emptyBits.clear();
    emptyBits.fill(true);
    emptyBits.flipAll();
    emptyBits.setRange(0u, 0u);
    emptyBits.clearRange(0u, 0u);
    emptyBits.flipRange(0u, 0u);
    emptyBits &= emptyOther;
    emptyBits |= emptyOther;
    emptyBits ^= emptyOther;
    emptyBits.subtract(emptyOther);
    emptyBits <<= 4u;
    emptyBits >>= 4u;
    EXPECT_TRUE(emptyBits.containsAll(emptyOther));
    EXPECT_FALSE(emptyBits.intersects(emptyOther));
    EXPECT_EQ(emptyBits, emptyOther);
    EXPECT_EQ(emptyBits <=> emptyOther, std::strong_ordering::equal);
    EXPECT_EQ(emptyBits.begin(), emptyBits.end());
    EXPECT_EQ(emptyBits.beginSetBits(), emptyBits.endSetBits());
    EXPECT_EQ(emptyBits.beginZeroBits(), emptyBits.endZeroBits());
    emptyBits.forEachSetBit(
        [](const gp::USize) noexcept -> void
    {
        ADD_FAILURE();
    }
    );
    emptyBits.forEachZeroBit(
        [](const gp::USize) noexcept -> void
    {
        ADD_FAILURE();
    }
    );
    emptyBits.consumeSetBits(
        [](const gp::USize) noexcept -> void
    {
        ADD_FAILURE();
    }
    );
    emptyBits.swap(emptyOther);
    gp::swap(emptyBits, emptyOther);
}

TEST(BitSetTests, ConstructionFactoriesAndWordAccess)
{
    const gp::BitSet<70u> defaultZero;
    EXPECT_TRUE(defaultZero.isNoneSet());
    EXPECT_FALSE(defaultZero.isAnySet());
    EXPECT_FALSE(defaultZero.isAllSet());
    EXPECT_EQ(defaultZero.size(), 70u);
    EXPECT_EQ(defaultZero.capacity(), 70u);
    EXPECT_EQ(defaultZero.wordCount(), 2u);
    EXPECT_EQ(defaultZero.sizeInBytes(), 16u);

    const gp::BitSet<70u> allOnes(true);
    EXPECT_TRUE(allOnes.isAllSet());
    EXPECT_EQ(allOnes.countSetBits(), 70u);
    EXPECT_EQ(allOnes.countZeroBits(), 0u);
    EXPECT_EQ(allOnes.getWord(0u), ~0ULL);
    EXPECT_EQ(allOnes.getWord(1u), 0x3FULL);

    const gp::BitSet<8u> initBits{ true, false, true, true, false, false, true };
    EXPECT_TRUE(initBits.test(0u));
    EXPECT_FALSE(initBits.test(1u));
    EXPECT_TRUE(initBits.test(2u));
    EXPECT_TRUE(initBits.test(3u));
    EXPECT_FALSE(initBits.test(4u));
    EXPECT_FALSE(initBits.test(5u));
    EXPECT_TRUE(initBits.test(6u));
    EXPECT_FALSE(initBits.test(7u));
    EXPECT_EQ(initBits.toUInt64(), 0b01001101ULL);

    const gp::BitSet<5u> masked = gp::BitSet<5u>::fromMask(0xFFFFFFFFFFFFFFFFULL);
    EXPECT_EQ(masked.toUInt64(), 0x1FULL);
    EXPECT_TRUE(masked.isAllSet());

    const gp::UInt64 rawWords[3] = { 0x0123456789ABCDEFULL, 0xFFFFFFFFFFFFFFFFULL, 0xAAAAAAAAAAAAAAAAULL };
    const gp::BitSet<70u> fromTwoWords = gp::BitSet<70u>::fromWords(rawWords, 3u);
    EXPECT_EQ(fromTwoWords.getWord(0u), 0x0123456789ABCDEFULL);
    EXPECT_EQ(fromTwoWords.getWord(1u), 0x3FULL);

    const gp::BitSet<130u> fromPartialWords = gp::BitSet<130u>::fromWords(rawWords, 1u);
    EXPECT_EQ(fromPartialWords.getWord(0u), 0x0123456789ABCDEFULL);
    EXPECT_EQ(fromPartialWords.getWord(1u), 0ULL);
    EXPECT_EQ(fromPartialWords.getWord(2u), 0ULL);

    gp::BitSet<70u> mutWords;
    gp::UInt64* const ptr = mutWords.data();
    ASSERT_NE(ptr, nullptr);
    ptr[0] = 0x55ULL;
    mutWords.setWord(1u, ~0ULL);   // Automatically trims bits >= 70.
    EXPECT_EQ(std::as_const(mutWords).data()[0], 0x55ULL);
    EXPECT_EQ(mutWords.getWord(1u), 0x3FULL);
}

TEST(BitSetTests, SingleBitProxyRangeAndBulkMutations)
{
    gp::BitSet<130u> bs;

    (void)bs.set(3u);
    (void)bs.set(64u, true);
    (void)bs.set(64u, false);
    EXPECT_TRUE(bs.test(3u));
    EXPECT_FALSE(bs.test(64u));

    (void)bs.clearBit(3u);
    EXPECT_FALSE(bs.test(3u));

    (void)bs.flipBit(129u);
    EXPECT_TRUE(bs.test(129u));
    (void)bs.flipBit(129u);
    EXPECT_FALSE(bs.test(129u));

    EXPECT_FALSE(bs.testAndSet(70u));
    EXPECT_TRUE(bs.testAndSet(70u));
    EXPECT_TRUE(bs.testAndClear(70u));
    EXPECT_FALSE(bs.testAndClear(70u));

    // BitReference proxy operations
    bs[5u] = true;
    bs[6u] = bs[5u];
    EXPECT_TRUE(static_cast<bool>(bs[5u]));
    EXPECT_TRUE(static_cast<bool>(bs[6u]));
    EXPECT_FALSE(~bs[6u]);
    bs[6u].flip();
    EXPECT_FALSE(static_cast<bool>(std::as_const(bs)[6u]));
    EXPECT_TRUE(~bs[6u]);

    bs[5u] &= false;
    EXPECT_FALSE(static_cast<bool>(bs[5u]));
    bs[5u] |= true;
    EXPECT_TRUE(static_cast<bool>(bs[5u]));
    bs[5u] ^= true;
    EXPECT_FALSE(static_cast<bool>(bs[5u]));

    bs[10u] = true;
    bs[11u] = false;
    bs[10u].swap(bs[11u]);
    EXPECT_FALSE(bs.test(10u));
    EXPECT_TRUE(bs.test(11u));
    std::ranges::swap(bs[10u], bs[11u]);
    EXPECT_TRUE(bs.test(10u));
    EXPECT_FALSE(bs.test(11u));

    // Range set / clear / flip: intra-word and multi-word spanning middle words.
    bs.clearAll();
    bs.setRange(5u, 10u);
    EXPECT_EQ(bs.countSetBits(), 10u);
    EXPECT_EQ(bs.countSetBitsInRange(5u, 10u), 10u);
    bs.clearRange(7u, 6u);
    EXPECT_EQ(bs.countSetBits(), 4u);
    bs.flipRange(6u, 8u);
    EXPECT_EQ(bs.countSetBitsInRange(5u, 10u), 8u);

    bs.clear();
    bs.setRange(10u, 118u);   // Spans word 0, full word 1, and partial word 2.
    EXPECT_EQ(bs.countSetBits(), 118u);
    EXPECT_EQ(bs.countSetBitsInRange(0u, 130u), 118u);
    EXPECT_EQ(bs.countSetBitsInRange(64u, 64u), 64u);
    bs.clearRange(20u, 90u);
    EXPECT_EQ(bs.countSetBits(), 28u);
    bs.flipRange(20u, 90u);
    EXPECT_EQ(bs.countSetBits(), 118u);

    bs.fill(true);
    EXPECT_TRUE(bs.isAllSet());
    bs.flipAll();
    EXPECT_TRUE(bs.isNoneSet());
    bs.fill(false);
    EXPECT_TRUE(bs.isNoneSet());
}

TEST(BitSetTests, BitScansContiguousZerosAndSetAlgebra)
{
    gp::BitSet<130u> bs;
    (void)bs.set(10u);
    (void)bs.set(64u);
    (void)bs.set(129u);

    EXPECT_EQ(bs.findFirstSet(), 10u);
    EXPECT_EQ(bs.findFirstSet(10u), 10u);
    EXPECT_EQ(bs.findFirstSet(11u), 64u);
    EXPECT_EQ(bs.findFirstSet(65u), 129u);
    EXPECT_EQ(bs.findFirstSet(130u), gp::BitSet<130u>::npos);
    EXPECT_EQ(bs.findLastSet(), 129u);

    EXPECT_EQ(bs.findFirstZero(), 0u);
    bs.setRange(0u, 10u);
    EXPECT_EQ(bs.findFirstZero(), 11u);
    EXPECT_EQ(bs.findLastZero(), 128u);

    bs.fill(true);
    EXPECT_EQ(bs.findFirstZero(), gp::BitSet<130u>::npos);
    EXPECT_EQ(bs.findLastZero(), gp::BitSet<130u>::npos);
    EXPECT_EQ(bs.findAndSetFirstZero(), gp::BitSet<130u>::npos);

    // Contiguous zero run across word boundaries
    bs.fill(true);
    bs.clearRange(5u, 4u);     // 4 zeros at [5, 9)
    bs.clearRange(60u, 70u);   // 70 zeros at [60, 130) spanning word 0 -> word 1 -> word 2
    EXPECT_EQ(bs.findContiguousZeros(0u), 0u);
    EXPECT_EQ(bs.findContiguousZeros(4u), 5u);
    EXPECT_EQ(bs.findContiguousZeros(5u), 60u);
    EXPECT_EQ(bs.findContiguousZeros(70u), 60u);
    EXPECT_EQ(bs.findContiguousZeros(71u), gp::BitSet<130u>::npos);
    EXPECT_EQ(bs.findContiguousZeros(10u, 65u), 65u);

    EXPECT_EQ(bs.findAndSetFirstZero(), 5u);
    EXPECT_TRUE(bs.test(5u));
    EXPECT_EQ(bs.findAndSetFirstZero(60u), 60u);
    EXPECT_TRUE(bs.test(60u));

    // Set algebra & shifts
    gp::BitSet<130u> a;
    gp::BitSet<130u> b;
    (void)a.set(1u);
    (void)a.set(65u);
    (void)a.set(129u);
    (void)b.set(1u);
    (void)b.set(100u);

    EXPECT_TRUE(a.intersects(b));
    EXPECT_FALSE(a.containsAll(b));

    const gp::BitSet<130u> unionSet = a | b;
    EXPECT_TRUE(unionSet.containsAll(a));
    EXPECT_TRUE(unionSet.containsAll(b));
    EXPECT_EQ(unionSet.countSetBits(), 4u);

    const gp::BitSet<130u> interSet = a & b;
    EXPECT_EQ(interSet.countSetBits(), 1u);
    EXPECT_TRUE(interSet.test(1u));

    const gp::BitSet<130u> xorSet = a ^ b;
    EXPECT_EQ(xorSet.countSetBits(), 3u);
    EXPECT_FALSE(xorSet.test(1u));
    EXPECT_TRUE(xorSet.test(65u));
    EXPECT_TRUE(xorSet.test(100u));
    EXPECT_TRUE(xorSet.test(129u));

    const gp::BitSet<130u> inverted = ~a;
    EXPECT_EQ(inverted.countSetBits(), 127u);
    EXPECT_FALSE(inverted.test(1u));
    EXPECT_FALSE(inverted.test(65u));
    EXPECT_FALSE(inverted.test(129u));

    gp::BitSet<130u> diff = a;
    diff.subtract(b);
    EXPECT_EQ(diff.countSetBits(), 2u);
    EXPECT_FALSE(diff.test(1u));
    EXPECT_TRUE(diff.test(65u));
    diff.subtract(diff);
    EXPECT_TRUE(diff.isNoneSet());

    gp::BitSet<130u> selfXor = a;
    selfXor ^= selfXor;
    EXPECT_TRUE(selfXor.isNoneSet());

    gp::BitSet<130u> disjoint;
    (void)disjoint.set(2u);
    EXPECT_FALSE(a.intersects(disjoint));

    // Left / right shifts across word boundaries
    gp::BitSet<130u> s;
    (void)s.set(0u);
    (void)s.set(63u);
    (void)s.set(129u);
    const gp::BitSet<130u> sLeft5 = s << 5u;
    EXPECT_FALSE(sLeft5.test(0u));
    EXPECT_TRUE(sLeft5.test(5u));
    EXPECT_TRUE(sLeft5.test(68u));
    EXPECT_EQ(sLeft5.countSetBits(), 2u);   // bit 129 shifted past 130

    const gp::BitSet<130u> sLeft64 = s << 64u;
    EXPECT_TRUE(sLeft64.test(64u));
    EXPECT_TRUE(sLeft64.test(127u));
    EXPECT_EQ(sLeft64.countSetBits(), 2u);

    const gp::BitSet<130u> sRight65 = s >> 65u;
    EXPECT_TRUE(sRight65.test(64u));
    EXPECT_EQ(sRight65.countSetBits(), 1u);

    gp::BitSet<130u> sZero = s;
    sZero <<= 130u;
    EXPECT_TRUE(sZero.isNoneSet());
    sZero = s;
    sZero >>= 130u;
    EXPECT_TRUE(sZero.isNoneSet());
}

TEST(BitSetTests, IteratorsVisitorsHashingSwapAndComparisons)
{
    gp::BitSet<70u> bs;
    (void)bs.set(0u);
    (void)bs.set(17u);
    (void)bs.set(63u);
    (void)bs.set(64u);
    (void)bs.set(69u);

    gp::USize setIndices[5]{};
    gp::USize count = 0u;
    for (const gp::USize idx: bs.setBits())
    {
        setIndices[count++] = idx;
    }
    ASSERT_EQ(count, 5u);
    EXPECT_EQ(setIndices[0], 0u);
    EXPECT_EQ(setIndices[1], 17u);
    EXPECT_EQ(setIndices[2], 63u);
    EXPECT_EQ(setIndices[3], 64u);
    EXPECT_EQ(setIndices[4], 69u);

    gp::BitSet<70u>::SetBitIterator setIt = bs.beginSetBits(18u);
    ASSERT_NE(setIt, bs.endSetBits());
    EXPECT_EQ(*setIt, 63u);
    ++setIt;
    EXPECT_EQ(*setIt++, 64u);
    EXPECT_EQ(*setIt, 69u);

    gp::USize zeroCount = 0u;
    for (const gp::USize idx: bs.zeroBits())
    {
        EXPECT_FALSE(bs.test(idx));
        ++zeroCount;
    }
    EXPECT_EQ(zeroCount, 65u);
    EXPECT_EQ(*bs.beginZeroBits(68u), 68u);
    EXPECT_EQ(bs.beginZeroBits(69u), bs.endZeroBits());

    gp::USize foreachSetSum = 0u;
    bs.forEachSetBit(
        [&foreachSetSum](const gp::USize idx) noexcept -> void
    {
        foreachSetSum += idx;
    }
    );
    EXPECT_EQ(foreachSetSum, 0u + 17u + 63u + 64u + 69u);

    gp::USize foreachZeroCount = 0u;
    bs.forEachZeroBit(
        [&foreachZeroCount](const gp::USize) noexcept -> void
    {
        ++foreachZeroCount;
    }
    );
    EXPECT_EQ(foreachZeroCount, 65u);

    // Random-access BitIterator & reverse iteration
    EXPECT_EQ(bs.end() - bs.begin(), 70);
    EXPECT_EQ(bs.cend() - bs.cbegin(), 70);
    EXPECT_EQ(bs.rend() - bs.rbegin(), 70);
    EXPECT_EQ(bs.crend() - bs.crbegin(), 70);
    EXPECT_TRUE(bs.begin()[17]);
    EXPECT_TRUE(*(std::as_const(bs).begin() + 69));
    EXPECT_TRUE(*bs.rbegin());
    EXPECT_TRUE(*bs.crbegin());

    gp::BitSet<70u>::Iterator mutIt = bs.begin() + 1;
    *mutIt = true;
    EXPECT_TRUE(bs.test(1u));
    *mutIt = false;
    EXPECT_FALSE(bs.test(1u));
    --mutIt;
    EXPECT_EQ(mutIt, bs.begin());
    EXPECT_EQ((17 + bs.cbegin())[0], true);

    // consumeSetBits visits and clears
    gp::BitSet<70u> copyForConsume = bs;
    gp::USize consumedCount = 0u;
    copyForConsume.consumeSetBits(
        [&consumedCount](const gp::USize) noexcept -> void
    {
        ++consumedCount;
    }
    );
    EXPECT_EQ(consumedCount, 5u);
    EXPECT_TRUE(copyForConsume.isNoneSet());

    // Unified gp::hashing::hash API
    gp::BitSet<70u> same(bs);
    gp::BitSet<70u> different(bs);
    (void)different.flipBit(40u);

    EXPECT_EQ(gp::hashing::hash(bs), gp::hashing::hash(same));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::XxHash64>(bs), gp::hashing::hash<gp::hashing::XxHash64>(same));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::Crc32>(bs), gp::hashing::hash<gp::hashing::Crc32>(same));
    EXPECT_EQ(gp::hashing::hash<gp::hashing::Sha256>(bs), gp::hashing::hash<gp::hashing::Sha256>(same));
    EXPECT_EQ(
        gp::hashing::hash(gp::hashing::HashAlgorithm::Wyhash64, bs),
        gp::hashing::hash(gp::hashing::HashAlgorithm::Wyhash64, same)
    );
    EXPECT_NE(gp::hashing::hash(bs), gp::hashing::hash(different));

    // Comparisons & swap
    gp::BitSet<70u> low;
    (void)low.set(5u);
    gp::BitSet<70u> high;
    (void)high.set(1u);   // Index 1 differs first; high[1] == 1 > low[1] == 0.
    EXPECT_LT(low, high);
    EXPECT_GT(high, low);
    EXPECT_NE(low, high);

    gp::swap(low, high);
    EXPECT_TRUE(low.test(1u));
    EXPECT_TRUE(high.test(5u));
    low.swap(low);
    EXPECT_TRUE(low.test(1u));
}

TEST(BitSetTests, WordBoundaryEdgesMultiWordSkipsRangesAndHashing)
{
    static_assert(sizeof(gp::BitSet<0u>) == 8u);
    static_assert(gp::concepts::IsTriviallyCopyable<gp::BitSet<0u>>);
    static_assert(gp::concepts::IsStandardLayout<gp::BitSet<0u>>);

    // Exact word-multiple vs 63 / 65 / 128 / 129 boundaries and mask clipping.
    const gp::BitSet<63u> clipped63 = gp::BitSet<63u>::fromMask(~0ULL);
    EXPECT_TRUE(clipped63.isAllSet());
    EXPECT_EQ(clipped63.countSetBits(), 63u);
    EXPECT_EQ(clipped63.toUInt64(), 0x7FFFFFFFFFFFFFFFULL);

    const gp::BitSet<64u> exact64 = gp::BitSet<64u>::fromMask(~0ULL);
    EXPECT_TRUE(exact64.isAllSet());
    EXPECT_EQ(exact64.countSetBits(), 64u);
    EXPECT_EQ(exact64.toUInt64(), ~0ULL);

    const gp::BitSet<65u> partial65 = gp::BitSet<65u>::fromMask(~0ULL);
    EXPECT_FALSE(partial65.isAllSet());
    EXPECT_EQ(partial65.countSetBits(), 64u);
    EXPECT_FALSE(partial65.test(64u));
    EXPECT_EQ(partial65.getWord(1u), 0ULL);

    gp::BitSet<128u> exact128(true);
    EXPECT_EQ(exact128.wordCount(), 2u);
    EXPECT_EQ(exact128.getWord(0u), ~0ULL);
    EXPECT_EQ(exact128.getWord(1u), ~0ULL);
    EXPECT_EQ(exact128.countSetBits(), 128u);

    const gp::BitSet<129u> partial129(true);
    EXPECT_EQ(partial129.wordCount(), 3u);
    EXPECT_EQ(partial129.getWord(2u), 1ULL);
    EXPECT_EQ(partial129.countSetBits(), 129u);

    gp::BitSet<130u> wordCheck;
    (void)wordCheck.set(3u);
    (void)wordCheck.set(63u);
    (void)wordCheck.set(64u);
    (void)wordCheck.set(129u);
    EXPECT_EQ(wordCheck.getWord(0u), (1ULL << 3u) | (1ULL << 63u));
    static_assert(!HasToUInt64<gp::BitSet<65u>>);
    static_assert(HasToUInt64<gp::BitSet<64u>>);
    EXPECT_TRUE(gp::BitSet<65u>::fromWords(nullptr, 2u).isNoneSet());

    // Exact aligned & cross-boundary ranges on 3 full words.
    gp::BitSet<192u> spans;
    spans.setRange(64u, 0u);
    EXPECT_TRUE(spans.isNoneSet());
    spans.setRange(64u, 64u);
    EXPECT_EQ(spans.getWord(0u), 0ULL);
    EXPECT_EQ(spans.getWord(1u), ~0ULL);
    EXPECT_EQ(spans.getWord(2u), 0ULL);
    EXPECT_EQ(spans.countSetBitsInRange(64u, 64u), 64u);
    EXPECT_EQ(spans.countSetBitsInRange(0u, 192u), 64u);

    spans.clearRange(64u, 64u);
    EXPECT_TRUE(spans.isNoneSet());
    spans.setRange(63u, 2u);
    EXPECT_TRUE(spans.test(63u));
    EXPECT_TRUE(spans.test(64u));
    EXPECT_EQ(spans.countSetBits(), 2u);

    spans.flipRange(0u, 192u);
    EXPECT_EQ(spans.countSetBits(), 190u);
    EXPECT_FALSE(spans.test(63u));
    EXPECT_FALSE(spans.test(64u));

    // Multi-word empty/full skips for first/last scans and contiguous zeros.
    gp::BitSet<256u> sparse;
    EXPECT_EQ(sparse.findFirstSet(), gp::BitSet<256u>::npos);
    EXPECT_EQ(sparse.findLastSet(), gp::BitSet<256u>::npos);
    (void)sparse.set(200u);
    EXPECT_EQ(sparse.findFirstSet(), 200u);
    EXPECT_EQ(sparse.findLastSet(), 200u);
    EXPECT_EQ(sparse.findFirstSet(201u), gp::BitSet<256u>::npos);

    gp::BitSet<256u> dense(true);
    EXPECT_EQ(dense.findFirstZero(), gp::BitSet<256u>::npos);
    EXPECT_EQ(dense.findLastZero(), gp::BitSet<256u>::npos);
    (void)dense.clearBit(200u);
    EXPECT_EQ(dense.findFirstZero(), 200u);
    EXPECT_EQ(dense.findLastZero(), 200u);
    EXPECT_EQ(dense.findFirstZero(201u), gp::BitSet<256u>::npos);

    // Zero run starting inside word 0, crossing full empty word 1, ending in word 2.
    dense.fill(true);
    dense.clearRange(50u, 100u);
    EXPECT_EQ(dense.findContiguousZeros(100u), 50u);
    EXPECT_EQ(dense.findContiguousZeros(101u), gp::BitSet<256u>::npos);
    EXPECT_EQ(dense.findContiguousZeros(0u, 256u), gp::BitSet<256u>::npos);
    EXPECT_EQ(dense.findContiguousZeros(300u), gp::BitSet<256u>::npos);

    // Multi-word iterator skips, forward multipass guarantee, and default sentinels.
    gp::BitSet<256u> iterBits;
    (void)iterBits.set(1u);
    (void)iterBits.set(250u);
    gp::BitSet<256u>::SetBitIterator setItA = iterBits.beginSetBits();
    gp::BitSet<256u>::SetBitIterator setItB = setItA;
    ASSERT_NE(setItA, iterBits.endSetBits());
    EXPECT_EQ(*setItA, 1u);
    ++setItA;
    EXPECT_EQ(*setItA, 250u);
    ++setItA;
    EXPECT_EQ(setItA, iterBits.endSetBits());
    EXPECT_EQ(*setItB, 1u);
    ++setItB;
    EXPECT_EQ(*setItB, 250u);
    EXPECT_EQ(*iterBits.beginSetBits(64u), 250u);
    EXPECT_EQ(gp::BitSet<256u>::SetBitIterator{}, gp::BitSet<256u>::SetBitIterator{});
    EXPECT_EQ(gp::BitSet<256u>::ZeroBitIterator{}, gp::BitSet<256u>::ZeroBitIterator{});

    gp::BitSet<256u> almostFull(true);
    (void)almostFull.clearBit(2u);
    (void)almostFull.clearBit(249u);
    gp::BitSet<256u>::ZeroBitIterator zeroIt = almostFull.beginZeroBits();
    ASSERT_NE(zeroIt, almostFull.endZeroBits());
    EXPECT_EQ(*zeroIt++, 2u);
    EXPECT_EQ(*zeroIt++, 249u);
    EXPECT_EQ(zeroIt, almostFull.endZeroBits());

    // Aligned and cross-word shifts, including shift == 0 identity.
    gp::BitSet<192u> sh;
    (void)sh.set(0u);
    (void)sh.set(63u);
    EXPECT_EQ(sh << 0u, sh);
    EXPECT_EQ(sh >> 0u, sh);

    const gp::BitSet<192u> shWord1 = sh << 64u;
    EXPECT_EQ(shWord1.getWord(0u), 0ULL);
    EXPECT_TRUE(shWord1.test(64u));
    EXPECT_TRUE(shWord1.test(127u));
    const gp::BitSet<192u> shWord2 = sh << 128u;
    EXPECT_TRUE(shWord2.test(128u));
    EXPECT_TRUE(shWord2.test(191u));
    EXPECT_EQ(shWord2 >> 128u, sh);

    // STL <ranges> compatibility & multi-word lexicographical ordering.
    EXPECT_EQ(std::ranges::count(iterBits, true), 2);
    EXPECT_EQ(std::ranges::count(iterBits, false), 254);
    const gp::BitSet<256u>::SetBitRange setView = iterBits.setBits();
    EXPECT_NE(std::ranges::find(setView, 250u), setView.end());
    const gp::BitSet<256u> iterCopy(iterBits);
    EXPECT_TRUE(std::ranges::equal(iterBits, iterCopy));

    gp::BitSet<256u> iterHigher(iterBits);
    (void)iterHigher.set(255u);
    EXPECT_LT(iterBits, iterHigher);

    // Zero-bit complement and size-framed hash distinctness.
    const gp::BitSet<0u> emptyBitSet;
    EXPECT_EQ(~emptyBitSet, emptyBitSet);
    EXPECT_EQ(gp::hashing::hash(emptyBitSet), gp::hashing::hash(gp::BitSet<0u>{}));

    const gp::BitSet<64u> zero64;
    const gp::BitSet<65u> zero65;
    EXPECT_NE(gp::hashing::hash(zero64), gp::hashing::hash(zero65));
    EXPECT_NE(gp::hashing::hash<gp::hashing::XxHash64>(zero64), gp::hashing::hash<gp::hashing::XxHash64>(zero65));
    EXPECT_NE(gp::hashing::hash<gp::hashing::Crc32>(zero64), gp::hashing::hash<gp::hashing::Crc32>(zero65));
    EXPECT_NE(gp::hashing::hash<gp::hashing::Sha256>(zero64), gp::hashing::hash<gp::hashing::Sha256>(zero65));

    // Unified hashOf, hashValues, and DynamicHasher consistency.
    const gp::BitSet<64u> maskCopy(exact64);
    EXPECT_EQ(gp::hashing::hashOf(exact64), gp::hashing::hash(maskCopy));
    EXPECT_EQ(gp::hashing::hashValues(exact64, partial65), gp::hashing::hashValues(maskCopy, partial65));
    gp::hashing::DynamicHasher dynA(gp::hashing::HashAlgorithm::XxHash64);
    gp::hashing::DynamicHasher dynB(gp::hashing::HashAlgorithm::XxHash64);
    exact64.hashAppend(dynA);
    maskCopy.hashAppend(dynB);
    EXPECT_EQ(dynA.finalize64(), dynB.finalize64());

    // Distinct BitSet containers must never compare iterators equal even with matching bits.
    gp::BitSet<64u> separateA;
    gp::BitSet<64u> separateB;
    (void)separateA.set(0u);
    (void)separateB.set(0u);
    EXPECT_NE(separateA.beginSetBits(), separateB.beginSetBits());

    // consumeSetBits clears words up front so callbacks can safely re-flag dirty slots in the same word.
    gp::BitSet<64u> requeueMask;
    (void)requeueMask.set(1u);
    (void)requeueMask.set(10u);
    requeueMask.consumeSetBits(
        [&requeueMask](const gp::USize bitIdx) noexcept -> void
    {
        if (bitIdx == 1u)
        {
            (void)requeueMask.set(20u);
        }
    }
    );
    EXPECT_FALSE(requeueMask.test(1u));
    EXPECT_FALSE(requeueMask.test(10u));
    EXPECT_TRUE(requeueMask.test(20u));
    EXPECT_EQ(requeueMask.countSetBits(), 1u);
}

}   // namespace gp::tests
