// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "profiling/ProfilingControl.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "profiling/Profiler.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace gp::profiling::tests
{

namespace
{

class ProfilingControlTest : public ::testing::Test
{
protected:
    virtual void TearDown() noexcept override
    {
        gp::console::IConsoleManager& console = gp::console::IConsoleManager::get();
        const char* const resetNames[] = { "prof.Enabled", "prof.Plots", "prof.TraceConsole", "prof.HitchThresholdMs" };
        for (const char* const name: resetNames)
        {
            if (gp::console::IConsoleVariable* const var = console.findConsoleVariable(name))
            {
                (void)var->reset(gp::console::ConsoleVarSetBy::Console);
            }
        }
    }
};

}   // namespace

TEST_F(ProfilingControlTest, RuntimeProfileCVarsUpdateAtomicsAndClampHitchThreshold)
{
    gp::console::IConsoleManager& console = gp::console::IConsoleManager::get();

    EXPECT_TRUE(ProfilingControl::isZonesEnabled());
    EXPECT_TRUE(ProfilingControl::isPlotsEnabled());
    EXPECT_TRUE(ProfilingControl::isConsoleTraceEnabled());
    EXPECT_DOUBLE_EQ(ProfilingControl::getHitchThresholdMs(), 0.0);

    EXPECT_EQ(console.processUserConsoleInput("prof.Enabled 0"), gp::console::ConsoleResult::Success);
    EXPECT_FALSE(ProfilingControl::isZonesEnabled());
    EXPECT_EQ(console.processUserConsoleInput("prof.Enabled 1"), gp::console::ConsoleResult::Success);
    EXPECT_TRUE(ProfilingControl::isZonesEnabled());

    EXPECT_EQ(console.processUserConsoleInput("prof.Plots 0"), gp::console::ConsoleResult::Success);
    EXPECT_FALSE(ProfilingControl::isPlotsEnabled());

    EXPECT_EQ(console.processUserConsoleInput("prof.TraceConsole=0"), gp::console::ConsoleResult::Success);
    EXPECT_FALSE(ProfilingControl::isConsoleTraceEnabled());

    // Negative hitch threshold clamps atomically to 0.0 (disabled), while positive threshold enables detection.
    EXPECT_EQ(console.processUserConsoleInput("prof.HitchThresholdMs -5.0"), gp::console::ConsoleResult::Success);
    EXPECT_DOUBLE_EQ(ProfilingControl::getHitchThresholdMs(), 0.0);
    EXPECT_FALSE(ProfilingControl::recordFrameHitchIfExceeded(100.0, 1ULL));

    EXPECT_EQ(console.processUserConsoleInput("prof.HitchThresholdMs 16.5"), gp::console::ConsoleResult::Success);
    EXPECT_DOUBLE_EQ(ProfilingControl::getHitchThresholdMs(), 16.5);
    EXPECT_FALSE(ProfilingControl::recordFrameHitchIfExceeded(12.0, 2ULL));
    EXPECT_TRUE(ProfilingControl::recordFrameHitchIfExceeded(16.5, 3ULL));
    EXPECT_TRUE(ProfilingControl::recordFrameHitchIfExceeded(42.25, 4ULL));

    // Non-finite values are rejected cleanly by the underlying Float64 CVar without corrupting state.
    gp::console::IConsoleVariable* const hitchVar = console.findConsoleVariable("prof.HitchThresholdMs");
    ASSERT_NE(hitchVar, nullptr);
    EXPECT_EQ(
        hitchVar->set(std::numeric_limits<gp::Float64>::quiet_NaN()), gp::console::ConsoleResult::InvalidArguments
    );
    EXPECT_DOUBLE_EQ(ProfilingControl::getHitchThresholdMs(), 16.5);
}

TEST_F(ProfilingControlTest, CommandsAndActiveConditionalMacrosExecuteCleanly)
{
    gp::console::IConsoleManager& console = gp::console::IConsoleManager::get();

    gp::FixedString<511> lastOutput{};
    gp::console::ConsoleOutputDelegate out = gp::console::ConsoleOutputDelegate::createLambda(
        [&lastOutput](const gp::StringView line) noexcept -> void
    {
        lastOutput.assign(line.substr(0u, lastOutput.capacity()));
    }
    );

    // Bookmark with default fallback and multi-token label.
    lastOutput.clear();
    EXPECT_EQ(console.processUserConsoleInput("prof.Bookmark", &out), gp::console::ConsoleResult::Success);
    EXPECT_EQ(lastOutput.view(), gp::StringView("[Tracy] Bookmark: UserBookmark"));

    lastOutput.clear();
    EXPECT_EQ(
        console.processUserConsoleInput("prof.Bookmark Scene Load Phase", &out), gp::console::ConsoleResult::Success
    );
    EXPECT_EQ(lastOutput.view(), gp::StringView("[Tracy] Bookmark: Scene Load Phase"));

    // Status command formats active telemetry switches cleanly.
    lastOutput.clear();
    EXPECT_EQ(console.processUserConsoleInput("prof.Status", &out), gp::console::ConsoleResult::Success);
    EXPECT_NE(lastOutput.view().find("Profiler["), gp::StringView::npos);
    EXPECT_NE(lastOutput.view().find("enabled=true"), gp::StringView::npos);

    // Empty bookmark label safely no-ops without crashing.
    ProfilingControl::emitBookmark(gp::StringView{});

    // Conditional macros compile and evaluate cleanly under both active and inactive states (including nested).
    for (gp::Int32 mode = 0; mode < 2; ++mode)
    {
        const bool active = (mode == 1);
        {
            GP_PROFILE_SCOPE_ACTIVE(active);
        }
        {
            GP_PROFILE_SCOPE_N_ACTIVE("TestOuter", active);
            {
                GP_PROFILE_SCOPE_C_ACTIVE(GP_U32_COLOR_GREEN, active);
                {
                    GP_PROFILE_SCOPE_NC_ACTIVE("TestInner", GP_U32_COLOR_BLUE, active);
                    GP_PROFILE_ZONE_VALUE(static_cast<gp::UInt64>(mode));
                }
            }
        }
        GP_PLOT_IF(active, "Test::PlotMetric", static_cast<gp::Int64>(mode));
        GP_MESSAGE_LC_IF(active, "TestActiveMessage", GP_U32_COLOR_CYAN);
        GP_MESSAGE_C_IF(active, "Msg", 3u, GP_U32_COLOR_WHITE);
    }
}

}   // namespace gp::profiling::tests
