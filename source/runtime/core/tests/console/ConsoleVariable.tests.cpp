// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleDesc.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleManager.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleCommand.hpp"
#include "console/IConsoleManager.hpp"   // IWYU pragma: keep
#include "console/IConsoleObject.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/strings/FixedString.hpp"
#include "containers/strings/String.hpp"   // IWYU pragma: keep
#include "containers/views/StringView.hpp"
#include "CoreMinimal.hpp"
#include "memory/HardwareConstants.hpp"
#include "templates/Delegate.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace gp::console::tests
{

namespace
{

static_assert(alignof(AutoConsoleVariable<gp::Int32>) >= gp::memory::HardwareConstants::cacheLineSize);
static_assert(alignof(AutoConsoleVariable<ConsoleString>) >= gp::memory::HardwareConstants::cacheLineSize);
static_assert(alignof(AutoConsoleVariableRef) >= gp::memory::HardwareConstants::cacheLineSize);
static_assert(alignof(internal::ConsoleDynamicVariable) >= gp::memory::HardwareConstants::cacheLineSize);

}   // namespace

class ConsoleVariableTest : public ::testing::Test
{
protected:
    BasicConsoleManager<128u, 16u, 16u, 16u> m_manager{};

    virtual void SetUp() noexcept override
    {
        ConsoleManagerDesc desc{};
        desc.enableConsole = true;
        desc.enableCheats = true;
        desc.applyCommandLineOverrides = false;
        desc.registerBuiltInCommands = false;
        desc.registerDeferredStaticObjects = false;
        ASSERT_EQ(m_manager.initialize(desc), ConsoleResult::Success);
    }

    virtual void TearDown() noexcept override
    {
        m_manager.shutdown();
    }
};

TEST_F(ConsoleVariableTest, AutoConsoleVariableAllSupportedTypesAndLifetime)
{
    {
        AutoConsoleVariable<bool> boolVar(m_manager, "t.Bool", true, "Bool var");
        AutoConsoleVariable<gp::Int32> intVar(m_manager, "t.Int", -42, "Int var");
        AutoConsoleVariable<gp::UInt32> uintVar(m_manager, "t.UInt", 99u, "UInt var");
        AutoConsoleVariable<gp::Float32> floatVar(m_manager, "t.Float", 1.5f, "Float var");
        AutoConsoleVariable<gp::Float64> doubleVar(m_manager, "t.Double", 3.25, "Double var");
        AutoConsoleVariable<gp::StringView> svVar(m_manager, "t.SV", gp::StringView("alpha"), "SV var");
        AutoConsoleVariable<const char*> cstrVar(m_manager, "t.CStr", "beta", "CStr var");
        AutoConsoleVariable<ConsoleString> fixedVar(m_manager, "t.Fixed", "gamma", "Fixed var");
        AutoConsoleVariable<gp::String> strVar(m_manager, "t.Str", "delta", "Str var");

        EXPECT_EQ(m_manager.getVariableCount(), 9u);

        EXPECT_TRUE(boolVar.isRegistered());
        EXPECT_FALSE(boolVar->isExternalReference());
        EXPECT_TRUE(boolVar->isRenderThreadSafe());
        EXPECT_EQ(boolVar.getDefaultValue(), true);
        EXPECT_EQ(boolVar.getValueOnGameThread(), true);
        EXPECT_EQ(boolVar.getValueOnRenderThread(), true);
        EXPECT_EQ(boolVar.getValueOnAnyThread(false), true);
        EXPECT_EQ(boolVar.getValueOnAnyThread(true), true);
        EXPECT_EQ(boolVar.asVariable(), m_manager.findConsoleVariable("t.Bool"));

        const AutoConsoleVariable<gp::Int32>& constIntRef = intVar;
        EXPECT_EQ(constIntRef.asVariable(), &intVar);
        EXPECT_EQ(constIntRef->getInt(), -42);
        EXPECT_EQ(intVar.getDefaultValue(), -42);

        EXPECT_EQ(uintVar.getValueOnGameThread(), 99u);
        EXPECT_EQ(uintVar.getDefaultValue(), 99u);
        EXPECT_FLOAT_EQ(floatVar.getValueOnGameThread(), 1.5f);
        EXPECT_FLOAT_EQ(floatVar.getDefaultValue(), 1.5f);
        EXPECT_DOUBLE_EQ(doubleVar.getValueOnGameThread(), 3.25);
        EXPECT_DOUBLE_EQ(doubleVar.getDefaultValue(), 3.25);
        EXPECT_EQ(svVar.getValueOnGameThread(), gp::StringView("alpha"));
        EXPECT_EQ(svVar.getDefaultValue(), gp::StringView("alpha"));
        EXPECT_STREQ(cstrVar.getValueOnGameThread(), "beta");
        EXPECT_STREQ(cstrVar.getDefaultValue(), "beta");
        EXPECT_EQ(fixedVar.getValueOnGameThread().view(), gp::StringView("gamma"));
        EXPECT_EQ(fixedVar.getDefaultValue().view(), gp::StringView("gamma"));
        EXPECT_EQ(strVar.getValueOnGameThread().view(), gp::StringView("delta"));
        EXPECT_EQ(strVar.getDefaultValue().view(), gp::StringView("delta"));
        EXPECT_EQ(strVar->getDefaultString().view(), gp::StringView("delta"));
        EXPECT_EQ(strVar.set("epsilon"), ConsoleResult::Success);
        EXPECT_EQ(strVar->getDefaultString().view(), gp::StringView("delta"));
        EXPECT_EQ(strVar.getValueOnRenderThread().view(), gp::StringView("delta"));
        EXPECT_EQ(strVar.getValueOnAnyThread(false).view(), gp::StringView("delta"));
        EXPECT_EQ(strVar.getValueOnAnyThread(true).view(), gp::StringView("epsilon"));
    }

    EXPECT_EQ(m_manager.getVariableCount(), 0u);
    EXPECT_EQ(m_manager.findConsoleVariable("t.Bool"), nullptr);
    EXPECT_EQ(m_manager.findConsoleVariable("t.Str"), nullptr);
}

TEST_F(ConsoleVariableTest, IConsoleObjectMetadataFlagsAndNameClamping)
{
    char longName[kMaxConsoleNameLength + 20u]{};
    char longHelp[kMaxConsoleHelpLength + 20u]{};
    for (gp::USize i = 0u; i < sizeof(longName) - 1u; ++i)
    {
        longName[i] = 'N';
    }
    for (gp::USize i = 0u; i < sizeof(longHelp) - 1u; ++i)
    {
        longHelp[i] = 'H';
    }

    AutoConsoleVariable<gp::Int32> var(
        m_manager,
        gp::StringView(longName, sizeof(longName) - 1u),
        7,
        gp::StringView(longHelp, sizeof(longHelp) - 1u),
        ConsoleVarFlags::Archive
    );

    IConsoleObject* const obj = var.asVariable();
    const IConsoleObject* const constObj = obj;

    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->getName().size(), kMaxConsoleNameLength);
    EXPECT_EQ(obj->getNameHash(), hashConsoleName(obj->getName()));
    EXPECT_EQ(obj->getHelp().size(), kMaxConsoleHelpLength);
    EXPECT_TRUE(obj->hasFlags(ConsoleVarFlags::Archive | ConsoleVarFlags::RenderThreadSafe));
    EXPECT_EQ(obj->asVariable(), &var);
    EXPECT_EQ(constObj->asVariable(), &var);
    EXPECT_EQ(obj->asCommand(), nullptr);
    EXPECT_EQ(constObj->asCommand(), nullptr);

    obj->addFlags(ConsoleVarFlags::Invisible);
    EXPECT_TRUE(obj->hasFlags(ConsoleVarFlags::Invisible));
    obj->clearFlags(ConsoleVarFlags::Archive);
    EXPECT_FALSE(obj->hasFlags(ConsoleVarFlags::Archive));
    obj->setFlags(ConsoleVarFlags::ReadOnly);
    EXPECT_TRUE(obj->hasFlags(ConsoleVarFlags::ReadOnly));
    EXPECT_FALSE(obj->hasFlags(ConsoleVarFlags::Invisible));
}

