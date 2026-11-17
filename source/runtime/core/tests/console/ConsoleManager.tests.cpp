// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "console/ConsoleManager.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/Console.hpp"
#include "console/ConsoleDesc.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleCommand.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleObject.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "threading/atomics/Atomic.hpp"
#include <gtest/gtest.h>
#include <thread>

namespace gp::console::tests
{

namespace
{

gp::Int32 g_staticGlobalRefBacking = 321;
gp::Int32 g_staticGlobalCmdCount = 0;
gp::Int32 g_staticGlobalSinkCount = 0;

AutoConsoleVariable<gp::Int32> g_staticGlobalVar("g.StaticDeferredVar", 77, "Static deferred global CVar");
AutoConsoleVariableRef g_staticGlobalRef("g.StaticDeferredRef", g_staticGlobalRefBacking, "Static deferred ref");
AutoConsoleVariableSink g_staticGlobalSink(
    ConsoleSinkDelegate::createLambda(
        []() noexcept -> void
{
    ++g_staticGlobalSinkCount;
}
    )
);
AutoConsoleCommand g_staticGlobalCmd(
    "g.StaticDeferredCmd",
    "Static deferred global cmd",
    []() noexcept -> void
{
    ++g_staticGlobalCmdCount;
}
);

ConsoleManagerDesc makeIsolatedDesc(const bool withBuiltIns = false, const bool withCli = false) noexcept
{
    ConsoleManagerDesc desc{};
    desc.enableConsole = true;
    desc.enableCheats = true;
    desc.applyCommandLineOverrides = withCli;
    desc.registerBuiltInCommands = withBuiltIns;
    desc.registerDeferredStaticObjects = false;
    return desc;
}

bool startsWithPrefix(const gp::StringView text, const gp::StringView prefix) noexcept
{
    return text.size() >= prefix.size() && text.substr(0u, prefix.size()) == prefix;
}

bool endsWithSuffix(const gp::StringView text, const gp::StringView suffix) noexcept
{
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size(), suffix.size()) == suffix;
}

}   // namespace

class ConsoleManagerTest : public ::testing::Test
{
protected:
    ConsoleManager m_manager{};

    virtual void SetUp() noexcept override
    {
        ASSERT_EQ(m_manager.initialize(makeIsolatedDesc(true, false)), ConsoleResult::Success);
    }

    virtual void TearDown() noexcept override
    {
        m_manager.shutdown();
        gp::CommandLine::shutdown();
    }
};

TEST_F(ConsoleManagerTest, LifecycleDoubleInitAndPolicyToggles)
{
    ConsoleManager localMgr{};
    EXPECT_FALSE(localMgr.isInitialized());
    EXPECT_EQ(localMgr.findConsoleVariable("anything"), nullptr);
    EXPECT_EQ(localMgr.processUserConsoleInput("help"), ConsoleResult::NotFound);

    EXPECT_EQ(localMgr.initialize(makeIsolatedDesc(false, false)), ConsoleResult::Success);
    EXPECT_TRUE(localMgr.isInitialized());
    EXPECT_EQ(localMgr.initialize(makeIsolatedDesc(false, false)), ConsoleResult::AlreadyExists);

    EXPECT_TRUE(localMgr.isConsoleEnabled());
    localMgr.setConsoleEnabled(false);
    EXPECT_FALSE(localMgr.isConsoleEnabled());

    EXPECT_TRUE(localMgr.areCheatsEnabled());
    localMgr.setCheatsEnabled(false);
    EXPECT_FALSE(localMgr.areCheatsEnabled());

    localMgr.shutdown();
    EXPECT_FALSE(localMgr.isInitialized());

    // Re-initialization after shutdown works cleanly.
    EXPECT_EQ(localMgr.initialize(makeIsolatedDesc(false, false)), ConsoleResult::Success);
    EXPECT_TRUE(localMgr.isInitialized());
    localMgr.shutdown();
}

