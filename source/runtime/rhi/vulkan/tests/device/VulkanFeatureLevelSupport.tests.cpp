// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanFeatureLevelSupport.hpp"
#include "containers/arrays/Vector.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"                // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "VulkanThirdParty.hpp"           // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Returns the features every `SM5` device must report.
/// @return Feature set meeting `SM5` but not the `SM6` extras.
[[nodiscard]] VkPhysicalDeviceFeatures makeShaderModel5Features() noexcept
{
    VkPhysicalDeviceFeatures features{};
    features.independentBlend = VK_TRUE;
    features.imageCubeArray = VK_TRUE;
    features.samplerAnisotropy = VK_TRUE;
    features.fragmentStoresAndAtomics = VK_TRUE;
    features.shaderImageGatherExtended = VK_TRUE;
    features.fullDrawIndexUint32 = VK_TRUE;
    return features;
}

/// @brief Returns the features every `SM6` device must report.
/// @return Feature set meeting `SM6`.
[[nodiscard]] VkPhysicalDeviceFeatures makeShaderModel6Features() noexcept
{
    VkPhysicalDeviceFeatures features = makeShaderModel5Features();
    features.shaderInt64 = VK_TRUE;
    features.shaderInt16 = VK_TRUE;
    features.multiDrawIndirect = VK_TRUE;
    features.drawIndirectFirstInstance = VK_TRUE;
    return features;
}

/// @brief Builds a complete probe whose only adapter is selected.
/// @param[in] apiVersion Usable API version of the adapter.
/// @param[in] features Core features of the adapter.
/// @return Probe with one selected adapter.
[[nodiscard]] VulkanAdapterProbe
    makeProbe(const gp::UInt32 apiVersion, const VkPhysicalDeviceFeatures& features) noexcept
{
    VulkanAdapterProbe probe{};
    probe.hasLoader = true;
    probe.isComplete = true;

    VulkanAdapterCapabilities adapter{};
    adapter.apiVersion = apiVersion;
    adapter.features = features;
    probe.adapters.pushBack(adapter);
    probe.selectedAdapter = 0;
    return probe;
}

}   // namespace

TEST(VulkanFeatureLevelRequirementsTest, ES31OnlyNeedsVulkan11)
{
    const VkPhysicalDeviceFeatures noFeatures{};

    EXPECT_FALSE(meetsVulkanFeatureLevelRequirements(FeatureLevel::ES3_1, VK_API_VERSION_1_0, noFeatures));
    EXPECT_TRUE(meetsVulkanFeatureLevelRequirements(FeatureLevel::ES3_1, VK_API_VERSION_1_1, noFeatures));
    EXPECT_TRUE(meetsVulkanFeatureLevelRequirements(FeatureLevel::ES3_1, VK_API_VERSION_1_3, noFeatures));
}

TEST(VulkanFeatureLevelRequirementsTest, SM5NeedsVulkan11AndEveryShaderModel5Feature)
{
    const VkPhysicalDeviceFeatures sm5 = makeShaderModel5Features();

    EXPECT_TRUE(meetsVulkanFeatureLevelRequirements(FeatureLevel::SM5, VK_API_VERSION_1_1, sm5));
    EXPECT_FALSE(meetsVulkanFeatureLevelRequirements(FeatureLevel::SM5, VK_API_VERSION_1_0, sm5));

    VkPhysicalDeviceFeatures missingBlend = sm5;
    missingBlend.independentBlend = VK_FALSE;
    EXPECT_FALSE(meetsVulkanFeatureLevelRequirements(FeatureLevel::SM5, VK_API_VERSION_1_3, missingBlend));

    VkPhysicalDeviceFeatures missingDrawIndex = sm5;
    missingDrawIndex.fullDrawIndexUint32 = VK_FALSE;
    EXPECT_FALSE(meetsVulkanFeatureLevelRequirements(FeatureLevel::SM5, VK_API_VERSION_1_3, missingDrawIndex));
}

TEST(VulkanFeatureLevelRequirementsTest, SM6NeedsVulkan13AndTheShaderModel6Extras)
{
    const VkPhysicalDeviceFeatures sm6 = makeShaderModel6Features();

    EXPECT_TRUE(meetsVulkanFeatureLevelRequirements(FeatureLevel::SM6, VK_API_VERSION_1_3, sm6));
    EXPECT_FALSE(meetsVulkanFeatureLevelRequirements(FeatureLevel::SM6, VK_API_VERSION_1_2, sm6));
    EXPECT_FALSE(
        meetsVulkanFeatureLevelRequirements(FeatureLevel::SM6, VK_API_VERSION_1_3, makeShaderModel5Features())
    );

    VkPhysicalDeviceFeatures missingInt16 = sm6;
    missingInt16.shaderInt16 = VK_FALSE;
    EXPECT_FALSE(meetsVulkanFeatureLevelRequirements(FeatureLevel::SM6, VK_API_VERSION_1_3, missingInt16));
}

