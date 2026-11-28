// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/macos/system/MacOSSharedLibrary.hpp"
#include <gtest/gtest.h>
#include <type_traits>

namespace gp::platform::macos::tests
{

static_assert(
    std::is_base_of_v<gp::platform::apple::SharedLibrary, gp::platform::macos::SharedLibrary>,
    "macos::SharedLibrary must derive from apple::SharedLibrary."
);
static_assert(std::is_final_v<SharedLibrary>, "macos::SharedLibrary must be final.");
static_assert(
    SharedLibrary::getDefaultExtension() == ".dylib", "macos::SharedLibrary default extension must be '.dylib'."
);

#if GP_PLATFORM_MACOS

TEST(MacOSSharedLibraryTest, LoadsSystemLibraryViaMacOSAlias)
{
    void* const handle = SharedLibrary::getHandle("libSystem.B.dylib");
    ASSERT_NE(handle, nullptr);
    EXPECT_NE(SharedLibrary::getExport(handle, "getpid"), nullptr);
    SharedLibrary::freeHandle(handle);
}

#endif   // GP_PLATFORM_MACOS

}   // namespace gp::platform::macos::tests