TEST_F(ConsoleManagerTest, ImperativeRegistrationAllTypesPoolCapacityAndReclamation)
{
    BasicConsoleManager<32u, 3u, 2u, 2u> tinyMgr{};
    ASSERT_EQ(tinyMgr.initialize(makeIsolatedDesc(false, false)), ConsoleResult::Success);

    IConsoleVariable* const vBool = tinyMgr.registerConsoleVariable("d.Bool", true, "b");
    IConsoleVariable* const vInt = tinyMgr.registerConsoleVariable("d.Int", gp::Int32(-12), "i");
    IConsoleVariable* const vStr = tinyMgr.registerConsoleVariable("d.Str", gp::StringView("hello"), "s");
    ASSERT_NE(vBool, nullptr);
    ASSERT_NE(vInt, nullptr);
    ASSERT_NE(vStr, nullptr);
    EXPECT_EQ(tinyMgr.getVariableCount(), 3u);

    // Duplicate case-insensitive name and empty name fail; pool is now full.
    EXPECT_EQ(tinyMgr.registerConsoleVariable("D.BOOL", false, "dup"), nullptr);
    EXPECT_EQ(tinyMgr.registerConsoleVariable("", false, "empty"), nullptr);
    EXPECT_EQ(tinyMgr.registerConsoleVariable("d.Extra", 1, "overflow"), nullptr);

    // Reclaim one dynamic slot and verify reuse for UInt32 / Float32 / Float64 / StringView.
    EXPECT_TRUE(tinyMgr.unregisterConsoleObject("D.INT"));
    EXPECT_FALSE(tinyMgr.unregisterConsoleObject("D.INT"));
    EXPECT_EQ(tinyMgr.getVariableCount(), 2u);

    IConsoleVariable* const vUInt = tinyMgr.registerConsoleVariable("d.UInt", 77u, "u");
    ASSERT_NE(vUInt, nullptr);
    EXPECT_EQ(vUInt->getUInt(), 77u);

    bool extB = true;
    gp::Int32 extI = 5;
    gp::UInt32 extU = 6u;
    gp::Float32 extF = 1.5f;
    gp::Float64 extD = 2.5;
    ConsoleString extCs("cs");
    gp::String extGs("gs");

    ASSERT_NE(m_manager.registerConsoleVariable("m.F32", 4.25f, "f32"), nullptr);
    ASSERT_NE(m_manager.registerConsoleVariable("m.F64", 8.75, "f64"), nullptr);
    ASSERT_NE(m_manager.registerConsoleVariableRef("m.RefB", extB, "rb"), nullptr);
    ASSERT_NE(m_manager.registerConsoleVariableRef("m.RefI", extI, "ri"), nullptr);
    ASSERT_NE(m_manager.registerConsoleVariableRef("m.RefU", extU, "ru"), nullptr);
    ASSERT_NE(m_manager.registerConsoleVariableRef("m.RefF", extF, "rf"), nullptr);
    ASSERT_NE(m_manager.registerConsoleVariableRef("m.RefD", extD, "rd"), nullptr);
    ASSERT_NE(m_manager.registerConsoleVariableRef("m.RefCS", extCs, "rcs"), nullptr);
    ASSERT_NE(m_manager.registerConsoleVariableRef("m.RefGS", extGs, "rgs"), nullptr);

    // Imperative commands: pool exhaustion, pointer unregistration, and reuse.
    gp::Int32 cmdHits = 0;
    IConsoleCommand* const c1 = tinyMgr.registerConsoleCommand(
        "c.One",
        "one",
        ConsoleCommandDelegate::createLambda(
            [&cmdHits]() noexcept -> void
    {
        ++cmdHits;
    }
        )
    );
    IConsoleCommand* const c2 = tinyMgr.registerConsoleCommand(
        "c.Two",
        "two",
        ConsoleCommandWithArgsDelegate::createLambda(
            [&cmdHits](const ConsoleCommandArgs& args) noexcept -> void
    {
        cmdHits += static_cast<gp::Int32>(args.getArgCount());
    }
        )
    );
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);
    EXPECT_EQ(tinyMgr.getCommandCount(), 2u);
    EXPECT_EQ(tinyMgr.registerConsoleCommand("c.Three", "full", ConsoleCommandDelegate{}), nullptr);
    EXPECT_EQ(
        tinyMgr.registerConsoleCommand(
            "c.Three",
            "full",
            ConsoleCommandDelegate::createLambda(
                []() noexcept -> void
    {
    }
            )
        ),
        nullptr
    );

    EXPECT_TRUE(tinyMgr.unregisterConsoleObject(c1));
    EXPECT_FALSE(tinyMgr.unregisterConsoleObject(c1));
    EXPECT_EQ(tinyMgr.getCommandCount(), 1u);

    IConsoleCommand* const c3 = tinyMgr.registerConsoleCommand(
        "c.Three",
        "three",
        ConsoleCommandDelegate::createLambda(
            [&cmdHits]() noexcept -> void
    {
        cmdHits += 10;
    }
        )
    );
    ASSERT_NE(c3, nullptr);
    EXPECT_EQ(tinyMgr.processUserConsoleInput("c.Two a b c"), ConsoleResult::Success);
    EXPECT_EQ(tinyMgr.processUserConsoleInput("c.Three"), ConsoleResult::Success);
    EXPECT_EQ(cmdHits, 13);

    // Sink pool capacity, unregistration, and generation stale-handle rejection.
    const gp::DelegateHandle s1 = tinyMgr.registerConsoleVariableSink(
        ConsoleSinkDelegate::createLambda(
            []() noexcept -> void
    {
    }
        )
    );
    const gp::DelegateHandle s2 = tinyMgr.registerConsoleVariableSink(
        ConsoleSinkDelegate::createLambda(
            []() noexcept -> void
    {
    }
        )
    );
    EXPECT_TRUE(s1.isValid());
    EXPECT_TRUE(s2.isValid());
    EXPECT_EQ(tinyMgr.getSinkCount(), 2u);
    EXPECT_FALSE(tinyMgr
                     .registerConsoleVariableSink(
                         ConsoleSinkDelegate::createLambda(
                             []() noexcept -> void
    {
    }
                         )
                     )
                     .isValid());
    EXPECT_FALSE(tinyMgr.registerConsoleVariableSink(ConsoleSinkDelegate{}).isValid());

    EXPECT_TRUE(tinyMgr.unregisterConsoleVariableSink(s1));
    EXPECT_FALSE(tinyMgr.unregisterConsoleVariableSink(s1));
    const gp::DelegateHandle s3 = tinyMgr.registerConsoleVariableSink(
        ConsoleSinkDelegate::createLambda(
            []() noexcept -> void
    {
    }
        )
    );
    EXPECT_TRUE(s3.isValid());
    EXPECT_NE(s1, s3);
    EXPECT_FALSE(tinyMgr.unregisterConsoleVariableSink(s1));

    // External object validation errors.
    EXPECT_EQ(tinyMgr.registerExternalObject(nullptr), ConsoleResult::InvalidArguments);
    tinyMgr.shutdown();
}

TEST_F(ConsoleManagerTest, OpenAddressedBackwardShiftDeletionIntegrityAndHashCapacity)
{
    BasicConsoleManager<16u, 15u, 2u, 2u> compactMgr{};
    ASSERT_EQ(compactMgr.initialize(makeIsolatedDesc(false, false)), ConsoleResult::Success);

    gp::container::BasicFixedString<char, 16u> names[15]{};
    IConsoleVariable* vars[15]{};
    for (gp::USize i = 0u; i < 15u; ++i)
    {
        names[i].assign("k.");
        names[i].append(formatConsoleInt32(static_cast<gp::Int32>(i)).view());
        vars[i] = compactMgr.registerConsoleVariable(names[i].view(), static_cast<gp::Int32>(i), "h");
        ASSERT_NE(vars[i], nullptr);
    }

    // Remove every even entry to force backward-shift deletions across dense probe chains.
    for (gp::USize i = 0u; i < 15u; i += 2u)
    {
        EXPECT_TRUE(compactMgr.unregisterConsoleObject(names[i].view()));
    }

    // Every remaining odd variable must still be reachable and every deleted one absent.
    for (gp::USize i = 0u; i < 15u; ++i)
    {
        if ((i & 1u) != 0u)
        {
            ASSERT_NE(compactMgr.findConsoleVariable(names[i].view()), nullptr);
            EXPECT_EQ(compactMgr.findConsoleVariable(names[i].view())->getInt(), static_cast<gp::Int32>(i));
        }
        else
        {
            EXPECT_EQ(compactMgr.findConsoleVariable(names[i].view()), nullptr);
        }
    }

    // Re-insert into freed slots and confirm lookup continues cleanly.
    for (gp::USize i = 0u; i < 15u; i += 2u)
    {
        ASSERT_NE(compactMgr.registerConsoleVariable(names[i].view(), static_cast<gp::Int32>(i + 100u), "h2"), nullptr);
    }
    for (gp::USize i = 0u; i < 15u; i += 2u)
    {
        ASSERT_NE(compactMgr.findConsoleVariable(names[i].view()), nullptr);
        EXPECT_EQ(compactMgr.findConsoleVariable(names[i].view())->getInt(), static_cast<gp::Int32>(i + 100u));
    }

    // Hash table keeps 1 empty sentinel out of 16 slots, so 16th external object returns CapacityExceeded.
    AutoConsoleVariable<gp::Int32> capTest(compactMgr, "k.CapOverflow", 1, "overflow");
    EXPECT_FALSE(capTest.isRegistered());
    compactMgr.shutdown();
}

