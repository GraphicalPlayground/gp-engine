// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "console/ImGuiConsole.hpp"
#include "console/ConsoleDesc.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleManager.hpp"   // IWYU pragma: keep
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleVariable.hpp"
#include "console/ImGuiConsoleDesc.hpp"
#include "console/ImGuiConsoleEnums.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>

namespace gp::imgui::tests
{

namespace
{

/// @brief Headless fixture: an isolated console manager with a small known registry, and an `ImGuiConsole` bound
///        to it. No ImGui context is needed because the completion/history model never touches ImGui.
class ImGuiConsoleTest : public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        gp::console::ConsoleManagerDesc managerDesc{};
        managerDesc.enableConsole = true;
        managerDesc.enableCheats = true;
        managerDesc.applyCommandLineOverrides = false;
        managerDesc.registerBuiltInCommands = false;
        managerDesc.registerDeferredStaticObjects = false;
        ASSERT_EQ(m_manager.initialize(managerDesc), gp::console::ConsoleResult::Success);

        m_fpsLimit = m_manager.registerConsoleVariable("fps.Limit", static_cast<gp::Int32>(60), "Frame cap.");
        ASSERT_NE(m_fpsLimit, nullptr);
        ASSERT_NE(m_manager.registerConsoleVariable("fps", true, "Show the FPS counter."), nullptr);
        ASSERT_NE(m_manager.registerConsoleVariable("app.TargetFps", static_cast<gp::Int32>(120), "Pacing."), nullptr);
        ASSERT_NE(m_manager.registerConsoleVariable("r.VSync", false, "Lock to vblank."), nullptr);
        ASSERT_NE(
            m_manager.registerConsoleCommand(
                "app.Quit",
                "Requests shutdown.",
                gp::console::ConsoleCommandDelegate::createLambda(
                    []() noexcept -> void
        {
        }
                )
            ),
            nullptr
        );

        initializeConsole(gp::imgui::ImGuiConsoleDesc{});
    }

    virtual void TearDown() override
    {
        m_console.shutdown();
        m_manager.shutdown();
    }

    /// @brief (Re)binds the console with @p desc and clears the startup banner so tests see only their own lines.
    void initializeConsole(const gp::imgui::ImGuiConsoleDesc& desc)
    {
        m_console.initialize(&m_manager, desc);
        m_console.clearLog();
    }

    /// @brief Submits @p line as if typed and confirmed with Enter.
    gp::console::ConsoleResult run(const gp::StringView line)
    {
        m_console.setInput(line);
        return m_console.submit();
    }

protected:
    gp::console::ConsoleManager m_manager{};
    gp::console::IConsoleVariable* m_fpsLimit{ nullptr };
    gp::imgui::ImGuiConsole m_console{};
};

}   // namespace

TEST_F(ImGuiConsoleTest, EmptyInputHasNoSuggestions)
{
    m_console.setInput("   ");

    EXPECT_TRUE(m_console.getSuggestions().isEmpty());
    EXPECT_FALSE(m_console.isShowingArgumentHint());
}

TEST_F(ImGuiConsoleTest, ExactThenPrefixThenSubstringRanking)
{
    m_console.setInput("fps");
    const gp::Vector<ImGuiConsoleSuggestion>& suggestions = m_console.getSuggestions();

    ASSERT_EQ(suggestions.size(), 3u);
    EXPECT_EQ(suggestions[0].name, gp::StringView("fps"));
    EXPECT_EQ(suggestions[0].match, ImGuiConsoleMatchKind::Exact);
    EXPECT_EQ(suggestions[1].name, gp::StringView("fps.Limit"));
    EXPECT_EQ(suggestions[1].match, ImGuiConsoleMatchKind::Prefix);
    EXPECT_EQ(suggestions[2].name, gp::StringView("app.TargetFps"));
    EXPECT_EQ(suggestions[2].match, ImGuiConsoleMatchKind::Substring);
}

TEST_F(ImGuiConsoleTest, MatchingIsCaseInsensitive)
{
    m_console.setInput("TARGETfps");

    ASSERT_EQ(m_console.getSuggestions().size(), 1u);
    EXPECT_EQ(m_console.getSuggestions()[0].name, gp::StringView("app.TargetFps"));
}

TEST_F(ImGuiConsoleTest, CommandsAreTaggedAsCommands)
{
    m_console.setInput("quit");

    ASSERT_EQ(m_console.getSuggestions().size(), 1u);
    EXPECT_EQ(m_console.getSuggestions()[0].kind, ImGuiConsoleSuggestionKind::Command);
}