TEST(VulkanFeatureLevelRequirementsTest, RemovedTiersAndSentinelAreNeverMet)
{
    const VkPhysicalDeviceFeatures sm6 = makeShaderModel6Features();

    EXPECT_FALSE(meetsVulkanFeatureLevelRequirements(FeatureLevel::ES2Removed, VK_API_VERSION_1_3, sm6));
    EXPECT_FALSE(meetsVulkanFeatureLevelRequirements(FeatureLevel::SM4Removed, VK_API_VERSION_1_3, sm6));
    EXPECT_FALSE(meetsVulkanFeatureLevelRequirements(FeatureLevel::Count, VK_API_VERSION_1_3, sm6));
}

TEST(VulkanFeatureLevelRequirementsTest, MaxFeatureLevelPicksTheHighestTierMet)
{
    EXPECT_EQ(getMaxVulkanFeatureLevel(VK_API_VERSION_1_3, makeShaderModel6Features()), FeatureLevel::SM6);
    EXPECT_EQ(getMaxVulkanFeatureLevel(VK_API_VERSION_1_3, makeShaderModel5Features()), FeatureLevel::SM5);
    EXPECT_EQ(getMaxVulkanFeatureLevel(VK_API_VERSION_1_2, makeShaderModel6Features()), FeatureLevel::SM5);
    EXPECT_EQ(getMaxVulkanFeatureLevel(VK_API_VERSION_1_1, VkPhysicalDeviceFeatures{}), FeatureLevel::ES3_1);
    EXPECT_EQ(getMaxVulkanFeatureLevel(VK_API_VERSION_1_0, makeShaderModel6Features()), FeatureLevel::Count);
}

TEST(VulkanAdapterProbeTest, SelectedAdapterIsBoundsChecked)
{
    VulkanAdapterProbe probe = makeProbe(VK_API_VERSION_1_3, makeShaderModel6Features());
    ASSERT_NE(probe.getSelectedAdapter(), nullptr);
    EXPECT_EQ(probe.getSelectedAdapter(), &probe.adapters[0u]);

    probe.selectedAdapter = -1;
    EXPECT_EQ(probe.getSelectedAdapter(), nullptr);

    probe.selectedAdapter = 1;
    EXPECT_EQ(probe.getSelectedAdapter(), nullptr);
}

TEST(VulkanAdapterProbeTest, BackendAvailabilityNeedsTheLoaderAndASelectedAdapter)
{
    VulkanAdapterProbe noLoader{};
    noLoader.isComplete = true;
    EXPECT_FALSE(isVulkanBackendAvailable(noLoader));

    VulkanAdapterProbe incomplete{};
    incomplete.hasLoader = true;
    EXPECT_TRUE(isVulkanBackendAvailable(incomplete));

    VulkanAdapterProbe noAdapter{};
    noAdapter.hasLoader = true;
    noAdapter.isComplete = true;
    EXPECT_FALSE(isVulkanBackendAvailable(noAdapter));

    EXPECT_TRUE(isVulkanBackendAvailable(makeProbe(VK_API_VERSION_1_1, VkPhysicalDeviceFeatures{})));
}

TEST(VulkanAdapterProbeTest, FeatureLevelAvailabilityRefusesRemovedTiersAndEditorMobile)
{
    const VulkanAdapterProbe probe = makeProbe(VK_API_VERSION_1_3, makeShaderModel6Features());

    EXPECT_FALSE(isVulkanFeatureLevelAvailable(FeatureLevel::ES2Removed, probe, true, false));
    EXPECT_FALSE(isVulkanFeatureLevelAvailable(FeatureLevel::SM4Removed, probe, true, false));
    EXPECT_FALSE(isVulkanFeatureLevelAvailable(FeatureLevel::ES3_1, probe, true, true));
    EXPECT_TRUE(isVulkanFeatureLevelAvailable(FeatureLevel::ES3_1, probe, true, false));
}