TEST_F(ConsoleManagerTest, LookupIterationAutocompleteAndConsoleInput)
{
    AutoConsoleVariable<gp::Int32> zVar(m_manager, "r.Zebra", 1, "zebra");
    AutoConsoleVariable<gp::Int32> aVar(m_manager, "r.Alpha", 2, "alpha");
    AutoConsoleVariable<gp::Int32> hiddenVar(m_manager, "r.Secret", 3, "hidden", ConsoleVarFlags::Invisible);
    AutoConsoleVariable<gp::String> modeVar(m_manager, "r.Mode", "Low", "render mode");
    AutoConsoleCommand alphaCmd(
        m_manager,
        "r.Apply",
        "apply cmd",
        []() noexcept -> void
    {
    }
    );

    // Cross-kind lookups reject wrong types cleanly.
    EXPECT_EQ(m_manager.findConsoleObject("R.ALPHA"), aVar.asVariable());
    EXPECT_EQ(m_manager.findConsoleVariable("R.ALPHA"), aVar.asVariable());
    EXPECT_EQ(m_manager.findConsoleCommand("R.ALPHA"), nullptr);
    EXPECT_EQ(m_manager.findConsoleObject("r.apply"), alphaCmd.asCommandInterface());
    EXPECT_EQ(m_manager.findConsoleCommand("r.apply"), alphaCmd.asCommandInterface());
    EXPECT_EQ(m_manager.findConsoleVariable("r.apply"), nullptr);

    // Autocomplete sorting, maxResults cap, and invisible filtering.
    gp::StringView acResults[8]{};
    const gp::USize visibleCount = m_manager.autoComplete("r.", acResults, 8u, false);
    ASSERT_EQ(visibleCount, 4u);
    EXPECT_EQ(acResults[0], gp::StringView("r.Alpha"));
    EXPECT_EQ(acResults[1], gp::StringView("r.Apply"));
    EXPECT_EQ(acResults[2], gp::StringView("r.Mode"));
    EXPECT_EQ(acResults[3], gp::StringView("r.Zebra"));

    EXPECT_EQ(m_manager.autoComplete("r.", acResults, 2u, false), 2u);
    EXPECT_EQ(acResults[0], gp::StringView("r.Alpha"));
    EXPECT_EQ(acResults[1], gp::StringView("r.Apply"));

    EXPECT_EQ(m_manager.autoComplete("r.", acResults, 8u, true), 5u);

    // Early termination in forEachConsoleObjectStartingWith.
    gp::USize visited = 0u;
    m_manager.forEachConsoleObjectStartingWith(
        ConsoleObjectVisitor::createLambda(
            [&visited](IConsoleObject*) noexcept -> bool
    {
        ++visited;
        return visited < 2u;
    }
        ),
        "r.",
        false
    );
    EXPECT_EQ(visited, 2u);

    // processUserConsoleInput query & assignment forms + error diagnostics.
    gp::container::BasicFixedString<char, 1023u> logBuf{};
    ConsoleOutputDelegate logOut = ConsoleOutputDelegate::createLambda(
        [&logBuf](const gp::StringView line) noexcept -> void
    {
        if (!logBuf.isEmpty())
        {
            logBuf.pushBack('\n');
        }
        logBuf.append(line);
    }
    );

    EXPECT_EQ(m_manager.processUserConsoleInput("  ", &logOut), ConsoleResult::InvalidArguments);
    EXPECT_EQ(m_manager.processUserConsoleInput("no.such.var", &logOut), ConsoleResult::NotFound);

    logBuf.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Alpha ?", &logOut), ConsoleResult::Success);
    EXPECT_NE(logBuf.view().find("r.Alpha = 2 [Int32"), gp::StringView::npos);

    EXPECT_EQ(m_manager.processUserConsoleInput("r.Alpha 10", &logOut), ConsoleResult::Success);
    EXPECT_EQ(aVar.getValueOnGameThread(), 10);
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Alpha = 20", &logOut), ConsoleResult::Success);
    EXPECT_EQ(aVar.getValueOnGameThread(), 20);
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Alpha= 25", &logOut), ConsoleResult::Success);
    EXPECT_EQ(aVar.getValueOnGameThread(), 25);
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Alpha =28", &logOut), ConsoleResult::Success);
    EXPECT_EQ(aVar.getValueOnGameThread(), 28);
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Alpha=30", &logOut), ConsoleResult::Success);
    EXPECT_EQ(aVar.getValueOnGameThread(), 30);
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Mode=\"Ultra High\"", &logOut), ConsoleResult::Success);
    EXPECT_EQ(modeVar.getValueOnGameThread().view(), gp::StringView("Ultra High"));
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Mode =\"Medium Rare\"", &logOut), ConsoleResult::Success);
    EXPECT_EQ(modeVar.getValueOnGameThread().view(), gp::StringView("Medium Rare"));
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Mode = \"High\"", &logOut), ConsoleResult::Success);
    EXPECT_EQ(modeVar.getValueOnGameThread().view(), gp::StringView("High"));
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Mode =\"\"", &logOut), ConsoleResult::Success);
    EXPECT_TRUE(modeVar.getValueOnGameThread().isEmpty());
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Mode =", &logOut), ConsoleResult::InvalidArguments);
    EXPECT_EQ(m_manager.processUserConsoleInput("no.such.var = 5", &logOut), ConsoleResult::NotFound);
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Alpha badvalue", &logOut), ConsoleResult::ParseFailed);

    m_manager.setConsoleEnabled(false);
    EXPECT_EQ(m_manager.processUserConsoleInput("r.Alpha 40", &logOut), ConsoleResult::ConsoleDisabled);
    EXPECT_EQ(aVar.getValueOnGameThread(), 30);
}

