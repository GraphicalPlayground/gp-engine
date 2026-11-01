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

static_assert(
    Paths::parseXdgUserDirsEntry(
        "# ~/.config/user-dirs.dirs\nXDG_DESKTOP_DIR=\"$HOME/Desktop\"\nXDG_DOCUMENTS_DIR=\"$HOME/MyDocs\"\n",
        "XDG_DOCUMENTS_DIR",
        "/home/developer/"
    )
            .view() == "/home/developer/MyDocs/",
    "parseXdgUserDirsEntry must expand $HOME and normalize the configured user directory at compile time."
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

TEST(UnixPathsXdgSpecTest, ParseXdgUserDirsEntryExpandsHomeAndIgnoresComments)
{
    constexpr gp::StringView kSampleUserDirs =
        "# This file is written by xdg-user-dirs-update\n"
        "# XDG_DOCUMENTS_DIR=\"/commented/out\"\n"
        "XDG_DOWNLOAD_DIR=\"$HOME/Downloads\"\n"
        "XDG_DOCUMENTS_DIR=\"$HOME/Workspace/Docs\"\n"
        "XDG_PICTURES_DIR=\"/mnt/media/Pictures\"\n";

    const PathBuffer docsDir = Paths::parseXdgUserDirsEntry(kSampleUserDirs, "XDG_DOCUMENTS_DIR", "/home/user/");
    EXPECT_EQ(docsDir.view(), gp::StringView("/home/user/Workspace/Docs/"));

    const PathBuffer picsDir = Paths::parseXdgUserDirsEntry(kSampleUserDirs, "XDG_PICTURES_DIR", "/home/user/");
    EXPECT_EQ(picsDir.view(), gp::StringView("/mnt/media/Pictures/"));

    const PathBuffer missingDir = Paths::parseXdgUserDirsEntry(kSampleUserDirs, "XDG_MUSIC_DIR", "/home/user/");
    EXPECT_TRUE(missingDir.isEmpty());
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
