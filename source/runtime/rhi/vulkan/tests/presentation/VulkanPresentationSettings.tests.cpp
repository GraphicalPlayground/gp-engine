// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanPresentationSettings.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "CoreMinimal.hpp"
#include "presentation/VulkanPresentationEnums.hpp"
#include "presentation/VulkanPresentationTypes.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::vulkan::tests
{

TEST(VulkanPresentationSettingsTest, DefaultsMatchConsoleVariableDefaults)
{
    const VulkanPresentationConfig config = resolveVulkanPresentationConfig();

#if defined(GP_PLATFORM_ANDROID) && GP_PLATFORM_ANDROID
    EXPECT_FALSE(config.keepSwapChain);
#else
    EXPECT_TRUE(config.keepSwapChain);
#endif
    EXPECT_FALSE(config.cpuWaitForFence);
    EXPECT_FALSE(config.cpuFramePacer);
    EXPECT_FALSE(config.forcePacingWithoutVSync);
    EXPECT_TRUE(config.allowPreTransform);
    EXPECT_EQ(config.framePacerRefreshRate, 60u);
    EXPECT_EQ(config.displayOutputFormat, VulkanDisplayOutputFormat::SdrSrgb);
    EXPECT_EQ(config.presentModeOverride, -1);
}

TEST(VulkanPresentationSettingsTest, OwnedConsoleVariablesAreRegistered)
{
    gp::console::IConsoleManager& manager = gp::console::IConsoleManager::get();

    EXPECT_NE(manager.findConsoleVariable("r.Vulkan.KeepSwapChain"), nullptr);
    EXPECT_NE(manager.findConsoleVariable("r.Vulkan.CpuWaitForFence"), nullptr);
    EXPECT_NE(manager.findConsoleVariable("r.Vulkan.CPURHIThreadFramePacer"), nullptr);
    EXPECT_NE(manager.findConsoleVariable("r.Vulkan.ForcePacingWithoutVSync"), nullptr);
    EXPECT_NE(manager.findConsoleVariable("r.Vulkan.FramePacerRefreshRate"), nullptr);
    EXPECT_NE(manager.findConsoleVariable("r.Vulkan.PreTransform"), nullptr);
}

TEST(VulkanPresentationSettingsTest, ConsoleVariableChangesAreResolved)
{
    gp::console::IConsoleVariable* const preTransform =
        gp::console::IConsoleManager::get().findConsoleVariable("r.Vulkan.PreTransform");
    gp::console::IConsoleVariable* const refreshRate =
        gp::console::IConsoleManager::get().findConsoleVariable("r.Vulkan.FramePacerRefreshRate");
    ASSERT_NE(preTransform, nullptr);
    ASSERT_NE(refreshRate, nullptr);

    static_cast<void>(preTransform->set(static_cast<gp::Int32>(0)));
    static_cast<void>(refreshRate->set(static_cast<gp::Int32>(0)));
    const VulkanPresentationConfig config = resolveVulkanPresentationConfig();
    static_cast<void>(preTransform->reset());
    static_cast<void>(refreshRate->reset());

    EXPECT_FALSE(config.allowPreTransform);
    // A non-positive refresh rate would divide by zero in the pacer: it falls back to 60 Hz.
    EXPECT_EQ(config.framePacerRefreshRate, 60u);

    const VulkanPresentationConfig restored = resolveVulkanPresentationConfig();
    EXPECT_TRUE(restored.allowPreTransform);
}

}   // namespace gp::rhi::vulkan::tests