TEST_F(ImGuiConsoleTest, SuggestionListKeepsOnlyTheBestRanked)
{
    gp::imgui::ImGuiConsoleDesc desc{};
    desc.maxSuggestions = 2u;
    initializeConsole(desc);

    m_console.setInput("fps");

    ASSERT_EQ(m_console.getSuggestions().size(), 2u);
    EXPECT_EQ(m_console.getSuggestions()[0].name, gp::StringView("fps"));
    EXPECT_EQ(m_console.getSuggestions()[1].name, gp::StringView("fps.Limit"));
}

TEST_F(ImGuiConsoleTest, BrowsingWrapsAndWritesNameWithoutRefiltering)
{
    m_console.setInput("fps");

    EXPECT_TRUE(m_console.moveSuggestionSelection(+1));
    EXPECT_EQ(m_console.getSelectedSuggestion(), 0);
    EXPECT_EQ(m_console.getInput(), gp::StringView("fps"));

    EXPECT_TRUE(m_console.moveSuggestionSelection(-1));
    EXPECT_EQ(m_console.getSelectedSuggestion(), 2);
    EXPECT_EQ(m_console.getInput(), gp::StringView("app.TargetFps"));
    // The list is still the one filtered by the typed query "fps".
    EXPECT_EQ(m_console.getSuggestions().size(), 3u);
}

TEST_F(ImGuiConsoleTest, BrowsingEmptyListFails)
{
    m_console.setInput("zzz");

    EXPECT_FALSE(m_console.moveSuggestionSelection(+1));
    EXPECT_EQ(m_console.getSelectedSuggestion(), ImGuiConsole::kNoSelection);
}

TEST_F(ImGuiConsoleTest, AcceptCompletesAndSwitchesToArgumentHint)
{
    m_console.setInput("fps.l");

    EXPECT_TRUE(m_console.acceptSuggestion());
    EXPECT_EQ(m_console.getInput(), gp::StringView("fps.Limit "));
    EXPECT_TRUE(m_console.isShowingArgumentHint());
    ASSERT_EQ(m_console.getSuggestions().size(), 1u);
    EXPECT_EQ(m_console.getSuggestions()[0].object, static_cast<gp::console::IConsoleObject*>(m_fpsLimit));

    // Already complete: a second Tab is a no-op.
    EXPECT_FALSE(m_console.acceptSuggestion());
}

TEST_F(ImGuiConsoleTest, AcceptUsesHighlightedSuggestion)
{
    m_console.setInput("fps");
    ASSERT_TRUE(m_console.moveSuggestionSelection(+1));
    ASSERT_TRUE(m_console.moveSuggestionSelection(+1));

    EXPECT_TRUE(m_console.acceptSuggestion());
    EXPECT_EQ(m_console.getInput(), gp::StringView("fps.Limit "));
}

TEST_F(ImGuiConsoleTest, ArgumentHintForUnknownNameIsEmpty)
{
    m_console.setInput("does.not.exist 1");

    EXPECT_TRUE(m_console.getSuggestions().isEmpty());
    EXPECT_FALSE(m_console.isShowingArgumentHint());
}

TEST_F(ImGuiConsoleTest, SubmitSetsVariableAndEchoesInput)
{
    EXPECT_EQ(run("  fps.Limit 30  "), gp::console::ConsoleResult::Success);

    EXPECT_EQ(m_fpsLimit->getInt(), 30);
    EXPECT_TRUE(m_console.getInput().isEmpty());
    ASSERT_GE(m_console.getLineCount(), 1u);
    EXPECT_EQ(m_console.getLine(0u).kind, ImGuiConsoleLineKind::Input);
    EXPECT_EQ(m_console.getLine(0u).text.view(), gp::StringView("> fps.Limit 30"));
    ASSERT_EQ(m_console.getHistory().size(), 1u);
    EXPECT_EQ(m_console.getHistory()[0].view(), gp::StringView("fps.Limit 30"));
}

TEST_F(ImGuiConsoleTest, SubmitUnknownNameLogsError)
{
    EXPECT_EQ(run("does.not.exist"), gp::console::ConsoleResult::NotFound);

    ASSERT_GE(m_console.getLineCount(), 2u);
    EXPECT_EQ(m_console.getLine(m_console.getLineCount() - 1u).kind, ImGuiConsoleLineKind::Error);
}

