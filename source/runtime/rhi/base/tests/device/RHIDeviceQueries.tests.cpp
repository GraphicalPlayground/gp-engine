// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/RHIDeviceQueries.hpp"
#include "device/DeviceEnums.hpp"
#include "device/RHIGlobals.hpp"
#include <gtest/gtest.h>

namespace gp::rhi::tests
{

TEST(RHIDeviceQueriesTest, VendorIdDeviceIdAndVendorPredicates)
{
    struct ScopedGlobalsReset final
    {
        ~ScopedGlobalsReset() noexcept
        {
            clearUseGPUCrashDebuggingOverride();
            clearUseGPUCrashBreadcrumbsOverride();
            getRHIGlobals().reset();
        }
    } const resetGuard{};

    getRHIGlobals().reset();
    EXPECT_EQ(getRHIDeviceVendorId(), GpuVendorId::NotQueried);
    EXPECT_EQ(getRHIDeviceId(), 0u);
    EXPECT_FALSE(isRHIDeviceAMD());
    EXPECT_FALSE(isRHIDeviceIntel());
    EXPECT_FALSE(isRHIDeviceNVIDIA());
    EXPECT_FALSE(isRHIDeviceQualcomm());
    EXPECT_FALSE(isRHIDeviceApple());
    EXPECT_FALSE(isRHIDeviceARM());
    EXPECT_FALSE(isRHIDeviceSamsungAMD());
    EXPECT_FALSE(isRHIDeviceMicrosoft());
    EXPECT_FALSE(isRHIDeviceMesa());

    getRHIGlobals().gpuInfo.vendorId = GpuVendorId::Nvidia;
    getRHIGlobals().gpuInfo.deviceId = 0x2684u;
    EXPECT_EQ(getRHIDeviceVendorId(), GpuVendorId::Nvidia);
    EXPECT_EQ(getRHIDeviceId(), 0x2684u);
    EXPECT_TRUE(isRHIDeviceNVIDIA());
    EXPECT_FALSE(isRHIDeviceAMD());

    getRHIGlobals().gpuInfo.vendorId = GpuVendorId::Amd;
    EXPECT_TRUE(isRHIDeviceAMD());

    getRHIGlobals().gpuInfo.vendorId = GpuVendorId::Intel;
    EXPECT_TRUE(isRHIDeviceIntel());

    // Both canonical 0x5143 and alternate ASCII 'QCOM' (0x4D4F4351) vendor IDs must resolve to Qualcomm.
    getRHIGlobals().gpuInfo.vendorId = GpuVendorId::Qualcomm;
    EXPECT_TRUE(isRHIDeviceQualcomm());
    getRHIGlobals().gpuInfo.vendorId = static_cast<GpuVendorId>(kQualcommAlternateVendorId);
    EXPECT_EQ(getRHIDeviceVendorId(), GpuVendorId::Qualcomm);
    EXPECT_TRUE(isRHIDeviceQualcomm());

    getRHIGlobals().gpuInfo.vendorId = GpuVendorId::Apple;
    EXPECT_TRUE(isRHIDeviceApple());

    getRHIGlobals().gpuInfo.vendorId = GpuVendorId::Arm;
    EXPECT_TRUE(isRHIDeviceARM());

    getRHIGlobals().gpuInfo.vendorId = GpuVendorId::SamsungAMD;
    EXPECT_TRUE(isRHIDeviceSamsungAMD());

    getRHIGlobals().gpuInfo.vendorId = GpuVendorId::Microsoft;
    EXPECT_TRUE(isRHIDeviceMicrosoft());

    getRHIGlobals().gpuInfo.vendorId = GpuVendorId::Mesa;
    EXPECT_TRUE(isRHIDeviceMesa());
}

TEST(RHIDeviceQueriesTest, GPUCrashDebuggingAndBreadcrumbsOverrides)
{
    struct ScopedOverrideReset final
    {
        ~ScopedOverrideReset() noexcept
        {
            clearUseGPUCrashDebuggingOverride();
            clearUseGPUCrashBreadcrumbsOverride();
        }
    } const resetGuard{};

    clearUseGPUCrashDebuggingOverride();
    clearUseGPUCrashBreadcrumbsOverride();

    const bool defaultCrashDebugging = useGPUCrashDebugging();
    const bool defaultBreadcrumbs = useGPUCrashBreadcrumbs();

    setUseGPUCrashDebugging(true);
    EXPECT_TRUE(useGPUCrashDebugging());
    setUseGPUCrashDebugging(false);
    EXPECT_FALSE(useGPUCrashDebugging());
    clearUseGPUCrashDebuggingOverride();
    EXPECT_EQ(useGPUCrashDebugging(), defaultCrashDebugging);

    setUseGPUCrashBreadcrumbs(false);
    EXPECT_FALSE(useGPUCrashBreadcrumbs());
    setUseGPUCrashBreadcrumbs(true);
    EXPECT_TRUE(useGPUCrashBreadcrumbs());
    clearUseGPUCrashBreadcrumbsOverride();
    EXPECT_EQ(useGPUCrashBreadcrumbs(), defaultBreadcrumbs);
}

}   // namespace gp::rhi::tests