TEST_F(ConsoleVariableTest, TypedCoercionsAndSetValidation)
{
    AutoConsoleVariable<bool> bVar(m_manager, "t.C_Bool", false, "bool");
    AutoConsoleVariable<gp::Int32> iVar(m_manager, "t.C_Int", 10, "int");
    AutoConsoleVariable<gp::UInt32> uVar(m_manager, "t.C_UInt", 20u, "uint");
    AutoConsoleVariable<gp::Float32> fVar(m_manager, "t.C_Float", 2.5f, "float");
    AutoConsoleVariable<gp::Float64> dVar(m_manager, "t.C_Double", -4.5, "double");
    AutoConsoleVariable<ConsoleString> sVar(m_manager, "t.C_Str", "0x10", "str");

    EXPECT_EQ(sVar->getInt(), 16);
    EXPECT_EQ(sVar->getUInt(), 16u);
    EXPECT_FALSE(sVar->getBool());
    EXPECT_EQ(sVar->setFromString("true"), ConsoleResult::Success);
    EXPECT_TRUE(sVar->getBool());
    EXPECT_FLOAT_EQ(sVar->getFloat(), 0.0f);
    EXPECT_EQ(sVar->setFromString("false"), ConsoleResult::Success);
    EXPECT_FALSE(sVar->getBool());
    EXPECT_EQ(sVar->setFromString("7.25"), ConsoleResult::Success);
    EXPECT_DOUBLE_EQ(sVar->getFloat64(), 7.25);

    EXPECT_EQ(bVar.set(true), ConsoleResult::Success);
    EXPECT_TRUE(bVar->getBool());
    EXPECT_EQ(bVar->getInt(), 1);
    EXPECT_EQ(bVar->getUInt(), 1u);
    EXPECT_FLOAT_EQ(bVar->getFloat(), 1.0f);
    EXPECT_DOUBLE_EQ(bVar->getFloat64(), 1.0);
    EXPECT_EQ(bVar->getString().view(), gp::StringView("true"));

    // Negative Int32 coerces to 0u when read as UInt32.
    EXPECT_EQ(iVar.set(-5), ConsoleResult::Success);
    EXPECT_EQ(iVar->getUInt(), 0u);
    EXPECT_EQ(iVar->set(12.75f), ConsoleResult::Success);
    EXPECT_EQ(iVar.getValueOnGameThread(), 12);

    // Large UInt32 clamps cleanly to INT32_MAX when read as Int32; negative float writes reject with InvalidArguments.
    EXPECT_EQ(uVar.set(3000000000u), ConsoleResult::Success);
    EXPECT_EQ(uVar->getInt(), std::numeric_limits<gp::Int32>::max());
    EXPECT_EQ(uVar->set(-3.0), ConsoleResult::InvalidArguments);
    EXPECT_EQ(uVar.getValueOnGameThread(), 3000000000u);
    EXPECT_EQ(uVar->set(0.0), ConsoleResult::Success);
    EXPECT_EQ(uVar.getValueOnGameThread(), 0u);

    EXPECT_EQ(fVar->set(gp::UInt32(8u)), ConsoleResult::Success);
    EXPECT_FLOAT_EQ(fVar.getValueOnGameThread(), 8.0f);
    EXPECT_EQ(dVar->set(gp::Int32(-19)), ConsoleResult::Success);
    EXPECT_DOUBLE_EQ(dVar.getValueOnGameThread(), -19.0);
    EXPECT_EQ(sVar->set(gp::Int32(123)), ConsoleResult::Success);
    EXPECT_EQ(sVar->getString().view(), gp::StringView("123"));

    // Parse failures leave previous value untouched.
    EXPECT_EQ(bVar->setFromString("not_a_bool"), ConsoleResult::ParseFailed);
    EXPECT_EQ(iVar->setFromString("99abc"), ConsoleResult::ParseFailed);
    EXPECT_EQ(uVar->setFromString("-1"), ConsoleResult::ParseFailed);
    EXPECT_EQ(fVar->setFromString("nan"), ConsoleResult::ParseFailed);
    EXPECT_EQ(dVar->setFromString("inf"), ConsoleResult::ParseFailed);

    // ReadOnly & Cheat policy rejections.
    AutoConsoleVariable<gp::Int32> roVar(m_manager, "t.ReadOnly", 5, "ro", ConsoleVarFlags::ReadOnly);
    EXPECT_EQ(roVar.set(99, ConsoleVarSetBy::Console), ConsoleResult::ReadOnly);
    EXPECT_EQ(roVar.set(99, ConsoleVarSetBy::Code), ConsoleResult::ReadOnly);
    EXPECT_EQ(roVar.getValueOnGameThread(), 5);
    EXPECT_EQ(roVar->reset(ConsoleVarSetBy::Console), ConsoleResult::ReadOnly);

    AutoConsoleVariable<gp::Int32> cheatVar(m_manager, "t.Cheat", 1, "cheat", ConsoleVarFlags::Cheat);
    EXPECT_EQ(cheatVar.set(2), ConsoleResult::Success);
    m_manager.setCheatsEnabled(false);
    EXPECT_EQ(cheatVar.set(3), ConsoleResult::CheatBlocked);
    EXPECT_EQ(cheatVar.getValueOnGameThread(), 2);
}

