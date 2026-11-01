// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/macos/system/MacOSPaths.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::platform::macos::tests
{

static_assert(
    std::is_base_of_v<gp::platform::apple::Paths, gp::platform::macos::Paths>,
    "macos::Paths must derive from apple::Paths."
);

TEST(MacOSPathsContractTest, InheritsAppleAndGenericLexicalContract)
{
    const PathBuffer resolved =
        Paths::resolve("../Resources/Shaders/Main.metal", "/Applications/GPEngine.app/Contents/MacOS/");
    EXPECT_EQ(resolved.view(), gp::StringView("/Applications/GPEngine.app/Contents/Resources/Shaders/Main.metal"));
}

}   // namespace gp::platform::macos::tests
