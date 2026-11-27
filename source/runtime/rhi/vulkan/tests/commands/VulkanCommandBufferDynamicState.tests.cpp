// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "commands/VulkanCommandBufferDynamicState.hpp"
#include "containers/arrays/Array.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "pipeline/VulkanGraphicsPipelineDesc.hpp"
#include "pipeline/VulkanGraphicsPipelineDescTypes.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Builds a viewport.
/// @param[in] x Left edge.
/// @param[in] width Width.
/// @return Viewport of height 100 and depth range [0, 1].
[[nodiscard]] VkViewport makeViewport(const gp::Float32 x, const gp::Float32 width) noexcept
{
    VkViewport viewport{};
    viewport.x = x;
    viewport.y = 0.0f;
    viewport.width = width;
    viewport.height = 100.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    return viewport;
}

/// @brief Builds a scissor rectangle.
/// @param[in] x Left edge.
/// @param[in] width Width.
/// @return Rectangle of height 100.
[[nodiscard]] VkRect2D makeRect(const gp::Int32 x, const gp::UInt32 width) noexcept
{
    VkRect2D rect{};
    rect.offset.x = x;
    rect.extent.width = width;
    rect.extent.height = 100u;
    return rect;
}

}   // namespace

TEST(VulkanCommandBufferDynamicStateTest, FreshStateNeedsAFullUpdate)
{
    const VulkanCommandBufferDynamicState state{};

    EXPECT_TRUE(state.needsFullUpdate);
    EXPECT_FALSE(state.hasViewport);
    EXPECT_FALSE(state.hasScissor);
    EXPECT_FALSE(state.hasStencilRef);
    EXPECT_FALSE(state.hasBlendConstants);
    EXPECT_FALSE(state.hasDepthBounds);
    EXPECT_EQ(state.lastPipelineState.topology, static_cast<gp::UInt32>(VK_PRIMITIVE_TOPOLOGY_MAX_ENUM));
}

TEST(VulkanCommandBufferDynamicStateTest, FirstUpdateAlwaysRecordsEvenWithMatchingBytes)
{
    VulkanCommandBufferDynamicState state{};

    // The cached values already equal these, but nothing was recorded on this command buffer yet.
    EXPECT_TRUE(state.updateStencilRef(0u));
    EXPECT_TRUE(state.updateBlendConstants(gp::Array<gp::Float32, 4u>{}));
    EXPECT_TRUE(state.updateDepthBounds(0.0f, 1.0f));
    EXPECT_TRUE(state.hasStencilRef);
    EXPECT_TRUE(state.hasBlendConstants);
    EXPECT_TRUE(state.hasDepthBounds);
}

TEST(VulkanCommandBufferDynamicStateTest, RepeatedValuesAreNotRecordedAgain)
{
    VulkanCommandBufferDynamicState state{};
    const gp::Array<gp::Float32, 4u> blend{ 0.25f, 0.5f, 0.75f, 1.0f };

    ASSERT_TRUE(state.updateStencilRef(3u));
    ASSERT_TRUE(state.updateBlendConstants(blend));
    ASSERT_TRUE(state.updateDepthBounds(0.1f, 0.9f));

    EXPECT_FALSE(state.updateStencilRef(3u));
    EXPECT_FALSE(state.updateBlendConstants(blend));
    EXPECT_FALSE(state.updateDepthBounds(0.1f, 0.9f));
}

TEST(VulkanCommandBufferDynamicStateTest, ChangedValuesAreRecordedAndStored)
{
    VulkanCommandBufferDynamicState state{};
    ASSERT_TRUE(state.updateStencilRef(3u));
    ASSERT_TRUE(state.updateBlendConstants(gp::Array<gp::Float32, 4u>{ 1.0f, 1.0f, 1.0f, 1.0f }));
    ASSERT_TRUE(state.updateDepthBounds(0.0f, 1.0f));

    EXPECT_TRUE(state.updateStencilRef(4u));
    EXPECT_EQ(state.stencilRef, 4u);

    EXPECT_TRUE(state.updateBlendConstants(gp::Array<gp::Float32, 4u>{ 1.0f, 1.0f, 0.5f, 1.0f }));
    EXPECT_FLOAT_EQ(state.blendConstants[2], 0.5f);

    EXPECT_TRUE(state.updateDepthBounds(0.0f, 0.5f));
    EXPECT_FLOAT_EQ(state.maxDepthBounds, 0.5f);
    EXPECT_TRUE(state.updateDepthBounds(0.25f, 0.5f));
    EXPECT_FLOAT_EQ(state.minDepthBounds, 0.25f);
}

TEST(VulkanCommandBufferDynamicStateTest, ViewportsCompareCountAndEveryField)
{
    VulkanCommandBufferDynamicState state{};
    const gp::Array<VkViewport, 2u> two{ makeViewport(0.0f, 64.0f), makeViewport(64.0f, 64.0f) };

    ASSERT_TRUE(state.updateViewports(two.data(), 2u));
    EXPECT_EQ(state.numViewports, 2u);
    EXPECT_FALSE(state.updateViewports(two.data(), 2u));

    // Fewer viewports differ even when the prefix matches.
    EXPECT_TRUE(state.updateViewports(two.data(), 1u));
    EXPECT_EQ(state.numViewports, 1u);

    VkViewport changed = two[0];
    changed.maxDepth = 0.5f;
    EXPECT_TRUE(state.updateViewports(&changed, 1u));
    EXPECT_FLOAT_EQ(state.viewports[0].maxDepth, 0.5f);
}

