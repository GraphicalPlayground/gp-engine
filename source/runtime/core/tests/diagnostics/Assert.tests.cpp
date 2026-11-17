// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "platforms/base/system/Thread.hpp"
#include "templates/ScopeExit.hpp"
#include "threading/Atomic.hpp"
#include <gtest/gtest.h>

namespace gp::diagnostics::tests
{

namespace
{

struct CapturedAssertRecord
{
    UInt32 callCount = 0u;
    AssertKind lastKind = AssertKind::Check;
    Int32 lastLine = 0;
    char expressionBuffer[256]{};
    char messageBuffer[256]{};
    char fileBuffer[256]{};
    char functionBuffer[256]{};
};

CapturedAssertRecord g_captured{};
gp::Atomic<gp::UInt32> g_concurrentEnsureHits{ 0u };
bool g_triggerNestedEnsureInHandler = false;

void copyCString(char* const dest, const USize capacity, const char* const src) noexcept
{
    if (capacity == 0u)
    {
        return;
    }
    if (src == nullptr)
    {
        dest[0] = '\0';
        return;
    }
    USize i = 0u;
    while (i + 1u < capacity && src[i] != '\0')
    {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
}

AssertAction captureAssertHandler(const AssertContext& context)
{
    ++g_captured.callCount;
    g_captured.lastKind = context.kind;
    g_captured.lastLine = context.line;
    copyCString(g_captured.expressionBuffer, sizeof(g_captured.expressionBuffer), context.expression);
    copyCString(g_captured.messageBuffer, sizeof(g_captured.messageBuffer), context.message);
    copyCString(g_captured.fileBuffer, sizeof(g_captured.fileBuffer), context.file);
    copyCString(g_captured.functionBuffer, sizeof(g_captured.functionBuffer), context.function);

    if (g_triggerNestedEnsureInHandler)
    {
        (void)GP_ENSURE(false);
    }

    return AssertAction::Continue;
}

AssertAction concurrentEnsureHandler(const AssertContext&)
{
    g_concurrentEnsureHits.fetchAdd(1u, MemoryOrder::Relaxed);
    return AssertAction::Continue;
}

}   // namespace

TEST(AssertTest, CheckCheckfAndAssertReportMetadataAndPreservePercentageLiterals)
{
    g_captured = {};
    g_triggerNestedEnsureInHandler = false;
    const AssertHandlerFn previous = setAssertHandler(&captureAssertHandler);
    // clang-format off
    GP_ON_SCOPE_EXIT
    {
        setAssertHandler(previous);
    };
    // clang-format on

    const Int32 validValue = 5;
    GP_CHECK(validValue == 5);
    GP_CHECKF(validValue > 0, "Expected positive value, got %d", validValue);
    GP_ASSERT(validValue != 0);
    GP_ASSERT(validValue == 5, "Should stay unused");
    EXPECT_EQ(g_captured.callCount, 0u);

#if GP_DO_CHECK
    GP_CHECK(validValue == 99);
    EXPECT_EQ(g_captured.callCount, 1u);
    EXPECT_EQ(g_captured.lastKind, AssertKind::Check);
    EXPECT_EQ(StringView(g_captured.expressionBuffer), StringView("validValue == 99"));
    EXPECT_EQ(StringView(g_captured.messageBuffer), StringView(""));
    EXPECT_GT(g_captured.lastLine, 0);

    GP_CHECKF(validValue == 0, "Mismatch expected=%d actual=%d", 0, validValue);
    EXPECT_EQ(g_captured.callCount, 2u);
    EXPECT_EQ(StringView(g_captured.messageBuffer), StringView("Mismatch expected=0 actual=5"));

    // Plain message with literal '%' must not invoke printf formatting.
    GP_ASSERT(false, "100% literal safety");
    EXPECT_EQ(g_captured.callCount, 3u);
    EXPECT_EQ(StringView(g_captured.messageBuffer), StringView("100% literal safety"));

    // Variadic format arguments forward through printf formatting cleanly.
    GP_ASSERT(false, "Slot=%u", 7u);
    EXPECT_EQ(g_captured.callCount, 4u);
    EXPECT_EQ(StringView(g_captured.messageBuffer), StringView("Slot=7"));
#endif
}

TEST(AssertTest, VerifyAlwaysEvaluatesConditionEvenWhenCheckIsDisabled)
{
    g_captured = {};
    const AssertHandlerFn previous = setAssertHandler(&captureAssertHandler);
    // clang-format off
    GP_ON_SCOPE_EXIT
    {
        setAssertHandler(previous);
    };
    // clang-format on

    Int32 sideEffectCount = 0;
    auto incrementAndReturn = [&](const bool outcome) noexcept -> bool
    {
        ++sideEffectCount;
        return outcome;
    };

    GP_VERIFY(incrementAndReturn(true));
    EXPECT_EQ(sideEffectCount, 1);
    EXPECT_EQ(g_captured.callCount, 0u);

    GP_VERIFYF(incrementAndReturn(false), "Verify step %d failed", sideEffectCount);
    EXPECT_EQ(sideEffectCount, 2);
#if GP_DO_CHECK
    EXPECT_EQ(g_captured.callCount, 1u);
    EXPECT_EQ(g_captured.lastKind, AssertKind::Verify);
    EXPECT_EQ(StringView(g_captured.messageBuffer), StringView("Verify step 2 failed"));
#endif
}

TEST(AssertTest, CheckSlowNoEntryAndUnimplementedRespectConfigAndKinds)
{
    g_captured = {};
    const AssertHandlerFn previous = setAssertHandler(&captureAssertHandler);
    // clang-format off
    GP_ON_SCOPE_EXIT
    {
        setAssertHandler(previous);
    };
    // clang-format on

    Int32 slowEvaluations = 0;
    GP_CHECK_SLOW((++slowEvaluations, false));
#if GP_DO_GUARD_SLOW
    EXPECT_EQ(slowEvaluations, 1);
    EXPECT_EQ(g_captured.lastKind, AssertKind::CheckSlow);
#else
    EXPECT_EQ(slowEvaluations, 0);
    EXPECT_EQ(g_captured.callCount, 0u);
#endif

#if GP_DO_CHECK
    const UInt32 countBeforeEntryTests = g_captured.callCount;
    GP_CHECK_NO_ENTRY();
    EXPECT_EQ(g_captured.callCount, countBeforeEntryTests + 1u);
    EXPECT_EQ(g_captured.lastKind, AssertKind::NoEntry);

    GP_UNIMPLEMENTED();
    EXPECT_EQ(g_captured.callCount, countBeforeEntryTests + 2u);
    EXPECT_EQ(g_captured.lastKind, AssertKind::Unimplemented);
#endif
}

TEST(AssertTest, EnsureReturnsBoolFiresOncePerCallSiteAndGuardsReentrancy)
{
    g_captured = {};
    const AssertHandlerFn previous = setAssertHandler(&captureAssertHandler);
    GP_ON_SCOPE_EXIT
    // clang-format off
    {
        g_triggerNestedEnsureInHandler = false;
        setAssertHandler(previous);
    };
    // clang-format on

    bool tookTrueBranch = false;
    if (GP_ENSURE(1 + 1 == 2))
    {
        tookTrueBranch = true;
    }
    EXPECT_TRUE(tookTrueBranch);
    EXPECT_EQ(g_captured.callCount, 0u);

    Int32 failBranchRuns = 0;
    for (Int32 iteration = 0; iteration < 5; ++iteration)
    {
        if (!GP_ENSURE_MSG(false, "Loop iteration %d failed", iteration))
        {
            ++failBranchRuns;
        }
    }
    EXPECT_EQ(failBranchRuns, 5);

#if GP_DO_ENSURE
    EXPECT_EQ(g_captured.callCount, 1u);
    EXPECT_EQ(g_captured.lastKind, AssertKind::Ensure);
    EXPECT_EQ(StringView(g_captured.messageBuffer), StringView("Loop iteration 0 failed"));

    // Verify nested GP_ENSURE inside the active handler does not recurse indefinitely.
    g_triggerNestedEnsureInHandler = true;
    EXPECT_FALSE(GP_ENSURE(false));
    EXPECT_EQ(g_captured.callCount, 2u);
#endif
}

#if GP_DO_ENSURE
TEST(AssertTest, ConcurrentEnsureCallSiteFiresOnlyOnceAcrossThreads)
{
    g_concurrentEnsureHits.store(0u, MemoryOrder::Relaxed);
    const AssertHandlerFn previous = setAssertHandler(&concurrentEnsureHandler);
    // clang-format off
    GP_ON_SCOPE_EXIT
    {
        setAssertHandler(previous);
    };
    // clang-format on

    struct SharedStartGate
    {
        Atomic<UInt32> readyCount{ 0u };
        Atomic<UInt32> goFlag{ 0u };
    };

    SharedStartGate gate{};

    auto workerEntry = [](void* const userData) -> UInt32
    {
        SharedStartGate* const shared = static_cast<SharedStartGate*>(userData);
        shared->readyCount.fetchAdd(1u, MemoryOrder::AcqRel);
        while (shared->goFlag.load(MemoryOrder::Acquire) == 0u)
        {
            platform::Thread::yieldExecution();
        }
        (void)GP_ENSURE(userData == nullptr);
        return 0u;
    };

    constexpr USize kWorkerCount = 4u;
    platform::Thread workers[kWorkerCount]{};
    for (USize i = 0u; i < kWorkerCount; ++i)
    {
        platform::ThreadDesc desc{};
        desc.name = "EnsureRaceWorker";
        desc.entry = workerEntry;
        desc.userData = &gate;
        ASSERT_EQ(workers[i].start(desc), platform::ThreadResult::Success);
    }

    while (gate.readyCount.load(MemoryOrder::Acquire) < static_cast<UInt32>(kWorkerCount))
    {
        platform::Thread::yieldExecution();
    }
    gate.goFlag.store(1u, MemoryOrder::Release);

    for (USize i = 0u; i < kWorkerCount; ++i)
    {
        (void)workers[i].join();
    }

    EXPECT_EQ(g_concurrentEnsureHits.load(MemoryOrder::Acquire), 1u);
}
#endif

#if GP_DO_CHECK && defined(GTEST_HAS_DEATH_TEST) && GTEST_HAS_DEATH_TEST
TEST(AssertDeathTest, DefaultHandlerLogsAndAbortsProcessOnFatalCheck)
{
    EXPECT_DEATH(
        {
            setAssertHandler(nullptr);
            GP_CHECKF(false, "Fatal death check code=%d", 77);
        },
        "Fatal death check code=77"
    );
}
#endif

}   // namespace gp::diagnostics::tests
