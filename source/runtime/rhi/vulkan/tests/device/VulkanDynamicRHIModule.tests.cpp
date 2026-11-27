// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/VulkanDynamicRHIModule.hpp"
#include "CoreMinimal.hpp"   // IWYU pragma: keep
#include "device/DeviceEnums.hpp"
#include "device/DynamicRHI.hpp"
#include "device/IDynamicRHIModule.hpp"
#include "device/RHIGlobals.hpp"
#include "device/VulkanDynamicRHI.hpp"
#include "device/VulkanFeatureLevelSupport.hpp"
#include "platform/VulkanPlatform.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

namespace
{

/// @brief Restores `RHIGlobals::maxSupportedFeatureLevel` when a test that calls `createRHI()` ends.
class VulkanDynamicRHIModuleTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        m_previousFeatureLevel = getRHIGlobals().maxSupportedFeatureLevel;
    }

    virtual void TearDown() override
    {
        getRHIGlobals().maxSupportedFeatureLevel = m_previousFeatureLevel;
    }

    /// @brief Module under test.
    VulkanDynamicRHIModule m_module{};

    /// @brief Feature level published before the test.
    FeatureLevel m_previousFeatureLevel{ FeatureLevel::SM5 };
};

}   // namespace

TEST_F(VulkanDynamicRHIModuleTest, ReportsTheRHIModuleInterface)
{
    const IDynamicRHIModule& asModule = m_module;

    EXPECT_EQ(asModule.getInterfaceTag(), IDynamicRHIModule::kInterfaceTag);
    EXPECT_FALSE(m_module.supportsDynamicReloading());
}

TEST_F(VulkanDynamicRHIModuleTest, SupportMatchesTheAdapterProbe)
{
    const bool expected = VulkanPlatform::isSupported() && isVulkanBackendAvailable(getVulkanAdapterProbe());

    EXPECT_EQ(m_module.isSupported(), expected);
}

TEST_F(VulkanDynamicRHIModuleTest, RemovedFeatureLevelsAreNeverSupported)
{
    EXPECT_FALSE(m_module.isSupported(FeatureLevel::ES2Removed));
    EXPECT_FALSE(m_module.isSupported(FeatureLevel::SM4Removed));
}

TEST_F(VulkanDynamicRHIModuleTest, FeatureLevelSupportImpliesBackendSupport)
{
    const bool backendSupported = m_module.isSupported();
    const FeatureLevel levels[] = { FeatureLevel::ES3_1, FeatureLevel::SM5, FeatureLevel::SM6, FeatureLevel::Count };

    for (const FeatureLevel level: levels)
    {
        if (m_module.isSupported(level))
        {
            EXPECT_TRUE(backendSupported) << "Feature level " << static_cast<gp::Int32>(level);
        }
    }
    EXPECT_EQ(m_module.isSupported(FeatureLevel::Count), backendSupported);
}

TEST_F(VulkanDynamicRHIModuleTest, CreateRHIReturnsAnUninitializedVulkanRHI)
{
    DynamicRHI* const rhi = m_module.createRHI(FeatureLevel::SM5);
    ASSERT_NE(rhi, nullptr);

    EXPECT_STREQ(rhi->getName(), "Vulkan");
    EXPECT_EQ(rhi->getInterfaceType(), InterfaceType::Vulkan);
    EXPECT_FALSE(static_cast<VulkanDynamicRHI*>(rhi)->isInitialized());
    EXPECT_EQ(
        getRHIGlobals().maxSupportedFeatureLevel,
        resolveVulkanFeatureLevel(VulkanPlatform::getFeatureLevel(FeatureLevel::SM5), getVulkanAdapterProbe())
    );

    delete rhi;
}

TEST_F(VulkanDynamicRHIModuleTest, CreateRHIForBestAvailablePublishesAConcreteLevel)
{
    DynamicRHI* const rhi = m_module.createRHI(FeatureLevel::Count);
    ASSERT_NE(rhi, nullptr);

    const FeatureLevel published = getRHIGlobals().maxSupportedFeatureLevel;
    EXPECT_NE(published, FeatureLevel::Count);
    EXPECT_NE(published, FeatureLevel::ES2Removed);
    EXPECT_NE(published, FeatureLevel::SM4Removed);

    delete rhi;
}

}   // namespace gp::rhi::vulkan::tests
