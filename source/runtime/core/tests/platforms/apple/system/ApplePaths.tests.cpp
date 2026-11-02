// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/apple/system/ApplePaths.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::platform::apple::tests
{

static_assert(
    std::is_base_of_v<gp::platform::unix::Paths, gp::platform::apple::Paths>,
    "apple::Paths must derive from unix::Paths."
);

static_assert(
    Paths::resolveMacOsBundleResourcesDir("/Applications/GraphicalPlayground.app/Contents/MacOS/").view() ==
        "/Applications/GraphicalPlayground.app/Contents/Resources/",
    "resolveMacOsBundleResourcesDir must map .app/Contents/MacOS/ to .app/Contents/Resources/ at compile time."
);

static_assert(
    Paths::resolveMacOsBundleResourcesDir("/Users/developer/bin/").view() == "/Users/developer/bin/",
    "resolveMacOsBundleResourcesDir must fall back to the executable directory when not inside a .app bundle."
);

TEST(ApplePathsContractTest, InheritsLexicalAndBundleFallbacks)
{
    const PathBuffer appSupport = Paths::resolveXdgDirectory("", "/Users/developer/", "Library/Application Support");
    EXPECT_EQ(appSupport.view(), gp::StringView("/Users/developer/Library/Application Support/"));

    const PathBuffer caches = Paths::resolveXdgDirectory("", "/Users/developer/", "Library/Caches");
    EXPECT_EQ(caches.view(), gp::StringView("/Users/developer/Library/Caches/"));

    const PathBuffer bundleRes = Paths::resolveMacOsBundleResourcesDir("/Applications/MyGame.app/Contents/MacOS");
    EXPECT_EQ(bundleRes.view(), gp::StringView("/Applications/MyGame.app/Contents/Resources/"));

    const PathBuffer nonBundleRes = Paths::resolveMacOsBundleResourcesDir("/usr/local/bin/");
    EXPECT_EQ(nonBundleRes.view(), gp::StringView("/usr/local/bin/"));
}

#if GP_PLATFORM_APPLE

TEST(ApplePathsDarwinTest, ExecutableAndLibraryDirectoriesAreValid)
{
    const gp::StringView exePath = Paths::executablePath();
    const gp::StringView appData = Paths::appDataDir();
    EXPECT_FALSE(exePath.isEmpty());
    EXPECT_TRUE(Paths::isAbsolute(exePath));
    EXPECT_TRUE(Paths::isAbsolute(appData));
    EXPECT_TRUE(appData.endsWith('/'));
}

#endif

}   // namespace gp::platform::apple::tests
