// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "translation/VulkanVertexFormat.hpp"
#include "CoreMinimal.hpp"
#include "pipeline/PipelineEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

static_assert(vertexElementTypeToVkFormat(pipeline::VertexElementType::None) == VK_FORMAT_UNDEFINED);
static_assert(vertexElementTypeToVkFormat(pipeline::VertexElementType::Float3) == VK_FORMAT_R32G32B32_SFLOAT);
static_assert(vertexElementTypeToVkFormat(pipeline::VertexElementType::Color) == VK_FORMAT_B8G8R8A8_UNORM);
static_assert(
    vertexElementTypeToVkFormat(pipeline::VertexElementType::URGB10A2N) == VK_FORMAT_A2B10G10R10_UNORM_PACK32
);
static_assert(getVertexElementByteSize(pipeline::VertexElementType::Float4) == 16u);
static_assert(getVertexElementByteSize(pipeline::VertexElementType::None) == 0u);

}   // namespace

TEST(VulkanVertexFormatTest, TranslatesEveryVertexElementTypeAndByteSize)
{
    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::None), VK_FORMAT_UNDEFINED);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::None), 0u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Float1), VK_FORMAT_R32_SFLOAT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Float1), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Float2), VK_FORMAT_R32G32_SFLOAT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Float2), 8u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Float3), VK_FORMAT_R32G32B32_SFLOAT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Float3), 12u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Float4), VK_FORMAT_R32G32B32A32_SFLOAT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Float4), 16u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::PackedNormal), VK_FORMAT_R8G8B8A8_SNORM);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::PackedNormal), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::UByte4), VK_FORMAT_R8G8B8A8_UINT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::UByte4), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::UByte4N), VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::UByte4N), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Color), VK_FORMAT_B8G8R8A8_UNORM);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Color), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Short2), VK_FORMAT_R16G16_SINT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Short2), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Short4), VK_FORMAT_R16G16B16A16_SINT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Short4), 8u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Short2N), VK_FORMAT_R16G16_SNORM);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Short2N), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Half2), VK_FORMAT_R16G16_SFLOAT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Half2), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Half4), VK_FORMAT_R16G16B16A16_SFLOAT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Half4), 8u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::Short4N), VK_FORMAT_R16G16B16A16_SNORM);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::Short4N), 8u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::UShort2), VK_FORMAT_R16G16_UINT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::UShort2), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::UShort4), VK_FORMAT_R16G16B16A16_UINT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::UShort4), 8u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::UShort2N), VK_FORMAT_R16G16_UNORM);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::UShort2N), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::UShort4N), VK_FORMAT_R16G16B16A16_UNORM);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::UShort4N), 8u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::URGB10A2N), VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::URGB10A2N), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::UInt), VK_FORMAT_R32_UINT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::UInt), 4u);

    EXPECT_EQ(vertexElementTypeToVkFormat(pipeline::VertexElementType::UInt2), VK_FORMAT_R32G32_UINT);
    EXPECT_EQ(getVertexElementByteSize(pipeline::VertexElementType::UInt2), 8u);

    // Every non-None enumerator in range must resolve to a valid format and non-zero size.
    for (gp::UInt32 raw = 1u; raw < pipeline::kVertexElementTypeCount; ++raw)
    {
        const pipeline::VertexElementType elementType = static_cast<pipeline::VertexElementType>(raw);
        EXPECT_NE(vertexElementTypeToVkFormat(elementType), VK_FORMAT_UNDEFINED);
        EXPECT_GT(getVertexElementByteSize(elementType), 0u);
    }

    // Out-of-range sentinels return VK_FORMAT_UNDEFINED / 0.
    const pipeline::VertexElementType boundary =
        static_cast<pipeline::VertexElementType>(pipeline::kVertexElementTypeCount);
    EXPECT_EQ(vertexElementTypeToVkFormat(boundary), VK_FORMAT_UNDEFINED);
    EXPECT_EQ(getVertexElementByteSize(boundary), 0u);

    const pipeline::VertexElementType invalid = static_cast<pipeline::VertexElementType>(255u);
    EXPECT_EQ(vertexElementTypeToVkFormat(invalid), VK_FORMAT_UNDEFINED);
    EXPECT_EQ(getVertexElementByteSize(invalid), 0u);
}

}   // namespace gp::rhi::vulkan::tests
