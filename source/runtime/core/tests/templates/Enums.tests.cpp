// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include <gtest/gtest.h>

// Include the enums header to test the bitwise and comparison operators.
#include "templates/Enums.hpp"

namespace gp::tests
{

// Enum designed for bitwise operations
enum class TestBitwiseFlags : gp::UInt8
{
    None = 0,
    FlagA = 1 << 0,
    FlagB = 1 << 1,
    FlagC = 1 << 2
};

// Enum designed for comparison operations
enum class TestCompareLevels : gp::UInt8
{
    Minimum = 0,
    Medium = 1,
    Maximum = 2
};

// Standard enum with no macros applied (used to verify negative trait checks)
enum class StandardEnum : gp::UInt8
{
    Value1 = 0,
    Value2 = 1
};

}   // namespace gp::tests

// Enable macros must be declared at a scope where they can properly specialize
// the templates in the `gp::detail` namespace (global scope is standard/safe).
GP_ENABLE_ENUM_BITWISE_OPERATIONS(gp::tests::TestBitwiseFlags);
GP_ENABLE_ENUM_COMPARISON_OPERATIONS(gp::tests::TestCompareLevels);

namespace gp::tests
{

TEST(EnumUtilsTraitsTest, VerifiesBitwiseTraits)
{
    EXPECT_TRUE(detail::HasBitwiseOperatorsV<TestBitwiseFlags>);

    EXPECT_FALSE(detail::HasBitwiseOperatorsV<TestCompareLevels>);
    EXPECT_FALSE(detail::HasBitwiseOperatorsV<StandardEnum>);
}

TEST(EnumUtilsTraitsTest, VerifiesComparisonTraits)
{
    EXPECT_TRUE(detail::HasComparisonOperatorsV<TestCompareLevels>);

    EXPECT_FALSE(detail::HasComparisonOperatorsV<TestBitwiseFlags>);
    EXPECT_FALSE(detail::HasComparisonOperatorsV<StandardEnum>);
}

TEST(EnumUtilsBitwiseOpsTest, BitwiseOr)
{
    auto result = TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB;
    EXPECT_EQ(static_cast<uint8_t>(result), 3);
}

TEST(EnumUtilsBitwiseOpsTest, BitwiseAnd)
{
    auto combined = TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB;
    auto result = combined & TestBitwiseFlags::FlagA;
    EXPECT_EQ(result, TestBitwiseFlags::FlagA);

    auto empty = combined & TestBitwiseFlags::FlagC;
    EXPECT_EQ(empty, TestBitwiseFlags::None);
}

TEST(EnumUtilsBitwiseOpsTest, BitwiseXor)
{
    auto combined = TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB;
    auto result = combined ^ TestBitwiseFlags::FlagB;
    EXPECT_EQ(result, TestBitwiseFlags::FlagA);
}

TEST(EnumUtilsBitwiseOpsTest, BitwiseNot)
{
    auto result = ~TestBitwiseFlags::FlagA;
    // ~1 in uint8_t is 254 (0xFE)
    EXPECT_EQ(static_cast<uint8_t>(result), static_cast<uint8_t>(~1));
}

TEST(EnumUtilsBitwiseOpsTest, BitwiseOrAssignment)
{
    auto flags = TestBitwiseFlags::FlagA;
    flags |= TestBitwiseFlags::FlagB;
    EXPECT_EQ(static_cast<uint8_t>(flags), 3);
}

TEST(EnumUtilsBitwiseOpsTest, BitwiseAndAssignment)
{
    auto flags = TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB;
    flags &= TestBitwiseFlags::FlagB;
    EXPECT_EQ(flags, TestBitwiseFlags::FlagB);
}

TEST(EnumUtilsBitwiseOpsTest, BitwiseXorAssignment)
{
    auto flags = TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB;
    flags ^= TestBitwiseFlags::FlagA;
    EXPECT_EQ(flags, TestBitwiseFlags::FlagB);
}

TEST(EnumUtilsFunctionsTest, HasAnyFlags)
{
    EXPECT_FALSE(enums::hasAnyFlags(TestBitwiseFlags::None));
    EXPECT_TRUE(enums::hasAnyFlags(TestBitwiseFlags::FlagA));

    auto combined = TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB;
    EXPECT_TRUE(enums::hasAnyFlags(combined));
}

TEST(EnumUtilsFunctionsTest, HasAllFlags)
{
    auto combined = TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB;

    EXPECT_TRUE(enums::hasAllFlags(combined, TestBitwiseFlags::FlagA));
    EXPECT_TRUE(enums::hasAllFlags(combined, TestBitwiseFlags::FlagB));
    EXPECT_TRUE(enums::hasAllFlags(combined, combined));

    EXPECT_FALSE(enums::hasAllFlags(combined, TestBitwiseFlags::FlagC));
    auto allThree = TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB | TestBitwiseFlags::FlagC;
    EXPECT_FALSE(enums::hasAllFlags(combined, allThree));
}

TEST(EnumUtilsFunctionsTest, HasNoFlags)
{
    auto combined = TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB;

    EXPECT_TRUE(enums::hasNoFlags(combined, TestBitwiseFlags::FlagC));
    EXPECT_FALSE(enums::hasNoFlags(combined, TestBitwiseFlags::FlagA));
    EXPECT_FALSE(enums::hasNoFlags(combined, combined));
}

TEST(EnumUtilsFunctionsTest, SetFlags)
{
    auto flags = TestBitwiseFlags::FlagA;
    auto result = enums::setFlags(flags, TestBitwiseFlags::FlagB);

    EXPECT_EQ(result, (TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB));
    EXPECT_TRUE(enums::hasAllFlags(result, TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB));
}

TEST(EnumUtilsFunctionsTest, ClearFlags)
{
    auto combined = TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB;
    auto result = enums::clearFlags(combined, TestBitwiseFlags::FlagA);

    EXPECT_EQ(result, TestBitwiseFlags::FlagB);

    // Clearing a flag that isn't set should do nothing
    auto result2 = enums::clearFlags(result, TestBitwiseFlags::FlagC);
    EXPECT_EQ(result2, TestBitwiseFlags::FlagB);
}

TEST(EnumUtilsFunctionsTest, ToggleFlags)
{
    auto flags = TestBitwiseFlags::FlagA;

    // Toggle on
    flags = enums::toggleFlags(flags, TestBitwiseFlags::FlagB);
    EXPECT_EQ(flags, (TestBitwiseFlags::FlagA | TestBitwiseFlags::FlagB));

    // Toggle off
    flags = enums::toggleFlags(flags, TestBitwiseFlags::FlagA);
    EXPECT_EQ(flags, TestBitwiseFlags::FlagB);
}

TEST(EnumUtilsComparisonOpsTest, Equality)
{
    EXPECT_TRUE(TestCompareLevels::Medium == TestCompareLevels::Medium);
    EXPECT_FALSE(TestCompareLevels::Minimum == TestCompareLevels::Maximum);
}

TEST(EnumUtilsComparisonOpsTest, Inequality)
{
    EXPECT_TRUE(TestCompareLevels::Minimum != TestCompareLevels::Maximum);
    EXPECT_FALSE(TestCompareLevels::Medium != TestCompareLevels::Medium);
}

TEST(EnumUtilsComparisonOpsTest, LessThan)
{
    EXPECT_TRUE(TestCompareLevels::Minimum < TestCompareLevels::Medium);
    EXPECT_TRUE(TestCompareLevels::Medium < TestCompareLevels::Maximum);
    EXPECT_FALSE(TestCompareLevels::Maximum < TestCompareLevels::Minimum);
    EXPECT_FALSE(TestCompareLevels::Medium < TestCompareLevels::Medium);
}

TEST(EnumUtilsComparisonOpsTest, LessThanOrEqual)
{
    EXPECT_TRUE(TestCompareLevels::Minimum <= TestCompareLevels::Medium);
    EXPECT_TRUE(TestCompareLevels::Medium <= TestCompareLevels::Medium);
    EXPECT_FALSE(TestCompareLevels::Maximum <= TestCompareLevels::Minimum);
}

TEST(EnumUtilsComparisonOpsTest, GreaterThan)
{
    EXPECT_TRUE(TestCompareLevels::Maximum > TestCompareLevels::Medium);
    EXPECT_TRUE(TestCompareLevels::Medium > TestCompareLevels::Minimum);
    EXPECT_FALSE(TestCompareLevels::Minimum > TestCompareLevels::Maximum);
    EXPECT_FALSE(TestCompareLevels::Medium > TestCompareLevels::Medium);
}

TEST(EnumUtilsComparisonOpsTest, GreaterThanOrEqual)
{
    EXPECT_TRUE(TestCompareLevels::Maximum >= TestCompareLevels::Medium);
    EXPECT_TRUE(TestCompareLevels::Medium >= TestCompareLevels::Medium);
    EXPECT_FALSE(TestCompareLevels::Minimum >= TestCompareLevels::Maximum);
}

}   // namespace gp::tests
