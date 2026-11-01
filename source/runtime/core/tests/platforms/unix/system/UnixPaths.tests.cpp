// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/unix/system/UnixPaths.hpp"
#include <gtest/gtest.h>

namespace gp::platform::unix::tests
{

// Verify freedesktop.org XDG Base Directory Specification rules at compile time across all host OSes
static_assert(
    Paths::resolveXdgDirectory("/custom/xdg/data", "/home/developer/", ".local/share").view() == "/custom/xdg/data/",
    "Absolute XDG environment path must take precedence over HOME fallback."
);

static_assert(
    Paths::resolveXdgDirectory("", "/home/developer/", ".local/share").view() == "/home/developer/.local/share/",
    "Empty XDG environment variable must fall back to HOME/fallbackRelativeSubpath/."
);

static_assert(
    Paths::resolveXdgDirectory("relative/untrusted/path", "/home/developer/", ".config").view() ==
        "/home/developer/.config/",
    "Per XDG Base Directory Specification, relative XDG environment paths must be ignored."
);

TEST(UnixPathsXdgSpecTest, ResolveXdgDirectoryHonorsAbsoluteAndRejectsRelativeEnvPaths)
{
    const PathBuffer customData =
        Paths::resolveXdgDirectory("/var/lib/gp-data/../gp-engine", "/home/user/", ".local/share");
    EXPECT_EQ(customData.view(), gp::StringView("/var/lib/gp-engine/"));

    const PathBuffer fallbackCache = Paths::resolveXdgDirectory("", "/home/user", ".cache");
    EXPECT_EQ(fallbackCache.view(), gp::StringView("/home/user/.cache/"));

    const PathBuffer rejectedRelative = Paths::resolveXdgDirectory("./relative_xdg", "/home/user/", ".config");
    EXPECT_EQ(rejectedRelative.view(), gp::StringView("/home/user/.config/"));
}

#if GP_PLATFORM_UNIX

TEST(UnixPathsPosixTest, PosixKnownDirectoriesAreAbsoluteAndNormalized)
{
    const gp::StringView home = Paths::homeDir();
    const gp::StringView appData = Paths::appDataDir();
    const gp::StringView localAppData = Paths::localAppDataDir();
    const gp::StringView userConfig = Paths::userConfigDir();
    const gp::StringView documents = Paths::documentsDir();
    const gp::StringView temp = Paths::tempDir();

    EXPECT_TRUE(Paths::isAbsolute(home));
    EXPECT_TRUE(home.endsWith('/'));
    EXPECT_TRUE(Paths::isAbsolute(appData));
    EXPECT_TRUE(appData.endsWith('/'));
    EXPECT_TRUE(Paths::isAbsolute(localAppData));
    EXPECT_TRUE(localAppData.endsWith('/'));
    EXPECT_TRUE(Paths::isAbsolute(userConfig));
    EXPECT_TRUE(userConfig.endsWith('/'));
    EXPECT_TRUE(Paths::isAbsolute(documents));
    EXPECT_TRUE(documents.endsWith('/'));
    EXPECT_TRUE(Paths::isAbsolute(temp));
    EXPECT_TRUE(temp.endsWith('/'));
}

#endif

}   // namespace gp::platform::unix::tests