TEST_F(ConsoleVariableTest, PriorityPrecedenceModifiedFlagAndReset)
{
    AutoConsoleVariable<gp::Int32> var(m_manager, "t.Priority", 10, "priority");
    EXPECT_EQ(var->getSetBy(), ConsoleVarSetBy::Constructor);
    EXPECT_FALSE(var->isModified());
    EXPECT_FALSE(var->hasFlags(ConsoleVarFlags::Modified));

    EXPECT_EQ(var.set(20, ConsoleVarSetBy::Scalability), ConsoleResult::Success);
    EXPECT_EQ(var->getSetBy(), ConsoleVarSetBy::Scalability);
    EXPECT_TRUE(var->isModified());

    EXPECT_EQ(var.set(30, ConsoleVarSetBy::CommandLine), ConsoleResult::Success);
    EXPECT_EQ(var->getSetBy(), ConsoleVarSetBy::CommandLine);
    EXPECT_EQ(var.getValueOnGameThread(), 30);

    // Lower priority tiers cannot overwrite CommandLine.
    EXPECT_EQ(var.set(40, ConsoleVarSetBy::GameSetting), ConsoleResult::PriorityTooLow);
    EXPECT_EQ(var.set(40, ConsoleVarSetBy::ConsoleVariables), ConsoleResult::PriorityTooLow);
    EXPECT_EQ(var.getValueOnGameThread(), 30);
    EXPECT_EQ(var->getSetBy(), ConsoleVarSetBy::CommandLine);

    // Equal or higher priority succeeds.
    EXPECT_EQ(var.set(50, ConsoleVarSetBy::Console), ConsoleResult::Success);
    EXPECT_EQ(var->getSetBy(), ConsoleVarSetBy::Console);
    EXPECT_EQ(var.getValueOnGameThread(), 50);

    // Writing default value clears Modified flag.
    EXPECT_EQ(var.set(10, ConsoleVarSetBy::Console), ConsoleResult::Success);
    EXPECT_FALSE(var->isModified());

    EXPECT_EQ(var.set(77, ConsoleVarSetBy::Console), ConsoleResult::Success);
    EXPECT_TRUE(var->isModified());
    EXPECT_EQ(var->reset(ConsoleVarSetBy::Code), ConsoleResult::PriorityTooLow);
    EXPECT_EQ(var.getValueOnGameThread(), 77);

    EXPECT_EQ(var->reset(ConsoleVarSetBy::Console), ConsoleResult::Success);
    EXPECT_EQ(var.getValueOnGameThread(), 10);
    EXPECT_EQ(var->getSetBy(), ConsoleVarSetBy::Constructor);
    EXPECT_FALSE(var->isModified());
    EXPECT_TRUE(var->hasPendingRenderSync());
}

