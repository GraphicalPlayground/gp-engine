// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/RHIDeviceQueries.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "device/RHIGlobals.hpp"
#include "memory/MemoryOrder.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi
{

namespace
{

gp::console::AutoConsoleVariable<bool> g_cvarGPUCrashDebugging(
    "r.GPUCrashDebugging",
    false,
    "Enable vendor-specific GPU crash analysis and post-mortem diagnostic tools.",
    gp::console::ConsoleVarFlags::ReadOnly
);

gp::console::AutoConsoleVariable<bool> g_cvarGPUCrashBreadcrumbs(
    "r.GPUCrashDebugging.Breadcrumbs",
    true,
    "Enable vendor-agnostic RHI GPU crash breadcrumbs to identify active passes during GPU hangs.",
    gp::console::ConsoleVarFlags::ReadOnly
);

gp::thread::Atomic<gp::Int8> g_gpuCrashDebuggingOverride{ -1 };
gp::thread::Atomic<gp::Int8> g_gpuCrashBreadcrumbsOverride{ -1 };

}   // namespace

GpuVendorId getRHIDeviceVendorId() noexcept
{
    const gp::UInt32 rawVendorId = static_cast<gp::UInt32>(getRHIGlobals().gpuInfo.vendorId);
    return convertToGpuVendorId(rawVendorId);
}

gp::UInt32 getRHIDeviceId() noexcept
{
    return getRHIGlobals().gpuInfo.deviceId;
}

bool isRHIDeviceAMD() noexcept
{
    return getRHIDeviceVendorId() == GpuVendorId::Amd;
}

bool isRHIDeviceIntel() noexcept
{
    return getRHIDeviceVendorId() == GpuVendorId::Intel;
}

bool isRHIDeviceNVIDIA() noexcept
{
    return getRHIDeviceVendorId() == GpuVendorId::Nvidia;
}

bool isRHIDeviceQualcomm() noexcept
{
    return getRHIDeviceVendorId() == GpuVendorId::Qualcomm;
}

bool isRHIDeviceApple() noexcept
{
    return getRHIDeviceVendorId() == GpuVendorId::Apple;
}

bool isRHIDeviceARM() noexcept
{
    return getRHIDeviceVendorId() == GpuVendorId::Arm;
}

bool isRHIDeviceSamsungAMD() noexcept
{
    return getRHIDeviceVendorId() == GpuVendorId::SamsungAMD;
}

bool isRHIDeviceMicrosoft() noexcept
{
    return getRHIDeviceVendorId() == GpuVendorId::Microsoft;
}

bool isRHIDeviceMesa() noexcept
{
    return getRHIDeviceVendorId() == GpuVendorId::Mesa;
}

bool useGPUCrashDebugging() noexcept
{
    const gp::Int8 overrideVal = g_gpuCrashDebuggingOverride.load(gp::MemoryOrder::Acquire);
    if (overrideVal >= 0)
    {
        return overrideVal != 0;
    }

    if (gp::CommandLine::isInitialized())
    {
        if (gp::CommandLine::hasArgument("nogpucrashdebugging"))
        {
            return false;
        }
        if (gp::CommandLine::hasArgument("gpucrashdebugging"))
        {
            return true;
        }
    }

    return g_cvarGPUCrashDebugging.getValueOnAnyThread(false);
}

void setUseGPUCrashDebugging(const bool enabled) noexcept
{
    g_gpuCrashDebuggingOverride.store(enabled ? 1 : 0, gp::MemoryOrder::Release);
}

void clearUseGPUCrashDebuggingOverride() noexcept
{
    g_gpuCrashDebuggingOverride.store(-1, gp::MemoryOrder::Release);
}

bool useGPUCrashBreadcrumbs() noexcept
{
    const gp::Int8 overrideVal = g_gpuCrashBreadcrumbsOverride.load(gp::MemoryOrder::Acquire);
    if (overrideVal >= 0)
    {
        return overrideVal != 0;
    }

    if (gp::CommandLine::isInitialized())
    {
        if (gp::CommandLine::hasArgument("nogpucrashdebugging"))
        {
            return false;
        }
        if (gp::CommandLine::hasArgument("gpucrashdebugging") || gp::CommandLine::hasArgument("gpubreadcrumbs"))
        {
            return true;
        }
    }

    if (g_cvarGPUCrashDebugging.getValueOnAnyThread(false))
    {
        return true;
    }

    return g_cvarGPUCrashBreadcrumbs.getValueOnAnyThread(true);
}

void setUseGPUCrashBreadcrumbs(const bool enabled) noexcept
{
    g_gpuCrashBreadcrumbsOverride.store(enabled ? 1 : 0, gp::MemoryOrder::Release);
}

void clearUseGPUCrashBreadcrumbsOverride() noexcept
{
    g_gpuCrashBreadcrumbsOverride.store(-1, gp::MemoryOrder::Release);
}

}   // namespace gp::rhi