TEST_F(ConsoleManagerTest, CommandLineOverridesAndLateRegistrationAutoApply)
{
    const char* argv[] = { "gp-engine.exe", "-cli.Int=77", "--cli.Float:2.5",
                           "+cli.Flag",     "+cli.Space",  "Ultra Quality",
                           "+cli.Cmd",      "alpha",       "-ExecCmds=cli.Exec=99; cli.Cmd beta" };
    gp::CommandLine::initialize(9, argv);

    AutoConsoleVariable<gp::Int32> cliInt(m_manager, "cli.Int", 1, "cli int");
    AutoConsoleVariable<gp::Float32> cliFloat(m_manager, "cli.Float", 0.5f, "cli float");
    AutoConsoleVariable<bool> cliFlag(m_manager, "cli.Flag", false, "cli bool");
    AutoConsoleVariable<ConsoleString> cliSpace(m_manager, "cli.Space", "Default", "cli space");
    AutoConsoleVariable<gp::Int32> cliExec(m_manager, "cli.Exec", 0, "cli exec");

    gp::container::BasicFixedString<char, 128u> cmdSeen{};
    AutoConsoleCommand cliCmd(
        m_manager,
        "cli.Cmd",
        "cli cmd",
        [&cmdSeen](const ConsoleCommandArgs& args) noexcept -> void
    {
        if (!cmdSeen.isEmpty())
        {
            cmdSeen.pushBack(',');
        }
        cmdSeen.append(args.getArg(0u));
    }
    );

    const gp::USize applied = m_manager.applyCommandLineOverrides(ConsoleVarSetBy::CommandLine);
    EXPECT_GE(applied, 6u);
    EXPECT_EQ(cliInt.getValueOnGameThread(), 77);
    EXPECT_EQ(cliInt->getSetBy(), ConsoleVarSetBy::CommandLine);
    EXPECT_FLOAT_EQ(cliFloat.getValueOnGameThread(), 2.5f);
    EXPECT_TRUE(cliFlag.getValueOnGameThread());
    EXPECT_EQ(cliSpace.getValueOnGameThread().view(), gp::StringView("Ultra Quality"));
    EXPECT_EQ(cliExec.getValueOnGameThread(), 99);
    EXPECT_EQ(cmdSeen.view(), gp::StringView("alpha,beta"));

    // Late-registered variable registered AFTER applyCommandLineOverrides() picks up CLI overrides automatically.
    gp::CommandLine::shutdown();
    const char* lateArgv[] = { "gp-engine.exe", "-cli.Late=512" };
    gp::CommandLine::initialize(2, lateArgv);
    (void)m_manager.applyCommandLineOverrides(ConsoleVarSetBy::CommandLine);

    AutoConsoleVariable<gp::Int32> lateVar(m_manager, "cli.Late", 16, "late cli");
    EXPECT_EQ(lateVar.getValueOnGameThread(), 512);
    EXPECT_EQ(lateVar->getSetBy(), ConsoleVarSetBy::CommandLine);
}

TEST_F(ConsoleManagerTest, DirtyQueueOverflowFallbacksAndResetAll)
{
    // Queue capacities = 2, triggering dirtyRenderOverflow & dirtyLatchedOverflow when 4 variables mutate together.
    internal::ConsoleHashSlot hashSlots[64]{};
    internal::ConsoleDynamicVariable dynVars[8]{};
    internal::ConsoleDynamicCommand dynCmds[8]{};
    internal::ConsoleSinkSlot sinkSlots[8]{};
    IConsoleVariable* dirtyRender[2]{};
    IConsoleVariable* dirtyLatched[2]{};
    IConsoleVariable* refVars[8]{};

    internal::ConsoleManagerStorageView storageView{};
    storageView.hashSlots = hashSlots;
    storageView.hashSlotCapacity = 64u;
    storageView.dynamicVariables = dynVars;
    storageView.dynamicVariableCapacity = 8u;
    storageView.dynamicCommands = dynCmds;
    storageView.dynamicCommandCapacity = 8u;
    storageView.sinkSlots = sinkSlots;
    storageView.sinkCapacity = 8u;
    storageView.dirtyRenderQueue = dirtyRender;
    storageView.dirtyRenderCapacity = 2u;
    storageView.dirtyLatchedQueue = dirtyLatched;
    storageView.dirtyLatchedCapacity = 2u;
    storageView.refVariables = refVars;
    storageView.refVariableCapacity = 8u;

    ConsoleManagerBase overflowMgr(storageView);
    ASSERT_EQ(overflowMgr.initialize(makeIsolatedDesc(false, false)), ConsoleResult::Success);

    AutoConsoleVariable<gp::Int32> v1(overflowMgr, "o.V1", 1, "1", ConsoleVarFlags::Latched);
    AutoConsoleVariable<gp::Int32> v2(overflowMgr, "o.V2", 2, "2", ConsoleVarFlags::Latched);
    AutoConsoleVariable<gp::Int32> v3(overflowMgr, "o.V3", 3, "3", ConsoleVarFlags::Latched);
    AutoConsoleVariable<gp::Int32> v4(overflowMgr, "o.V4", 4, "4", ConsoleVarFlags::Latched);
    AutoConsoleVariable<gp::Int32> vRo(overflowMgr, "o.RO", 9, "ro", ConsoleVarFlags::ReadOnly);

    EXPECT_EQ(v1.set(11, ConsoleVarSetBy::GameSetting), ConsoleResult::Success);
    EXPECT_EQ(v2.set(22, ConsoleVarSetBy::CommandLine), ConsoleResult::Success);
    EXPECT_EQ(v3.set(33, ConsoleVarSetBy::Code), ConsoleResult::Success);
    EXPECT_EQ(v4.set(44, ConsoleVarSetBy::Console), ConsoleResult::Success);

    // Flush fallback must apply ALL 4 latched variables even though queue capacity was 2.
    EXPECT_EQ(overflowMgr.flushLatchedVariables(), 4u);
    EXPECT_EQ(v1.getValueOnGameThread(), 11);
    EXPECT_EQ(v2.getValueOnGameThread(), 22);
    EXPECT_EQ(v3.getValueOnGameThread(), 33);
    EXPECT_EQ(v4.getValueOnGameThread(), 44);

    // Render sync fallback must sync ALL 4 dirty variables even though queue capacity was 2.
    EXPECT_EQ(overflowMgr.syncRenderThreadValues(), 4u);
    EXPECT_EQ(v1.getValueOnRenderThread(), 11);
    EXPECT_EQ(v2.getValueOnRenderThread(), 22);
    EXPECT_EQ(v3.getValueOnRenderThread(), 33);
    EXPECT_EQ(v4.getValueOnRenderThread(), 44);

    // resetAllVariables(CommandLine) resets v1 (GameSetting) & v2 (CommandLine), leaves v3 (Code) & v4 (Console) & RO.
    EXPECT_EQ(overflowMgr.resetAllVariables(ConsoleVarSetBy::CommandLine), 2u);
    EXPECT_EQ(v1.getValueOnGameThread(), 1);
    EXPECT_EQ(v2.getValueOnGameThread(), 2);
    EXPECT_EQ(v3.getValueOnGameThread(), 33);
    EXPECT_EQ(v4.getValueOnGameThread(), 44);
    EXPECT_EQ(vRo.getValueOnGameThread(), 9);

    overflowMgr.shutdown();
}