TEST_F(ImGuiConsoleTest, EmptySubmitIsIgnored)
{
    EXPECT_EQ(run("   "), gp::console::ConsoleResult::Success);

    EXPECT_EQ(m_console.getLineCount(), 0u);
    EXPECT_TRUE(m_console.getHistory().isEmpty());
}

TEST_F(ImGuiConsoleTest, ConsecutiveDuplicateCommandsShareOneHistoryEntry)
{
    (void)run("fps.Limit 30");
    (void)run("fps.Limit 30");
    (void)run("r.VSync 1");

    EXPECT_EQ(m_console.getHistory().size(), 2u);
}

TEST_F(ImGuiConsoleTest, HistoryIsBoundedOldestFirstOut)
{
    gp::imgui::ImGuiConsoleDesc desc{};
    desc.maxHistoryEntries = 2u;
    initializeConsole(desc);

    (void)run("fps.Limit 1");
    (void)run("fps.Limit 2");
    (void)run("fps.Limit 3");

    ASSERT_EQ(m_console.getHistory().size(), 2u);
    EXPECT_EQ(m_console.getHistory()[0].view(), gp::StringView("fps.Limit 2"));
    EXPECT_EQ(m_console.getHistory()[1].view(), gp::StringView("fps.Limit 3"));
}

TEST_F(ImGuiConsoleTest, HistoryNavigationWalksBackAndReturnsToEmptyLine)
{
    (void)run("fps.Limit 10");
    (void)run("fps.Limit 20");

    EXPECT_TRUE(m_console.navigateHistory(-1));
    EXPECT_EQ(m_console.getInput(), gp::StringView("fps.Limit 20"));
    EXPECT_TRUE(m_console.navigateHistory(-1));
    EXPECT_EQ(m_console.getInput(), gp::StringView("fps.Limit 10"));
    EXPECT_FALSE(m_console.navigateHistory(-1));
    EXPECT_EQ(m_console.getInput(), gp::StringView("fps.Limit 10"));

    EXPECT_TRUE(m_console.navigateHistory(+1));
    EXPECT_EQ(m_console.getInput(), gp::StringView("fps.Limit 20"));
    EXPECT_TRUE(m_console.navigateHistory(+1));
    EXPECT_TRUE(m_console.getInput().isEmpty());
    EXPECT_FALSE(m_console.navigateHistory(+1));
}

TEST_F(ImGuiConsoleTest, ClearCommandWipesLogWithoutReachingManager)
{
    m_console.addLine(ImGuiConsoleLineKind::Output, "something");

    EXPECT_EQ(run("CLEAR"), gp::console::ConsoleResult::Success);
    EXPECT_EQ(m_console.getLineCount(), 0u);
}

TEST_F(ImGuiConsoleTest, AddLineSplitsNewlinesAndStripsCarriageReturns)
{
    m_console.addLine(ImGuiConsoleLineKind::Output, "first\r\nsecond\n");

    ASSERT_EQ(m_console.getLineCount(), 2u);
    EXPECT_EQ(m_console.getLine(0u).text.view(), gp::StringView("first"));
    EXPECT_EQ(m_console.getLine(1u).text.view(), gp::StringView("second"));
}

TEST_F(ImGuiConsoleTest, LogRingBufferKeepsNewestLinesInOrder)
{
    gp::imgui::ImGuiConsoleDesc desc{};
    desc.maxOutputLines = 3u;
    initializeConsole(desc);

    m_console.addLine(ImGuiConsoleLineKind::Output, "l1");
    m_console.addLine(ImGuiConsoleLineKind::Output, "l2");
    m_console.addLine(ImGuiConsoleLineKind::Output, "l3");
    m_console.addLine(ImGuiConsoleLineKind::Output, "l4");
    m_console.addLine(ImGuiConsoleLineKind::Output, "l5");

    ASSERT_EQ(m_console.getLineCount(), 3u);
    EXPECT_EQ(m_console.getLine(0u).text.view(), gp::StringView("l3"));
    EXPECT_EQ(m_console.getLine(1u).text.view(), gp::StringView("l4"));
    EXPECT_EQ(m_console.getLine(2u).text.view(), gp::StringView("l5"));
}

TEST_F(ImGuiConsoleTest, ToggleOpensAndCloses)
{
    EXPECT_FALSE(m_console.isOpen());
    m_console.toggle();
    EXPECT_TRUE(m_console.isOpen());
    m_console.toggle();
    EXPECT_FALSE(m_console.isOpen());
}

}   // namespace gp::imgui::tests
