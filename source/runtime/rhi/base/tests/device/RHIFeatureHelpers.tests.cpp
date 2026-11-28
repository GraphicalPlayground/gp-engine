// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/RHIFeatureHelpers.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "miscellaneous/CommandLine.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::tests
{

namespace
{

struct ScopedCommandLine final
{
    bool wasInitialized{ false };
    gp::String savedExecutable{};
    gp::String savedArgs{};

    explicit ScopedCommandLine(const gp::StringView commandLine) noexcept
        : wasInitialized(gp::CommandLine::isInitialized())
    {
        if (wasInitialized)
        {
            savedExecutable = gp::String(gp::CommandLine::getExecutablePath());
            savedArgs = gp::String(gp::CommandLine::getOriginalCommandLine());
        }
        gp::CommandLine::initialize(commandLine);
    }

    ~ScopedCommandLine() noexcept
    {
        if (wasInitialized)
        {
            gp::String combined = savedExecutable;
            if (!savedArgs.isEmpty())
            {
                combined.pushBack(' ');
                combined.append(savedArgs.view());
            }
            gp::CommandLine::initialize(combined.view());
        }
        else
        {
            gp::CommandLine::shutdown();
        }
    }
};

}   // namespace

TEST(RHIFeatureHelpersTest, RegisteredConsoleVariablesAndDefaults)
{
    gp::console::IConsoleManager& consoleManager = gp::console::IConsoleManager::get();
    if (!consoleManager.isInitialized())
    {
        consoleManager.initialize();
    }

    gp::console::IConsoleVariable* const cvarAtomic64 = consoleManager.findConsoleVariable("r.SM6.RequireAtomic64");
    ASSERT_NE(cvarAtomic64, nullptr);
    EXPECT_TRUE(cvarAtomic64->getBool());
    EXPECT_NE(static_cast<gp::UInt32>(cvarAtomic64->getFlags() & gp::console::ConsoleVarFlags::ReadOnly), 0u);

    gp::console::IConsoleVariable* const cvarVendor = consoleManager.findConsoleVariable("rhi.AllowVendorDevice");
    ASSERT_NE(cvarVendor, nullptr);
    EXPECT_TRUE(cvarVendor->getBool());
    EXPECT_NE(static_cast<gp::UInt32>(cvarVendor->getFlags() & gp::console::ConsoleVarFlags::ReadOnly), 0u);

    gp::console::IConsoleVariable* const cvarRayTracing =
        consoleManager.findConsoleVariable("rhi.EnableRayTracingShaders");
    ASSERT_NE(cvarRayTracing, nullptr);
    EXPECT_TRUE(cvarRayTracing->getBool());
    EXPECT_NE(static_cast<gp::UInt32>(cvarRayTracing->getFlags() & gp::console::ConsoleVarFlags::ReadOnly), 0u);
}

TEST(RHIFeatureHelpersTest, DefaultsOverridesCommandLineAndBindlessStartup)
{
    const bool prevAtomic64 = isAtomic64RequiredForSM6();
    const bool prevVendor = allowVendorDevice();
    const bool prevRayTracing = areRayTracingShadersEnabled();
    const view::BindlessConfiguration prevBindless =
        getBindlessConfigurationOnStartup(static_cast<shader::ShaderPlatform>(0u));

    EXPECT_TRUE(prevAtomic64);
    EXPECT_TRUE(prevVendor);
    EXPECT_TRUE(prevRayTracing);
    EXPECT_EQ(prevBindless, view::BindlessConfiguration::Disabled);

    setAtomic64RequiredForSM6(false);
    setAllowVendorDevice(false);
    setRayTracingShadersEnabled(false);
    setBindlessConfigurationOnStartup(view::BindlessConfiguration::All);

    EXPECT_FALSE(isAtomic64RequiredForSM6());
    EXPECT_FALSE(allowVendorDevice());
    EXPECT_FALSE(areRayTracingShadersEnabled());
    EXPECT_EQ(
        getBindlessConfigurationOnStartup(static_cast<shader::ShaderPlatform>(0u)), view::BindlessConfiguration::All
    );

    // Re-enabling allowVendorDevice works when -novendordevice is not on the command line.
    setAllowVendorDevice(true);
    EXPECT_TRUE(allowVendorDevice());

    // Passing -novendordevice on the command line disables vendor devices even when CVar/override is true.
    {
        const ScopedCommandLine cmdGuard("gp_test.exe -novendordevice");
        EXPECT_FALSE(allowVendorDevice());
    }
    EXPECT_TRUE(allowVendorDevice());

    setAtomic64RequiredForSM6(prevAtomic64);
    setAllowVendorDevice(prevVendor);
    setRayTracingShadersEnabled(prevRayTracing);
    setBindlessConfigurationOnStartup(prevBindless);

    EXPECT_EQ(isAtomic64RequiredForSM6(), prevAtomic64);
    EXPECT_EQ(allowVendorDevice(), prevVendor);
    EXPECT_EQ(areRayTracingShadersEnabled(), prevRayTracing);
    EXPECT_EQ(getBindlessConfigurationOnStartup(static_cast<shader::ShaderPlatform>(0u)), prevBindless);
}

}   // namespace gp::rhi::tests