TEST_F(ConsoleManagerTest, BuiltInDiagnosticCommandsWorkViaConsoleInput)
{
    AutoConsoleVariable<gp::Int32> visVar(m_manager, "diag.Width", 1920, "Viewport width");
    AutoConsoleVariable<gp::Int32> roVar(m_manager, "diag.Locked", 42, "Locked value", ConsoleVarFlags::ReadOnly);
    (void)visVar.set(3840, ConsoleVarSetBy::Console);

    gp::container::BasicFixedString<char, 2047u> outText{};
    ConsoleOutputDelegate out = ConsoleOutputDelegate::createLambda(
        [&outText](const gp::StringView line) noexcept -> void
    {
        if (!outText.isEmpty())
        {
            outText.pushBack('\n');
        }
        const gp::USize rem = (outText.capacity() > outText.size()) ? (outText.capacity() - outText.size()) : 0u;
        outText.append(line.substr(0u, rem));
    }
    );

    EXPECT_EQ(m_manager.processUserConsoleInput("help diag.", &out), ConsoleResult::Success);
    EXPECT_NE(outText.view().find("diag.Width = 3840 [Int32"), gp::StringView::npos);

    outText.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("cvar.list diag.", &out), ConsoleResult::Success);
    EXPECT_NE(outText.view().find("diag.Locked"), gp::StringView::npos);

    outText.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("cvar.dump diag.", &out), ConsoleResult::Success);
    EXPECT_NE(outText.view().find("diag.Width = 3840"), gp::StringView::npos);
    EXPECT_EQ(outText.view().find("diag.Locked"), gp::StringView::npos);

    outText.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("cvar.reset", &out), ConsoleResult::Success);
    EXPECT_NE(outText.view().find("Usage:"), gp::StringView::npos);

    outText.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("cvar.reset diag.Missing", &out), ConsoleResult::Success);
    EXPECT_NE(outText.view().find("not found"), gp::StringView::npos);

    outText.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("cvar.reset diag.Locked", &out), ConsoleResult::Success);
    EXPECT_NE(outText.view().find("ReadOnly"), gp::StringView::npos);

    outText.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("cvar.reset diag.Width", &out), ConsoleResult::Success);
    EXPECT_EQ(visVar.getValueOnGameThread(), 1920);
    EXPECT_NE(outText.view().find("Success"), gp::StringView::npos);
}

TEST_F(ConsoleManagerTest, GlobalSingletonDeferredStaticsAndScopedGlobalObjects)
{
    IConsoleManager& globalMgr = IConsoleManager::get();
    EXPECT_TRUE(IConsoleManager::isGlobalInitialized());
    EXPECT_TRUE(globalMgr.isInitialized());

    // Verify file-scope deferred static objects & sink migrated automatically without SIOF.
    IConsoleVariable* const staticVar = globalMgr.findConsoleVariable("g.StaticDeferredVar");
    IConsoleVariable* const staticRef = globalMgr.findConsoleVariable("g.StaticDeferredRef");
    IConsoleCommand* const staticCmd = globalMgr.findConsoleCommand("g.StaticDeferredCmd");
    ASSERT_NE(staticVar, nullptr);
    ASSERT_NE(staticRef, nullptr);
    ASSERT_NE(staticCmd, nullptr);
    EXPECT_EQ(staticVar->getInt(), 77);
    EXPECT_EQ(staticRef->getInt(), 321);

    const gp::Int32 cmdBefore = g_staticGlobalCmdCount;
    EXPECT_EQ(globalMgr.processUserConsoleInput("g.StaticDeferredCmd"), ConsoleResult::Success);
    EXPECT_EQ(g_staticGlobalCmdCount, cmdBefore + 1);

    const gp::Int32 sinkBefore = g_staticGlobalSinkCount;
    EXPECT_EQ(staticVar->set(88), ConsoleResult::Success);
    (void)globalMgr.callAllConsoleVariableSinks(false);
    EXPECT_GE(g_staticGlobalSinkCount, sinkBefore + 1);

    // Scoped global Auto* objects created AFTER initialization register and unregister automatically.
    {
        bool scopedFlag = false;
        gp::Int32 scopedSinkHits = 0;
        AutoConsoleVariable<gp::Int32> scopedGlobalVar("g.ScopedVar", 12, "scoped var");
        AutoConsoleVariableRef scopedGlobalRef("g.ScopedRef", scopedFlag, "scoped ref");
        AutoConsoleVariableSink scopedGlobalSink(
            ConsoleSinkDelegate::createLambda(
                [&scopedSinkHits]() noexcept -> void
        {
            ++scopedSinkHits;
        }
            )
        );
        AutoConsoleCommand scopedGlobalCmd(
            "g.ScopedCmd",
            "scoped cmd",
            ConsoleCommandDelegate::createLambda(
                [&scopedFlag]() noexcept -> void
        {
            scopedFlag = true;
        }
            )
        );

        EXPECT_TRUE(scopedGlobalVar.isRegistered());
        EXPECT_TRUE(scopedGlobalRef.isRegistered());
        EXPECT_TRUE(scopedGlobalSink.isRegistered());
        EXPECT_TRUE(scopedGlobalCmd.isRegistered());
        EXPECT_EQ(globalMgr.findConsoleVariable("g.ScopedVar"), &scopedGlobalVar);
        EXPECT_EQ(globalMgr.processUserConsoleInput("g.ScopedCmd"), ConsoleResult::Success);
        EXPECT_TRUE(scopedFlag);
        EXPECT_GE(globalMgr.callAllConsoleVariableSinks(true), 1u);
        EXPECT_EQ(scopedSinkHits, 1);
    }

    EXPECT_EQ(globalMgr.findConsoleVariable("g.ScopedVar"), nullptr);
    EXPECT_EQ(globalMgr.findConsoleVariable("g.ScopedRef"), nullptr);
    EXPECT_EQ(globalMgr.findConsoleCommand("g.ScopedCmd"), nullptr);

    // Restore static global default so test re-runs stay clean.
    (void)staticVar->reset(ConsoleVarSetBy::Console);
}

