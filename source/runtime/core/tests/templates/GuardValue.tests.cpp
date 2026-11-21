// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "templates/GuardValue.hpp"
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::tests
{

TEST(GuardValueTest, RestoresOriginalValueAfterScopeAndEarlyReturn)
{
    Int32 depth = 0;

    auto recurseOnce = [&]()
    {
        GuardValue guard(depth, depth + 1);
        EXPECT_EQ(depth, 1);
        EXPECT_EQ(guard.originalValue(), 0);

        {
            GuardValue innerGuard(depth, 5);
            EXPECT_EQ(depth, 5);
            EXPECT_EQ(innerGuard.originalValue(), 1);
            return;
        }
    };

    recurseOnce();
    EXPECT_EQ(depth, 0);
}

TEST(GuardValueTest, SaveOnlyConstructorAndMixedLiteralDeductionWork)
{
    Float64 scale = 1.5;
    {
        GuardValue saveOnly(scale);
        EXPECT_DOUBLE_EQ(scale, 1.5);
        scale = 8.0;
        EXPECT_DOUBLE_EQ(saveOnly.originalValue(), 1.5);
    }
    EXPECT_DOUBLE_EQ(scale, 1.5);

    {
        // Int literal assigned to Float64 target exercises CTAD deduction guide.
        GuardValue mixedGuard(scale, 4);
        EXPECT_DOUBLE_EQ(scale, 4.0);
    }
    EXPECT_DOUBLE_EQ(scale, 1.5);

    constexpr auto evaluateConstexpr = []() consteval
    {
        Int32 state = 10;
        {
            GuardValue guard(state, 99);
            if (guard.originalValue() != 10 || state != 99)
            {
                return false;
            }
        }
        return state == 10;
    };
    static_assert(evaluateConstexpr());
}

TEST(GuardValueTest, SupportsMoveOnlyTargetTypesAndIsNonCopyableNonMovable)
{
    struct MoveOnlyToken
    {
        Int32 value = 0;

        explicit constexpr MoveOnlyToken(Int32 inValue) noexcept
            : value(inValue)
        {}

        MoveOnlyToken(const MoveOnlyToken&) = delete;
        MoveOnlyToken& operator=(const MoveOnlyToken&) = delete;

        constexpr MoveOnlyToken(MoveOnlyToken&& other) noexcept
            : value(other.value)
        {
            other.value = -1;
        }

        constexpr MoveOnlyToken& operator=(MoveOnlyToken&& other) noexcept
        {
            if (this != &other)
            {
                value = other.value;
                other.value = -1;
            }
            return *this;
        }
    };

    static_assert(!std::is_copy_constructible_v<GuardValue<MoveOnlyToken>>);
    static_assert(!std::is_move_constructible_v<GuardValue<MoveOnlyToken>>);
    static_assert(!std::is_copy_assignable_v<GuardValue<MoveOnlyToken>>);
    static_assert(!std::is_move_assignable_v<GuardValue<MoveOnlyToken>>);

    MoveOnlyToken activeToken(7);
    {
        GuardValue tokenGuard(activeToken, MoveOnlyToken(42));
        EXPECT_EQ(activeToken.value, 42);
        EXPECT_EQ(tokenGuard.originalValue().value, 7);
    }
    EXPECT_EQ(activeToken.value, 7);
}

}   // namespace gp::tests