TEST(VulkanAdapterProbeTest, FeatureLevelAvailabilityChecksTheSelectedAdapterWhenEnforced)
{
    const VulkanAdapterProbe sm5Probe = makeProbe(VK_API_VERSION_1_3, makeShaderModel5Features());

    EXPECT_TRUE(isVulkanFeatureLevelAvailable(FeatureLevel::SM5, sm5Probe, true, false));
    EXPECT_FALSE(isVulkanFeatureLevelAvailable(FeatureLevel::SM6, sm5Probe, true, false));
    EXPECT_TRUE(isVulkanFeatureLevelAvailable(FeatureLevel::Count, sm5Probe, true, false));

    // Without profile checks, any available backend is accepted.
    EXPECT_TRUE(isVulkanFeatureLevelAvailable(FeatureLevel::SM6, sm5Probe, false, false));
}

TEST(VulkanAdapterProbeTest, FeatureLevelAvailabilityTrustsIncompleteProbes)
{
    VulkanAdapterProbe incomplete{};
    incomplete.hasLoader = true;
    EXPECT_TRUE(isVulkanFeatureLevelAvailable(FeatureLevel::SM6, incomplete, true, false));

    VulkanAdapterProbe noLoader{};
    EXPECT_FALSE(isVulkanFeatureLevelAvailable(FeatureLevel::SM5, noLoader, false, false));
    EXPECT_FALSE(isVulkanFeatureLevelAvailable(FeatureLevel::Count, noLoader, false, false));
}

TEST(VulkanFeatureLevelResolutionTest, ConcreteRequestsAreKept)
{
    const VulkanAdapterProbe probe = makeProbe(VK_API_VERSION_1_3, makeShaderModel6Features());

    EXPECT_EQ(resolveVulkanFeatureLevel(FeatureLevel::ES3_1, probe), FeatureLevel::ES3_1);
    EXPECT_EQ(resolveVulkanFeatureLevel(FeatureLevel::SM5, probe), FeatureLevel::SM5);
    EXPECT_EQ(resolveVulkanFeatureLevel(FeatureLevel::SM6, VulkanAdapterProbe{}), FeatureLevel::SM6);
}

TEST(VulkanFeatureLevelResolutionTest, BestAvailableFollowsTheSelectedAdapter)
{
    EXPECT_EQ(
        resolveVulkanFeatureLevel(FeatureLevel::Count, makeProbe(VK_API_VERSION_1_3, makeShaderModel6Features())),
        FeatureLevel::SM6
    );
    EXPECT_EQ(
        resolveVulkanFeatureLevel(FeatureLevel::Count, makeProbe(VK_API_VERSION_1_1, makeShaderModel5Features())),
        FeatureLevel::SM5
    );
    EXPECT_EQ(
        resolveVulkanFeatureLevel(FeatureLevel::Count, makeProbe(VK_API_VERSION_1_1, VkPhysicalDeviceFeatures{})),
        FeatureLevel::ES3_1
    );
}

TEST(VulkanFeatureLevelResolutionTest, BestAvailableFallsBackToSM5)
{
    VulkanAdapterProbe incomplete = makeProbe(VK_API_VERSION_1_3, makeShaderModel6Features());
    incomplete.isComplete = false;
    EXPECT_EQ(resolveVulkanFeatureLevel(FeatureLevel::Count, incomplete), FeatureLevel::SM5);

    VulkanAdapterProbe unselected = makeProbe(VK_API_VERSION_1_3, makeShaderModel6Features());
    unselected.selectedAdapter = -1;
    EXPECT_EQ(resolveVulkanFeatureLevel(FeatureLevel::Count, unselected), FeatureLevel::SM5);

    EXPECT_EQ(
        resolveVulkanFeatureLevel(FeatureLevel::Count, makeProbe(VK_API_VERSION_1_0, VkPhysicalDeviceFeatures{})),
        FeatureLevel::SM5
    );
}

TEST(VulkanAdapterProbeTest, HostProbeIsStableAcrossCacheResets)
{
    const VulkanAdapterProbe first = getVulkanAdapterProbe();
    const VulkanAdapterProbe cached = getVulkanAdapterProbe();
    resetVulkanAdapterProbeCache();
    const VulkanAdapterProbe reprobed = getVulkanAdapterProbe();

    EXPECT_EQ(first.hasLoader, cached.hasLoader);
    EXPECT_EQ(first.hasLoader, reprobed.hasLoader);
    EXPECT_EQ(isVulkanBackendAvailable(first), isVulkanBackendAvailable(reprobed));
    if (first.isComplete && reprobed.isComplete)
    {
        EXPECT_EQ(first.adapters.size(), reprobed.adapters.size());
        EXPECT_EQ(first.selectedAdapter, reprobed.selectedAdapter);
    }
}

}   // namespace gp::rhi::vulkan::tests
