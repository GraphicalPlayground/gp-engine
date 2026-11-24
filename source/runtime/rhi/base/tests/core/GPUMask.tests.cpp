// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "core/GPUMask.hpp"
#include "hashing/Fnv1a.hpp"
#include "hashing/HashOf.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::rhi::tests
{

static_assert(sizeof(GPUMask) == sizeof(gp::UInt32), "GPUMask must remain a 4-byte bitmask.");
static_assert(alignof(GPUMask) == alignof(gp::UInt32), "GPUMask must remain 4-byte aligned.");
static_assert(std::is_trivially_copyable_v<GPUMask>, "GPUMask must be trivially copyable.");
static_assert(GPUMask::kMaxGPUs == 32u, "GPUMask::kMaxGPUs must equal 32.");

static_assert(GPUMask{} == GPUMask::gpu0(), "Default-constructed GPUMask must select GPU 0.");
static_assert(GPUMask::gpu0().getNative() == 1u, "GPUMask::gpu0() must set bit 0.");
static_assert(GPUMask::all() == GPUMask::gpu0(), "GPUMask::all() must match gpu0() in single-GPU mode.");
static_assert(GPUMask::fromIndex(3u).getNative() == (1u << 3u), "fromIndex(3) must set bit 3.");
static_assert(!GPUMask(0u).isValid(), "Zero mask must report invalid.");
static_assert(GPUMask::gpu0().isValid(), "gpu0() must report valid.");
static_assert(GPUMask::fromIndex(5u).hasSingleIndex(), "Single-bit mask must report hasSingleIndex().");
static_assert(
    !(GPUMask::fromIndex(0u) | GPUMask::fromIndex(1u)).hasSingleIndex(), "Two-bit mask has multiple indices."
);
static_assert(
    (GPUMask::fromIndex(4u) | GPUMask::fromIndex(9u)).getFirstIndex() == 4u,
    "getFirstIndex() must return the lowest set bit."
);
static_assert(GPUMask(0u).getFirstIndex() == 0u, "getFirstIndex() on empty mask must return 0.");
static_assert(
    (GPUMask::fromIndex(1u) | GPUMask::fromIndex(7u) | GPUMask::fromIndex(31u)).getNumGPUs() == 3u,
    "getNumGPUs() must count set bits."
);
static_assert(
    gp::hashing::hashOf(GPUMask::fromIndex(2u)) == GPUMask::fromIndex(2u).getHash(),
    "hashOf must dispatch to GPUMask::getHash()."
);

TEST(GPUMaskTest, DefaultAndFactoryConstructors)
{
    const GPUMask defaultMask{};
    const GPUMask gpu0 = GPUMask::gpu0();
    const GPUMask allMask = GPUMask::all();
    const GPUMask emptyMask(0u);

    EXPECT_EQ(defaultMask, gpu0);
    EXPECT_EQ(defaultMask.getNative(), 1u);
    EXPECT_TRUE(defaultMask.isValid());
    EXPECT_TRUE(defaultMask.hasSingleIndex());
    EXPECT_EQ(defaultMask.getFirstIndex(), 0u);
    EXPECT_EQ(defaultMask.getNumGPUs(), 1u);

    EXPECT_EQ(allMask, gpu0);
    EXPECT_EQ(allMask.getNative(), 1u);
    EXPECT_TRUE(allMask.isValid());
    EXPECT_TRUE(allMask.contains(0u));

    EXPECT_EQ(emptyMask.getNative(), 0u);
    EXPECT_FALSE(emptyMask.isValid());
    EXPECT_FALSE(emptyMask.hasSingleIndex());
    EXPECT_EQ(emptyMask.getFirstIndex(), 0u);
    EXPECT_EQ(emptyMask.getNumGPUs(), 0u);
}

TEST(GPUMaskTest, FromIndexAndSingleBitQueries)
{
    for (gp::UInt32 gpuIndex = 0u; gpuIndex < GPUMask::kMaxGPUs; ++gpuIndex)
    {
        const GPUMask mask = GPUMask::fromIndex(gpuIndex);
        EXPECT_EQ(mask.getNative(), 1u << gpuIndex);
        EXPECT_TRUE(mask.isValid());
        EXPECT_TRUE(mask.hasSingleIndex());
        EXPECT_EQ(mask.getFirstIndex(), gpuIndex);
        EXPECT_EQ(mask.getNumGPUs(), 1u);
        EXPECT_TRUE(mask.contains(gpuIndex));

        const gp::UInt32 otherIndex = (gpuIndex + 1u) % GPUMask::kMaxGPUs;
        EXPECT_FALSE(mask.contains(otherIndex));
    }

    // Out-of-range GPU indices must safely return false.
    const GPUMask fullMask(0xFFFFFFFFu);
    EXPECT_FALSE(fullMask.contains(GPUMask::kMaxGPUs));
    EXPECT_FALSE(fullMask.contains(64u));
    EXPECT_FALSE(fullMask.contains(0xFFFFFFFFu));
}

TEST(GPUMaskTest, ContainsAllAndMultiGPUQueries)
{
    const GPUMask empty(0u);
    const GPUMask gpu0 = GPUMask::fromIndex(0u);
    const GPUMask gpu2 = GPUMask::fromIndex(2u);
    const GPUMask gpu5 = GPUMask::fromIndex(5u);
    const GPUMask gpu31 = GPUMask::fromIndex(31u);

    const GPUMask subset = gpu2 | gpu5;
    const GPUMask superset = gpu0 | gpu2 | gpu5 | gpu31;

    EXPECT_TRUE(superset.isValid());
    EXPECT_FALSE(superset.hasSingleIndex());
    EXPECT_EQ(superset.getFirstIndex(), 0u);
    EXPECT_EQ(superset.getNumGPUs(), 4u);

    EXPECT_EQ(subset.getFirstIndex(), 2u);
    EXPECT_EQ(subset.getNumGPUs(), 2u);
    EXPECT_FALSE(subset.hasSingleIndex());

    EXPECT_TRUE(superset.containsAll(subset));
    EXPECT_TRUE(superset.containsAll(superset));
    EXPECT_TRUE(superset.containsAll(empty));
    EXPECT_TRUE(empty.containsAll(empty));

    EXPECT_FALSE(subset.containsAll(superset));
    EXPECT_FALSE(subset.containsAll(gpu0));
    EXPECT_FALSE(empty.containsAll(gpu0));

    const GPUMask all32(0xFFFFFFFFu);
    EXPECT_TRUE(all32.isValid());
    EXPECT_FALSE(all32.hasSingleIndex());
    EXPECT_EQ(all32.getFirstIndex(), 0u);
    EXPECT_EQ(all32.getNumGPUs(), 32u);
    EXPECT_TRUE(all32.containsAll(superset));
}

TEST(GPUMaskTest, BitwiseOperatorsAndEquality)
{
    const GPUMask maskA = GPUMask::fromIndex(1u) | GPUMask::fromIndex(3u);
    const GPUMask maskB = GPUMask::fromIndex(3u) | GPUMask::fromIndex(6u);

    const GPUMask unionMask = maskA | maskB;
    EXPECT_EQ(unionMask.getNative(), (1u << 1u) | (1u << 3u) | (1u << 6u));
    EXPECT_EQ(unionMask.getNumGPUs(), 3u);
    EXPECT_NE(unionMask, maskA);
    EXPECT_NE(unionMask, maskB);

    const GPUMask intersectMask = maskA & maskB;
    EXPECT_EQ(intersectMask, GPUMask::fromIndex(3u));
    EXPECT_TRUE(intersectMask.hasSingleIndex());

    const GPUMask disjointIntersect = GPUMask::fromIndex(0u) & GPUMask::fromIndex(4u);
    EXPECT_EQ(disjointIntersect, GPUMask(0u));
    EXPECT_FALSE(disjointIntersect.isValid());

    GPUMask mutableMask = maskA;
    GPUMask& orResult = (mutableMask |= maskB);
    EXPECT_EQ(&orResult, &mutableMask);
    EXPECT_EQ(mutableMask, unionMask);

    GPUMask& andResult = (mutableMask &= maskB);
    EXPECT_EQ(&andResult, &mutableMask);
    EXPECT_EQ(mutableMask, maskB);
}

TEST(GPUMaskTest, HashingAndStreamingHashAppend)
{
    const GPUMask maskA1 = GPUMask::fromIndex(0u) | GPUMask::fromIndex(2u);
    const GPUMask maskA2(0b101u);
    const GPUMask maskB = GPUMask::fromIndex(1u) | GPUMask::fromIndex(2u);
    const GPUMask emptyMask(0u);

    EXPECT_EQ(maskA1, maskA2);
    EXPECT_EQ(maskA1.getHash(), maskA2.getHash());
    EXPECT_EQ(maskA1.getHash(), gp::hashing::hashOf(maskA1.getNative()));
    EXPECT_EQ(gp::hashing::hashOf(maskA1), maskA1.getHash());

    EXPECT_NE(maskA1.getHash(), maskB.getHash());
    EXPECT_NE(maskA1.getHash(), emptyMask.getHash());

    gp::hashing::Fnv1a64 hasherA1;
    maskA1.hashAppend(hasherA1);
    gp::hashing::Fnv1a64 hasherA2;
    maskA2.hashAppend(hasherA2);
    gp::hashing::Fnv1a64 hasherRaw;
    hasherRaw.append(maskA1.getNative());
    gp::hashing::Fnv1a64 hasherB;
    maskB.hashAppend(hasherB);

    EXPECT_EQ(hasherA1.finalize(), hasherA2.finalize());
    EXPECT_EQ(hasherA1.finalize(), hasherRaw.finalize());
    EXPECT_NE(hasherA1.finalize(), hasherB.finalize());
}

TEST(GPUMaskTest, IndexRangeIterationInvertFilterAndIntersects)
{
    const GPUMask gpu0 = GPUMask::gpu0();
    const GPUMask gpu3 = GPUMask::fromIndex(3u);
    const GPUMask gpu7 = GPUMask::fromIndex(7u);
    const GPUMask multi = gpu0 | gpu3 | gpu7;

    EXPECT_EQ(gpu3.toIndex(), 3u);
    EXPECT_EQ(multi.getFirstIndex(), 0u);
    EXPECT_EQ(multi.getLastIndex(), 7u);
    EXPECT_EQ(GPUMask(0u).getLastIndex(), 0u);
    EXPECT_EQ(multi.getNumActive(), 3u);
    EXPECT_EQ(multi.getForDisplay(), multi.getNative());

    EXPECT_TRUE(multi.intersects(gpu3));
    EXPECT_FALSE(gpu0.intersects(gpu3));

    EXPECT_EQ(GPUMask::filterGPUsBefore(0u), GPUMask::all());
    EXPECT_EQ(GPUMask::filterGPUsBefore(1u), GPUMask(0u));
    EXPECT_EQ(GPUMask::filterGPUsBefore(GPUMask::kMaxGPUs), GPUMask(0u));

    GPUMask inverse{};
    EXPECT_FALSE(GPUMask::all().invert(inverse));
    EXPECT_EQ(inverse, GPUMask::gpu0());

    const GPUMask universe = gpu0 | gpu3 | gpu7;
    EXPECT_TRUE(gpu3.invert(inverse, universe));
    EXPECT_EQ(inverse, gpu0 | gpu7);

    gp::UInt32 visited[4]{};
    gp::UInt32 visitedCount = 0u;
    for (const gp::UInt32 gpuIndex: multi)
    {
        visited[visitedCount++] = gpuIndex;
    }
    ASSERT_EQ(visitedCount, 3u);
    EXPECT_EQ(visited[0], 0u);
    EXPECT_EQ(visited[1], 3u);
    EXPECT_EQ(visited[2], 7u);

    GPUMask::Iterator it = begin(gpu3);
    EXPECT_TRUE(static_cast<bool>(it));
    EXPECT_FALSE(!it);
    EXPECT_EQ(*it, 3u);
    GPUMask::Iterator post = it++;
    EXPECT_EQ(*post, 3u);
    EXPECT_TRUE(!it);
    EXPECT_EQ(it, end(gpu3));

    // Incrementing an exhausted iterator must remain a safe no-op at end().
    ++it;
    EXPECT_EQ(it, end(gpu3));
    EXPECT_TRUE(!it);

    // Iterators over distinct masks sitting on the same first bit must not compare equal.
    const GPUMask mask0And2(0b0101u);
    const GPUMask mask0Only(0b0001u);
    EXPECT_NE(begin(mask0And2), begin(mask0Only));
    EXPECT_EQ(*begin(mask0And2), *begin(mask0Only));
    EXPECT_EQ(GPUMask::Iterator(0b0101u), begin(mask0And2));

    // Empty mask range-for emits zero elements and begin() == end().
    const GPUMask emptyMask(0u);
    EXPECT_EQ(begin(emptyMask), end(emptyMask));
    EXPECT_TRUE(!begin(emptyMask));
    gp::UInt32 emptyCount = 0u;
    for ([[maybe_unused]] const gp::UInt32 idx: emptyMask)
    {
        ++emptyCount;
    }
    EXPECT_EQ(emptyCount, 0u);

    // Inverting empty mask against a universe returns the full universe.
    EXPECT_TRUE(emptyMask.invert(inverse, universe));
    EXPECT_EQ(inverse, universe);

    // Bit 31 boundary: toIndex(), getLastIndex(), and iteration across full 32-bit mask.
    const GPUMask gpu31 = GPUMask::fromIndex(31u);
    EXPECT_EQ(gpu31.toIndex(), 31u);
    EXPECT_EQ(gpu31.getLastIndex(), 31u);
    const GPUMask full32(0xFFFFFFFFu);
    EXPECT_EQ(full32.getLastIndex(), 31u);
    gp::UInt32 fullCount = 0u;
    gp::UInt32 lastVisited = 0u;
    for (const gp::UInt32 idx: full32)
    {
        EXPECT_EQ(idx, fullCount);
        lastVisited = idx;
        ++fullCount;
    }
    EXPECT_EQ(fullCount, 32u);
    EXPECT_EQ(lastVisited, 31u);
}

}   // namespace gp::rhi::tests
