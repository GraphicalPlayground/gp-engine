// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "contexts/VulkanDrawUtils.hpp"
#include "CoreMinimal.hpp"        // IWYU pragma: keep
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanDrawUtilsTest, ZeroPrimitivesDrawNothingForEveryTopology)
{
    EXPECT_EQ(getVulkanVertexCountForPrimitiveCount(0u, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST), 0u);
    EXPECT_EQ(getVulkanVertexCountForPrimitiveCount(0u, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP), 0u);
    EXPECT_EQ(getVulkanVertexCountForPrimitiveCount(0u, VK_PRIMITIVE_TOPOLOGY_LINE_LIST), 0u);
    EXPECT_EQ(getVulkanVertexCountForPrimitiveCount(0u, VK_PRIMITIVE_TOPOLOGY_POINT_LIST), 0u);
}

TEST(VulkanDrawUtilsTest, ListsMultiplyByVerticesPerPrimitive)
{
    EXPECT_EQ(getVulkanVertexCountForPrimitiveCount(4u, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST), 12u);
    EXPECT_EQ(getVulkanVertexCountForPrimitiveCount(4u, VK_PRIMITIVE_TOPOLOGY_LINE_LIST), 8u);
    EXPECT_EQ(getVulkanVertexCountForPrimitiveCount(4u, VK_PRIMITIVE_TOPOLOGY_POINT_LIST), 4u);
}

TEST(VulkanDrawUtilsTest, TriangleStripAddsTwoPrimingVertices)
{
    EXPECT_EQ(getVulkanVertexCountForPrimitiveCount(1u, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP), 3u);
    EXPECT_EQ(getVulkanVertexCountForPrimitiveCount(10u, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP), 12u);
}

TEST(VulkanDrawUtilsTest, IndirectCapacityCountsWholeCommands)
{
    EXPECT_EQ(getVulkanIndirectCommandCapacity(64u, 0u, 16u), 4u);
    EXPECT_EQ(getVulkanIndirectCommandCapacity(64u, 16u, 16u), 3u);
    EXPECT_EQ(getVulkanIndirectCommandCapacity(63u, 0u, 16u), 3u);
}

TEST(VulkanDrawUtilsTest, IndirectCapacityIsZeroPastTheEndOrForZeroStride)
{
    EXPECT_EQ(getVulkanIndirectCommandCapacity(64u, 64u, 16u), 0u);
    EXPECT_EQ(getVulkanIndirectCommandCapacity(64u, 128u, 16u), 0u);
    EXPECT_EQ(getVulkanIndirectCommandCapacity(64u, 0u, 0u), 0u);
    EXPECT_EQ(getVulkanIndirectCommandCapacity(0u, 0u, 16u), 0u);
}

}   // namespace gp::rhi::vulkan::tests