TEST(VulkanCommandBufferDynamicStateTest, ScissorsCompareCountAndEveryField)
{
    VulkanCommandBufferDynamicState state{};
    const VkRect2D rect = makeRect(0, 64u);

    ASSERT_TRUE(state.updateScissors(&rect, 1u));
    EXPECT_FALSE(state.updateScissors(&rect, 1u));

    VkRect2D moved = rect;
    moved.offset.y = 8;
    EXPECT_TRUE(state.updateScissors(&moved, 1u));
    EXPECT_EQ(state.scissors[0].offset.y, 8);
}

TEST(VulkanCommandBufferDynamicStateTest, ViewportCountIsClampedToCapacity)
{
    VulkanCommandBufferDynamicState state{};
    gp::Array<VkViewport, kVulkanMaxPendingViewports + 4u> viewports{};
    gp::Array<VkRect2D, kVulkanMaxPendingViewports + 4u> rects{};
    for (gp::UInt32 index = 0u; index < kVulkanMaxPendingViewports + 4u; ++index)
    {
        viewports[index] = makeViewport(static_cast<gp::Float32>(index), 1.0f);
        rects[index] = makeRect(static_cast<gp::Int32>(index), 1u);
    }

    ASSERT_TRUE(state.updateViewports(viewports.data(), kVulkanMaxPendingViewports + 4u));
    ASSERT_TRUE(state.updateScissors(rects.data(), kVulkanMaxPendingViewports + 4u));

    EXPECT_EQ(state.numViewports, kVulkanMaxPendingViewports);
    EXPECT_EQ(state.numScissors, kVulkanMaxPendingViewports);
    EXPECT_FALSE(state.updateViewports(viewports.data(), kVulkanMaxPendingViewports));
}

TEST(VulkanCommandBufferDynamicStateTest, ResetForgetsEverythingRecorded)
{
    VulkanCommandBufferDynamicState state{};
    const VkViewport viewport = makeViewport(0.0f, 64.0f);
    const VkRect2D rect = makeRect(0, 64u);
    ASSERT_TRUE(state.updateViewports(&viewport, 1u));
    ASSERT_TRUE(state.updateScissors(&rect, 1u));
    ASSERT_TRUE(state.updateStencilRef(9u));
    state.lastPipelineState.topology = static_cast<gp::UInt32>(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    state.needsFullUpdate = false;

    state.reset();

    EXPECT_TRUE(state.needsFullUpdate);
    EXPECT_EQ(state.numViewports, 0u);
    EXPECT_EQ(state.numScissors, 0u);
    EXPECT_EQ(state.stencilRef, 0u);
    EXPECT_EQ(state.lastPipelineState.topology, static_cast<gp::UInt32>(VK_PRIMITIVE_TOPOLOGY_MAX_ENUM));
    EXPECT_TRUE(state.updateViewports(&viewport, 1u));
    EXPECT_TRUE(state.updateStencilRef(9u));
}

TEST(VulkanDynamicPipelineStateSnapshotTest, ColorAttachmentsRoundTripThroughCapture)
{
    VulkanGraphicsPipelineDesc desc{};
    desc.colorAttachmentStates.pushBack(VulkanBlendAttachmentDesc{});
    desc.colorAttachmentStates.pushBack(VulkanBlendAttachmentDesc{});
    VulkanDynamicPipelineStateSnapshot snapshot{};

    EXPECT_FALSE(snapshot.hasSameColorAttachments(desc));
    snapshot.captureColorAttachments(desc);
    EXPECT_EQ(snapshot.numColorAttachments, 2u);
    EXPECT_TRUE(snapshot.hasSameColorAttachments(desc));

    desc.colorAttachmentStates[1].blend = true;
    EXPECT_FALSE(snapshot.hasSameColorAttachments(desc));
}

TEST(VulkanDynamicPipelineStateSnapshotTest, VertexInputRoundTripsThroughCapture)
{
    VulkanGraphicsPipelineDesc desc{};
    VulkanVertexBindingDesc binding{};
    binding.stride = 16u;
    desc.vertexBindings.pushBack(binding);
    VulkanVertexAttributeDesc attribute{};
    attribute.location = 1u;
    desc.vertexAttributes.pushBack(attribute);
    VulkanDynamicPipelineStateSnapshot snapshot{};

    EXPECT_FALSE(snapshot.hasSameVertexInput(desc));
    snapshot.captureVertexInput(desc);
    EXPECT_EQ(snapshot.numVertexBindings, 1u);
    EXPECT_EQ(snapshot.numVertexAttributes, 1u);
    EXPECT_TRUE(snapshot.hasSameVertexInput(desc));

    desc.vertexAttributes[0].offset = 4u;
    EXPECT_FALSE(snapshot.hasSameVertexInput(desc));
    snapshot.captureVertexInput(desc);
    desc.vertexAttributes.pushBack(attribute);
    EXPECT_FALSE(snapshot.hasSameVertexInput(desc));
}

}   // namespace gp::rhi::vulkan::tests
