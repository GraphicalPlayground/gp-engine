// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "platforms/base/Platform.hpp"
#include "CoreMinimal.hpp"
#include <gtest/gtest.h>

#if GP_PLATFORM_WINDOWS
    #include "platforms/windows/AllowWindowsPlatformTypes.hpp"
GP_THIRD_PARTY_INCLUDES_START
    #include <dxgi1_6.h>
GP_THIRD_PARTY_INCLUDES_END
    #include "platforms/windows/HideWindowsPlatformTypes.hpp"
#elif GP_PLATFORM_LINUX
    #include "platforms/base/AllowPlatformTypes.hpp"
    #define None 0L
    #define Success 0
    #define Always 2
    #define Bool int
    #define Status int
    #include "platforms/base/HidePlatformTypes.hpp"
#elif GP_PLATFORM_MACOS
    #include "platforms/base/AllowPlatformTypes.hpp"
    #define check(x) ((void)(x))
    #define verify(x) ((void)(x))
    #define require(x, label) ((void)(x))
    #include "platforms/base/HidePlatformTypes.hpp"
#endif

// Verify nested and repeated AllowPlatformTypes / HidePlatformTypes brackets preserve pre-existing macros.
#define GP_GUARD_TEST_SENTINEL 123
#include "platforms/base/AllowPlatformTypes.hpp"
static_assert(GP_GUARD_TEST_SENTINEL == 123, "AllowPlatformTypes must not clobber unrelated macros.");
#include "platforms/base/HidePlatformTypes.hpp"
static_assert(GP_GUARD_TEST_SENTINEL == 123, "HidePlatformTypes must preserve unrelated macros.");
#undef GP_GUARD_TEST_SENTINEL

namespace gp::tests
{

namespace
{

template <typename InT>
concept HasArrayCount = requires(const InT& value) { gp::arrayCount(value); };

template <typename InT>
concept HasArrayCountMacro = requires(const InT& value) { GP_ARRAY_COUNT(value); };

GP_DEPRECATED(0.2.0, "Use replacementValue instead.")

constexpr gp::Int32 deprecatedHelper(const gp::Int32 value) noexcept
{
    return value + 1;
}

GP_THIRD_PARTY_INCLUDES_START
inline gp::Int32 thirdPartyWarningProbe(gp::Int32 unusedParameter) noexcept
{
    return 42;
}

GP_THIRD_PARTY_INCLUDES_END

struct DummyPair
{
    gp::Int32 first{ 0 };
    gp::Int32 second{ 0 };
};

constexpr gp::Int32 kCompileTimeInts[4]{ 10, 20, 30, 40 };
constexpr gp::Char kCompileTimeChars[]{ "gp-rhi" };
constexpr DummyPair kCompileTimeStructs[3]{};
constexpr gp::Int32 kCompileTimeMatrix[2][5]{};

static_assert(gp::arrayCount(kCompileTimeInts) == 4u, "gp::arrayCount failed for 1D int array.");
static_assert(GP_ARRAY_COUNT(kCompileTimeInts) == 4u, "GP_ARRAY_COUNT failed for 1D int array.");
static_assert(gp::arrayCount(kCompileTimeChars) == 7u, "gp::arrayCount failed for char array with null terminator.");
static_assert(GP_ARRAY_COUNT(kCompileTimeChars) == 7u, "GP_ARRAY_COUNT failed for char array.");
static_assert(gp::arrayCount(kCompileTimeStructs) == 3u, "gp::arrayCount failed for struct array.");
static_assert(GP_ARRAY_COUNT(kCompileTimeMatrix) == 2u, "GP_ARRAY_COUNT failed for 2D outer dimension.");
static_assert(gp::arrayCount(kCompileTimeMatrix[0]) == 5u, "gp::arrayCount failed for 2D inner dimension.");

static_assert(HasArrayCount<gp::Int32[4]>, "Fixed C array must satisfy HasArrayCount.");
static_assert(!HasArrayCount<gp::Int32*>, "Pointer must be rejected by gp::arrayCount.");
static_assert(!HasArrayCount<const gp::Int32*>, "Const pointer must be rejected by gp::arrayCount.");
static_assert(HasArrayCountMacro<gp::Int32[4]>, "Fixed C array must satisfy HasArrayCountMacro.");
static_assert(!HasArrayCountMacro<gp::Int32*>, "Pointer must be rejected by GP_ARRAY_COUNT.");
static_assert(!HasArrayCountMacro<const gp::Int32*>, "Const pointer must be rejected by GP_ARRAY_COUNT.");

}   // namespace

TEST(PlatformMacrosTest, ArrayCountEvaluatesRuntimeAndConstexprArrays)
{
    gp::Int32 runtimeArray[7]{ 1, 2, 3, 4, 5, 6, 7 };

    EXPECT_EQ(gp::arrayCount(runtimeArray), 7u);
    EXPECT_EQ(GP_ARRAY_COUNT(runtimeArray), 7u);
    EXPECT_EQ(gp::arrayCount(kCompileTimeInts), 4u);
    EXPECT_EQ(GP_ARRAY_COUNT(kCompileTimeInts), 4u);
}

TEST(PlatformMacrosTest, DeprecationAndThirdPartyWarningBracketsCompileCleanly)
{
    GP_DISABLE_DEPRECATION_WARNINGS
    const gp::Int32 deprecatedResult = deprecatedHelper(41);
    GP_ENABLE_DEPRECATION_WARNINGS

    EXPECT_EQ(deprecatedResult, 42);
    EXPECT_EQ(thirdPartyWarningProbe(99), 42);
}

TEST(PlatformMacrosTest, BuildFlagsAreWellFormed)
{
    constexpr gp::Int32 withTestsFlag = GP_WITH_TESTS;
    EXPECT_TRUE(withTestsFlag == GP_FALSE || withTestsFlag == GP_TRUE);
}

TEST(PlatformHeaderGuardsTest, AllowAndHidePlatformTypesIsolateMacrosAndSupportNesting)
{
#if GP_PLATFORM_WINDOWS
    const gp::USize dxgiDescSize = sizeof(DXGI_ADAPTER_DESC1);
    EXPECT_GT(dxgiDescSize, 0u);

    enum class CleanEngineSymbols : gp::UInt8
    {
        Yield = 0,
        MemoryBarrier = 1,
        GetObject = 2,
        CreateDirectory = 3,
        DeleteFile = 4,
        PF_MAX = 5
    };
    EXPECT_EQ(static_cast<gp::UInt8>(CleanEngineSymbols::PF_MAX), 5u);

    const gp::Int32 min = 3;
    const gp::Int32 max = 9;
    EXPECT_LT(min, max);
#elif GP_PLATFORM_LINUX
    enum class CleanLinuxSymbols : gp::UInt8
    {
        None = 0,
        Success = 1,
        Always = 2
    };
    EXPECT_EQ(static_cast<gp::UInt8>(CleanLinuxSymbols::Always), 2u);
#elif GP_PLATFORM_MACOS
    const gp::Int32 check = 1;
    const gp::Int32 verify = 2;
    const gp::Int32 require = 3;
    EXPECT_EQ(check + verify + require, 6);
#endif
}

}   // namespace gp::tests
