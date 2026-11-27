// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "presentation/VulkanPresentationSettings.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/IConsoleManager.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/views/StringView.hpp"   // IWYU pragma: keep
#include "CoreMinimal.hpp"
#include "miscellaneous/CommandLine.hpp"
#include "presentation/VulkanPresentationEnums.hpp"
#include "presentation/VulkanPresentationTypes.hpp"
#include "templates/Optional.hpp"

namespace gp::rhi::vulkan
{

namespace
{

#if defined(GP_PLATFORM_ANDROID) && GP_PLATFORM_ANDROID
/// @brief Android surfaces are torn down with the activity; keeping them across a recreation is unsafe.
constexpr gp::Int32 kDefaultKeepSwapChain = 0;
#else
/// @brief Desktop surfaces outlive swapchain recreations, so reusing them saves a window-system round trip.
constexpr gp::Int32 kDefaultKeepSwapChain = 1;
#endif

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanKeepSwapChain(
    "r.Vulkan.KeepSwapChain",
    kDefaultKeepSwapChain,
    "Whether to keep the old swapchain and surface alive to pass them to the new swapchain on recreation.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanCpuWaitForFence(
    "r.Vulkan.CpuWaitForFence",
    0,
    "0 to only wait on the acquire semaphore on the GPU (default), 1 to also wait for each acquired image on the CPU.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanCpuRhiThreadFramePacer(
    "r.Vulkan.CPURHIThreadFramePacer",
    0,
    "Whether to sleep on the RHI thread before presenting to hold r.Vulkan.FramePacerRefreshRate (0 by default).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanForcePacingWithoutVSync(
    "r.Vulkan.ForcePacingWithoutVSync",
    0,
    "Whether the CPU frame pacer also runs when vertical sync is off.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanFramePacerRefreshRate(
    "r.Vulkan.FramePacerRefreshRate",
    60,
    "Refresh rate in Hz the CPU frame pacer targets.",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

gp::console::AutoConsoleVariable<gp::Int32> g_cvarVulkanPreTransform(
    "r.Vulkan.PreTransform",
    1,
    "Whether to create swapchains with the surface's current transform (rotated displays render pre-rotated).",
    gp::console::ConsoleVarFlags::RenderThreadSafe
);

/// @brief Reads an integer console variable registered by another module.
/// @param[in] name Variable name.
/// @param[in] defaultValue Value returned when the variable is not registered.
/// @return Current value, or `defaultValue`.
[[nodiscard]] gp::Int32 readForeignIntVariable(const gp::StringView name, const gp::Int32 defaultValue) noexcept
{
    const gp::console::IConsoleVariable* const variable = gp::console::IConsoleManager::get().findConsoleVariable(name);
    return (variable != nullptr) ? variable->getIntOnAnyThread() : defaultValue;
}

/// @brief Maps the integer value of `r.HDR.Display.OutputDevice` to a display output format.
/// @param[in] value Raw variable value.
/// @return Matching format, `SdrSrgb` when out of range.
[[nodiscard]] VulkanDisplayOutputFormat toDisplayOutputFormat(const gp::Int32 value) noexcept
{
    if ((value < 0) || (value >= static_cast<gp::Int32>(VulkanDisplayOutputFormat::Count)))
    {
        return VulkanDisplayOutputFormat::SdrSrgb;
    }
    return static_cast<VulkanDisplayOutputFormat>(value);
}

}   // namespace

VulkanPresentationConfig resolveVulkanPresentationConfig() noexcept
{
    VulkanPresentationConfig config{};
    config.keepSwapChain = (g_cvarVulkanKeepSwapChain.getValueOnAnyThread(true) != 0);
    config.cpuWaitForFence = (g_cvarVulkanCpuWaitForFence.getValueOnAnyThread(true) != 0);
    config.cpuFramePacer = (g_cvarVulkanCpuRhiThreadFramePacer.getValueOnAnyThread(true) != 0);
    config.forcePacingWithoutVSync = (g_cvarVulkanForcePacingWithoutVSync.getValueOnAnyThread(true) != 0);
    config.allowPreTransform = (g_cvarVulkanPreTransform.getValueOnAnyThread(true) != 0);

    const gp::Int32 refreshRate = g_cvarVulkanFramePacerRefreshRate.getValueOnAnyThread(true);
    config.framePacerRefreshRate = (refreshRate > 0) ? static_cast<gp::UInt32>(refreshRate) : 60u;

    config.displayOutputFormat = toDisplayOutputFormat(readForeignIntVariable("r.HDR.Display.OutputDevice", 0));
    config.lockToVsync = (readForeignIntVariable("r.VSync", 0) != 0);
    config.allowPresentOnComputeQueue = (readForeignIntVariable("r.Vulkan.AllowPresentOnComputeQueue", 0) != 0);

    if (gp::CommandLine::isInitialized())
    {
        const gp::Optional<gp::Int32> presentMode = gp::CommandLine::tryGetInt32("vulkanpresentmode");
        if (presentMode.hasValue() && (*presentMode >= 0))
        {
            config.presentModeOverride = *presentMode;
        }
    }
    return config;
}

}   // namespace gp::rhi::vulkan