TEST_F(ConsoleManagerTest, ConcurrentGameThreadWritesAndLockFreeRenderAnyThreadReads)
{
    AutoConsoleVariable<gp::Int32> frameVar(m_manager, "mt.Frame", 0, "monotonic counter");
    AutoConsoleVariable<gp::Float64> timeVar(m_manager, "mt.Time", 0.0, "scaled time");
    AutoConsoleVariable<ConsoleString> labelVar(m_manager, "mt.Label", "Frame_0_End", "seqlocked label");

    gp::thread::Atomic<bool> startFlag{ false };
    gp::thread::Atomic<bool> stopFlag{ false };
    gp::thread::Atomic<bool> observedTearOrRegression{ false };

    std::thread renderReader(
        [&frameVar, &timeVar, &labelVar, &startFlag, &stopFlag, &observedTearOrRegression]() noexcept -> void
    {
        while (!startFlag.load(gp::MemoryOrder::Acquire))
        {
            std::this_thread::yield();
        }

        gp::Int32 lastFrame = -1;
        gp::Float64 lastTime = -1.0;
        while (!stopFlag.load(gp::MemoryOrder::Acquire))
        {
            const gp::Int32 curFrame = frameVar.getValueOnRenderThread();
            const gp::Float64 curTime = timeVar.getValueOnRenderThread();
            const ConsoleString curLabel = labelVar.getValueOnRenderThread();

            if (curFrame < lastFrame || curTime < lastTime)
            {
                observedTearOrRegression.store(true, gp::MemoryOrder::Release);
            }
            lastFrame = curFrame;
            lastTime = curTime;

            const gp::StringView view = curLabel.view();
            if (!startsWithPrefix(view, "Frame_") || !endsWithSuffix(view, "_End"))
            {
                observedTearOrRegression.store(true, gp::MemoryOrder::Release);
            }
        }
    }
    );

    std::thread anyReader(
        [&frameVar, &labelVar, &startFlag, &stopFlag, &observedTearOrRegression]() noexcept -> void
    {
        while (!startFlag.load(gp::MemoryOrder::Acquire))
        {
            std::this_thread::yield();
        }

        gp::Int32 lastFrame = -1;
        while (!stopFlag.load(gp::MemoryOrder::Acquire))
        {
            const gp::Int32 curFrame = frameVar->getIntOnAnyThread();
            const ConsoleString curLabel = labelVar->getStringOnAnyThread();
            if (curFrame < lastFrame)
            {
                observedTearOrRegression.store(true, gp::MemoryOrder::Release);
            }
            lastFrame = curFrame;

            const gp::StringView view = curLabel.view();
            if (!startsWithPrefix(view, "Frame_") || !endsWithSuffix(view, "_End"))
            {
                observedTearOrRegression.store(true, gp::MemoryOrder::Release);
            }
        }
    }
    );

    startFlag.store(true, gp::MemoryOrder::Release);

    constexpr gp::Int32 kIterations = 4000;
    for (gp::Int32 i = 1; i <= kIterations; ++i)
    {
        (void)frameVar.set(i);
        (void)timeVar.set(static_cast<gp::Float64>(i) * 0.5);

        gp::container::BasicFixedString<char, kMaxConsoleStringLength> payload{};
        payload.append("Frame_");
        payload.append(formatConsoleInt32(i).view());
        payload.append("_RenderPayloadPayloadPayload_End");
        (void)labelVar.set(payload.view());

        if ((i & 3) == 0)
        {
            (void)m_manager.syncRenderThreadValues();
        }
    }
    (void)m_manager.syncRenderThreadValues();

    stopFlag.store(true, gp::MemoryOrder::Release);
    renderReader.join();
    anyReader.join();

    EXPECT_FALSE(observedTearOrRegression.load(gp::MemoryOrder::Acquire));
    EXPECT_EQ(frameVar.getValueOnRenderThread(), kIterations);
    EXPECT_DOUBLE_EQ(timeVar.getValueOnRenderThread(), static_cast<gp::Float64>(kIterations) * 0.5);
    EXPECT_EQ(labelVar.getValueOnRenderThread().view(), gp::StringView("Frame_4000_RenderPayloadPayloadPayload_End"));
}

