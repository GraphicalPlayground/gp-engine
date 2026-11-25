// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "barriers/VulkanBarrierTypes.hpp"
#include "barriers/VulkanBarrierDesc.hpp"
#include "barriers/VulkanBarrierEnums.hpp"
#include "CoreMinimal.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

// kVulkanReadAccessMask must contain exactly the documented read bits and no write bits.
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0u,
    "kVulkanReadAccessMask missing INDIRECT_COMMAND_READ_BIT"
);
static_assert((kVulkanReadAccessMask & VK_ACCESS_INDEX_READ_BIT) != 0u, "kVulkanReadAccessMask missing INDEX_READ_BIT");
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT) != 0u,
    "kVulkanReadAccessMask missing VERTEX_ATTRIBUTE_READ_BIT"
);
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_UNIFORM_READ_BIT) != 0u, "kVulkanReadAccessMask missing UNIFORM_READ_BIT"
);
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0u,
    "kVulkanReadAccessMask missing INPUT_ATTACHMENT_READ_BIT"
);
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_SHADER_READ_BIT) != 0u, "kVulkanReadAccessMask missing SHADER_READ_BIT"
);
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT) != 0u,
    "kVulkanReadAccessMask missing COLOR_ATTACHMENT_READ_BIT"
);
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT) != 0u,
    "kVulkanReadAccessMask missing DEPTH_STENCIL_ATTACHMENT_READ_BIT"
);
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_TRANSFER_READ_BIT) != 0u, "kVulkanReadAccessMask missing TRANSFER_READ_BIT"
);
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_MEMORY_READ_BIT) != 0u, "kVulkanReadAccessMask missing MEMORY_READ_BIT"
);

// Write bits must be absent from the read-only mask.
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_SHADER_WRITE_BIT) == 0u,
    "kVulkanReadAccessMask must not contain SHADER_WRITE_BIT"
);
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) == 0u,
    "kVulkanReadAccessMask must not contain COLOR_ATTACHMENT_WRITE_BIT"
);
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) == 0u,
    "kVulkanReadAccessMask must not contain DEPTH_STENCIL_ATTACHMENT_WRITE_BIT"
);
static_assert(
    (kVulkanReadAccessMask & VK_ACCESS_TRANSFER_WRITE_BIT) == 0u,
    "kVulkanReadAccessMask must not contain TRANSFER_WRITE_BIT"
);

}   // namespace

// VulkanBarrierEnums.hpp - enum default values

TEST(VulkanBarrierEnumsTest, EnumDefaultValuesMatchDocumentedSemantics)
{
    // VulkanShadingRateImageType
    EXPECT_EQ(static_cast<gp::UInt8>(VulkanShadingRateImageType::NotSupported), 0u);
    EXPECT_NE(
        static_cast<gp::UInt8>(VulkanShadingRateImageType::Palette),
        static_cast<gp::UInt8>(VulkanShadingRateImageType::NotSupported)
    );
    EXPECT_NE(
        static_cast<gp::UInt8>(VulkanShadingRateImageType::Fractional),
        static_cast<gp::UInt8>(VulkanShadingRateImageType::Palette)
    );

    // VulkanBarrierResourceType
    EXPECT_EQ(static_cast<gp::UInt8>(VulkanBarrierResourceType::None), 0u);
    EXPECT_NE(
        static_cast<gp::UInt8>(VulkanBarrierResourceType::Texture),
        static_cast<gp::UInt8>(VulkanBarrierResourceType::None)
    );
    EXPECT_NE(
        static_cast<gp::UInt8>(VulkanBarrierResourceType::Buffer),
        static_cast<gp::UInt8>(VulkanBarrierResourceType::Texture)
    );
    EXPECT_NE(
        static_cast<gp::UInt8>(VulkanBarrierResourceType::BVH),
        static_cast<gp::UInt8>(VulkanBarrierResourceType::Buffer)
    );
}

// VulkanBarrierTypes.hpp - struct defaults and operator==

