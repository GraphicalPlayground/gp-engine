// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "device/RHIFeatureHelpers.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "memory/MemoryOrder.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::rhi
{

namespace
{

gp::console::AutoConsoleVariable<bool> g_cvarSM6RequireAtomic64(
    "r.SM6.RequireAtomic64",
    true,
    "If true, SM6 feature level is only selected on devices supporting 64-bit typed atomics.",
    gp::console::ConsoleVarFlags::ReadOnly
);

gp::console::AutoConsoleVariable<bool> g_cvarAllowVendorDevice(
    "rhi.AllowVendorDevice",
    true,
    "If true, allows backend initialization of vendor-specific device extensions.",
    gp::console::ConsoleVarFlags::ReadOnly
);

gp::console::AutoConsoleVariable<bool> g_cvarEnableRayTracingShaders(
    "rhi.EnableRayTracingShaders",
    true,
    "If true, enables full hardware ray tracing shader pipelines when supported by the device.",
    gp::console::ConsoleVarFlags::ReadOnly
);

// Atomic override slots: -1 means read from the registered CVar, 0 forces false, 1 forces true.
gp::thread::Atomic<gp::Int8> g_sm6RequireAtomic64Override{ -1 };
gp::thread::Atomic<gp::Int8> g_allowVendorDeviceOverride{ -1 };
gp::thread::Atomic<gp::Int8> g_enableRayTracingShadersOverride{ -1 };
gp::thread::Atomic<gp::UInt8> g_bindlessConfigOnStartup{
    static_cast<gp::UInt8>(view::BindlessConfiguration::Disabled)
};

}   // namespace

bool isAtomic64RequiredForSM6() noexcept
{
    const gp::Int8 overrideVal = g_sm6RequireAtomic64Override.load(gp::MemoryOrder::Acquire);
    if (overrideVal >= 0)
    {
        return overrideVal != 0;
    }

    return g_cvarSM6RequireAtomic64.getValueOnAnyThread(true);
}

void setAtomic64RequiredForSM6(const bool required) noexcept
{
    g_sm6RequireAtomic64Override.store(required ? 1 : 0, gp::MemoryOrder::Release);
}

bool allowVendorDevice() noexcept
{
    if (gp::CommandLine::isInitialized() && gp::CommandLine::hasArgument("novendordevice"))
    {
        return false;
    }

    const gp::Int8 overrideVal = g_allowVendorDeviceOverride.load(gp::MemoryOrder::Acquire);
    if (overrideVal >= 0)
    {
        return overrideVal != 0;
    }

    return g_cvarAllowVendorDevice.getValueOnAnyThread(true);
}

void setAllowVendorDevice(const bool allow) noexcept
{
    g_allowVendorDeviceOverride.store(allow ? 1 : 0, gp::MemoryOrder::Release);
}

bool areRayTracingShadersEnabled() noexcept
{
    const gp::Int8 overrideVal = g_enableRayTracingShadersOverride.load(gp::MemoryOrder::Acquire);
    if (overrideVal >= 0)
    {
        return overrideVal != 0;
    }

    return g_cvarEnableRayTracingShaders.getValueOnAnyThread(true);
}

void setRayTracingShadersEnabled(const bool enabled) noexcept
{
    g_enableRayTracingShadersOverride.store(enabled ? 1 : 0, gp::MemoryOrder::Release);
}

view::BindlessConfiguration getBindlessConfigurationOnStartup(const shader::ShaderPlatform platform) noexcept
{
    static_cast<void>(platform);
    return static_cast<view::BindlessConfiguration>(g_bindlessConfigOnStartup.load(gp::MemoryOrder::Acquire));
}

void setBindlessConfigurationOnStartup(const view::BindlessConfiguration configuration) noexcept
{
    g_bindlessConfigOnStartup.store(static_cast<gp::UInt8>(configuration), gp::MemoryOrder::Release);
}

}   // namespace gp::rhi
