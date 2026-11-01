// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/linux/system/LinuxPaths.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::platform::linux::tests
{

static_assert(
    std::is_base_of_v<gp::platform::unix::Paths, gp::platform::linux::Paths>,
    "linux::Paths must derive from unix::Paths."
);

TEST(LinuxPathsContractTest, InheritsLexicalAndXdgUtilities)
{
    const PathBuffer resolved = Paths::resolve("../Shaders/PBR.hlsl", "/opt/gp-engine/bin/");
    EXPECT_EQ(resolved.view(), gp::StringView("/opt/gp-engine/Shaders/PBR.hlsl"));
}

#if GP_PLATFORM_LINUX

TEST(LinuxPathsProcfsTest, ExecutablePathAndDirViaProcSelfExe)
{
    const gp::StringView exePath = Paths::executablePath();
    const gp::StringView exeDir = Paths::executableDir();
    EXPECT_FALSE(exePath.isEmpty());
    EXPECT_TRUE(Paths::isAbsolute(exePath));
    EXPECT_TRUE(Paths::isAbsolute(exeDir));
    EXPECT_TRUE(exeDir.endsWith('/'));
}

#endif

}   // namespace gp::platform::linux::tests
