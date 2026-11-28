// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/system/SharedLibrary.hpp"
#include "platforms/generic/system/GenericSharedLibrary.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::platform::tests
{

static_assert(
    !std::is_default_constructible_v<SharedLibrary>,
    "SharedLibrary must be a static-only utility type and not default-constructible."
);
static_assert(!std::is_copy_constructible_v<SharedLibrary>, "SharedLibrary must not be copy-constructible.");
static_assert(!std::is_move_constructible_v<SharedLibrary>, "SharedLibrary must not be move-constructible.");

class SharedLibraryBaseTest : public ::testing::Test
{
protected:
    void SetUp() noexcept override
    {
        SharedLibrary::resetSearchPaths();
    }

    void TearDown() noexcept override
    {
        SharedLibrary::resetSearchPaths();
    }
};

TEST_F(SharedLibraryBaseTest, GenericFallbackContractReturnsSafeNullAndZeroValues)
{
    EXPECT_EQ(gp::platform::generic::SharedLibrary::getHandle("nonexistent.lib"), nullptr);
    EXPECT_EQ(gp::platform::generic::SharedLibrary::getExport(nullptr, "symbol"), nullptr);
    gp::platform::generic::SharedLibrary::freeHandle(nullptr);

    gp::platform::generic::SharedLibrary::addDirectory("some/dir");
    gp::platform::generic::SharedLibrary::pushDirectory("some/dir");
    gp::platform::generic::SharedLibrary::popDirectory("some/dir");
    gp::platform::generic::SharedLibrary::resetSearchPaths();

    EXPECT_TRUE(gp::platform::generic::SharedLibrary::getDefaultExtension().isEmpty());
    EXPECT_EQ(gp::platform::generic::SharedLibrary::getDirectoryStackDepth(), 0u);
    EXPECT_TRUE(gp::platform::generic::SharedLibrary::getActiveDirectory().isEmpty());
    EXPECT_EQ(gp::platform::generic::SharedLibrary::getRegisteredDirectoryCount(), 0u);
    EXPECT_EQ(gp::platform::generic::SharedLibrary::getCachedLibraryCount(), 0u);
}

TEST_F(SharedLibraryBaseTest, InvalidInputsAreHandledGracefully)
{
    EXPECT_EQ(SharedLibrary::getHandle(""), nullptr);
    EXPECT_EQ(SharedLibrary::getHandle("gp_definitely_nonexistent_library_987654321.dll"), nullptr);
    EXPECT_EQ(SharedLibrary::getExport(nullptr, "SomeSymbol"), nullptr);
    EXPECT_EQ(SharedLibrary::getExport(nullptr, ""), nullptr);

    // Passing nullptr to freeHandle must always be a safe no-op.
    SharedLibrary::freeHandle(nullptr);
}

TEST_F(SharedLibraryBaseTest, InitialSearchPathStateIsEmpty)
{
    EXPECT_EQ(SharedLibrary::getDirectoryStackDepth(), 0u);
    EXPECT_TRUE(SharedLibrary::getActiveDirectory().isEmpty());
    EXPECT_EQ(SharedLibrary::getRegisteredDirectoryCount(), 0u);
    EXPECT_EQ(SharedLibrary::getCachedLibraryCount(), 0u);
}

#if GP_PLATFORM_WINDOWS
TEST_F(SharedLibraryBaseTest, PlatformAliasResolvesToWindowsSharedLibrary)
{
    EXPECT_TRUE((std::is_same_v<SharedLibrary, gp::platform::windows::SharedLibrary>));
    EXPECT_EQ(SharedLibrary::getDefaultExtension(), gp::StringView(".dll"));
}
#elif GP_PLATFORM_LINUX
TEST_F(SharedLibraryBaseTest, PlatformAliasResolvesToLinuxSharedLibrary)
{
    EXPECT_TRUE((std::is_same_v<SharedLibrary, gp::platform::linux::SharedLibrary>));
    EXPECT_EQ(SharedLibrary::getDefaultExtension(), gp::StringView(".so"));
}
#elif GP_PLATFORM_MACOS
TEST_F(SharedLibraryBaseTest, PlatformAliasResolvesToMacOSSharedLibrary)
{
    EXPECT_TRUE((std::is_same_v<SharedLibrary, gp::platform::macos::SharedLibrary>));
    EXPECT_EQ(SharedLibrary::getDefaultExtension(), gp::StringView(".dylib"));
}
#endif

}   // namespace gp::platform::tests
