// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "translation/VulkanSamplerEnums.hpp"
#include "CoreMinimal.hpp"        // IWYU pragma: keep
#include "pipeline/PipelineEnums.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

static_assert(samplerFilterToVkMipmapMode(pipeline::SamplerFilter::Trilinear) == VK_SAMPLER_MIPMAP_MODE_LINEAR);
static_assert(samplerFilterToVkFilter(pipeline::SamplerFilter::Point) == VK_FILTER_NEAREST);
static_assert(
    samplerAddressModeToVkAddressMode(pipeline::SamplerAddressMode::Clamp) == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
);
static_assert(samplerCompareFunctionToVkCompareOp(pipeline::SamplerCompareFunction::Less) == VK_COMPARE_OP_LESS);

}   // namespace

TEST(VulkanSamplerEnumsTest, FilterAddressModeAndCompareConversions)
{
    EXPECT_EQ(samplerFilterToVkMipmapMode(pipeline::SamplerFilter::Point), VK_SAMPLER_MIPMAP_MODE_NEAREST);
    EXPECT_EQ(samplerFilterToVkMipmapMode(pipeline::SamplerFilter::Bilinear), VK_SAMPLER_MIPMAP_MODE_NEAREST);
    EXPECT_EQ(samplerFilterToVkMipmapMode(pipeline::SamplerFilter::Trilinear), VK_SAMPLER_MIPMAP_MODE_LINEAR);
    EXPECT_EQ(samplerFilterToVkMipmapMode(pipeline::SamplerFilter::AnisotropicPoint), VK_SAMPLER_MIPMAP_MODE_NEAREST);
    EXPECT_EQ(samplerFilterToVkMipmapMode(pipeline::SamplerFilter::AnisotropicLinear), VK_SAMPLER_MIPMAP_MODE_LINEAR);
    EXPECT_EQ(samplerFilterToVkMipmapMode(static_cast<pipeline::SamplerFilter>(255u)), VK_SAMPLER_MIPMAP_MODE_MAX_ENUM);

    EXPECT_EQ(samplerFilterToVkFilter(pipeline::SamplerFilter::Point), VK_FILTER_NEAREST);
    EXPECT_EQ(samplerFilterToVkFilter(pipeline::SamplerFilter::Bilinear), VK_FILTER_LINEAR);
    EXPECT_EQ(samplerFilterToVkFilter(pipeline::SamplerFilter::Trilinear), VK_FILTER_LINEAR);
    EXPECT_EQ(samplerFilterToVkFilter(pipeline::SamplerFilter::AnisotropicPoint), VK_FILTER_LINEAR);
    EXPECT_EQ(samplerFilterToVkFilter(pipeline::SamplerFilter::AnisotropicLinear), VK_FILTER_LINEAR);
    EXPECT_EQ(samplerFilterToVkFilter(static_cast<pipeline::SamplerFilter>(255u)), VK_FILTER_MAX_ENUM);

    EXPECT_EQ(samplerAddressModeToVkAddressMode(pipeline::SamplerAddressMode::Wrap), VK_SAMPLER_ADDRESS_MODE_REPEAT);
    EXPECT_EQ(
        samplerAddressModeToVkAddressMode(pipeline::SamplerAddressMode::Clamp), VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    );
    EXPECT_EQ(
        samplerAddressModeToVkAddressMode(pipeline::SamplerAddressMode::Mirror), VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
    );
    EXPECT_EQ(
        samplerAddressModeToVkAddressMode(pipeline::SamplerAddressMode::Border), VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
    );
    EXPECT_EQ(
        samplerAddressModeToVkAddressMode(static_cast<pipeline::SamplerAddressMode>(255u)),
        VK_SAMPLER_ADDRESS_MODE_MAX_ENUM
    );

    EXPECT_EQ(samplerCompareFunctionToVkCompareOp(pipeline::SamplerCompareFunction::Never), VK_COMPARE_OP_NEVER);
    EXPECT_EQ(samplerCompareFunctionToVkCompareOp(pipeline::SamplerCompareFunction::Less), VK_COMPARE_OP_LESS);
    EXPECT_EQ(
        samplerCompareFunctionToVkCompareOp(static_cast<pipeline::SamplerCompareFunction>(255u)), VK_COMPARE_OP_MAX_ENUM
    );
}

}   // namespace gp::rhi::vulkan::tests
