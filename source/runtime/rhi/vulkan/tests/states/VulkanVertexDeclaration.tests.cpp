// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanVertexDeclaration.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/VertexDeclaration.hpp"
#include "pipeline/VertexDeclarationDesc.hpp"
#include "states/VulkanStateRHI.hpp"
#include "states/VulkanVertexDeclarationKey.hpp"
#include "states/VulkanVertexInputStateInfo.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Interleaved position and normal in stream 0, per-instance color in stream 2.
/// @param[in] stride Stride of stream 0.
/// @return Element list.
[[nodiscard]] pipeline::VertexDeclarationElementList makeElements(const gp::UInt16 stride = 24u) noexcept
{
    pipeline::VertexDeclarationElementList elements;
    elements.emplaceBack(pipeline::VertexElement(0u, 0u, pipeline::VertexElementType::Float3, 0u, stride));
    elements.emplaceBack(pipeline::VertexElement(0u, 12u, pipeline::VertexElementType::Float3, 1u, stride));
    elements.emplaceBack(pipeline::VertexElement(2u, 0u, pipeline::VertexElementType::Float4, 2u, 16u, true));
    return elements;
}

/// @brief Fixture emptying the process-wide declaration cache around each test.
class VulkanVertexDeclarationTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        VulkanVertexDeclaration::emptyCache();
    }

    virtual void TearDown() override
    {
        VulkanVertexDeclaration::emptyCache();
    }
};

}   // namespace

TEST(VulkanVertexDeclarationKeyTest, EqualElementsProduceEqualKeys)
{
    const VulkanVertexDeclarationKey first(makeElements());
    const VulkanVertexDeclarationKey second(makeElements());

    EXPECT_TRUE(first == second);
    EXPECT_EQ(first.hash, second.hash);
    EXPECT_EQ(first.hashNoStrides, second.hashNoStrides);
}

TEST(VulkanVertexDeclarationKeyTest, StridesOnlyChangeTheFullHash)
{
    const VulkanVertexDeclarationKey narrow(makeElements(24u));
    const VulkanVertexDeclarationKey wide(makeElements(32u));

    EXPECT_FALSE(narrow == wide);
    EXPECT_NE(narrow.hash, wide.hash);
    EXPECT_EQ(narrow.hashNoStrides, wide.hashNoStrides);
}

TEST(VulkanVertexDeclarationKeyTest, StridelessHashIgnoresElementOrder)
{
    const pipeline::VertexDeclarationElementList elements = makeElements();
    pipeline::VertexDeclarationElementList reversed;
    reversed.emplaceBack(elements[2]);
    reversed.emplaceBack(elements[1]);
    reversed.emplaceBack(elements[0]);

    const VulkanVertexDeclarationKey ordered(elements);
    const VulkanVertexDeclarationKey shuffled(reversed);

    EXPECT_NE(ordered.hash, shuffled.hash);
    EXPECT_EQ(ordered.hashNoStrides, shuffled.hashNoStrides);
}

TEST(VulkanVertexDeclarationKeyTest, SortOrdersByStreamOffsetThenAttribute)
{
    pipeline::VertexDeclarationElementList elements;
    elements.emplaceBack(pipeline::VertexElement(1u, 0u, pipeline::VertexElementType::Float2, 3u, 8u));
    elements.emplaceBack(pipeline::VertexElement(0u, 4u, pipeline::VertexElementType::Float1, 1u, 8u));
    elements.emplaceBack(pipeline::VertexElement(0u, 0u, pipeline::VertexElementType::Float1, 2u, 8u));
    elements.emplaceBack(pipeline::VertexElement(0u, 0u, pipeline::VertexElementType::Float1, 0u, 8u));

    VulkanVertexDeclarationKey::sortElements(elements);

    EXPECT_EQ(elements[0].attributeIndex, 0u);
    EXPECT_EQ(elements[1].attributeIndex, 2u);
    EXPECT_EQ(elements[2].attributeIndex, 1u);
    EXPECT_EQ(elements[3].attributeIndex, 3u);
}

TEST_F(VulkanVertexDeclarationTest, CacheDeduplicatesEqualDeclarations)
{
    const pipeline::VertexDeclarationRef first = VulkanVertexDeclaration::findOrCreate(makeElements());
    const pipeline::VertexDeclarationRef second = createVulkanVertexDeclaration(makeElements());
    const pipeline::VertexDeclarationRef wide = VulkanVertexDeclaration::findOrCreate(makeElements(32u));

    ASSERT_NE(first.get(), nullptr);
    EXPECT_EQ(first.get(), second.get());
    EXPECT_NE(first.get(), wide.get());
    EXPECT_EQ(VulkanVertexDeclaration::getCacheSize(), 2u);

    // Pipeline precaching ignores strides.
    EXPECT_EQ(first->getPrecachePSOHash(), wide->getPrecachePSOHash());

    VulkanVertexDeclaration::emptyCache();
    EXPECT_EQ(VulkanVertexDeclaration::getCacheSize(), 0u);
    // References held by callers outlive the cache.
    EXPECT_EQ(static_cast<const VulkanVertexDeclaration*>(first.get())->getElements().size(), 3u);
}

