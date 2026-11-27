// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "pipeline/VulkanPipelineDynamicStates.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanGraphicsPipelineDesc.hpp"
#include "pipeline/VulkanPipelineTypes.hpp"
#include "VulkanCommon.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Returns a feature set with every optional dynamic state extension.
/// @return Features.
[[nodiscard]] VulkanDynamicStateFeatures makeAllFeatures() noexcept
{
    VulkanDynamicStateFeatures features{};
    features.hasExtendedDynamicState1 = true;
    features.hasExtendedDynamicState2 = true;
    features.hasExtendedDynamicState3 = true;
    features.hasVertexInputDynamicState = true;
    return features;
}

/// @brief Returns a vertex pipeline description with one color attachment.
/// @return Description.
[[nodiscard]] VulkanGraphicsPipelineDesc makeColorDesc()
{
    VulkanGraphicsPipelineDesc desc{};
    desc.shaderKeys[static_cast<gp::USize>(shader_stage::Stage::Vertex)] = 1u;
    desc.colorAttachmentStates.emplaceBack();
    return desc;
}

}   // namespace

static_assert(kVulkanSpecializationMapEntries[0].constantID == 0u);
static_assert(kVulkanSpecializationMapEntries[0].offset == 0u);
static_assert(kVulkanSpecializationMapEntries[kMaxNumSpecializationConstants - 1u].constantID == 31u);
static_assert(
    kVulkanSpecializationMapEntries[kMaxNumSpecializationConstants - 1u].offset == 31u * kSpecializationConstantSize
);
static_assert(kVulkanSpecializationMapEntries[7].size == kSpecializationConstantSize);

TEST(VulkanDynamicStateListTest, AddAndContains)
{
    VulkanDynamicStateList list{};
    EXPECT_FALSE(list.contains(VK_DYNAMIC_STATE_VIEWPORT));

    list.add(VK_DYNAMIC_STATE_VIEWPORT);

    EXPECT_EQ(list.count, 1u);
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_VIEWPORT));
    EXPECT_FALSE(list.contains(VK_DYNAMIC_STATE_SCISSOR));
}

TEST(VulkanPipelineDynamicStatesTest, BaselineUsesStaticViewportCountStates)
{
    const VulkanDynamicStateList list = fillVulkanDynamicStates(VulkanDynamicStateFeatures{}, makeColorDesc());

    EXPECT_EQ(list.count, 5u);
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_VIEWPORT));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_SCISSOR));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_STENCIL_REFERENCE));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_DEPTH_BOUNDS));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_BLEND_CONSTANTS));
    EXPECT_FALSE(list.contains(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT));
}

TEST(VulkanPipelineDynamicStatesTest, ExtendedDynamicState1ReplacesViewportStates)
{
    VulkanDynamicStateFeatures features{};
    features.hasExtendedDynamicState1 = true;

    const VulkanDynamicStateList list = fillVulkanDynamicStates(features, makeColorDesc());

    EXPECT_EQ(list.count, 13u);
    EXPECT_FALSE(list.contains(VK_DYNAMIC_STATE_VIEWPORT));
    EXPECT_FALSE(list.contains(VK_DYNAMIC_STATE_SCISSOR));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_CULL_MODE));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE));
}

TEST(VulkanPipelineDynamicStatesTest, ExtendedDynamicState2AddsDepthBias)
{
    VulkanDynamicStateFeatures features{};
    features.hasExtendedDynamicState2 = true;

    const VulkanDynamicStateList list = fillVulkanDynamicStates(features, makeColorDesc());

    EXPECT_EQ(list.count, 7u);
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_DEPTH_BIAS));
}

TEST(VulkanPipelineDynamicStatesTest, ExtendedDynamicState3BlendStatesNeedColorAttachments)
{
    VulkanDynamicStateFeatures features{};
    features.hasExtendedDynamicState3 = true;

    const VulkanDynamicStateList withColor = fillVulkanDynamicStates(features, makeColorDesc());
    VulkanGraphicsPipelineDesc depthOnly = makeColorDesc();
    depthOnly.colorAttachmentStates.clear();
    const VulkanDynamicStateList withoutColor = fillVulkanDynamicStates(features, depthOnly);

    EXPECT_EQ(withColor.count, 11u);
    EXPECT_TRUE(withColor.contains(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT));
    EXPECT_TRUE(withColor.contains(VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT));
    EXPECT_TRUE(withColor.contains(VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT));
    EXPECT_EQ(withoutColor.count, 8u);
    EXPECT_FALSE(withoutColor.contains(VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT));
    EXPECT_TRUE(withoutColor.contains(VK_DYNAMIC_STATE_POLYGON_MODE_EXT));
    EXPECT_TRUE(withoutColor.contains(VK_DYNAMIC_STATE_RASTERIZATION_SAMPLES_EXT));
    EXPECT_TRUE(withoutColor.contains(VK_DYNAMIC_STATE_ALPHA_TO_COVERAGE_ENABLE_EXT));
}

TEST(VulkanPipelineDynamicStatesTest, EveryFeatureFitsTheFixedList)
{
    const VulkanDynamicStateList list = fillVulkanDynamicStates(makeAllFeatures(), makeColorDesc());

    EXPECT_EQ(list.count, kMaxPipelineDynamicStates);
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT));
}

TEST(VulkanPipelineDynamicStatesTest, MeshPipelinesSkipVertexInputAndTopology)
{
    VulkanGraphicsPipelineDesc desc = makeColorDesc();
    desc.shaderKeys[static_cast<gp::USize>(shader_stage::Stage::Vertex)] = 0u;
    desc.shaderKeys[static_cast<gp::USize>(shader_stage::Stage::Mesh)] = 7u;

    const VulkanDynamicStateList list = fillVulkanDynamicStates(makeAllFeatures(), desc);

    EXPECT_EQ(list.count, kMaxPipelineDynamicStates - 2u);
    EXPECT_FALSE(list.contains(VK_DYNAMIC_STATE_VERTEX_INPUT_EXT));
    EXPECT_FALSE(list.contains(VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY));
    EXPECT_TRUE(list.contains(VK_DYNAMIC_STATE_CULL_MODE));
}

}   // namespace gp::rhi::vulkan::tests