TEST_F(ConsoleVariableTest, DualSlotRenderSyncAndAnyThreadReads)
{
    AutoConsoleVariable<gp::Float64> scalarVar(m_manager, "t.SyncScalar", 1.25, "scalar");
    AutoConsoleVariable<gp::String> strVar(m_manager, "t.SyncStr", "Initial", "string");

    EXPECT_FALSE(scalarVar->hasPendingRenderSync());
    EXPECT_FALSE(strVar->hasPendingRenderSync());

    EXPECT_EQ(scalarVar.set(9.75), ConsoleResult::Success);
    EXPECT_EQ(strVar.set("Updated"), ConsoleResult::Success);

    EXPECT_TRUE(scalarVar->hasPendingRenderSync());
    EXPECT_TRUE(strVar->hasPendingRenderSync());

    // Render slot remains isolated until sync barrier runs.
    EXPECT_DOUBLE_EQ(scalarVar.getValueOnGameThread(), 9.75);
    EXPECT_DOUBLE_EQ(scalarVar.getValueOnAnyThread(true), 9.75);
    EXPECT_DOUBLE_EQ(scalarVar.getValueOnRenderThread(), 1.25);
    EXPECT_EQ(strVar.getValueOnGameThread().view(), gp::StringView("Updated"));
    EXPECT_EQ(strVar.getValueOnAnyThread(true).view(), gp::StringView("Updated"));
    EXPECT_EQ(strVar->getStringOnRenderThread().view(), gp::StringView("Initial"));
    EXPECT_EQ(strVar->getStringOnAnyThread().view(), gp::StringView("Updated"));

    // Per-variable sync updates just that variable.
    scalarVar->syncRenderThreadValue();
    EXPECT_FALSE(scalarVar->hasPendingRenderSync());
    EXPECT_DOUBLE_EQ(scalarVar.getValueOnRenderThread(), 9.75);
    EXPECT_DOUBLE_EQ(scalarVar.getValueOnAnyThread(false), 9.75);

    // Remaining string variable syncs cleanly via manager barrier.
    EXPECT_EQ(m_manager.syncRenderThreadValues(), 1u);
    EXPECT_FALSE(strVar->hasPendingRenderSync());
    EXPECT_EQ(strVar.getValueOnRenderThread().view(), gp::StringView("Updated"));
    EXPECT_EQ(m_manager.syncRenderThreadValues(), 0u);
}

TEST_F(ConsoleVariableTest, LatchedVariablesStageFlushAndResetBehavior)
{
    AutoConsoleVariable<gp::Int32> latchedVar(m_manager, "t.Latched", 100, "latched", ConsoleVarFlags::Latched);
    gp::Int32 callbackCount = 0;
    latchedVar->setOnChangedCallback(
        ConsoleVarChangedDelegate::createLambda(
            [&callbackCount](IConsoleVariable*) noexcept -> void
    {
        ++callbackCount;
    }
        )
    );

    EXPECT_TRUE(latchedVar->isLatched());
    EXPECT_FALSE(latchedVar->hasPendingLatchedValue());
    EXPECT_FALSE(latchedVar->applyLatchedValue());

    EXPECT_EQ(latchedVar.set(200, ConsoleVarSetBy::CommandLine), ConsoleResult::Success);
    EXPECT_TRUE(latchedVar->hasPendingLatchedValue());
    EXPECT_EQ(latchedVar.getValueOnGameThread(), 100);
    EXPECT_EQ(latchedVar.getValueOnRenderThread(), 100);
    EXPECT_EQ(callbackCount, 0);

    // Lower priority write to an already-staged latched variable fails with PriorityTooLow.
    EXPECT_EQ(latchedVar.set(300, ConsoleVarSetBy::GameSetting), ConsoleResult::PriorityTooLow);
    // Higher priority overwrites pending latched slot.
    EXPECT_EQ(latchedVar.set(400, ConsoleVarSetBy::Console), ConsoleResult::Success);
    EXPECT_EQ(latchedVar.getValueOnGameThread(), 100);

    EXPECT_EQ(m_manager.flushLatchedVariables(), 1u);
    EXPECT_FALSE(latchedVar->hasPendingLatchedValue());
    EXPECT_EQ(latchedVar.getValueOnGameThread(), 400);
    EXPECT_EQ(latchedVar->getSetBy(), ConsoleVarSetBy::Console);
    EXPECT_TRUE(latchedVar->isModified());
    EXPECT_TRUE(latchedVar->hasPendingRenderSync());
    EXPECT_EQ(callbackCount, 1);

    // Second flush is a no-op; render sync publishes 400.
    EXPECT_EQ(m_manager.flushLatchedVariables(), 0u);
    EXPECT_EQ(m_manager.syncRenderThreadValues(), 1u);
    EXPECT_EQ(latchedVar.getValueOnRenderThread(), 400);

    // Stage another change and call reset() to clear both active and latched state.
    EXPECT_EQ(latchedVar.set(500, ConsoleVarSetBy::Console), ConsoleResult::Success);
    EXPECT_TRUE(latchedVar->hasPendingLatchedValue());
    EXPECT_EQ(latchedVar->reset(ConsoleVarSetBy::Console), ConsoleResult::Success);
    EXPECT_FALSE(latchedVar->hasPendingLatchedValue());
    EXPECT_EQ(latchedVar.getValueOnGameThread(), 100);
    EXPECT_FALSE(latchedVar->isModified());
}