TEST_F(VulkanVertexDeclarationTest, InitializerRoundTrips)
{
    const pipeline::VertexDeclarationRef declaration = VulkanVertexDeclaration::findOrCreate(makeElements());
    ASSERT_NE(declaration.get(), nullptr);

    pipeline::VertexDeclarationElementList roundTrip;
    EXPECT_TRUE(declaration->getInitializer(roundTrip));
    ASSERT_EQ(roundTrip.size(), 3u);
    EXPECT_EQ(roundTrip[2], makeElements()[2]);
}

TEST_F(VulkanVertexDeclarationTest, InputStateCompactsBindingsOfUsedStreams)
{
    const pipeline::VertexDeclarationRef declaration = VulkanVertexDeclaration::findOrCreate(makeElements());
    VulkanVertexInputStateInfo info;
    info.generate(*static_cast<const VulkanVertexDeclaration*>(declaration.get()), 0b111u);

    ASSERT_EQ(info.getNumBindings(), 2u);
    ASSERT_EQ(info.getNumAttributes(), 3u);
    EXPECT_EQ(info.getBindingsMask(), 0b101u);
    EXPECT_NE(info.getHash(), 0u);

    // Stream 2 is compacted down to binding 1.
    EXPECT_EQ(info.getBindingForStream(0u), 0u);
    EXPECT_EQ(info.getBindingForStream(2u), 1u);
    EXPECT_EQ(info.getBindingForStream(1u), VulkanVertexInputStateInfo::kInvalidMapping);
    EXPECT_EQ(info.getStreamForBinding(1u), 2u);

    EXPECT_EQ(info.getBinding(0u).stride, 24u);
    EXPECT_EQ(info.getBinding(0u).inputRate, VK_VERTEX_INPUT_RATE_VERTEX);
    EXPECT_EQ(info.getBinding(1u).binding, 1u);
    EXPECT_EQ(info.getBinding(1u).stride, 16u);
    EXPECT_EQ(info.getBinding(1u).inputRate, VK_VERTEX_INPUT_RATE_INSTANCE);

    EXPECT_EQ(info.getAttribute(1u).location, 1u);
    EXPECT_EQ(info.getAttribute(1u).offset, 12u);
    EXPECT_EQ(info.getAttribute(1u).format, VK_FORMAT_R32G32B32_SFLOAT);
    EXPECT_EQ(info.getAttribute(2u).binding, 1u);
    EXPECT_EQ(info.getAttribute(2u).format, VK_FORMAT_R32G32B32A32_SFLOAT);

    const VkPipelineVertexInputStateCreateInfo& createInfo = info.getInfo();
    EXPECT_EQ(createInfo.sType, VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    EXPECT_EQ(createInfo.vertexBindingDescriptionCount, 2u);
    EXPECT_EQ(createInfo.vertexAttributeDescriptionCount, 3u);
}

TEST_F(VulkanVertexDeclarationTest, UnusedAttributesDropTheirStreams)
{
    const pipeline::VertexDeclarationRef declaration = VulkanVertexDeclaration::findOrCreate(makeElements());
    const VulkanVertexDeclaration& vulkanDeclaration = *static_cast<const VulkanVertexDeclaration*>(declaration.get());

    VulkanVertexInputStateInfo positionOnly;
    positionOnly.generate(vulkanDeclaration, 0b001u);
    EXPECT_EQ(positionOnly.getNumBindings(), 1u);
    EXPECT_EQ(positionOnly.getNumAttributes(), 1u);

    VulkanVertexInputStateInfo none;
    none.generate(vulkanDeclaration, 0u);
    EXPECT_EQ(none.getNumBindings(), 0u);
    EXPECT_EQ(none.getHash(), 0u);
}

TEST_F(VulkanVertexDeclarationTest, CopiedInputStateRepointsItsArrays)
{
    const pipeline::VertexDeclarationRef declaration = VulkanVertexDeclaration::findOrCreate(makeElements());
    VulkanVertexInputStateInfo original;
    original.generate(*static_cast<const VulkanVertexDeclaration*>(declaration.get()), 0b111u);

    const VulkanVertexInputStateInfo copy(original);

    EXPECT_TRUE(copy == original);
    EXPECT_EQ(copy.getHash(), original.getHash());
    EXPECT_EQ(copy.getInfo().pVertexBindingDescriptions, &copy.getBinding(0u));
    EXPECT_EQ(copy.getInfo().pVertexAttributeDescriptions, &copy.getAttribute(0u));
}

}   // namespace gp::rhi::vulkan::tests