TEST_F(ConsoleManagerTest, UninitializedRejectionsInvalidStorageAndReentrantSinkUnregister)
{
    internal::ConsoleManagerStorageView badStorage{};
    ConsoleManagerBase badMgr(badStorage);
    EXPECT_EQ(badMgr.initialize(makeIsolatedDesc(false, false)), ConsoleResult::InvalidArguments);

    internal::ConsoleHashSlot oddSlots[20]{};
    badStorage.hashSlots = oddSlots;
    badStorage.hashSlotCapacity = 20u;   // Not a power of two.
    EXPECT_EQ(badMgr.initialize(makeIsolatedDesc(false, false)), ConsoleResult::InvalidArguments);

    ConsoleManager uninitMgr{};
    gp::Int32 dummyInt = 0;
    ConsoleString dummyCs{};
    gp::String dummyGs{};
    bool dummyB = false;
    gp::UInt32 dummyU = 0u;
    gp::Float32 dummyF = 0.0f;
    gp::Float64 dummyD = 0.0;
    gp::StringView acNames[4]{};
    EXPECT_EQ(uninitMgr.registerConsoleVariable("u.V", 1, "h"), nullptr);
    EXPECT_EQ(uninitMgr.registerConsoleVariableRef("u.Rb", dummyB, "h"), nullptr);
    EXPECT_EQ(uninitMgr.registerConsoleVariableRef("u.Ri", dummyInt, "h"), nullptr);
    EXPECT_EQ(uninitMgr.registerConsoleVariableRef("u.Ru", dummyU, "h"), nullptr);
    EXPECT_EQ(uninitMgr.registerConsoleVariableRef("u.Rf", dummyF, "h"), nullptr);
    EXPECT_EQ(uninitMgr.registerConsoleVariableRef("u.Rd", dummyD, "h"), nullptr);
    EXPECT_EQ(uninitMgr.registerConsoleVariableRef("u.Rcs", dummyCs, "h"), nullptr);
    EXPECT_EQ(uninitMgr.registerConsoleVariableRef("u.Rgs", dummyGs, "h"), nullptr);
    EXPECT_EQ(
        uninitMgr.registerConsoleCommand(
            "u.C",
            "h",
            ConsoleCommandDelegate::createLambda(
                []() noexcept -> void
    {
    }
            )
        ),
        nullptr
    );
    EXPECT_EQ(uninitMgr.registerExternalObject(nullptr), ConsoleResult::InvalidArguments);
    EXPECT_FALSE(uninitMgr.unregisterConsoleObject("u.V"));
    EXPECT_FALSE(uninitMgr.unregisterConsoleObject(static_cast<IConsoleObject*>(nullptr)));
    EXPECT_FALSE(uninitMgr
                     .registerConsoleVariableSink(
                         ConsoleSinkDelegate::createLambda(
                             []() noexcept -> void
    {
    }
                         )
                     )
                     .isValid());
    EXPECT_FALSE(uninitMgr.unregisterConsoleVariableSink(gp::DelegateHandle(1ULL)));
    EXPECT_EQ(uninitMgr.findConsoleObject("u.V"), nullptr);
    EXPECT_EQ(uninitMgr.findConsoleCommand("u.C"), nullptr);
    EXPECT_EQ(uninitMgr.autoComplete("u.", acNames, 4u), 0u);
    EXPECT_EQ(uninitMgr.applyCommandLineOverrides(), 0u);
    EXPECT_EQ(uninitMgr.syncRenderThreadValues(), 0u);
    EXPECT_EQ(uninitMgr.flushLatchedVariables(), 0u);
    EXPECT_EQ(uninitMgr.callAllConsoleVariableSinks(true), 0u);
    EXPECT_EQ(uninitMgr.resetAllVariables(), 0u);

    // Re-entrant sink unregistration: Sink 1 removes Sink 2 during sweep; Sink 2 must not execute.
    gp::Int32 sink1Runs = 0;
    gp::Int32 sink2Runs = 0;
    gp::DelegateHandle sink2Handle{};
    const gp::DelegateHandle sink1Handle = m_manager.registerConsoleVariableSink(
        ConsoleSinkDelegate::createLambda(
            [this, &sink1Runs, &sink2Handle]() noexcept -> void
    {
        ++sink1Runs;
        EXPECT_TRUE(m_manager.unregisterConsoleVariableSink(sink2Handle));
    }
        )
    );
    sink2Handle = m_manager.registerConsoleVariableSink(
        ConsoleSinkDelegate::createLambda(
            [&sink2Runs]() noexcept -> void
    {
        ++sink2Runs;
    }
        )
    );
    EXPECT_TRUE(sink1Handle.isValid());
    EXPECT_TRUE(sink2Handle.isValid());
    EXPECT_EQ(m_manager.callAllConsoleVariableSinks(true), 1u);
    EXPECT_EQ(sink1Runs, 1);
    EXPECT_EQ(sink2Runs, 0);
}

TEST_F(ConsoleManagerTest, ChunkedSweepsScalePastTwoHundredFiftySixVariablesWithoutTruncation)
{
    // Heap/static-free large test manager to verify > 256 chunk boundary in forEach, flush, renderSync, and resetAll.
    static BasicConsoleManager<1024u, 320u, 8u, 8u> largeMgr{};
    ASSERT_EQ(largeMgr.initialize(makeIsolatedDesc(false, false)), ConsoleResult::Success);

    constexpr gp::USize kCount = 300u;
    for (gp::USize i = 0u; i < kCount; ++i)
    {
        gp::container::BasicFixedString<char, 24u> name{};
        name.assign("bulk.");
        name.append(formatConsoleUInt32(static_cast<gp::UInt32>(i)).view());
        IConsoleVariable* const var =
            largeMgr.registerConsoleVariable(name.view(), static_cast<gp::Int32>(i), "bulk", ConsoleVarFlags::Latched);
        ASSERT_NE(var, nullptr);
        ASSERT_EQ(var->set(static_cast<gp::Int32>(i + 1000u), ConsoleVarSetBy::Code), ConsoleResult::Success);
    }

    gp::USize visitedCount = 0u;
    largeMgr.forEachConsoleObjectStartingWith(
        ConsoleObjectVisitor::createLambda(
            [&visitedCount](IConsoleObject*) noexcept -> bool
    {
        ++visitedCount;
        return true;
    }
        ),
        "bulk."
    );
    EXPECT_EQ(visitedCount, kCount);
    EXPECT_EQ(largeMgr.flushLatchedVariables(), kCount);
    EXPECT_EQ(largeMgr.syncRenderThreadValues(), kCount);
    EXPECT_EQ(largeMgr.resetAllVariables(ConsoleVarSetBy::Code), kCount);
    EXPECT_EQ(largeMgr.syncRenderThreadValues(), kCount);

    largeMgr.shutdown();
}

