// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "profiling/ProfilingControl.hpp"
#include "console/AutoConsoleVariable.hpp"
#include "console/ConsoleEnums.hpp"
#include "console/ConsoleTypes.hpp"
#include "console/IConsoleVariable.hpp"
#include "containers/strings/FixedString.hpp"
#include "logging/LogFormatter.hpp"
#include "memory/MemoryOrder.hpp"
#include "threading/atomics/Atomic.hpp"

namespace gp::profiling
{

namespace
{

constinit gp::thread::Atomic<bool> g_profZonesEnabled{ true };
constinit gp::thread::Atomic<bool> g_profPlotsEnabled{ true };
constinit gp::thread::Atomic<bool> g_profTraceConsoleEnabled{ true };
constinit gp::thread::Atomic<gp::Float64> g_profHitchThresholdMs{ 0.0 };

void onProfEnabledChanged(gp::console::IConsoleVariable* const var) noexcept
{
    if (var != nullptr)
    {
        g_profZonesEnabled.store(var->getBool(), gp::MemoryOrder::Release);
    }
}

void onProfPlotsChanged(gp::console::IConsoleVariable* const var) noexcept
{
    if (var != nullptr)
    {
        g_profPlotsEnabled.store(var->getBool(), gp::MemoryOrder::Release);
    }
}

void onProfTraceConsoleChanged(gp::console::IConsoleVariable* const var) noexcept
{
    if (var != nullptr)
    {
        g_profTraceConsoleEnabled.store(var->getBool(), gp::MemoryOrder::Release);
    }
}

void onProfHitchThresholdChanged(gp::console::IConsoleVariable* const var) noexcept
{
    if (var != nullptr)
    {
        const gp::Float64 val = var->getFloat64();
        g_profHitchThresholdMs.store((val > 0.0) ? val : 0.0, gp::MemoryOrder::Release);
    }
}

gp::console::AutoConsoleVariable<bool> g_cvarProfEnabled(
    "prof.Enabled",
    true,
    "Enables runtime conditional CPU profiling zones (GP_PROFILE_SCOPE*_ACTIVE).",
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleVariable<bool> g_cvarProfPlots(
    "prof.Plots",
    true,
    "Enables runtime submission of Tracy numeric telemetry plots (GP_PLOT_IF).",
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleVariable<bool> g_cvarProfTraceConsole(
    "prof.TraceConsole",
    true,
    "Emits Tracy timeline bookmark annotations when console variables or commands execute interactively.",
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleVariable<gp::Float64> g_cvarProfHitchThresholdMs(
    "prof.HitchThresholdMs",
    0.0,
    "Frame delta threshold in milliseconds that triggers a red Tracy hitch bookmark (<= 0 disables).",
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleCommand g_cmdProfBookmark(
    "prof.Bookmark",
    "Emits a named bookmark marker to the active profiler timeline.",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    gp::FixedString<255> label{};
    if (args.getArgCount() == 0u)
    {
        label.assign("UserBookmark");
    }
    else
    {
        for (gp::USize i = 0u; i < args.getArgCount(); ++i)
        {
            if (i > 0u && label.size() < label.capacity())
            {
                label.pushBack(' ');
            }
            const gp::USize rem = (label.capacity() > label.size()) ? (label.capacity() - label.size()) : 0u;
            label.append(args.getArg(i).substr(0u, rem));
        }
    }

    ProfilingControl::emitBookmark(label.view(), GP_U32_COLOR_CYAN);

    gp::FixedString<319> msg{};
    (void)gp::log::formatTo(msg, "[Tracy] Bookmark: {}", label.view());
    args.writeLine(msg.view());
},
    gp::console::ConsoleVarFlags::None
);

gp::console::AutoConsoleCommand g_cmdProfStatus(
    "prof.Status",
    "Displays active profiler backend status and runtime CVar gating configuration.",
    [](const gp::console::ConsoleCommandArgs& args) noexcept -> void
{
    gp::FixedString<383> line{};
#if GP_USE_TRACY_PROFILER
    const gp::StringView backendName = "Tracy";
#else
    const gp::StringView backendName = "Null";
#endif
    (void)gp::log::formatTo(
        line,
        "Profiler[{}] enabled={} plots={} traceConsole={} hitchThresholdMs={:.2f}",
        backendName,
        ProfilingControl::isZonesEnabled(),
        ProfilingControl::isPlotsEnabled(),
        ProfilingControl::isConsoleTraceEnabled(),
        ProfilingControl::getHitchThresholdMs()
    );
    args.writeLine(line.view());
},
    gp::console::ConsoleVarFlags::None
);

// Bind CVar change delegates during initialization cleanly without altering deferred registration order.
struct ProfilingControlRegistrar final
{
    ProfilingControlRegistrar() noexcept
    {
        g_cvarProfEnabled->setOnChangedCallback(
            gp::console::ConsoleVarChangedDelegate::createStatic<&onProfEnabledChanged>()
        );
        g_cvarProfPlots->setOnChangedCallback(
            gp::console::ConsoleVarChangedDelegate::createStatic<&onProfPlotsChanged>()
        );
        g_cvarProfTraceConsole->setOnChangedCallback(
            gp::console::ConsoleVarChangedDelegate::createStatic<&onProfTraceConsoleChanged>()
        );
        g_cvarProfHitchThresholdMs->setOnChangedCallback(
            gp::console::ConsoleVarChangedDelegate::createStatic<&onProfHitchThresholdChanged>()
        );
    }
};

const ProfilingControlRegistrar g_profilingControlRegistrar{};

}   // namespace

bool ProfilingControl::isZonesEnabled() noexcept
{
    return g_profZonesEnabled.load(gp::MemoryOrder::Relaxed);
}

bool ProfilingControl::isPlotsEnabled() noexcept
{
    return g_profPlotsEnabled.load(gp::MemoryOrder::Relaxed);
}

bool ProfilingControl::isConsoleTraceEnabled() noexcept
{
    return g_profTraceConsoleEnabled.load(gp::MemoryOrder::Relaxed);
}

gp::Float64 ProfilingControl::getHitchThresholdMs() noexcept
{
    return g_profHitchThresholdMs.load(gp::MemoryOrder::Relaxed);
}

void ProfilingControl::emitBookmark(const gp::StringView label, [[maybe_unused]] const gp::UInt32 color) noexcept
{
    if (label.isEmpty())
    {
        return;
    }

    gp::FixedString<255> safeBuf{};
    safeBuf.assign(label.substr(0u, safeBuf.capacity()));
    GP_MESSAGE_C_IF(isZonesEnabled(), safeBuf.cString(), safeBuf.size(), color);
}

bool ProfilingControl::recordFrameHitchIfExceeded(const gp::Float64 rawDeltaMs, const gp::UInt64 frameIndex) noexcept
{
    const gp::Float64 threshold = g_profHitchThresholdMs.load(gp::MemoryOrder::Relaxed);
    if (threshold <= 0.0 || rawDeltaMs < threshold)
    {
        return false;
    }

    gp::FixedString<255> msg{};
    (void)gp::log::formatTo(
        msg, "Frame Hitch: {:.2f} ms (budget {:.2f} ms, frame #{})", rawDeltaMs, threshold, frameIndex
    );
    GP_MESSAGE_C_IF(isZonesEnabled(), msg.cString(), msg.size(), GP_U32_COLOR_RED);
    return true;
}

}   // namespace gp::profiling
