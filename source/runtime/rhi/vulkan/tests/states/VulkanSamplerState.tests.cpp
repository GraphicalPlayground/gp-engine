// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "states/VulkanSamplerState.hpp"
#include "../resources/VulkanResourceTestFakes.hpp"
#include "CoreMinimal.hpp"                     // IWYU pragma: keep
#include "maths/base/Arithmetic.hpp"
#include "memory/pointers/RefCountedPtr.hpp"   // IWYU pragma: keep
#include "pipeline/PipelineEnums.hpp"
#include "pipeline/SamplerState.hpp"
#include "pipeline/SamplerStateDesc.hpp"
#include "states/VulkanSamplerStateCache.hpp"
#include "states/VulkanStateRHI.hpp"
#include "states/VulkanStateSettings.hpp"
#include "VulkanThirdParty.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Returns the native sampler behind an RHI sampler reference.
/// @param[in] sampler Sampler created by a Vulkan cache.
/// @return Native handle.
[[nodiscard]] VkSampler getSamplerHandle(const pipeline::SamplerStateRef& sampler) noexcept
{
    return static_cast<const VulkanSamplerState*>(sampler.get())->getHandle();
}

/// @brief Live fixture creating samplers on a real device.
class VulkanSamplerStateLiveTest : public VulkanResourceLiveTest
{};

}   // namespace

TEST(VulkanStateSettingsTest, AnisotropyIsClampedToTheSupportedRange)
{
    EXPECT_EQ(computeVulkanSamplerAnisotropy(4), 4);
    EXPECT_EQ(computeVulkanSamplerAnisotropy(kMaxSamplerAnisotropy), kMaxSamplerAnisotropy);
    EXPECT_EQ(computeVulkanSamplerAnisotropy(64), kMaxSamplerAnisotropy);
}

TEST(VulkanStateSettingsTest, UnspecifiedAnisotropyFallsBackToTheConsoleDefault)
{
    const gp::Int32 expected =
        gp::math::clamp<gp::Int32>(getVulkanDefaultMaxAnisotropy(), kMinSamplerAnisotropy, kMaxSamplerAnisotropy);

    EXPECT_EQ(computeVulkanSamplerAnisotropy(0), expected);
    EXPECT_EQ(computeVulkanSamplerAnisotropy(-3), expected);
    EXPECT_GE(expected, kMinSamplerAnisotropy);
}