TEST_F(ConsoleVariableTest, SingleAndMulticastChangeDelegatesAndLockFreeReentrancy)
{
    AutoConsoleVariable<gp::Int32> primaryVar(m_manager, "t.Events", 0, "events");
    AutoConsoleVariable<gp::Int32> companionVar(m_manager, "t.Companion", 1, "companion");

    gp::Int32 primaryHits = 0;
    gp::Int32 observedValue = -1;
    const ConsoleVarChangedDelegate primaryDelegate = ConsoleVarChangedDelegate::createLambda(
        [&primaryHits, &observedValue, this](IConsoleVariable* const var) noexcept -> void
    {
        ++primaryHits;
        observedValue = var->getInt();
        // Verify manager locks are NOT held during callback by querying and mutating companion.
        if (IConsoleVariable* const other = m_manager.findConsoleVariable("t.Companion"))
        {
            (void)other->set(observedValue * 10);
        }
    }
    );
    primaryVar->setOnChangedCallback(primaryDelegate);
    EXPECT_TRUE(primaryVar->getOnChangedCallback().isBound());

    gp::Int32 multicastSum = 0;
    gp::DelegateHandle handles[kMaxConsoleCallbacksPerVariable]{};
    for (gp::USize i = 0u; i < kMaxConsoleCallbacksPerVariable; ++i)
    {
        const gp::Int32 weight = static_cast<gp::Int32>(i + 1u);
        handles[i] = primaryVar->addOnChangedCallback(
            ConsoleVarChangedDelegate::createLambda(
                [&multicastSum, weight](IConsoleVariable*) noexcept -> void
        {
            multicastSum += weight;
        }
            )
        );
        EXPECT_TRUE(handles[i].isValid());
    }

    // 9th multicast callback must fail cleanly with invalid handle.
    const gp::DelegateHandle overflowHandle = primaryVar->addOnChangedCallback(
        ConsoleVarChangedDelegate::createLambda(
            [](IConsoleVariable*) noexcept -> void
    {
    }
        )
    );
    EXPECT_FALSE(overflowHandle.isValid());
    EXPECT_FALSE(primaryVar->addOnChangedCallback(ConsoleVarChangedDelegate{}).isValid());

    EXPECT_EQ(primaryVar.set(5), ConsoleResult::Success);
    EXPECT_EQ(primaryHits, 1);
    EXPECT_EQ(observedValue, 5);
    EXPECT_EQ(companionVar.getValueOnGameThread(), 50);
    EXPECT_EQ(multicastSum, 36);   // 1+2+3+4+5+6+7+8

    // Writing an unchanged value suppresses duplicate callbacks.
    EXPECT_EQ(primaryVar.set(5), ConsoleResult::Success);
    EXPECT_EQ(primaryHits, 1);
    EXPECT_EQ(multicastSum, 36);

    EXPECT_TRUE(primaryVar->removeOnChangedCallback(handles[0]));
    EXPECT_FALSE(primaryVar->removeOnChangedCallback(handles[0]));

    EXPECT_EQ(primaryVar.set(6), ConsoleResult::Success);
    EXPECT_EQ(primaryHits, 2);
    EXPECT_EQ(multicastSum, 36 + 35);

    // clearOnChangedCallbacks removes multicast listeners while keeping primary delegate intact.
    primaryVar->clearOnChangedCallbacks();
    EXPECT_EQ(primaryVar.set(7), ConsoleResult::Success);
    EXPECT_EQ(primaryHits, 3);
    EXPECT_EQ(multicastSum, 36 + 35);
}

TEST_F(ConsoleVariableTest, AutoConsoleVariableRefAllSevenTypesAndExternalMutation)
{
    bool extBool = true;
    gp::Int32 extInt = -10;
    gp::UInt32 extUInt = 40u;
    gp::Float32 extFloat = 1.25f;
    gp::Float64 extDouble = 2.5;
    ConsoleString extFixed("FixedInit");
    gp::String extGp("GpInit");

    AutoConsoleVariableRef refBool(m_manager, "r.RefBool", extBool, "ref bool");
    AutoConsoleVariableRef refInt(m_manager, "r.RefInt", extInt, "ref int");
    AutoConsoleVariableRef refUInt(m_manager, "r.RefUInt", extUInt, "ref uint");
    AutoConsoleVariableRef refFloat(m_manager, "r.RefFloat", extFloat, "ref float");
    AutoConsoleVariableRef refDouble(m_manager, "r.RefDouble", extDouble, "ref double");
    AutoConsoleVariableRef refFixed(m_manager, "r.RefFixed", extFixed, "ref fixed");
    AutoConsoleVariableRef refGp(m_manager, "r.RefGp", extGp, "ref gp");

    EXPECT_TRUE(refBool.isRegistered());
    EXPECT_TRUE(refBool->isExternalReference());
    EXPECT_EQ(refInt.asVariable(), m_manager.findConsoleVariable("r.RefInt"));

    // Console writes update bound C++ variables directly.
    EXPECT_EQ(refBool->set(false), ConsoleResult::Success);
    EXPECT_EQ(refInt->set(77), ConsoleResult::Success);
    EXPECT_EQ(refUInt->set(888u), ConsoleResult::Success);
    EXPECT_EQ(refFloat->set(4.5f), ConsoleResult::Success);
    EXPECT_EQ(refDouble->set(9.5), ConsoleResult::Success);
    EXPECT_EQ(refFixed->setFromString("FixedNew"), ConsoleResult::Success);
    EXPECT_EQ(refGp->setFromString("GpNew"), ConsoleResult::Success);

    EXPECT_FALSE(extBool);
    EXPECT_EQ(extInt, 77);
    EXPECT_EQ(extUInt, 888u);
    EXPECT_FLOAT_EQ(extFloat, 4.5f);
    EXPECT_DOUBLE_EQ(extDouble, 9.5);
    EXPECT_EQ(extFixed.view(), gp::StringView("FixedNew"));
    EXPECT_EQ(extGp.view(), gp::StringView("GpNew"));

    // Direct C++ mutations are reflected immediately in Game reads and sync to Render slot at barrier.
    extInt = 555;
    extGp.assign("DirectEdit");
    EXPECT_EQ(refInt->getInt(), 555);
    EXPECT_EQ(refInt->getIntOnRenderThread(), -10);
    EXPECT_EQ(refGp->getString().view(), gp::StringView("DirectEdit"));
    EXPECT_EQ(refGp->getStringOnRenderThread().view(), gp::StringView("GpInit"));

    (void)m_manager.syncRenderThreadValues();
    EXPECT_EQ(refInt->getIntOnRenderThread(), 555);
    EXPECT_EQ(refGp->getStringOnRenderThread().view(), gp::StringView("DirectEdit"));

    // Reset restores initial construction-time snapshot.
    EXPECT_EQ(refInt->reset(ConsoleVarSetBy::Code), ConsoleResult::Success);
    EXPECT_EQ(extInt, -10);
    EXPECT_EQ(refGp->reset(ConsoleVarSetBy::Code), ConsoleResult::Success);
    EXPECT_EQ(extGp.view(), gp::StringView("GpInit"));
}

