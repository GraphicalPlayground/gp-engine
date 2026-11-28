// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanResourceTable.hpp"
#include "containers/views/VectorView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Collects the `(resourceIndex, bindIndex)` pairs an enumeration visits.
struct VisitedEntries final
{
    gp::UInt32 resourceIndices[8]{};
    gp::UInt32 bindIndices[8]{};
    gp::UInt32 count{ 0u };
};

}   // namespace

static_assert(encodeVulkanResourceTableEntry(3u, 0x1234u, 7u) == 0x03123407u);
static_assert(getVulkanResourceTableUniformBufferIndex(0x03123407u) == 3u);
static_assert(getVulkanResourceTableResourceIndex(0x03123407u) == 0x1234u);
static_assert(getVulkanResourceTableBindIndex(0x03123407u) == 7u);
static_assert(getVulkanResourceTableUniformBufferIndex(kVulkanResourceTableEndOfTable) == 0xFFu);

TEST(VulkanResourceTableTest, EncodingMasksOversizedFields)
{
    const gp::UInt32 entry = encodeVulkanResourceTableEntry(0x1FFu, 0x1FFFFu, 0x1FFu);
    EXPECT_EQ(getVulkanResourceTableUniformBufferIndex(entry), 0xFFu);
    EXPECT_EQ(getVulkanResourceTableResourceIndex(entry), 0xFFFFu);
    EXPECT_EQ(getVulkanResourceTableBindIndex(entry), 0xFFu);
}

TEST(VulkanResourceTableTest, EnumerationVisitsOneBufferListUntilTheNextBuffer)
{
    // Two buffers: header [offset(ub0), offset(ub1)], then ub0's two entries, ub1's entry and the sentinel.
    const gp::UInt32 map[] = { 2u,
                               4u,
                               encodeVulkanResourceTableEntry(0u, 0u, 3u),
                               encodeVulkanResourceTableEntry(0u, 1u, 5u),
                               encodeVulkanResourceTableEntry(1u, 0u, 9u),
                               kVulkanResourceTableEndOfTable };

    VisitedEntries visited{};
    const gp::UInt32 count = enumerateVulkanResourceTableEntries(
        gp::ConstVectorView<gp::UInt32>(map, 6u),
        0u,
        [&visited](const gp::UInt32 resourceIndex, const gp::UInt32 bindIndex) -> void
    {
        visited.resourceIndices[visited.count] = resourceIndex;
        visited.bindIndices[visited.count] = bindIndex;
        ++visited.count;
    }
    );

    ASSERT_EQ(count, 2u);
    ASSERT_EQ(visited.count, 2u);
    EXPECT_EQ(visited.resourceIndices[0], 0u);
    EXPECT_EQ(visited.bindIndices[0], 3u);
    EXPECT_EQ(visited.resourceIndices[1], 1u);
    EXPECT_EQ(visited.bindIndices[1], 5u);
}

TEST(VulkanResourceTableTest, EnumerationStopsAtTheSentinel)
{
    const gp::UInt32 map[] = { 2u,
                               4u,
                               encodeVulkanResourceTableEntry(0u, 0u, 3u),
                               encodeVulkanResourceTableEntry(0u, 1u, 5u),
                               encodeVulkanResourceTableEntry(1u, 2u, 9u),
                               kVulkanResourceTableEndOfTable };

    gp::UInt32 lastBind = 0u;
    const gp::UInt32 count = enumerateVulkanResourceTableEntries(
        gp::ConstVectorView<gp::UInt32>(map, 6u),
        1u,
        [&lastBind](const gp::UInt32 resourceIndex, const gp::UInt32 bindIndex) -> void
    {
        static_cast<void>(resourceIndex);
        lastBind = bindIndex;
    }
    );

    EXPECT_EQ(count, 1u);
    EXPECT_EQ(lastBind, 9u);
}

TEST(VulkanResourceTableTest, MissingOrOutOfRangeListsVisitNothing)
{
    const gp::UInt32 map[] = { 0u, 42u, kVulkanResourceTableEndOfTable };
    const auto fail = [](const gp::UInt32 resourceIndex, const gp::UInt32 bindIndex) -> void
    {
        static_cast<void>(resourceIndex);
        static_cast<void>(bindIndex);
        ADD_FAILURE() << "No entry should be visited.";
    };

    const gp::ConstVectorView<gp::UInt32> view(map, 3u);
    EXPECT_EQ(enumerateVulkanResourceTableEntries(view, 0u, fail), 0u);    // Zero offset: buffer has no list.
    EXPECT_EQ(enumerateVulkanResourceTableEntries(view, 1u, fail), 0u);    // Offset past the end.
    EXPECT_EQ(enumerateVulkanResourceTableEntries(view, 10u, fail), 0u);   // Buffer index past the header.
    EXPECT_EQ(enumerateVulkanResourceTableEntries(gp::ConstVectorView<gp::UInt32>(), 0u, fail), 0u);
}

}   // namespace gp::rhi::vulkan::tests