TEST(VulkanBarrierTypesTest, VulkanStageAccessFlagsDefaultsAndEquality)
{
    const VulkanStageAccessFlags defaultFlags{};
    EXPECT_EQ(defaultFlags.stageFlags, 0u);
    EXPECT_EQ(defaultFlags.accessFlags, 0u);

    VulkanStageAccessFlags a{};
    a.stageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    a.accessFlags = VK_ACCESS_SHADER_READ_BIT;

    VulkanStageAccessFlags b{};
    b.stageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    b.accessFlags = VK_ACCESS_SHADER_READ_BIT;

    VulkanStageAccessFlags c{};
    c.stageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    c.accessFlags = VK_ACCESS_SHADER_READ_BIT;

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(a == defaultFlags);
}

TEST(VulkanBarrierTypesTest, VulkanStageAccessLayoutDefaultsAndEquality)
{
    const VulkanStageAccessLayout defaultLayout{};
    EXPECT_EQ(defaultLayout.stageFlags, 0u);
    EXPECT_EQ(defaultLayout.accessFlags, 0u);
    EXPECT_EQ(defaultLayout.layout, VK_IMAGE_LAYOUT_UNDEFINED);

    VulkanStageAccessLayout a{};
    a.stageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    a.accessFlags = VK_ACCESS_TRANSFER_READ_BIT;
    a.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    VulkanStageAccessLayout b = a;
    EXPECT_TRUE(a == b);

    b.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    EXPECT_FALSE(a == b);

    b.layout = a.layout;
    b.stageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    EXPECT_FALSE(a == b);

    b.stageFlags = a.stageFlags;
    b.accessFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
    EXPECT_FALSE(a == b);
}

TEST(VulkanBarrierTypesTest, VulkanCommitReservedResourceDescDefaultsAndEquality)
{
    const VulkanCommitReservedResourceDesc defaultDesc{};
    EXPECT_EQ(defaultDesc.resource, nullptr);
    EXPECT_EQ(defaultDesc.commitSizeInBytes, 0u);

    VulkanCommitReservedResourceDesc a{};
    a.resource = nullptr;
    a.commitSizeInBytes = 1024u;

    VulkanCommitReservedResourceDesc b{};
    b.resource = nullptr;
    b.commitSizeInBytes = 1024u;

    EXPECT_TRUE(a == b);

    b.commitSizeInBytes = 2048u;
    EXPECT_FALSE(a == b);
}

// VulkanBarrierDesc.hpp - struct default member values

TEST(VulkanBarrierDescTest, VulkanBarrierSettingsHasCorrectDefaultValues)
{
    const VulkanBarrierSettings settings{};
    EXPECT_TRUE(settings.useMemoryBarrierOpt);
    EXPECT_EQ(settings.maxBarriersPerBatch, -1);
    EXPECT_TRUE(settings.allowSplitBarriers);
    EXPECT_FALSE(settings.allowConcurrentImage);
}

TEST(VulkanBarrierDescTest, VulkanBarrierTranslationDescHasCorrectDefaultValues)
{
    const VulkanBarrierTranslationDesc desc{};

    // Always starts with vertex + fragment stages.
    const VkPipelineStageFlags kMinGraphics =
        static_cast<VkPipelineStageFlags>(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    EXPECT_EQ(desc.graphicsPipelineStageBits & kMinGraphics, kMinGraphics);

    EXPECT_FALSE(desc.supportsRayTracingShaders);
    EXPECT_EQ(desc.shadingRateImageType, VulkanShadingRateImageType::NotSupported);
    EXPECT_FALSE(desc.supportsDedicatedSRVGraphicsNonPixelAccess);
}

TEST(VulkanBarrierDescTest, VulkanBarrierDeviceDescHasCorrectDefaultValues)
{
    const VulkanBarrierDeviceDesc desc{};
    EXPECT_FALSE(desc.supportsParallelRendering);
    EXPECT_TRUE(desc.needsQueueFamilyOwnershipTransfers);
    EXPECT_FALSE(desc.hasAsyncComputeQueue);
    EXPECT_FALSE(desc.hasKHRMaintenance9);
    EXPECT_EQ(desc.graphicsQueueFamilyIndex, 0u);
    EXPECT_EQ(desc.computeQueueFamilyIndex, 0u);
    EXPECT_EQ(desc.computeSupportedStages, 0u);
    EXPECT_EQ(desc.computeSupportedAccess, 0u);
}

}   // namespace gp::rhi::vulkan::tests