TEST_F(VulkanSamplerStateLiveTest, PointSamplerTranslatesWithoutAnisotropy)
{
    const pipeline::SamplerStateDesc desc(
        pipeline::SamplerFilter::Point,
        pipeline::SamplerAddressMode::Clamp,
        pipeline::SamplerAddressMode::Mirror,
        pipeline::SamplerAddressMode::Border,
        0.5f,
        0,
        1.0f,
        4.0f
    );
    VkSamplerCreateInfo createInfo;
    VulkanSamplerState::setupSamplerCreateInfo(desc, *m_device, createInfo);

    EXPECT_EQ(createInfo.sType, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
    EXPECT_EQ(createInfo.pNext, nullptr);
    EXPECT_EQ(createInfo.magFilter, VK_FILTER_NEAREST);
    EXPECT_EQ(createInfo.mipmapMode, VK_SAMPLER_MIPMAP_MODE_NEAREST);
    EXPECT_EQ(createInfo.addressModeU, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    EXPECT_EQ(createInfo.addressModeV, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT);
    EXPECT_EQ(createInfo.addressModeW, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
    EXPECT_FLOAT_EQ(createInfo.mipLodBias, 0.5f);
    EXPECT_FLOAT_EQ(createInfo.minLod, 1.0f);
    EXPECT_FLOAT_EQ(createInfo.maxLod, 4.0f);
    EXPECT_EQ(createInfo.anisotropyEnable, VK_FALSE);
    EXPECT_FLOAT_EQ(createInfo.maxAnisotropy, 1.0f);
    EXPECT_EQ(createInfo.compareEnable, VK_FALSE);
    EXPECT_EQ(createInfo.borderColor, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);
}

TEST_F(VulkanSamplerStateLiveTest, ComparisonSamplerEnablesCompare)
{
    pipeline::SamplerStateDesc desc(pipeline::SamplerFilter::Bilinear);
    desc.samplerComparisonFunction = pipeline::SamplerCompareFunction::Less;
    desc.borderColor = 0xFFFFFFFFu;
    VkSamplerCreateInfo createInfo;
    VulkanSamplerState::setupSamplerCreateInfo(desc, *m_device, createInfo);

    EXPECT_EQ(createInfo.magFilter, VK_FILTER_LINEAR);
    EXPECT_EQ(createInfo.compareEnable, VK_TRUE);
    EXPECT_EQ(createInfo.borderColor, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);
}

TEST_F(VulkanSamplerStateLiveTest, AnisotropyStaysWithinDeviceLimit)
{
    pipeline::SamplerStateDesc desc(pipeline::SamplerFilter::AnisotropicLinear);
    desc.maxAnisotropy = 16;
    VkSamplerCreateInfo createInfo;
    VulkanSamplerState::setupSamplerCreateInfo(desc, *m_device, createInfo);

    EXPECT_GE(createInfo.maxAnisotropy, 1.0f);
    EXPECT_LE(createInfo.maxAnisotropy, gp::math::max(1.0f, m_device->getLimits().maxSamplerAnisotropy));
    EXPECT_EQ(createInfo.anisotropyEnable, (createInfo.maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE);
}

TEST_F(VulkanSamplerStateLiveTest, EqualCreateInfosHashEqually)
{
    const pipeline::SamplerStateDesc desc(pipeline::SamplerFilter::Trilinear);
    VkSamplerCreateInfo first;
    VkSamplerCreateInfo second;
    VulkanSamplerState::setupSamplerCreateInfo(desc, *m_device, first);
    VulkanSamplerState::setupSamplerCreateInfo(desc, *m_device, second);

    VkSamplerCreateInfo other;
    VulkanSamplerState::setupSamplerCreateInfo(
        pipeline::SamplerStateDesc(pipeline::SamplerFilter::Point), *m_device, other
    );

    EXPECT_EQ(VulkanSamplerState::computeCreateInfoHash(first), VulkanSamplerState::computeCreateInfoHash(second));
    EXPECT_NE(VulkanSamplerState::computeCreateInfoHash(first), VulkanSamplerState::computeCreateInfoHash(other));
}

TEST_F(VulkanSamplerStateLiveTest, CacheDeduplicatesSamplers)
{
    VulkanSamplerStateCache cache(*m_device);
    const pipeline::SamplerStateDesc bilinear(pipeline::SamplerFilter::Bilinear);

    const pipeline::SamplerStateRef first = cache.findOrCreate(bilinear);
    const pipeline::SamplerStateRef second = createVulkanSamplerState(cache, bilinear);
    const pipeline::SamplerStateRef point =
        cache.findOrCreate(pipeline::SamplerStateDesc(pipeline::SamplerFilter::Point));

    ASSERT_NE(first.get(), nullptr);
    ASSERT_NE(point.get(), nullptr);
    EXPECT_NE(getSamplerHandle(first), static_cast<VkSampler>(VK_NULL_HANDLE));
    EXPECT_EQ(first.get(), second.get());
    EXPECT_NE(first.get(), point.get());
    EXPECT_NE(getSamplerHandle(first), getSamplerHandle(point));
    EXPECT_EQ(cache.getNumSamplers(), 2u);
    EXPECT_FALSE(first->isImmutable());
    EXPECT_NE(
        static_cast<const VulkanSamplerState*>(first.get())->getSamplerId(),
        static_cast<const VulkanSamplerState*>(point.get())->getSamplerId()
    );

    cache.reset();
    EXPECT_EQ(cache.getNumSamplers(), 0u);
    // Callers keep their reference alive past the reset.
    EXPECT_NE(getSamplerHandle(first), static_cast<VkSampler>(VK_NULL_HANDLE));
}

TEST_F(VulkanSamplerStateLiveTest, ImmutableSamplersAreFlagged)
{
    const pipeline::SamplerStateDesc desc(pipeline::SamplerFilter::Point);
    VkSamplerCreateInfo createInfo;
    VulkanSamplerState::setupSamplerCreateInfo(desc, *m_device, createInfo);

    const pipeline::SamplerStateRef sampler(new VulkanSamplerState(*m_device, createInfo, nullptr, true));

    EXPECT_TRUE(sampler->isImmutable());
    EXPECT_NE(getSamplerHandle(sampler), static_cast<VkSampler>(VK_NULL_HANDLE));
}

}   // namespace gp::rhi::vulkan::tests
