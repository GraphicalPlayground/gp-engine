// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/unix/system/UnixSharedLibrary.hpp"
#include "containers/arrays/Vector.hpp"
#include "platforms/unix/system/UnixSharedLibraryNaming.hpp"
#include <gtest/gtest.h>
#include <type_traits>

#if GP_PLATFORM_UNIX
    #include "platforms/base/system/FileSystem.hpp"
    #include "platforms/base/system/Paths.hpp"
#endif

namespace gp::platform::unix::tests
{

static_assert(
    std::is_base_of_v<gp::platform::generic::SharedLibrary, gp::platform::unix::SharedLibrary>,
    "unix::SharedLibrary must derive from generic::SharedLibrary."
);
static_assert(!std::is_default_constructible_v<SharedLibrary>, "unix::SharedLibrary must not be constructible.");
static_assert(SharedLibrary::getDefaultExtension() == ".so", "unix::SharedLibrary default extension must be '.so'.");

static_assert(SharedLibraryNaming::isSharedObjectFilename("libvulkan.so"), "Standard .so file must match.");
static_assert(SharedLibraryNaming::isSharedObjectFilename("libvulkan.so.1"), "Versioned .so.1 file must match.");
static_assert(SharedLibraryNaming::isSharedObjectFilename("libGL.so.1.7.0"), "Multi-part .so version must match.");
static_assert(!SharedLibraryNaming::isSharedObjectFilename(".so"), "Empty stem before .so must not match.");
static_assert(
    !SharedLibraryNaming::isSharedObjectFilename("libvulkan.so.bak"), "Non-numeric suffix after .so. must not match."
);
static_assert(
    !SharedLibraryNaming::isSharedObjectFilename("libvulkan.dylib"), "Apple dylib must not match Unix .so filter."
);

TEST(UnixSharedLibraryNamingTest, ExpandsBareNamesWithPosixLibPrefixAndSoExtension)
{
    gp::InlineVector<PathBuffer, 4u> candidates{};

    // Bare stem -> exact, stem.so, libstem.so.
    SharedLibraryNaming::buildCandidateFilenames("vulkan", candidates);
    ASSERT_EQ(candidates.size(), 3u);
    EXPECT_EQ(candidates[0].view(), gp::StringView("vulkan"));
    EXPECT_EQ(candidates[1].view(), gp::StringView("vulkan.so"));
    EXPECT_EQ(candidates[2].view(), gp::StringView("libvulkan.so"));

    // Prefixed stem -> exact, libstem.so.
    SharedLibraryNaming::buildCandidateFilenames("libvulkan", candidates);
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0].view(), gp::StringView("libvulkan"));
    EXPECT_EQ(candidates[1].view(), gp::StringView("libvulkan.so"));

    // Unprefixed .so -> exact, libname.so.
    SharedLibraryNaming::buildCandidateFilenames("vulkan.so", candidates);
    ASSERT_EQ(candidates.size(), 2u);
    EXPECT_EQ(candidates[0].view(), gp::StringView("vulkan.so"));
    EXPECT_EQ(candidates[1].view(), gp::StringView("libvulkan.so"));

    // Fully qualified versioned soname stays singular.
    SharedLibraryNaming::buildCandidateFilenames("libvulkan.so.1", candidates);
    ASSERT_EQ(candidates.size(), 1u);
    EXPECT_EQ(candidates[0].view(), gp::StringView("libvulkan.so.1"));

    // Empty input yields no candidates.
    SharedLibraryNaming::buildCandidateFilenames("", candidates);
    EXPECT_TRUE(candidates.isEmpty());
}

TEST(UnixSharedLibraryNamingTest, TryBuildWithDefaultExtensionRespectsExistingSoSuffix)
{
    PathBuffer out{};
    EXPECT_TRUE(SharedLibraryNaming::tryBuildWithDefaultExtension("/opt/plugins/myplugin", out));
    EXPECT_EQ(out.view(), gp::StringView("/opt/plugins/myplugin.so"));

    EXPECT_FALSE(SharedLibraryNaming::tryBuildWithDefaultExtension("/opt/plugins/libmyplugin.so", out));
    EXPECT_FALSE(SharedLibraryNaming::tryBuildWithDefaultExtension("/opt/plugins/libmyplugin.so.2", out));
    EXPECT_FALSE(SharedLibraryNaming::tryBuildWithDefaultExtension("trailing.", out));
    EXPECT_FALSE(SharedLibraryNaming::tryBuildWithDefaultExtension("", out));
}

#if GP_PLATFORM_UNIX

TEST(UnixSharedLibraryPosixTest, InvalidInputsAndSearchPathLifecycleAreSafe)
{
    SharedLibrary::resetSearchPaths();

    EXPECT_EQ(SharedLibrary::getHandle(""), nullptr);
    EXPECT_EQ(SharedLibrary::getHandle("libgp_definitely_nonexistent_987654321.so"), nullptr);
    EXPECT_EQ(SharedLibrary::getExport(nullptr, "getpid"), nullptr);
    SharedLibrary::freeHandle(nullptr);

    const PathBuffer testDir = Paths::combine(Paths::tempDir(), "gp_unix_shlib_stack_test");
    SharedLibrary::pushDirectory(testDir.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 1u);
    EXPECT_TRUE(Paths::equals(SharedLibrary::getActiveDirectory().view(), testDir.view(), true));

    SharedLibrary::popDirectory(testDir.view());
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 0u);
    EXPECT_TRUE(SharedLibrary::getActiveDirectory().isEmpty());

    SharedLibrary::resetSearchPaths();
}

#endif   // GP_PLATFORM_UNIX

}   // namespace gp::platform::unix::tests