TEST_F(ConsoleManagerTest, UnregisterForeignAndExplicitAutoObjectsAndCliEdgeCases)
{
    ConsoleManager secondMgr{};
    ASSERT_EQ(secondMgr.initialize(makeIsolatedDesc(false, false)), ConsoleResult::Success);

    AutoConsoleVariable<gp::Int32> firstMgrVar(m_manager, "f.Owned", 42, "owned by m_manager");
    EXPECT_EQ(m_manager.getVariableCount(), 1u);

    // Passing a foreign variable to another manager must fail cleanly without touching either registry.
    EXPECT_FALSE(secondMgr.unregisterConsoleObject(firstMgrVar.asVariable()));
    EXPECT_FALSE(secondMgr.unregisterConsoleObject("f.Owned"));
    EXPECT_EQ(m_manager.findConsoleVariable("f.Owned"), firstMgrVar.asVariable());
    EXPECT_TRUE(firstMgrVar.isRegistered());

    // Explicitly unregistering an active AutoConsoleVariable clears its owner before destructor runs.
    EXPECT_TRUE(m_manager.unregisterConsoleObject(firstMgrVar.asVariable()));
    EXPECT_FALSE(firstMgrVar.isRegistered());
    EXPECT_EQ(m_manager.getVariableCount(), 0u);

    // Null/zero-capacity autoComplete and unbound visitor / invisible iteration checks.
    AutoConsoleVariable<gp::Int32> vis(secondMgr, "Z.Vis", 1, "vis");
    AutoConsoleVariable<gp::Int32> hid(secondMgr, "Z.Hid", 2, "hid", ConsoleVarFlags::Invisible);
    gp::StringView acBuf[4]{};
    EXPECT_EQ(secondMgr.autoComplete("z.", nullptr, 4u), 0u);
    EXPECT_EQ(secondMgr.autoComplete("z.", acBuf, 0u), 0u);
    EXPECT_EQ(secondMgr.autoComplete("", acBuf, 4u, false), 1u);
    EXPECT_EQ(acBuf[0], gp::StringView("Z.Vis"));
    EXPECT_EQ(secondMgr.autoComplete("z.", acBuf, 4u, true), 2u);

    secondMgr.forEachConsoleObjectStartingWith(ConsoleObjectVisitor{}, "", true);
    gp::USize hidCount = 0u;
    secondMgr.forEachConsoleObjectStartingWith(
        ConsoleObjectVisitor::createLambda(
            [&hidCount](IConsoleObject*) noexcept -> bool
    {
        ++hidCount;
        return true;
    }
        ),
        "z.",
        true
    );
    EXPECT_EQ(hidCount, 2u);

    // Extended CommandLine forms: two-token -ExecCmds, empty -ExecCmds, argless +cmd, missing vars, lower SetBy.
    gp::Int32 noArgCmdHits = 0;
    AutoConsoleVariable<gp::Int32> preCode(secondMgr, "c.HighPri", 999, "high");
    ASSERT_EQ(preCode.set(777, ConsoleVarSetBy::Console), ConsoleResult::Success);
    AutoConsoleVariable<gp::Int32> twoTokVar(secondMgr, "c.TwoTok", 0, "two token execcmds");
    AutoConsoleCommand noArgCmd(
        secondMgr,
        "c.Ping",
        "ping",
        [&noArgCmdHits]() noexcept -> void
    {
        ++noArgCmdHits;
    }
    );

    const char* extArgv[] = { "gp-engine.exe", "-ExecCmds",    "c.TwoTok=333;", "-ExecCmds=",
                              "+c.Ping",       "-c.HighPri=1", "+c.Missing",    "-c.Missing=5" };
    gp::CommandLine::initialize(8, extArgv);
    const gp::USize extApplied = secondMgr.applyCommandLineOverrides(ConsoleVarSetBy::CommandLine);
    EXPECT_EQ(extApplied, 2u);
    EXPECT_EQ(twoTokVar.getValueOnGameThread(), 333);
    EXPECT_EQ(noArgCmdHits, 1);
    EXPECT_EQ(preCode.getValueOnGameThread(), 777);

    secondMgr.shutdown();
}

TEST_F(ConsoleManagerTest, UserInputDiagnosticsErrorsCommandWithEqualsAndAdjacentCliPlus)
{
    ConsoleString lastLine{};
    ConsoleOutputDelegate outDelegate = ConsoleOutputDelegate::createLambda(
        [&lastLine](const gp::StringView msg) noexcept -> void
    {
        lastLine.assign(msg);
    }
    );

    AutoConsoleVariable<gp::Int32> roVar(m_manager, "u.Ro", 10, "read only", ConsoleVarFlags::ReadOnly);
    AutoConsoleVariable<gp::Int32> cheatVar(m_manager, "u.Cheat", 20, "cheat", ConsoleVarFlags::Cheat);
    AutoConsoleVariable<gp::Int32> priVar(m_manager, "u.Pri", 30, "priority");
    ASSERT_EQ(priVar.set(40, ConsoleVarSetBy::Console), ConsoleResult::Success);

    gp::String capturedArg{};
    gp::Int32 eqCmdCount = 0;
    AutoConsoleCommand eqCmd(
        m_manager,
        "u.EqCmd",
        "command with equals argument",
        [&capturedArg, &eqCmdCount](const ConsoleCommandArgs& args) noexcept -> void
    {
        ++eqCmdCount;
        if (args.getArgCount() > 0u)
        {
            capturedArg = args.getArg(0u);
        }
    }
    );

    // Every assignment / lookup error emits a descriptive diagnostic line when output is bound.
    lastLine.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("u.Ro 99", &outDelegate), ConsoleResult::ReadOnly);
    EXPECT_EQ(lastLine.view(), gp::StringView("ReadOnly"));

    m_manager.setCheatsEnabled(false);
    lastLine.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("u.Cheat=55", &outDelegate), ConsoleResult::CheatBlocked);
    EXPECT_EQ(lastLine.view(), gp::StringView("CheatBlocked"));
    m_manager.setCheatsEnabled(true);

    // Interactive Console (tier 9) blocks subsequent Code (tier 8) override with PriorityTooLow.
    EXPECT_EQ(priVar.set(77, ConsoleVarSetBy::Code), ConsoleResult::PriorityTooLow);

    lastLine.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("u.DoesNotExist 1", &outDelegate), ConsoleResult::NotFound);
    EXPECT_EQ(lastLine.view(), gp::StringView("Unknown console command or variable."));

    lastLine.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("u.Pri=notanint", &outDelegate), ConsoleResult::ParseFailed);
    EXPECT_EQ(lastLine.view(), gp::StringView("ParseFailed"));

    lastLine.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("u.Pri=", &outDelegate), ConsoleResult::InvalidArguments);
    EXPECT_EQ(lastLine.view(), gp::StringView("InvalidArguments"));

    m_manager.setConsoleEnabled(false);
    lastLine.clear();
    EXPECT_EQ(m_manager.processUserConsoleInput("u.Pri ?", &outDelegate), ConsoleResult::ConsoleDisabled);
    EXPECT_EQ(lastLine.view(), gp::StringView("Console is disabled."));
    m_manager.setConsoleEnabled(true);

    // Command followed by space and '=token' must invoke the command rather than matching variable '=' syntax.
    EXPECT_EQ(m_manager.processUserConsoleInput("u.EqCmd =alpha", &outDelegate), ConsoleResult::Success);
    EXPECT_EQ(eqCmdCount, 1);
    EXPECT_EQ(capturedArg.view(), gp::StringView("=alpha"));

    // Adjacent CLI +flags must not treat the second '+flag' as the value of the first flag.
    AutoConsoleVariable<bool> flagA(m_manager, "u.FlagA", false, "flag A");
    AutoConsoleVariable<bool> flagB(m_manager, "u.FlagB", false, "flag B");
    const char* adjArgv[] = { "gp-engine.exe", "+u.FlagA", "+u.FlagB" };
    gp::CommandLine::initialize(3, adjArgv);
    EXPECT_EQ(m_manager.applyCommandLineOverrides(ConsoleVarSetBy::CommandLine), 2u);
    EXPECT_TRUE(flagA.getValueOnGameThread());
    EXPECT_TRUE(flagB.getValueOnGameThread());
}

}   // namespace gp::console::tests