TEST_F(ConsoleVariableTest, AutoConsoleVariableSinkAndAutoConsoleCommand)
{
    AutoConsoleVariable<gp::Int32> triggerVar(m_manager, "t.Trigger", 0, "trigger");

    gp::Int32 sinkCalls = 0;
    {
        AutoConsoleVariableSink sink(
            m_manager,
            ConsoleSinkDelegate::createLambda(
                [&sinkCalls]() noexcept -> void
        {
            ++sinkCalls;
        }
            )
        );
        EXPECT_TRUE(sink.isRegistered());
        EXPECT_TRUE(sink.getHandle().isValid());
        EXPECT_EQ(m_manager.getSinkCount(), 1u);

        EXPECT_EQ(m_manager.callAllConsoleVariableSinks(false), 0u);
        EXPECT_EQ(sinkCalls, 0);

        EXPECT_EQ(m_manager.callAllConsoleVariableSinks(true), 1u);
        EXPECT_EQ(sinkCalls, 1);

        EXPECT_EQ(triggerVar.set(1), ConsoleResult::Success);
        EXPECT_EQ(m_manager.callAllConsoleVariableSinks(false), 1u);
        EXPECT_EQ(sinkCalls, 2);

        // Second sweep after no new change is skipped.
        EXPECT_EQ(m_manager.callAllConsoleVariableSinks(false), 0u);
        EXPECT_EQ(sinkCalls, 2);
    }
    EXPECT_EQ(m_manager.getSinkCount(), 0u);

    gp::Int32 voidCount = 0;
    gp::Int32 argSum = 0;
    gp::container::BasicFixedString<char, 127u> cmdReply{};
    ConsoleOutputDelegate outDelegate = ConsoleOutputDelegate::createLambda(
        [&cmdReply](const gp::StringView line) noexcept -> void
    {
        cmdReply.assign(line);
    }
    );

    AutoConsoleCommand voidLambdaCmd(
        m_manager,
        "cmd.VoidLambda",
        "void lambda",
        [&voidCount]() noexcept -> void
    {
        ++voidCount;
    }
    );
    AutoConsoleCommand argsLambdaCmd(
        m_manager,
        "cmd.ArgsLambda",
        "args lambda",
        [&argSum](const ConsoleCommandArgs& args) noexcept -> void
    {
        const gp::Optional<gp::Int32> lhs = args.tryGetInt32(0u);
        const gp::Optional<gp::Int32> rhs = args.tryGetInt32(1u);
        if (lhs.hasValue() && rhs.hasValue())
        {
            argSum = *lhs + *rhs;
            args.writeLine("ok");
        }
    }
    );
    AutoConsoleCommand cheatCmd(
        m_manager,
        "cmd.Cheat",
        "cheat cmd",
        ConsoleCommandDelegate::createLambda(
            [&voidCount]() noexcept -> void
    {
        voidCount += 100;
    }
        ),
        ConsoleVarFlags::Cheat
    );

    EXPECT_TRUE(voidLambdaCmd.isRegistered());
    EXPECT_EQ(voidLambdaCmd.asCommandInterface(), &voidLambdaCmd);
    const AutoConsoleCommand& constVoidCmd = voidLambdaCmd;
    EXPECT_EQ(constVoidCmd.asCommandInterface(), &voidLambdaCmd);
    EXPECT_EQ(constVoidCmd.asVariable(), nullptr);
    EXPECT_EQ(constVoidCmd.asCommand(), &voidLambdaCmd);

    EXPECT_EQ(m_manager.processUserConsoleInput("cmd.VoidLambda"), ConsoleResult::Success);
    EXPECT_EQ(voidCount, 1);

    EXPECT_EQ(m_manager.processUserConsoleInput("cmd.ArgsLambda 14 28", &outDelegate), ConsoleResult::Success);
    EXPECT_EQ(argSum, 42);
    EXPECT_EQ(cmdReply.view(), gp::StringView("ok"));

    EXPECT_TRUE(cheatCmd.isEnabled());
    EXPECT_EQ(cheatCmd.execute(ConsoleCommandArgs("cmd.Cheat", nullptr)), ConsoleResult::Success);
    EXPECT_EQ(voidCount, 101);

    m_manager.setCheatsEnabled(false);
    EXPECT_FALSE(cheatCmd.isEnabled());
    EXPECT_EQ(cheatCmd.execute(ConsoleCommandArgs("cmd.Cheat", nullptr)), ConsoleResult::CheatBlocked);
    EXPECT_EQ(voidCount, 101);

    m_manager.setCheatsEnabled(true);
    m_manager.setConsoleEnabled(false);
    EXPECT_FALSE(voidLambdaCmd.isEnabled());
    EXPECT_EQ(voidLambdaCmd.execute(ConsoleCommandArgs("cmd.VoidLambda", nullptr)), ConsoleResult::ConsoleDisabled);
}

