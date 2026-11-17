// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "templates/ScopeExit.hpp"
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>
#include <type_traits>
#include <utility>

namespace gp::tests
{

// clang-format off

TEST(ScopeExitTest, ExecutesOnNormalExitEarlyReturnAndInLifoOrder)
{
    Int32 step = 0;
    Int32 trace[3] = { 0, 0, 0 };

    auto runEarlyReturn = [&](const bool early)
    {
        GP_ON_SCOPE_EXIT
        {
            trace[0] = ++step;
        };

        GP_ON_SCOPE_EXIT
        {
            trace[1] = ++step;
        };

        if (early)
        {
            GP_ON_SCOPE_EXIT
            {
                trace[2] = ++step;
            };
            return;
        }

        trace[2] = ++step;
    };

    runEarlyReturn(true);
    EXPECT_EQ(step, 3);
    EXPECT_EQ(trace[2], 1);
    EXPECT_EQ(trace[1], 2);
    EXPECT_EQ(trace[0], 3);
}

TEST(ScopeExitTest, ReleaseCancelsCleanupAndMoveTransfersActiveState)
{
    Int32 rollbackCount = 0;

    {
        auto guard = makeScopeExit([&]() noexcept { ++rollbackCount; });
        EXPECT_TRUE(guard.isActive());

        auto movedGuard = std::move(guard);
        EXPECT_FALSE(guard.isActive());
        EXPECT_TRUE(movedGuard.isActive());

        movedGuard.release();
        EXPECT_FALSE(movedGuard.isActive());
    }
    EXPECT_EQ(rollbackCount, 0);

    {
        auto guard = makeScopeExit([&]() noexcept { ++rollbackCount; });
        auto movedGuard = std::move(guard);
        EXPECT_FALSE(guard.isActive());
        EXPECT_TRUE(movedGuard.isActive());
    }
    EXPECT_EQ(rollbackCount, 1);
}

TEST(ScopeExitTest, MultipleGuardsOnOneLineAndConstexprEvaluationWork)
{
    Int32 counter = 0;
    {
        GP_ON_SCOPE_EXIT { counter += 1; }; GP_ON_SCOPE_EXIT { counter += 10; };
    }
    EXPECT_EQ(counter, 11);

    constexpr auto evaluateConstexpr = []() consteval
    {
        Int32 value = 1;
        {
            auto guard = makeScopeExit([&]() noexcept { value += 41; });
            (void)guard;
        }
        return value;
    };
    static_assert(evaluateConstexpr() == 42);
}

TEST(ScopeExitTest, StatelessMacroGuardCompilesToCompactObject)
{
    auto emptyCallback = []() noexcept {};
    using GuardType = ScopeExit<decltype(emptyCallback)>;
    static_assert(sizeof(GuardType) == 1);
    static_assert(!std::is_copy_constructible_v<GuardType>);
    static_assert(!std::is_copy_assignable_v<GuardType>);
    static_assert(!std::is_move_assignable_v<GuardType>);
    static_assert(std::is_move_constructible_v<GuardType>);
    SUCCEED();
}

// clang-format on

}   // namespace gp::tests
