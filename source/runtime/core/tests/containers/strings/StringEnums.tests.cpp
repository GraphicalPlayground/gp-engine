// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "containers/strings/StringEnums.hpp"
#include <gtest/gtest.h>

namespace gp::tests
{

TEST(StringEnumsTests, SearchCaseValues)
{
    EXPECT_NE(SearchCase::CaseSensitive, SearchCase::IgnoreCase);
    EXPECT_EQ(static_cast<gp::UInt8>(SearchCase::CaseSensitive), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(SearchCase::IgnoreCase), 1u);
}

TEST(StringEnumsTests, SearchDirValues)
{
    EXPECT_NE(SearchDir::FromStart, SearchDir::FromEnd);
    EXPECT_EQ(static_cast<gp::UInt8>(SearchDir::FromStart), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(SearchDir::FromEnd), 1u);
}

TEST(StringEnumsTests, StringSplitOptionsBitwiseOperations)
{
    constexpr StringSplitOptions none = StringSplitOptions::None;
    constexpr StringSplitOptions cull = StringSplitOptions::CullEmpty;
    constexpr StringSplitOptions trim = StringSplitOptions::TrimTokens;
    constexpr StringSplitOptions combined = StringSplitOptions::CullEmptyAndTrim;

    static_assert((cull | trim) == combined);
    static_assert((combined & cull) == cull);
    static_assert((combined & trim) == trim);
    static_assert((none & cull) == none);

    StringSplitOptions mutableOpts = StringSplitOptions::None;
    mutableOpts |= StringSplitOptions::CullEmpty;
    EXPECT_EQ(mutableOpts, StringSplitOptions::CullEmpty);

    mutableOpts |= StringSplitOptions::TrimTokens;
    EXPECT_EQ(mutableOpts, StringSplitOptions::CullEmptyAndTrim);

    mutableOpts &= ~StringSplitOptions::CullEmpty;
    EXPECT_EQ(mutableOpts, StringSplitOptions::TrimTokens);
}

}   // namespace gp::tests