TEST_F(ConsoleVariableTest, EdgeCasesNonFiniteFloatResetLatchAndDanglingRefSafety)
{
    AutoConsoleVariable<gp::Float32> fVar(m_manager, "e.Float", 1.0f, "f32");
    AutoConsoleVariable<gp::Float64> dVar(m_manager, "e.Double", 2.0, "f64");

    // Non-finite floats must be rejected cleanly across Float32 and Float64 variables.
    EXPECT_EQ(fVar.set(std::numeric_limits<gp::Float32>::quiet_NaN()), ConsoleResult::InvalidArguments);
    EXPECT_EQ(fVar.set(std::numeric_limits<gp::Float32>::infinity()), ConsoleResult::InvalidArguments);
    EXPECT_EQ(dVar.set(std::numeric_limits<gp::Float64>::quiet_NaN()), ConsoleResult::InvalidArguments);
    EXPECT_EQ(dVar.set(-std::numeric_limits<gp::Float64>::infinity()), ConsoleResult::InvalidArguments);
    EXPECT_FLOAT_EQ(fVar.getValueOnGameThread(), 1.0f);
    EXPECT_DOUBLE_EQ(dVar.getValueOnGameThread(), 2.0);

    // Reset on an unchanged variable at default keeps hasPendingRenderSync() clean and skips notifications.
    gp::Int32 spuriousCallbacks = 0;
    fVar->setOnChangedCallback(
        ConsoleVarChangedDelegate::createLambda(
            [&spuriousCallbacks](IConsoleVariable*) noexcept -> void
    {
        ++spuriousCallbacks;
    }
        )
    );
    EXPECT_EQ(fVar->reset(ConsoleVarSetBy::Console), ConsoleResult::Success);
    EXPECT_FALSE(fVar->hasPendingRenderSync());
    EXPECT_EQ(spuriousCallbacks, 0);

    // Unbinding the primary callback preserves multicast listeners.
    gp::Int32 multiHits = 0;
    const gp::DelegateHandle multiHandle = fVar->addOnChangedCallback(
        ConsoleVarChangedDelegate::createLambda(
            [&multiHits](IConsoleVariable*) noexcept -> void
    {
        ++multiHits;
    }
        )
    );
    EXPECT_TRUE(multiHandle.isValid());
    fVar->setOnChangedCallback(ConsoleVarChangedDelegate{});
    EXPECT_FALSE(fVar->getOnChangedCallback().isBound());
    EXPECT_EQ(fVar.set(3.5f), ConsoleResult::Success);
    EXPECT_EQ(spuriousCallbacks, 0);
    EXPECT_EQ(multiHits, 1);

    // Higher-priority staged Latched override blocks lower-priority reset().
    AutoConsoleVariable<gp::Int32> latchedVar(m_manager, "e.Latched", 10, "latched", ConsoleVarFlags::Latched);
    EXPECT_EQ(latchedVar.set(25, ConsoleVarSetBy::Console), ConsoleResult::Success);
    EXPECT_TRUE(latchedVar->hasPendingLatchedValue());
    EXPECT_EQ(latchedVar->reset(ConsoleVarSetBy::Code), ConsoleResult::PriorityTooLow);
    EXPECT_TRUE(latchedVar->hasPendingLatchedValue());
    EXPECT_EQ(latchedVar->reset(ConsoleVarSetBy::Console), ConsoleResult::Success);
    EXPECT_FALSE(latchedVar->hasPendingLatchedValue());

    // Staging same-as-active value into Latched clears pending latch and promotes SetBy on flush.
    EXPECT_EQ(latchedVar.set(10, ConsoleVarSetBy::CommandLine), ConsoleResult::Success);
    EXPECT_EQ(m_manager.flushLatchedVariables(), 1u);
    EXPECT_FALSE(latchedVar->hasPendingLatchedValue());
    EXPECT_EQ(latchedVar->getSetBy(), ConsoleVarSetBy::CommandLine);

    // Duplicate registration on manager leaves second Auto* instance unregistered without breaking first on destroy.
    AutoConsoleVariable<gp::Int32> winner(m_manager, "e.Duplicate", 11, "winner");
    {
        AutoConsoleVariable<gp::Int32> loser(m_manager, "E.DUPLICATE", 22, "loser");
        EXPECT_FALSE(loser.isRegistered());
    }
    ASSERT_EQ(m_manager.findConsoleVariable("e.Duplicate"), &winner);
    EXPECT_EQ(winner.getValueOnGameThread(), 11);

    // Scoped AutoConsoleVariableRef unlinks cleanly from m_refVariables so subsequent sync does not dangle.
    gp::Int32 scopedBacking = 90;
    {
        AutoConsoleVariableRef scopedRef(m_manager, "e.ScopedRef", scopedBacking, "scoped ref");
        EXPECT_TRUE(scopedRef.isRegistered());
        EXPECT_GE(m_manager.syncRenderThreadValues(), 1u);
    }
    EXPECT_EQ(m_manager.findConsoleVariable("e.ScopedRef"), nullptr);
    (void)m_manager.syncRenderThreadValues();

    // String AutoConsoleVariable accepts const char*, StringView, ConsoleString, gp::String, and nullptr cleanly.
    AutoConsoleVariable<ConsoleString> strSetVar(m_manager, "e.StrTypes", "Init", "string setters");
    EXPECT_EQ(strSetVar.set("FromLiteral"), ConsoleResult::Success);
    EXPECT_EQ(strSetVar.getValueOnGameThread().view(), gp::StringView("FromLiteral"));
    EXPECT_EQ(strSetVar.set(gp::StringView("FromView")), ConsoleResult::Success);
    EXPECT_EQ(strSetVar.getValueOnGameThread().view(), gp::StringView("FromView"));
    EXPECT_EQ(strSetVar.set(ConsoleString("FromFixed")), ConsoleResult::Success);
    EXPECT_EQ(strSetVar.getValueOnGameThread().view(), gp::StringView("FromFixed"));
    EXPECT_EQ(strSetVar.set(gp::String("FromGpString")), ConsoleResult::Success);
    EXPECT_EQ(strSetVar.getValueOnGameThread().view(), gp::StringView("FromGpString"));
    EXPECT_EQ(strSetVar.set(static_cast<const char*>(nullptr)), ConsoleResult::Success);
    EXPECT_TRUE(strSetVar.getValueOnGameThread().isEmpty());

    // String values exceeding kMaxConsoleStringLength (255 bytes) clamp cleanly in ctor, setFromString, and registry.
    char oversizedStr[kMaxConsoleStringLength + 32u]{};
    for (gp::USize i = 0u; i < sizeof(oversizedStr) - 1u; ++i)
    {
        oversizedStr[i] = 'Z';
    }
    const gp::StringView longView(oversizedStr, sizeof(oversizedStr) - 1u);
    AutoConsoleVariable<ConsoleString> clampedStrVar(m_manager, "e.ClampedStr", longView, "clamped string");
    EXPECT_EQ(clampedStrVar.getValueOnGameThread().size(), kMaxConsoleStringLength);
    EXPECT_EQ(clampedStrVar.getDefaultValue().size(), kMaxConsoleStringLength);
    EXPECT_EQ(clampedStrVar.set(longView), ConsoleResult::Success);
    EXPECT_EQ(clampedStrVar.getValueOnGameThread().size(), kMaxConsoleStringLength);

    IConsoleVariable* const dynClamped = m_manager.registerConsoleVariable("e.DynClamped", longView, "dyn clamped");
    ASSERT_NE(dynClamped, nullptr);
    EXPECT_EQ(dynClamped->getString().size(), kMaxConsoleStringLength);

    // AutoConsoleVariableRef supports Latched staging, change callbacks, and duplicate rejection cleanly.
    gp::Int32 latchedRefVal = 5;
    AutoConsoleVariableRef latchedRef(
        m_manager, "e.LatchedRef", latchedRefVal, "latched ref", ConsoleVarFlags::Latched
    );
    gp::Int32 refCallbackHits = 0;
    latchedRef->setOnChangedCallback(
        ConsoleVarChangedDelegate::createLambda(
            [&refCallbackHits](IConsoleVariable*) noexcept -> void
    {
        ++refCallbackHits;
    }
        )
    );
    EXPECT_EQ(latchedRef->set(88, ConsoleVarSetBy::Code), ConsoleResult::Success);
    EXPECT_EQ(latchedRefVal, 5);
    EXPECT_TRUE(latchedRef->hasPendingLatchedValue());
    EXPECT_EQ(refCallbackHits, 0);
    EXPECT_EQ(m_manager.flushLatchedVariables(), 1u);
    EXPECT_EQ(latchedRefVal, 88);
    EXPECT_EQ(refCallbackHits, 1);

    AutoConsoleVariableRef dupRef(m_manager, "E.LATCHEDREF", latchedRefVal, "dup ref");
    EXPECT_FALSE(dupRef.isRegistered());

    // Const IConsoleCommand inspection and runtime flag manipulation.
    gp::Int32 dynCmdHits = 0;
    IConsoleCommand* const dynCmd = m_manager.registerConsoleCommand(
        "e.DynFlagsCmd",
        "command flag manipulation test",
        [&dynCmdHits]() noexcept -> void
    {
        ++dynCmdHits;
    },
        ConsoleVarFlags::None
    );
    ASSERT_NE(dynCmd, nullptr);
    const IConsoleCommand& constDynCmd = *dynCmd;
    EXPECT_EQ(constDynCmd.getName(), gp::StringView("e.DynFlagsCmd"));
    EXPECT_EQ(constDynCmd.getNameHash(), hashConsoleName("E.DYNFLAGSCMD"));
    EXPECT_EQ(constDynCmd.getHelp(), gp::StringView("command flag manipulation test"));
    EXPECT_EQ(constDynCmd.asVariable(), nullptr);
    EXPECT_EQ(constDynCmd.asCommand(), dynCmd);
    EXPECT_TRUE(constDynCmd.isEnabled());

    dynCmd->addFlags(ConsoleVarFlags::Cheat);
    EXPECT_TRUE(dynCmd->hasFlags(ConsoleVarFlags::Cheat));
    m_manager.setCheatsEnabled(false);
    EXPECT_FALSE(dynCmd->isEnabled());
    EXPECT_EQ(dynCmd->execute(ConsoleCommandArgs("e.DynFlagsCmd", nullptr)), ConsoleResult::CheatBlocked);
    dynCmd->clearFlags(ConsoleVarFlags::Cheat);
    dynCmd->setFlags(ConsoleVarFlags::Invisible);
    EXPECT_EQ(dynCmd->getFlags(), ConsoleVarFlags::Invisible);
    EXPECT_EQ(dynCmd->execute(ConsoleCommandArgs("e.DynFlagsCmd", nullptr)), ConsoleResult::Success);
    EXPECT_EQ(dynCmdHits, 1);
    m_manager.setCheatsEnabled(true);

    // AutoConsoleVariableRef respects ReadOnly, Cheat, and gp::String external backing sync to Render Thread.
    bool roRefBacking = false;
    AutoConsoleVariableRef roRef(m_manager, "e.RoRef", roRefBacking, "read-only ref", ConsoleVarFlags::ReadOnly);
    EXPECT_EQ(roRef->set(true, ConsoleVarSetBy::Console), ConsoleResult::ReadOnly);
    EXPECT_FALSE(roRefBacking);

    gp::Float32 cheatRefBacking = 1.5f;
    AutoConsoleVariableRef cheatRef(m_manager, "e.CheatRef", cheatRefBacking, "cheat ref", ConsoleVarFlags::Cheat);
    m_manager.setCheatsEnabled(false);
    EXPECT_EQ(cheatRef->set(4.5f, ConsoleVarSetBy::Console), ConsoleResult::CheatBlocked);
    EXPECT_FLOAT_EQ(cheatRefBacking, 1.5f);
    m_manager.setCheatsEnabled(true);

    gp::String gpStrRefBacking("InitialGp");
    AutoConsoleVariableRef gpStrRef(m_manager, "e.GpStrRef", gpStrRefBacking, "gp::String ref");
    EXPECT_EQ(gpStrRef.getString().view(), gp::StringView("InitialGp"));
    gpStrRefBacking = "ExternalMut";
    EXPECT_GE(m_manager.syncRenderThreadValues(), 1u);
    const ConsoleString rtStrOut = gpStrRef->getStringOnRenderThread();
    EXPECT_EQ(rtStrOut.view(), gp::StringView("ExternalMut"));
}

}   // namespace gp::console::tests
