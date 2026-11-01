// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/system/PathTypes.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::platform::tests
{

static_assert(kMaxPathLength == 1024u, "kMaxPathLength must be 1024.");
static_assert(kPreferredSeparator == '/', "kPreferredSeparator must be '/'.");
static_assert(kAltSeparator == '\\', "kAltSeparator must be '\\'.");
static_assert(std::is_same_v<PathBuffer, gp::PathBuffer>, "gp::PathBuffer must alias gp::platform::PathBuffer.");
static_assert(std::is_standard_layout_v<PathSplitResult>, "PathSplitResult must be standard layout.");

constexpr bool testConstexprPathSplitResult() noexcept
{
    const PathSplitResult a{ "Engine/Source", "Paths", "cpp" };
    const PathSplitResult b{ "Engine/Source", "Paths", "cpp" };
    const PathSplitResult c{ "Engine/Source", "Paths", "hpp" };
    return (a == b) && !(a == c);
}

static_assert(testConstexprPathSplitResult(), "PathSplitResult equality must work in constexpr contexts.");

TEST(PathTypesTest, SeparatorConstantsAndPlatformNativeSeparator)
{
    EXPECT_EQ(kPreferredSeparator, '/');
    EXPECT_EQ(kAltSeparator, '\\');
#if GP_PLATFORM_WINDOWS
    EXPECT_EQ(kNativeSeparator, '\\');
#else
    EXPECT_EQ(kNativeSeparator, '/');
#endif
}

TEST(PathTypesTest, PathBufferCapacityAndNullTermination)
{
    PathBuffer buf("C:/Projects/gp-engine");
    EXPECT_EQ(buf.capacity(), kMaxPathLength);
    EXPECT_EQ(buf.maxSize(), kMaxPathLength);
    EXPECT_EQ(buf.size(), 21u);
    EXPECT_STREQ(buf.cString(), "C:/Projects/gp-engine");
    EXPECT_EQ(buf.view(), gp::StringView("C:/Projects/gp-engine"));
}

TEST(PathTypesTest, PathSplitResultDefaultAndCustomEquality)
{
    const PathSplitResult emptySplit{};
    EXPECT_TRUE(emptySplit.directory.isEmpty());
    EXPECT_TRUE(emptySplit.filename.isEmpty());
    EXPECT_TRUE(emptySplit.extension.isEmpty());

    const PathSplitResult splitA{ "a/b", "file", "txt" };
    const PathSplitResult splitB{ "a/b", "file", "txt" };
    const PathSplitResult splitC{ "a/b", "other", "txt" };

    EXPECT_EQ(splitA, splitB);
    EXPECT_NE(splitA, splitC);
}

}   // namespace gp::platform::tests
