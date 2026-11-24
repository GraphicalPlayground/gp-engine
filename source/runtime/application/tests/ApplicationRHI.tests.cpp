// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "Application.hpp"
#include "ApplicationDesc.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "containers/strings/FixedString.hpp"
#include "device/DynamicRHI.hpp"
#include "device/RHIGlobals.hpp"
#include "device/RHIModuleDiscovery.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "modules/ModuleManager.hpp"
#include <gtest/gtest.h>

namespace gp::tests
{

namespace
{

/// @brief Minimal headless application capturing RHI readiness and `rhi.Status` output during lifecycle hooks.
class RHIBootstrapProbeApp final : public Application
{
public:
    bool rhiReadyInInitialize{ false };
    bool rhiReadyInShutdown{ false };
    bool usingNullRHIInInitialize{ false };
    gp::FixedString<63> loadedModuleName{};
    gp::FixedString<255> rhiStatusOutput{};
    console::ConsoleResult rhiStatusResult{ console::ConsoleResult::NotFound };

public:
    explicit RHIBootstrapProbeApp(const ApplicationDesc& desc) noexcept
        : Application(desc)
    {}

    virtual ~RHIBootstrapProbeApp() noexcept override = default;

protected:
    [[nodiscard]] virtual bool onInitialize() noexcept override
    {
        rhiReadyInInitialize = isRHIInitialized();
        if (rhiReadyInInitialize)
        {
            usingNullRHIInInitialize = rhi::getRHIGlobals().usingNullRHI;
            loadedModuleName.assign(getLoadedRHIModuleName());
        }

        console::ConsoleOutputDelegate outputDelegate = console::ConsoleOutputDelegate::createLambda(
            [this](const gp::StringView line) noexcept -> void
        {
            rhiStatusOutput.assign(line);
        }
        );
        rhiStatusResult = executeConsoleCommand("rhi.Status", &outputDelegate);
        return true;
    }

    virtual void onUpdate(const gp::time::FrameTimeStats&) noexcept override
    {
        requestQuit();
    }

    virtual void onShutdown() noexcept override
    {
        rhiReadyInShutdown = isRHIInitialized();
    }
};

ApplicationDesc makeHeadlessRHIAppDesc() noexcept
{
    ApplicationDesc desc{};
    desc.createMainWindow = false;
    desc.enableJobSystem = false;
    desc.enableLogging = false;
    desc.enableVirtualFileSystem = false;
    desc.rhi.forceNullRHI = true;
    desc.rhi.applyCommandLineOverrides = false;
    return desc;
}

class ApplicationRHITest : public ::testing::Test
{
protected:
    virtual void SetUp() noexcept override
    {
        rhi::rhiExit();
        gp::CommandLine::shutdown();
    }

    virtual void TearDown() noexcept override
    {
        rhi::rhiExit();
        gp::CommandLine::shutdown();
    }
};

}   // namespace

TEST_F(ApplicationRHITest, BootstrapsNullRHIAndTearsDownBeforeModuleUnload)
{
    ApplicationDesc desc = makeHeadlessRHIAppDesc();
    desc.enableRHI = true;

    RHIBootstrapProbeApp app(desc);
    const gp::Int32 exitCode = app.run();

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(app.rhiReadyInInitialize);
    EXPECT_TRUE(app.rhiReadyInShutdown);
    EXPECT_TRUE(app.usingNullRHIInInitialize);
    EXPECT_EQ(app.loadedModuleName.view(), rhi::kNullRHIModuleName);
    EXPECT_EQ(app.rhiStatusResult, console::ConsoleResult::Success);
    EXPECT_TRUE(app.rhiStatusOutput.view().contains("rhi/null"));

    // Application owned RHI initialization, so rhiExit() must have run cleanly during shutdown.
    EXPECT_FALSE(Application::isRHIInitialized());
    EXPECT_EQ(Application::getDynamicRHI(), nullptr);
    EXPECT_TRUE(Application::getLoadedRHIModuleName().isEmpty());
}

TEST_F(ApplicationRHITest, DisabledRHISkipsInitialization)
{
    ApplicationDesc desc = makeHeadlessRHIAppDesc();
    desc.enableRHI = false;

    RHIBootstrapProbeApp app(desc);
    const gp::Int32 exitCode = app.run();

    EXPECT_EQ(exitCode, 0);
    EXPECT_FALSE(app.rhiReadyInInitialize);
    EXPECT_FALSE(app.rhiReadyInShutdown);
    EXPECT_EQ(app.rhiStatusResult, console::ConsoleResult::Success);
    EXPECT_TRUE(app.rhiStatusOutput.view().contains("not initialized"));
    EXPECT_FALSE(Application::isRHIInitialized());
}

TEST_F(ApplicationRHITest, CommandLineNoRHIDisablesRHI)
{
    gp::CommandLine::initialize("app.exe -no-rhi");

    ApplicationDesc desc = makeHeadlessRHIAppDesc();
    desc.enableRHI = true;

    RHIBootstrapProbeApp app(desc);
    const gp::Int32 exitCode = app.run();

    EXPECT_EQ(exitCode, 0);
    EXPECT_FALSE(app.rhiReadyInInitialize);
    EXPECT_FALSE(Application::isRHIInitialized());
}

TEST_F(ApplicationRHITest, PreservesExternallyInitializedRHI)
{
    // Pre-initialize RHI before Application::run() starts.
    rhi::RHIBootstrapDesc preDesc{};
    preDesc.forceNullRHI = true;
    preDesc.applyCommandLineOverrides = false;
    ASSERT_TRUE(rhi::rhiInit(preDesc, modules::ModuleManager::get()));
    rhi::DynamicRHI* const preDevice = rhi::getDynamicRHI();
    ASSERT_NE(preDevice, nullptr);

    ApplicationDesc desc = makeHeadlessRHIAppDesc();
    desc.enableRHI = true;
    desc.unloadModulesOnShutdown = true;

    RHIBootstrapProbeApp app(desc);
    const gp::Int32 exitCode = app.run();

    EXPECT_EQ(exitCode, 0);
    EXPECT_TRUE(app.rhiReadyInInitialize);
    EXPECT_TRUE(app.rhiReadyInShutdown);
    EXPECT_EQ(Application::getDynamicRHI(), preDevice);
    EXPECT_TRUE(modules::ModuleManager::get().isModuleLoaded(rhi::kNullRHIModuleName));

    // Tear down externally owned RHI cleanly and unload the null backend module afterward.
    rhi::rhiExit();
    EXPECT_FALSE(Application::isRHIInitialized());
    (void)modules::ModuleManager::get().unloadModule(rhi::kNullRHIModuleName, modules::ModuleShutdownPhase::Exit);
}

TEST_F(ApplicationRHITest, FailsStartupCleanlyWhenForcedRHICannotLoad)
{
    ApplicationDesc desc = makeHeadlessRHIAppDesc();
    desc.enableRHI = true;
    desc.rhi.forceNullRHI = false;
    desc.rhi.preferredModuleName = "rhi/nonexistent-backend";
    desc.rhi.allowNullFallback = false;
    desc.rhi.forcedModuleByCommandLine = true;

    RHIBootstrapProbeApp app(desc);
    const gp::Int32 exitCode = app.run();

    EXPECT_EQ(exitCode, -1);
    EXPECT_FALSE(app.rhiReadyInInitialize);
    EXPECT_FALSE(Application::isRHIInitialized());
}

}   // namespace gp::tests
